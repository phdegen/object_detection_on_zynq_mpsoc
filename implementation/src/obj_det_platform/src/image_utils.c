#include "const.def"
#include "image_utils.h"
#include <stdlib.h>
#include <stdio.h>

char inp_img_path[] = "img1.bin";
char outp_img_path[] = "img_out.bin";
//float rgb_offset[3] = {103.939, 116.779, 123.68};
//activation_t rgb_offset[3] = {52,58,62}; //round(mean/2)
img_t rgb_offset[3] = {104,117,124};
FILE *fp;


int load_image(activation_t * image){

	fp = fopen(inp_img_path,"r"); // read mode
	if( fp == NULL )
	{
	   perror("Error while opening the file.\n");
	   return -1;
	}

	fread(image, sizeof(activation_t),IMG_WIDTH*IMG_HEIGHT*3,fp);

	fclose(fp);

	return 0;
}

int save_image(activation_t * image){

	fp = fopen(outp_img_path,"w"); // write mode
	if( fp == NULL )
	{
	   perror("Error while opening the file.\n");
	   return -1;
	}

	fwrite(image, sizeof(activation_t),IMG_WIDTH*IMG_HEIGHT*3,fp);

	fclose(fp);

	return 0;
}

int preprocessing(activation_t * image){
	int q,k;
	for(q=0;q<3;q++){
		for(k=0;k<IMG_WIDTH*IMG_HEIGHT;k++){
			image[k*3+q] = (activation_t)(((img_t)image[k*3+q]+1 - rgb_offset[q])>>1); //including rounding +1
		}
	}

	return 0;
}

int postprocessing(activation_t * image){
	int q,k;
	for(q=0;q<3;q++){
		for(k=0;k<IMG_WIDTH*IMG_HEIGHT;k++){
			image[k*3+q] = (activation_t)((((img_t)(image[k*3+q]))<<1)-1 + rgb_offset[q]);
		}
	}

	return 0;
}

int testing_save_image(activation_t * image){

	fp = fopen(outp_img_path,"w"); // write mode
	if( fp == NULL )
	{
	   perror("Error while opening the file.\n");
	   return -1;
	}

	fwrite(image, sizeof(activation_t),2*8*8,fp);

	fclose(fp);

	return 0;
}
