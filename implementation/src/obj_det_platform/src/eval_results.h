/*
 * eval_results.h
 *
 *  Created on: Jan 26, 2017
 *      Author: msc17f2
 */

#ifndef SRC_EVAL_RESULTS_MMMH_
#define SRC_EVAL_RESULTS_MMMH_

#include "model_types.h"

int eval_results(activation_t * inp_buf, int * outp_boxes);

int get_highest_classes_per_anchor(finish_t * buf_1);

int get_highest_prob_anchors();

int extract_boxes(finish_t * featuremap);

int perform_nms(finish_t * convDet_fm, int * output_bbxs);

int batch_nms(int cls );

#endif /* SRC_EVAL_RESULTS_MMMH_ */
