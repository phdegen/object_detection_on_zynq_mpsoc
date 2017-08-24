

#define USE_EIGEN_TENSOR
#define EIGEN_USE_THREADS

#define QUANT_BITS        6 //One less than the actual because of the signed bit

#include "tensorflow/core/framework/function.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/util/padding.h"
#include "tensorflow/core/util/tensor_format.h"


#include "tensorflow/core/framework/common_shape_fns.h"
#include "tensorflow/core/framework/numeric_op.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/shape_inference.h"
#include "tensorflow/core/util/mirror_pad_mode.h"

#include "tensorflow/core/user_ops/quant_conv_ops.h"
#include <string.h>
#include <map>
#include <vector>
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/register_types.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/framework/tensor_slice.h"
#include "tensorflow/core/kernels/bounds_check.h"
#include "tensorflow/core/kernels/conv_2d.h"
#include "tensorflow/core/kernels/deep_conv2d.h"
#include "tensorflow/core/kernels/ops_util.h"

#include "tensorflow/core/lib/gtl/array_slice.h"
#include "tensorflow/core/lib/strings/numbers.h"
#include "tensorflow/core/lib/strings/str_util.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/macros.h"
#include "tensorflow/core/util/use_cudnn.h"

#include "tensorflow/core/kernels/conv_grad_ops.h"

#include "tensorflow/core/lib/core/status.h"

#include "tensorflow/core/util/work_sharder.h"

#include "tensorflow/core/kernels/bias_op.h"
#include "third_party/eigen3/unsupported/Eigen/CXX11/Tensor"

#if GOOGLE_CUDA
#include "tensorflow/core/kernels/bias_op_gpu.h"
#include "tensorflow/core/kernels/conv_ops_gpu.h"
#include "tensorflow/core/platform/stream_executor.h"
#endif  // GOOGLE_CUDA


void doquant(int N, float scale, float *tensor);
void doquant2(int N, float scale, float *tensor, float *output);

namespace {

// Returns in 'col_data', image patches in storage order (height, width, depth)
// extracted from image at 'input_data', which is required to be in storage
// order (batch, height, width, depth).
// Implementation written by Yangqing Jia (jiayq).
template <typename T>
void Im2col(const T* input_data, const int depth, const int height,
            const int width, const int filter_h, const int filter_w,
            const int pad_t, const int pad_l, const int pad_b, const int pad_r,
            const int stride_h, const int stride_w, T* col_data) {
  int height_col = (height + pad_t + pad_b - filter_h) / stride_h + 1;
  int width_col = (width + pad_l + pad_r - filter_w) / stride_w + 1;

  int h_pad = -pad_t;
  for (int h = 0; h < height_col; ++h) {
    int w_pad = -pad_l;
    for (int w = 0; w < width_col; ++w) {
      for (int ih = h_pad; ih < h_pad + filter_h; ++ih) {
        for (int iw = w_pad; iw < w_pad + filter_w; ++iw) {
          if (ih >= 0 && ih < height && iw >= 0 && iw < width) {
            memcpy(col_data, input_data + (ih * width + iw) * depth,
                   sizeof(T) * depth);
          } else {
            // This should be simply padded with zero.
            memset(col_data, 0, sizeof(T) * depth);
          }
          col_data += depth;
        }
      }
      w_pad += stride_w;
    }
    h_pad += stride_h;
  }
}

} 

namespace {

// Returns in 'im_data' (assumes to be zero-initialized) image patch in storage
// order (height, width, depth), constructed from patches in 'col_data', which
// is required to be in storage order (out_height * out_width, filter_height,
// filter_width, in_depth).  Implementation by Yangqing Jia (jiayq).
template <typename T>
void Col2im(const T* col_data, const int depth, const int height,
            const int width, const int filter_h, const int filter_w,
            const int pad_t, const int pad_l, const int pad_b, const int pad_r,
            const int stride_h, const int stride_w, T* im_data) {
  int height_col = (height + pad_t + pad_b - filter_h) / stride_h + 1;
  int width_col = (width + pad_l + pad_r - filter_w) / stride_w + 1;
  int h_pad = -pad_t;
  for (int h = 0; h < height_col; ++h) {
    int w_pad = -pad_l;
    for (int w = 0; w < width_col; ++w) {
      T* im_patch_data = im_data + (h_pad * width + w_pad) * depth;
      for (int ih = h_pad; ih < h_pad + filter_h; ++ih) {
        for (int iw = w_pad; iw < w_pad + filter_w; ++iw) {
          if (ih >= 0 && ih < height && iw >= 0 && iw < width) {
            // TODO(andydavis) Vectorize this loop (if compiler does not).
            for (int i = 0; i < depth; ++i) {
              im_patch_data[i] += col_data[i];
            }
          }
          im_patch_data += depth;
          col_data += depth;
        }
        // Jump over remaining number of depth.
        im_patch_data += depth * (width - filter_w);
      }
      w_pad += stride_w;
    }
    h_pad += stride_h;
  }
}

}  



namespace tensorflow {


typedef FunctionDefHelper FDH;

typedef Eigen::ThreadPoolDevice CPUDevice;
typedef Eigen::GpuDevice GPUDevice;

using shape_inference::DimensionHandle;
using shape_inference::InferenceContext;
using shape_inference::ShapeHandle;


namespace {


Status InputTensorShapeOrUnknown(InferenceContext* c, int input_idx,
                                 int ndims) {
  ShapeHandle out;
  const Tensor* input = c->input_tensor(input_idx);
  if (input == nullptr) {
    out = c->UnknownShapeOfRank(ndims);
  } else {
    TF_RETURN_IF_ERROR(c->MakeShapeFromShapeTensor(input_idx, &out));
  }
  c->set_output(0, out);
  return Status::OK();
}


}

Status Conv2DquantGrad(const AttrSlice& attrs, FunctionDef* g) {
  // clang-format off
  *g = FDH::Define(
    // Arg defs
    {"input: T", "filter: T", "grad: T"},
    // Ret val defs
    {"input_grad: T", "filter_grad: T"},
    // Attr defs
    {"T: {float, double}",
     "strides: list(int)",
     "use_cudnn_on_gpu: bool = true",
     GetPaddingAttrString(),
     GetConvnetDataFormatAttrString()},
    // Nodes
    {
      {{"i_shape"}, "Shape", {"input"}, {{"T", "$T"}}},
      {{"input_grad"}, "Conv2DquantBackpropInput", {"i_shape", "filter", "grad"},
       /*Attrs=*/{{"T", "$T"},
                  {"strides", "$strides"},
                  {"padding", "$padding"},
                  {"data_format", "$data_format"},
                  {"use_cudnn_on_gpu", "$use_cudnn_on_gpu"}}},

      {{"f_shape"}, "Shape", {"filter"}, {{"T", "$T"}}},
      {{"filter_grad"}, "Conv2DquantBackpropFilter", {"input", "f_shape", "grad"},
       /*Attrs=*/{{"T", "$T"},
                  {"strides", "$strides"},
                  {"padding", "$padding"},
                  {"data_format", "$data_format"},
                  {"use_cudnn_on_gpu", "$use_cudnn_on_gpu"}}},
    });
  // clang-format on
  return Status::OK();
}
REGISTER_OP_GRADIENT("Conv2Dquant", Conv2DquantGrad);





namespace {
template <typename Device, typename T>
struct LaunchGeneric {
  static void launch(OpKernelContext* ctx, const Tensor& input,
                     const Tensor& filter, int row_stride, int col_stride,
                     const Eigen::PaddingType& padding, Tensor* output,
                     TensorFormat data_format) {
    CHECK(data_format == FORMAT_NHWC) << "Generic conv implementation only "
                                         "supports NHWC tensor format for now.";
    if (filter.dim_size(0) == 1 && filter.dim_size(1) == 1 && row_stride == 1 &&
        col_stride == 1) {
      // For 1x1 kernel, the 2D convolution is reduced to matrix
      // multiplication.
      //
      // TODO(vrv): We should be able to call SpatialConvolution
      // and it will produce the same result, but doing so
      // led to NaNs during training.  Using matmul instead for now.
      int conv_width = 1;  // Width for the convolution step.
      for (int i = 0; i < 3; ++i) {
        conv_width *= output->dim_size(i);
      }

      Eigen::array<Eigen::IndexPair<Eigen::DenseIndex>, 1> dim_pair;
      dim_pair[0] = Eigen::IndexPair<Eigen::DenseIndex>(1, 0);
      functor::MatMulConvFunctor<Device, T>()(
          ctx->eigen_device<Device>(),
          output->shaped<T, 2>({conv_width, filter.dim_size(3)}),
          input.shaped<T, 2>({conv_width, filter.dim_size(2)}),
          filter.shaped<T, 2>({filter.dim_size(2), filter.dim_size(3)}),
          dim_pair);
    } else if (filter.dim_size(0) == input.dim_size(1) &&
               filter.dim_size(1) == input.dim_size(2) &&
               padding == Eigen::PADDING_VALID) {
      // If the input data and filter have the same height/width,
      // the 2D convolution is reduced to matrix multiplication.
      const int k =  // Length of reduction dimension.
          filter.dim_size(0) * filter.dim_size(1) * filter.dim_size(2);

      Eigen::array<Eigen::IndexPair<Eigen::DenseIndex>, 1> dim_pair;
      dim_pair[0] = Eigen::IndexPair<Eigen::DenseIndex>(1, 0);
      functor::MatMulConvFunctor<Device, T>()(
          ctx->eigen_device<Device>(),
          output->shaped<T, 2>({input.dim_size(0), filter.dim_size(3)}),
          input.shaped<T, 2>({input.dim_size(0), k}),
          filter.shaped<T, 2>({k, filter.dim_size(3)}), dim_pair);
    } else {
      functor::SpatialConvolution<Device, T>()(
          ctx->eigen_device<Device>(), output->tensor<T, 4>(),
          input.tensor<T, 4>(), filter.tensor<T, 4>(), row_stride, col_stride,
          padding);
    }
  }
};
}  // namespace

template <typename T>
class LaunchConv2DquantOp<CPUDevice, T> {
 public:
  void launch(OpKernelContext* ctx, bool use_cudnn, bool cudnn_use_autotune,
              const Tensor& input, const Tensor& filter, int row_stride,
              int col_stride, const Eigen::PaddingType& padding, Tensor* output,
              TensorFormat data_format) {
    LaunchGeneric<CPUDevice, T>::launch(ctx, input, filter, row_stride,
                                        col_stride, padding, output,
                                        data_format);
  }
};

template <typename Device, typename T>
class LaunchDeepConvOp {
 public:
  static bool Run(OpKernelContext* ctx, const Tensor& input,
                  const Tensor& filter, int batch, int input_rows,
                  int input_cols, int in_depth, int filter_rows,
                  int filter_cols, int pad_rows, int pad_cols, int out_rows,
                  int out_cols, int out_depth, int stride_rows, int stride_cols,
                  Tensor* output, TensorFormat data_format) {
    return false;
  }
};

// Conditionally launches DeepConv operation based on convolution parameters.
template <>
class LaunchDeepConvOp<CPUDevice, float> {
 public:
  static bool Run(OpKernelContext* ctx, const Tensor& input,
                  const Tensor& filter, int batch, int input_rows,
                  int input_cols, int in_depth, int filter_rows,
                  int filter_cols, int pad_rows, int pad_cols, int out_rows,
                  int out_cols, int out_depth, int stride_rows, int stride_cols,
                  Tensor* output, TensorFormat data_format) {
    if (data_format != FORMAT_NHWC ||
        !CanUseDeepConv2D(stride_rows, stride_cols, filter_rows, filter_cols,
                          in_depth, out_depth, out_rows, out_cols)) {
      return false;
    }

    Conv2DArgs args;
    args.batch = batch;
    args.in_rows = input_rows;
    args.in_cols = input_cols;
    args.in_depth = in_depth;
    args.filter_rows = filter_rows;
    args.filter_cols = filter_cols;
    args.pad_rows = pad_rows;
    args.pad_cols = pad_cols;
    args.out_rows = out_rows;
    args.out_cols = out_cols;
    args.out_depth = out_depth;

    auto input_ptr = input.template flat<float>().data();
    auto filter_ptr = filter.template flat<float>().data();
    auto output_ptr = output->template flat<float>().data();

    functor::DeepConv2D<CPUDevice, float>()(ctx, args, input_ptr, filter_ptr,
                                            output_ptr);
    return true;
  }
};


template <typename Device, typename T>
class Conv2DquantOp : public BinaryOp<T> {
 public:
  explicit Conv2DquantOp(OpKernelConstruction* context) : BinaryOp<T>(context) {
    OP_REQUIRES_OK(context, context->GetAttr("strides", &strides_));
    string data_format;
    OP_REQUIRES_OK(context, context->GetAttr("data_format", &data_format));
    OP_REQUIRES(context, FormatFromString(data_format, &data_format_),
                errors::InvalidArgument("Invalid data format"));
    OP_REQUIRES_OK(context, context->GetAttr("use_cudnn_on_gpu", &use_cudnn_));
    use_cudnn_ &= CanUseCudnn();
    cudnn_use_autotune_ = CudnnUseAutotune();
    OP_REQUIRES(context, strides_.size() == 4,
                errors::InvalidArgument("Sliding window strides field must "
                                        "specify 4 dimensions"));
    const int64 stride_n = GetTensorDim(strides_, data_format_, 'N');
    const int64 stride_c = GetTensorDim(strides_, data_format_, 'C');
    OP_REQUIRES(
        context, stride_n == 1 && stride_c == 1,
        errors::InvalidArgument("Current implementation does not yet support "
                                "strides in the batch and depth dimensions."));
    OP_REQUIRES_OK(context, context->GetAttr("padding", &padding_));
    OP_REQUIRES_OK(context, context->GetAttr("quant_int_bits", &quant_int_bits));
  }

