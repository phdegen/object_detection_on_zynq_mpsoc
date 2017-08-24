# TENSORFLOW QUANTISATION MODULE BUILD

The three source files include a header file, a CUDA file that compiles the quantisation function on GPU and the c++ file that containes all the modules.

The whole tensorflow repository is needed to compile the files as various includes are needed.

The quantised modules are bascially taken from the original modules and adapted in order to perform quantisation in between.


The compile command on the authors machine with the previously compiled cuda file is the following:

    > g++ -std=c++11 -shared quantized_conv2d.cc quant.o -o quantized.so -fPIC -I '/home/msc17f2/.local/lib/python2.7/site-packages/tensorflow/include' -I . -I '/usr/local/cuda/include' -O2 -D GOOGLE_CUDA=1 -fpermissive

where the files would be located in the root directory of the tensorflow repository. The used tensorflow repo commit is 63b3fea438b1fc700db38b77ca691e083b63bb5f .
