//  **********************************************************
//  FILE NAME   pline.h
//  PURPOSE     Polyline (contour) recognition - main interface
//  NOTES
//  **********************************************************

#ifndef _PLINE_H_
#define _PLINE_H_

#include "utl.h"
#include "raster.h"

//////////////////////////////////////////////////////////////////////////

// Static settings
#ifndef MAX_LETTERS
    #define   MAX_LETTERS               512
#endif

#ifndef MAX_POINTS_IN_POLYLINE
    #define   MAX_POINTS_IN_POLYLINE    648
#endif

//////////////////////////////////////////////////////////////////////////

typedef struct tagPLine
{
    int               m_numPoints;
    V2d               m_points[MAX_POINTS_IN_POLYLINE];
    V2f               m_pointsNorm[MAX_POINTS_IN_POLYLINE];
    float             m_tar[MAX_POINTS_IN_POLYLINE];
} PLine;

typedef struct tagLetterDesc
{
    char    m_letter;
    PLine   m_polyline;
} LetterDesc;

typedef struct tagLetterSet
{
    int           m_numLetters;
    LetterDesc    m_letters[MAX_LETTERS];
} LetterSet;

//////////////////////////////////////////////////////////////////////////

#ifdef    __cplusplus
extern "C"
{
#endif    /* __cplusplus */

int   PLineCreate(const LetterSet *set);
void  PLineDestroy();
int   PLineProcess(PLine *polyline, char *letterRecognized, int *letterIndex, ImageDesc *screenDst);

#ifdef    __cplusplus
}
#endif  /* __cplusplus */

#endif  


   