  void Compute(OpKernelContext* context) override {
    // Input tensor is of the following dimensions:
    // [ batch, in_rows, in_cols, in_depth ]

    const Tensor& input = context->input(0);

    // Input filter is of the following dimensions:
    // [ filter_rows, filter_cols, in_depth, out_depth]
    const Tensor& filter = context->input(1);

    // For 2D convolution, there should be 4 dimensions.
    OP_REQUIRES(context, input.dims() == 4,
                errors::InvalidArgument("input must be 4-dimensional",
                                        input.shape().DebugString()));
    OP_REQUIRES(context, filter.dims() == 4,
                errors::InvalidArgument("filter must be 4-dimensional: ",
                                        filter.shape().DebugString()));

    for (int i = 0; i < 3; i++) {
      OP_REQUIRES(context, FastBoundsCheck(filter.dim_size(i),
                                           std::numeric_limits<int>::max()),
                  errors::InvalidArgument("filter too large"));
    }

    // The last dimension for input is in_depth. It must be the same as the
    // filter's in_depth.
    const int64 in_depth = GetTensorDim(input, data_format_, 'C');
    OP_REQUIRES(
        context, in_depth == filter.dim_size(2),
        errors::InvalidArgument("input and filter must have the same depth: ",
                                in_depth, " vs ", filter.dim_size(2)));

    // The last dimension for filter is out_depth.
    const int out_depth = static_cast<int>(filter.dim_size(3));

    // The second dimension for input is rows/height.
    // The first dimension for filter is rows/height.
    const int64 input_rows_raw = GetTensorDim(input, data_format_, 'H');
    OP_REQUIRES(context, FastBoundsCheck(input_rows_raw,
                                         std::numeric_limits<int>::max()),
                errors::InvalidArgument("Input rows too large"));
    const int input_rows = static_cast<int>(input_rows_raw);
    const int filter_rows = static_cast<int>(filter.dim_size(0));

    // The third dimension for input is columns/width.
    // The second dimension for filter is columns/width.
    const int64 input_cols_raw = GetTensorDim(input, data_format_, 'W');
    OP_REQUIRES(context, FastBoundsCheck(input_cols_raw,
                                         std::numeric_limits<int>::max()),
                errors::InvalidArgument("Input cols too large"));
    const int input_cols = static_cast<int>(input_cols_raw);
    const int filter_cols = static_cast<int>(filter.dim_size(1));

    // The first dimension for input is batch.
    const int64 batch_raw = GetTensorDim(input, data_format_, 'N');
    OP_REQUIRES(context,
                FastBoundsCheck(batch_raw, std::numeric_limits<int>::max()),
                errors::InvalidArgument("batch is too large"));
    const int batch = static_cast<int>(batch_raw);

    // For now we take the stride from the second and third dimensions only (we
    // do not support striding on the batch or depth dimension).
    const int stride_rows = GetTensorDim(strides_, data_format_, 'H');
    const int stride_cols = GetTensorDim(strides_, data_format_, 'W');

    Tensor quant_filter = Tensor(filter);
    auto tensor_data = quant_filter.flat<T>();

    //get the quant scaling factor
    float scaling;
    scaling = std::pow(2, (float)(QUANT_BITS-quant_int_bits));

    doquant(tensor_data.dimension(0), scaling, tensor_data.data());
    //perform quantisation
    //for(int i=0;i<filter_rows*filter_cols*in_depth*out_depth;i++){
    //  tensor_data(i) = round(tensor_data(i)* scaling) / scaling;
    //}

    int64 out_rows = 0, out_cols = 0, pad_rows = 0, pad_cols = 0;
    OP_REQUIRES_OK(context,
                   GetWindowedOutputSize(input_rows, filter_rows, stride_rows,
                                         padding_, &out_rows, &pad_rows));
    OP_REQUIRES_OK(context,
                   GetWindowedOutputSize(input_cols, filter_cols, stride_cols,
                                         padding_, &out_cols, &pad_cols));
    TensorShape out_shape =
        ShapeFromFormat(data_format_, batch, out_rows, out_cols, out_depth);

    // Output tensor is of the following dimensions:
    // [ in_batch, out_rows, out_cols, out_depth ]
    Tensor* output = nullptr;
    OP_REQUIRES_OK(context, context->allocate_output(0, out_shape, &output));

    VLOG(2) << "Conv2Dquant: in_depth = " << in_depth
            << ", input_cols = " << input_cols
            << ", filter_cols = " << filter_cols
            << ", input_rows = " << input_rows
            << ", filter_rows = " << filter_rows
            << ", stride_rows = " << stride_rows
            << ", stride_cols = " << stride_cols
            << ", out_depth = " << out_depth;

    // If there is nothing to compute, return.
    if (out_shape.num_elements() == 0) {
      return;
    }

#ifdef TENSORFLOW_USE_LIBXSMM
    if (LaunchXsmmConvOp<Device, T>::Run(
            context, input, quant_filter, batch, input_rows, input_cols, in_depth,
            filter_rows, filter_cols, pad_rows, pad_cols, out_rows, out_cols,
            out_depth, stride_rows, stride_cols, output, data_format_)) {
      return;
    }
#endif

    if (LaunchDeepConvOp<Device, T>::Run(
            context, input, quant_filter, batch, input_rows, input_cols, in_depth,
            filter_rows, filter_cols, pad_rows, pad_cols, out_rows, out_cols,
            out_depth, stride_rows, stride_cols, output, data_format_)) {
      return;
    }

    launcher_.launch(context, use_cudnn_, cudnn_use_autotune_, input, quant_filter,
                     stride_rows, stride_cols,
                     BrainPadding2EigenPadding(padding_), output, data_format_);
  }

 private:
  std::vector<int32> strides_;
  bool use_cudnn_;
  Padding padding_;
  int quant_int_bits;
  TensorFormat data_format_;
  LaunchConv2DquantOp<Device, T> launcher_;
  bool cudnn_use_autotune_;

  TF_DISALLOW_COPY_AND_ASSIGN(Conv2DquantOp);
};

#define REGISTER_CPU(T)                                         \
  REGISTER_KERNEL_BUILDER(                                      \
      Name("Conv2Dquant").Device(DEVICE_CPU).TypeConstraint<T>("T"), \
      Conv2DquantOp<CPUDevice, T>);


//TF_CALL_half(REGISTER_CPU);
TF_CALL_float(REGISTER_CPU);



#if GOOGLE_CUDA
int64 GetCudnnWorkspaceLimit(const string& envvar_in_mb,
                             int64 default_value_in_bytes) {
  const char* workspace_limit_in_mb_str = getenv(envvar_in_mb.c_str());
  if (workspace_limit_in_mb_str != nullptr &&
      strcmp(workspace_limit_in_mb_str, "") != 0) {
    int64 scratch_limit_in_mb = -1;
    if (strings::safe_strto64(workspace_limit_in_mb_str,
                              &scratch_limit_in_mb)) {
      return scratch_limit_in_mb * (1 << 20);
    } else {
      LOG(WARNING) << "Invalid value for env-var " << envvar_in_mb << ": "
                   << workspace_limit_in_mb_str;
    }
  }
  return default_value_in_bytes;
}

// A dummy type to group forward convolution autotune results together.
struct ConvAutoTuneGroup {};
typedef AutoTuneSingleton<ConvAutoTuneGroup, ConvParameters,
                          perftools::gputools::dnn::AlgorithmConfig>
    AutoTuneConv;

template <typename T>
void LaunchConv2DquantOp<GPUDevice, T>::launch(
    OpKernelContext* ctx, bool use_cudnn, bool cudnn_use_autotune,
    const Tensor& input_param, const Tensor& filter, int row_stride,
    int col_stride, const Eigen::PaddingType& padding, Tensor* output,
    TensorFormat data_format) {
  using perftools::gputools::dnn::AlgorithmConfig;
  using perftools::gputools::dnn::AlgorithmType;
  using perftools::gputools::dnn::ProfileResult;
  using perftools::gputools::dnn::kDefaultAlgorithm;
  auto* stream = ctx->op_device_context()->stream();
  OP_REQUIRES(ctx, stream, errors::Internal("No GPU stream available."));

  if (!use_cudnn) {
    ctx->SetStatus(
        errors::Unimplemented("Conv2D for GPU is not currently supported "
                              "without cudnn"));
    return;
  }

  Tensor input = input_param;

  if (filter.dim_size(0) == 1 && filter.dim_size(1) == 1 && row_stride == 1 &&
      col_stride == 1 && data_format == FORMAT_NHWC) {
    // 1x1 filter, so call cublas directly.
    const uint64 m = input.dim_size(0) * input.dim_size(1) * input.dim_size(2);
    const uint64 k = filter.dim_size(2);
    const uint64 n = filter.dim_size(3);

    auto a_ptr = AsDeviceMemory(input.template flat<T>().data(),
                                input.template flat<T>().size());
    auto b_ptr = AsDeviceMemory(filter.template flat<T>().data(),
                                filter.template flat<T>().size());
    auto c_ptr = AsDeviceMemory(output->template flat<T>().data(),
                                output->template flat<T>().size());

    auto no_transpose = perftools::gputools::blas::Transpose::kNoTranspose;
    bool blas_launch_status =
        stream
            ->ThenBlasGemm(no_transpose, no_transpose, n, m, k, 1.0f, b_ptr, n,
                           a_ptr, k, 0.0f, &c_ptr, n)
            .ok();
    if (!blas_launch_status) {
      ctx->SetStatus(errors::Internal("Blas SGEMM launch failed : m=", m,
                                      ", n=", n, ", k=", k));
    }
    return;
  } else if (filter.dim_size(0) == input.dim_size(1) &&
             filter.dim_size(1) == input.dim_size(2) &&
             padding == Eigen::PADDING_VALID && data_format == FORMAT_NHWC) {
    // The input data and filter have the same height/width, so call cublas
    // directly.
    const uint64 m = input.dim_size(0);
    const uint64 k =
        filter.dim_size(0) * filter.dim_size(1) * filter.dim_size(2);
    const uint64 n = filter.dim_size(3);

    auto a_ptr = AsDeviceMemory(input.template flat<T>().data(),
                                input.template flat<T>().size());
    auto b_ptr = AsDeviceMemory(filter.template flat<T>().data(),
                                filter.template flat<T>().size());
    auto c_ptr = AsDeviceMemory(output->template flat<T>().data(),
                                output->template flat<T>().size());

    auto no_transpose = perftools::gputools::blas::Transpose::kNoTranspose;
    bool blas_launch_status =
        stream
            ->ThenBlasGemm(no_transpose, no_transpose, n, m, k, 1.0f, b_ptr, n,
                           a_ptr, k, 0.0f, &c_ptr, n)
            .ok();
    if (!blas_launch_status) {
      ctx->SetStatus(errors::Internal("Blas SGEMM launch failed : m=", m,
                                      ", n=", n, ", k=", k));
    }
    return;
  }

  int padding_rows = 0;
  int padding_cols = 0;
  const int64 in_batch = GetTensorDim(input, data_format, 'N');
  int64 in_rows = GetTensorDim(input, data_format, 'H');
  int64 in_cols = GetTensorDim(input, data_format, 'W');
  const int64 in_depths = GetTensorDim(input, data_format, 'C');
  const int64 out_batch = GetTensorDim(*output, data_format, 'N');
  const int64 out_rows = GetTensorDim(*output, data_format, 'H');
  const int64 out_cols = GetTensorDim(*output, data_format, 'W');
  const int64 out_depths = GetTensorDim(*output, data_format, 'C');
  const int64 patch_rows = filter.dim_size(0);
  const int64 patch_cols = filter.dim_size(1);
  if (padding == Eigen::PADDING_SAME) {
    // Total padding on rows and cols is
    // Pr = (R' - 1) * S + Kr - R
    // Pc = (C' - 1) * S + Kc - C
    // where (R', C') are output dimensions, (R, C) are input dimensions, S
    // is stride, (Kr, Kc) are filter dimensions.
    // We pad Pr/2 on the left and Pr - Pr/2 on the right, Pc/2 on the top
    // and Pc - Pc/2 on the bottom.  When Pr or Pc is odd, this means
    // we pad more on the right and bottom than on the top and left.
    padding_rows =
        std::max<int>(0, (out_rows - 1) * row_stride + patch_rows - in_rows);
    padding_cols =
        std::max<int>(0, (out_cols - 1) * col_stride + patch_cols - in_cols);
    const bool rows_odd = (padding_rows % 2 != 0);
    const bool cols_odd = (padding_cols % 2 != 0);
    if (rows_odd || cols_odd) {
      Tensor transformed_input;
      int64 new_in_rows = in_rows + rows_odd;
      int64 new_in_cols = in_cols + cols_odd;
      OP_REQUIRES_OK(
          ctx,
          ctx->allocate_temp(DataTypeToEnum<T>::value,
                             ShapeFromFormat(data_format, in_batch, new_in_rows,
                                             new_in_cols, in_depths),
                             &transformed_input));

      functor::PadInput<GPUDevice, T, int, 4>()(
          ctx->eigen_device<GPUDevice>(), To32Bit(input_param.tensor<T, 4>()),
          {{0, 0}}, {{rows_odd, cols_odd}},
          To32Bit(transformed_input.tensor<T, 4>()), data_format);

      input = transformed_input;
      in_rows = new_in_rows;
      in_cols = new_in_cols;
    }
  }

  if (data_format == FORMAT_NHWC) {
    // Convert the input tensor from NHWC to NCHW.
    TensorShape nchw_shape =
        ShapeFromFormat(FORMAT_NCHW, in_batch, in_rows, in_cols, in_depths);
    if (in_depths > 1) {
      Tensor transformed_input;
      OP_REQUIRES_OK(ctx, ctx->allocate_temp(DataTypeToEnum<T>::value,
                                             nchw_shape, &transformed_input));
      functor::NHWCToNCHW<GPUDevice, T, 4>()(
          ctx->eigen_device<GPUDevice>(),
          const_cast<const Tensor&>(input).tensor<T, 4>(),
          transformed_input.tensor<T, 4>());
      input = transformed_input;
    } else {
      // If depth <= 1, then just reshape.
      CHECK(input.CopyFrom(input, nchw_shape));
    }
  }

  CHECK(padding_rows >= 0 && padding_cols >= 0)
      << "Negative row or col paddings: (" << padding_rows << ", "
      << padding_cols << ")";
  perftools::gputools::dnn::BatchDescriptor input_desc;
  input_desc.set_count(in_batch)
      .set_feature_map_count(in_depths)
      .set_height(in_rows)
      .set_width(in_cols)
      .set_layout(perftools::gputools::dnn::DataLayout::kBatchDepthYX);
  perftools::gputools::dnn::BatchDescriptor output_desc;
  output_desc.set_count(out_batch)
      .set_height(out_rows)
      .set_width(out_cols)
      .set_feature_map_count(out_depths)
      .set_layout(perftools::gputools::dnn::DataLayout::kBatchDepthYX);
  perftools::gputools::dnn::FilterDescriptor filter_desc;
  filter_desc.set_input_filter_height(filter.dim_size(0))
      .set_input_filter_width(filter.dim_size(1))
      .set_input_feature_map_count(filter.dim_size(2))
      .set_output_feature_map_count(filter.dim_size(3));
  perftools::gputools::dnn::ConvolutionDescriptor conv_desc;
  conv_desc.set_vertical_filter_stride(row_stride)
      .set_horizontal_filter_stride(col_stride)
      .set_zero_padding_height(padding_rows / 2)
      .set_zero_padding_width(padding_cols / 2);

  Tensor transformed_filter;
  OP_REQUIRES_OK(ctx, ctx->allocate_temp(
                          DataTypeToEnum<T>::value,
                          TensorShape({filter.dim_size(3), filter.dim_size(2),
                                       filter.dim_size(0), filter.dim_size(1)}),
                          &transformed_filter));

  functor::TransformFilter<GPUDevice, T, int, 4>()(
      ctx->eigen_device<GPUDevice>(), To32Bit(filter.tensor<T, 4>()),
      To32Bit(transformed_filter.tensor<T, 4>()));

  Tensor transformed_output;
  OP_REQUIRES_OK(
      ctx, ctx->allocate_temp(DataTypeToEnum<T>::value,
                              ShapeFromFormat(FORMAT_NCHW, out_batch, out_rows,
                                              out_cols, out_depths),
                              &transformed_output));

  auto input_ptr = AsDeviceMemory(input.template flat<T>().data(),
                                  input.template flat<T>().size());
  auto filter_ptr =
      AsDeviceMemory(transformed_filter.template flat<T>().data(),
                     transformed_filter.template flat<T>().size());
  auto output_ptr =
      AsDeviceMemory(transformed_output.template flat<T>().data(),
                     transformed_output.template flat<T>().size());

  static int64 ConvolveScratchSize = GetCudnnWorkspaceLimit(
      // default value is in bytes despite the name of the environment variable
      "TF_CUDNN_WORKSPACE_LIMIT_IN_MB", 1LL << 32  // 4GB
      );

  int device_id = stream->parent()->device_ordinal();
  ConvParameters conv_parameters = {
      in_batch,      // batch
      in_depths,     // in_depths
      in_rows,       // in_rows
      in_cols,       // in_cols
      out_depths,    // out_depths
      patch_rows,    // filter_rows
      patch_cols,    // filter_cols
      row_stride,    // stride_rows
      col_stride,    // stride_cols
      padding_rows,  // padding_rows
      padding_cols,  // padding_cols
      device_id,     // device_id
  };
  AlgorithmConfig algorithm_config;
  if (cudnn_use_autotune &&
      !AutoTuneConv::GetInstance()->Find(conv_parameters, &algorithm_config)) {
    std::vector<AlgorithmType> algorithms;
    CHECK(stream->parent()->GetConvolveAlgorithms(&algorithms));
    ProfileResult best_result;
    ProfileResult best_result_no_scratch;
    for (auto profile_algorithm : algorithms) {
      // TODO(zhengxq): profile each algorithm multiple times to better
      // accuracy.
      CudnnScratchAllocator scratch_allocator(ConvolveScratchSize, ctx);
      ProfileResult profile_result;
      bool cudnn_launch_status =
          stream
              ->ThenConvolveWithAlgorithm(
                  input_desc, input_ptr, filter_desc, filter_ptr, conv_desc,
                  output_desc, &output_ptr, &scratch_allocator,
                  AlgorithmConfig(profile_algorithm), &profile_result)
              .ok();
      if (cudnn_launch_status) {
        if (profile_result.is_valid()) {
          if (profile_result.elapsed_time_in_ms() <
              best_result.elapsed_time_in_ms()) {
            best_result = profile_result;
          }
          if (scratch_allocator.TotalByteSize() == 0 &&
              profile_result.elapsed_time_in_ms() <
                  best_result_no_scratch.elapsed_time_in_ms()) {
            best_result_no_scratch = profile_result;
          }
        }
      }
    }
    OP_REQUIRES(ctx, best_result.is_valid() &&
                         best_result.algorithm() != kDefaultAlgorithm,
                errors::NotFound("No algorithm worked!"));
    OP_REQUIRES(ctx,
                best_result_no_scratch.is_valid() &&
                    best_result_no_scratch.algorithm() != kDefaultAlgorithm,
                errors::NotFound("No algorithm without scratch worked!"));
    algorithm_config.set_algorithm(best_result.algorithm());
    algorithm_config.set_algorithm_no_scratch(
        best_result_no_scratch.algorithm());
    AutoTuneConv::GetInstance()->Insert(conv_parameters, algorithm_config);
  }

  CudnnScratchAllocator scratch_allocator(ConvolveScratchSize, ctx);
  bool cudnn_launch_status =
      stream
          ->ThenConvolveWithAlgorithm(input_desc, input_ptr, filter_desc,
                                      filter_ptr, conv_desc, output_desc,
                                      &output_ptr, &scratch_allocator,
                                      algorithm_config, nullptr)
          .ok();

  if (!cudnn_launch_status) {
    ctx->SetStatus(errors::Internal(
        "cuDNN launch failure : input shape(", input.shape().DebugString(),
        ") filter shape(", filter.shape().DebugString(), ")"));
  }

  // Convert the output tensor back from NHWC to NCHW.
  if (data_format == FORMAT_NHWC) {
    functor::NCHWToNHWC<GPUDevice, T, 4>()(
        ctx->eigen_device<GPUDevice>(),
        const_cast<const Tensor&>(transformed_output).tensor<T, 4>(),
        output->tensor<T, 4>());
  } else {
    *output = transformed_output;
  }
}

// Forward declarations of the functor specializations for GPU.
namespace functor {
#define DECLARE_GPU_SPEC(T)                                                  \
  template <>                                                                \
  void SpatialConvolution<GPUDevice, T>::operator()(                         \
      const GPUDevice& d, typename TTypes<T, 4>::Tensor output,              \
      typename TTypes<T, 4>::ConstTensor input,                              \
      typename TTypes<T, 4>::ConstTensor filter, int row_stride,             \
      int col_stride, const Eigen::PaddingType& padding);                    \
  extern template struct SpatialConvolution<GPUDevice, T>;                   \
  template <>                                                                \
  void MatMulConvFunctor<GPUDevice, T>::operator()(                          \
      const GPUDevice& d, typename TTypes<T, 2>::Tensor out,                 \
      typename TTypes<T, 2>::ConstTensor in0,                                \
      typename TTypes<T, 2>::ConstTensor in1,                                \
      const Eigen::array<Eigen::IndexPair<Eigen::DenseIndex>, 1>& dim_pair); \
  extern template struct MatMulConvFunctor<GPUDevice, T>;                    \
  template <>                                                                \
  void TransformFilter<GPUDevice, T, int, 4>::operator()(                    \
      const GPUDevice& d, typename TTypes<T, 4, int>::ConstTensor in,        \
      typename TTypes<T, 4, int>::Tensor out);                               \
  extern template struct TransformFilter<GPUDevice, T, int, 4>;              \
  template <>                                                                \
  void PadInput<GPUDevice, T, int, 4>::operator()(                           \
      const GPUDevice& d, typename TTypes<T, 4, int>::ConstTensor in,        \
      const std::array<int, 2>& padding_left,                                \
      const std::array<int, 2>& padding_right,                               \
      typename TTypes<T, 4, int>::Tensor out, TensorFormat data_format);     \
  extern template struct PadInput<GPUDevice, T, int, 4>

DECLARE_GPU_SPEC(float);
//DECLARE_GPU_SPEC(Eigen::half);
#undef DECLARE_GPU_SPEC
}  // namespace functor

// Registration of the GPU implementations.
/*REGISTER_KERNEL_BUILDER(
    Name("Conv2DquantOp").Device(DEVICE_GPU).TypeConstraint<Eigen::half>("T"),
    Conv2DquantOp<GPUDevice, Eigen::half>);*/
REGISTER_KERNEL_BUILDER(
    Name("Conv2DquantOp").Device(DEVICE_GPU).TypeConstraint<float>("T"),
    Conv2DquantOp<GPUDevice, float>);

// To be used inside depthwise_conv_op.cc.
template class LaunchConv2DquantOp<GPUDevice, float>;

#endif  // GOOGLE_CUDAf



// Based on implementation written by Yangqing Jia (jiayq).
template <typename Device, class T>
class Conv2DquantCustomBackpropInputOp : public OpKernel {
 public:
  explicit Conv2DquantCustomBackpropInputOp(OpKernelConstruction* context)
      : OpKernel(context) {
    string data_format;
    OP_REQUIRES_OK(context, context->GetAttr("data_format", &data_format));
    OP_REQUIRES(context, FormatFromString(data_format, &data_format_),
                errors::InvalidArgument("Invalid data format"));
    OP_REQUIRES(context, data_format_ == FORMAT_NHWC,
                errors::InvalidArgument(
                    "Conv2DquantCustomBackpropInputOp only supports NHWC."));
    OP_REQUIRES_OK(context, context->GetAttr("strides", &strides_));
    OP_REQUIRES(context, strides_.size() == 4,
                errors::InvalidArgument("Sliding window strides field must "
                                        "specify 4 dimensions"));
    OP_REQUIRES(
        context, (strides_[0] == 1 && strides_[3] == 1),
        errors::InvalidArgument("Current implementation does not yet support "
                                "strides in the batch and depth dimensions."));
    OP_REQUIRES_OK(context, context->GetAttr("padding", &padding_));
  }

