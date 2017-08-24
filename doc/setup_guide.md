# Setup Guide Object Detection on Zynq MPSOC


This setup guide introduces the reader to the steps required to compile and run the application on the zcu102 board by Xilinx.

The Xilinx toolsuide used is of the release 2016.3. Different versions might lead to misfunctioning of the compilation.

For both designs, after opening the implementation/src/ folder as the workspace in SDSoC the path for the includes and for the libraries has to be set in all the projects. 

In order to analyse data (and illustrate the binary images) openCV in python is used. This has to be downloaded if not available. 



##Object Detection from File

For running the application from file the project has to simply be built in SDSoC and the respective sd_card folder in the Release directory has to be copied to an sd-card.
Additionally to that the model weights and a respective image has to be stored on the sd-card analogously to the example sd-card in this repository. For the application without hardware accelerator another weights file without padding is needed.

The applied image size is 1242*375.


## Object Detection with GreyPoint Flea3 Cam and Networking

In order to use the application with the camera and the networking following additional steps are required.

* Adapt the server's ip address in the network_utils.c file as well as its port.
* Adapt and set the port to the same value in the server application (implementation/utils/pythonserver.py).

The sd-card additionally needs both files camera_init.elf and start_cam.sh. The sample image is however not needed.


Before starting the application on the board the server application has to be launched (>> python pythonserver.py) on a machine that has installed openCV on python. Further the start_cam.sh script has to be launched on the board after the camera is attached by USB.

Then launching the main .elf file will start the application. The applied image size in this application is 1232*374 pixels. The received images will be shown at the server with openCV.
Sometimes the network system has to synchronise and some images are not displayed.




# Use of Quantisation Modules

First of all TensorFlow in python is needed. Then the precompiled modules can be used as quantised convolution/bias/activation modules. The file 'quantisation/quant_modules.py' gives an example of how to initialise respective modules.
For modification and recompiling the modules, some brief instructions are in the README file. For that case the whole tensorflow repository is needed.

