//  **********************************************************
//  FILE NAME   xml.c
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

#include "xml.h"

//  **********************************************************
//  Defines
//  **********************************************************

#define IABS(a)                   (((a)>0)?(a):-(a))
#define FABS(a)                   (((a)>0.0f)?(a):-(a))
#define ISWAP(aa,bb)              { int ttt; ttt = (aa); (aa)=(bb); (bb)=ttt; }

//  **********************************************************
//  Vars
//  **********************************************************

static char   s_strBuf[1024 * 8];

//  **********************************************************
//  Func
//  **********************************************************

int   XmlSavePolyline(const char letter, const PLine *polyline, const char *xmlFileName)
{
  FILE          *file;
  int           i;
  char          str[32];

  // build string with points
  s_strBuf[0] = 0;
  for (i = 0; i < polyline->m_numPoints; i++)
  {
    if (i != polyline->m_numPoints - 1)
      sprintf(str, "%d, %d, ", polyline->m_points[i].x, polyline->m_points[i].y);
    else
      sprintf(str, "%d, %d", polyline->m_points[i].x, polyline->m_points[i].y);
    if (strlen(s_strBuf) + 32 < sizeof(s_strBuf) )
      strcat(s_strBuf, str);
  }

  // write file
  file = fopen(xmlFileName,"a+b");
  if (file == NULL)
    return -1;

  fprintf(file, "  <letter>\n");

  fprintf(file, "    <code>%d</code>\n", (int)letter);
  fprintf(file, "    <points>%s</points>\n", s_strBuf);

  fprintf(file, "  </letter>\n");
  fclose(file);
  return 0;
}

static int _xmlGetNumTags(const char *xmlString, const char *tagName)
{
  int         numTags;
  const char  *src;
  char        *strTag;
  int         tagLen;

  tagLen = strlen(tagName);
  numTags = 0;
  src = xmlString;
  while (src)
  {
    strTag = strstr(src, tagName);
    if (strTag == NULL)
      break;
    if (strTag[-1] != '<')
    {
      src = strTag + 1;
      continue;
    }
    if (strTag[tagLen] != '>')
    {
      src = strTag + tagLen + 1;
      continue;
    }
    numTags++;
    src = strTag + tagLen + 1;
  }     // while

  return numTags;
}

static int _xmlGetString(const char *xmlString, const char *tagName, const int tagIndex, char *strOut, const int outBufLen)
{
  const char  *src, *srcStart, *srcEnd;
  int         tagLen;
  char        *dst;
  int         index;

  tagLen = strlen(tagName);
  index = 0;
  for (src = xmlString + 1; src[0] != 0; src++)
  {
    if (src[0] != '<')
      continue;
    src++;

    if (strncmp(src, tagName, tagLen) != 0)
      continue;
    src += tagLen;

    if (src[0] != '>')
      continue;
    src++;

    if (index == tagIndex)
      break;
    index++;
  }
  if (src[0] == 0)
    return -1;

  srcStart = srcEnd = src;

  for (; src[0] != 0; src++)
  {
    srcEnd = src;
    if (src[0] != '<')
      continue;
    src++;

    if (src[0] != '/')
      continue;
    src++;

    if (strncmp(src, tagName, tagLen) != 0)
      continue;
    src += tagLen;

    if (src[0] != '>')
      continue;
    src++;
    break;
  }
  if (src[0] == 0)
    return -2;    // TAG_NPT_CLOSED
  if (srcEnd - srcStart >= outBufLen)
    return -3;    // XML_TOO_SMALL_STRING_BUFFER;

  dst = strOut;
  for (src = srcStart; src != srcEnd; src++)
  {
    *dst = *src;
    dst++;
  }
  *dst = 0;

  return 0;
}

 
int   XmlLoadLetterSet(const char *xmlFileName, LetterSet *set)
{
  FILE        *file;
  int         fsize, i, n, len, k, numCommas, m;
  char        *fileBuf;
  LetterDesc  *letter;
  char        str[32], *dst, sym;
  int         indexDig, indexPoint, indexCoord;

  set->m_numLetters = 0;
  file = fopen(xmlFileName, "rb");
  if (file == NULL)
    return -1;

  fseek(file, 0, SEEK_END);
  fsize = (int)ftell(file);
  fseek(file, 0, SEEK_SET);

  fileBuf = (char*) malloc( fsize + 4);
  fread(fileBuf, 1, fsize, file);
  fclose(file);
  fileBuf[fsize] = 0;
  fclose(file);


  set->m_numLetters = _xmlGetNumTags(fileBuf, "letter");
  assert(set->m_numLetters <= MAX_LETTERS);
  
  for (i = 0; i < set->m_numLetters; i++)
  {
    letter = &set->m_letters[i];

    _xmlGetString(fileBuf, "code",  i, s_strBuf, sizeof(s_strBuf) );
    sscanf(s_strBuf, "%d", &n);
    assert( n > 0);
    assert( n < 256);
    letter->m_letter = (char)n;

    _xmlGetString(fileBuf, "points", i, s_strBuf, sizeof(s_strBuf) );
    // how much ',' in string
    len = strlen(s_strBuf);
    numCommas = 0;
    for (k = 0; k < len; k++)
    {
      if (s_strBuf[k] == ',')
        numCommas++;
    }
    letter->m_polyline.m_numPoints = (numCommas + 1) / 2;
    if (letter->m_polyline.m_numPoints > MAX_POINTS_IN_POLYLINE)
    {
      free(fileBuf);
      return -1;      // Too much points in polyline
    }
    // read points
    indexDig = 0;
    for (k = 0; k < len; k++)
    {
      sym = s_strBuf[k];
      if ( (sym == ',') || (sym == ' '))
        continue;
      for (dst = str, m = 0; m < sizeof(str) - 2; m++, dst++)
      {
        sym = s_strBuf[k + m];
        if ( (sym == ',') || (sym == ' ') || (sym == 0) )
          break;
        *dst = sym;
      }
      *dst = 0;
      k = k+ m - 1;
      indexPoint = indexDig >> 1;
      indexCoord = indexDig & 1;
      if (indexCoord == 0)
        letter->m_polyline.m_points[indexPoint].x = atoi(str);
      else
        letter->m_polyline.m_points[indexPoint].y = atoi(str);
      indexDig++;
    }
    assert( indexPoint == letter->m_polyline.m_numPoints - 1);
  }   // for (i) all letters in file

  free(fileBuf);
  return 0;
}