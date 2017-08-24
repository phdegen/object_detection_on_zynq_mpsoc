/*
 * image_utils.h
 *
 *  Created on: Jan 26, 2017
 *      Author: msc17f2
 */

#ifndef SRC_IMAGE_UTILS_H_
#define SRC_IMAGE_UTILS_H_

#include "model_types.h"

int load_image(activation_t * image);
int save_image(activation_t * image);
int preprocessing(activation_t * image);
int postprocessing(activation_t * image);

#endif /* SRC_IMAGE_UTILS_H_ */
