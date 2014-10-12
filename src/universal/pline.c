//  **********************************************************
//  FILE NAME   pline.c
//  PURPOSE     
//  NOTES
//  **********************************************************
 
//  **********************************************************
//  Includes
//  **********************************************************

#define _CRT_SECURE_NO_WARNINGS

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>

#include "pline.h"
#include "linsolv.h"
#include "parabola.h"
#include "pca.h"

//  **********************************************************
//  Defines
//  **********************************************************

#define IABS(a)                   (((a)>0)?(a):-(a))
#define FABS(a)                   (((a)>0.0f)?(a):-(a))
#define ISWAP(aa,bb)              { int ttt; ttt = (aa); (aa)=(bb); (bb)=ttt; }

#define TOO_SMALL_VALUE           1.0e-6f

//  **********************************************************
//  Types
//  **********************************************************

typedef struct tagPointFeature
{
  float m_dot;
  int   m_index;
} PointFeature;

//  **********************************************************
//  Vars
//  **********************************************************

static PointFeature featArray[NUM_POINTS_ROUGH_POLYLINE];

static  V2f             s_points[MAX_POINTS_IN_POLYLINE];
static LetterSet        s_letterSet;

static  V2f             s_pointsTest[MAX_POINTS_IN_POLYLINE];

// Mean descriptor for each letter
static PointDescriptor  s_descCharMean[256];
static int              s_numTestSamples[256];
static float            s_maxErrHog;

//  **********************************************************
//  Func
//  **********************************************************


#define LINE_ACC    10
#define LINE_HALF   (1 << (LINE_ACC-1) )

static int   _writeLine(
                        int         *imageDst, 
                        const int    imageW,
                        const int    imageH,
                        const int     xS, 
                        const int     yS, 
                        const int     xE,
                        const int     yE,
                        const int     valDraw
                      )
{
  int  xs, ys, xe, ye, adx, ady, dx, dy, stepAcc, xAcc, yAcc, off, x, y;

  USE_PARAM(imageH);
  xs = xS; ys = yS; xe = xE; ye = yE;
  adx = IABS(xe - xs);
  ady = IABS(ye - ys);
  if (adx > ady)
  {
    if (adx == 0)
      return -1;
    // hor line
    if (xs > xe)
    {
      int t; t = xs; xs = xe; xe = t; t = ys; ys = ye; ye = t;
    }
    // Hor line tendention
    dx = xe - xs;
    stepAcc = ((ye - ys) << LINE_ACC ) / dx;
    yAcc  = (ys << LINE_ACC) + LINE_HALF;
    for (x = xs; x < xe; x++, yAcc += stepAcc)
    {
      y = yAcc >> LINE_ACC;
      off = y * imageW + x;
      if ((x < 0) || (y < 0) || (x >= imageW) || (y >= imageH))
        continue;
      imageDst[off] = valDraw;
    }

  } // hor line
  else
  {
    if (ady == 0)
      return -1;
    // vert line
    if (ys > ye)
    {
      int t; t = xs; xs = xe; xe = t; t = ys; ys = ye; ye = t;
    }
    // Hor line tendention
    dy = ye - ys;
    stepAcc = ((xe - xs) << LINE_ACC ) / dy;
    xAcc  = (xs << LINE_ACC) + LINE_HALF;
    for (y = ys; y < ye; y++, xAcc += stepAcc)
    {
      x = xAcc >> LINE_ACC;
      off = y * imageW + x;
      if ((x < 0) || (y < 0) || (x >= imageW) || (y >= imageH))
        continue;
      imageDst[off] = valDraw;
    }
  }   // vert line
  return 0;
}

#define MAX_ARG_SQRT_TABLE  256

static int    s_initSqrtTable = 0;
static float  s_sqrtTable[MAX_ARG_SQRT_TABLE];

static void _initSqrtTable()
{
  int   i;

  for (i = 0; i < MAX_ARG_SQRT_TABLE; i++)
    s_sqrtTable[i] = sqrtf( (float) i );
}

static void _writePoint(int *imageDst, const int imageW, const int imageH, const int xS, const int yS, const int color, const int radius)
{
  int     dx, dy, r2, radius2, x, y;
  float   ratio;
  int     srcR, srcG, srcB;
  int     off, val;
  int     dstR, dstG, dstB;

  if (imageDst == NULL)
    return;

  radius2 = radius *radius;
  srcR    = (color >> 16) & 0xff;
  srcG    = (color >>  8) & 0xff;
  srcB    = (color      ) & 0xff;
  if (!s_initSqrtTable)
  {
    s_initSqrtTable = 1;
    _initSqrtTable();
  }
  for (dy = -radius; dy <= +radius; dy++)
  {
    for (dx = -radius; dx <= +radius; dx++)
    {
      r2 = dx * dx + dy * dy;
      if (r2 >= radius2)
        continue;

      ratio = s_sqrtTable[r2] / (float)radius;
      x = xS + dx;
      y = yS + dy;
      if ( (x < 0) || (y < 0) || (x >= imageW) || (y >= imageH) )
        continue;
      off = x + y * imageW;
      val = imageDst[off];
      dstR = (val >> 16) & 0xff;
      dstG = (val >>  8) & 0xff;
      dstB = (val      ) & 0xff;

      dstR = (int)( srcR * (1.0f - ratio) + dstR * ratio );
      dstG = (int)( srcG * (1.0f - ratio) + dstG * ratio );
      dstB = (int)( srcB * (1.0f - ratio) + dstB * ratio );
      imageDst[off] = 0xff000000 | (dstR << 16) | (dstG << 8) | dstB;
    }   // for (dx)
  }     // for (dy)
}


static void _writeLeterToScreen(LetterDesc *desc, ImageDesc *screenDst)
{
  int         p;
  PLine       *polyline;
  int         xs, ys, xe, ye;

  polyline = &desc->m_polyline;
  for (p = 0; p < polyline->m_numPoints - 1; p++)
  {
    xs = polyline->m_points[p].x;
    ys = polyline->m_points[p].y;
    xe = polyline->m_points[p + 1].x;
    ye = polyline->m_points[p + 1].y;
    _writeLine(screenDst->m_pixels, screenDst->m_width, screenDst->m_height, xs, ys, xe, ye, 0xffeeffee);
  }
}

void RenderDescriptorTableToScreen( 
                                      const PointDescriptor *desc, 
                                      const int xLeft, const int yTop, const int wTable, const int hTable,
                                      int *imagePixels, const int imageW, const int imageH
                                    )
{
  int   x, y, r, c, k;
  int   xMin, xMax, yMin, yMax, val, offRow;

  // draw values
  k = 0;
  for (r = 0; r < NUM_RADIUSES; r++)
  {
    yMin = yTop + hTable * (r+0) / NUM_RADIUSES;
    yMax = yTop + hTable * (r+1) / NUM_RADIUSES;
    for (c = 0; c < NUM_ANGLES; c++)
    {
      xMin = xLeft + wTable * (c+0) / NUM_ANGLES;
      xMax = xLeft + wTable * (c+1) / NUM_ANGLES;

      val = desc->m_values[k++];
      val = 0xff000000 | (val << 16) | (val << 8) | val;
      for (y = yMin; y < yMax; y++)
      {
        offRow = y * imageW;
        for (x = xMin; x < xMax; x++)
        {
          imagePixels[offRow + x] = val;
        }   // for (x)
      }     // for (y)


    }       // for (c)
  }         // for (r)

  // draw grid
  for (r = 0; r <= NUM_RADIUSES; r++)
  {
    y = yTop + hTable * r / NUM_RADIUSES;
    _writeLine(imagePixels, imageW, imageH, xLeft, y, xLeft + wTable, y, 0xffeeffee);
  }
  for (c = 0; c <= NUM_ANGLES; c++)
  {
    x = xLeft + wTable * c / NUM_ANGLES;
    _writeLine(imagePixels, imageW, imageH, x, yTop, x, yTop + hTable, 0xffeeffee);
  }
}

static void _writeTransformedPointsToScreen(
                                            int        *imagePixels, 
                                            const int   imageW, 
                                            const int   imageH,
                                            const V2f  *points,
                                            const int   numPoints,
                                            const int   xMin,
                                            const int   yMin,
                                            const int   xMax,
                                            const int   yMax,
                                            const int   lineColorARGB
                                          )
{
  int     i, x, y, xPrev, yPrev;
  float   pxMin, pyMin, pxMax, pyMax, px, py;
  float   dx, dy, scale;

  // Write bbox
  _writeLine(imagePixels, imageW, imageH, xMin, yMin, xMax, yMin, 0xffeeffee);
  _writeLine(imagePixels, imageW, imageH, xMin, yMin, xMin, yMax, 0xffeeffee);
  _writeLine(imagePixels, imageW, imageH, xMax, yMin, xMax, yMax, 0xffeeffee);
  _writeLine(imagePixels, imageW, imageH, xMin, yMax, xMax, yMax, 0xffeeffee);


  // Get letter bbox
  pxMin = pyMin = +1.0e12f;
  pxMax = pyMax = -1.0e12f;
  for (i = 0; i < numPoints; i++)
  {
    pxMin = (points[i].x < pxMin)? points[i].x: pxMin;
    pxMax = (points[i].x > pxMax)? points[i].x: pxMax;
    pyMin = (points[i].y < pyMin)? points[i].y: pyMin;
    pyMax = (points[i].y > pyMax)? points[i].y: pyMax;
  }   // for (i)

  dx    = (pxMax - pxMin);
  dy    = (pyMax - pyMin);
  scale = (dx > dy)? 1.0f / dx: 1.0f / dy;
  xPrev = yPrev = -1;
  for (i = 0; i < numPoints; i++)
  {
    px = (points[i].x - pxMin) * scale;
    py = (points[i].y - pyMin) * scale;

    x  = (int)(xMin + px * (xMax - xMin));
    y  = (int)(yMin + py * (yMax - yMin));
    if (xPrev != -1)
      _writeLine(imagePixels, imageW, imageH, xPrev, yPrev, x, y, lineColorARGB);
    xPrev = x; yPrev = y;
  }     // for (i)
}
static __inline float _power(const float x, const int n)
{
  float res; int i ;

  if (n == 0)
    return 1.0f;
  if (n == 1)
    return x;
  res = x;
  for (i = 1; i < n; i++)
    res = res * x;
  return res;
}

