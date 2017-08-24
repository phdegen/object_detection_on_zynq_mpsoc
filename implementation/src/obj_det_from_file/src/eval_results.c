#include "const.def"
#include "eval_results.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

//Numbers for anchor calculation
float dw = IMG_WIDTH / (CONVDET_WIDTH + 1);
float dh = IMG_HEIGHT / (CONVDET_HEIGHT + 1);
float anchor_boxes[NUM_ANCHOR][2] = { {36, 37},{366, 174},{115, 59},{162, 87},{38, 90},{258,173},
									{224, 108},{78, 170},{72, 43}};


finish_t *probabilities;//[NUM_ANCHOR * NUM_CONVDET_PXLS];
int classArr[NUM_ANCHOR*NUM_CONVDET_PXLS];
int topclasses[TOP_N_DETECTION];
int indices[NUM_ANCHOR*NUM_CONVDET_PXLS];
int num_probs_per_class[NUM_CLASSES];
finish_t kept_proba_per_class[NUM_CLASSES][TOP_N_DETECTION];
int idx_kept_proba_per_class[NUM_CLASSES][TOP_N_DETECTION];
float bbox_kept_proba_per_class[NUM_CLASSES][TOP_N_DETECTION][4];
int keep_bbox[NUM_CLASSES][TOP_N_DETECTION];

int comp_from_idx(const void * a, const void * b);
int comp(const void * a, const void * b);

int eval_results(activation_t * inp_buf, int * outp_boxes){
	probabilities = malloc(NUM_ANCHOR * NUM_CONVDET_PXLS* sizeof(finish_t));
	int num_boxes;

	get_highest_classes_per_anchor((finish_t *) inp_buf);

	get_highest_prob_anchors();

	num_boxes = perform_nms((finish_t *)inp_buf, outp_boxes);

	return num_boxes;
}






int get_highest_classes_per_anchor(finish_t * buf_1){
		//For each anchor get class with max probability (save prob value and class index)
	int pixels, anchors,classes;
	for(pixels=0;pixels<NUM_CONVDET_PXLS;pixels++){
		for(anchors=0;anchors<NUM_ANCHOR;anchors++){
			finish_t max=buf_1[pixels*CONVDET_CHANNELS+anchors*NUM_CLASSES];
			int cls=0;
			for(classes=1;classes<NUM_CLASSES;classes++){
				if(max < buf_1[pixels*CONVDET_CHANNELS+anchors*NUM_CLASSES+classes]){
					max = buf_1[pixels*CONVDET_CHANNELS+anchors*NUM_CLASSES+classes];
					cls = classes;
				}
			}
			probabilities[pixels * NUM_ANCHOR + anchors] = max;
			classArr[pixels * NUM_ANCHOR + anchors] = cls;
		}
	}


	return 0;
}


int get_highest_prob_anchors(){
	int i,j;
	//Get all indices into array
	for(i=0;i<NUM_ANCHOR*NUM_CONVDET_PXLS;i++){
		indices[i]=i;
	}

	qsort(indices, NUM_ANCHOR*NUM_CONVDET_PXLS, sizeof(int), comp_from_idx);

	qsort(probabilities, NUM_ANCHOR*NUM_CONVDET_PXLS, sizeof(finish_t), comp);

	for(j=0;j < TOP_N_DETECTION;j++){
		topclasses[j] = classArr[indices[j]];
	}
	return 0;
}



int comp_from_idx(const void * a, const void * b){
    int ia = *(int *)a;
    int ib = *(int *)b;
    return probabilities[ib] > probabilities[ia] ? 1 : -1; // highest values first (descending order)
}

int comp(const void * a, const void * b){
	finish_t fa = *(finish_t *)a;
	finish_t fb = *(finish_t *)b;
    return fb > fa ? 1 : -1;  // highest values first (descending order)
}


