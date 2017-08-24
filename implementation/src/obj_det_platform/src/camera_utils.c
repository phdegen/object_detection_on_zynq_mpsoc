#include "camera_utils.h"

fc2Error error;
fc2Context context;
fc2PGRGuid guid;
unsigned int numCameras = 0;
fc2Image rawImage;
fc2Image convertedImage;




int launch_cam(activation_t * image){

	fc2Format7ImageSettings imgSettings;
	unsigned int packetSize;
	float percentage;

	check_error(fc2CreateContext(&context),"fc2CreateContext");

	check_error(fc2GetNumOfCameras(context, &numCameras),"fc2GetNumOfCameras");

    if (numCameras == 0)
    {
        fc2DestroyContext(context);

        // No cameras detected
        printf("No cameras detected.\n");
        printf("Press Enter to exit...\n");
        getchar();
        return -1;
    }

    check_error(fc2GetCameraFromIndex(context, 0, &guid),"fc2GetCameraFromIndex");

    check_error(fc2Connect(context, &guid),"fc2Connect");

    check_error(fc2GetFormat7Configuration(context, &imgSettings, &packetSize,&percentage),"fc2GetFormat7Configuration");

	printf("the packetsize is  %u and the percentage is %f\n",packetSize,percentage);
	printf("at the moment mode is %u, offsetx is %u offsety is %u width is %u"
			" height is %u, pixelformat is %u \n\n",
			imgSettings.mode,imgSettings.offsetX,imgSettings.offsetY,imgSettings.width,
			imgSettings.height,imgSettings.pixelFormat);

    check_error(fc2StartCapture(context),"fc2StartCapture");

    check_error(fc2CreateImage(&rawImage),"fc2CreateImage,rawImg");

    check_error( fc2CreateImage(&convertedImage),"fc2CreateImage,convertedImg");

    check_error( fc2SetImageData(&convertedImage, (unsigned char *)image,imgSettings.width*imgSettings.height*3),"fc2SetImageData");


	return 0;
}

int grab_image(){

	check_error(fc2RetrieveBuffer(context, &rawImage),"fc2RetrieveBuffer");

	check_error(fc2ConvertImageTo(FC2_PIXEL_FORMAT_BGR, &rawImage, &convertedImage),"fc2ConvertImageTo BGR");



	return 0;
}


int end_cam(){

	check_error(fc2DestroyImage(&rawImage),"fc2DestroyImage rawImg");

	check_error(fc2DestroyImage(&convertedImage),"fc2DestroyImage convImg");

	check_error(fc2StopCapture(context),"fc2StopCapture");

	check_error(fc2DestroyContext(context),"fc2DestroyContext");

	return 0;
}



int check_error(fc2Error error, char functionwitherr_string[]){

    if (error != FC2_ERROR_OK)
    {
    	printf("Error in");
    	//printf(functionwitherr_string);
    	printf(": %d\n\n", error);
    }

	return 0;
}
