/******************************************************************************
 *
 * (c) Copyright 2012-2016 Xilinx, Inc. All rights reserved.
 *
 * This file contains confidential and proprietary information of Xilinx, Inc.
 * and is protected under U.S. and international copyright and other
 * intellectual property laws.
 *
 * DISCLAIMER
 * This disclaimer is not a license and does not grant any rights to the
 * materials distributed herewith. Except as otherwise provided in a valid
 * license issued to you by Xilinx, and to the maximum extent permitted by
 * applicable law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND WITH ALL
 * FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES AND CONDITIONS, EXPRESS,
 * IMPLIED, OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF
 * MERCHANTABILITY, NON-INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE;
 * and (2) Xilinx shall not be liable (whether in contract or tort, including
 * negligence, or under any other theory of liability) for any loss or damage
 * of any kind or nature related to, arising under or in connection with these
 * materials, including for any direct, or any indirect, special, incidental,
 * or consequential loss or damage (including loss of data, profits, goodwill,
 * or any type of loss or damage suffered as a result of any action brought by
 * a third party) even if such damage or loss was reasonably foreseeable or
 * Xilinx had been advised of the possibility of the same.
 *
 * CRITICAL APPLICATIONS
 * Xilinx products are not designed or intended to be fail-safe, or for use in
 * any application requiring fail-safe performance, such as life-support or
 * safety devices or systems, Class III medical devices, nuclear facilities,
 * applications related to the deployment of airbags, or any other applications
 * that could lead to death, personal injury, or severe property or
 * environmental damage (individually and collectively, "Critical
 * Applications"). Customer assumes the sole risk and liability of any use of
 * Xilinx products in Critical Applications, subject only to applicable laws
 * and regulations governing limitations on product liability.
 *
 * THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS PART OF THIS FILE
 * AT ALL TIMES.
 *
 *******************************************************************************/
