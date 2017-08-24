#include "const.def"
#include "finalise_output.h"
#include <stdio.h>
#define BOX_THICKNESS	2
//int colorcodes[NUM_CLASSES][3] = {{255,191,0},{0,191,255},{255,0,191}};
activation_t colorcodes[NUM_CLASSES][3] = {{-1,-65,0},{0,-65,-1},{-1,0,-65}}; // signed version

int draw_boxes(activation_t * img, int * boxes, int numboxes){
	int i;
	int x,y,xmin,xmax,ymin,ymax, class;
	int count;

	for(i=0;i<numboxes;i++){
		count = 0;
		x= xmin = boxes[i*6+0];
		y= ymin = boxes[i*6+1];
		xmax = boxes[i*6+2];
		ymax = boxes[i*6+3];
		class = boxes[i*6+4];


		do{
			img[y*IMG_WIDTH*3+x*3]	= colorcodes[class][0];
			img[y*IMG_WIDTH*3+x*3+1]= colorcodes[class][1];
			img[y*IMG_WIDTH*3+x*3+2]= colorcodes[class][2];

			x++;
			if(x>=xmax){
				x=xmin;
				y++;
				count++;
			}
			if(count == BOX_THICKNESS){
				y = ymax - BOX_THICKNESS;
				y = y<0 ? 0:y;
			}

		}while(count< 2* BOX_THICKNESS);
		count = 0;
		x= xmin;
		y= ymin;
		do{
			img[y*IMG_WIDTH*3+x*3]	= colorcodes[class][0];
			img[y*IMG_WIDTH*3+x*3+1]= colorcodes[class][1];
			img[y*IMG_WIDTH*3+x*3+2]= colorcodes[class][2];

			y++;
			if(y>=ymax){
				y=ymin;
				x++;
				count++;
			}
			if(count == BOX_THICKNESS){
				x = xmax - BOX_THICKNESS;
				x = x<0 ? 0:x;
			}

		}while(count< 2* BOX_THICKNESS);	
	}

	return 0;
}
