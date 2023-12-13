/* $TOG: Region.c /main/31 1998/02/06 17:50:22 kaleb $ */
/************************************************************************

Copyright 1987, 1988, 1998  The Open Group

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


Copyright 1987, 1988 by Digital Equipment Corporation, Maynard, Massachusetts.

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
/* $XFree86: xc/lib/X11/Region.c,v 1.5 1999/05/09 10:50:01 dawes Exp $ */
/*
 * The functions in this file implement the Region abstraction, similar to one
 * used in the X11 sample server. A Region is simply an area, as the name
 * implies, and is implemented as a "y-x-banded" array of rectangles. To
 * explain: Each Region is made up of a certain number of rectangles sorted
 * by y coordinate first, and then by x coordinate.
 *
 * Furthermore, the rectangles are banded such that every rectangle with a
 * given upper-left y coordinate (y1) will have the same lower-right y
 * coordinate (y2) and vice versa. If a rectangle has scanlines in a band, it
 * will span the entire vertical distance of the band. This means that some
 * areas that could be merged into a taller rectangle will be represented as
 * several shorter rectangles to account for shorter rectangles to its left
 * or right but within its "vertical scope".
 *
 * An added constraint on the rectangles is that they must cover as much
 * horizontal area as possible. E.g. no two rectangles in a band are allowed
 * to touch.
 *
 * Whenever possible, bands will be merged together to cover a greater vertical
 * distance (and thus reduce the number of rectangles). Two bands can be merged
 * only if the bottom of one touches the top of the other and they have
 * rectangles in the same places (of the same width, of course). This maintains
 * the y-x-banding that's so nice to have...
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <gdkregion.h>
#include "gdkregion-generic.h"
#include "gdkalias.h"

typedef void (* overlapFunc)    (GdkRegion    *pReg,
                                 GdkRegionBox *r1,
                                 GdkRegionBox *r1End,
                                 GdkRegionBox *r2,
                                 GdkRegionBox *r2End,
                                 gint          y1,
                                 gint          y2);
typedef void (* nonOverlapFunc) (GdkRegion    *pReg,
                                 GdkRegionBox *r,
                                 GdkRegionBox *rEnd,
                                 gint          y1,
                                 gint          y2);

static void miRegionCopy (GdkRegion       *dstrgn,
			  const GdkRegion *rgn);
static void miRegionOp   (GdkRegion       *newReg,
			  GdkRegion       *reg1,
			  const GdkRegion *reg2,
			  overlapFunc      overlapFn,
			  nonOverlapFunc   nonOverlap1Fn,
			  nonOverlapFunc   nonOverlap2Fn);
static void miSetExtents (GdkRegion       *pReg);

/**
 * gdk_region_new:
 *
 * Creates a new empty #GdkRegion.
 *
 * Returns: a new empty #GdkRegion
 */
GdkRegion *
gdk_region_new (void)
{
  GdkRegion *temp;

  temp = g_slice_new (GdkRegion);

  temp->numRects = 0;
  temp->rects = &temp->extents;
  temp->extents.x1 = 0;
  temp->extents.y1 = 0;
  temp->extents.x2 = 0;
  temp->extents.y2 = 0;
  temp->size = 1;
  
  return temp;
}

GdkRegion *
_gdk_region_new_from_yxbanded_rects (GdkRectangle *rects,
				     int num_rects)
{
  GdkRegion *temp;
  int i;

  temp = g_slice_new (GdkRegion);

  temp->rects = g_new (GdkRegionBox, num_rects);
  temp->size = num_rects;
  temp->numRects = num_rects;
  for (i = 0; i < num_rects; i++)
    {
      temp->rects[i].x1 = rects[i].x;
      temp->rects[i].y1 = rects[i].y;
      temp->rects[i].x2 = rects[i].x + rects[i].width;
      temp->rects[i].y2 = rects[i].y + rects[i].height;
    }
  miSetExtents (temp);  
  
  return temp;
}


/**
 * gdk_region_rectangle:
 * @rectangle: a #GdkRectangle
 * 
 * Creates a new region containing the area @rectangle.
 * 
 * Return value: a new region
 **/
GdkRegion *
gdk_region_rectangle (const GdkRectangle *rectangle)
{
  GdkRegion *temp;

  g_return_val_if_fail (rectangle != NULL, NULL);

  if (rectangle->width <= 0 || rectangle->height <= 0)
    return gdk_region_new();

  temp = g_slice_new (GdkRegion);

  temp->numRects = 1;
  temp->rects = &temp->extents;
  temp->extents.x1 = rectangle->x;
  temp->extents.y1 = rectangle->y;
  temp->extents.x2 = rectangle->x + rectangle->width;
  temp->extents.y2 = rectangle->y + rectangle->height;
  temp->size = 1;
  
  return temp;
}

/**
 * gdk_region_copy:
 * @region: a #GdkRegion
 * 
 * Copies @region, creating an identical new region.
 * 
 * Return value: a new region identical to @region
 **/
GdkRegion *
gdk_region_copy (const GdkRegion *region)
{
  GdkRegion *temp;

  g_return_val_if_fail (region != NULL, NULL);

  temp = gdk_region_new ();

  miRegionCopy (temp, region);

  return temp;
}

/**
 * gdk_region_get_clipbox:
 * @region: a #GdkRegion
 * @rectangle: return location for the clipbox
 *
 * Obtains the smallest rectangle which includes the entire #GdkRegion.
 *
 */
void
gdk_region_get_clipbox (const GdkRegion *region,
			GdkRectangle    *rectangle)
{
  g_return_if_fail (region != NULL);
  g_return_if_fail (rectangle != NULL);
  
  rectangle->x = region->extents.x1;
  rectangle->y = region->extents.y1;
  rectangle->width = region->extents.x2 - region->extents.x1;
  rectangle->height = region->extents.y2 - region->extents.y1;
}


/**
 * gdk_region_get_rectangles:
 * @region: a #GdkRegion
 * @rectangles: (array length=n_rectangles) (transfer container): return location for an array of rectangles
 * @n_rectangles: length of returned array
 *
 * Obtains the area covered by the region as a list of rectangles.
 * The array returned in @rectangles must be freed with g_free().
 **/
void
gdk_region_get_rectangles (const GdkRegion  *region,
                           GdkRectangle    **rectangles,
                           gint             *n_rectangles)
{
  gint i;
  
  g_return_if_fail (region != NULL);
  g_return_if_fail (rectangles != NULL);
  g_return_if_fail (n_rectangles != NULL);
  
  *n_rectangles = region->numRects;
  *rectangles = g_new (GdkRectangle, region->numRects);

  for (i = 0; i < region->numRects; i++)
    {
      GdkRegionBox rect;
      rect = region->rects[i];
      (*rectangles)[i].x = rect.x1;
      (*rectangles)[i].y = rect.y1;
      (*rectangles)[i].width = rect.x2 - rect.x1;
      (*rectangles)[i].height = rect.y2 - rect.y1;
    }
}

