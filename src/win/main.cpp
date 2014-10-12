//  **********************************************************
//  FILE NAME   hand_main_win.cpp
//  PURPOSE     Hand writing recognition
//  NOTES
//  **********************************************************


//  **********************************************************
//  Includes
//  **********************************************************

// System specific includes
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <gdiplus.h>
#include <sys/stat.h>
#include <share.h>
#include <winbase.h>

#include <tchar.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>


// App specific includes
#include "..\universal\raster.h"
#include "..\universal\xml.h"
#include "..\universal\pline.h"

//  **********************************************************
//  Defines
//  **********************************************************

//#define RENDER_DESC_TABLE

// How long time (in ms) polyline is visible on screen
#define POLYLINE_TIME_VISIBLE   3000

#define WINDOW_W        800
#define WINDOW_H        600

#define MAX_RECOGNIZED_STRING_SIZE    120

#define   XML_LETTER_SET_FILE_NAME  "data\\letters.xml"
#define   XML_SAVE_FILE_NAME        "data\\save.xml"

#define   MAIN_TITLE                "Hand writing recognition"
#define   MAIN_CLASS_NAME           "HandWrRecWindowClass"

// Define next line only for deep debuf performance evaluation!
//#define _PERFORMANCE_CHECK_

//  **********************************************************
//  Macros
//  **********************************************************

