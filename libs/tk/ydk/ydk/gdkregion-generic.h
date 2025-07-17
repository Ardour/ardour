/* $TOG: region.h /main/9 1998/02/06 17:50:30 kaleb $ */
/************************************************************************

Copyright 1987, 1998  The Open Group

All Rights Reserved.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

************************************************************************/

#ifndef __GDK_REGION_GENERIC_H__
#define __GDK_REGION_GENERIC_H__

typedef GdkSegment GdkRegionBox;

/* 
 *   clip region
 */

struct _GdkRegion
{
  long size;
  long numRects;
  GdkRegionBox *rects;
  GdkRegionBox extents;
};

/*  1 if two BOXs overlap.
 *  0 if two BOXs do not overlap.
 *  Remember, x2 and y2 are not in the region 
 */
#define EXTENTCHECK(r1, r2) \
	((r1)->x2 > (r2)->x1 && \
	 (r1)->x1 < (r2)->x2 && \
	 (r1)->y2 > (r2)->y1 && \
	 (r1)->y1 < (r2)->y2)

/*
 *  update region extents
 */
#define EXTENTS(r,idRect){\
            if((r)->x1 < (idRect)->extents.x1)\
              (idRect)->extents.x1 = (r)->x1;\
            if((r)->y1 < (idRect)->extents.y1)\
              (idRect)->extents.y1 = (r)->y1;\
            if((r)->x2 > (idRect)->extents.x2)\
              (idRect)->extents.x2 = (r)->x2;\
            if((r)->y2 > (idRect)->extents.y2)\
              (idRect)->extents.y2 = (r)->y2;\
        }

#define GROWREGION(reg, nRects) {  					   \
	  if ((nRects) == 0) {						   \
            if ((reg)->rects != &(reg)->extents) {			   \
	      g_free ((reg)->rects);					   \
              (reg)->rects = &(reg)->extents;				   \
	    }								   \
	  }  								   \
	  else if ((reg)->rects == &(reg)->extents) {                      \
            (reg)->rects = g_new (GdkRegionBox, (nRects));		   \
            (reg)->rects[0] = (reg)->extents;                              \
          }                                                                \
          else                                                             \
            (reg)->rects = g_renew (GdkRegionBox, (reg)->rects, (nRects)); \
	  (reg)->size = (nRects);                                          \
       }				 

/*
 *   Check to see if there is enough memory in the present region.
 */
#define MEMCHECK(reg, rect, firstrect){					  	 \
        if ((reg)->numRects >= ((reg)->size - 1)) {			 	 \
          GROWREGION(reg,2*(reg)->size);                                         \
          (rect) = &(firstrect)[(reg)->numRects];				 \
         }									 \
       }

/*  this routine checks to see if the previous rectangle is the same
 *  or subsumes the new rectangle to add.
 */

#define CHECK_PREVIOUS(Reg, R, Rx1, Ry1, Rx2, Ry2)\
               (!(((Reg)->numRects > 0)&&\
                  ((R-1)->y1 == (Ry1)) &&\
                  ((R-1)->y2 == (Ry2)) &&\
                  ((R-1)->x1 <= (Rx1)) &&\
                  ((R-1)->x2 >= (Rx2))))

/*  add a rectangle to the given Region */
#define ADDRECT(reg, r, rx1, ry1, rx2, ry2){\
    if (((rx1) < (rx2)) && ((ry1) < (ry2)) &&\
        CHECK_PREVIOUS((reg), (r), (rx1), (ry1), (rx2), (ry2))){\
              (r)->x1 = (rx1);\
              (r)->y1 = (ry1);\
              (r)->x2 = (rx2);\
              (r)->y2 = (ry2);\
              EXTENTS((r), (reg));\
              (reg)->numRects++;\
              (r)++;\
            }\
        }



/*  add a rectangle to the given Region */
#define ADDRECTNOX(reg, r, rx1, ry1, rx2, ry2){\
            if ((rx1 < rx2) && (ry1 < ry2) &&\
                CHECK_PREVIOUS((reg), (r), (rx1), (ry1), (rx2), (ry2))){\
              (r)->x1 = (rx1);\
              (r)->y1 = (ry1);\
              (r)->x2 = (rx2);\
              (r)->y2 = (ry2);\
              (reg)->numRects++;\
              (r)++;\
            }\
        }

#define EMPTY_REGION(pReg) pReg->numRects = 0

#define REGION_NOT_EMPTY(pReg) pReg->numRects

#define INBOX(r, x, y) \
      ( ( ((r).x2 >  x)) && \
        ( ((r).x1 <= x)) && \
        ( ((r).y2 >  y)) && \
        ( ((r).y1 <= y)) )

/*
 * number of points to buffer before sending them off
 * to scanlines() :  Must be an even number
 */
#define NUMPTSTOBUFFER 200

/*
 * used to allocate buffers for points and link
 * the buffers together
 */
typedef struct _POINTBLOCK {
  GdkPoint pts[NUMPTSTOBUFFER];
  struct _POINTBLOCK *next;
} POINTBLOCK;

#endif /* __GDK_REGION_GENERIC_H__ */
