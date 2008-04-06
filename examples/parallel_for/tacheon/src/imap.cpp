/*
    Copyright 2005-2007 Intel Corporation.  All Rights Reserved.

    This file is part of Threading Building Blocks.

    Threading Building Blocks is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    Threading Building Blocks is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Threading Building Blocks; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/

/*
    The original source for this example is
    Copyright (c) 1994, 1995, 1996, 1997  John E. Stone
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. All advertising materials mentioning features or use of this software
       must display the following acknowledgement:
	 This product includes software developed by John E. Stone
    4. The name of the author may not be used to endorse or promote products
       derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
    OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
*/

/*
 * imap.c - This file contains code for doing image map type things.  
 *
 *  $Id: imap.cpp,v 1.2 2007/02/22 17:54:15 dpoulsen Exp $
 */

#include "machine.h"
#include "types.h"
#include "imap.h"
#include "util.h"
#include "imageio.h"

rawimage * imagelist[MAXIMGS];
int numimages;

void ResetImages(void) {
  int i;
  numimages=0;
  for (i=0; i<MAXIMGS; i++) {
    imagelist[i]=NULL;
  }
}

void LoadImage(rawimage * image) {
  if (!image->loaded) {
    readimage(image);
    image->loaded=1;
  }
}

color ImageMap(rawimage * image, flt u, flt v) {
  color col, colx, colx2;
  flt x,y, px, py;
  int x1, x2, y1, y2;
  unsigned char * ptr;
  unsigned char * ptr2;

  if (!image->loaded) {   
    LoadImage(image);
    image->loaded=1;
  }

  if ((u <= 1.0) && (u >=0.0) && (v <= 1.0) && (v >= 0.0)) {
    x=(image->xres - 1.0) * u; /* floating point X location */
    y=(image->yres - 1.0) * v; /* floating point Y location */

    px = x - ((int) x);
    py = y - ((int) y);

    x1 = (int) x;
    x2 = x1 + 1;

    y1 = (int) y;
    y2 = y1 + 1;

    ptr  = image->data + ((image->xres * y1) + x1) * 3; 
    ptr2 = image->data + ((image->xres * y1) + x2) * 3; 

    colx.r = (flt) ((flt)ptr[0] + px*((flt)ptr2[0] - (flt) ptr[0])) / 255.0; 
    colx.g = (flt) ((flt)ptr[1] + px*((flt)ptr2[1] - (flt) ptr[1])) / 255.0; 
    colx.b = (flt) ((flt)ptr[2] + px*((flt)ptr2[2] - (flt) ptr[2])) / 255.0; 

    ptr  = image->data + ((image->xres * y2) + x1) * 3; 
    ptr2 = image->data + ((image->xres * y2) + x2) * 3; 

    colx2.r = ((flt)ptr[0] + px*((flt)ptr2[0] - (flt)ptr[0])) / 255.0; 
    colx2.g = ((flt)ptr[1] + px*((flt)ptr2[1] - (flt)ptr[1])) / 255.0; 
    colx2.b = ((flt)ptr[2] + px*((flt)ptr2[2] - (flt)ptr[2])) / 255.0; 

    col.r = colx.r + py*(colx2.r - colx.r);
    col.g = colx.g + py*(colx2.g - colx.g);
    col.b = colx.b + py*(colx2.b - colx.b);

  }
  else {
    col.r=0.0;
    col.g=0.0;
    col.b=0.0;
  }
  return col;
} 

rawimage * AllocateImage(char * filename) { 
  rawimage * newimage = NULL;
  int i, len, intable;

  intable=0;
  if (numimages!=0) {
    for (i=0; i<numimages; i++) {
      if (!strcmp(filename, imagelist[i]->name)) {
        newimage=imagelist[i];
        intable=1;
      }
    }
  }

  if (!intable) {
    newimage=(rawimage *)rt_getmem(sizeof(rawimage));
    newimage->loaded=0;
    newimage->xres=0;
    newimage->yres=0;
    newimage->bpp=0;
    newimage->data=NULL;
    len=strlen(filename);
    if (len > 80) rtbomb("Filename too long in image map!!"); 
    strcpy(newimage->name, filename);

    imagelist[numimages]=newimage;  /* add new one to the table       */ 
    numimages++;                    /* increment the number of images */
  }
 
  return newimage;
}

void DeallocateImage(rawimage * image) {
  image->loaded=0;
  rt_freemem(image->data);
}


