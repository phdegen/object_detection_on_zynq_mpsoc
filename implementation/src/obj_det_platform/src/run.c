
#include "camera_utils.h"
#include "network_utils.h"
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
#include <omp.h>


net_model * squeezeDet;
activation_t *image;//[IMG_HEIGHT][IMG_WIDTH][3];
activation_t *intermediate_feature_map_buf_1;//[MAX_FEATURE_MAP_SIZE];
activation_t *intermediate_feature_map_buf_2;//[MAX_FEATURE_MAP_SIZE];
int num_pred_boxes;
int boxes[TOP_N_DETECTION][4+1+1]; // bbox, class, probability
int proc_n_images=1000;
double begin,end;


int main(){

	image = (activation_t *)sds_alloc(IMG_HEIGHT*IMG_WIDTH*3* sizeof(activation_t));
	intermediate_feature_map_buf_1 = (activation_t *)sds_alloc(MAX_FEATURE_MAP_SIZE* sizeof(activation_t));
	intermediate_feature_map_buf_2 = (activation_t *)sds_alloc(MAX_FEATURE_MAP_SIZE* sizeof(activation_t));
	//squeezeDet = (net_model *) xalloc(1,sizeof(net_model));
	squeezeDet = (net_model *) sds_alloc(sizeof(net_model));
	load_model(squeezeDet);

	launch_cam(image);

	setup_conn();

	printf("--Model loading done-- \n");


	//per image processing
		while(proc_n_images>0)
		{
//#pragma omp parallel num_threads(4)
			{
//#pragma omp single
			{
			begin = omp_get_wtime();
			//load_image((activation_t*)image);
			grab_image();
			end = omp_get_wtime();
			printf("imageloading lasts %2.6f s. \n", (end - begin));

			begin = omp_get_wtime();
			preprocessing((activation_t*)image);
			end = omp_get_wtime();
			printf("preprocessing lasts %2.6f s. \n", (end - begin));
			printf("--Image loading and preprocessing done-- \n");
			//clock_t begin = clock();
			begin = omp_get_wtime();
			}
			run_network(squeezeDet, (activation_t*)image, intermediate_feature_map_buf_1,
					intermediate_feature_map_buf_2);
//#pragma omp single
			{
			end = omp_get_wtime();
			printf("--CNN Comp done-- \n");
			printf("Processing time for run_network is %2.6f s. \n", (end - begin));


			begin = omp_get_wtime();
			num_pred_boxes = eval_results(intermediate_feature_map_buf_1, (int *)boxes);

			end = omp_get_wtime();
			printf("finalising lasts is %2.6f s. \n", (end - begin));

			printf("Numbers of detected boxes: %d \n\n",num_pred_boxes);
			begin = omp_get_wtime();
			postprocessing((activation_t*)image);
			end = omp_get_wtime();
			printf("postprocessing lasts is %2.6f s. \n", (end - begin));
			int juu;
			for(juu=0;juu<num_pred_boxes;juu++){
				printf("box number %d is x: %d,%d; y: %d,%d \n",juu,boxes[juu][0],boxes[juu][2],boxes[juu][1],boxes[juu][3]);
			}
			begin = omp_get_wtime();
			draw_boxes((activation_t*)image, (int*) boxes, num_pred_boxes);
			end = omp_get_wtime();
			printf(" drawing lasts is %2.6f s. \n", (end - begin));

			begin = omp_get_wtime();
			//save_image((activation_t*) image);
			send_image((char *) image);

			end = omp_get_wtime();
			printf("saving lasts is %2.6f s. \n", (end - begin));
			printf("image number: %d \n", 1000-proc_n_images);

			proc_n_images--;
			}
		}

	}
	close_conn();
	end_cam();

	sds_free(intermediate_feature_map_buf_1);
	sds_free(intermediate_feature_map_buf_2);
	sds_free(image);

}