int perform_nms(finish_t * convDet_fm, int * output_bbxs){
	int i,j,k,cls;
	int final_num_boxes = 0;

	//Select the ones up to threshold proba into separate arrays
	for(i=0;i<TOP_N_DETECTION;i++){
		if(probabilities[i]<PLOT_PROB_THRESH)
			continue; // break; ?
		cls = topclasses[i];
		kept_proba_per_class[cls][num_probs_per_class[cls]] = probabilities[i];
		idx_kept_proba_per_class[cls][num_probs_per_class[cls]] = indices[i];
		num_probs_per_class[cls]++;
	}

	extract_boxes(convDet_fm);

	for(j=0;j<NUM_CLASSES;j++){
		batch_nms(j);
	}

	for(j=0;j<NUM_CLASSES;j++){
		for(k=0;k<num_probs_per_class[j];k++){
			if (keep_bbox[j][k]==1) {
				output_bbxs[final_num_boxes*6+0] = (int) bbox_kept_proba_per_class[j][k][0];
				output_bbxs[final_num_boxes*6+1] = (int) bbox_kept_proba_per_class[j][k][1];
				output_bbxs[final_num_boxes*6+2] = (int) bbox_kept_proba_per_class[j][k][2];
				output_bbxs[final_num_boxes*6+3] = (int) bbox_kept_proba_per_class[j][k][3];
				output_bbxs[final_num_boxes*6+4] = j;
				output_bbxs[final_num_boxes*6+5] = kept_proba_per_class[j][k];

				final_num_boxes += 1;
			}
		}
	}
	return final_num_boxes;
}


int extract_boxes(finish_t * featuremap){
	int i,j;
	for(i=0;i<NUM_CLASSES;i++){
		for(j=0;j<num_probs_per_class[i];j++){
			int index, pxl ,x ,y , anchor, offset;
			float box_center_x, box_center_y, box_width, box_height;
			index = idx_kept_proba_per_class[i][j];
			pxl=index/NUM_ANCHOR;
			anchor = index%NUM_ANCHOR;
			x = pxl % CONVDET_WIDTH;
			y = pxl / CONVDET_WIDTH;
			offset = pxl*CONVDET_CHANNELS+NUM_ANCHOR*(NUM_CLASSES+1)+ 4*anchor;

			//calculate bbox from feature map and anchors
			box_center_x =  (x+1) * dw + featuremap[offset]  * anchor_boxes[anchor][0];
			box_center_y = (y+1) * dh + featuremap[offset +1] * anchor_boxes[anchor][1];
			box_width = anchor_boxes[anchor][0] * expf(featuremap[offset+2]);
			box_height = anchor_boxes[anchor][1] * expf(featuremap[offset+3]);

			//transform to xmin,xmax,ymin,ymax format inlcuding trimming
			bbox_kept_proba_per_class[i][j][0] = fmin(fmax(0.0, (box_center_x-0.5*box_width)), IMG_WIDTH-1); //xmin
			bbox_kept_proba_per_class[i][j][1] = fmin(fmax(0.0, (box_center_y-0.5*box_height)), IMG_HEIGHT-1); //ymin
			bbox_kept_proba_per_class[i][j][2] = fmax(fmin(IMG_WIDTH-1, (box_center_x+0.5*box_width)), 0.0); //xmax	//should be the same as above (min(max())) ?! (taken from squeezedet sourcecode)
			bbox_kept_proba_per_class[i][j][3] = fmax(fmin(IMG_HEIGHT-1, (box_center_y+0.5*box_height)), 0.0); //ymax

		}
	}
	return 0;
}


int batch_nms(int cls ) {
	int k,l,o;
	//Set keep vector to 1
	for(o=0;o<num_probs_per_class[cls];o++)
		keep_bbox[cls][o]=1;

	for(k=0;k<num_probs_per_class[cls]-1;k++){
		for(l=1;l<num_probs_per_class[cls]-k;l++){
			float intersect, union_, hor, vert;
			float * bbox0, * bbox_iou;
			bbox0 = bbox_kept_proba_per_class[cls][k];
			bbox_iou = bbox_kept_proba_per_class[cls][k+l];
			hor = fmax(fmin(bbox0[2],bbox_iou[2])-
				fmax(bbox0[0],bbox_iou[0]),0);
			vert = fmax(fmin(bbox0[3],bbox_iou[3])-
				fmax(bbox0[1],bbox_iou[1]),0);
			intersect = hor*vert;
			union_ = (bbox0[2]-bbox0[0]) * (bbox0[3]-bbox0[1]) + (bbox_iou[2]-bbox_iou[0]) * (bbox_iou[3]-bbox_iou[1]) - intersect;
			if(intersect/union_ > NMS_THRESH)
				keep_bbox[cls][k+l] = 0;
		}
	}
	return 0;
}