/**
 * gdk_region_union_with_rect:
 * @region: a #GdkRegion.
 * @rect: a #GdkRectangle.
 * 
 * Sets the area of @region to the union of the areas of @region and
 * @rect. The resulting area is the set of pixels contained in
 * either @region or @rect.
 **/
void
gdk_region_union_with_rect (GdkRegion          *region,
			    const GdkRectangle *rect)
{
  GdkRegion tmp_region;

  g_return_if_fail (region != NULL);
  g_return_if_fail (rect != NULL);

  if (rect->width <= 0 || rect->height <= 0)
    return;
    
  tmp_region.rects = &tmp_region.extents;
  tmp_region.numRects = 1;
  tmp_region.extents.x1 = rect->x;
  tmp_region.extents.y1 = rect->y;
  tmp_region.extents.x2 = rect->x + rect->width;
  tmp_region.extents.y2 = rect->y + rect->height;
  tmp_region.size = 1;

  gdk_region_union (region, &tmp_region);
}

/*-
 *-----------------------------------------------------------------------
 * miSetExtents --
 *	Reset the extents of a region to what they should be. Called by
 *	miSubtract and miIntersect b/c they can't figure it out along the
 *	way or do so easily, as miUnion can.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The region's 'extents' structure is overwritten.
 *
 *-----------------------------------------------------------------------
 */
static void
miSetExtents (GdkRegion *pReg)
{
  GdkRegionBox *pBox, *pBoxEnd, *pExtents;

  if (pReg->numRects == 0)
    {
      pReg->extents.x1 = 0;
      pReg->extents.y1 = 0;
      pReg->extents.x2 = 0;
      pReg->extents.y2 = 0;
      return;
    }
  
  pExtents = &pReg->extents;
  pBox = pReg->rects;
  pBoxEnd = &pBox[pReg->numRects - 1];

    /*
     * Since pBox is the first rectangle in the region, it must have the
     * smallest y1 and since pBoxEnd is the last rectangle in the region,
     * it must have the largest y2, because of banding. Initialize x1 and
     * x2 from  pBox and pBoxEnd, resp., as good things to initialize them
     * to...
     */
  pExtents->x1 = pBox->x1;
  pExtents->y1 = pBox->y1;
  pExtents->x2 = pBoxEnd->x2;
  pExtents->y2 = pBoxEnd->y2;

  g_assert(pExtents->y1 < pExtents->y2);
  while (pBox <= pBoxEnd)
    {
      if (pBox->x1 < pExtents->x1)
	{
	  pExtents->x1 = pBox->x1;
	}
      if (pBox->x2 > pExtents->x2)
	{
	  pExtents->x2 = pBox->x2;
	}
      pBox++;
    }
  g_assert(pExtents->x1 < pExtents->x2);
}

/**
 * gdk_region_destroy:
 * @region: a #GdkRegion
 *
 * Destroys a #GdkRegion.
 */
void
gdk_region_destroy (GdkRegion *region)
{
  g_return_if_fail (region != NULL);

  if (region->rects != &region->extents)
    g_free (region->rects);
  g_slice_free (GdkRegion, region);
}


/**
 * gdk_region_offset:
 * @region: a #GdkRegion
 * @dx: the distance to move the region horizontally
 * @dy: the distance to move the region vertically
 *
 * Moves a region the specified distance.
 */
void
gdk_region_offset (GdkRegion *region,
		   gint       x,
		   gint       y)
{
  int nbox;
  GdkRegionBox *pbox;

  g_return_if_fail (region != NULL);

  pbox = region->rects;
  nbox = region->numRects;

  while(nbox--)
    {
      pbox->x1 += x;
      pbox->x2 += x;
      pbox->y1 += y;
      pbox->y2 += y;
      pbox++;
    }
  if (region->rects != &region->extents)
    {
      region->extents.x1 += x;
      region->extents.x2 += x;
      region->extents.y1 += y;
      region->extents.y2 += y;
    }
}

/* 
   Utility procedure Compress:
   Replace r by the region r', where 
     p in r' iff (Quantifer m <= dx) (p + m in r), and
     Quantifier is Exists if grow is TRUE, For all if grow is FALSE, and
     (x,y) + m = (x+m,y) if xdir is TRUE; (x,y+m) if xdir is FALSE.

   Thus, if xdir is TRUE and grow is FALSE, r is replaced by the region
   of all points p such that p and the next dx points on the same
   horizontal scan line are all in r.  We do this using by noting
   that p is the head of a run of length 2^i + k iff p is the head
   of a run of length 2^i and p+2^i is the head of a run of length
   k. Thus, the loop invariant: s contains the region corresponding
   to the runs of length shift.  r contains the region corresponding
   to the runs of length 1 + dxo & (shift-1), where dxo is the original
   value of dx.  dx = dxo & ~(shift-1).  As parameters, s and t are
   scratch regions, so that we don't have to allocate them on every
   call.
*/

#define ZOpRegion(a,b) if (grow) gdk_region_union (a, b); \
			 else gdk_region_intersect (a,b)
#define ZShiftRegion(a,b) if (xdir) gdk_region_offset (a,b,0); \
			  else gdk_region_offset (a,0,b)

static void
Compress(GdkRegion *r,
	 GdkRegion *s,
	 GdkRegion *t,
	 guint      dx,
	 int        xdir,
	 int        grow)
{
  guint shift = 1;

  miRegionCopy (s, r);
  while (dx)
    {
      if (dx & shift)
	{
	  ZShiftRegion(r, -(int)shift);
	  ZOpRegion(r, s);
	  dx -= shift;
	  if (!dx) break;
        }
      miRegionCopy (t, s);
      ZShiftRegion(s, -(int)shift);
      ZOpRegion(s, t);
      shift <<= 1;
    }
}

#undef ZOpRegion
#undef ZShiftRegion
#undef ZCopyRegion

/**
 * gdk_region_shrink:
 * @region: a #GdkRegion
 * @dx: the number of pixels to shrink the region horizontally
 * @dy: the number of pixels to shrink the region vertically
 *
 * Resizes a region by the specified amount.
 * Positive values shrink the region. Negative values expand it.
 *
 * Deprecated: 2.22: There is no replacement for this function.
 */
void
gdk_region_shrink (GdkRegion *region,
		   int        dx,
		   int        dy)
{
  GdkRegion *s, *t;
  int grow;

  g_return_if_fail (region != NULL);

  if (!dx && !dy)
    return;

  s = gdk_region_new ();
  t = gdk_region_new ();

  grow = (dx < 0);
  if (grow)
    dx = -dx;
  if (dx)
     Compress(region, s, t, (unsigned) 2*dx, TRUE, grow);
     
  grow = (dy < 0);
  if (grow)
    dy = -dy;
  if (dy)
     Compress(region, s, t, (unsigned) 2*dy, FALSE, grow);
  
  gdk_region_offset (region, dx, dy);
  gdk_region_destroy (s);
  gdk_region_destroy (t);
}


