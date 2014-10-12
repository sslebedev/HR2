//  **********************************************************
//  FILE NAME   xml.h
//  PURPOSE     Data and Xml read/write
//  NOTES
//  **********************************************************

#ifndef _XML_H_
#define _XML_H_

//  **********************************************************
//  Includes
//  **********************************************************

#include "utl.h"
#include "pline.h"

//  **********************************************************
//  Defines
//  **********************************************************

//  **********************************************************
//  Types
//  **********************************************************


//  **********************************************************
//  Functions prototypes
//  **********************************************************

#ifdef    __cplusplus
extern "C"
{
#endif    /* __cplusplus */

int   XmlSavePolyline(const char letter, const PLine *polyline, const char *xmlFileName);
int   XmlLoadLetterSet(const char *xmlFileName, LetterSet *set);

#ifdef    __cplusplus
}
#endif  /* __cplusplus */

#endif  


  