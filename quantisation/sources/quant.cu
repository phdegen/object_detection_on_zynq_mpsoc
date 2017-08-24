#if GOOGLE_CUDA
#define EIGEN_USE_GPU
#include "third_party/eigen3/unsupported/Eigen/CXX11/Tensor"


__global__ void quantize(int N, float scale, float *tensor)
{
  int i = blockIdx.x*blockDim.x + threadIdx.x;
  if (i<N) tensor[i] = round(scale*tensor[i])/scale;
}

__global__ void quantize2(int N, float scale, float *tensor, float *output)
{
  int i = blockIdx.x*blockDim.x + threadIdx.x;
  if (i<N) output[i] = round(scale*tensor[i])/scale;
}



void doquant(int N, float scale, float *tensor){

quantize<<<(N+255)/256, 256>>>(N, scale, tensor);

}

void doquant2(int N, float scale, float *tensor, float *output){

quantize2<<<(N+255)/256, 256>>>(N, scale, tensor, output);

}

#endif