  void Compute(OpKernelContext* context) override {
    const Tensor& input_sizes = context->input(0);
    const Tensor& filter = context->input(1);
    const Tensor& out_backprop = context->input(2);
    OP_REQUIRES(
        context, TensorShapeUtils::IsVector(input_sizes.shape()),
        errors::InvalidArgument(
            "Conv2DquantBackpropInput: input_sizes input must be 1-dim, not ",
            input_sizes.dims()));
    TensorShape input_shape;
    OP_REQUIRES_OK(context, TensorShapeUtils::MakeShape(
                                input_sizes.vec<int32>(), &input_shape));

    Conv2DBackpropDimensions dims;
    OP_REQUIRES_OK(context, Conv2DBackpropComputeDimensions(
                                "Conv2DquantCustomBackpropInput", input_shape,
                                filter.shape(), out_backprop.shape(), strides_,
                                padding_, data_format_, &dims));

    Tensor* in_backprop = nullptr;
    OP_REQUIRES_OK(context,
                   context->allocate_output(0, input_shape, &in_backprop));

#if defined TENSORFLOW_USE_LIBXSMM && defined TENSORFLOW_USE_LIBXSMM_BACKWARD
    if (LaunchXsmmBackwardInputConvolution<Device, T>()(
            context, context->eigen_device<Device>(),
            in_backprop->tensor<T, 4>(), filter.tensor<T, 4>(),
            out_backprop.tensor<T, 4>(), dims.rows.input_size,
            dims.cols.input_size, dims.rows.stride, dims.cols.stride,
            data_format_)) {
      return;
    }
#endif

    // TODO(andydavis) Consider moving code shared with
    // Conv2DCustomBackpropFilterOp into a shared helper function.
    int64 pad_top, pad_bottom;
    int64 pad_left, pad_right;
    OP_REQUIRES_OK(context, GetWindowedOutputSizeVerbose(
                                dims.rows.input_size, dims.rows.filter_size,
                                dims.rows.stride, padding_,
                                &dims.rows.output_size, &pad_top, &pad_bottom));
    OP_REQUIRES_OK(context, GetWindowedOutputSizeVerbose(
                                dims.cols.input_size, dims.cols.filter_size,
                                dims.cols.stride, padding_,
                                &dims.cols.output_size, &pad_left, &pad_right));

    // The total dimension size of each kernel.
    const int filter_total_size =
        dims.rows.filter_size * dims.cols.filter_size * dims.in_depth;
    // The output image size is the spatial size of the output.
    const int output_image_size = dims.rows.output_size * dims.cols.output_size;

    // TODO(andydavis) Get L2/L3 cache sizes from device.
    const size_t l2_cache_size = 256LL << 10;
    const size_t l3_cache_size = 30LL << 20;

    // Use L3 cache size as target working set size.
    const size_t target_working_set_size = l3_cache_size / sizeof(T);

    // Calculate size of matrices involved in MatMul: C = A x B.
    const size_t size_A = output_image_size * dims.out_depth;

    const size_t size_B = filter_total_size * dims.out_depth;

    const size_t size_C = output_image_size * filter_total_size;

    const size_t work_unit_size = size_A + size_B + size_C;

    auto worker_threads = *(context->device()->tensorflow_cpu_worker_threads());

    // Calculate per-thread work unit size.
    const size_t thread_work_unit_size =
        work_unit_size / worker_threads.num_threads;

    // Set minimum per-thread work unit size to size of L2 cache.
    const size_t min_thread_work_unit_size = l2_cache_size / sizeof(T);

    // Use parallel tensor contractions if there is no batching, or if the
    // minimum per-thread work unit size threshold has been exceeded.
    // Otherwise, revert to multiple single-threaded matmul ops running in
    // parallel to keep all threads busy.
    // TODO(andydavis) Explore alternatives to branching the code in this way
    // (i.e. run multiple, parallel tensor contractions in another thread pool).
    const bool use_parallel_contraction =
        dims.batch_size == 1 ||
        thread_work_unit_size >= min_thread_work_unit_size;

    const size_t shard_size =
        use_parallel_contraction
            ? 1
            : (target_working_set_size + work_unit_size - 1) / work_unit_size;

    Tensor col_buffer;
    OP_REQUIRES_OK(context,
                   context->allocate_temp(
                       DataTypeToEnum<T>::value,
                       TensorShape({static_cast<int64>(shard_size),
                                    static_cast<int64>(output_image_size),
                                    static_cast<int64>(filter_total_size)}),
                       &col_buffer));

    // The input offset corresponding to a single input image.
    const int input_offset =
        dims.rows.input_size * dims.cols.input_size * dims.in_depth;
    // The output offset corresponding to a single output image.
    const int output_offset =
        dims.rows.output_size * dims.cols.output_size * dims.out_depth;

    const T* filter_data = filter.template flat<T>().data();
    T* col_buffer_data = col_buffer.template flat<T>().data();
    const T* out_backprop_data = out_backprop.template flat<T>().data();

    auto in_backprop_flat = in_backprop->template flat<T>();
    T* input_backprop_data = in_backprop_flat.data();
    in_backprop_flat.device(context->eigen_device<Device>()) =
        in_backprop_flat.constant(T(0));

    if (use_parallel_contraction) {
      typedef Eigen::TensorMap<Eigen::Tensor<T, 2, Eigen::RowMajor>,
                               Eigen::Unaligned>
          TensorMap;
      typedef Eigen::TensorMap<Eigen::Tensor<const T, 2, Eigen::RowMajor>,
                               Eigen::Unaligned>
          ConstTensorMap;

      // Initialize contraction dims (we need to transpose 'B' below).
      Eigen::array<Eigen::IndexPair<Eigen::DenseIndex>, 1> contract_dims;
      contract_dims[0].first = 1;
      contract_dims[0].second = 1;

      for (int image_id = 0; image_id < dims.batch_size; ++image_id) {
        // Compute gradient into col_buffer.
        TensorMap C(col_buffer_data, output_image_size, filter_total_size);

        ConstTensorMap A(out_backprop_data + output_offset * image_id,
                         output_image_size, dims.out_depth);
        ConstTensorMap B(filter_data, filter_total_size, dims.out_depth);

        C.device(context->eigen_cpu_device()) = A.contract(B, contract_dims);

        Col2im<T>(col_buffer_data, dims.in_depth, dims.rows.input_size,
                  dims.cols.input_size, dims.rows.filter_size,
                  dims.cols.filter_size, pad_top, pad_left, pad_bottom,
                  pad_right, dims.rows.stride, dims.cols.stride,
                  input_backprop_data);

        input_backprop_data += input_offset;
      }
    } else {
      typedef Eigen::Map<
          Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>
          MatrixMap;
      typedef Eigen::Map<const Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic,
                                             Eigen::RowMajor>>
          ConstMatrixMap;

      for (int image_id = 0; image_id < dims.batch_size;
           image_id += shard_size) {
        const int shard_limit =
            std::min(static_cast<int>(shard_size),
                     static_cast<int>(dims.batch_size) - image_id);

        auto shard = [&dims, &pad_top, &pad_left, &pad_bottom, &pad_right,
                      &output_image_size, &filter_total_size,
                      &input_backprop_data, &col_buffer_data,
                      &out_backprop_data, &filter_data, &input_offset,
                      &output_offset, &size_C](int64 start, int64 limit) {
          for (int shard_id = start; shard_id < limit; ++shard_id) {
            T* im2col_buf = col_buffer_data + shard_id * size_C;
            T* input_data = input_backprop_data + shard_id * input_offset;
            const T* out_data = out_backprop_data + shard_id * output_offset;

            // Compute gradient into 'im2col_buf'.
            MatrixMap C(im2col_buf, output_image_size, filter_total_size);

            ConstMatrixMap A(out_data, output_image_size, dims.out_depth);
            ConstMatrixMap B(filter_data, filter_total_size, dims.out_depth);

            C.noalias() = A * B.transpose();

            Col2im<T>(im2col_buf, dims.in_depth, dims.rows.input_size,
                      dims.cols.input_size, dims.rows.filter_size,
                      dims.cols.filter_size, pad_top, pad_left, pad_bottom,
                      pad_right, dims.rows.stride, dims.cols.stride,
                      input_data);
          }
        };
        Shard(worker_threads.num_threads, worker_threads.workers, shard_limit,
              work_unit_size, shard);

        input_backprop_data += input_offset * shard_limit;
        out_backprop_data += output_offset * shard_limit;
      }
    }
  }

 private:
  std::vector<int32> strides_;
  Padding padding_;
  TensorFormat data_format_;