/*======================================================================
 *	    Region Intersection
 *====================================================================*/
/*-
 *-----------------------------------------------------------------------
 * miIntersectO --
 *	Handle an overlapping band for miIntersect.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Rectangles may be added to the region.
 *
 *-----------------------------------------------------------------------
 */
/* static void*/
static void
miIntersectO (GdkRegion    *pReg,
	      GdkRegionBox *r1,
	      GdkRegionBox *r1End,
	      GdkRegionBox *r2,
	      GdkRegionBox *r2End,
	      gint          y1,
	      gint          y2)
{
  int  	x1;
  int  	x2;
  GdkRegionBox *pNextRect;

  pNextRect = &pReg->rects[pReg->numRects];

  while ((r1 != r1End) && (r2 != r2End))
    {
      x1 = MAX (r1->x1,r2->x1);
      x2 = MIN (r1->x2,r2->x2);

      /*
       * If there's any overlap between the two rectangles, add that
       * overlap to the new region.
       * There's no need to check for subsumption because the only way
       * such a need could arise is if some region has two rectangles
       * right next to each other. Since that should never happen...
       */
      if (x1 < x2)
	{
	  g_assert (y1<y2);

	  MEMCHECK (pReg, pNextRect, pReg->rects);
	  pNextRect->x1 = x1;
	  pNextRect->y1 = y1;
	  pNextRect->x2 = x2;
	  pNextRect->y2 = y2;
	  pReg->numRects += 1;
	  pNextRect++;
	  g_assert (pReg->numRects <= pReg->size);
	}

      /*
       * Need to advance the pointers. Shift the one that extends
       * to the right the least, since the other still has a chance to
       * overlap with that region's next rectangle, if you see what I mean.
       */
      if (r1->x2 < r2->x2)
	{
	  r1++;
	}
      else if (r2->x2 < r1->x2)
	{
	  r2++;
	}
      else
	{
	  r1++;
	  r2++;
	}
    }
}

/**
 * gdk_region_intersect:
 * @source1: a #GdkRegion
 * @source2: another #GdkRegion
 *
 * Sets the area of @source1 to the intersection of the areas of @source1
 * and @source2. The resulting area is the set of pixels contained in
 * both @source1 and @source2.
 **/
void
gdk_region_intersect (GdkRegion       *source1,
		      const GdkRegion *source2)
{
  g_return_if_fail (source1 != NULL);
  g_return_if_fail (source2 != NULL);
  
  /* check for trivial reject */
  if ((!(source1->numRects)) || (!(source2->numRects))  ||
      (!EXTENTCHECK(&source1->extents, &source2->extents)))
    source1->numRects = 0;
  else
    miRegionOp (source1, source1, source2,
    		miIntersectO, (nonOverlapFunc) NULL, (nonOverlapFunc) NULL);
    
  /*
   * Can't alter source1's extents before miRegionOp depends on the
   * extents of the regions being unchanged. Besides, this way there's
   * no checking against rectangles that will be nuked due to
   * coalescing, so we have to examine fewer rectangles.
   */
  miSetExtents(source1);
}

static void
miRegionCopy (GdkRegion       *dstrgn,
	      const GdkRegion *rgn)
{
  if (dstrgn != rgn) /*  don't want to copy to itself */
    {  
      if (dstrgn->size < rgn->numRects)
        {
	  if (dstrgn->rects != &dstrgn->extents)
	    g_free (dstrgn->rects);

	  dstrgn->rects = g_new (GdkRegionBox, rgn->numRects);
	  dstrgn->size = rgn->numRects;
	}

      dstrgn->numRects = rgn->numRects;
      dstrgn->extents = rgn->extents;

      memcpy (dstrgn->rects, rgn->rects, rgn->numRects * sizeof (GdkRegionBox));
    }
}


/*======================================================================
 *	    Generic Region Operator
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * miCoalesce --
 *	Attempt to merge the boxes in the current band with those in the
 *	previous one. Used only by miRegionOp.
 *
 * Results:
 *	The new index for the previous band.
 *
 * Side Effects:
 *	If coalescing takes place:
 *	    - rectangles in the previous band will have their y2 fields
 *	      altered.
 *	    - pReg->numRects will be decreased.
 *
 *-----------------------------------------------------------------------
 */
/* static int*/
static int
miCoalesce (GdkRegion *pReg,         /* Region to coalesce */
	    gint       prevStart,    /* Index of start of previous band */
	    gint       curStart)     /* Index of start of current band */
{
  GdkRegionBox *pPrevBox;   	/* Current box in previous band */
  GdkRegionBox *pCurBox;    	/* Current box in current band */
  GdkRegionBox *pRegEnd;    	/* End of region */
  int	    	curNumRects;	/* Number of rectangles in current
				 * band */
  int	    	prevNumRects;	/* Number of rectangles in previous
				 * band */
  int	    	bandY1;	    	/* Y1 coordinate for current band */

  pRegEnd = &pReg->rects[pReg->numRects];

  pPrevBox = &pReg->rects[prevStart];
  prevNumRects = curStart - prevStart;

    /*
     * Figure out how many rectangles are in the current band. Have to do
     * this because multiple bands could have been added in miRegionOp
     * at the end when one region has been exhausted.
     */
  pCurBox = &pReg->rects[curStart];
  bandY1 = pCurBox->y1;
  for (curNumRects = 0;
       (pCurBox != pRegEnd) && (pCurBox->y1 == bandY1);
       curNumRects++)
    {
      pCurBox++;
    }
    
  if (pCurBox != pRegEnd)
    {
      /*
       * If more than one band was added, we have to find the start
       * of the last band added so the next coalescing job can start
       * at the right place... (given when multiple bands are added,
       * this may be pointless -- see above).
       */
      pRegEnd--;
      while (pRegEnd[-1].y1 == pRegEnd->y1)
	{
	  pRegEnd--;
	}
      curStart = pRegEnd - pReg->rects;
      pRegEnd = pReg->rects + pReg->numRects;
    }
	
  if ((curNumRects == prevNumRects) && (curNumRects != 0)) {
    pCurBox -= curNumRects;
    /*
     * The bands may only be coalesced if the bottom of the previous
     * matches the top scanline of the current.
     */
    if (pPrevBox->y2 == pCurBox->y1)
      {
	/*
	 * Make sure the bands have boxes in the same places. This
	 * assumes that boxes have been added in such a way that they
	 * cover the most area possible. I.e. two boxes in a band must
	 * have some horizontal space between them.
	 */
	do
	  {
	    if ((pPrevBox->x1 != pCurBox->x1) ||
		(pPrevBox->x2 != pCurBox->x2))
	      {
		/*
		 * The bands don't line up so they can't be coalesced.
		 */
		return (curStart);
	      }
	    pPrevBox++;
	    pCurBox++;
	    prevNumRects -= 1;
	  } while (prevNumRects != 0);

	pReg->numRects -= curNumRects;
	pCurBox -= curNumRects;
	pPrevBox -= curNumRects;

	/*
	 * The bands may be merged, so set the bottom y of each box
	 * in the previous band to that of the corresponding box in
	 * the current band.
	 */
	do
	  {
	    pPrevBox->y2 = pCurBox->y2;
	    pPrevBox++;
	    pCurBox++;
	    curNumRects -= 1;
	  }
	while (curNumRects != 0);

	/*
	 * If only one band was added to the region, we have to backup
	 * curStart to the start of the previous band.
	 *
	 * If more than one band was added to the region, copy the
	 * other bands down. The assumption here is that the other bands
	 * came from the same region as the current one and no further
	 * coalescing can be done on them since it's all been done
	 * already... curStart is already in the right place.
	 */
	if (pCurBox == pRegEnd)
	  {
	    curStart = prevStart;
	  }
	else
	  {
	    do
	      {
		*pPrevBox++ = *pCurBox++;
	      }
	    while (pCurBox != pRegEnd);
	  }
	    
      }
  }
  return curStart;
}

