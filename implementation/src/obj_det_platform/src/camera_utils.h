#include "C/FlyCapture2_C.h"
#include "model_types.h"

#include <stdlib.h>
#include <stdio.h>

int launch_cam(activation_t * image);

int grab_image();

int end_cam();

int check_error(fc2Error error, char functionwitherr_string[]);