static void _writeParamCurve(
                                int          *imagePixels, 
                                const int     imageW, 
                                const int     imageH,
                                const float  *xKoefs,
                                const float  *yKoefs,
                                const int     numKoefs,
                                const int     xMin,
                                const int     yMin,
                                const int     xMax,
                                const int     yMax
                            )
{
  float   t, x, y;
  int     i;
  int     xx, yy, px, py;

  px = py = -1;
  for (t = 0; t <= 1.0f; t += 0.05f)
  {
    // x(t), y(t)
    x = 0.0f; y = 0.0f;
    for (i = 0; i < numKoefs; i++)
    {
      x += xKoefs[i] * _power(t, i);
      y += yKoefs[i] * _power(t, i);
    }

    xx = (int)(xMin + (xMax - xMin) * x);
    yy = (int)(yMin + (yMax - yMin) * y);
    if (px != -1)
      _writeLine(imagePixels, imageW, imageH, px, py, xx, yy, 0xffee7788);
    px = xx; py = yy;

  }   // for (t)
}                                

static void _writeFeaturePoints(
                                int            *imagePixels, 
                                const int       imageW, 
                                const int       imageH,
                                const FeaturePoints *featPoints,
                                const int     xMin,
                                const int     yMin,
                                const int     xMax,
                                const int     yMax,
                                const int     colARGB
                          )
{
  float               x, y;
  int                 i, xx, yy;

  // write only m_v[1] points
  for (i = 0; i < 3; i++)
  {
    x = featPoints->m_points[i].x;
    y = featPoints->m_points[i].y;
    xx = (int)(xMin + (xMax - xMin) * x);
    yy = (int)(yMin + (yMax - yMin) * y);

    _writePoint(imagePixels, imageW, imageH, xx, yy, colARGB, 8);
  }
}

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
  int       i, x, y, xp, yp;
  float     tarMin, tarMax;

  tarMin = tarMax = tar[TAR_NEIGH];
  for (i = TAR_NEIGH; i < numPoints - TAR_NEIGH; i++)
  {
    if (tar[i] < tarMin) tarMin = tar[i];
    if (tar[i] > tarMax) tarMax = tar[i];
  }   // for (i)
  xp = yp = -1;
  for (i = TAR_NEIGH; i < numPoints - TAR_NEIGH; i++)
  {
    x = xMin + (xMax - xMin) * i / numPoints;
    y = (int)(yMin + (yMax - yMin) * (tar[i] - tarMin) / (tarMax - tarMin));
    if (xp != -1)
      _writeLine(imagePixels, imageW, imageH, xp, yp, x, y, 0xffeeffee);
    xp = x; yp = y;
  }

}



//  **********************************************************

static __inline int _getAngle(const V2f *v)
{
  float tan;
  int   angle;

  if (FABS(v->x) <= 1.0e-8f)
  {
    if (v->y > 0)
      return 90;
    else
      return 270;
  }
  if (v->x > 0.0f)
  {
    tan     = v->y / v->x;
    angle   = (int)(atanf(tan) * 180.0f / 3.1415926f);
    if (angle < 0)
      angle += 360;
  }
  else
  {
    tan   = - v->y / v->x;
    angle = (int)(180.0f - atanf(tan) * 180.0f / 3.1415926f);
  }
  return angle;
}

// **********************************************************
static int _compareFeatPoint(const void *a, const void *b)
{
  const PointFeature *fa, *fb;
  fa = (const PointFeature*)a;
  fb = (const PointFeature*)b;
  if (fa->m_dot < fb->m_dot)
    return -1;
  return (fa->m_dot > fb->m_dot);
}
static void _smoothArrayByGauss(float *arr, const int numElems, const int numNeigh, const float sigma)
{
  int           i, di, index;
  float         sum, sumW, w, koefMult, koefExp;
  static float  arrOut[MAX_POINTS_IN_POLYLINE];

  koefMult = 1.0f / (sqrtf(2.0f * 3.14159f) * sigma);
  koefExp  = 1.0f / (2.0f * sigma * sigma);
  for (i = 0; i < numElems; i++)
  {
    sum = 0.0f; sumW = 0.0f;
    for (di = -numNeigh; di <= numNeigh; di++)
    {
      index = i + di;
      if (index < 0)    index = 0;
      if (index > numElems-1)  index = numElems-1;
      w = koefMult / expf( di * di * koefExp );
      sum += (float)arr[index] * w;
      sumW += w;
    }
    arrOut[i] = sum / sumW;
  }
  for (i = 0; i < numElems; i++)
    arr[i] = arrOut[i];
}

static void _setupPolylineFeatureMap(PLine *polyline, const V2f *vCenter, const V2f *axisX, const V2f *axisY)
{
  static  float     dotArray[MAX_POINTS_IN_POLYLINE];
  float             xMin, xMax, yMin, yMax, x, y, xc, yc;
  int               i;
  V2f               v;
  float             scale;
  PointDescriptor   *pDesc;
  int               count;
  V2f               vPrev, vNext;
  float             dot;
  int               k, numFeatures;
  int               xx, yy;

  // transform points to new coordinate system
  // and detect bbox
  xMin = yMin = 1000000.0f;
  xMax = yMax = -1000000.0f;
  for (i = 0; i < polyline->m_numPoints; i++)
  {
    v.x = polyline->m_points[i].x - vCenter->x;
    v.y = polyline->m_points[i].y - vCenter->y;
    x = UtilV2fDotProduct(&v, axisX);
    y = UtilV2fDotProduct(&v, axisY);
    s_points[i].x = x;
    s_points[i].y = y;
    if (x < xMin) xMin = x;
    if (y < yMin) yMin = y;
    if (x > xMax) xMax = x;
    if (y > yMax) yMax = y;
  }
  xMin += 1.0f;
  xMax += 1.0f;
  // Normalize to [-1..+1][-1..+1] transformed coordinates
  xc = (xMin + xMax) * 0.5f;
  yc = (yMin + yMax) * 0.5f;
  scale = (xMax - xMin > yMax - yMin)? 2.0f / (xMax - xMin): 2.0f / (yMax - yMin);
  for (i = 0; i < polyline->m_numPoints; i++)
  {
    x = s_points[i].x;
    y = s_points[i].y;
    //points[i].x = (FEATURE_MAP_COLS/2) + (x - xc) * scale * (FEATURE_MAP_COLS/2);
    //points[i].y = (FEATURE_MAP_ROWS/2) + (y - yc) * scale * (FEATURE_MAP_ROWS/2);
    s_points[i].x = FEATURE_MAP_COLS * (x - xMin) / (xMax - xMin);
    s_points[i].y = FEATURE_MAP_ROWS * (y - yMin) / (yMax - yMin);
  }

  dotArray[0] = 0.0f;
  dotArray[polyline->m_numPoints-1] = 0.0f;
  for (i = 1; i < polyline->m_numPoints-1; i++)
  {
    vPrev.x = (float)polyline->m_points[i].x - polyline->m_points[i - 1].x;
    vPrev.y = (float)polyline->m_points[i].y - polyline->m_points[i - 1].y;
    vNext.x = (float)polyline->m_points[i + 1].x - polyline->m_points[i].x;
    vNext.y = (float)polyline->m_points[i + 1].y - polyline->m_points[i].y;
    UtilV2fNormalize(&vPrev);
    UtilV2fNormalize(&vNext);
    dot = UtilV2fDotProduct(&vPrev, &vNext);
    dotArray[i] = dot;
  }
  dotArray[0] = dotArray[1];
  dotArray[polyline->m_numPoints-1] = dotArray[polyline->m_numPoints-2];
  // smooth dotArray
  _smoothArrayByGauss(dotArray, polyline->m_numPoints, 4, 1.1f);

  // detect maximums and minimums in dotArray
  k = 0;
  for (i = 1; i < polyline->m_numPoints-1; i++)
  {
    if ( 
          ( (dotArray[i] > dotArray[i - 1]) && (dotArray[i] > dotArray[i + 1]) ) ||
          ( (dotArray[i] < dotArray[i - 1]) && (dotArray[i] < dotArray[i + 1]) ) 
       )
    {
      featArray[k].m_dot    = dotArray[i];
      featArray[k].m_index  = i;
      k++;
    }
  }
  featArray[k].m_dot    = 0.0f;
  featArray[k].m_index  = 0;
  k++;
  featArray[k].m_dot    = 0.0f;
  featArray[k].m_index  = polyline->m_numPoints-1;
  k++;

  numFeatures = k;
  qsort(featArray, numFeatures, sizeof(PointFeature), _compareFeatPoint);

  pDesc = &polyline->m_desc;
  memset(pDesc->m_featMap, 0, FEATURE_MAP_ROWS * FEATURE_MAP_COLS * sizeof(int) );
  count = 5;
  if (count > numFeatures)
    count = numFeatures;
  for (k = 0; k < count; k++)
  {
    i = featArray[k].m_index;
    xx = (int)s_points[i].x;
    yy = (int)s_points[i].y;
    if (xx < 0)
      xx = 0;
    if (yy < 0)
      yy = 0;
    if (xx >= FEATURE_MAP_COLS)
      xx = FEATURE_MAP_COLS-1;
    if (yy >= FEATURE_MAP_ROWS)
      yy = FEATURE_MAP_ROWS-1;
    pDesc->m_featMap[xx + yy * FEATURE_MAP_COLS] = 1;
  }
}