/*-
 *-----------------------------------------------------------------------
 * miRegionOp --
 *	Apply an operation to two regions. Called by miUnion, miInverse,
 *	miSubtract, miIntersect...
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The new region is overwritten.
 *
 * Notes:
 *	The idea behind this function is to view the two regions as sets.
 *	Together they cover a rectangle of area that this function divides
 *	into horizontal bands where points are covered only by one region
 *	or by both. For the first case, the nonOverlapFunc is called with
 *	each the band and the band's upper and lower extents. For the
 *	second, the overlapFunc is called to process the entire band. It
 *	is responsible for clipping the rectangles in the band, though
 *	this function provides the boundaries.
 *	At the end of each band, the new region is coalesced, if possible,
 *	to reduce the number of rectangles in the region.
 *
 *-----------------------------------------------------------------------
 */
/* static void*/
static void
miRegionOp(GdkRegion       *newReg,
	   GdkRegion       *reg1,
	   const GdkRegion *reg2,
	   overlapFunc      overlapFn,          /* Function to call for over-
						 * lapping bands */
	   nonOverlapFunc   nonOverlap1Fn,	/* Function to call for non-
						 * overlapping bands in region
						 * 1 */
	   nonOverlapFunc   nonOverlap2Fn)	/* Function to call for non-
						 * overlapping bands in region
						 * 2 */
{
    GdkRegionBox *r1; 	    	    	/* Pointer into first region */
    GdkRegionBox *r2; 	    	    	/* Pointer into 2d region */
    GdkRegionBox *r1End;    	    	/* End of 1st region */
    GdkRegionBox *r2End;    	    	/* End of 2d region */
    int    	  ybot;	    	    	/* Bottom of intersection */
    int  	  ytop;	    	    	/* Top of intersection */
    GdkRegionBox *oldRects;   	    	/* Old rects for newReg */
    int	    	  prevBand;   	    	/* Index of start of
					 * previous band in newReg */
    int	  	  curBand;    	    	/* Index of start of current
					 * band in newReg */
    GdkRegionBox *r1BandEnd;  	    	/* End of current band in r1 */
    GdkRegionBox *r2BandEnd;  	    	/* End of current band in r2 */
    int     	  top;	    	    	/* Top of non-overlapping
					 * band */
    int     	  bot;	    	    	/* Bottom of non-overlapping
					 * band */
    
    /*
     * Initialization:
     *	set r1, r2, r1End and r2End appropriately, preserve the important
     * parts of the destination region until the end in case it's one of
     * the two source regions, then mark the "new" region empty, allocating
     * another array of rectangles for it to use.
     */
    r1 = reg1->rects;
    r2 = reg2->rects;
    r1End = r1 + reg1->numRects;
    r2End = r2 + reg2->numRects;
    
    oldRects = newReg->rects;
    
    EMPTY_REGION(newReg);

    /*
     * Allocate a reasonable number of rectangles for the new region. The idea
     * is to allocate enough so the individual functions don't need to
     * reallocate and copy the array, which is time consuming, yet we don't
     * have to worry about using too much memory. I hope to be able to
     * nuke the Xrealloc() at the end of this function eventually.
     */
    newReg->size = MAX (reg1->numRects, reg2->numRects) * 2;
    newReg->rects = g_new (GdkRegionBox, newReg->size);
    
    /*
     * Initialize ybot and ytop.
     * In the upcoming loop, ybot and ytop serve different functions depending
     * on whether the band being handled is an overlapping or non-overlapping
     * band.
     * 	In the case of a non-overlapping band (only one of the regions
     * has points in the band), ybot is the bottom of the most recent
     * intersection and thus clips the top of the rectangles in that band.
     * ytop is the top of the next intersection between the two regions and
     * serves to clip the bottom of the rectangles in the current band.
     *	For an overlapping band (where the two regions intersect), ytop clips
     * the top of the rectangles of both regions and ybot clips the bottoms.
     */
    if (reg1->extents.y1 < reg2->extents.y1)
      ybot = reg1->extents.y1;
    else
      ybot = reg2->extents.y1;
    
    /*
     * prevBand serves to mark the start of the previous band so rectangles
     * can be coalesced into larger rectangles. qv. miCoalesce, above.
     * In the beginning, there is no previous band, so prevBand == curBand
     * (curBand is set later on, of course, but the first band will always
     * start at index 0). prevBand and curBand must be indices because of
     * the possible expansion, and resultant moving, of the new region's
     * array of rectangles.
     */
    prevBand = 0;
    
    do
      {
	curBand = newReg->numRects;

	/*
	 * This algorithm proceeds one source-band (as opposed to a
	 * destination band, which is determined by where the two regions
	 * intersect) at a time. r1BandEnd and r2BandEnd serve to mark the
	 * rectangle after the last one in the current band for their
	 * respective regions.
	 */
	r1BandEnd = r1;
	while ((r1BandEnd != r1End) && (r1BandEnd->y1 == r1->y1))
	  {
	    r1BandEnd++;
	  }
	
	r2BandEnd = r2;
	while ((r2BandEnd != r2End) && (r2BandEnd->y1 == r2->y1))
	  {
	    r2BandEnd++;
	  }
	
	/*
	 * First handle the band that doesn't intersect, if any.
	 *
	 * Note that attention is restricted to one band in the
	 * non-intersecting region at once, so if a region has n
	 * bands between the current position and the next place it overlaps
	 * the other, this entire loop will be passed through n times.
	 */
	if (r1->y1 < r2->y1)
	  {
	    top = MAX (r1->y1,ybot);
	    bot = MIN (r1->y2,r2->y1);

	    if ((top != bot) && (nonOverlap1Fn != (void (*)())NULL))
	      {
		(* nonOverlap1Fn) (newReg, r1, r1BandEnd, top, bot);
	      }

	    ytop = r2->y1;
	  }
	else if (r2->y1 < r1->y1)
	  {
	    top = MAX (r2->y1,ybot);
	    bot = MIN (r2->y2,r1->y1);

	    if ((top != bot) && (nonOverlap2Fn != (void (*)())NULL))
	      {
		(* nonOverlap2Fn) (newReg, r2, r2BandEnd, top, bot);
	      }

	    ytop = r1->y1;
	  }
	else
	  {
	    ytop = r1->y1;
	  }

	/*
	 * If any rectangles got added to the region, try and coalesce them
	 * with rectangles from the previous band. Note we could just do
	 * this test in miCoalesce, but some machines incur a not
	 * inconsiderable cost for function calls, so...
	 */
	if (newReg->numRects != curBand)
	  {
	    prevBand = miCoalesce (newReg, prevBand, curBand);
	  }

	/*
	 * Now see if we've hit an intersecting band. The two bands only
	 * intersect if ybot > ytop
	 */
	ybot = MIN (r1->y2, r2->y2);
	curBand = newReg->numRects;
	if (ybot > ytop)
	  {
	    (* overlapFn) (newReg, r1, r1BandEnd, r2, r2BandEnd, ytop, ybot);

	  }
	
	if (newReg->numRects != curBand)
	  {
	    prevBand = miCoalesce (newReg, prevBand, curBand);
	  }

	/*
	 * If we've finished with a band (y2 == ybot) we skip forward
	 * in the region to the next band.
	 */
	if (r1->y2 == ybot)
	  {
	    r1 = r1BandEnd;
	  }
	if (r2->y2 == ybot)
	  {
	    r2 = r2BandEnd;
	  }
      } while ((r1 != r1End) && (r2 != r2End));

    /*
     * Deal with whichever region still has rectangles left.
     */
    curBand = newReg->numRects;
    if (r1 != r1End)
      {
	if (nonOverlap1Fn != (nonOverlapFunc )NULL)
	  {
	    do
	      {
		r1BandEnd = r1;
		while ((r1BandEnd < r1End) && (r1BandEnd->y1 == r1->y1))
		  {
		    r1BandEnd++;
		  }
		(* nonOverlap1Fn) (newReg, r1, r1BandEnd,
				     MAX (r1->y1,ybot), r1->y2);
		r1 = r1BandEnd;
	      } while (r1 != r1End);
	  }
      }
    else if ((r2 != r2End) && (nonOverlap2Fn != (nonOverlapFunc) NULL))
      {
	do
	  {
	    r2BandEnd = r2;
	    while ((r2BandEnd < r2End) && (r2BandEnd->y1 == r2->y1))
	      {
		r2BandEnd++;
	      }
	    (* nonOverlap2Fn) (newReg, r2, r2BandEnd,
			       MAX (r2->y1,ybot), r2->y2);
	    r2 = r2BandEnd;
	  } while (r2 != r2End);
      }

    if (newReg->numRects != curBand)
    {
      (void) miCoalesce (newReg, prevBand, curBand);
    }

    /*
     * A bit of cleanup. To keep regions from growing without bound,
     * we shrink the array of rectangles to match the new number of
     * rectangles in the region. This never goes to 0, however...
     *
     * Only do this stuff if the number of rectangles allocated is more than
     * twice the number of rectangles in the region (a simple optimization...).
     */
    if (newReg->numRects < (newReg->size >> 1))
      {
	if (REGION_NOT_EMPTY (newReg))
	  {
	    newReg->size = newReg->numRects;
	    newReg->rects = g_renew (GdkRegionBox, newReg->rects, newReg->size);
	  }
	else
	  {
	    /*
	     * No point in doing the extra work involved in an Xrealloc if
	     * the region is empty
	     */
	    newReg->size = 1;
	    g_free (newReg->rects);
	    newReg->rects = &newReg->extents;
	  }
      }

    if (oldRects != &newReg->extents)
      g_free (oldRects);
}


