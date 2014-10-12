//  **********************************************************
//  FILE NAME   raster.h
//  PURPOSE     Simple rasterization
//  NOTES
//  **********************************************************

#ifndef _RASTER_H_
#define _RASTER_H_

#include "utl.h"

//////////////////////////////////////////////////////////////////////////

typedef struct tagImageDesc
{
    int  *m_pixels;
    int   m_width;
    int   m_height;
} ImageDesc;

#endif