static float _getDescriptorsDifByFeaturesMap(const PointDescriptor *descTrain, const PointDescriptor *descTest)
{
  int     x, y, dif, off, hasSame;
  float   fdif;

  dif = 0;
  off = 0;
  for (y = 0; y < FEATURE_MAP_ROWS; y++)
  {
    for (x = 0; x < FEATURE_MAP_COLS; x++, off++)
    {
      if (descTest->m_featMap[off] == 0)
        continue;
      hasSame = 0;
      if (descTrain->m_featMap[off] != 0)
        hasSame = 1;
      if (x > 0)
        if (descTrain->m_featMap[off - 1] != 0)
          hasSame = 1;
      if (x < FEATURE_MAP_COLS-1)
        if (descTrain->m_featMap[off + 1] != 0)
          hasSame = 1;
      if (y > 0)
        if (descTrain->m_featMap[off - FEATURE_MAP_COLS] != 0)
          hasSame = 1;
      if (y < FEATURE_MAP_ROWS-1)
        if (descTrain->m_featMap[off + FEATURE_MAP_COLS] != 0)
          hasSame = 1;
      if (!hasSame)
        dif++;
    }     // for (x)
  }       // for (y)
  fdif = (float)dif;
  return fdif;
}

// ****************************************************************************************************
// Histogram of Oriented Gradients (Hog) based letter matching
// ****************************************************************************************************
static void _setupPolylineHog(PLine *polyline, const V2f *vCenter, const V2f *axisX, const V2f *axisY)
{
  float             xMin, xMax, yMin, yMax, x, y;
  int               i;
  V2f               v;
  float             scale;
  PointDescriptor   *pDesc;
  int               angle, dir;
  int               row, col;
  int               indexCell, indexHist;
  float             sum, dx, dy;

  // transform points to new coordinate system
  // and detect bbox
  xMin = yMin = 1000000.0f;
  xMax = yMax = -1000000.0f;
  for (i = 0; i < polyline->m_numPoints; i++)
  {
    v.x = polyline->m_points[i].x - vCenter->x;
    v.y = polyline->m_points[i].y - vCenter->y;
    x = UtilV2fDotProduct(&v, axisX);
    y = UtilV2fDotProduct(&v, axisY);
    s_points[i].x = x;
    s_points[i].y = y;
    if (x < xMin) xMin = x;
    if (y < yMin) yMin = y;
    if (x > xMax) xMax = x;
    if (y > yMax) yMax = y;
  }
  xMin += 1.0f;
  xMax += 1.0f;
  // Normalize to [0..+1][0..+1] transformed coordinates
  dx = (xMax - xMin);
  dy = (yMax - yMin);

  assert(polyline->m_numPoints > 4);
  assert( ( FABS(dx) > 0.0001f) || ( FABS(dy) > 0.0001f) );
  scale = (dx > dy)?  (1.0f / dx) : (1.0f / dy);
  for (i = 0; i < polyline->m_numPoints; i++)
  {
    x = s_points[i].x;
    y = s_points[i].y;
    s_points[i].x = (x - xMin) * scale;
    s_points[i].y = (y - yMin) * scale;
  }

  pDesc = &polyline->m_desc;
  memset(pDesc->m_hog, 0, sizeof(pDesc->m_hog) );

  for (i = 0; i < polyline->m_numPoints - 1; i++)
  {
    v.x     = s_points[i + 1].x - s_points[i].x;
    v.y     = s_points[i + 1].y - s_points[i].y;
    angle   = _getAngle(&v);
    if (angle >= 360 - 11)
      angle = angle - 11;
    dir = (int)( (angle + 11.25f) / 22.5f );
    assert(dir >= 0 );
    assert(dir <= 15 );

    col = (s_points[i].x < 0.5f)? 0: 1;
    row = (s_points[i].y < 0.5f)? 0: 1;
    indexCell = (row << 1) + col;
    indexHist = indexCell * NUM_HOG_ANGLES + dir;
    assert(indexHist >= 0);
    assert(indexHist < NUM_HOG_ELEMENTS);
    pDesc->m_hog[indexHist] += 1.0f;
  }

  // Normalize histogram by sum
  sum = 0.0f;
  for (i = 0; i < NUM_HOG_ELEMENTS; i++)
    sum += pDesc->m_hog[i];
  sum += 1.0e-4f;
  for (i = 0; i < NUM_HOG_ELEMENTS; i++)
    pDesc->m_hog[i] = pDesc->m_hog[i] / sum;
  // Replace histogram entries with square root
  for (i = 0; i < NUM_HOG_ELEMENTS; i++)
    pDesc->m_hog[i] = sqrtf(pDesc->m_hog[i]);
  // Normalize histogram by normal
  sum = 0.0f;
  for (i = 0; i < NUM_HOG_ELEMENTS; i++)
    sum += (pDesc->m_hog[i] * pDesc->m_hog[i]);
  sum = sqrtf(sum) + 1.0e-4f;
  for (i = 0; i < NUM_HOG_ELEMENTS; i++)
    pDesc->m_hog[i] = pDesc->m_hog[i] / sum;
}
static float _getDescriptorsDifByHog(const PointDescriptor *descA, const PointDescriptor *descB)
{
  float   dif;
  float   up, lo;
  int     k;

  dif = 0.0f;
  for (k = 0; k < NUM_HOG_ELEMENTS; k++)
  {
    up = (descA->m_hog[k] - descB->m_hog[k]);
    lo = (descA->m_hog[k] + descB->m_hog[k]);
    if (FABS(lo) > 1.0e-12f)
      dif += up * up / lo;
  }
  dif *= 0.5f;
  return dif;
}

// ****************************************************************************************************
// Special direction code based letter matching
// ****************************************************************************************************

static void _setupPolylineDirCodes(PLine *polyline, const V2f *vCenter, const V2f *axisX, const V2f *axisY)
{
  float             xMin, xMax, yMin, yMax, x, y;
  int               i;
  V2f               vCur, vPrev, v, vN, vNext, vProj;
  float             scale;
  PointDescriptor   *pDesc;
  float             dx, dy;
  float             dist2cur, dist2prev;
  int               a, radInc, codeAngle;
  DirCode           code, codePrev;
  int               c;
  V2f               vC;

  // transform points to new coordinate system
  // and detect bbox
  xMin = yMin = 1000000.0f;
  xMax = yMax = -1000000.0f;
  for (i = 0; i < polyline->m_numPoints; i++)
  {
    v.x = polyline->m_points[i].x - vCenter->x;
    v.y = polyline->m_points[i].y - vCenter->y;
    x = UtilV2fDotProduct(&v, axisX);
    y = UtilV2fDotProduct(&v, axisY);
    s_points[i].x = x;
    s_points[i].y = y;
    if (x < xMin) xMin = x;
    if (y < yMin) yMin = y;
    if (x > xMax) xMax = x;
    if (y > yMax) yMax = y;
  }
  xMin += 1.0f;
  xMax += 1.0f;
  // Normalize to [0..+1][0..+1] transformed coordinates
  dx = (xMax - xMin);
  dy = (yMax - yMin);

  assert(polyline->m_numPoints > 4);
  assert( ( FABS(dx) > 0.0001f) || ( FABS(dy) > 0.0001f) );
  scale = (dx > dy)?  (1.0f / dx) : (1.0f / dy);
  // calc new centroid
  vC.x = vC.y = 0.0f;
  for (i = 0; i < polyline->m_numPoints; i++)
  {
    x = s_points[i].x;
    y = s_points[i].y;
    s_points[i].x = (x - xMin) * scale;
    s_points[i].y = (y - yMin) * scale;
    vC.x += s_points[i].x;
    vC.y += s_points[i].y;
  }
  vC.x /= polyline->m_numPoints;
  vC.y /= polyline->m_numPoints;

  pDesc = &polyline->m_desc;
  for (i = 0; i < MAX_DIR_CODES; i++)
    pDesc->m_dirCodes[i] = DIRCODE_NA;

  codePrev = DIRCODE_NA;
  c = 0;
  for (i = 1; i < polyline->m_numPoints - 1; i++)
  {
    // Radius inc or dec ?
    vCur.x  = s_points[i + 0].x - vC.x;
    vCur.y  = s_points[i + 0].y - vC.y;
    vPrev.x = s_points[i - 1].x - vC.x;
    vPrev.y = s_points[i - 1].y - vC.y;

    dist2cur  = vCur.x  * vCur.x  + vCur.y  * vCur.y;
    dist2prev = vPrev.x * vPrev.x + vPrev.y * vPrev.y;
    radInc = (dist2cur > dist2prev)? 1: 0;

    // Angle
    vPrev.x = s_points[i + 0].x - s_points[i - 1].x;
    vPrev.y = s_points[i + 0].y - s_points[i - 1].y;
    vNext.x = s_points[i + 1].x - s_points[i + 0].x;
    vNext.y = s_points[i + 1].y - s_points[i + 0].y;

    vN = vPrev;
    UtilV2fNormalize(&vN);
    vProj.x = UtilV2fDotProduct(&vN, &vNext);
    vN.x = +vPrev.y;
    vN.y =-vPrev.x;
    UtilV2fNormalize(&vN);
    vProj.y = UtilV2fDotProduct(&vN, &vNext);

    a = _getAngle(&vProj);
    if (a > 180)
      a = a - 360;
    if ((a >= -40) && (a <= +40))
      codeAngle = ANGLE_SMALL;
    else if ( ( a >= -90) && (a <= -40) )
      codeAngle = ANGLE_NEG_NORMAL;
    else if (a < -90)
      codeAngle = ANGLE_NEG_LARGE;
    else if ( ( a >= +40) && (a <= +90) )
      codeAngle = ANGLE_POS_NORMAL;
    else
      codeAngle = ANGLE_POS_LARGE;

    code = (DirCode)(radInc * 5 + codeAngle);
    if (codePrev == code)
      continue;

    assert(c < MAX_DIR_CODES);
    pDesc->m_dirCodes[c++] = code;
    codePrev = code;
  }
}

