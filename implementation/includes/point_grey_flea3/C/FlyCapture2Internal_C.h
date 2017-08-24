//=============================================================================
// Copyright © 2008 Point Grey Research, Inc. All Rights Reserved.
//
// This software is the confidential and proprietary information of Point
// Grey Research, Inc. ("Confidential Information").  You shall not
// disclose such Confidential Information and shall use it only in
// accordance with the terms of the license agreement you entered into
// with PGR.
//
// PGR MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY OF THE
// SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE, OR NON-INFRINGEMENT. PGR SHALL NOT BE LIABLE FOR ANY DAMAGES
// SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR DISTRIBUTING
// THIS SOFTWARE OR ITS DERIVATIVES.
//=============================================================================
//=============================================================================
// $Id: FlyCapture2Internal_C.h,v 1.6 2010-04-08 22:39:45 hirokim Exp $
//=============================================================================

#ifndef FLYCAPTURE2INTERNAL_C_H
#define FLYCAPTURE2INTERNAL_C_H

#include "C/FlyCapture2Defs_C.h"

#include "FlyCapture2.h"
#include "FlyCapture2GUI.h"

typedef struct _fc2InternalContext
{
	FlyCapture2::BusManager* pBusMgr;
	FlyCapture2::CameraBase* pCamera;

} fc2InternalContext;

typedef struct _fc2InternalGuiContext
{
	FlyCapture2::CameraSelectionDlg* pCameraSelectionDlg;
	FlyCapture2::CameraControlDlg* pCameraControlDlg;

} fc2InternalGuiContext;

typedef struct _fc2InternalImageCallback
{
	fc2ImageEventCallback   pCallback;
	void*                   pCallbackData;
}fc2InternalImageCallback;

inline bool IsContextValid( fc2Context context )
{
	if ( context == NULL )
	{
		return false;
	}

	fc2InternalContext* pIntContext = (fc2InternalContext*)context;

	if ( pIntContext->pBusMgr == NULL || pIntContext->pCamera == NULL )
	{
		return false;
	}

	return true;
}

inline bool IsGuiContextValid( fc2GuiContext context )
{
	if ( context == NULL )
	{
		return false;
	}

	fc2InternalGuiContext* pIntContext = (fc2InternalGuiContext*)context;

	if ( pIntContext->pCameraSelectionDlg == NULL || pIntContext->pCameraControlDlg == NULL )
	{
		return false;
	}

	return true;
}
inline void SyncCppImageToStruct( fc2Image* pImage )
{
	FlyCapture2::Image* pCppImage = (FlyCapture2::Image*)pImage->imageImpl;

	unsigned int uiRows;
	unsigned int uiCols;
	unsigned int uiStride;
	FlyCapture2::PixelFormat pixFormat;
	FlyCapture2::BayerTileFormat bayerFormat;

	pCppImage->GetDimensions(
			&uiRows,
			&uiCols,
			&uiStride,
			&pixFormat,
			&bayerFormat );

	pImage->rows = uiRows;
	pImage->cols = uiCols;
	pImage->stride = uiStride;
	pImage->pData = pCppImage->GetData();
	pImage->dataSize = uiRows * uiStride;
	pImage->receivedDataSize = pCppImage->GetReceivedDataSize();
	pImage->format = (fc2PixelFormat)pixFormat;
	pImage->bayerFormat = (fc2BayerTileFormat)bayerFormat;
}

#endif // FLYCAPTURE2INTERNAL_C_H
//=============================================================================
// $Log: not supported by cvs2svn $
// Revision 1.5  2009/04/06 21:38:41  soowei
// [1] Modified AVIContext to directly point to an instance of AVIRecorder
//
// Revision 1.4  2009/04/06 21:28:16  soowei
// [1] Removed void* typedefs since we can directly use the FlyCapture2 C++ objects
//
// Revision 1.3  2009/04/03 18:43:14  matt
// [1] Added AVI recording support.
//
// Revision 1.2  2009/03/24 17:34:21  mgara
// [1] added fc2InternalImageCallback structure to pass the callback and the additional arg into the c++ pCallbackData
//
// Revision 1.1  2009/01/19 20:19:12  soowei
// [1] Split GUI off into seperate project
//
// Revision 1.2  2009/01/16 18:17:36  hirokim
// [1] added GUI functions
//
// Revision 1.1  2008/12/12 01:24:38  soowei
// [1] Added internal header file
//
//=============================================================================