  TF_DISALLOW_COPY_AND_ASSIGN(Conv2DquantCustomBackpropInputOp);
};

#define REGISTER_CPU_KERNELS(T)                                              \
  REGISTER_KERNEL_BUILDER(                                                   \
      Name("Conv2DquantBackpropInput").Device(DEVICE_CPU).TypeConstraint<T>("T"), \
      Conv2DquantCustomBackpropInputOp<CPUDevice, T>);                            \
  REGISTER_KERNEL_BUILDER(Name("Conv2DquantBackpropInput")                        \
                              .Device(DEVICE_CPU)                            \
                              .Label("custom")                               \
                              .TypeConstraint<T>("T"),                       \
                          Conv2DquantCustomBackpropInputOp<CPUDevice, T>);

//TF_CALL_half(REGISTER_CPU_KERNELS);
TF_CALL_float(REGISTER_CPU_KERNELS);
#undef REGISTER_CPU_KERNELS


#if GOOGLE_CUDA
// The slow version (but compiles for GPU)

// A dummy type to group forward backward data autotune results together.
struct ConvBackwardDataAutoTuneGroup {};
typedef AutoTuneSingleton<ConvBackwardDataAutoTuneGroup, ConvParameters,
                          perftools::gputools::dnn::AlgorithmConfig>
    AutoTuneConvBwdData;

// Backprop for input.
template <typename Device, class T>
class Conv2DSlowBackpropInputOp : public OpKernel {
 public:
  explicit Conv2DSlowBackpropInputOp(OpKernelConstruction* context)
      : OpKernel(context) {
    string data_format;
    OP_REQUIRES_OK(context, context->GetAttr("data_format", &data_format));
    OP_REQUIRES(context, FormatFromString(data_format, &data_format_),
                errors::InvalidArgument("Invalid data format"));
    OP_REQUIRES_OK(context, context->GetAttr("strides", &strides_));
    OP_REQUIRES(context, strides_.size() == 4,
                errors::InvalidArgument("Sliding window strides field must "
                                        "specify 4 dimensions"));
    int stride_n = GetTensorDim(strides_, data_format_, 'N');
    int stride_c = GetTensorDim(strides_, data_format_, 'C');
    OP_REQUIRES(
        context, (stride_n == 1 && stride_c == 1),
        errors::InvalidArgument("Current implementation does not yet support "
                                "strides in the batch and depth dimensions."));
    OP_REQUIRES_OK(context, context->GetAttr("use_cudnn_on_gpu", &use_cudnn_));
    use_cudnn_ &= CanUseCudnn();
    cudnn_use_autotune_ = CudnnUseAutotune();
    OP_REQUIRES_OK(context, context->GetAttr("padding", &padding_));
  }

  void Compute(OpKernelContext* context) override {
    using perftools::gputools::dnn::AlgorithmConfig;
    using perftools::gputools::dnn::AlgorithmType;
    using perftools::gputools::dnn::ProfileResult;
    using perftools::gputools::dnn::kDefaultAlgorithm;
    const Tensor& input_sizes = context->input(0);
    const Tensor& filter = context->input(1);
    const Tensor& out_backprop = context->input(2);
    OP_REQUIRES(
        context, TensorShapeUtils::IsVector(input_sizes.shape()),
        errors::InvalidArgument(
            "Conv2DBackpropInput: input_sizes input must be 1-dim, not ",
            input_sizes.dims()));
    TensorShape input_shape;
    OP_REQUIRES_OK(context, TensorShapeUtils::MakeShape(
                                input_sizes.vec<int32>(), &input_shape));
    const TensorShape& filter_shape = filter.shape();

    Conv2DBackpropDimensions dims;
    OP_REQUIRES_OK(context, Conv2DBackpropComputeDimensions(
                                "Conv2DSlowBackpropInput", input_shape,
                                filter_shape, out_backprop.shape(), strides_,
                                padding_, data_format_, &dims));

    Tensor* in_backprop = nullptr;
    OP_REQUIRES_OK(context,
                   context->allocate_output(0, input_shape, &in_backprop));

    const int padding_rows =
        (padding_ == VALID)
            ? 0
            : std::max<int>(0, (dims.rows.output_size - 1) * dims.rows.stride +
                                   dims.rows.filter_size -
                                   dims.rows.input_size);
    const int padding_cols =
        (padding_ == VALID)
            ? 0
            : std::max<int>(0, (dims.cols.output_size - 1) * dims.cols.stride +
                                   dims.cols.filter_size -
                                   dims.cols.input_size);

    // TODO(keveman): cuDNN only supports equal padding on both sides, so only
    // calling it when that is true. Remove this check when (if?) cuDNN starts
    // supporting different padding.
    bool rows_odd = (padding_rows % 2 != 0);
    bool cols_odd = (padding_cols % 2 != 0);

    auto* stream = context->op_device_context()->stream();
    OP_REQUIRES(context, stream, errors::Internal("No GPU stream available."));

    if (!use_cudnn_) {
      context->SetStatus(errors::Unimplemented(
          "Conv2DBackpropInput for GPU is not currently supported "
          "without cudnn"));
      return;
    }

    if (dims.rows.filter_size == 1 && dims.cols.filter_size == 1 &&
        dims.rows.stride == 1 && dims.cols.stride == 1 &&
        data_format_ == FORMAT_NHWC) {
      // 1x1 filter, so call cublas directly.
      const uint64 m =
          dims.batch_size * dims.rows.input_size * dims.cols.input_size;
      const uint64 k = dims.out_depth;
      const uint64 n = dims.in_depth;

      auto a_ptr = AsDeviceMemory(out_backprop.template flat<T>().data(),
                                  out_backprop.template flat<T>().size());
      auto b_ptr = AsDeviceMemory(filter.template flat<T>().data(),
                                  filter.template flat<T>().size());
      auto c_ptr = AsDeviceMemory(in_backprop->template flat<T>().data(),
                                  in_backprop->template flat<T>().size());

      auto transpose = perftools::gputools::blas::Transpose::kTranspose;
      auto no_transpose = perftools::gputools::blas::Transpose::kNoTranspose;

      bool blas_launch_status =
          stream
              ->ThenBlasGemm(transpose, no_transpose, n, m, k, 1.0f, b_ptr, k,
                             a_ptr, k, 0.0f, &c_ptr, n)
              .ok();
      if (!blas_launch_status) {
        context->SetStatus(errors::Internal("Blas SGEMM launch failed : m=", m,
                                            ", n=", n, ", k=", k));
      }
      return;
    } else if (dims.rows.filter_size == dims.rows.input_size &&
               dims.cols.filter_size == dims.cols.input_size &&
               padding_ == VALID && data_format_ == FORMAT_NHWC) {
      // The input data and filter have the same height/width, so call cublas
      // directly.
      const uint64 m = dims.batch_size;
      const uint64 k = dims.out_depth;
      const uint64 n =
          dims.rows.input_size * dims.cols.input_size * dims.in_depth;

      auto a_ptr = AsDeviceMemory(out_backprop.template flat<T>().data(),
                                  out_backprop.template flat<T>().size());
      auto b_ptr = AsDeviceMemory(filter.template flat<T>().data(),
                                  filter.template flat<T>().size());
      auto c_ptr = AsDeviceMemory(in_backprop->template flat<T>().data(),
                                  in_backprop->template flat<T>().size());

      auto transpose = perftools::gputools::blas::Transpose::kTranspose;
      auto no_transpose = perftools::gputools::blas::Transpose::kNoTranspose;

      bool blas_launch_status =
          stream
              ->ThenBlasGemm(transpose, no_transpose, n, m, k, 1.0f, b_ptr, k,
                             a_ptr, k, 0.0f, &c_ptr, n)
              .ok();
      if (!blas_launch_status) {
        context->SetStatus(errors::Internal("Blas SGEMM launch failed : m=", m,
                                            ", n=", n, ", k=", k));
      }
      return;
    }

    TensorShape compatible_input_shape;
    if (rows_odd || cols_odd) {
      // If a padding dimension is odd, we have one more element on the right
      // side or the bottom side. This is unsupported in cudnn. Therefore,
      // we pad that extra element and make it compatible.
      compatible_input_shape = ShapeFromFormat(
          data_format_, dims.batch_size, dims.rows.input_size + rows_odd,
          dims.cols.input_size + cols_odd, dims.in_depth);
    } else {
      compatible_input_shape = input_shape;
    }

    CHECK(padding_rows >= 0 && padding_cols >= 0)
        << "Negative row or col paddings: (" << padding_rows << ", "
        << padding_cols << ")";
    perftools::gputools::dnn::BatchDescriptor input_desc;
    input_desc.set_count(dims.batch_size)
        .set_height(GetTensorDim(compatible_input_shape, data_format_, 'H'))
        .set_width(GetTensorDim(compatible_input_shape, data_format_, 'W'))
        .set_feature_map_count(dims.in_depth)
        .set_layout(perftools::gputools::dnn::DataLayout::kBatchDepthYX);
    perftools::gputools::dnn::BatchDescriptor output_desc;
    output_desc.set_count(dims.batch_size)
        .set_height(dims.rows.output_size)
        .set_width(dims.cols.output_size)
        .set_feature_map_count(dims.out_depth)
        .set_layout(perftools::gputools::dnn::DataLayout::kBatchDepthYX);
    perftools::gputools::dnn::FilterDescriptor filter_desc;
    filter_desc.set_input_filter_height(dims.rows.filter_size)
        .set_input_filter_width(dims.cols.filter_size)
        .set_input_feature_map_count(dims.in_depth)
        .set_output_feature_map_count(dims.out_depth);
    perftools::gputools::dnn::ConvolutionDescriptor conv_desc;
    conv_desc.set_vertical_filter_stride(dims.rows.stride)
        .set_horizontal_filter_stride(dims.cols.stride)
        .set_zero_padding_height(padding_rows / 2)
        .set_zero_padding_width(padding_cols / 2);

    // NOTE(keveman):
    // cuDNN only supports the following layouts :
    // Input  : B x D x R x C
    // Filter : OD x ID x R x C
    // Whereas, we have
    // Input  : B x R x C x D
    // Filter : R x C x ID x OD
    // TransformFilter performs (R x C x ID x OD) => (OD x ID x R x C)
    // The first TransformDepth performs
    // (B x R x C x D) => (B x D x R x C).
    // Since the tensor returned from cuDNN is B x D x R x C also,
    // the second TransformDepth performs
    // (B x D x R x C) => (B x R x C x D).
    Tensor transformed_filter;
    OP_REQUIRES_OK(context, context->allocate_temp(
                                DataTypeToEnum<T>::value,
                                TensorShape({dims.out_depth, dims.in_depth,
                                             dims.rows.filter_size,
                                             dims.cols.filter_size}),
                                &transformed_filter));

    functor::TransformFilter<Device, T, int, 4>()(
        context->eigen_device<Device>(), To32Bit(filter.tensor<T, 4>()),
        To32Bit(transformed_filter.tensor<T, 4>()));

    Tensor transformed_out_backprop;
    if (data_format_ == FORMAT_NHWC) {
      TensorShape nchw_shape =
          ShapeFromFormat(FORMAT_NCHW, dims.batch_size, dims.rows.output_size,
                          dims.cols.output_size, dims.out_depth);
      if (dims.out_depth > 1) {
        OP_REQUIRES_OK(context, context->allocate_temp(
                                    DataTypeToEnum<T>::value, nchw_shape,
                                    &transformed_out_backprop));
        functor::NHWCToNCHW<Device, T, 4>()(
            context->eigen_device<Device>(), out_backprop.tensor<T, 4>(),
            transformed_out_backprop.tensor<T, 4>());
      } else {
        // If depth <= 1, then just reshape.
        CHECK(transformed_out_backprop.CopyFrom(out_backprop, nchw_shape));
      }
    } else {
      transformed_out_backprop = out_backprop;
    }

    Tensor pre_transformed_in_backprop;
    OP_REQUIRES_OK(
        context,
        context->allocate_temp(
            DataTypeToEnum<T>::value,
            ShapeFromFormat(
                FORMAT_NCHW,
                GetTensorDim(compatible_input_shape, data_format_, 'N'),
                GetTensorDim(compatible_input_shape, data_format_, 'H'),
                GetTensorDim(compatible_input_shape, data_format_, 'W'),
                GetTensorDim(compatible_input_shape, data_format_, 'C')),
            &pre_transformed_in_backprop));

    auto out_backprop_ptr =
        AsDeviceMemory(transformed_out_backprop.template flat<T>().data(),
                       transformed_out_backprop.template flat<T>().size());
    auto filter_ptr =
        AsDeviceMemory(transformed_filter.template flat<T>().data(),
                       transformed_filter.template flat<T>().size());
    auto in_backprop_ptr =
        AsDeviceMemory(pre_transformed_in_backprop.template flat<T>().data(),
                       pre_transformed_in_backprop.template flat<T>().size());

    static int64 ConvolveBackwardDataScratchSize = GetCudnnWorkspaceLimit(
        "TF_CUDNN_WORKSPACE_LIMIT_IN_MB", 1LL << 32  // 4GB by default
        );
    CudnnScratchAllocator scratch_allocator(ConvolveBackwardDataScratchSize,
                                            context);
    int device_id = stream->parent()->device_ordinal();
    ConvParameters conv_parameters = {
        dims.batch_size,        // batch
        dims.in_depth,          // in_depths
        input_desc.height(),    // in_rows
        input_desc.width(),     // in_cols
        dims.out_depth,         // out_depths
        dims.rows.filter_size,  // filter_rows
        dims.cols.filter_size,  // filter_cols
        dims.rows.stride,       // stride_rows
        dims.cols.stride,       // stride_cols
        padding_rows,           // padding_rows
        padding_cols,           // padding_cols
        device_id,              // device_id
    };
    AlgorithmConfig algorithm_config;
    if (cudnn_use_autotune_ &&
        !AutoTuneConvBwdData::GetInstance()->Find(conv_parameters,
                                                  &algorithm_config)) {
      std::vector<AlgorithmType> algorithms;
      CHECK(stream->parent()->GetConvolveBackwardDataAlgorithms(&algorithms));
      ProfileResult best_result;
      ProfileResult best_result_no_scratch;
      for (auto profile_algorithm : algorithms) {
        // TODO(zhengxq): profile each algorithm multiple times to better
        // accuracy.
        CudnnScratchAllocator scratch_allocator(ConvolveBackwardDataScratchSize,
                                                context);
        ProfileResult profile_result;
        bool cudnn_launch_status =
            stream
                ->ThenConvolveBackwardDataWithAlgorithm(
                    filter_desc, filter_ptr, output_desc, out_backprop_ptr,
                    conv_desc, input_desc, &in_backprop_ptr, &scratch_allocator,
                    AlgorithmConfig(profile_algorithm), &profile_result)
                .ok();
        if (cudnn_launch_status) {
          if (profile_result.is_valid()) {
            if (profile_result.elapsed_time_in_ms() <
                best_result.elapsed_time_in_ms()) {
              best_result = profile_result;
            }
            if (scratch_allocator.TotalByteSize() == 0 &&
                profile_result.elapsed_time_in_ms() <
                    best_result_no_scratch.elapsed_time_in_ms()) {
              best_result_no_scratch = profile_result;
            }
          }
        }
      }
      OP_REQUIRES(context, best_result.is_valid() &&
                               best_result.algorithm() != kDefaultAlgorithm,
                  errors::NotFound("No algorithm worked!"));
      OP_REQUIRES(context,
                  best_result_no_scratch.is_valid() &&
                      best_result_no_scratch.algorithm() != kDefaultAlgorithm,
                  errors::NotFound("No algorithm without scratch worked!"));
      algorithm_config.set_algorithm(best_result.algorithm());
      algorithm_config.set_algorithm_no_scratch(
          best_result_no_scratch.algorithm());
      AutoTuneConvBwdData::GetInstance()->Insert(conv_parameters,
                                                 algorithm_config);
    }
    bool cudnn_launch_status =
        stream
            ->ThenConvolveBackwardDataWithAlgorithm(
                filter_desc, filter_ptr, output_desc, out_backprop_ptr,
                conv_desc, input_desc, &in_backprop_ptr, &scratch_allocator,
                algorithm_config, nullptr)
            .ok();

    if (!cudnn_launch_status) {
      context->SetStatus(errors::Internal(
          "cuDNN Backward Data function launch failure : input shape(",
          input_shape.DebugString(), ") filter shape(",
          filter_shape.DebugString(), ")"));
      return;
    }

    if (rows_odd || cols_odd) {
      Tensor in_backprop_remove_padding;
      OP_REQUIRES_OK(
          context,
          context->allocate_temp(
              DataTypeToEnum<T>::value,
              ShapeFromFormat(FORMAT_NCHW,
                              GetTensorDim(input_shape, data_format_, 'N'),
                              GetTensorDim(input_shape, data_format_, 'H'),
                              GetTensorDim(input_shape, data_format_, 'W'),
                              GetTensorDim(input_shape, data_format_, 'C')),
              &in_backprop_remove_padding));

      // Remove the padding for odd rows or cols.
      functor::PadInput<GPUDevice, T, int, 4>()(
          context->template eigen_device<GPUDevice>(),
          To32Bit(const_cast<const Tensor&>(pre_transformed_in_backprop)
                      .tensor<T, 4>()),
          {{0, 0}}, {{-rows_odd, -cols_odd}},
          To32Bit(in_backprop_remove_padding.tensor<T, 4>()), FORMAT_NCHW);

      pre_transformed_in_backprop = in_backprop_remove_padding;
    }

    if (data_format_ == FORMAT_NHWC) {
      auto toConstTensor = [](const Tensor& x) -> const Tensor { return x; };
      functor::NCHWToNHWC<Device, T, 4>()(
          context->eigen_device<Device>(),
          toConstTensor(pre_transformed_in_backprop).template tensor<T, 4>(),
          in_backprop->tensor<T, 4>());
    } else {
      *in_backprop = pre_transformed_in_backprop;
    }
  }