/*======================================================================
 *	    Region Union
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * miUnionNonO --
 *	Handle a non-overlapping band for the union operation. Just
 *	Adds the rectangles into the region. Doesn't have to check for
 *	subsumption or anything.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	pReg->numRects is incremented and the final rectangles overwritten
 *	with the rectangles we're passed.
 *
 *-----------------------------------------------------------------------
 */
static void
miUnionNonO (GdkRegion    *pReg,
	     GdkRegionBox *r,
	     GdkRegionBox *rEnd,
	     gint          y1,
	     gint          y2)
{
  GdkRegionBox *pNextRect;

  pNextRect = &pReg->rects[pReg->numRects];

  g_assert(y1 < y2);

  while (r != rEnd)
    {
      g_assert(r->x1 < r->x2);
      MEMCHECK(pReg, pNextRect, pReg->rects);
      pNextRect->x1 = r->x1;
      pNextRect->y1 = y1;
      pNextRect->x2 = r->x2;
      pNextRect->y2 = y2;
      pReg->numRects += 1;
      pNextRect++;

      g_assert(pReg->numRects<=pReg->size);
      r++;
    }
}


/*-
 *-----------------------------------------------------------------------
 * miUnionO --
 *	Handle an overlapping band for the union operation. Picks the
 *	left-most rectangle each time and merges it into the region.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Rectangles are overwritten in pReg->rects and pReg->numRects will
 *	be changed.
 *
 *-----------------------------------------------------------------------
 */

/* static void*/
static void
miUnionO (GdkRegion *pReg,
	  GdkRegionBox *r1,
	  GdkRegionBox *r1End,
	  GdkRegionBox *r2,
	  GdkRegionBox *r2End,
	  gint          y1,
	  gint          y2)
{
  GdkRegionBox *	pNextRect;
    
  pNextRect = &pReg->rects[pReg->numRects];

#define MERGERECT(r) 					\
    if ((pReg->numRects != 0) &&  			\
	(pNextRect[-1].y1 == y1) &&  			\
	(pNextRect[-1].y2 == y2) &&  			\
	(pNextRect[-1].x2 >= r->x1))  			\
      {  						\
	if (pNextRect[-1].x2 < r->x2)  			\
	  {  						\
	    pNextRect[-1].x2 = r->x2;  			\
	    g_assert(pNextRect[-1].x1<pNextRect[-1].x2); 	\
	  }  						\
      }  						\
    else  						\
      {  						\
	MEMCHECK(pReg, pNextRect, pReg->rects); 	\
	pNextRect->y1 = y1;  				\
	pNextRect->y2 = y2;  				\
	pNextRect->x1 = r->x1;  			\
	pNextRect->x2 = r->x2;  			\
	pReg->numRects += 1;  				\
        pNextRect += 1;  				\
      }  						\
    g_assert(pReg->numRects<=pReg->size);			\
    r++;
    
    g_assert (y1<y2);
    while ((r1 != r1End) && (r2 != r2End))
    {
	if (r1->x1 < r2->x1)
	{
	    MERGERECT(r1);
	}
	else
	{
	    MERGERECT(r2);
	}
    }
    
    if (r1 != r1End)
    {
	do
	{
	    MERGERECT(r1);
	} while (r1 != r1End);
    }
    else while (r2 != r2End)
    {
	MERGERECT(r2);
    }
}