static float _getDescriptorsDifByCode(const PointDescriptor *descA, const PointDescriptor *descB)
{
  int   i, numA, numB;
  int   deltaLen, minLen;
  int   dif;

  for (i = 0; i < MAX_DIR_CODES; i++)
  {
    if (descA->m_dirCodes[i] == MAX_DIR_CODES)
      break;
  } 
  numA = i;
  for (i = 0; i < MAX_DIR_CODES; i++)
  {
    if (descB->m_dirCodes[i] == MAX_DIR_CODES)
      break;
  } 
  numB = i;
  deltaLen = IABS(numA - numB);

  dif = 0;
  minLen = (numA < numB)? numA: numB;
  for (i = 0; i < minLen; i++)
  {
    dif += (descA->m_dirCodes[i] != descB->m_dirCodes[i])?1: 0;
  }
  dif += deltaLen;
  return (float)dif;
}

// ****************************************************************************************************
// Triangle Area Representation (TAR)
// ****************************************************************************************************


static __inline float _getTar(const int i, const int n)
{
  float   a, b, c, d, e, f;
  float   tar;
  a = s_points[i - n].x;
  b = s_points[i - n].y;
  c = s_points[i + 0].x;
  d = s_points[i + 0].y;
  e = s_points[i + n].x;
  f = s_points[i + n].y;
  tar = 0.5f * (a * d + b * e + c * f - d * e - b * c - a * f);
  return tar;
}
static void _setupPolylineTar(PLine *polyline, const V2f *vCenter, const V2f *axisX, const V2f *axisY)
{
  float             xMin, xMax, yMin, yMax, x, y;
  int               i;
  float             scale;
  float             dx, dy;
  V2f               v;
  int               j, jBest;
  float             tar, tarMax, tarMin, maxSide;

  // transform points to new coordinate system
  // and detect bbox
  xMin = yMin = 1000000.0f;
  xMax = yMax = -1000000.0f;
  for (i = 0; i < polyline->m_numPoints; i++)
  {
    v.x = polyline->m_points[i].x - vCenter->x;
    v.y = polyline->m_points[i].y - vCenter->y;
    x = UtilV2fDotProduct(&v, axisX);
    y = UtilV2fDotProduct(&v, axisY);
    s_points[i].x = x;
    s_points[i].y = y;
    if (x < xMin) xMin = x;
    if (y < yMin) yMin = y;
    if (x > xMax) xMax = x;
    if (y > yMax) yMax = y;
  }
  xMin += 1.0f;
  xMax += 1.0f;
  // Normalize to [0..+1][0..+1] transformed coordinates
  dx = (xMax - xMin);
  dy = (yMax - yMin);

  assert(polyline->m_numPoints > 4);
  assert( ( FABS(dx) > 0.0001f) || ( FABS(dy) > 0.0001f) );
  scale = (dx > dy)?  (1.0f / dx) : (1.0f / dy);
  for (i = 0; i < polyline->m_numPoints; i++)
  {
    x = s_points[i].x;
    y = s_points[i].y;
    s_points[i].x = (x - xMin) * scale;
    s_points[i].y = (y - yMin) * scale;
  }

  tarMin = 1.0e12f; tarMax = -1.0e12f;
  for (i = 0; i < MAX_POINTS_IN_POLYLINE; i++)
    polyline->m_tar[i] = 0.0f;
  for (i = TAR_NEIGH; i < polyline->m_numPoints - TAR_NEIGH; i++)
  {
    jBest = -1;
    tarMax = 0.0f;
    for(j = 1; j <= TAR_NEIGH; j++)
    {
      tar = _getTar(i, j);
      if (FABS(tar) > tarMax)
      {
        tarMax = FABS(tar);
        jBest = j;
      }
    }     // for (j)
    polyline->m_tar[i] = _getTar(i, jBest);
    if (polyline->m_tar[i] < tarMin)
      tarMin = polyline->m_tar[i];
    if (polyline->m_tar[i] > tarMax)
      tarMax = polyline->m_tar[i];
  }       // for (i)

  // normalize tar
  maxSide = ( (-tarMin) > tarMax)? (-tarMin): tarMax;
  scale = 1.0f / maxSide;
  for (i = TAR_NEIGH; i < polyline->m_numPoints - TAR_NEIGH; i++)
  {
    tar = polyline->m_tar[i];
    tar = tar * scale;
    polyline->m_tar[i] = tar;
  }   // for (i)

}

static float _getDescriptorsDifByTar(float *tarOrig, float *tarTest, const int numPoints)
{
  int     indMin, indMax, q, s, i, j, sBest, n;
  float   dif, minDif, dt;

  q = numPoints * 1 / 4;
  indMin = q + TAR_NEIGH;
  indMax = numPoints - q - TAR_NEIGH;

  minDif = 1.0e12f;
  sBest = -1;
  //for (s = -q; s <= +q; s++)
  for (s = -6; s <= +6; s++)
  {
    // calc dif between tar's with shift s
    dif = 0.0f;
    n = 0;
    for (i = indMin; i < indMax; i++)
    {
      j = i + s;
      dt = tarOrig[i] - tarTest[j];
      dif += dt * dt;
      n++;
    }
    dif /= n;
    dif = sqrtf(dif);
    if (dif < minDif)
    {
      minDif = dif;
      sBest = s;
    }
  }     // for (s) all shifts
  return minDif;
}

// ****************************************************************************************************
// Force letter model
// ****************************************************************************************************

static void _setupPolylineForce(PLine *polyline, const V2f *vCenter, const V2f *axisX, const V2f *axisY)
{
  float             xMin, xMax, yMin, yMax, x, y;
  int               i;
  float             scale;
  float             dx, dy;
  V2f               v;

  // transform points to new coordinate system
  // with 1st vertex at (0,0)
  xMin = yMin = 1000000.0f;
  xMax = yMax = -1000000.0f;
  for (i = 0; i < polyline->m_numPoints; i++)
  {
    v.x = polyline->m_points[i].x - vCenter->x;
    v.y = polyline->m_points[i].y - vCenter->y;
    x = UtilV2fDotProduct(&v, axisX);
    y = UtilV2fDotProduct(&v, axisY);
    s_points[i].x = x;
    s_points[i].y = y;
    if (x < xMin) xMin = x;
    if (y < yMin) yMin = y;
    if (x > xMax) xMax = x;
    if (y > yMax) yMax = y;
  }
  // Normalize to [0..+1][0..+1] transformed coordinates
  dx = (xMax - xMin);
  dy = (yMax - yMin);

  assert(polyline->m_numPoints > 4);
  assert( ( FABS(dx) > 0.0001f) || ( FABS(dy) > 0.0001f) );
  scale = (dx > dy)?  (1.0f / dx) : (1.0f / dy);
  for (i = 0; i < polyline->m_numPoints; i++)
  {
    x = s_points[i].x;
    y = s_points[i].y;
    s_points[i].x = (x - xMin) * scale;
    s_points[i].y = (y - yMin) * scale;
  }
}

