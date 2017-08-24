This directory is arranged followingly:

* data/ : Model weights and sample images already in binary(raw) format. Further info in the respective READMEs

* src/ : The source directory contains 3 different implementations of the quantised SqueezDet algorithm. One is for the complete platform including the camera and the network output and the algorithm running with the hardware accelerator; the second is an implementation running on a sample image with the hardware accelerator; the third implementation runs the algorithm on a sample image and only on the ARM CPU(s) of the Zynq SoC. By default OpenMP is not activated.
This directory is a SDSoC workspace and all implementations are SDx projects with the respective files.

* includes/ : Contains the includes needed to build the platform project. The includes are mainly for the camera attached in the platform.

* lib/ : Includes all libraries that are used mainly for the compilation with the camera module.

* utils/ : Includes helper programs such as for online camera initialisation and calibration, the needed server module in python and a helper to illustrate the processed image.

* zcu102_linux_platform/ : Contains a built SDSoc platform with root/root username/pw. The built linux features a fixed specific MAC adress: 00:0a:35:03:5a:5b and all necessary libraries. 