/**
 * gdk_region_union:
 * @source1:  a #GdkRegion
 * @source2: a #GdkRegion 
 * 
 * Sets the area of @source1 to the union of the areas of @source1 and
 * @source2. The resulting area is the set of pixels contained in
 * either @source1 or @source2.
 **/
void
gdk_region_union (GdkRegion       *source1,
		  const GdkRegion *source2)
{
  g_return_if_fail (source1 != NULL);
  g_return_if_fail (source2 != NULL);
  
  /*  checks all the simple cases */

  /*
   * source1 and source2 are the same or source2 is empty
   */
  if ((source1 == source2) || (!(source2->numRects)))
    return;

  /* 
   * source1 is empty
   */
  if (!(source1->numRects))
    {
      miRegionCopy (source1, source2);
      return;
    }
  
  /*
   * source1 completely subsumes source2
   */
  if ((source1->numRects == 1) && 
      (source1->extents.x1 <= source2->extents.x1) &&
      (source1->extents.y1 <= source2->extents.y1) &&
      (source1->extents.x2 >= source2->extents.x2) &&
      (source1->extents.y2 >= source2->extents.y2))
    return;

  /*
   * source2 completely subsumes source1
   */
  if ((source2->numRects == 1) && 
      (source2->extents.x1 <= source1->extents.x1) &&
      (source2->extents.y1 <= source1->extents.y1) &&
      (source2->extents.x2 >= source1->extents.x2) &&
      (source2->extents.y2 >= source1->extents.y2))
    {
      miRegionCopy(source1, source2);
      return;
    }

  miRegionOp (source1, source1, source2, miUnionO, 
	      miUnionNonO, miUnionNonO);

  source1->extents.x1 = MIN (source1->extents.x1, source2->extents.x1);
  source1->extents.y1 = MIN (source1->extents.y1, source2->extents.y1);
  source1->extents.x2 = MAX (source1->extents.x2, source2->extents.x2);
  source1->extents.y2 = MAX (source1->extents.y2, source2->extents.y2);
}


/*======================================================================
 * 	    	  Region Subtraction
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * miSubtractNonO --
 *	Deal with non-overlapping band for subtraction. Any parts from
 *	region 2 we discard. Anything from region 1 we add to the region.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	pReg may be affected.
 *
 *-----------------------------------------------------------------------
 */
/* static void*/
static void
miSubtractNonO1 (GdkRegion    *pReg,
		 GdkRegionBox *r,
		 GdkRegionBox *rEnd,
		 gint          y1,
		 gint          y2)
{
  GdkRegionBox *	pNextRect;
	
  pNextRect = &pReg->rects[pReg->numRects];
	
  g_assert(y1<y2);

  while (r != rEnd)
    {
      g_assert (r->x1<r->x2);
      MEMCHECK (pReg, pNextRect, pReg->rects);
      pNextRect->x1 = r->x1;
      pNextRect->y1 = y1;
      pNextRect->x2 = r->x2;
      pNextRect->y2 = y2;
      pReg->numRects += 1;
      pNextRect++;

      g_assert (pReg->numRects <= pReg->size);

      r++;
    }
}

/*-
 *-----------------------------------------------------------------------
 * miSubtractO --
 *	Overlapping band subtraction. x1 is the left-most point not yet
 *	checked.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	pReg may have rectangles added to it.
 *
 *-----------------------------------------------------------------------
 */
/* static void*/
static void
miSubtractO (GdkRegion    *pReg,
	     GdkRegionBox *r1,
	     GdkRegionBox *r1End,
	     GdkRegionBox *r2,
	     GdkRegionBox *r2End,
	     gint          y1,
	     gint          y2)
{
  GdkRegionBox *	pNextRect;
  int  	x1;
    
  x1 = r1->x1;
    
  g_assert(y1<y2);
  pNextRect = &pReg->rects[pReg->numRects];

  while ((r1 != r1End) && (r2 != r2End))
    {
      if (r2->x2 <= x1)
	{
	  /*
	   * Subtrahend missed the boat: go to next subtrahend.
	   */
	  r2++;
	}
      else if (r2->x1 <= x1)
	{
	  /*
	   * Subtrahend preceeds minuend: nuke left edge of minuend.
	   */
	  x1 = r2->x2;
	  if (x1 >= r1->x2)
	    {
	      /*
	       * Minuend completely covered: advance to next minuend and
	       * reset left fence to edge of new minuend.
	       */
	      r1++;
	      if (r1 != r1End)
		x1 = r1->x1;
	    }
	  else
	    {
	      /*
	       * Subtrahend now used up since it doesn't extend beyond
	       * minuend
	       */
	      r2++;
	    }
	}
      else if (r2->x1 < r1->x2)
	{
	  /*
	   * Left part of subtrahend covers part of minuend: add uncovered
	   * part of minuend to region and skip to next subtrahend.
	   */
	  g_assert(x1<r2->x1);
	  MEMCHECK(pReg, pNextRect, pReg->rects);
	  pNextRect->x1 = x1;
	  pNextRect->y1 = y1;
	  pNextRect->x2 = r2->x1;
	  pNextRect->y2 = y2;
	  pReg->numRects += 1;
	  pNextRect++;

	  g_assert(pReg->numRects<=pReg->size);

	  x1 = r2->x2;
	  if (x1 >= r1->x2)
	    {
	      /*
	       * Minuend used up: advance to new...
	       */
	      r1++;
	      if (r1 != r1End)
		x1 = r1->x1;
	    }
	  else
	    {
	      /*
	       * Subtrahend used up
	       */
	      r2++;
	    }
	}
      else
	{
	  /*
	   * Minuend used up: add any remaining piece before advancing.
	   */
	  if (r1->x2 > x1)
	    {
	      MEMCHECK(pReg, pNextRect, pReg->rects);
	      pNextRect->x1 = x1;
	      pNextRect->y1 = y1;
	      pNextRect->x2 = r1->x2;
	      pNextRect->y2 = y2;
	      pReg->numRects += 1;
	      pNextRect++;
	      g_assert(pReg->numRects<=pReg->size);
	    }
	  r1++;
	  if (r1 != r1End)
	    x1 = r1->x1;
	}
    }

  /*
     * Add remaining minuend rectangles to region.
     */
  while (r1 != r1End)
    {
      g_assert(x1<r1->x2);
      MEMCHECK(pReg, pNextRect, pReg->rects);
      pNextRect->x1 = x1;
      pNextRect->y1 = y1;
      pNextRect->x2 = r1->x2;
      pNextRect->y2 = y2;
      pReg->numRects += 1;
      pNextRect++;

      g_assert(pReg->numRects<=pReg->size);

      r1++;
      if (r1 != r1End)
	{
	  x1 = r1->x1;
	}
    }
}

