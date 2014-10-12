//  **********************************************************
//  FILE NAME   utl.c
//  PURPOSE     Simple image processing
//  NOTES
//  **********************************************************

//  **********************************************************
//  Includes
//  **********************************************************

// Avoid warnings with msvc compilation
#define _CRT_SECURE_NO_WARNINGS

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

#include "utl.h"


//  **********************************************************
//  Defines
//  **********************************************************

#define IABS(aaa)       (((aaa)>=0)?(aaa):(-(aaa)))
#define FABS(aaa)       (((aaa)>=0.0f)?(aaa):(-(aaa)))

//  **********************************************************
//  Funcs
//  **********************************************************