#ifdef _PERFORMANCE_CHECK_
  #define PERFCHECK_BEGIN(funcName)         \
  {                                         \
    static int s_completed_##funcName = 0;  \
    int         i, timeS, timeE, dt, cnt;   \
    char        str[64];                    \
                                            \
    cnt = (!s_completed_##funcName)?40: 1;  \
    timeS = GetTickCount() & 0xfffffff;     \
    for (i = 0; i < cnt; i++)               \
    {                                     

  #define PERFCHECK_END(funcName)           \
    }                                       \
    if (!s_completed_##funcName)            \
    {                                       \
      timeE = GetTickCount() & 0xfffffff;   \
      dt = (timeE - timeS) / cnt;           \
      sprintf(str, "[%-32s] = %d\n", #funcName, dt);  \
      OutputDebugString(str);               \
    }                                       \
    s_completed_##funcName = 1;             \
  }

#else
  #define PERFCHECK_BEGIN(funcName)
  #define PERFCHECK_END(funcName)
#endif

//  **********************************************************
//  Types
//  **********************************************************

enum AppMode
{
  MODE_NA             =-1,
  MODE_LEARNING       = 0,
  MODE_RECOGNITION    = 1,
};

enum MouseState
{
  MOUSE_NA    = -1,
  MOUSE_START =  0,
  MOUSE_STOP  =  1,
  MOUSE_MOVE  =  2,
};

//  **********************************************************
//  Static data for this module
//  **********************************************************

static  ImageDesc             s_imageScreen;

static PLine                  *s_polyline;

static char                   s_strRecognized[MAX_RECOGNIZED_STRING_SIZE];
static int                    s_strRecognizedLen = 0;
static LetterDesc            *s_letter = NULL;

static char                   s_letterTyped                 = '0';
static int                    s_numTypedLetters             = 0;
static int                    s_numCorrectRecognizedLetters = 0;


//static  AppMode               s_appMode = MODE_LEARNING;
static  AppMode               s_appMode = MODE_RECOGNITION;


// gdi+ stuff
static ULONG_PTR              s_gdiplusToken;



static  HWND                  s_hWnd;
static  BOOL                  s_mousePressedLeft   = FALSE;
static  BOOL                  s_mousePressedRight  = FALSE;

static  V2d                   s_mousePrev;
static  V2d                   s_mouseStart;
static  long                  s_mouseTimeLast;

static  BOOL                  s_active      = FALSE;
static  BOOL                  s_ready       = FALSE;

// Windows rendering buffer
static  HBITMAP               _hbmp;
static  unsigned char         *s_screenPixels;

static  int                   s_startTime;
static  int                   s_spacePressed = 0;

//  **********************************************************
//  Forward refs
//  **********************************************************

//  **********************************************************
//  Functions 
//  **********************************************************

// NAME       
// PURPOSE    
// ARGUMENTS  None
// RETURNS    None
// NOTES      None
//
static  BOOL _mainCreate(HWND hWnd)
{
  HDC            hdc;
  BYTE           bmiBuf[56];
  BITMAPINFO     *bmi;
  DWORD          *bmiCol;

  hdc   = GetDC(hWnd);
  if (!hdc)
    return FALSE;
  bmi   = ( BITMAPINFO * ) bmiBuf ;
  ZeroMemory( bmi , sizeof(BITMAPINFO) ) ;
  bmi->bmiHeader.biSize         = sizeof(BITMAPINFOHEADER);
  bmi->bmiHeader.biWidth        = s_imageScreen.m_width;
  bmi->bmiHeader.biHeight       = -s_imageScreen.m_height;
  bmi->bmiHeader.biPlanes       = 1;
  bmi->bmiHeader.biBitCount     = 4 * 8;
  bmi->bmiHeader.biCompression  = BI_RGB;
  bmi->bmiHeader.biSizeImage    = s_imageScreen.m_width * s_imageScreen.m_height * 4;
  bmiCol                        = ( DWORD * )( bmiBuf + sizeof(BITMAPINFOHEADER) ) ;
  bmiCol[0]                     = 0 ;
  bmiCol[1]                     = 0 ;
  bmiCol[2]                     = 0 ;

  _hbmp  = CreateDIBSection( 
                            hdc,
                            bmi,
                            DIB_RGB_COLORS,
                            (void**)&s_screenPixels,
                            NULL,
                            0
                         );
  ReleaseDC(hWnd, hdc);
  if (!_hbmp)
    return FALSE;

  return TRUE;
}

// NAME       
// PURPOSE    
// ARGUMENTS  None
// RETURNS    None
// NOTES      None
//
static  BOOL  _mainDestroy()
{
  if (_hbmp)
  {
    DeleteObject(_hbmp);
    _hbmp = 0;
  }
  return TRUE;
}


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

static char _getRandomLetter()
{
  int let;

  let = '0' + 10 * (rand() & 0xff) / 255;
  if (let > '9')
    let = '9';
  return (char)let;
}


static int    s_timePolylineCompleted = -1;
static void _updateMouse(const V2d *pos, const MouseState state)
{
  switch(state)
  {
    case MOUSE_START:
    {
      s_polyline->m_points[0] = *pos;
      s_polyline->m_numPoints = 1;
      s_letter                = NULL;
      break;
    }
    case MOUSE_MOVE:
    {
      if (s_polyline->m_numPoints < MAX_POINTS_IN_POLYLINE)
      {
        s_polyline->m_points[s_polyline->m_numPoints ++] = *pos;
      }
      break;
    }
    case MOUSE_STOP:
    {
      s_timePolylineCompleted = (int)(GetTickCount() & 0x7fffffff);

      #ifdef METHOD_HOG
        PLineMakeRoughByNumSeg(s_polyline, NUM_POINTS_ROUGH_POLYLINE);
      #endif

      #ifdef METHOD_DIR_CODE
        PLineMakeRoughByCurvature(s_polyline, 40);
        PLineMakeRoughRemovingClosePoints(s_polyline, 0.1f);
      #endif

      #ifdef METHOD_TAR
        PLineMakeRoughByNumSeg(s_polyline, NUM_POINTS_ROUGH_POLYLINE);
      #endif
      #ifdef METHOD_FORCE
        PLineMakeRoughByNumSeg(s_polyline, NUM_POINTS_ROUGH_POLYLINE);
      #endif
      #ifdef METHOD_POLAPPROX
        PLineMakeRoughByNumSeg(s_polyline, NUM_POINTS_ROUGH_POLYLINE);
      #endif




      /*
      // !!!!!!!!!!!! TEST !!!!!!!!!!!!!
      {
        int   i; 
        float a, c, s, rx, ry;
        for (i = 0; i < (64); i++)
        {
          a = 3.1415926f * 2.0f * i / 64;
          c = cosf(a);
          s = sinf(a);
          rx = 100.0f -  70.0f*i/64;
          ry = 160.0f -  80.0f*i/64;
          rx = 100.0f;
          ry = 160.0f;
          s_polyline->m_points[i].x = (int)(400 - rx * s);
          s_polyline->m_points[i].y = (int)(300 - ry * c);
        }
        s_polyline->m_numPoints = 64;
      }
      */


      if (s_appMode == MODE_LEARNING)
      {
        XmlSavePolyline(s_letterTyped, s_polyline, XML_SAVE_FILE_NAME);
      }
      if ( (s_appMode == MODE_RECOGNITION) && (s_polyline->m_numPoints > 3) )
      {
        int   status, indexLetter;
        char  letter;

        status = PLineProcess(s_polyline, &letter, &indexLetter, &s_imageScreen);

        // Ratio account
        s_numTypedLetters ++;
        if (status)
        {
          if (letter == s_letterTyped)
            s_numCorrectRecognizedLetters++;
        }
        s_letterTyped = _getRandomLetter();


        if (status && (s_strRecognizedLen < MAX_RECOGNIZED_STRING_SIZE))
        {
          s_strRecognized[s_strRecognizedLen + 0] = letter;
          s_strRecognized[s_strRecognizedLen + 1] = 0;
          s_strRecognizedLen++;
          //s_letter =  PLineGetLetterFromLearningSet(indexLetter);

          // !!!!!!!!!
          //if (letter == '1')
          //  status = PLineProcess(s_polyline, &letter, &indexLetter, &s_imageScreen);
          if (letter == '5')
            status = PLineProcess(s_polyline, &letter, &indexLetter, &s_imageScreen);
        }
        if ( (status != 1) && (s_strRecognizedLen < MAX_RECOGNIZED_STRING_SIZE))
        {
          // !!!!!!!!!!!!!
          //status = PLineProcess(s_polyline, &letter, &indexLetter, &s_imageScreen);


          s_strRecognized[s_strRecognizedLen + 0] = '?';
          s_strRecognized[s_strRecognizedLen + 1] = 0;
          s_strRecognizedLen++;

          s_letter = NULL;
        }
      }

      break;
    }
  }   // switch
}


// NAME       
// PURPOSE    
// ARGUMENTS  None
// RETURNS    None
// NOTES      None
//
static  BOOL  _mainUpdateFrame(HWND hWnd)
{
  HDC             hdc;
  HDC             memDC;
  HBITMAP         hOldBitmap;
  int             curTime, dt, alpha, i, valDraw;

  curTime = (int)(GetTickCount() & 0x7fffffff);

  // clear screen
  memset(s_imageScreen.m_pixels, 0, s_imageScreen.m_width * s_imageScreen.m_height * sizeof(int) );

  // detect alpha (fading effect)
  alpha = 255;
  if (s_timePolylineCompleted != -1)
  {
    dt = curTime - s_timePolylineCompleted;
    if (dt < POLYLINE_TIME_VISIBLE)
    {
      alpha = 255 - 255 * dt / POLYLINE_TIME_VISIBLE;
    }   // if (polyline is visible)
    else
      alpha = 0;
  }     // if have at least 1 completed polyline
  // if pressed left mouse button: draw line anyway
  if (s_mousePressedLeft)
    alpha = 255;

  // draw debug descriptor
  #ifdef RENDER_DESC_TABLE
  if (s_timePolylineCompleted != -1)
  {
    int tw, th;
    tw = (int)(s_imageScreen.m_width  * 0.3f);
    th = (int)(s_imageScreen.m_height * 0.3f);
    RenderDescriptorTableToScreen( 
                                  &s_polyline->m_desc, 
                                  0, 0, tw, th,
                                  s_imageScreen.m_pixels, s_imageScreen.m_width, s_imageScreen.m_height
                                 );
  }
  #endif

  // draw if we have some polyline
  if ( (s_polyline->m_numPoints > 2) && (alpha != 0) )
  {
    valDraw = 0xff000000 | (alpha << 16) | (alpha << 8) | alpha;

    // write just drawed contour (to be recognized)
    for (i = 0; i < s_polyline->m_numPoints - 1; i++)
    {
      V2d vs, ve;
      vs = s_polyline->m_points[i];
      ve = s_polyline->m_points[i + 1];
      _writeLine(s_imageScreen.m_pixels, s_imageScreen.m_width, s_imageScreen.m_height, vs.x, vs.y, ve.x, ve.y, valDraw);
    }

    // write recognized leter match
    if ( (s_appMode == MODE_RECOGNITION) && (s_letter != NULL) )
    {
      for (i = 0; i < s_letter->m_polyline.m_numPoints - 1; i++)
      {
        V2d vs, ve;
        vs = s_letter->m_polyline.m_points[i];
        ve = s_letter->m_polyline.m_points[i + 1];
        _writeLine(s_imageScreen.m_pixels, s_imageScreen.m_width, s_imageScreen.m_height, vs.x, vs.y, ve.x, ve.y, valDraw);
      }
    }

    // write tar

  }   // if we have ready polyline


  if ( (s_appMode == MODE_RECOGNITION) && (s_timePolylineCompleted != -1) && (s_polyline->m_numPoints > 16) && (!s_mousePressedLeft))
  {
    int   status, indexLetter;
    char  letter;

    status = PLineProcess(s_polyline, &letter, &indexLetter, &s_imageScreen);
  }


  /*
  if (s_spacePressed)
  {
    //memcpy(s_imageScreen.m_pixels, s_imageTexture[s_indexCur].m_pixels, s_imageScreen.m_width * s_imageScreen.m_height * sizeof(int) );
  }
  else
  {
  }
  */
  
  // Put to screen
  hdc      = GetDC(hWnd);
  if (!hdc)
    return FALSE;

  memDC    = CreateCompatibleDC(hdc);
  if (!memDC)
    return FALSE;
  hOldBitmap  = (HBITMAP)SelectObject(memDC, _hbmp);

  TCHAR * strMessageDraw = "Press left mouse key and draw any digit in range [0..9]";
  SetBkColor(memDC, RGB(0, 0, 0) );
  SetTextColor(memDC, RGB(255, 255, 255) );
  if (curTime - s_startTime < 4000)
  {
    alpha = 255 - 255 * (curTime - s_startTime) / 4000;
    SetTextColor(memDC, RGB(alpha, alpha, alpha) );
    TextOut(memDC, 0, 0, strMessageDraw, strlen(strMessageDraw) );
  }

  SetTextColor(memDC, RGB(255, 255, 255) );
  TextOut(memDC, 0, 16, s_strRecognized, strlen(s_strRecognized) );

  if (s_numTypedLetters > 4)
  {
    float ratio;
    TCHAR   strRatio[48];

    ratio = 100.0f * s_numCorrectRecognizedLetters / s_numTypedLetters;
    sprintf(strRatio, "Correctly recognized %%%5.2f", ratio);
    TextOut(memDC, 0, s_imageScreen.m_height - 20, strRatio, strlen(strRatio) );
  }


  SetTextColor(memDC, RGB(255, 255, 255) );
  TextOut(memDC, 0, 16, s_strRecognized, strlen(s_strRecognized) );


  BitBlt( 
          hdc,
          0,                      // XDest
          0,                      // YDest
          s_imageScreen.m_width,  // Width
          s_imageScreen.m_height, // Height
          memDC,                  // DC
          0,                      // XSrc
          0,                      // YSrc
          SRCCOPY                 // OpCode
       );


  SelectObject(  memDC , hOldBitmap);
  DeleteDC(      memDC);
  ReleaseDC(     hWnd, hdc);

  return TRUE;
}

// NAME       
// PURPOSE    
// ARGUMENTS  None
// RETURNS    None
// NOTES      None
//
static   long  s_lastUpdateFrameTime  = -1;
static   long  s_lastRenderNumber     = 0;
static   long  s_renderNumber         = 0;

static  BOOL _mainShowFrameRate(HWND hWnd)
{
  char    str[64];
  long    curTime;

   curTime = GetTickCount() & 0x7fffffff;
   if (s_lastUpdateFrameTime < 0)
     s_lastUpdateFrameTime = curTime;
   if (curTime - s_lastUpdateFrameTime > 500 )
   {
     sprintf_s(
                str , 64,
                "FPS=%8.2f | Please draw letter \'%c\'  ", 
                (float)(s_renderNumber - s_lastRenderNumber) * 1000.0 / 
                (float)(curTime - s_lastUpdateFrameTime),
                s_letterTyped
               );
      SetWindowText(hWnd , str);
      s_lastUpdateFrameTime = curTime;
      s_lastRenderNumber    = s_renderNumber;
   }
   s_renderNumber++;

  return(TRUE);
}

static int _getEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
   UINT  num = 0;          // number of image encoders
   UINT  size = 0;         // size of the image encoder array in bytes

   Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

   Gdiplus::GetImageEncodersSize(&num, &size);
   if(size == 0)
      return -1;  // Failure

   pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
   if(pImageCodecInfo == NULL)
      return -1;  // Failure

   GetImageEncoders(num, size, pImageCodecInfo);

   for(UINT j = 0; j < num; ++j)
   {
      if( wcscmp(pImageCodecInfo[j].MimeType, format) == 0 )
      {
         *pClsid = pImageCodecInfo[j].Clsid;
         free(pImageCodecInfo);
         return j;  // Success
      }    
   }

   free(pImageCodecInfo);
   return -1;  // Failure
}


unsigned int *readBitmap(char *fileName, int *imgOutW, int *imgOutH)
{
  WCHAR                 uFileName[64];
  Gdiplus::Bitmap      *bitmap;
  int                   i, j;
  int                   imageWidth, imageHeight;
  unsigned int          *pixels;
  unsigned int          *src, *dst;


  // File name to uniclode
  for (i = 0; fileName[i]; i++)
    uFileName[i] = (WCHAR)fileName[i];
  uFileName[i] = 0;

  bitmap = Gdiplus::Bitmap::FromFile(uFileName);
  if (bitmap == NULL)
    return NULL;

  *imgOutW = imageWidth   = bitmap->GetWidth();
  *imgOutH = imageHeight  = bitmap->GetHeight();
  if (imageWidth == 0)
    return NULL;

  pixels = new unsigned int[imageWidth * imageHeight];
  if (pixels == NULL)
    return NULL;

  Gdiplus::Rect rect(0, 0, imageWidth, imageHeight);
  Gdiplus::BitmapData* bitmapData = new Gdiplus::BitmapData;
  Gdiplus::Status status = bitmap->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, bitmapData);
  if (status != Gdiplus::Ok)
    return NULL;

  src = (unsigned int *)bitmapData->Scan0;
  dst = (unsigned int *)pixels;

  for (j = 0; j < imageHeight; j++)
  {
    // Read in inverse lines order
    //src = (unsigned int *)bitmapData->Scan0 + (imageHeight - 1 - j) * imageWidth;

    // Read in normal lines order
    src = (unsigned int *)bitmapData->Scan0 + j * imageWidth;

    for (i = 0; i < imageWidth; i++)
    {
      *dst = *src;
      src++; dst++;
    } // for all pixels
  }

  bitmap->UnlockBits(bitmapData);
  delete bitmapData;
  delete bitmap;

  return pixels;
}
static int saveBitmap(const char *fileName, const unsigned char *bitmapPixels , int bitmapW, int bitmapH)
{
  WCHAR             uFileName[64];
  int               x, y, i;
  unsigned int      *src, color;

  for (i = 0; fileName[i]; i++)
    uFileName[i] = (WCHAR)fileName[i];
  uFileName[i] = 0;

  Gdiplus::Bitmap *image  = new Gdiplus::Bitmap(bitmapW, bitmapH);
  for (y = 0; y < bitmapH; y++)
  {
    // save in inverse Y order
    //src = (unsigned int*)bitmapPixels + (bitmapH - 1 - y)* bitmapW;

    // save in normal order
    src = (unsigned int*)bitmapPixels + y * bitmapW;

    for (x = 0; x < bitmapW; x++, src++)
    {
      color = *src;
      image->SetPixel(x, y, color);
    } // for all x
  } // for all y


  CLSID clsid;
	//_getEncoderClsid(L"image/jpeg", &clsid);
  _getEncoderClsid(L"image/png", &clsid);
  image->Save(uFileName, &clsid);

  delete image;


  return 0;
}




// NAME       
// PURPOSE    
// ARGUMENTS  None
// RETURNS    None
// NOTES      None
//
LRESULT CALLBACK  MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    MINMAXINFO      *pMinMax;
    V2d             pos;

    switch (msg)
    {
        case WM_ACTIVATEAPP:
            // Pause if minimized or not the top window
        	s_active = (wParam == WA_ACTIVE) || (wParam == WA_CLICKACTIVE);
            return 0L;
        case WM_RBUTTONDOWN:
        {
            s_mousePressedRight = TRUE;
            s_mousePrev.x = LOWORD(lParam);
            s_mousePrev.y = HIWORD(lParam);
            s_mouseTimeLast = GetTickCount();

          SetCapture(hWnd);
          break;
        }
        case WM_RBUTTONUP:
        {
            s_mousePressedRight = FALSE;
            ReleaseCapture();
            break;
        }
        case WM_LBUTTONDOWN:
        {
            s_mousePressedLeft = TRUE;
            pos.x = LOWORD(lParam);
            pos.y = HIWORD(lParam);
            s_mouseTimeLast = GetTickCount();
            SetCapture(hWnd);

            _updateMouse(&pos, MOUSE_START);
            break;
        }
        case WM_LBUTTONUP:
        {
            pos.x = LOWORD(lParam);
            pos.y = HIWORD(lParam);
            s_mousePressedLeft = FALSE;
            ReleaseCapture();
            _updateMouse(&pos, MOUSE_STOP);
            break;
        }

        case WM_MOUSEMOVE:
        {
            if (s_mousePressedLeft)
            {
                pos.x = LOWORD(lParam);
                pos.y = HIWORD(lParam);
                _updateMouse(&pos, MOUSE_MOVE);
            }

            if (s_mousePressedRight)
            {
            }     // if (right mouse)

            break;
        }
        case WM_KEYDOWN:
            if ( wParam == VK_ESCAPE )
            {
                PostMessage(hWnd, WM_CLOSE, 0, 0);
                return 0L;
            }
            else if (wParam == 'S')
            {
                saveBitmap("dump.png", (const unsigned char*)s_imageScreen.m_pixels, s_imageScreen.m_width, s_imageScreen.m_height);
            }
            else if (wParam == VK_SPACE)
            {
                s_spacePressed = 1 - s_spacePressed;
            }
            else if ( (wParam >= '0') && (wParam <= 'z'))
            {
                s_letterTyped = (char)wParam;
            }
            break;
        case WM_DESTROY:
            // Clean up and close the app
            PostQuitMessage(0);
            return 0L;

        case WM_GETMINMAXINFO:
            // Fix the size of the window to client size
            pMinMax = (MINMAXINFO *)lParam;
            pMinMax->ptMinTrackSize.x = s_imageScreen.m_width  + GetSystemMetrics(SM_CXSIZEFRAME)*2;
            pMinMax->ptMinTrackSize.y = s_imageScreen.m_height + GetSystemMetrics(SM_CYSIZEFRAME)*2
                                           +GetSystemMetrics(SM_CYMENU);
            pMinMax->ptMaxTrackSize.x = pMinMax->ptMinTrackSize.x;
            pMinMax->ptMaxTrackSize.y = pMinMax->ptMinTrackSize.y;
            break;

        case WM_SIZE:
            // Check to see if we are losing our window...
            if ( (SIZE_MAXHIDE == wParam) || (SIZE_MINIMIZED == wParam) )
                s_active = FALSE;
            else
                s_active = TRUE;
            break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}


static void _testSaveLetterSetTar(LetterSet *letterSet)
{
  int   *image, w, h, i;
  LetterDesc  *desc;

  w = 640;
  h = 480;
  image = (int*)malloc( w * h * sizeof(int) );
  for (i = w * h-1; i >= 0; i--)
    image[i] = 0xff000000;

  for (i = 0; i < letterSet->m_numLetters; i++)
  {
/*    desc = PLineGetLetterFromLearningSet(i);
    if (desc->m_letter != '6')
      continue;
    RenderTarToScreen(desc->m_polyline.m_tar, desc->m_polyline.m_numPoints, 0, 0, w, h, image, w, h);*/
  }
  saveBitmap("tar.png", (const unsigned char*)image, w, h);
  free(image);
}
// NAME       
// PURPOSE    
// ARGUMENTS  None
// RETURNS    None
// NOTES      None
//
int PASCAL  WinMain(
                      HINSTANCE   hInstance,
                      HINSTANCE   hPrevInstance,
                      LPSTR       cpCmdLine,
                      int         nCmdShow
                   )
{
  WNDCLASS	      wc;
  MSG			        msg;
  int             cx, cy;
  static char     fileName[96];

  // Init GDI+
  Gdiplus::GdiplusStartupInput gdiplusStartupInput;
  Gdiplus::GdiplusStartup(&s_gdiplusToken, &gdiplusStartupInput, NULL);

  cpCmdLine = cpCmdLine;
  if (!hPrevInstance)
  {
    // Register the Window Class
    wc.lpszClassName = MAIN_CLASS_NAME;
    wc.lpfnWndProc = MainWndProc;
    wc.style = CS_VREDRAW | CS_HREDRAW;
    wc.hInstance = hInstance;
    wc.hIcon = NULL; //LoadIcon( hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL ; // MAKEINTRESOURCE(IDR_MENU);
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    RegisterClass(&wc);
  }

  s_polyline = (PLine*)malloc( sizeof(PLine) );

  LetterSet    *letterSet;


  letterSet = (LetterSet*)malloc(sizeof(LetterSet));
  letterSet->m_numLetters = 0;
  if (s_appMode == MODE_RECOGNITION)
  {
    cx = XmlLoadLetterSet(XML_LETTER_SET_FILE_NAME, letterSet);
    if (cx < 0)
      return cx;
  }
  PLineCreate(letterSet);

  cx = 4;
  if (!cx)
    _testSaveLetterSetTar(letterSet);

  free(letterSet);


  s_imageScreen.m_width   = WINDOW_W;
  s_imageScreen.m_height  = WINDOW_H;

  // Calculate the proper size for the window given a client 
  cx = s_imageScreen.m_width  + GetSystemMetrics(SM_CXSIZEFRAME)*2;
  cy = s_imageScreen.m_height + GetSystemMetrics(SM_CYSIZEFRAME)*2+GetSystemMetrics(SM_CYMENU);
  // Create and Show the Main Window
  s_hWnd = CreateWindowEx(0,
                          MAIN_CLASS_NAME,
                          MAIN_TITLE,
                          WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT,
                          CW_USEDEFAULT,
  	                      cx,
                          cy,
                          NULL,
                          NULL,
                          hInstance,
                          NULL);
  if (s_hWnd == NULL)
    return FALSE;
  ShowWindow(s_hWnd, nCmdShow);
  UpdateWindow(s_hWnd);

  if (!_mainCreate(s_hWnd))
  {
    return FALSE;
  }

  s_imageScreen.m_pixels  = (int*)s_screenPixels;

  s_ready = TRUE;

  s_startTime = (int)(GetTickCount() & 0x7fffffff);

  //--------------------------------------------------------------
  //           The Message Loop
  //--------------------------------------------------------------
  msg.wParam = 0;
  while (s_ready)
  {
    if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
    {
      // if QUIT message received quit app
      if (!GetMessage(&msg, NULL, 0, 0 ))
        break;
      // Translate and dispatch the message
      TranslateMessage(&msg); 
      DispatchMessage(&msg);
    }
    else
    if (s_active && s_ready)
    {
      // Update the background and flip every time the timer ticks
      _mainUpdateFrame(s_hWnd);
      _mainShowFrameRate(s_hWnd);
    } // Update app
  } // while ( TRUE )

  _mainDestroy();
  PLineDestroy();
  free(s_polyline);


  return msg.wParam;
}