/**
 * gdk_region_subtract:
 * @source1: a #GdkRegion
 * @source2: another #GdkRegion
 *
 * Subtracts the area of @source2 from the area @source1. The resulting
 * area is the set of pixels contained in @source1 but not in @source2.
 **/
void
gdk_region_subtract (GdkRegion       *source1,
		     const GdkRegion *source2)
{
  g_return_if_fail (source1 != NULL);
  g_return_if_fail (source2 != NULL);
  
  /* check for trivial reject */
  if ((!(source1->numRects)) || (!(source2->numRects)) ||
      (!EXTENTCHECK(&source1->extents, &source2->extents)))
    return;
 
  miRegionOp (source1, source1, source2, miSubtractO,
	      miSubtractNonO1, (nonOverlapFunc) NULL);

  /*
   * Can't alter source1's extents before we call miRegionOp because miRegionOp
   * depends on the extents of those regions being the unaltered. Besides, this
   * way there's no checking against rectangles that will be nuked
   * due to coalescing, so we have to examine fewer rectangles.
   */
  miSetExtents (source1);
}

/**
 * gdk_region_xor:
 * @source1: a #GdkRegion
 * @source2: another #GdkRegion
 *
 * Sets the area of @source1 to the exclusive-OR of the areas of @source1
 * and @source2. The resulting area is the set of pixels contained in one
 * or the other of the two sources but not in both.
 **/
void
gdk_region_xor (GdkRegion       *source1,
		const GdkRegion *source2)
{
  GdkRegion *trb;

  g_return_if_fail (source1 != NULL);
  g_return_if_fail (source2 != NULL);

  trb = gdk_region_copy (source2);

  gdk_region_subtract (trb, source1);
  gdk_region_subtract (source1, source2);

  gdk_region_union (source1, trb);
  
  gdk_region_destroy (trb);
}

/**
 * gdk_region_empty: 
 * @region: a #GdkRegion
 *
 * Finds out if the #GdkRegion is empty.
 *
 * Returns: %TRUE if @region is empty.
 */
gboolean
gdk_region_empty (const GdkRegion *region)
{
  g_return_val_if_fail (region != NULL, FALSE);
  
  if (region->numRects == 0)
    return TRUE;
  else
    return FALSE;
}

/**
 * gdk_region_equal:
 * @region1: a #GdkRegion
 * @region2: a #GdkRegion
 *
 * Finds out if the two regions are the same.
 *
 * Returns: %TRUE if @region1 and @region2 are equal.
 */
gboolean
gdk_region_equal (const GdkRegion *region1,
		  const GdkRegion *region2)
{
  int i;

  g_return_val_if_fail (region1 != NULL, FALSE);
  g_return_val_if_fail (region2 != NULL, FALSE);

  if (region1->numRects != region2->numRects) return FALSE;
  else if (region1->numRects == 0) return TRUE;
  else if (region1->extents.x1 != region2->extents.x1) return FALSE;
  else if (region1->extents.x2 != region2->extents.x2) return FALSE;
  else if (region1->extents.y1 != region2->extents.y1) return FALSE;
  else if (region1->extents.y2 != region2->extents.y2) return FALSE;
  else
    for(i = 0; i < region1->numRects; i++ )
      {
	if (region1->rects[i].x1 != region2->rects[i].x1) return FALSE;
	else if (region1->rects[i].x2 != region2->rects[i].x2) return FALSE;
	else if (region1->rects[i].y1 != region2->rects[i].y1) return FALSE;
	else if (region1->rects[i].y2 != region2->rects[i].y2) return FALSE;
      }
  return TRUE;
}

/**
 * gdk_region_rect_equal:
 * @region: a #GdkRegion
 * @rectangle: a #GdkRectangle
 *
 * Finds out if a regions is the same as a rectangle.
 *
 * Returns: %TRUE if @region and @rectangle are equal.
 *
 * Since: 2.18
 *
 * Deprecated: 2.22: Use gdk_region_new_rect() and gdk_region_equal() to 
 *             achieve the same effect.
 */
gboolean
gdk_region_rect_equal (const GdkRegion    *region,
		       const GdkRectangle *rectangle)
{
  g_return_val_if_fail (region != NULL, FALSE);
  g_return_val_if_fail (rectangle != NULL, FALSE);

  if (region->numRects != 1) return FALSE;
  else if (region->extents.x1 != rectangle->x) return FALSE;
  else if (region->extents.y1 != rectangle->y) return FALSE;
  else if (region->extents.x2 != rectangle->x + rectangle->width) return FALSE;
  else if (region->extents.y2 != rectangle->y + rectangle->height) return FALSE;
  return TRUE;
}

/**
 * gdk_region_point_in:
 * @region: a #GdkRegion
 * @x: the x coordinate of a point
 * @y: the y coordinate of a point
 *
 * Finds out if a point is in a region.
 *
 * Returns: %TRUE if the point is in @region.
 */
gboolean
gdk_region_point_in (const GdkRegion *region,
		     int              x,
		     int              y)
{
  int i;

  g_return_val_if_fail (region != NULL, FALSE);

  if (region->numRects == 0)
    return FALSE;
  if (!INBOX(region->extents, x, y))
    return FALSE;
  for (i = 0; i < region->numRects; i++)
    {
      if (INBOX (region->rects[i], x, y))
	return TRUE;
    }
  return FALSE;
}

/**
 * gdk_region_rect_in: 
 * @region: a #GdkRegion.
 * @rectangle: a #GdkRectangle.
 *
 * Tests whether a rectangle is within a region.
 *
 * Returns: %GDK_OVERLAP_RECTANGLE_IN, %GDK_OVERLAP_RECTANGLE_OUT, or
 *   %GDK_OVERLAP_RECTANGLE_PART, depending on whether the rectangle is inside,
 *   outside, or partly inside the #GdkRegion, respectively.
 */
