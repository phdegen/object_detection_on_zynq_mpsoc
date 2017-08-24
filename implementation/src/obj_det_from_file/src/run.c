
#include "model_types.h"
#include "const.def"
#include "load_model.h"
#include "image_utils.h"
#include "run_network.h"
#include "eval_results.h"
#include "finalise_output.h"
#include <stdio.h>
#include <stdlib.h>
#include "sds_lib.h"
#include <time.h>
#include <omp.h>


net_model * squeezeDet;
activation_t *image;//[IMG_HEIGHT][IMG_WIDTH][3];
activation_t *intermediate_feature_map_buf_1;//[MAX_FEATURE_MAP_SIZE];
activation_t *intermediate_feature_map_buf_2;//[MAX_FEATURE_MAP_SIZE];
int num_pred_boxes;
int boxes[TOP_N_DETECTION][4+1+1]; // bbox, class, probability


int main(){
	image = (activation_t *)sds_alloc(IMG_HEIGHT*IMG_WIDTH*3* sizeof(activation_t));
	intermediate_feature_map_buf_1 = (activation_t *)sds_alloc(MAX_FEATURE_MAP_SIZE* sizeof(activation_t));
	intermediate_feature_map_buf_2 = (activation_t *)sds_alloc(MAX_FEATURE_MAP_SIZE* sizeof(activation_t));
	//mallocs for compilation on host
	//image = (activation_t *)malloc(IMG_HEIGHT*IMG_WIDTH*3* sizeof(activation_t));
	//intermediate_feature_map_buf_1 = (activation_t *)malloc(MAX_FEATURE_MAP_SIZE* sizeof(activation_t));
	//intermediate_feature_map_buf_2 = (activation_t *)malloc(MAX_FEATURE_MAP_SIZE* sizeof(activation_t));

	squeezeDet = (net_model *) sds_alloc(sizeof(net_model));
	//squeezeDet = (net_model *) malloc(sizeof(net_model));
	load_model(squeezeDet);

	printf("--Model loading done-- \n");

	//per image processing
	{
		double begin1 = omp_get_wtime();
		load_image((activation_t*)image);
		
		preprocessing((activation_t*)image);
		double end1 = omp_get_wtime();

		double time_spent1 = (end1 - begin1);
		printf("loading lasts is %2.6f s. \n", time_spent1);
		printf("--Image loading and preprocessing done-- \n");
		double begin = omp_get_wtime();
		run_network(squeezeDet, (activation_t*)image, intermediate_feature_map_buf_1,
					intermediate_feature_map_buf_2);
		double end = omp_get_wtime();
		double time_spent = (end - begin);
		printf("--CNN Comp done-- \n");
		printf("Processing time for run_network is %2.6f s. \n", time_spent);


		double begin2 = omp_get_wtime();
		num_pred_boxes = eval_results(intermediate_feature_map_buf_1, (int *)boxes);

		double end2 = omp_get_wtime();
		double time_spent2 = (end2 - begin2);
		printf("finalising lasts is %2.6f s. \n", time_spent2);

		printf("Numbers of detected boxes: %d \n\n",num_pred_boxes);
		double begin3 = omp_get_wtime();
		postprocessing((activation_t*)image);

		int juu;
		for(juu=0;juu<num_pred_boxes;juu++){
			printf("box number %d is x: %d,%d; y: %d,%d \n",juu,boxes[juu][0],boxes[juu][2],boxes[juu][1],boxes[juu][3]);
		}

		draw_boxes((activation_t*)image, (int*) boxes, num_pred_boxes);

		save_image((activation_t*) image);
		double end3 = omp_get_wtime();
		double time_spent3 = (end3 - begin3);
		printf("postprocessing(inkl drawing) and saving lasts is %2.6f s. \n", time_spent3);

	}
	sds_free(squeezeDet);
	sds_free(intermediate_feature_map_buf_1);
	sds_free(intermediate_feature_map_buf_2);
	sds_free(image);
	//free(squeezeDet);
	//free(intermediate_feature_map_buf_1);
	//free(intermediate_feature_map_buf_2);
	//free(image);

}