 private:
  std::vector<int32> strides_;
  Padding padding_;
  bool use_cudnn_;
  TensorFormat data_format_;
  bool cudnn_use_autotune_;

  TF_DISALLOW_COPY_AND_ASSIGN(Conv2DSlowBackpropInputOp);
};

// Forward declarations of the functor specializations for GPU.
namespace functor {
#define DECLARE_GPU_SPEC(T)                                              \
  template <>                                                            \
  void ShuffleAndReverse<GPUDevice, T, 4, int>::operator()(              \
      const GPUDevice& d, typename TTypes<T, 4, int>::ConstTensor input, \
      const Eigen::DSizes<int, 4>& order,                                \
      const Eigen::array<bool, 4>& reverse_dims,                         \
      typename TTypes<T, 4, int>::Tensor output);                        \
  extern template struct ShuffleAndReverse<GPUDevice, T, 4, int>;        \
  template <>                                                            \
  void InflatePadAndShuffle<GPUDevice, T, 4, int>::operator()(           \
      const GPUDevice& d, typename TTypes<T, 4, int>::ConstTensor input, \
      const Eigen::DSizes<int, 4>& strides,                              \
      const Eigen::array<Eigen::IndexPair<int>, 4>& pad_dims,            \
      const Eigen::DSizes<int, 4>& order,                                \
      typename TTypes<T, 4, int>::Tensor output);                        \
  extern template struct InflatePadAndShuffle<GPUDevice, T, 4, int>;     \
  template <>                                                            \
  void TransformFilter<GPUDevice, T, int, 4>::operator()(                \
      const GPUDevice& d, typename TTypes<T, 4, int>::ConstTensor in,    \
      typename TTypes<T, 4, int>::Tensor out);                           \
  extern template struct TransformFilter<GPUDevice, T, int, 4>;          \
  template <>                                                            \
  void TransformDepth<GPUDevice, T, int>::operator()(                    \
      const GPUDevice& d, typename TTypes<T, 4, int>::ConstTensor in,    \
      const Eigen::DSizes<int, 4>& shuffle,                              \
      typename TTypes<T, 4, int>::Tensor out);                           \
  extern template struct TransformDepth<GPUDevice, T, int>;              \
  template <>                                                            \
  void PadInput<GPUDevice, T, int, 4>::operator()(                       \
      const GPUDevice& d, typename TTypes<T, 4, int>::ConstTensor in,    \
      const std::array<int, 2>& padding_left,                            \
      const std::array<int, 2>& padding_right,                           \
      typename TTypes<T, 4, int>::Tensor out, TensorFormat data_format); \
  extern template struct PadInput<GPUDevice, T, int, 4>;

DECLARE_GPU_SPEC(float);
//DECLARE_GPU_SPEC(Eigen::half);
#undef DECLARE_GPU_SPEC
}  // namespace functor

REGISTER_KERNEL_BUILDER(Name("Conv2DquantBackpropInput")
                            .Device(DEVICE_GPU)
                            .TypeConstraint<float>("T")
                            .HostMemory("input_sizes"),
                        Conv2DSlowBackpropInputOp<GPUDevice, float>);
/*REGISTER_KERNEL_BUILDER(Name("Conv2DquantBackpropInput")
                            .Device(DEVICE_GPU)
                            .TypeConstraint<Eigen::half>("T")
                            .HostMemory("input_sizes"),
                        Conv2DSlowBackpropInputOp<GPUDevice, Eigen::half>);*/
#endif  // GOOGLE_CUDA




// Based on implementation written by Yangqing Jia (jiayq).
template <typename Device, class T>
class Conv2DquantCustomBackpropFilterOp : public OpKernel {
 public:
  explicit Conv2DquantCustomBackpropFilterOp(OpKernelConstruction* context)
      : OpKernel(context) {
    string data_format;
    OP_REQUIRES_OK(context, context->GetAttr("data_format", &data_format));
    OP_REQUIRES(context, FormatFromString(data_format, &data_format_),
                errors::InvalidArgument("Invalid data format"));
    OP_REQUIRES(context, data_format_ == FORMAT_NHWC,
                errors::InvalidArgument(
                    "Conv2DquantCustomBackpropFilterOp only supports NHWC."));
    OP_REQUIRES_OK(context, context->GetAttr("strides", &strides_));
    OP_REQUIRES(context, strides_.size() == 4,
                errors::InvalidArgument("Sliding window strides field must "
                                        "specify 4 dimensions"));
    OP_REQUIRES(
        context, (strides_[0] == 1 && strides_[3] == 1),
        errors::InvalidArgument("Current implementation does not yet support "
                                "strides in the batch and depth dimensions."));
    OP_REQUIRES_OK(context, context->GetAttr("padding", &padding_));
  }

  void Compute(OpKernelContext* context) override {
    const Tensor& input = context->input(0);
    const Tensor& filter_sizes = context->input(1);
    const Tensor& out_backprop = context->input(2);
    OP_REQUIRES(
        context, TensorShapeUtils::IsVector(filter_sizes.shape()),
        errors::InvalidArgument(
            "Conv2DquantCustomBackpropFilter: filter_sizes input must be 1-dim, "
            "not ",
            filter_sizes.dims()));
    TensorShape filter_shape;
    OP_REQUIRES_OK(context, TensorShapeUtils::MakeShape(
                                filter_sizes.vec<int32>(), &filter_shape));

    Conv2DBackpropDimensions dims;
    OP_REQUIRES_OK(context, Conv2DBackpropComputeDimensions(
                                "Conv2DquantCustomBackpropFilter", input.shape(),
                                filter_shape, out_backprop.shape(), strides_,
                                padding_, data_format_, &dims));

    Tensor* filter_backprop;
    OP_REQUIRES_OK(context,
                   context->allocate_output(0, filter_shape, &filter_backprop));

    int64 pad_top, pad_bottom;
    int64 pad_left, pad_right;
    OP_REQUIRES_OK(context, GetWindowedOutputSizeVerbose(
                                dims.rows.input_size, dims.rows.filter_size,
                                dims.rows.stride, padding_,
                                &dims.rows.output_size, &pad_top, &pad_bottom));
    OP_REQUIRES_OK(context, GetWindowedOutputSizeVerbose(
                                dims.cols.input_size, dims.cols.filter_size,
                                dims.cols.stride, padding_,
                                &dims.cols.output_size, &pad_left, &pad_right));

    // The total dimension size of each kernel.
    const int filter_total_size =
        dims.rows.filter_size * dims.cols.filter_size * dims.in_depth;
    // The output image size is the spatial size of the output.
    const int output_image_size = dims.rows.output_size * dims.cols.output_size;

    // Shard 'batch' images into 'shard_size' groups of images to be fed
    // into the parallel matmul. Calculate 'shard_size' by dividing the L3 cache
    // size ('target_working_set_size') by the matmul size of an individual
    // image ('work_unit_size').

    // TODO(andydavis)
    // *) Get L3 cache size from device at runtime (30MB is from ivybridge).
    // *) Consider reducing 'target_working_set_size' if L3 is shared by
    //    other concurrently running tensorflow ops.
    const size_t target_working_set_size = (30LL << 20) / sizeof(T);

    const size_t size_A = output_image_size * filter_total_size;

    const size_t size_B = output_image_size * dims.out_depth;

    const size_t size_C = filter_total_size * dims.out_depth;

    const size_t work_unit_size = size_A + size_B + size_C;

    const size_t shard_size =
        (target_working_set_size + work_unit_size - 1) / work_unit_size;

    Tensor col_buffer;
    OP_REQUIRES_OK(context,
                   context->allocate_temp(
                       DataTypeToEnum<T>::value,
                       TensorShape({static_cast<int64>(shard_size),
                                    static_cast<int64>(output_image_size),
                                    static_cast<int64>(filter_total_size)}),
                       &col_buffer));

    // The input offset corresponding to a single input image.
    const int input_offset =
        dims.rows.input_size * dims.cols.input_size * dims.in_depth;
    // The output offset corresponding to a single output image.
    const int output_offset =
        dims.rows.output_size * dims.cols.output_size * dims.out_depth;

    const T* input_data = input.template flat<T>().data();
    T* col_buffer_data = col_buffer.template flat<T>().data();
    const T* out_backprop_data = out_backprop.template flat<T>().data();
    T* filter_backprop_data = filter_backprop->template flat<T>().data();

    typedef Eigen::TensorMap<Eigen::Tensor<T, 2, Eigen::RowMajor>,
                             Eigen::Unaligned>
        TensorMap;
    typedef Eigen::TensorMap<Eigen::Tensor<const T, 2, Eigen::RowMajor>,
                             Eigen::Unaligned>
        ConstTensorMap;

    TensorMap C(filter_backprop_data, filter_total_size, dims.out_depth);
    C.setZero();

    // Initialize contraction dims (we need to transpose 'A' below).
    Eigen::array<Eigen::IndexPair<Eigen::DenseIndex>, 1> contract_dims;
    contract_dims[0].first = 0;
    contract_dims[0].second = 0;

    auto worker_threads = *(context->device()->tensorflow_cpu_worker_threads());

    for (int image_id = 0; image_id < dims.batch_size; image_id += shard_size) {
      const int shard_limit =
          std::min(static_cast<int>(shard_size),
                   static_cast<int>(dims.batch_size) - image_id);

      auto shard = [&input_data, &col_buffer_data, &dims, &pad_top, &pad_left,
                    &pad_bottom, &pad_right, &input_offset,
                    &size_A](int64 start, int64 limit) {
        for (int shard_id = start; shard_id < limit; ++shard_id) {
          const T* input_data_shard = input_data + shard_id * input_offset;
          T* col_data_shard = col_buffer_data + shard_id * size_A;

          // When we compute the gradient with respect to the filters, we need
          // to do im2col to allow gemm-type computation.
          Im2col<T>(input_data_shard, dims.in_depth, dims.rows.input_size,
                    dims.cols.input_size, dims.rows.filter_size,
                    dims.cols.filter_size, pad_top, pad_left, pad_bottom,
                    pad_right, dims.rows.stride, dims.cols.stride,
                    col_data_shard);
        }
      };
      Shard(worker_threads.num_threads, worker_threads.workers, shard_limit,
            size_A, shard);

      ConstTensorMap A(col_buffer_data, output_image_size * shard_limit,
                       filter_total_size);
      ConstTensorMap B(out_backprop_data, output_image_size * shard_limit,
                       dims.out_depth);

      // Gradient with respect to filter.
      C.device(context->eigen_cpu_device()) += A.contract(B, contract_dims);

      input_data += input_offset * shard_limit;
      out_backprop_data += output_offset * shard_limit;
    }
  }

 private:
  std::vector<int32> strides_;
  Padding padding_;
  TensorFormat data_format_;

  TF_DISALLOW_COPY_AND_ASSIGN(Conv2DquantCustomBackpropFilterOp);
};

#define REGISTER_CPU_KERNELS(T)                                               \
  REGISTER_KERNEL_BUILDER(                                                    \
      Name("Conv2DquantBackpropFilter").Device(DEVICE_CPU).TypeConstraint<T>("T"), \
      Conv2DquantCustomBackpropFilterOp<CPUDevice, T>);                            \
  REGISTER_KERNEL_BUILDER(Name("Conv2DquantBackpropFilter")                        \
                              .Device(DEVICE_CPU)                             \
                              .Label("custom")                                \
                              .TypeConstraint<T>("T"),                        \
                          Conv2DquantCustomBackpropFilterOp<CPUDevice, T>);        


//TF_CALL_half(REGISTER_CPU_KERNELS);
TF_CALL_float(REGISTER_CPU_KERNELS);
#undef REGISTER_CPU_KERNELS

#if GOOGLE_CUDA
// The slow version (but compiles for GPU)

// A dummy type to group forward backward filter autotune results together.
struct ConvBackwardFilterAutoTuneGroup {};
typedef AutoTuneSingleton<ConvBackwardFilterAutoTuneGroup, ConvParameters,
                          perftools::gputools::dnn::AlgorithmConfig>
    AutoTuneConvBwdFilter;

// Backprop for filter.
template <typename Device, class T>
class Conv2DSlowBackpropFilterOp : public OpKernel {
 public:
  explicit Conv2DSlowBackpropFilterOp(OpKernelConstruction* context)
      : OpKernel(context) {
    string data_format;
    OP_REQUIRES_OK(context, context->GetAttr("data_format", &data_format));
    OP_REQUIRES(context, FormatFromString(data_format, &data_format_),
                errors::InvalidArgument("Invalid data format"));
    OP_REQUIRES_OK(context, context->GetAttr("strides", &strides_));
    int stride_n = GetTensorDim(strides_, data_format_, 'N');
    int stride_c = GetTensorDim(strides_, data_format_, 'C');
    OP_REQUIRES(
        context, (stride_n == 1 && stride_c == 1),
        errors::InvalidArgument("Current implementation does not yet support "
                                "strides in the batch and depth dimensions."));
    OP_REQUIRES_OK(context, context->GetAttr("use_cudnn_on_gpu", &use_cudnn_));
    use_cudnn_ &= CanUseCudnn();
    cudnn_use_autotune_ = CudnnUseAutotune();
    OP_REQUIRES_OK(context, context->GetAttr("padding", &padding_));
  }