GdkOverlapType
gdk_region_rect_in (const GdkRegion    *region,
		    const GdkRectangle *rectangle)
{
  GdkRegionBox *pbox;
  GdkRegionBox *pboxEnd;
  GdkRegionBox  rect;
  GdkRegionBox *prect = &rect;
  gboolean      partIn, partOut;
  gint rx, ry;

  g_return_val_if_fail (region != NULL, GDK_OVERLAP_RECTANGLE_OUT);
  g_return_val_if_fail (rectangle != NULL, GDK_OVERLAP_RECTANGLE_OUT);

  rx = rectangle->x;
  ry = rectangle->y;
  
  prect->x1 = rx;
  prect->y1 = ry;
  prect->x2 = rx + rectangle->width;
  prect->y2 = ry + rectangle->height;
    
    /* this is (just) a useful optimization */
  if ((region->numRects == 0) || !EXTENTCHECK (&region->extents, prect))
    return GDK_OVERLAP_RECTANGLE_OUT;

  partOut = FALSE;
  partIn = FALSE;

    /* can stop when both partOut and partIn are TRUE, or we reach prect->y2 */
  for (pbox = region->rects, pboxEnd = pbox + region->numRects;
       pbox < pboxEnd;
       pbox++)
    {

      if (pbox->y2 <= ry)
	continue;	/* getting up to speed or skipping remainder of band */

      if (pbox->y1 > ry)
	{
	  partOut = TRUE;	/* missed part of rectangle above */
	  if (partIn || (pbox->y1 >= prect->y2))
	    break;
	  ry = pbox->y1;	/* x guaranteed to be == prect->x1 */
	}

      if (pbox->x2 <= rx)
	continue;		/* not far enough over yet */

      if (pbox->x1 > rx)
	{
	  partOut = TRUE;	/* missed part of rectangle to left */
	  if (partIn)
	    break;
	}

      if (pbox->x1 < prect->x2)
	{
	  partIn = TRUE;	/* definitely overlap */
	  if (partOut)
	    break;
	}

      if (pbox->x2 >= prect->x2)
	{
	  ry = pbox->y2;	/* finished with this band */
	  if (ry >= prect->y2)
	    break;
	  rx = prect->x1;	/* reset x out to left again */
	}
      else
	{
	  /*
	   * Because boxes in a band are maximal width, if the first box
	   * to overlap the rectangle doesn't completely cover it in that
	   * band, the rectangle must be partially out, since some of it
	   * will be uncovered in that band. partIn will have been set true
	   * by now...
	   */
	  break;
	}

    }

  return (partIn ?
	     ((ry < prect->y2) ?
	      GDK_OVERLAP_RECTANGLE_PART : GDK_OVERLAP_RECTANGLE_IN) : 
	  GDK_OVERLAP_RECTANGLE_OUT);
}


static void
gdk_region_unsorted_spans_intersect_foreach (GdkRegion     *region,
					     const GdkSpan *spans,
					     int            n_spans,
					     GdkSpanFunc    function,
					     gpointer       data)
{
  gint i, left, right, y;
  gint clipped_left, clipped_right;
  GdkRegionBox *pbox;
  GdkRegionBox *pboxEnd;

  if (!region->numRects)
    return;

  for (i=0;i<n_spans;i++)
    {
      y = spans[i].y;
      left = spans[i].x;
      right = left + spans[i].width; /* right is not in the span! */
    
      if (! ((region->extents.y1 <= y) &&
	     (region->extents.y2 > y) &&
	     (region->extents.x1 < right) &&
	     (region->extents.x2 > left)) ) 
	continue;

      /* can stop when we passed y */
      for (pbox = region->rects, pboxEnd = pbox + region->numRects;
	   pbox < pboxEnd;
	   pbox++)
	{
	  if (pbox->y2 <= y)
	    continue; /* Not quite there yet */
	  
	  if (pbox->y1 > y)
	    break; /* passed the spanline */
	  
	  if ((right > pbox->x1) && (left < pbox->x2)) 
	    {
              GdkSpan out_span;

	      clipped_left = MAX (left, pbox->x1);
	      clipped_right = MIN (right, pbox->x2);
	      
	      out_span.y = y;
	      out_span.x = clipped_left;
	      out_span.width = clipped_right - clipped_left;
	      (*function) (&out_span, data);
	    }
	}
    }
}

/**
 * gdk_region_spans_intersect_foreach:
 * @region: a #GdkRegion
 * @spans: an array of #GdkSpans
 * @n_spans: the length of @spans
 * @sorted: %TRUE if @spans is sorted wrt. the y coordinate
 * @function: function to call on each span in the intersection
 * @data: data to pass to @function
 *
 * Calls a function on each span in the intersection of @region and @spans.
 *
 * Deprecated: 2.22: There is no replacement.
 */
void
gdk_region_spans_intersect_foreach (GdkRegion     *region,
				    const GdkSpan *spans,
				    int            n_spans,
				    gboolean       sorted,
				    GdkSpanFunc    function,
				    gpointer       data)
{
  gint left, right, y;
  gint clipped_left, clipped_right;
  GdkRegionBox *pbox;
  GdkRegionBox *pboxEnd;
  const GdkSpan *span, *tmpspan;
  const GdkSpan *end_span;

  g_return_if_fail (region != NULL);
  g_return_if_fail (spans != NULL);

  if (!sorted)
    {
      gdk_region_unsorted_spans_intersect_foreach (region,
						   spans,
						   n_spans,
						   function,
						   data);
      return;
    }
  
  if ((!region->numRects) || (n_spans == 0))
    return;

  /* The main method here is to step along the
   * sorted rectangles and spans in lock step, and
   * clipping the spans that are in the current
   * rectangle before going on to the next rectangle.
   */

  span = spans;
  end_span = spans + n_spans;
  pbox = region->rects;
  pboxEnd = pbox + region->numRects;
  while (pbox < pboxEnd)
    {
      while ((pbox->y2 < span->y) || (span->y < pbox->y1))
	{
	  /* Skip any rectangles that are above the current span */
	  if (pbox->y2 < span->y)
	    {
	      pbox++;
	      if (pbox == pboxEnd)
		return;
	    }
	  /* Skip any spans that are above the current rectangle */
	  if (span->y < pbox->y1)
	    {
	      span++;
	      if (span == end_span)
		return;
	    }
	}
      
      /* Ok, we got at least one span that might intersect this rectangle. */
      tmpspan = span;
      while ((tmpspan < end_span) &&
	     (tmpspan->y < pbox->y2))
	{
	  y = tmpspan->y;
	  left = tmpspan->x;
	  right = left + tmpspan->width; /* right is not in the span! */
	  
	  if ((right > pbox->x1) && (left < pbox->x2))
	    {
              GdkSpan out_span;

	      clipped_left = MAX (left, pbox->x1);
	      clipped_right = MIN (right, pbox->x2);
	      
	      out_span.y = y;
	      out_span.x = clipped_left;
	      out_span.width = clipped_right - clipped_left;
	      (*function) (&out_span, data);
	    }
	  
	  tmpspan++;
	}

      /* Finished this rectangle.
       * The spans could still intersect the next one
       */
      pbox++;
    }
}

#define __GDK_REGION_GENERIC_C__
#include "gdkaliasdef.c"
