//  **********************************************************
//  FILE NAME   utl.h
//  PURPOSE     arithmetic support
//  NOTES
//  **********************************************************

#ifndef _UTL_H_
#define _UTL_H_

#define USE_PARAM(aaa) { int iii; iii = (int)(aaa); iii++; }

typedef struct tagV2d
{
  int x, y;
} V2d;

typedef struct tagV3d
{
  int x, y, z;
} V3d;

typedef struct tagBox2d
{
  V2d   m_vMin, m_vMax;
  int   m_val;
} Box2d;


typedef struct tagV2f
{
  float x, y;
} V2f;

typedef struct tagV3f
{
  float x, y, z;
} V3f;

typedef struct tagV4f
{
  float x, y, z, w;
} V4f;

typedef struct tagMatrix4x4f
{
  float m_matrix[16];
} Matrix4x4f;

void  UtilV3fTransform(const V3f *v, const Matrix4x4f *matrix, V3f *vOut)
{
  vOut->x = 
            v->x * matrix->m_matrix[ 0] + 
            v->y * matrix->m_matrix[ 4] + 
            v->z * matrix->m_matrix[ 8] + 
                   matrix->m_matrix[12];
  vOut->y = 
            v->x * matrix->m_matrix[ 1] + 
            v->y * matrix->m_matrix[ 5] + 
            v->z * matrix->m_matrix[ 9] + 
                   matrix->m_matrix[13];
  vOut->z = 
            v->x * matrix->m_matrix[ 2] + 
            v->y * matrix->m_matrix[ 6] + 
            v->z * matrix->m_matrix[10] + 
                   matrix->m_matrix[14];
}


void  UtilV3fAdd(const V3f *v0, const V3f *v1, V3f *vOut)
{
  vOut->x = v0->x + v1->x;
  vOut->y = v0->y + v1->y;
  vOut->z = v0->z + v1->z;
}
void  UtilV3fSub(const V3f *v0, const V3f *v1, V3f *vOut)
{
  vOut->x = v0->x - v1->x;
  vOut->y = v0->y - v1->y;
  vOut->z = v0->z - v1->z;
}
void  UtilV3fScale(const V3f *v0, const float s, V3f *vOut)
{
  vOut->x = v0->x * s;
  vOut->y = v0->y * s;
  vOut->z = v0->z * s;
}
float UtilV3fDot(const V3f *v0, const V3f *v1)
{
  return v0->x * v1->x + v0->y * v1->y + v0->z * v1->z;
}
float UtilV3fLength(const V3f *v)
{
  float dot;

  dot = UtilV3fDot(v, v);
  return sqrtf(dot);
}
void  UtilV3fCross(const V3f *v0, const V3f *v1, V3f *vOut)
{
  vOut->x = v0->y * v1->z - v0->z * v1->y;
  vOut->y = v0->z * v1->x - v0->x * v1->z;
  vOut->z = v0->x * v1->y - v0->y * v1->x;
}

void  UtilV3fNormalize(V3f *v)
{
  float len = v->x * v->x + v->y * v->y + v->z * v->z;
  len = sqrtf(len);
  if (len > 0.0f)
  {
    float rLen = 1.0f / len;
    v->x *= rLen;
    v->y *= rLen;
    v->z *= rLen;
  }
}

void  UtilV2fSub(const V2f *v0, const V2f *v1, V2f *vOut)
{
  vOut->x = v0->x - v1->x;
  vOut->y = v0->y - v1->y;
}

void  UtilV2fNormalize(V2f *v)
{
  float len = v->x * v->x + v->y * v->y;
  len = sqrtf(len);
  if (len > 0.0f)
  {
    float rLen = 1.0f / len;
    v->x *= rLen;
    v->y *= rLen;
  }
}
float UtilV2fDotProduct(const V2f *v0, const V2f *v1)
{
  return v0->x * v1->x + v0->y * v1->y;
}

float UtilV2fLength(const V2f *v)
{
  float dot;

  dot = UtilV2fDotProduct(v, v);
  return sqrtf(dot);
}

#endif