static int _isLineIntersected(const V2f *p0, const V2f *p1, const V2f *t0, const V2f *t1)
{
  V2f   v, n;
  float s0, s1, c0, c1;
  float ps, pe, ts, te;
  V2f   pMin, pMax, tMin, tMax;

  // chech bounding boxes: x
  pMin.x = (p0->x < p1->x)?p0->x: p1->x;
  pMin.y = (p0->y < p1->y)?p0->y: p1->y;
  pMax.x = (p0->x > p1->x)?p0->x: p1->x;
  pMax.y = (p0->y > p1->y)?p0->y: p1->y;

  tMin.x = (t0->x < t1->x)?t0->x: t1->x;
  tMin.y = (t0->y < t1->y)?t0->y: t1->y;
  tMax.x = (t0->x > t1->x)?t0->x: t1->x;
  tMax.y = (t0->y > t1->y)?t0->y: t1->y;

  if (pMax.x < tMin.x)
    return 0;
  if (pMax.y < tMin.y)
    return 0;
  if (pMin.x > tMax.x)
    return 0;
  if (pMin.y > tMax.y)
    return 0;



  v.x = p1->x - p0->x;
  v.y = p1->y - p0->y;
  n.x = v.y; n.y = -v.x;

  v.x = t0->x - p0->x; 
  v.y = t0->y - p0->y; 
  s0 = UtilV2fDotProduct(&v, &n);
  v.x = t1->x - p0->x; 
  v.y = t1->y - p0->y; 
  s1 = UtilV2fDotProduct(&v, &n);
  
  if ( (fabsf(s0) < TOO_SMALL_VALUE) && (fabsf(s1) < TOO_SMALL_VALUE) )
  {
    // lying on one line
    v.x = t1->x - t0->x;
    v.y = t1->y - t0->y;
    if (fabsf(v.x) > fabsf(v.y))
    {
      if (p0->x < p1->x)
      {
        ps = p0->x;
        pe = p1->x;
      }
      else
      {
        ps = p1->x;
        pe = p0->x;
      }
      if (t0->x < t1->x)
      {
        ts = t0->x;
        te = t1->x;
      }
      else
      {
        ts = t1->x;
        te = t0->x;
      }      
    }
    else
    {
      if (p0->y < p1->y)
      {
        ps = p0->y;
        pe = p1->y;
      }
      else
      {
        ps = p1->y;
        pe = p0->y;
      }
      if (t0->y < t1->y)
      {
        ts = t0->y;
        te = t1->y;
      }
      else
      {
        ts = t1->y;
        te = t0->y;
      }      
    }
    if (ts < ps)
    {
      if (te < ps)
        return 0;
      return 1;
    }
    if (ts < pe)
      return 1;
    return 0;
  }

  v.x = t1->x - t0->x; 
  v.y = t1->y - t0->y; 
  n.x = v.y; n.y = -v.x;

  v.x = p0->x - t0->x; 
  v.y = p0->y - t0->y; 
  c0 = UtilV2fDotProduct(&v, &n);
  v.x = p1->x - t0->x;
  v.y = p1->y - t0->y;
  c1 = UtilV2fDotProduct(&v, &n);

  if ( (s0 * s1 < -TOO_SMALL_VALUE) && (c0 * c1 < -TOO_SMALL_VALUE) )
    return 1;
  return 0;
}


static int _lineIntresectedPoly(const V2f *vs, const V2f *ve, const V2f *vaPoints, const int numPoints, const int indexPoint) 
{
  int   i, j;
  int   isIntersect;

  for (i = 0; i < numPoints-1; i++)
  {
    j = i + 1;
    if ( (i == indexPoint) || (j == indexPoint) )
      continue;
    isIntersect = _isLineIntersected(vs, ve, &vaPoints[i], &vaPoints[j]);
    if (isIntersect)
      return 1;
  }     // for (i) all points
  return 0;
}

static float _getErrorOfModifyOnto(const PLine *polylineTest, const PLine *polylineMatch)
{
  float   err, len, lenTest, scale;
  int     i, n;
  V2f     v;
  float   xm, ym, xt, yt, xx, yy, s, c, x, y;
  float   maxErr;

  err = 0.0f;
  // try to change coordinates of polylineTest => s_polylineModify , trying to match coordinates of polylineMatch
  // and calculate modification error
  //
  // polylineMatch is normalized point array
  //
  n = polylineTest->m_numPoints;
  if (n > polylineMatch->m_numPoints)
    n = polylineMatch->m_numPoints;

  // transform match coordinates to (0,0) for start point
  v = s_points[0];
  for (i = 0; i < n; i++)
  {
    s_points[i].x -= v.x;
    s_points[i].y -= v.y;
  }

  // polylineMatch => s_points
  // polylineTest  => s_pointsTest
  v     = s_points[n - 1];
  len   = sqrtf( v.x * v.x + v.y * v.y );

  v.x   = (float)polylineTest->m_points[n-1].x - polylineTest->m_points[0].x;
  v.y   = (float)polylineTest->m_points[n-1].y - polylineTest->m_points[0].y;
  lenTest = sqrtf( v.x * v.x + v.y * v.y );
  scale = len / lenTest;
  for (i = 1; i < n; i++)
  {
    v.x   = (float)polylineTest->m_points[i].x - polylineTest->m_points[0].x;
    v.y   = (float)polylineTest->m_points[i].y - polylineTest->m_points[0].y;
    v.x *= scale;
    v.y *= scale;
    s_pointsTest[i] = v;
  }
  s_pointsTest[0].x = s_pointsTest[0].y = 0.0f;

  xm = s_points[n - 1].x;
  ym = s_points[n - 1].y;
  xt = s_pointsTest[n - 1].x;
  yt = s_pointsTest[n - 1].y;

  s = (xt * ym - yt * xm) / ( xt * xt + yt * yt );
  if (FABS(xt) > FABS(yt) )
    c = (xm + yt * s) / xt;
  else
    c = (ym - xt * s) / yt;
  //c = (xm * yt + yt * yt * s) / (xt * yt);

  assert( s >= -2.0f );
  assert( s <= +2.0f );
  assert( c >= -2.0f );
  assert( c <= +2.0f );

  // rotate coordinate system
  for (i = 0; i < n; i++)
  {
    x = s_pointsTest[i].x;
    y = s_pointsTest[i].y;
    xx = x * c - y * s;
    yy = x * s + y * c;
    s_pointsTest[i].x = xx;
    s_pointsTest[i].y = yy;
  }

  // Build move vectors between s_pointsTest => s_points

  maxErr = 0.0f;
  err = 0.0f;
  for (i = 0; i < n; i++)
  {
    x = s_pointsTest[i].x - s_points[i].x;
    y = s_pointsTest[i].y - s_points[i].y;
    //err += (x * x + y * y);
    err = (x * x + y * y);
    if (err > maxErr)
      maxErr = err;
    //if (_lineIntresectedPoly(&s_pointsTest[i], &s_points[i], s_pointsTest, n, i) )
    //  maxErr = 1.0e12f;
    //if (_lineIntresectedPoly(&s_pointsTest[i], &s_points[i], s_points, n, i) )
    //  maxErr = 1.0e12f;
  }
  maxErr = sqrtf(maxErr);
  assert( maxErr > 0.0000001f);

  return maxErr;
}

// **********************************************************************************************
// Polynom approximation by x and y (independent)
// **********************************************************************************************
static void _buildNormalizedCurve(PLine *polyline, const V2f *vCenter, const V2f *axisX, const V2f *axisY, V2f *pointsDst)
{
  float             xMin, xMax, yMin, yMax, x, y;
  int               i;
  float             scale;
  float             dx, dy;
  V2f               v;

  // transform points to new coordinate system
  // with 1st vertex at (0,0)
  xMin = yMin = 1000000.0f;
  xMax = yMax = -1000000.0f;
  for (i = 0; i < polyline->m_numPoints; i++)
  {
    v.x = polyline->m_points[i].x - vCenter->x;
    v.y = polyline->m_points[i].y - vCenter->y;
    x = UtilV2fDotProduct(&v, axisX);
    y = UtilV2fDotProduct(&v, axisY);
    pointsDst[i].x = x;
    pointsDst[i].y = y;
    if (x < xMin) xMin = x;
    if (y < yMin) yMin = y;
    if (x > xMax) xMax = x;
    if (y > yMax) yMax = y;
  }
  // Normalize to [0..+1][0..+1] transformed coordinates
  dx = (xMax - xMin);
  dy = (yMax - yMin);

  assert(polyline->m_numPoints > 4);
  assert( ( FABS(dx) > 0.0001f) || ( FABS(dy) > 0.0001f) );
  scale = (dx > dy)?  (1.0f / dx) : (1.0f / dy);
  for (i = 0; i < polyline->m_numPoints; i++)
  {
    x = pointsDst[i].x;
    y = pointsDst[i].y;
    pointsDst[i].x = (x - xMin) * scale;
    pointsDst[i].y = (y - yMin) * scale;
  }
}
static float _getPointCurvature(const V2f *vPrev, const V2f *vCur, const V2f *vNext)
{
  V2f   vp, vn; float dot, curv;

  vp.x = vCur->x - vPrev->x;
  vp.y = vCur->y - vPrev->y;
  vn.x = vNext->x - vCur->x;
  vn.y = vNext->y - vCur->y;
  UtilV2fNormalize(&vp);
  UtilV2fNormalize(&vn);
  dot = UtilV2fDotProduct(&vp, &vn);
  curv = 1.0f - dot;
  return curv;
}
#define MASK_XMIN   (1<<0)
#define MASK_XMAX   (1<<1)
#define MASK_YMIN   (1<<2)
#define MASK_YMAX   (1<<3)