/**
 * @brief Custom implementation of LK optical flow. Ref doc for more info
 *
 * @author Pari Kannan (parik@xilinx.com)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ap_int.h>
#include <hls_stream.h>
#include <iostream>
#include <iomanip>
#include "assert.h"
#include "optflow.h"

  pix_t *f0Stream = NULL;
  pix_t *f1Stream = NULL;
  yuv_t *ffStream = NULL;

  int *ixix = NULL;
  int *ixiy = NULL;
  int *iyiy = NULL;
  int *dix = NULL;
  int *diy = NULL;

  float *fx = NULL;
  float *fy = NULL;

//#define FUNC_DBG

//----------------------------------------------------------------------------
// Input video dimension: height * stride, only need height * width
//----------------------------------------------------------------------------
void readMatRows (yuv_t *matB, pix_t* pixStream, int height, int width, int stride)
{
  int pix_index = 0;
  for (int i=0; i < height; ++i) {                                              
#pragma HLS loop_tripcount min=1080 max=1080 avg=1080
    for (int j=0; j < stride; ++j) {                                            
#pragma HLS loop_tripcount min=1920 max=1920 avg=1920
      #pragma HLS PIPELINE
      yuv_t tmpData = matB [i*stride + j];
      if ( j < width )
    	  pixStream[pix_index++] = tmpData & 0x00FF;
    }
  }
}

//----------------------------------------------------------------------------
// Output video dimension: height * stride, padding height * width
//----------------------------------------------------------------------------
void writeMatRows (yuv_t* pixStream, yuv_t *dst, int height, int width, int stride)
{
  int pix_index = 0;
  for (int i=0; i < height; ++i) {                                              
#pragma HLS loop_tripcount min=1080 max=1080 avg=1080
    for (int j=0; j < stride; ++j) {                                            
#pragma HLS loop_tripcount min=1920 max=1920 avg=1920
      #pragma HLS PIPELINE
      yuv_t tmpData = j < width ? pixStream[pix_index++] : 0;
      dst [i*stride +j] = tmpData;
    }
  }
}

//----------------------------------------------------------------------------
// compute left sums for col=1 and subtract
// left shift, add new col to right
// compute right sums and add
//----------------------------------------------------------------------------
 void computeSum(pix_t* f0Stream, 
                 pix_t* f1Stream, 
                 int*   ixix_out, 
                 int*   ixiy_out, 
                 int*   iyiy_out, 
                 int*   dix_out, 
                 int*   diy_out, int height, int width)
{
   hls::stream <pix_t> img1Col [KMEDP1], img2Col [KMEDP1];
   #pragma HLS STREAM variable=img1Col  depth=16
   #pragma HLS STREAM variable=img2Col  depth=16
   #pragma HLS ARRAY_PARTITION variable=img1Col complete dim=0
   #pragma HLS ARRAY_PARTITION variable=img2Col complete dim=0

   static pix_t lb1 [KMEDP1][LB_WIDTH], lb2 [KMEDP1][LB_WIDTH];
   #pragma HLS ARRAY_PARTITION variable=lb1 complete dim=1
   #pragma HLS ARRAY_PARTITION variable=lb2 complete dim=1

   static pix_t img1Win [2 * KMEDP1], img2Win [1 * KMEDP1];
   static int ixix=0, ixiy=0, iyiy=0, dix=0, diy=0;
   #pragma HLS ARRAY_PARTITION variable=img1Win complete dim=0
   #pragma HLS ARRAY_PARTITION variable=img2Win complete dim=0

   pix_t img1Col_ [KMEDP1], img2Col_ [KMEDP1];
   #pragma HLS ARRAY_PARTITION variable=img1Col_ complete dim=0
   #pragma HLS ARRAY_PARTITION variable=img2Col_ complete dim=0

   // column sums for entire image width.
   // For II=1 pipelining, need two read and 1 write ports. Simulate with two
   // 2-P RAMs with their write port tied together.
   static int csIxix [LB_WIDTH], csIxiy [LB_WIDTH], csIyiy [LB_WIDTH], csDix [LB_WIDTH], csDiy [LB_WIDTH];
   static int cbIxix [LB_WIDTH], cbIxiy [LB_WIDTH], cbIyiy [LB_WIDTH], cbDix [LB_WIDTH], cbDiy [LB_WIDTH];
   #pragma HLS RESOURCE variable=csIxix core=RAM_2P_BRAM
   #pragma HLS RESOURCE variable=csIxiy core=RAM_2P_BRAM
   #pragma HLS RESOURCE variable=csIyiy core=RAM_2P_BRAM
   #pragma HLS RESOURCE variable=csDix core=RAM_2P_BRAM
   #pragma HLS RESOURCE variable=csDiy core=RAM_2P_BRAM
   #pragma HLS DEPENDENCE variable=csIxix inter WAR false
   #pragma HLS DEPENDENCE variable=csIxiy inter WAR false
   #pragma HLS DEPENDENCE variable=csIyiy inter WAR false
   #pragma HLS DEPENDENCE variable=csDix  inter WAR false
   #pragma HLS DEPENDENCE variable=csDiy  inter WAR false
   #pragma HLS RESOURCE variable=cbIxix core=RAM_2P_BRAM
   #pragma HLS RESOURCE variable=cbIxiy core=RAM_2P_BRAM
   #pragma HLS RESOURCE variable=cbIyiy core=RAM_2P_BRAM
   #pragma HLS RESOURCE variable=cbDix core=RAM_2P_BRAM
   #pragma HLS RESOURCE variable=cbDiy core=RAM_2P_BRAM
   #pragma HLS DEPENDENCE variable=cbIxix inter WAR false
   #pragma HLS DEPENDENCE variable=cbIxiy inter WAR false
   #pragma HLS DEPENDENCE variable=cbIyiy inter WAR false
   #pragma HLS DEPENDENCE variable=cbDix  inter WAR false
   #pragma HLS DEPENDENCE variable=cbDiy  inter WAR false

   int csIxixR, csIxiyR, csIyiyR, csDixR, csDiyR;

   // the left and right indices onto the column sums
   int zIdx= - (KMED-2);
   int nIdx = zIdx + KMED-2;

   int index = 0;
   for (int r = 0; r < height; r++) {
     for (int c = 0; c < width; c++) {
       #pragma HLS PIPELINE
       #pragma HLS DEPENDENCE variable=lb1 false
       #pragma HLS DEPENDENCE variable=lb2 false

        // shift up both linebuffers at col=c
        for (int i = 0; i < KMEDP1 - 1; i++) {
         lb1 [i][c] = lb1 [i + 1][c];
         img1Col [i]. write (lb1 [i][c]);

         lb2 [i][c] = lb2 [i+1][c];
         img2Col [i]. write (lb2 [i][c]);
        }

        // read in the new pixels at col=c and row=bottom_of_lb
       pix_t pix0 = f0Stream[r*width+c];
        lb1 [KMEDP1 - 1][c] = pix0;
        img1Col [KMEDP1 - 1]. write (pix0);

       pix_t pix1 = f1Stream[r*width+c];
        lb2 [KMEDP1 -1][c] = pix1;
        img2Col [KMEDP1 - 1]. write (pix1);
        // line-buffer done

        // the leftSums
        int csIxixL=0, csIxiyL=0, csIyiyL=0, csDixL=0, csDiyL=0;
        if (zIdx >= 0) {
          csIxixL = csIxix [zIdx];
          csIxiyL = csIxiy [zIdx];
          csIyiyL = csIyiy [zIdx];
          csDixL  = csDix [zIdx];
          csDiyL  = csDiy [zIdx];
        }
        for (int wr=0; wr<KMEDP1; ++wr) {
          img1Col_[wr] = img1Col [wr]. read ();
          img2Col_[wr] = img2Col [wr]. read ();
        }
        // p(x+1,y) and p(x-1,y)
        int wrt=1;
        int cIxTopR = (img1Col_ [wrt] - img1Win [wrt*2 + 2-2]) /2 ;
        // p(x,y+1) and p(x,y-1)
        int cIyTopR = (img1Win [ (wrt+1)*2 + 2-1] - img1Win [ (wrt-1)*2 + 2-1])  /2;
        // p1(x,y) and p2(x,y)
        int delTopR = img1Win [wrt*2 + 2-1] - img2Win [wrt*1 + 1-1];

        int wrb = KMED-1;
        int cIxBotR = (img1Col_ [wrb] - img1Win [wrb*2 + 2-2]) /2 ;
        int cIyBotR = (img1Win [ (wrb+1)*2 + 2-1] - img1Win [ (wrb-1)*2 + 2-1]) /2;
        int delBotR = img1Win [wrb*2 + 2-1] - img2Win [wrb*1 + 1-1];
        if (0 && r < KMED) {
          cIxTopR = 0; cIyTopR = 0; delTopR = 0;
        }

        // compute rightSums incrementally
        csIxixR = cbIxix [nIdx] + cIxBotR * cIxBotR - cIxTopR * cIxTopR;
        csIxiyR = cbIxiy [nIdx] + cIxBotR * cIyBotR - cIxTopR * cIyTopR;
        csIyiyR = cbIyiy [nIdx] + cIyBotR * cIyBotR - cIyTopR * cIyTopR;
        csDixR  = cbDix [nIdx]  + delBotR * cIxBotR - delTopR * cIxTopR;
        csDiyR  = cbDiy [nIdx]  + delBotR * cIyBotR - delTopR * cIyTopR;

        // sums += (rightSums - leftLums)
        ixix += (csIxixR - csIxixL);
        ixiy += (csIxiyR - csIxiyL);
        iyiy += (csIyiyR - csIyiyL);
        dix += (csDixR - csDixL);
        diy += (csDiyR - csDiyL);

        ixix_out[index] = ixix;
        ixiy_out[index] = ixiy;
        iyiy_out[index] = iyiy;
        dix_out[index]  = dix;
        diy_out[index]  = diy;
        index++;

        // shift windows
        for (int i = 0; i < KMEDP1; i++) {
          img1Win [i * 2] = img1Win [i * 2 + 1];
        }
        for (int i=0; i < KMEDP1; ++i) {
          img1Win  [i*2 + 1] = img1Col_ [i];
          img2Win  [i] = img2Col_ [i];
        }

        // write new rightSums to both RAMs of 3-P RAM
        cbIxix [nIdx] = csIxixR;
        cbIxiy [nIdx] = csIxiyR;
        cbIyiy [nIdx] = csIyiyR;
        cbDix  [nIdx] = csDixR;
        cbDiy  [nIdx] = csDiyR;

        csIxix [nIdx] = csIxixR;
        csIxiy [nIdx] = csIxiyR;
        csIyiy [nIdx] = csIyiyR;
        csDix  [nIdx] = csDixR;
        csDiy  [nIdx] = csDiyR;

        zIdx++;
        if (zIdx == width) zIdx = 0;
        nIdx++;
        if (nIdx == width) nIdx = 0;

      } // for c
   }  // for r

   // cleanup  linebuffer and other statics.
   // TODO combine with lb loading. Set the pixel to 0 if row < KMEDP1
   for (int r = 0; r < KMEDP1; r++) {
     for (int c = 0; c < width; c++) {
       #pragma HLS PIPELINE
       lb1 [r][c] = 0;
       lb2 [r][c] = 0;
     }
   }

   for (int r = 0; r < KMEDP1; r++) {
     #pragma HLS PIPELINE
      img1Win [r] = 0; img1Win [r+KMEDP1] = 0; img2Win [r] = 0;
      img1Col_ [r] =0; img2Col_ [r] =0;
   }
   for (int r=0; r < width; ++r) {
      #pragma HLS PIPELINE
      csIxix [r] = 0; csIxiy [r] = 0; csIyiy [r] = 0; csDix [r] = 0; csDiy [r] = 0;
      cbIxix [r] = 0; cbIxiy [r] = 0; cbIyiy [r] = 0; cbDix [r] = 0; cbDiy [r] = 0;
   }
   ixix=0; ixiy=0; iyiy=0; dix=0; diy=0;
}


void computeFlow (int* ixix,
                  int* ixiy, 
                  int* iyiy, 
                  int* dix, 
                  int* diy, 
                  float* fx_out, 
                  float* fy_out, int height, int width)
{
  int index = 0;
  for (int r = 0; r < height; r++) {
    for (int c = 0; c < width; c++) {
      #pragma HLS PIPELINE
      int ixix_ = ixix[index];
      int ixiy_ = ixiy[index];
      int iyiy_ = iyiy[index];
      int dix_  = dix[index];
      int diy_  = diy[index];
      float fx_=0, fy_=0;

      // matrix inv
      float det = (float)ixix_ * iyiy_ - (float)ixiy_ * ixiy_;
      if (det <= 1.0f) {
        fx_ = 0.0;
        fy_ = 0.0;
      } else {
        float i00 = (float)iyiy_ / det;
        float i01 = (float) (-ixiy_) / det;
        float i10 = (float) (-ixiy_) / det;
        float i11 = (float)ixix_ / det;

        fx_ = i00 * dix_ + i01 * diy_;
        fy_ = i10 * dix_ + i11 * diy_;
      }
      fx_out[index] = fx_;
      fy_out[index] = fy_;
      index++;

#ifdef FUNC_DBG
      std::cout << "computeFlow r=" << r << " c=" << c << " fx_=" << fx_ << " fy_=" << fy_ << std::endl;
#endif
    }
  }
}

pix_t getLuma (float fx, float fy, float clip_flowmag)
{
#pragma HLS inline
  float rad = sqrtf (fx*fx + fy*fy);

  if (rad > clip_flowmag) rad = clip_flowmag; // clamp to MAX
  rad /= clip_flowmag;			      // convert 0..MAX to 0.0..1.0
  pix_t pix = (pix_t) (255.0f * rad);

#ifdef FUNC_DBG
  std::cout << "getLuma fx=" << fx << " fy=" << fy << " rad=" << rad << " pix=" << (int)pix << std::endl;
#endif
  return pix;
}

pix_t getChroma (float f, float clip_flowmag)
{
#pragma HLS inline
  if (f >   clip_flowmag ) f =  clip_flowmag; 	// clamp big positive f to  MAX
  if (f < (-clip_flowmag)) f = -clip_flowmag; 	// clamp big negative f to -MAX
  f /= clip_flowmag;				// convert -MAX..MAX to -1.0..1.0
  pix_t pix = (pix_t) (127.0f * f + 128.0f);	// convert -1.0..1.0 to -127..127 to 1..255

  return pix;
}

void getOutPix (float* fx, 
                float* fy, 
                yuv_t* out_pix,
				int height, int width, float clip_flowmag)
{
  int pix_index = 0;
  for (int r = 0; r < height; r++) {
    for (int c = 0; c < width; c++) {
      #pragma HLS PIPELINE
      float fx_ = fx[pix_index];
      float fy_ = fy[pix_index];

      pix_t outLuma = getLuma (fx_, fy_, clip_flowmag);
      pix_t outChroma = (c&1)? getChroma (fy_, clip_flowmag) : getChroma (fx_, clip_flowmag);
      yuv_t yuvpix;

      yuvpix = ((yuv_t)outChroma << 8) | outLuma;

      out_pix[pix_index++] = yuvpix;

#ifdef FUNC_DBG
      std::cout << "getOutPix r=" << r << " c="<<c<< " fx_=" << fx_ << " fy_=" << fy_ << " outPix=" << (int)yuvpix << std::endl;
#endif
    }
  }
}

int fpga_optflow (yuv_t *frame0, yuv_t *frame1, yuv_t *framef,
		int height_in, int width_in, int stride_in,
		int height_out, int width_out, int stride_out, float clip_flowmag)
{
	if (width_in != width_out) return -1;
	if (height_in != height_out) return -1;

#ifdef COMPILEFORSW
	  int img_pix_count = height_in * width_in;
#else
	  int img_pix_count = 10;
#endif

  if (f0Stream == NULL) f0Stream = (pix_t *) malloc(sizeof(pix_t) * img_pix_count);
  if (f1Stream == NULL) f1Stream = (pix_t *) malloc(sizeof(pix_t) * img_pix_count);
  if (ffStream == NULL) ffStream = (yuv_t *) malloc(sizeof(yuv_t) * img_pix_count);

  if (ixix == NULL) ixix = (int *) malloc(sizeof(int) * img_pix_count);
  if (ixiy == NULL) ixiy = (int *) malloc(sizeof(int) * img_pix_count);
  if (iyiy == NULL) iyiy = (int *) malloc(sizeof(int) * img_pix_count);
  if (dix == NULL) dix = (int *) malloc(sizeof(int) * img_pix_count);
  if (diy == NULL) diy = (int *) malloc(sizeof(int) * img_pix_count);

  if (fx == NULL) fx = (float *) malloc(sizeof(float) * img_pix_count);
  if (fy == NULL) fy = (float *) malloc(sizeof(float) * img_pix_count);

  readMatRows (frame0, f0Stream, height_in, width_in, stride_in);
  readMatRows (frame1, f1Stream, height_in, width_in, stride_in);

  computeSum (f0Stream, f1Stream, ixix, ixiy, iyiy, dix, diy, height_in, width_in);
  computeFlow (ixix, ixiy, iyiy, dix, diy, fx, fy, height_in, width_in);
  getOutPix (fx, fy, ffStream, height_in, width_in, clip_flowmag);

  writeMatRows (ffStream, framef, height_in, width_in, stride_out);

  return 0;
}

