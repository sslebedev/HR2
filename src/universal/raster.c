//  **********************************************************
//  FILE NAME   rand.c
//  PURPOSE     
//  NOTES
//  **********************************************************
 
//  **********************************************************
//  Includes
//  **********************************************************


#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <memory.h>

#include "raster.h"


//  **********************************************************
//  Defines
//  **********************************************************


#define IABS(a)                   (((a)>0)?(a):-(a))
#define FABS(a)                   (((a)>0.0f)?(a):-(a))
#define ISWAP(aa,bb)              { int ttt; ttt = (aa); (aa)=(bb); (bb)=ttt; }


// Line draw
#define LINE_ACC    10
#define LINE_HALF   (1 << (LINE_ACC-1) )

// Accuracy for fixed integer operations instead of float point
#define ACC_DEG   12

//  **********************************************************
//  Vars
//  **********************************************************

