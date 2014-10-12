//  **********************************************************
//  FILE NAME   pline.h
//  PURPOSE     Polyline (contour) recognition
//  NOTES
//  **********************************************************

#ifndef _PLINE_H_
#define _PLINE_H_

//  **********************************************************
//  Includes
//  **********************************************************

#include "utl.h"
#include "raster.h"

//  **********************************************************
//  Defines
//  **********************************************************

// Method for letter mathcing
// HOG gets 86-94% correct recognition ratio
//
//#define   METHOD_HOG
//#define   METHOD_DIR_CODE
//#define   METHOD_TAR
//#define   METHOD_FORCE
#define   METHOD_POLAPPROX

#define   TAR_NEIGH                 8



#define   MAX_POINTS_IN_POLYLINE    648
#define   MAX_LETTERS               512

// Simplified polyline points count
#define   NUM_POINTS_ROUGH_POLYLINE 64

#define   NUM_RADIUSES              8
#define   NUM_ANGLES                12

#define   MAX_DIR_CODES             16

#define   FEATURE_MAP_COLS          4
#define   FEATURE_MAP_ROWS          8

#define   NUM_HOG_CELLS             4
#define   NUM_HOG_ANGLES            16
#define   NUM_HOG_ELEMENTS          (NUM_HOG_CELLS * NUM_HOG_ANGLES)

#define   ANGLE_NEG_LARGE           0
#define   ANGLE_NEG_NORMAL          1
#define   ANGLE_SMALL               2
#define   ANGLE_POS_NORMAL          3
#define   ANGLE_POS_LARGE           4

//  **********************************************************
//  Types
//  **********************************************************

typedef enum tagDirCode
{
    DIRCODE_NA                      =-1,

    DIRCODE_RAD_DEC_ANG_NEG_LARGE   = 0,
    DIRCODE_RAD_DEC_ANG_NEG_NORMAL  = 1,
    DIRCODE_RAD_DEC_ANG_SMALL       = 2,
    DIRCODE_RAD_DEC_ANG_POS_NORMAL  = 3,
    DIRCODE_RAD_DEC_ANG_POS_LARGE   = 4,

    DIRCODE_RAD_INC_ANG_NEG_LARGE   = 5,
    DIRCODE_RAD_INC_ANG_NEG_NORMAL  = 6,
    DIRCODE_RAD_INC_ANG_SMALL       = 7,
    DIRCODE_RAD_INC_ANG_POS_NORMAL  = 8,
    DIRCODE_RAD_INC_ANG_POS_LARGE   = 9,

}   DirCode;

typedef struct tagFeaturePoints
{
  V2f   m_points[3];
} FeaturePoints;


typedef struct tagPointDescriptor
{
  int               m_values[NUM_RADIUSES * NUM_ANGLES];
  DirCode           m_dirCodes[MAX_DIR_CODES];
  int               m_featMap[FEATURE_MAP_ROWS * FEATURE_MAP_COLS];
  float             m_hog[NUM_HOG_ELEMENTS];
  FeaturePoints     m_featPoints;
} PointDescriptor;

typedef struct tagPLine
{
  int               m_numPoints;
  V2d               m_points[MAX_POINTS_IN_POLYLINE];
  V2f               m_pointsNorm[MAX_POINTS_IN_POLYLINE];
  PointDescriptor   m_desc;
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


//  **********************************************************
//  Functions prototypes
//  **********************************************************

#ifdef    __cplusplus
extern "C"
{
#endif    /* __cplusplus */

int   PLineCreate(const LetterSet *set);
void  PLineDestroy();
int   PLineProcess(PLine *polyline, char *letterRecognized, int *letterIndex, ImageDesc *screenDst);

int   PLineMakeRoughByNumSeg(PLine *polyline, const int numRequiredPointsInPolyline);
int   PLineMakeRoughByCurvature(PLine *polyline, const int minRequiredAngle);
int   PLineMakeRoughRemovingClosePoints(PLine *polyline, const float ratioTooCloseByBbox);

LetterDesc  *PLineGetLetterFromLearningSet(const int letterIndex);

// deep debug
void RenderDescriptorTableToScreen( 
                                      const PointDescriptor *desc, 
                                      const int xLeft, const int yTop, const int wTable, const int hTable,
                                      int *imagePixels, const int imageW, const int imageH
                                    );
void RenderTarToScreen(
                        const float *tar, 
                        const int   numPoints, 
                        const int   xMin,
                        const int   yMin,
                        const int   xMax,
                        const int   yMax,
                        int         *imagePixels,
                        const int   imageW, 
                        const int   imageH
                      );



#ifdef    __cplusplus
}
#endif  /* __cplusplus */

#endif  


   