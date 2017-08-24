import numpy as np
import tensorflow as tf

from tensorflow.python.framework import ops
from tensorflow.python.ops import array_ops

#loading library
quant_conv_module = tf.load_op_library('quantized.so')

# couple gradients to forward function
@ops.RegisterGradient("Conv2Dquant")
def _Conv2DquantGrad(op, grad):
  return [quant_conv_module.conv2_dquant_backprop_input(
      array_ops.shape(op.inputs[0]), op.inputs[1], grad, op.get_attr("strides"),
      op.get_attr("padding"), op.get_attr("use_cudnn_on_gpu"),
      op.get_attr("data_format")),
          quant_conv_module.conv2_dquant_backprop_filter(op.inputs[0],
                                        array_ops.shape(op.inputs[1]), grad,
                                        op.get_attr("strides"),
                                        op.get_attr("padding"),
                                        op.get_attr("use_cudnn_on_gpu"),
                                        op.get_attr("data_format"))]

@ops.RegisterGradient("BiasquantAdd")
def _BiasquantAddGrad(op, received_grad):
  try:
    data_format = op.get_attr("data_format")
  except ValueError:
    data_format = None
  return (received_grad, quant_conv_module.biasquant_add_grad(out_backprop=received_grad,
                                                  data_format=data_format))

@ops.RegisterGradient("Activationquant")
def _ActivationquantGrad(op, received_grad):
  try:
    data_format = op.get_attr("data_format")
  except ValueError:
    data_format = None
  return quant_conv_module.activationquant_grad(out_backprop=received_grad,
                                                  data_format=data_format)

# use quantised modules

# quant_int_bits: number of integer bits of 8bit value
out = quant_conv_module.activationquant(input, quant_int_bits = 6)

# quant_int_bits: number of integer bits of 8bit value
# padding 'SAME' or 'VALID'
# kernel incorporates convolution weights
conv = quant_conv_module.conv2_dquant(input, kernel, [1, stride, stride, 1], padding=padding, quant_int_bits = 8, name='convolution')

# quant_int_bits: number of integer bits of 8bit value
# biases incorporates bias weights
conv_bias = quant_conv_module.biasquant_add(conv, biases, quant_int_bits = 4, name='bias_add')