static void _getContourFeaturePoints(const V2f *points, const int numPoints, FeaturePoints *featPoints)
{
  int           index[4];
  int           i, p, iBest, step;
  static float  curvature[64];
  const V2f     *vPrev, *vCur, *vNext;
  float         maxCurv;

  // Try to find most rough points in polyline
  memset(curvature, 0, sizeof(curvature) );
  assert( numPoints <= sizeof(curvature)/sizeof(curvature[0]) );


  step = (int)(numPoints * 0.1f);
  for (i = 8; i < numPoints - 8; i++)
  {
    // estimate curvature
    vPrev = &points[i - step];
    vCur  = &points[i + 0];
    vNext = &points[i + step];
    curvature[i] = _getPointCurvature(vPrev, vCur, vNext);

  }       // for (i)

  /*
  step = (int)(numPoints * 0.1f);
  for (i = step; i < numPoints - step; i++)
  {
    maxCurv = 0.0f;
    vPrev = &points[i - step];
    vCur  = &points[i + 0];
    vNext = &points[i + step];
    maxCurv = _getPointCurvature(vPrev, vCur, vNext);

    curvature[i] = maxCurv;
  }   // for (i)
  */
  // Find point with maximum curvature
  maxCurv = 0.0f;
  for (p = 0; p < 1; p++)
  {
    // find maximum curvature point
    maxCurv = 0.0f;
    iBest = -1;
    for (i = 0; i < numPoints; i++)
    {
      if (curvature[i] > maxCurv)
      {
        maxCurv = curvature[i];
        iBest = i;
      }
    }     // for (i) num points
    //if (maxCurv < 0.6f)
    //  break;
    // clear curvature around detected curve point
    for (i = iBest - step; i < iBest + step; i++)
      curvature[i] = 0.0f;
    index[p] = iBest;
  }       // for (p) parabolas

  if (maxCurv < 0.9f)
  {
    float len2, maxLen2;
    V2f   v0, v1, v;

    // find most far point from endpoints
    iBest = -1;
    maxLen2 = 0.0f;
    v0 = points[0];
    v1 = points[numPoints-1];
    for (i = 8 ; i < numPoints - 8; i++)
    {
      v.x = points[i].x - v0.x;
      v.y = points[i].y - v0.y;
      len2 = v.x * v.x + v.y * v.y;
      if (len2 > maxLen2)
      {
        maxLen2 = len2;
        iBest = i;
      }
      v.x = points[i].x - v1.x;
      v.y = points[i].y - v1.y;
      len2 = v.x * v.x + v.y * v.y;
      if (len2 > maxLen2)
      {
        maxLen2 = len2;
        iBest = i;
      }
    }   // for (i) all points
    featPoints->m_points[0] = points[0];
    featPoints->m_points[1] = points[iBest];
    featPoints->m_points[2] = points[numPoints-1];

  }     // if (no one rough points by curvature)
  else
  {
    featPoints->m_points[0] = points[0];
    featPoints->m_points[1] = points[index[0]];
    featPoints->m_points[2] = points[numPoints-1];
  }
}


static float _getPointArrayDif(const V2f *pointsA, const V2f *pointsB, const int numPoints)
{
  float   dif, maxDif;
  int     i;
  V2f     v;

  dif = maxDif = 0.0f;
  for (i = 0; i < numPoints; i++)
  {
    v.x = pointsA[i].x - pointsB[i].x;
    v.y = pointsA[i].y - pointsB[i].y;
    dif = v.x * v.x + v.y * v.y;
    if (dif > maxDif)
      maxDif = dif;
  }
  maxDif = sqrtf(maxDif);
  return maxDif;
}

static float _getFeatureDifference(const V2f *pointsA, const V2f *pointsB)
{
  V2f     vA21, vA10, vB21, vB10;
  float   dif, dot, angA, angB, ang;

  UtilV2fSub(&pointsA[1], &pointsA[0], &vA10 );
  UtilV2fSub(&pointsA[2], &pointsA[1], &vA21 );
  UtilV2fNormalize(&vA10);
  UtilV2fNormalize(&vA21);

  UtilV2fSub(&pointsB[1], &pointsB[0], &vB10 );
  UtilV2fSub(&pointsB[2], &pointsB[1], &vB21 );
  UtilV2fNormalize(&vB10);
  UtilV2fNormalize(&vB21);

  // Dif between maikn directions
  dot = UtilV2fDotProduct(&vA10, &vB10);
  ang = acosf(dot) * 180.0f / 3.1415926f;

  // Dif between internal angles
  dot = UtilV2fDotProduct(&vA10, &vA21);
  angA = acosf(dot) * 180.0f / 3.1415926f;

  dot = UtilV2fDotProduct(&vB10, &vB21);
  angB = acosf(dot) * 180.0f / 3.1415926f;
  dif = FABS(angA - angB);

  if (ang > dif)
    dif = ang;

  return dif;
}

// ***********************************************************

LetterDesc  *PLineGetLetterFromLearningSet(const int letterIndex)
{
  assert( letterIndex >= 0);
  assert( letterIndex < s_letterSet.m_numLetters);
  return &s_letterSet.m_letters[letterIndex];
}

static int _getAngle3Points(const V2d *vPrev, const V2d *vCur, const V2d *vNext)
{
  V2f   vp, vn, vAt, vNorm, v;
  int   angle;

  vp.x = (float)vCur->x - vPrev->x;
  vp.y = (float)vCur->y - vPrev->y;
  vn.x = (float)vNext->x - vCur->x;
  vn.y = (float)vNext->y - vCur->y;
  vAt.x   = (float)vp.x;
  vAt.y   = (float)vp.y;
  vNorm.x = (float)vAt.y;
  vNorm.y =-(float)vAt.x;
  UtilV2fNormalize(&vAt);
  UtilV2fNormalize(&vNorm);

  v.x     = UtilV2fDotProduct(&vn, &vAt);
  v.y     = UtilV2fDotProduct(&vn, &vNorm);
  angle   = _getAngle(&v);
  if (angle > 180)
    angle = 360 - angle;
  return angle;
}

static __inline float _getLen(const V2d *v0, const V2d *v1)
{
  V2f   v;

  v.x = (float)v0->x - v1->x;
  v.y = (float)v0->y - v1->y;
  return sqrtf( v.x * v.x + v.y * v.y );
}

int   PLineMakeRoughByCurvature(PLine *polyline, const int minRequiredAngle)
{
  static V2d    points[MAX_POINTS_IN_POLYLINE];
  int           i, j, iPrev, iNext;
  int           angle, indexBest;
  int           minAngle;
  float         minCurv, curv, lenP, lenN;

  // keep only each 4th vertex
  for (i = 0, j = 0; i < polyline->m_numPoints; i += 4)
  {
    points[j++] = polyline->m_points[i];
  }
  polyline->m_numPoints = j;
  memcpy(polyline->m_points, points, polyline->m_numPoints * sizeof(V2d) );
  minCurv = 0;
  minAngle = 0;
  while (minAngle < minRequiredAngle)
  {
    //minCurv = 1.0e+12f;
    minAngle = 4000;
    minCurv = 1.0e12f;
    indexBest = -1;
    for (i = 1; i < polyline->m_numPoints - 1; i++)
    {
      iPrev = i - 1;
      iNext = i + 1;

      lenP = _getLen(&polyline->m_points[iPrev], &polyline->m_points[i]);
      lenN = _getLen(&polyline->m_points[i], &polyline->m_points[iNext]);

      angle = _getAngle3Points(&polyline->m_points[iPrev], &polyline->m_points[i], &polyline->m_points[iNext]);
      curv = angle * lenP * lenN / (lenP + lenN);
      if (curv < minCurv)
      {
        minCurv = curv;
        minAngle = angle;
        indexBest = i;
      }
    }     // for (i) all points
    if (minAngle > minRequiredAngle)
      break;

    // delete vertex [indexBest] in polyline
    for (i = indexBest; i < polyline->m_numPoints - 1; i++)
    {
      polyline->m_points[i] = polyline->m_points[i + 1];
    }
    polyline->m_numPoints--;

    if (polyline->m_numPoints <= 6)
      break;
  }       // while (has very low curvature point)
  return 0;
}

int   PLineMakeRoughRemovingClosePoints(PLine *polyline, const float ratioTooCloseByBbox)
{
  int   i, xMin, xMax, yMin, yMax, x, y;
  int   dx, dy, dMax, distBar, distBar2;
  int   indexToDel;
  int   minDist2, dist2;
  V2d   v;

  if (polyline->m_numPoints <= 5)
    return 0;


  // calc bbox
  xMin = yMin = +100000;
  xMax = yMax = -100000;
  for (i = 0; i < polyline->m_numPoints; i++)
  {
    x = polyline->m_points[i].x;
    y = polyline->m_points[i].y;
    xMin = (x < xMin)? x: xMin;
    yMin = (y < yMin)? y: yMin;
    xMax = (x > xMax)? x: xMax;
    yMax = (y > yMax)? y: yMax;
  }   // for (i)
  dx = xMax - xMin;
  dy = yMax - yMin;
  dMax = (dx > dy)? dx: dy;
  distBar = (int)(dMax * ratioTooCloseByBbox);
  distBar2 = distBar * distBar;

  indexToDel = 0;
  while (indexToDel >= 0)
  {
    indexToDel    = -1;
    minDist2      = 1 << 25;
    for (i = 0; i < polyline->m_numPoints - 1; i++)
    {
      v.x     = polyline->m_points[i + 1].x - polyline->m_points[i].x;
      v.y     = polyline->m_points[i + 1].y - polyline->m_points[i].y;
      dist2   = v.x * v.x + v.y * v.y;
      if (dist2 < minDist2)
      {
        minDist2 = dist2;
        indexToDel = i;
      }
    }     // for (i) all points
    if (minDist2 > distBar2)
      break;

    // delete vertex [indexBest] in polyline
    for (i = indexToDel; i < polyline->m_numPoints - 1; i++)
    {
      polyline->m_points[i] = polyline->m_points[i + 1];
    }
    polyline->m_numPoints--;
    if (polyline->m_numPoints <= 5)
      break;
  }       // while (has too close points)
  
  return 0;
}

