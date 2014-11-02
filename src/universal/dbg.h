//  **********************************************************
//  FILE NAME   dbg.h
//  PURPOSE     Polyline (contour) recognition - debug interface
//  NOTES
//  **********************************************************
#ifndef _DEBUG_H_
#define _DEBUG_H_

void RenderTarToScreen(
    const float  *tar, 
    const int     numPoints, 
    const int     xMin,
    const int     yMin,
    const int     xMax,
    const int     yMax,
    int          *imagePixels,
    const int     imageW, 
    const int     imageH
    );

#endif