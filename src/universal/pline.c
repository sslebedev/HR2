//  **********************************************************
//  FILE NAME   pline.c
//  PURPOSE     
//  NOTES
//  **********************************************************
 
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>

#include "pline.h"

#define TOO_SMALL_VALUE           1.0e-6f

//////////////////////////////////////////////////////////////////////////

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
                      )
{
}

int PLineCreate(const LetterSet *set)
{
  

  return 0;
}

void  PLineDestroy()
{
}

int   PLineProcess(PLine *polylineTest, char *letterRecognized, int *letterIndex, ImageDesc *screenDst)
{
  return 0;
}
