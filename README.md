# OBJECT DETECTION ON THE ZYNQ MPSOC

The repository contains the main work of the Master thesis conducted by Ph. Degen in summer 2017. The different directories are quickly explained in the following.

* doc/ : The doc directory contains the actual master thesis report and a guide that illustrates what is needed to run a compilation and/or run the application on the board/system.

* example/ : The example directory contains a sample sd-card that includes all needed files and a README with some instruction how to proceed.

* implementation/ : The implementation directory contains all the files used for building the application. This goes from the actual sources in the SDSOC format to supporting libraries over the filter weights and sample images. An own README gives more details over the folders/files of that directory.

* quantisation/ : The quantisation folder has the compiled files and an example python-code that illustrates how to use the quantised modules that were made during the project. The compiled file implements a 8 bit signed quantisation. Further the source files of the custom modules are provided with a short [illustration](quantisation/sources/README.md) on how to compile the TensorFlow module on its own. (Precision adaptation ect..)


For a general overview on the proceeding for running the system please check out the [guide](doc/setup_guide.md) in the doc/ directory.

The master thesis' report can be found [here](doc/report.pdf).