int   PLineMakeRoughBySegLen(PLine *polyline)
{
  static V2d    points[MAX_POINTS_IN_POLYLINE];
  int           i, j;
  V2d           v;
  V2d           vPrev, vp;
  int           dist2;

  vPrev.x = vPrev.y = 1000000;
  for (i = 0, j = 0; i < polyline->m_numPoints; i++)
  {
    v = polyline->m_points[i];
    vp.x = v.x - vPrev.x;
    vp.y = v.y - vPrev.y;
    dist2 = vp.x * vp.x + vp.y * vp.y;
    if (dist2 < 12 * 12)
      continue;
    vPrev = v;
    points[j++] = v;
  }
  polyline->m_numPoints = j;
  memcpy(polyline->m_points, points, polyline->m_numPoints * sizeof(V2d) );
  return 0;
}

int   PLineMakeRoughByNumSeg(PLine *polyline, const int numRequiredPointsInPolyline)
{
  static V2d    points[MAX_POINTS_IN_POLYLINE];
  int           i, j;
  V2d           v;
  float         lenPolyline, lenStep;

  // calc polyline len
  lenPolyline = 0.0f;
  for (i = 0; i < polyline->m_numPoints - 1; i++)
  {
    v.x = polyline->m_points[i + 1].x - polyline->m_points[i].x;
    v.y = polyline->m_points[i + 1].y - polyline->m_points[i].y;
    lenPolyline += sqrtf( v.x * v.x + v.y * v.y );
  }
  // approximate step len for numRequiredPointsInPolyline points in destination
  lenStep = lenPolyline / numRequiredPointsInPolyline;

  // copy to dest
  j = 0;
  points[j++] = polyline->m_points[0];
  lenPolyline = 0.0f;
  for (i = 1, j = 0; i < polyline->m_numPoints; i++)
  {
    v.x = polyline->m_points[i].x - polyline->m_points[i - 1].x;
    v.y = polyline->m_points[i].y - polyline->m_points[i - 1].y;
    lenPolyline += sqrtf( v.x * v.x + v.y * v.y );
    if (lenPolyline >= lenStep)
    {
      lenPolyline -= lenStep;
      points[j++] = polyline->m_points[i];
    }
  }
  if (j < numRequiredPointsInPolyline)
    points[j++] = polyline->m_points[polyline->m_numPoints - 1];
  polyline->m_numPoints = j;
  memcpy(polyline->m_points, points, polyline->m_numPoints * sizeof(V2d) );
  return 0;
}

static void _makeAxisAligned(V2f *vAxis)
{
  if (FABS(vAxis[0].x) < FABS(vAxis[0].y))
  {
    V2f   tmp;
    tmp = vAxis[0]; vAxis[0] = vAxis[1]; vAxis[1] = tmp;
  }
  if (vAxis[0].x < 0.0f)
  {
    vAxis[0].x = -vAxis[0].x;
    vAxis[0].y = -vAxis[0].y;
  }
  if (vAxis[1].y < 0.0f)
  {
    vAxis[1].x = -vAxis[1].x;
    vAxis[1].y = -vAxis[1].y;
  }
}

static int _compareFloat(const void *a, const void *b)
{
  const float *fa, *fb;
  fa = (const float *)a;
  fb = (const float *)b;
  if ( (*fa) < (*fb) )
    return -1;
  return ( (*fa) > (*fb) );
}

int   PLineCreate(const LetterSet *set)
{
  int           i, ok;
  LetterDesc    *letter;
  PLine         *polyline;
  V2f           vEllipseCenter;
  float         ellipseRad[2];
  V2f           ellipseVec[2];
  static float  arr[64];
  #ifdef METHOD_HOG
  int           k;
  float         dif;
  #endif



  memcpy(&s_letterSet, set, sizeof(LetterSet) );

  // setup point descriptors for all letters (polylines)
  for (i = 0; i < s_letterSet.m_numLetters; i++)
  {
    letter    = &s_letterSet.m_letters[i];
    polyline  = &letter->m_polyline;

    #ifdef METHOD_DIR_CODE
      PLineMakeRoughByCurvature(polyline, 35);
      PLineMakeRoughRemovingClosePoints(polyline, 0.1f);
    #endif


    // Define best approximative ellipse
    ok = PcaGetAxis(polyline->m_points, polyline->m_numPoints, &vEllipseCenter, ellipseRad, ellipseVec);
    assert(ok == 0);

    _makeAxisAligned(ellipseVec);

    #ifdef METHOD_HOG
      _setupPolylineHog(polyline, &vEllipseCenter, &ellipseVec[0], &ellipseVec[1]);
    #endif
    #ifdef METHOD_DIR_CODE
      _setupPolylineDirCodes(polyline, &vEllipseCenter, &ellipseVec[0], &ellipseVec[1]);
    #endif
    #ifdef METHOD_TAR
      _setupPolylineTar(polyline, &vEllipseCenter, &ellipseVec[0], &ellipseVec[1]);
    #endif
    #ifdef METHOD_FORCE
      _setupPolylineForce(polyline, &vEllipseCenter, &ellipseVec[0], &ellipseVec[1]);
    #endif


    #ifdef METHOD_POLAPPROX
    {
      _buildNormalizedCurve(polyline, &vEllipseCenter, &ellipseVec[0], &ellipseVec[1], polyline->m_pointsNorm);
      _getContourFeaturePoints(polyline->m_pointsNorm, polyline->m_numPoints, &polyline->m_desc.m_featPoints);
    }
    #endif
  }     // for (i) all train set letters

  // Check codes conflict for different chars
  #ifdef METHOD_DIR_CODE
  for (i = 0; i < s_letterSet.m_numLetters; i++)
  {
    int       j, k, conflictFound, allSame;
    char      c, ch;
    DirCode   *codeChar, *codeAnother;
    int       angCode, angAnother, angDif;

    letter    = &s_letterSet.m_letters[i];
    c         = letter->m_letter;
    codeChar  = letter->m_polyline.m_desc.m_dirCodes;
    allSame   = 1;
    conflictFound = 0;
    for (j = 0; j < s_letterSet.m_numLetters; j++)
    {
      letter    = &s_letterSet.m_letters[j];
      ch        = letter->m_letter;
      if (ch == c)
        continue;
      codeAnother = letter->m_polyline.m_desc.m_dirCodes;
      // compare code sequence
      for (k = 0; codeChar[k] != DIRCODE_NA; k++)
      {
        if (codeAnother[k] == DIRCODE_NA)
          break;
        angCode    = codeChar[k] % 5;
        angAnother = codeAnother[k] % 5;
        angDif = IABS(angCode - angAnother);
        if (angDif > 1)
        {
          allSame = 0;
          break;
        }
      }   // for (k) all codes

      if (allSame)
      {
        conflictFound = 1;
      }
    }     // for (j)  all another letters
  }       // for (i) al letters

  #endif



  #ifdef METHOD_HOG
  // Setup mean descriptor for each know char
  memset(s_descCharMean, 0, sizeof(s_descCharMean) );
  memset(s_numTestSamples, 0, sizeof(s_numTestSamples) );
  for (i = 0; i < s_letterSet.m_numLetters; i++)
  {
    int   k;

    letter    = &s_letterSet.m_letters[i];
    polyline  = &letter->m_polyline;
    s_numTestSamples[letter->m_letter] ++;
    for (k = 0; k < NUM_HOG_ELEMENTS; k++)
    {
      s_descCharMean[letter->m_letter].m_hog[k] += polyline->m_desc.m_hog[k];
    }
  }   // for (i)
  for (i = 0 ; i < 256; i++)
  {
    if (s_numTestSamples[i] > 0)
    {
      float   scale;

      scale = 1.0f / s_numTestSamples[i];
      for (k = 0; k < NUM_HOG_ELEMENTS; k++)
        s_descCharMean[i].m_hog[k] *= scale;
    }
  }

  // Setup max dif error for single class of polyline
  k = 0;
  for (i = 1; i < s_letterSet.m_numLetters; i++)
  {
    letter    = &s_letterSet.m_letters[i];
    if (letter->m_letter != s_letterSet.m_letters[0].m_letter)
      continue;
    polyline  = &letter->m_polyline;
    dif = _getDescriptorsDifByHog(&polyline->m_desc, &s_letterSet.m_letters[0].m_polyline.m_desc);
    arr[k++] = dif;
    assert( k < sizeof(arr)/sizeof(float) );
  }
  qsort( arr, k, sizeof(float), _compareFloat);
  s_maxErrHog = arr[k * 3 / 4 ];
  #endif


  /*
  // self check: maximum inside-class difference should be < than minimum between-class difference
  maxInsideDif  = 0.0f;
  minBetweenDif = 1.0e12f;
  for (i = 1; i < s_letterSet.m_numLetters; i++)
  {
    letter    = &s_letterSet.m_letters[i];
    polyline  = &letter->m_polyline;
    dif = _getDescriptorsDifByHog(&polyline->m_desc, &s_letterSet.m_letters[0].m_polyline.m_desc);
    if (letter->m_letter == s_letterSet.m_letters[0].m_letter)
    {
      maxInsideDif = (dif > maxInsideDif)? dif: maxInsideDif;
    }
    else
    {
      minBetweenDif = (dif < minBetweenDif)? dif: minBetweenDif;
    }
  }
  //assert( maxInsideDif < minBetweenDif);
  */

  #ifdef METHOD_FORCE
  #endif

  return 0;
}

void  PLineDestroy()
{
}