  void Compute(OpKernelContext* context) override {
    using perftools::gputools::dnn::AlgorithmConfig;
    using perftools::gputools::dnn::AlgorithmType;
    using perftools::gputools::dnn::ProfileResult;
    using perftools::gputools::dnn::kDefaultAlgorithm;
    const Tensor& input = context->input(0);
    const Tensor& filter_sizes = context->input(1);
    const Tensor& out_backprop = context->input(2);
    OP_REQUIRES(
        context, TensorShapeUtils::IsVector(filter_sizes.shape()),
        errors::InvalidArgument(
            "Conv2DBackpropFilter: filter_sizes input must be 1-dim, not ",
            filter_sizes.dims()));
    const TensorShape& input_shape = input.shape();
    TensorShape filter_shape;
    OP_REQUIRES_OK(context, TensorShapeUtils::MakeShape(
                                filter_sizes.vec<int32>(), &filter_shape));

    Conv2DBackpropDimensions dims;
    OP_REQUIRES_OK(context, Conv2DBackpropComputeDimensions(
                                "Conv2DSlowBackpropFilter", input.shape(),
                                filter_shape, out_backprop.shape(), strides_,
                                padding_, data_format_, &dims));

    Tensor* filter_backprop = nullptr;
    OP_REQUIRES_OK(context,
                   context->allocate_output(0, filter_shape, &filter_backprop));

    const int padding_rows =
        (padding_ == VALID)
            ? 0
            : std::max<int>(0, (dims.rows.output_size - 1) * dims.rows.stride +
                                   dims.rows.filter_size -
                                   dims.rows.input_size);
    const int padding_cols =
        (padding_ == VALID)
            ? 0
            : std::max<int>(0, (dims.cols.output_size - 1) * dims.cols.stride +
                                   dims.cols.filter_size -
                                   dims.cols.input_size);

    // TODO(zhengxq): cuDNN only supports equal padding on both sides, so only
    // calling it when that is true. Remove this check when (if?) cuDNN starts
    // supporting different padding.
    bool rows_odd = (padding_rows % 2 != 0);
    bool cols_odd = (padding_cols % 2 != 0);

    auto* stream = context->op_device_context()->stream();
    OP_REQUIRES(context, stream, errors::Internal("No GPU stream available."));

    if (!use_cudnn_) {
      context->SetStatus(errors::Unimplemented(
          "Conv2DBackprop for GPU is not currently supported "
          "without cudnn"));
      return;
    }

    if (dims.rows.filter_size == 1 && dims.cols.filter_size == 1 &&
        dims.rows.stride == 1 && dims.cols.stride == 1 &&
        data_format_ == FORMAT_NHWC) {
      const uint64 m = dims.in_depth;
      const uint64 k =
          dims.batch_size * dims.rows.input_size * dims.cols.input_size;
      const uint64 n = dims.out_depth;

      // The shape of output backprop is
      //   [batch, out_rows, out_cols, out_depth]
      //   From cublas's perspective, it is: n x k
      auto a_ptr = AsDeviceMemory(out_backprop.template flat<T>().data(),
                                  out_backprop.template flat<T>().size());

      // The shape of input is
      //   [batch, in_rows, in_cols, in_depth],
      //   From cublas's perspective, it is: m x k
      auto b_ptr = AsDeviceMemory(input.template flat<T>().data(),
                                  input.template flat<T>().size());

      // the shape of the filter backprop from the conv_2d should be
      //   [1, 1, in_depth, out_depth]
      //   From cublas's perspective, it is: n x m
      auto c_ptr = AsDeviceMemory(filter_backprop->template flat<T>().data(),
                                  filter_backprop->template flat<T>().size());

      bool blas_launch_status =
          stream
              ->ThenBlasGemm(perftools::gputools::blas::Transpose::kNoTranspose,
                             perftools::gputools::blas::Transpose::kTranspose,
                             n, m, k, 1.0f, a_ptr, n, b_ptr, m, 0.0f, &c_ptr, n)
              .ok();
      if (!blas_launch_status) {
        context->SetStatus(errors::Internal("Blas SGEMM launch failed : m=", m,
                                            ", n=", n, ", k=", k));
      }
      return;
    } else if (dims.rows.filter_size == dims.rows.input_size &&
               dims.cols.filter_size == dims.cols.input_size &&
               padding_ == VALID && data_format_ == FORMAT_NHWC) {
      // The input data and filter have the same height/width, so call cublas
      // directly.
      const uint64 m =
          dims.rows.input_size * dims.cols.input_size * dims.in_depth;
      const uint64 k = dims.batch_size;
      const uint64 n = dims.out_depth;

      auto a_ptr = AsDeviceMemory(input.template flat<T>().data(),
                                  input.template flat<T>().size());
      auto b_ptr = AsDeviceMemory(out_backprop.template flat<T>().data(),
                                  out_backprop.template flat<T>().size());
      auto c_ptr = AsDeviceMemory(filter_backprop->template flat<T>().data(),
                                  filter_backprop->template flat<T>().size());

      bool blas_launch_status =
          stream
              ->ThenBlasGemm(perftools::gputools::blas::Transpose::kNoTranspose,
                             perftools::gputools::blas::Transpose::kTranspose,
                             n, m, k, 1.0f, b_ptr, n, a_ptr, m, 0.0f, &c_ptr, n)
              .ok();
      if (!blas_launch_status) {
        context->SetStatus(errors::Internal("Blas SGEMM launch failed : m=", m,
                                            ", n=", n, ", k=", k));
      }
      return;
    }

    Tensor compatible_input;
    if (rows_odd || cols_odd) {
      // If a padding dimension is odd, we have one more element on the right
      // side or the bottom side. This is unsupported in cudnn. Therefore,
      // we pad that extra element and make it compatible.
      OP_REQUIRES_OK(
          context,
          context->allocate_temp(
              DataTypeToEnum<T>::value,
              ShapeFromFormat(data_format_, dims.batch_size,
                              dims.rows.input_size + rows_odd,
                              dims.cols.input_size + cols_odd, dims.in_depth),
              &compatible_input));

      functor::PadInput<GPUDevice, T, int, 4>()(
          context->template eigen_device<GPUDevice>(),
          To32Bit(input.tensor<T, 4>()), {{0, 0}}, {{rows_odd, cols_odd}},
          To32Bit(compatible_input.tensor<T, 4>()), data_format_);
    } else {
      compatible_input = input;
    }

    CHECK(padding_rows >= 0 && padding_cols >= 0)
        << "Negative row or col paddings: (" << padding_rows << ", "
        << padding_cols << ")";
    perftools::gputools::dnn::BatchDescriptor input_desc;
    input_desc.set_count(dims.batch_size)
        .set_height(GetTensorDim(compatible_input, data_format_, 'H'))
        .set_width(GetTensorDim(compatible_input, data_format_, 'W'))
        .set_feature_map_count(dims.in_depth)
        .set_layout(perftools::gputools::dnn::DataLayout::kBatchDepthYX);
    perftools::gputools::dnn::BatchDescriptor output_desc;
    output_desc.set_count(dims.batch_size)
        .set_height(dims.rows.output_size)
        .set_width(dims.cols.output_size)
        .set_feature_map_count(dims.out_depth)
        .set_layout(perftools::gputools::dnn::DataLayout::kBatchDepthYX);
    perftools::gputools::dnn::FilterDescriptor filter_desc;
    filter_desc.set_input_filter_height(dims.rows.filter_size)
        .set_input_filter_width(dims.cols.filter_size)
        .set_input_feature_map_count(dims.in_depth)
        .set_output_feature_map_count(dims.out_depth);
    perftools::gputools::dnn::ConvolutionDescriptor conv_desc;
    conv_desc.set_vertical_filter_stride(dims.rows.stride)
        .set_horizontal_filter_stride(dims.cols.stride)
        .set_zero_padding_height(padding_rows / 2)
        .set_zero_padding_width(padding_cols / 2);

    // NOTE(zhengxq):
    // cuDNN only supports the following layouts :
    // Input  : B x D x R x C
    // Filter : OD x ID x R x C
    // Whereas, we have
    // Input  : B x R x C x D
    // Filter : R x C x ID x OD
    // TransformFilter performs (R x C x ID x OD) => (OD x ID x R x C)
    // The first TransformDepth performs
    // (B x R x C x D) => (B x D x R x C).
    // Since the tensor returned from cuDNN is B x D x R x C also,
    // the second TransformDepth performs
    // (B x D x R x C) => (B x R x C x D).

    Tensor pre_transformed_filter_backprop;
    OP_REQUIRES_OK(context, context->allocate_temp(
                                DataTypeToEnum<T>::value,
                                TensorShape({dims.out_depth, dims.in_depth,
                                             dims.rows.filter_size,
                                             dims.cols.filter_size}),
                                &pre_transformed_filter_backprop));

    Tensor transformed_out_backprop;
    if (data_format_ == FORMAT_NHWC) {
      TensorShape nchw_shape =
          ShapeFromFormat(FORMAT_NCHW, dims.batch_size, dims.rows.output_size,
                          dims.cols.output_size, dims.out_depth);
      if (dims.out_depth > 1) {
        OP_REQUIRES_OK(context, context->allocate_temp(
                                    DataTypeToEnum<T>::value, nchw_shape,
                                    &transformed_out_backprop));
        functor::NHWCToNCHW<Device, T, 4>()(
            context->eigen_device<Device>(), out_backprop.tensor<T, 4>(),
            transformed_out_backprop.tensor<T, 4>());
      } else {
        // If depth <= 1, just reshape.
        CHECK(transformed_out_backprop.CopyFrom(out_backprop, nchw_shape));
      }
    } else {
      transformed_out_backprop = out_backprop;
    }

    Tensor transformed_input;
    if (data_format_ == FORMAT_NHWC) {
      TensorShape nchw_shape = ShapeFromFormat(
          FORMAT_NCHW, GetTensorDim(compatible_input, data_format_, 'N'),
          GetTensorDim(compatible_input, data_format_, 'H'),
          GetTensorDim(compatible_input, data_format_, 'W'),
          GetTensorDim(compatible_input, data_format_, 'C'));
      if (nchw_shape.dim_size(1) > 1) {
        OP_REQUIRES_OK(context,
                       context->allocate_temp(DataTypeToEnum<T>::value,
                                              nchw_shape, &transformed_input));
        functor::NHWCToNCHW<Device, T, 4>()(
            context->eigen_device<Device>(),
            const_cast<const Tensor&>(compatible_input).tensor<T, 4>(),
            transformed_input.tensor<T, 4>());
      } else {
        // If depth <= 1, just reshape.
        CHECK(transformed_input.CopyFrom(compatible_input, nchw_shape));
      }
    } else {
      transformed_input = compatible_input;
    }

    auto out_backprop_ptr =
        AsDeviceMemory(transformed_out_backprop.template flat<T>().data(),
                       transformed_out_backprop.template flat<T>().size());
    auto filter_backprop_ptr = AsDeviceMemory(
        pre_transformed_filter_backprop.template flat<T>().data(),
        pre_transformed_filter_backprop.template flat<T>().size());
    auto input_ptr =
        AsDeviceMemory(transformed_input.template flat<T>().data(),
                       transformed_input.template flat<T>().size());

    static int64 ConvolveBackwardFilterScratchSize = GetCudnnWorkspaceLimit(
        "TF_CUDNN_WORKSPACE_LIMIT_IN_MB", 1LL << 32  // 4GB by default
        );
    int device_id = stream->parent()->device_ordinal();
    ConvParameters conv_parameters = {
        dims.batch_size,        // batch
        dims.in_depth,          // in_depths
        input_desc.height(),    // in_rows
        input_desc.width(),     // in_cols
        dims.out_depth,         // out_depths
        dims.rows.filter_size,  // filter_rows
        dims.cols.filter_size,  // filter_cols
        dims.rows.stride,       // stride_rows
        dims.cols.stride,       // stride_cols
        padding_rows,           // padding_rows
        padding_cols,           // padding_cols
        device_id,              // device_id
    };
    AlgorithmConfig algorithm_config;
    if (cudnn_use_autotune_ &&
        !AutoTuneConvBwdFilter::GetInstance()->Find(conv_parameters,
                                                    &algorithm_config)) {
      std::vector<AlgorithmType> algorithms;
      CHECK(stream->parent()->GetConvolveBackwardFilterAlgorithms(&algorithms));
      ProfileResult best_result;
      ProfileResult best_result_no_scratch;
      for (auto profile_algorithm : algorithms) {
        // TODO(zhengxq): profile each algorithm multiple times to better
        // accuracy.
        CudnnScratchAllocator scratch_allocator(
            ConvolveBackwardFilterScratchSize, context);
        ProfileResult profile_result;
        bool cudnn_launch_status =
            stream
                ->ThenConvolveBackwardFilterWithAlgorithm(
                    input_desc, input_ptr, output_desc, out_backprop_ptr,
                    conv_desc, filter_desc, &filter_backprop_ptr,
                    &scratch_allocator, AlgorithmConfig(profile_algorithm),
                    &profile_result)
                .ok();
        if (cudnn_launch_status) {
          if (profile_result.is_valid()) {
            if (profile_result.elapsed_time_in_ms() <
                best_result.elapsed_time_in_ms()) {
              best_result = profile_result;
            }
            if (scratch_allocator.TotalByteSize() == 0 &&
                profile_result.elapsed_time_in_ms() <
                    best_result_no_scratch.elapsed_time_in_ms()) {
              best_result_no_scratch = profile_result;
            }
          }
        }
      }
      OP_REQUIRES(context, best_result.is_valid() &&
                               best_result.algorithm() != kDefaultAlgorithm,
                  errors::NotFound("No algorithm worked!"));
      OP_REQUIRES(context,
                  best_result_no_scratch.is_valid() &&
                      best_result_no_scratch.algorithm() != kDefaultAlgorithm,
                  errors::NotFound("No algorithm without scratch worked!"));
      algorithm_config.set_algorithm(best_result.algorithm());
      algorithm_config.set_algorithm_no_scratch(
          best_result_no_scratch.algorithm());
      AutoTuneConvBwdFilter::GetInstance()->Insert(conv_parameters,
                                                   algorithm_config);
    }
    CudnnScratchAllocator scratch_allocator(ConvolveBackwardFilterScratchSize,
                                            context);
    bool cudnn_launch_status =
        stream
            ->ThenConvolveBackwardFilterWithAlgorithm(
                input_desc, input_ptr, output_desc, out_backprop_ptr, conv_desc,
                filter_desc, &filter_backprop_ptr, &scratch_allocator,
                algorithm_config, nullptr)
            .ok();

    if (!cudnn_launch_status) {
      context->SetStatus(errors::Internal(
          "cuDNN Backward Filter function launch failure : input shape(",
          input_shape.DebugString(), ") filter shape(",
          filter_shape.DebugString(), ")"));
      return;
    }

    auto toConstTensor = [](const Tensor& x) -> const Tensor { return x; };
    functor::ReverseTransformFilter<Device, T, 4>()(
        context->eigen_device<Device>(),
        toConstTensor(pre_transformed_filter_backprop).template tensor<T, 4>(),
        filter_backprop->tensor<T, 4>());
  }

 private:
  std::vector<int32> strides_;
  Padding padding_;
  bool use_cudnn_;
  TensorFormat data_format_;
  bool cudnn_use_autotune_;

  TF_DISALLOW_COPY_AND_ASSIGN(Conv2DSlowBackpropFilterOp);
};

// Forward declarations of the functor specializations for GPU.
namespace functor {
#define DECLARE_GPU_SPEC(T)                                              \
  template <>                                                            \
  void ShuffleAndReverse<GPUDevice, T, 4, int>::operator()(              \
      const GPUDevice& d, typename TTypes<T, 4, int>::ConstTensor input, \
      const Eigen::DSizes<int, 4>& order,                                \
      const Eigen::array<bool, 4>& reverse_dims,                         \
      typename TTypes<T, 4, int>::Tensor output);                        \
  extern template struct ShuffleAndReverse<GPUDevice, T, 4, int>;        \
  template <>                                                            \
  void InflatePadAndShuffle<GPUDevice, T, 4, int>::operator()(           \
      const GPUDevice& d, typename TTypes<T, 4, int>::ConstTensor input, \
      const Eigen::DSizes<int, 4>& strides,                              \
      const Eigen::array<Eigen::IndexPair<int>, 4>& pad_dims,            \
      const Eigen::DSizes<int, 4>& order,                                \
      typename TTypes<T, 4, int>::Tensor output);                        \
  extern template struct InflatePadAndShuffle<GPUDevice, T, 4, int>;     \
  template <>                                                            \
  void TransformFilter<GPUDevice, T, int, 4>::operator()(                \
      const GPUDevice& d, typename TTypes<T, 4, int>::ConstTensor in,    \
      typename TTypes<T, 4, int>::Tensor out);                           \
  extern template struct TransformFilter<GPUDevice, T, int, 4>;          \
  template <>                                                            \
  void TransformDepth<GPUDevice, T, int>::operator()(                    \
      const GPUDevice& d, typename TTypes<T, 4, int>::ConstTensor in,    \
      const Eigen::DSizes<int, 4>& shuffle,                              \
      typename TTypes<T, 4, int>::Tensor out);                           \
  extern template struct TransformDepth<GPUDevice, T, int>;              \
  template <>                                                            \
  void PadInput<GPUDevice, T, int, 4>::operator()(                       \
      const GPUDevice& d, typename TTypes<T, 4, int>::ConstTensor in,    \
      const std::array<int, 2>& padding_left,                            \
      const std::array<int, 2>& padding_right,                           \
      typename TTypes<T, 4, int>::Tensor out, TensorFormat data_format); \
  extern template struct PadInput<GPUDevice, T, int, 4>;

DECLARE_GPU_SPEC(float);
//DECLARE_GPU_SPEC(Eigen::half);
#undef DECLARE_GPU_SPEC
}  // namespace functor

REGISTER_KERNEL_BUILDER(Name("Conv2DquantBackpropFilter")
                            .Device(DEVICE_GPU)
                            .TypeConstraint<float>("T")
                            .HostMemory("filter_sizes"),
                        Conv2DSlowBackpropFilterOp<GPUDevice, float>);
/*REGISTER_KERNEL_BUILDER(Name("Conv2DquantBackpropFilter")
                            .Device(DEVICE_GPU)
                            .TypeConstraint<Eigen::half>("T")
                            .HostMemory("filter_sizes"),
                        Conv2DSlowBackpropFilterOp<GPUDevice, Eigen::half>);*/
#endif  // GOOGLE_CUDA








/////////////BIAS/////////
////////////////////////////////////////////////////////////////////////


template <typename Device, typename T>
class BiasquantOp;

template <typename T>
class BiasquantOp<CPUDevice, T> : public BinaryOp<T> {
 public:
  typedef CPUDevice Device;
  explicit BiasquantOp(OpKernelConstruction* context) : BinaryOp<T>(context) {
    string data_format;
    if (context->GetAttr("data_format", &data_format).ok()) {
      OP_REQUIRES(context, FormatFromString(data_format, &data_format_),
                  errors::InvalidArgument("Invalid data format"));
    } else {
      data_format_ = FORMAT_NHWC;
    }
    OP_REQUIRES(context, data_format_ == FORMAT_NHWC,
                errors::InvalidArgument("CPU BiasquantOp only supports NHWC."));
    OP_REQUIRES_OK(context, context->GetAttr("quant_int_bits", &quant_int_bits));
  }

