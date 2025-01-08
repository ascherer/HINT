/* This file is part of HINT
 * Copyright 2017-2021 Martin Ruckert, Hochschule Muenchen, Lothstrasse 64, 80336 Muenchen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
 * OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Except as contained in this notice, the name of the copyright holders shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * copyright holders.
 */
#ifndef WINMAIN_H
#define WINMAIN_H

#define VERSION "Version 2.1.0"

#if 0
extern HDC hDCMain;  
extern HDC hPage; /* The memory device context for the rendered page */
extern HBITMAP hBmpPage; /* the memory bitmap for rendering */

extern int page_t, page_b,page_l,page_r; /* margins of client area in scaled points*/
extern double dpmm, dpmmx, dpmmy; /* dots per mm */

//extern char data_path[MAX_PATH+1];


/* conversion between units and scaled points */
extern int Page_width, Page_height, Page_dpi; /* size and dpi of page scaled for rendering */
#endif

extern HWND hMainWnd;
extern HWND hSearchWnd;
extern HWND hOutlineWnd;
extern HDC hDCMain;
extern char szClassName[];
extern HINSTANCE hInst;
extern int client_width, client_height; /* size of client aerea in pixel */
extern double xdpi, ydpi; /* dots per inch */
extern char *hin_name;
extern uint64_t pos;
extern double scale, dpi_x, dpi_y; /* universal scale factor between TeX sizes and client sizes */
extern int dark;
extern int autoreload;
//define round(X)     ((int)((X)>=0.0?floor((X)+0.5):ceil((X)-0.5)))

#define WM_GAMMA WM_USER

extern double gamma;

/* colors foreground and background for day and night mode */
#define GET_R(C) GetRValue(C)
#define GET_G(C) GetGValue(C)
#define GET_B(C) GetBValue(C)

#define FG_DAY RGB(0x00,0x00,0x00) /* black */
#define BG_DAY RGB(0xFF,0xFF,0xFF) /* white */
#define FG_NIGHT RGB(0xFF,0xFF,0xEE) /* warm white */
#define BG_NIGHT RGB(0x00,0x00,0x00) /* black */

#define BF_ON_DAY RGB(0xD0,0xD0,0xD0) /* lighter grey */
#define BF_OFF_DAY RGB(0xA0,0xA0,0xA0) /* light grey */
#define BF_ON_NIGHT RGB(0x90,0x90, 0x90) /* medium grey */
#define BF_OFF_NIGHT RGB(0x60,0x60, 0x60) /* dark grey */



#endif
//include "error.h"