int   PLineProcess(PLine *polylineTest, char *letterRecognized, int *letterIndex, ImageDesc *screenDst)
{
  int         i;
  float       dif, minDif;
  int         status, ok;
  V2f         vEllipseCenter;
  float       ellipseRad[2];
  V2f         ellipseVec[2];

  USE_PARAM(letterIndex);
  USE_PARAM(letterRecognized);
  status = 0;
  i = -1;
  dif = minDif = 0.0f;

  // Define best approximative ellipse
  ok = PcaGetAxis(polylineTest->m_points, polylineTest->m_numPoints, &vEllipseCenter, ellipseRad, ellipseVec);
  if (ok != 0)
    return ok;

  _makeAxisAligned(ellipseVec);

  // Build polyline descriptor, based on best ellipse
  //_setupPolylineFeatureMap(polylineTest, &vEllipseCenter, &ellipseVec[0], &ellipseVec[1]);

  #ifdef METHOD_HOG
    _setupPolylineHog(polylineTest, &vEllipseCenter, &ellipseVec[0], &ellipseVec[1]);
  
    minDif = 100000000;
    for (i = 0; i < 256; i++)
    {
      if (s_numTestSamples[i] == 0)
        continue;
      #ifdef METHOD_HOG
        dif = _getDescriptorsDifByHog(&s_descCharMean[i], &polylineTest->m_desc);
      #endif
      if (dif < minDif)
      {
        minDif    = dif;
        indexBest = i;
      }
    }   // for (I0
    if (minDif <= s_maxErrHog)
    {
      *letterRecognized = (char)indexBest;
      status = 1;
    }
    else
    {
      *letterRecognized = 0;
      status = 0;
    }

    // find best exact letter
    if (status)
    {
      minDif = 1.0e12f;
      indexBest = -1;
      for (i = 0; i < s_letterSet.m_numLetters; i++)
      {
        LetterDesc  *letter;
        PLine       *polylineOrig;

        letter        = &s_letterSet.m_letters[i];
        if (letter->m_letter != *letterRecognized)
          continue;
        polylineOrig  = &letter->m_polyline;
        dif = _getDescriptorsDifByHog(&polylineOrig->m_desc, &polylineTest->m_desc);
        if (dif < minDif)
        {
          minDif = dif;
          indexBest = i;
        }
      }     // for (i)
      *letterIndex = indexBest;
    }       // if (has match)
  #endif


  #ifdef METHOD_DIR_CODE
    _setupPolylineDirCodes(polylineTest, &vEllipseCenter, &ellipseVec[0], &ellipseVec[1]);
    status = 0;

    dif = minDif = 1.0e12f;
    indexBest = -1;
    for (i = 0; i < s_letterSet.m_numLetters; i++)
    {
      LetterDesc  *letter;
      PLine       *polylineOrig;

      letter        = &s_letterSet.m_letters[i];
      polylineOrig  = &letter->m_polyline;

      dif = _getDescriptorsDifByCode(&polylineOrig->m_desc, &polylineTest->m_desc);
      if (dif < minDif)
      {
        minDif    = dif;
        indexBest = i;
      }
    }     // for (i) all letters in train set
    if (minDif < 1)
    {
      status = 1;
      *letterRecognized = s_letterSet.m_letters[indexBest].m_letter;
      *letterIndex = indexBest;
    }
    else
    {
      status = 0;
    }

  #endif

  #ifdef METHOD_TAR
    _setupPolylineTar(polylineTest, &vEllipseCenter, &ellipseVec[0], &ellipseVec[1]);
    dif = minDif = 1.0e12f;
    indexBest = -1;
    for (i = 0; i < s_letterSet.m_numLetters; i++)
    {
      LetterDesc  *letter;
      float       *tar;
      int         n;

      letter        = &s_letterSet.m_letters[i];
      tar           = letter->m_polyline.m_tar;
      n             = letter->m_polyline.m_numPoints;
      if (polylineTest->m_numPoints < n)
        n = polylineTest->m_numPoints;

      dif = _getDescriptorsDifByTar(tar, polylineTest->m_tar, n);
      if (dif < minDif)
      {
        minDif    = dif;
        indexBest = i;
      }
    }
    if (minDif < 0.11f)
    {
      status = 1;
      *letterRecognized = s_letterSet.m_letters[indexBest].m_letter;
      *letterIndex = indexBest;
    }
    else
    {
      status = 0;
    }


  #endif


  #ifdef METHOD_FORCE
    _setupPolylineForce(polylineTest, &vEllipseCenter, &ellipseVec[0], &ellipseVec[1]);
    dif = minDif = 1.0e12f;
    indexBest = -1;
    for (i = 0; i < s_letterSet.m_numLetters; i++)
    {
      LetterDesc  *letter;

      letter = &s_letterSet.m_letters[i];
      dif = _getErrorOfModifyOnto(&letter->m_polyline, polylineTest);
      if (dif < minDif)
      {
        minDif    = dif;
        indexBest = i;
      }
    }
    if (minDif < 0.2f)
    {
      status = 1;
      *letterRecognized = s_letterSet.m_letters[indexBest].m_letter;
      *letterIndex = indexBest;
    }
    else
    {
      status = 0;
    }


  #endif


  #ifdef METHOD_POLAPPROX
  {
    int         iBest;
    LetterDesc  *letter;
    PLine       *polylineOrig;
    int         n;
    float       matrix[3*3], difFeature;

    _buildNormalizedCurve(polylineTest, &vEllipseCenter, &ellipseVec[0], &ellipseVec[1], polylineTest->m_pointsNorm);
    _getContourFeaturePoints(polylineTest->m_pointsNorm, polylineTest->m_numPoints, &polylineTest->m_desc.m_featPoints);

    minDif = 1.0e12f;
    iBest = -1;
    for (i = 0; i < s_letterSet.m_numLetters; i++)
    {

      letter        = &s_letterSet.m_letters[i];
      polylineOrig  = &letter->m_polyline;
      n = polylineTest->m_numPoints;
      if (n < polylineOrig->m_numPoints)
        n = polylineOrig->m_numPoints;

      ok = LinearFindBestTransform(polylineOrig->m_pointsNorm, polylineTest->m_pointsNorm, n, matrix);
      if (ok != 0)
        continue;

      LinearTransformPoints(polylineOrig->m_pointsNorm, n, matrix, s_points);
      //dif = _getPointArrayDif(s_points, polylineTest->m_pointsNorm, n);
      dif = LinearGetTransformAveError(polylineOrig->m_pointsNorm, n, matrix, polylineTest->m_pointsNorm);

      //if (matrix[0] < 0.0f)
      //  dif = 100000000.0f;


      // compare feature points
      //difFeature = _getFeatureDifference(polylineOrig->m_desc.m_featPoints.m_points, polylineTest->m_desc.m_featPoints.m_points);
      //if (difFeature > 25.0f)
      //  dif = 100000.0f;

      if (dif < minDif)
      {
        minDif = dif;
        iBest = i;
      }
    }       // for (i) all points

    if (minDif < 0.09f)
    {
      status = 1;
      *letterRecognized = s_letterSet.m_letters[iBest].m_letter;
      *letterIndex = iBest;
    }
    else
    {
      status = 0;
    }


    if (screenDst)
    {
      letter        = &s_letterSet.m_letters[iBest];
      polylineOrig  = &letter->m_polyline;
      n = polylineTest->m_numPoints;
      if (n < polylineOrig->m_numPoints)
        n = polylineOrig->m_numPoints;
      LinearFindBestTransform(polylineOrig->m_pointsNorm, polylineTest->m_pointsNorm, n, matrix);
      LinearTransformPoints(polylineOrig->m_pointsNorm, n, matrix, s_points);

      _writeTransformedPointsToScreen(screenDst->m_pixels, screenDst->m_width, screenDst->m_height, s_points, n, 0, 50, 0 + 100, 50 + 150, 0xffffeeee);
      _writeFeaturePoints(screenDst->m_pixels, screenDst->m_width, screenDst->m_height, &polylineOrig->m_desc.m_featPoints, 0, 50, 0 + 100, 50 + 150, 0xffffeeee);

      _writeTransformedPointsToScreen(screenDst->m_pixels, screenDst->m_width, screenDst->m_height, polylineTest->m_pointsNorm, n, 0, 50, 0 + 100, 50 + 150, 0xffaaffaa);
      _writeFeaturePoints(screenDst->m_pixels, screenDst->m_width, screenDst->m_height, &polylineTest->m_desc.m_featPoints, 0, 50, 0 + 100, 50 + 150, 0xffaaffaa);
    }

    return status;

    // Draw parametric curve, based on xKoefs, yKoefs
    // Draw polyline s_points[polylineTest->m_numPoints]

  }
  #endif


  // Draw descriptor to output screen buffer
  if (screenDst && screenDst->m_pixels)
  {
    int   w, h, tw, th;
    int   sw, sh;

    w = screenDst->m_width;
    h = screenDst->m_height;
    tw = (int)(w * 0.3f);
    th = (int)(h * 0.2f);
    #ifdef RENDER_DESC_TABLE
    RenderDescriptorTableToScreen( 
                                  &polylineTest->m_desc, 
                                  0, 0, tw, th,
                                  screenDst->m_pixels, w, h
                                 );
    #endif
    #ifdef METHOD_TAR
    RenderTarToScreen(polylineTest->m_tar, polylineTest->m_numPoints, 0, 0, tw, th, screenDst->m_pixels, w, h);
    #endif

    
    sw = (int)(w*0.1f);
    sh = (int)(h*0.25f);
    _writeTransformedPointsToScreen(screenDst->m_pixels, w, h, s_points, polylineTest->m_numPoints, 0, th + 16, 0 + sw, th + 16 + sh, 0xffffeeee );
  }

  return status;
}