  void Compute(OpKernelContext* context) override {
    const Tensor& input = context->input(0);
    const Tensor& bias = context->input(1);

    OP_REQUIRES(context, TensorShapeUtils::IsMatrixOrHigher(input.shape()),
                errors::InvalidArgument("Input tensor must be at least 2D: ",
                                        input.shape().DebugString()));
    OP_REQUIRES(context, TensorShapeUtils::IsVector(bias.shape()),
                errors::InvalidArgument("Biases must be 1D: ",
                                        bias.shape().DebugString()));
    const auto last_dim = input.shape().dims() - 1;
    OP_REQUIRES(
        context, bias.shape().dim_size(0) == input.shape().dim_size(last_dim),
        errors::InvalidArgument(
            "Must provide as many biases as the last dimension "
            "of the input tensor: ",
            bias.shape().DebugString(), " vs. ", input.shape().DebugString()));

    Tensor quant_bias = Tensor(bias);
    auto tensor_data = quant_bias.flat<T>();

    //get the quant scaling factor
    float scaling;
    scaling = std::pow(2, (float)(QUANT_BITS-quant_int_bits));

    //perform quantisation
    for(int i=0;i<bias.shape().dim_size(0);i++){
      tensor_data(i) = round(tensor_data(i)* scaling) / scaling;
    }


    Tensor* output = nullptr;
    OP_REQUIRES_OK(context,
                   context->allocate_output(0, input.shape(), &output));
    if (input.NumElements() == 0) return;

    switch (input.shape().dims()) {
      case 2:
        Compute<2>(context, input, quant_bias, output);
        break;
      case 3:
        Compute<3>(context, input, quant_bias, output);
        break;
      case 4:
        Compute<4>(context, input, quant_bias, output);
        break;
      case 5:
        Compute<5>(context, input, quant_bias, output);
        break;
      default:
        OP_REQUIRES(context, false,
                    errors::InvalidArgument("Only ranks up to 5 supported: ",
                                            input.shape().DebugString()));
    }
  }

  // Add biases for an input matrix of rank Dims, by using the Bias.
  template <int Dims>
  void Compute(OpKernelContext* ctx, const Tensor& input, const Tensor& bias,
               Tensor* output) {
    functor::Bias<Device, T, Dims> functor;
    functor(ctx->eigen_device<Device>(), input.tensor<T, Dims>(), bias.vec<T>(),
            output->tensor<T, Dims>());
  }

 private:
  TensorFormat data_format_;
  int quant_int_bits;
};

#define REGISTER_KERNEL(type)                                         \
  REGISTER_KERNEL_BUILDER(                                            \
      Name("BiasquantAdd").Device(DEVICE_CPU).TypeConstraint<type>("T"),   \
      BiasquantOp<CPUDevice, type>);                                       \
  REGISTER_KERNEL_BUILDER(                                            \
      Name("BiasquantAddV1").Device(DEVICE_CPU).TypeConstraint<type>("T"), \
      BiasquantOp<CPUDevice, type>);

//TF_CALL_NUMBER_TYPES(REGISTER_KERNEL);
TF_CALL_float(REGISTER_KERNEL);
#undef REGISTER_KERNEL

namespace {

void GetBiasValueDims(const Tensor& value_tensor, TensorFormat data_format,
                      int32* batch, int32* height, int32* width,
                      int32* channel) {
  *batch = 1;
  *width = 1;
  *height = 1;
  *channel = 1;
  if (data_format == FORMAT_NHWC) {
    int32 channel_dim = value_tensor.dims() - 1;
    *channel = static_cast<int32>(value_tensor.dim_size(channel_dim));
    for (int32 i = 0; i < channel_dim; i++) {
      *batch *= static_cast<int32>(value_tensor.dim_size(i));
    }
  } else if (data_format == FORMAT_NCHW) {
    int32 channel_dim = value_tensor.dims() - 3;
    int32 height_dim = value_tensor.dims() - 2;
    int32 width_dim = value_tensor.dims() - 1;
    *channel = static_cast<int32>(value_tensor.dim_size(channel_dim));
    *height = static_cast<int32>(value_tensor.dim_size(height_dim));
    *width = static_cast<int32>(value_tensor.dim_size(width_dim));
    for (int32 i = 0; i < channel_dim; i++) {
      *batch *= static_cast<int32>(value_tensor.dim_size(i));
    }
  }
}


template <class T>
struct AccumulatorType {
  typedef T type;
};

// float is faster on the CPU than half, and also more precise,
// so use float for the temporary accumulators.
template <>
struct AccumulatorType<Eigen::half> {
  typedef float type;
};

}  // namespace

template <typename Device, typename T>
class BiasquantGradOp;

template <typename T>
class BiasquantGradOp<CPUDevice, T> : public OpKernel {
 public:
  typedef CPUDevice Device;
  explicit BiasquantGradOp(OpKernelConstruction* context) : OpKernel(context) {
    string data_format;
    if (context->GetAttr("data_format", &data_format).ok()) {
      OP_REQUIRES(context, FormatFromString(data_format, &data_format_),
                  errors::InvalidArgument("Invalid data format"));
    } else {
      data_format_ = FORMAT_NHWC;
    }
    OP_REQUIRES(context, data_format_ == FORMAT_NHWC,
                errors::InvalidArgument("CPU BiasquantGradOp only supports NHWC."));
  }

  void Compute(OpKernelContext* context) override {
    const Tensor& output_backprop = context->input(0);

    OP_REQUIRES(context,
                TensorShapeUtils::IsMatrixOrHigher(output_backprop.shape()),
                errors::InvalidArgument("Input tensor must be at least 2D: ",
                                        output_backprop.shape().DebugString()));

    OP_REQUIRES(
        context, FastBoundsCheck(output_backprop.NumElements(),
                                 std::numeric_limits<int32>::max()),
        errors::InvalidArgument("BiasquantGrad requires tensor size <= int32 max"));

    int32 batch, height, width, channel;
    GetBiasValueDims(output_backprop, data_format_, &batch, &height, &width,
                     &channel);
    Tensor* output = nullptr;
    TensorShape output_shape{channel};
    OP_REQUIRES_OK(context, context->allocate_output(0, output_shape, &output));

    if (channel == 0) {
      return;  // Nothing to do
    } else if (output_backprop.NumElements() == 0) {
      // Eigen often crashes by design on empty tensors, but setZero is safe
      output->template flat<T>().setZero();
    } else {
      Eigen::DSizes<int, 2> two_dims(batch * height * width, channel);
#ifdef EIGEN_HAS_INDEX_LIST
      Eigen::IndexList<Eigen::type2index<0> > reduction_axis;
#else
      Eigen::array<int, 1> reduction_axis = {0};
#endif
      output->template flat<T>().device(context->eigen_device<CPUDevice>()) =
          output_backprop.flat<T>()
              .template cast<typename AccumulatorType<T>::type>()
              .reshape(two_dims)
              .sum(reduction_axis)
              .template cast<T>();
    }
  }

 private:
  TensorFormat data_format_;
};

// Registration of the GPU implementations.
#define REGISTER_KERNEL(type)                                           \
  REGISTER_KERNEL_BUILDER(                                              \
      Name("BiasquantAddGrad").Device(DEVICE_CPU).TypeConstraint<type>("T"), \
      BiasquantGradOp<CPUDevice, type>);

TF_CALL_NUMBER_TYPES(REGISTER_KERNEL);
#undef REGISTER_KERNEL


#if GOOGLE_CUDA
template <typename T>
class BiasquantOp<GPUDevice, T> : public BinaryOp<T> {
 public:
  typedef GPUDevice Device;
  explicit BiasquantOp(OpKernelConstruction* context) : BinaryOp<T>(context) {
    string data_format;
    if (context->GetAttr("data_format", &data_format).ok()) {
      OP_REQUIRES(context, FormatFromString(data_format, &data_format_),
                  errors::InvalidArgument("Invalid data format"));
    } else {
      data_format_ = FORMAT_NHWC;
    }
    OP_REQUIRES_OK(context, context->GetAttr("quant_int_bits", &quant_int_bits));
  }

  void Compute(OpKernelContext* context) override {
    const Tensor& input = context->input(0);
    const Tensor& bias = context->input(1);

    OP_REQUIRES(context, TensorShapeUtils::IsMatrixOrHigher(input.shape()),
                errors::InvalidArgument("Input tensor must be at least 2D: ",
                                        input.shape().DebugString()));
    OP_REQUIRES(context, TensorShapeUtils::IsVector(bias.shape()),
                errors::InvalidArgument("Biases must be 1D: ",
                                        bias.shape().DebugString()));
    int32 batch, height, width, channel;
    GetBiasValueDims(input, data_format_, &batch, &height, &width, &channel);
    OP_REQUIRES(context, bias.shape().dim_size(0) == channel,
                errors::InvalidArgument(
                    "Must provide as many biases as the channel dimension "
                    "of the input tensor: ",
                    bias.shape().DebugString(), " vs. ", channel, " in ",
                    input.shape().DebugString()));


    Tensor quant_bias = Tensor(bias);
    auto tensor_data = quant_bias.flat<T>();
    
    //get the quant scaling factor
    float scaling;
    scaling = std::pow(2, (float)(QUANT_BITS-quant_int_bits));
    /*
    //perform quantisation
    for(int i=0;i<bias.shape().dim_size(0);i++){
      tensor_data.data[i] = round(tensor_data(i)* scaling) / scaling;
    }
    */
    //tensor_data = ((scaling*tensor_data).cast<int>().cast<float>())*(1/scaling);
    //tensor_data = (scaling*tensor_data).cast<float>();
    doquant(tensor_data.dimension(0), scaling, tensor_data.data());

    Tensor* output = nullptr;
    OP_REQUIRES_OK(context,
                   context->allocate_output(0, input.shape(), &output));
    if (input.NumElements() > 0) {
      BiasGPU<T>::compute(context->template eigen_device<Device>(),
                          input.flat<T>().data(), quant_bias.flat<T>().data(),
                          output->flat<T>().data(), batch, width, height,
                          channel, data_format_);
    }
  }

 private:
  TensorFormat data_format_;
  int quant_int_bits;
};

// Registration of the GPU implementations.
#define REGISTER_GPU_KERNEL(type)                                     \
  REGISTER_KERNEL_BUILDER(                                            \
      Name("BiasquantAdd").Device(DEVICE_GPU).TypeConstraint<type>("T"),   \
      BiasquantOp<GPUDevice, type>);                                       \
  REGISTER_KERNEL_BUILDER(                                            \
      Name("BiasquantAddV1").Device(DEVICE_GPU).TypeConstraint<type>("T"), \
      BiasquantOp<GPUDevice, type>);

//TF_CALL_GPU_NUMBER_TYPES(REGISTER_GPU_KERNEL);
TF_CALL_float(REGISTER_GPU_KERNEL);
#undef REGISTER_GPU_KERNEL

template <typename T>
class BiasquantGradOp<GPUDevice, T> : public OpKernel {
 public:
  typedef GPUDevice Device;
  explicit BiasquantGradOp(OpKernelConstruction* context) : OpKernel(context) {
    string data_format;
    if (context->GetAttr("data_format", &data_format).ok()) {
      OP_REQUIRES(context, FormatFromString(data_format, &data_format_),
                  errors::InvalidArgument("Invalid data format"));
    } else {
      data_format_ = FORMAT_NCHW;
    }
  }

  void Compute(OpKernelContext* context) override {
    const Tensor& output_backprop = context->input(0);

    OP_REQUIRES(context,
                TensorShapeUtils::IsMatrixOrHigher(output_backprop.shape()),
                errors::InvalidArgument("Input tensor must be at least 2D: ",
                                        output_backprop.shape().DebugString()));
    int32 batch, height, width, channel;
    GetBiasValueDims(output_backprop, data_format_, &batch, &height, &width,
                     &channel);
    Tensor* output = nullptr;
    TensorShape output_shape{channel};
    OP_REQUIRES_OK(context, context->allocate_output(0, output_shape, &output));
    if (channel == 0) return;
    auto* stream = context->op_device_context()->stream();
    OP_REQUIRES(context, stream, errors::Internal("No GPU stream available."));
    perftools::gputools::DeviceMemoryBase output_ptr(
        output->flat<T>().data(), output->NumElements() * sizeof(T));
    stream->ThenMemZero(&output_ptr, output->NumElements() * sizeof(T));
    if (output_backprop.NumElements() > 0) {
      BiasGradGPU<T>::compute(context->template eigen_device<Device>(),
                              output_backprop.template flat<T>().data(),
                              output->flat<T>().data(), batch, width, height,
                              channel, data_format_);
    }
  }

 private:
  TensorFormat data_format_;
};

// Registration of the GPU implementations.
#define REGISTER_GPU_KERNEL(type)                                       \
  REGISTER_KERNEL_BUILDER(                                              \
      Name("BiasquantAddGrad").Device(DEVICE_GPU).TypeConstraint<type>("T"), \
      BiasquantGradOp<GPUDevice, type>);

//TF_CALL_GPU_NUMBER_TYPES(REGISTER_GPU_KERNEL);
TF_CALL_float(REGISTER_GPU_KERNEL);
#undef REGISTER_GPU_KERNEL

#endif  // GOOGLE_CUDA


/////////////////Activation quantisation
///////////////////////////////////////////////////

template <typename Device, typename T>
class ActivationquantOp;

template <typename T>
class ActivationquantOp<CPUDevice, T> : public UnaryOp<T> {
 public:
  typedef CPUDevice Device;
  explicit ActivationquantOp(OpKernelConstruction* context) : UnaryOp<T>(context) {
    string data_format;
    if (context->GetAttr("data_format", &data_format).ok()) {
      OP_REQUIRES(context, FormatFromString(data_format, &data_format_),
                  errors::InvalidArgument("Invalid data format"));
    } else {
      data_format_ = FORMAT_NHWC;
    }
    OP_REQUIRES(context, data_format_ == FORMAT_NHWC,
                errors::InvalidArgument("CPU ActivationquantOp only supports NHWC."));
    OP_REQUIRES_OK(context, context->GetAttr("quant_int_bits", &quant_int_bits));
  }

  void Compute(OpKernelContext* context) override {
    const Tensor& input = context->input(0);

    OP_REQUIRES(context, TensorShapeUtils::IsMatrixOrHigher(input.shape()),
                errors::InvalidArgument("Input tensor must be at least 2D: ",
                                        input.shape().DebugString()));


    if (input.NumElements() == 0) return;

    Tensor* output = nullptr;
    OP_REQUIRES_OK(context,
                   context->allocate_output(0, input.shape(), &output));

    auto tensor_data = input.flat<T>();
    auto outp_data = output->flat<T>();

    //get the quant scaling factor
    float scaling;
    scaling = std::pow(2, (float)(QUANT_BITS-quant_int_bits)); //the activations are always unsigned and therefore one bit more is available

    //perform quantisation
    for(int i=0;i<tensor_data.dimension(0);i++){
      outp_data(i) = round(tensor_data(i)* scaling) / scaling;
    }



  }

 private:
  TensorFormat data_format_;
  int quant_int_bits;
};

#define REGISTER_KERNEL(type)                                         \
  REGISTER_KERNEL_BUILDER(                                            \
      Name("Activationquant").Device(DEVICE_CPU).TypeConstraint<type>("T"),   \
      ActivationquantOp<CPUDevice, type>);

//TF_CALL_NUMBER_TYPES(REGISTER_KERNEL);
TF_CALL_float(REGISTER_KERNEL);
#undef REGISTER_KERNEL



#if GOOGLE_CUDA
template <typename T>
class ActivationquantOp<GPUDevice, T> : public UnaryOp<T> {
 public:
  typedef GPUDevice Device;
  explicit ActivationquantOp(OpKernelConstruction* context) : UnaryOp<T>(context) {
    string data_format;
    if (context->GetAttr("data_format", &data_format).ok()) {
      OP_REQUIRES(context, FormatFromString(data_format, &data_format_),
                  errors::InvalidArgument("Invalid data format"));
    } else {
      data_format_ = FORMAT_NHWC;
    }
    OP_REQUIRES_OK(context, context->GetAttr("quant_int_bits", &quant_int_bits));
  }

  void Compute(OpKernelContext* context) override {
    const Tensor& input = context->input(0);

    OP_REQUIRES(context, TensorShapeUtils::IsMatrixOrHigher(input.shape()),
                errors::InvalidArgument("Input tensor must be at least 2D: ",
                                        input.shape().DebugString()));

    Tensor* output = nullptr;
    OP_REQUIRES_OK(context,
                   context->allocate_output(0, input.shape(), &output));

    auto tensor_data = input.flat<T>();
    
    //get the quant scaling factor
    float scaling;
    scaling = std::pow(2, (float)(QUANT_BITS-quant_int_bits));
    /*
    //perform quantisation
    for(int i=0;i<bias.shape().dim_size(0);i++){
      tensor_data.data[i] = round(tensor_data(i)* scaling) / scaling;
    }
    */
    //tensor_data = ((scaling*tensor_data).cast<int>().cast<float>())*(1/scaling);
    //tensor_data = (scaling*tensor_data).cast<float>();
    doquant2(tensor_data.dimension(0), scaling, tensor_data.data(), output->flat<T>().data());
  }

 private:
  TensorFormat data_format_;
  int quant_int_bits;
};

// Registration of the GPU implementations.
#define REGISTER_GPU_KERNEL(type)                                     \
  REGISTER_KERNEL_BUILDER(                                            \
      Name("Activationquant").Device(DEVICE_GPU).TypeConstraint<type>("T"),   \
      ActivationquantOp<GPUDevice, type>);

//TF_CALL_GPU_NUMBER_TYPES(REGISTER_GPU_KERNEL);
TF_CALL_float(REGISTER_GPU_KERNEL);
#undef REGISTER_GPU_KERNEL


template <typename Device, typename T>
class ActivationquantGradOp;

template <typename T>
class ActivationquantGradOp<CPUDevice, T> : public OpKernel {
 public:
  typedef CPUDevice Device;
  explicit ActivationquantGradOp(OpKernelConstruction* context) : OpKernel(context) {
    string data_format;
    if (context->GetAttr("data_format", &data_format).ok()) {
      OP_REQUIRES(context, FormatFromString(data_format, &data_format_),
                  errors::InvalidArgument("Invalid data format"));
    } else {
      data_format_ = FORMAT_NHWC;
    }
    OP_REQUIRES(context, data_format_ == FORMAT_NHWC,
                errors::InvalidArgument("CPU ActivationquantGradOp only supports NHWC."));
  }

  void Compute(OpKernelContext* context) override {
    const Tensor& output_backprop = context->input(0);

    OP_REQUIRES(context,
                TensorShapeUtils::IsMatrixOrHigher(output_backprop.shape()),
                errors::InvalidArgument("Input tensor must be at least 2D: ",
                                        output_backprop.shape().DebugString()));

    OP_REQUIRES(
        context, FastBoundsCheck(output_backprop.NumElements(),
                                 std::numeric_limits<int32>::max()),
        errors::InvalidArgument("ActivationquantGrad requires tensor size <= int32 max"));

    Tensor* output = nullptr;
    OP_REQUIRES_OK(context, context->allocate_output(0, output_backprop.shape(), &output));
    CHECK(output->CopyFrom(output_backprop, output_backprop.shape()));
  }

 private:
  TensorFormat data_format_;
};

// Registration of the GPU implementations.
#define REGISTER_KERNEL(type)                                           \
  REGISTER_KERNEL_BUILDER(                                              \
      Name("ActivationquantGrad").Device(DEVICE_CPU).TypeConstraint<type>("T"), \
      ActivationquantGradOp<CPUDevice, type>);

TF_CALL_NUMBER_TYPES(REGISTER_KERNEL);
#undef REGISTER_KERNEL


template <typename T>
class ActivationquantGradOp<GPUDevice, T> : public OpKernel {
 public:
  typedef GPUDevice Device;
  explicit ActivationquantGradOp(OpKernelConstruction* context) : OpKernel(context) {
    string data_format;
    if (context->GetAttr("data_format", &data_format).ok()) {
      OP_REQUIRES(context, FormatFromString(data_format, &data_format_),
                  errors::InvalidArgument("Invalid data format"));
    } else {
      data_format_ = FORMAT_NCHW;
    }
  }

  void Compute(OpKernelContext* context) override {
    const Tensor& output_backprop = context->input(0);

    OP_REQUIRES(context,
                TensorShapeUtils::IsMatrixOrHigher(output_backprop.shape()),
                errors::InvalidArgument("Input tensor must be at least 2D: ",
                                        output_backprop.shape().DebugString()));

    Tensor* output = nullptr;
    OP_REQUIRES_OK(context, context->allocate_output(0, output_backprop.shape(), &output));
    CHECK(output->CopyFrom(output_backprop, output_backprop.shape()));
  }

 private:
  TensorFormat data_format_;
};

// Registration of the GPU implementations.
#define REGISTER_GPU_KERNEL(type)                                       \
  REGISTER_KERNEL_BUILDER(                                              \
      Name("ActivationquantGrad").Device(DEVICE_GPU).TypeConstraint<type>("T"), \
      ActivationquantGradOp<GPUDevice, type>);

//TF_CALL_GPU_NUMBER_TYPES(REGISTER_GPU_KERNEL);
TF_CALL_float(REGISTER_GPU_KERNEL);
#undef REGISTER_GPU_KERNEL

#endif  // GOOGLE_CUDA


REGISTER_OP("Conv2Dquant")
    .Input("input: T")
    .Input("filter: T")
    .Output("output: T")
    .Attr("T: {half, float, double}")
    .Attr("strides: list(int)")
    .Attr("quant_int_bits: int")
    .Attr("use_cudnn_on_gpu: bool = true")
    .Attr(GetPaddingAttrString())
    .Attr(GetConvnetDataFormatAttrString())
    .SetShapeFn(shape_inference::Conv2DShape)
    .Doc(R"doc(
Computes a 2-D convolution given 4-D `input` and `filter` tensors.

Given an input tensor of shape `[batch, in_height, in_width, in_channels]`
and a filter / kernel tensor of shape
`[filter_height, filter_width, in_channels, out_channels]`, this op
performs the following:

1. Flattens the filter to a 2-D matrix with shape
   `[filter_height * filter_width * in_channels, output_channels]`.
2. Extracts image patches from the input tensor to form a *virtual*
   tensor of shape `[batch, out_height, out_width,
   filter_height * filter_width * in_channels]`.
3. For each patch, right-multiplies the filter matrix and the image patch
   vector.

In detail, with the default NHWC format,

    output[b, i, j, k] =
        sum_{di, dj, q} input[b, strides[1] * i + di, strides[2] * j + dj, q] *
                        filter[di, dj, q, k]

Must have `strides[0] = strides[3] = 1`.  For the most common case of the same
horizontal and vertices strides, `strides = [1, stride, stride, 1]`.

strides: 1-D of length 4.  The stride of the sliding window for each dimension
  of `input`. Must be in the same order as the dimension specified with format.
padding: The type of padding algorithm to use.
data_format: Specify the data format of the input and output data. With the
    default format "NHWC", the data is stored in the order of:
        [batch, in_height, in_width, in_channels].
    Alternatively, the format could be "NCHW", the data storage order of:
        [batch, in_channels, in_height, in_width].
)doc");

REGISTER_OP("Conv2DquantBackpropInput")
    .Input("input_sizes: int32")
    .Input("filter: T")
    .Input("out_backprop: T")
    .Output("output: T")
    .Attr("T: {half, float, double}")
    .Attr("strides: list(int)")
    .Attr("use_cudnn_on_gpu: bool = true")
    .Attr(GetPaddingAttrString())
    .Attr(GetConvnetDataFormatAttrString())
    .SetShapeFn([](InferenceContext* c) {
      // NOTE(mrry): We could in principle work out the shape from the
      // gradients and the attrs, but if we do not know orig_input_shape
      // statically, then we are unlikely to know the shape of the
      // gradients either.
      return InputTensorShapeOrUnknown(c, 0 /* input_idx */, 4 /* ndims */);
    })
    .Doc(R"doc(
Computes the gradients of convolution with respect to the input.

input_sizes: An integer vector representing the shape of `input`,
  where `input` is a 4-D `[batch, height, width, channels]` tensor.
filter: 4-D with shape
  `[filter_height, filter_width, in_channels, out_channels]`.
out_backprop: 4-D with shape `[batch, out_height, out_width, out_channels]`.
  Gradients w.r.t. the output of the convolution.
strides: The stride of the sliding window for each dimension of the input
  of the convolution. Must be in the same order as the dimension specified with
  format.
padding: The type of padding algorithm to use.
output: 4-D with shape `[batch, in_height, in_width, in_channels]`.  Gradient
  w.r.t. the input of the convolution.
data_format: Specify the data format of the input and output data. With the
    default format "NHWC", the data is stored in the order of:
        [batch, in_height, in_width, in_channels].
    Alternatively, the format could be "NCHW", the data storage order of:
        [batch, in_channels, in_height, in_width].
)doc");

// TODO(jeff): Instead of 'use_cudnn_for_gpu', maybe we should have a
// more general string attribute ('kernel_impl'?) that can be used to
// select among several possible implementations.
REGISTER_OP("Conv2DquantBackpropFilter")
    .Input("input: T")
    .Input("filter_sizes: int32")
    .Input("out_backprop: T")
    .Output("output: T")
    .Attr("T: {half, float, double}")
    .Attr("strides: list(int)")
    .Attr("use_cudnn_on_gpu: bool = true")
    .Attr(GetPaddingAttrString())
    .Attr(GetConvnetDataFormatAttrString())
    .SetShapeFn([](InferenceContext* c) {
      // NOTE(mrry): We could in principle work out the shape from the
      // gradients and the attrs, but if we do not know orig_input_shape
      // statically, then we are unlikely to know the shape of the
      // gradients either.
      return InputTensorShapeOrUnknown(c, 1 /* input_idx */, 4 /* ndims */);
    })
    .Doc(R"doc(
Computes the gradients of convolution with respect to the filter.

input: 4-D with shape `[batch, in_height, in_width, in_channels]`.
filter_sizes: An integer vector representing the tensor shape of `filter`,
  where `filter` is a 4-D
  `[filter_height, filter_width, in_channels, out_channels]` tensor.
out_backprop: 4-D with shape `[batch, out_height, out_width, out_channels]`.
  Gradients w.r.t. the output of the convolution.
strides: The stride of the sliding window for each dimension of the input
  of the convolution. Must be in the same order as the dimension specified with
  format.
padding: The type of padding algorithm to use.
output: 4-D with shape
  `[filter_height, filter_width, in_channels, out_channels]`.  Gradient w.r.t.
  the `filter` input of the convolution.
data_format: Specify the data format of the input and output data. With the
    default format "NHWC", the data is stored in the order of:
        [batch, in_height, in_width, in_channels].
    Alternatively, the format could be "NCHW", the data storage order of:
        [batch, in_channels, in_height, in_width].
)doc");


REGISTER_OP("BiasquantAdd")
    .Attr("T: numbertype")
    .Input("value: T")
    .Input("bias: T")
    .Attr(GetConvnetDataFormatAttrString())
    .Attr("quant_int_bits: int")
    .Output("output: T")
    .SetShapeFn(shape_inference::BiasAddShape)
    .Doc(R"doc(
Adds `bias` to `value`.

This is a special case of `tf.add` where `bias` is restricted to be 1-D.
Broadcasting is supported, so `value` may have any number of dimensions.

value: Any number of dimensions.
bias: 1-D with size the last dimension of `value`.
data_format: Specify the data format of the input and output data. With the
    default format "NHWC", the bias tensor will be added to the last dimension
    of the value tensor.
    Alternatively, the format could be "NCHW", the data storage order of:
        [batch, in_channels, in_height, in_width].
    The tensor will be added to "in_channels", the third-to-the-last
        dimension.
output: Broadcasted sum of `value` and `bias`.
)doc");
// --------------------------------------------------------------------------

REGISTER_OP("BiasquantAddGrad")
    .Attr("T: numbertype")
    .Input("out_backprop: T")
    .Attr(GetConvnetDataFormatAttrString())
    .Output("output: T")
    .SetShapeFn(shape_inference::BiasAddGradShape)
    .Doc(R"doc(
The backward operation for "BiasAdd" on the "bias" tensor.

It accumulates all the values from out_backprop into the feature dimension.
For NHWC data format, the feature dimension is the last. For NCHW data format,
the feature dimension is the third-to-last.

out_backprop: Any number of dimensions.
output: 1-D with size the feature dimension of `out_backprop`.
data_format: Specify the data format of the input and output data. With the
    default format "NHWC", the bias tensor will be added to the last dimension
    of the value tensor.
    Alternatively, the format could be "NCHW", the data storage order of:
        [batch, in_channels, in_height, in_width].
    The tensor will be added to "in_channels", the third-to-the-last
        dimension.
)doc");

REGISTER_OP("Activationquant")
    .Attr("T: numbertype")
    .Input("value: T")
    .Attr(GetConvnetDataFormatAttrString())
    .Attr("quant_int_bits: int")
    .Output("output: T")
    .SetShapeFn([](::tensorflow::shape_inference::InferenceContext* c) {
      c->set_output(0, c->input(0));
      return Status::OK();
    })
    .Doc(R"doc(
Quantises the activation between layers with quant_int_bits integer bits and
prcision beeing QUANT_BITS
)doc");
// --------------------------------------------------------------------------


REGISTER_OP("ActivationquantGrad")
    .Attr("T: numbertype")
    .Input("out_backprop: T")
    .Attr(GetConvnetDataFormatAttrString())
    .Output("output: T")
    .SetShapeFn([](::tensorflow::shape_inference::InferenceContext* c) {
      c->set_output(0, c->input(0));
      return Status::OK();
    })
    .Doc(R"doc(
The backward operation for "Activationquant" on the input tensor.

Just passes the backprop tensor to the output.
)doc");

}