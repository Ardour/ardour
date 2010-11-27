/*
 * Copyright (C) 1997, 1998, 1999, 2000 Free Software Foundation
 * All rights reserved.
 *
 * This file is part of the Gnome Library.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/*
  @NOTATION@
 */
/* Miscellaneous utility functions for the GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
 *
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>

/* needed for M_PI_2 under 'gcc -ansi -predantic' on GNU/Linux */
#ifndef _BSD_SOURCE
#  define _BSD_SOURCE 1
#endif
#include <sys/types.h>

#include <glib.h>
#include <math.h>
#include "gnome-canvas.h"
#include "gnome-canvas-util.h"
#include <libart_lgpl/art_uta.h>
#include <libart_lgpl/art_svp.h>
#include <libart_lgpl/art_svp_ops.h>
#include <libart_lgpl/art_rgb.h>
#include <libart_lgpl/art_rgb_svp.h>
#include <libart_lgpl/art_uta_svp.h>
#include <libart_lgpl/art_rect_svp.h>

/**
 * gnome_canvas_points_new:
 * @num_points: The number of points to allocate space for in the array.
 * 
 * Creates a structure that should be used to pass an array of points to
 * items.
 * 
 * Return value: A newly-created array of points.  It should be filled in
 * by the user.
 **/
GnomeCanvasPoints *
gnome_canvas_points_new (int num_points)
{
	GnomeCanvasPoints *points;

	g_return_val_if_fail (num_points > 1, NULL);

	points = g_new (GnomeCanvasPoints, 1);
	points->num_points = num_points;
	points->coords = g_new (double, 2 * num_points);
	points->ref_count = 1;

	return points;
}

/**
 * gnome_canvas_points_ref:
 * @points: A canvas points structure.
 * 
 * Increases the reference count of the specified points structure.
 * 
 * Return value: The canvas points structure itself.
 **/
GnomeCanvasPoints *
gnome_canvas_points_ref (GnomeCanvasPoints *points)
{
	g_return_val_if_fail (points != NULL, NULL);

	points->ref_count += 1;
	return points;
}

/**
 * gnome_canvas_points_free:
 * @points: A canvas points structure.
 * 
 * Decreases the reference count of the specified points structure.  If it
 * reaches zero, then the structure is freed.
 **/
void
gnome_canvas_points_free (GnomeCanvasPoints *points)
{
	g_return_if_fail (points != NULL);

	points->ref_count -= 1;
	if (points->ref_count == 0) {
		g_free (points->coords);
		g_free (points);
	}
}

/**
 * gnome_canvas_get_miter_points:
 * @x1: X coordinate of the first point
 * @y1: Y coordinate of the first point
 * @x2: X coordinate of the second (angle) point
 * @y2: Y coordinate of the second (angle) point
 * @x3: X coordinate of the third point
 * @y3: Y coordinate of the third point
 * @width: Width of the line
 * @mx1: The X coordinate of the first miter point is returned here.
 * @my1: The Y coordinate of the first miter point is returned here.
 * @mx2: The X coordinate of the second miter point is returned here.
 * @my2: The Y coordinate of the second miter point is returned here.
 * 
 * Given three points forming an angle, computes the coordinates of the inside
 * and outside points of the mitered corner formed by a line of a given width at
 * that angle.
 * 
 * Return value: FALSE if the angle is less than 11 degrees (this is the same
 * threshold as X uses.  If this occurs, the return points are not modified.
 * Otherwise, returns TRUE.
 **/
int
gnome_canvas_get_miter_points (double x1, double y1, double x2, double y2, double x3, double y3,
			       double width,
			       double *mx1, double *my1, double *mx2, double *my2)
{
	double theta1;		/* angle of segment p2-p1 */
	double theta2;		/* angle of segment p2-p3 */
	double theta;		/* angle between line segments */
	double theta3;		/* angle that bisects theta1 and theta2 and points to p1 */
	double dist;		/* distance of miter points from p2 */
	double dx, dy;		/* x and y offsets corresponding to dist */

#define ELEVEN_DEGREES (11.0 * G_PI / 180.0)

	if (y2 == y1)
		theta1 = (x2 < x1) ? 0.0 : G_PI;
	else if (x2 == x1)
		theta1 = (y2 < y1) ? G_PI_2 : -G_PI_2;
	else
		theta1 = atan2 (y1 - y2, x1 - x2);

	if (y3 == y2)
		theta2 = (x3 > x2) ? 0 : G_PI;
	else if (x3 == x2)
		theta2 = (y3 > y2) ? G_PI_2 : -G_PI_2;
	else
		theta2 = atan2 (y3 - y2, x3 - x2);

	theta = theta1 - theta2;

	if (theta > G_PI)
		theta -= 2.0 * G_PI;
	else if (theta < -G_PI)
		theta += 2.0 * G_PI;

	if ((theta < ELEVEN_DEGREES) && (theta > -ELEVEN_DEGREES))
		return FALSE;

	dist = 0.5 * width / sin (0.5 * theta);
	if (dist < 0.0)
		dist = -dist;

	theta3 = (theta1 + theta2) / 2.0;
	if (sin (theta3 - (theta1 + G_PI)) < 0.0)
		theta3 += G_PI;

	dx = dist * cos (theta3);
	dy = dist * sin (theta3);

	*mx1 = x2 + dx;
	*mx2 = x2 - dx;
	*my1 = y2 + dy;
	*my2 = y2 - dy;

	return TRUE;
}

/**
 * gnome_canvas_get_butt_points:
 * @x1: X coordinate of first point in the line
 * @y1: Y cooordinate of first point in the line
 * @x2: X coordinate of second point (endpoint) of the line
 * @y2: Y coordinate of second point (endpoint) of the line
 * @width: Width of the line
 * @project: Whether the butt points should project out by width/2 distance
 * @bx1: X coordinate of first butt point is returned here
 * @by1: Y coordinate of first butt point is returned here
 * @bx2: X coordinate of second butt point is returned here
 * @by2: Y coordinate of second butt point is returned here
 * 
 * Computes the butt points of a line segment.
 **/
void
gnome_canvas_get_butt_points (double x1, double y1, double x2, double y2,
			      double width, int project,
			      double *bx1, double *by1, double *bx2, double *by2)
{
	double length;
	double dx, dy;

	width *= 0.5;
	dx = x2 - x1;
	dy = y2 - y1;
	length = sqrt (dx * dx + dy * dy);

	if (length < GNOME_CANVAS_EPSILON) {
		*bx1 = *bx2 = x2;
		*by1 = *by2 = y2;
	} else {
		dx = -width * (y2 - y1) / length;
		dy = width * (x2 - x1) / length;

		*bx1 = x2 + dx;
		*bx2 = x2 - dx;
		*by1 = y2 + dy;
		*by2 = y2 - dy;

		if (project) {
			*bx1 += dy;
			*bx2 += dy;
			*by1 -= dx;
			*by2 -= dx;
		}
	}
}

/**
 * gnome_canvas_polygon_to_point:
 * @poly: Vertices of the polygon.  X coordinates are in the even indices, and Y
 * coordinates are in the odd indices
 * @num_points: Number of points in the polygon
 * @x: X coordinate of the point
 * @y: Y coordinate of the point
 * 
 * Computes the distance between a point and a polygon.
 * 
 * Return value: The distance from the point to the polygon, or zero if the
 * point is inside the polygon.
 **/
double
gnome_canvas_polygon_to_point (double *poly, int num_points, double x, double y)
{
	double best;
	int intersections;
	int i;
	double *p;
	double dx, dy;

	/* Iterate through all the edges in the polygon, updating best and intersections.
	 *
	 * When computing intersections, include left X coordinate of line within its range, but not
	 * Y coordinate.  Otherwise if the point lies exactly below a vertex we'll count it as two
	 * intersections.
	 */

	best = 1.0e36;
	intersections = 0;

	for (i = num_points, p = poly; i > 1; i--, p += 2) {
		double px, py, dist;

		/* Compute the point on the current edge closest to the point and update the
		 * intersection count.  This must be done separately for vertical edges, horizontal
		 * edges, and others.
		 */

		if (p[2] == p[0]) {
			/* Vertical edge */

			px = p[0];

			if (p[1] >= p[3]) {
				py = MIN (p[1], y);
				py = MAX (py, p[3]);
			} else {
				py = MIN (p[3], y);
				py = MAX (py, p[1]);
			}
		} else if (p[3] == p[1]) {
			/* Horizontal edge */

			py = p[1];

			if (p[0] >= p[2]) {
				px = MIN (p[0], x);
				px = MAX (px, p[2]);

				if ((y < py) && (x < p[0]) && (x >= p[2]))
					intersections++;
			} else {
				px = MIN (p[2], x);
				px = MAX (px, p[0]);

				if ((y < py) && (x < p[2]) && (x >= p[0]))
					intersections++;
			}
		} else {
			double m1, b1, m2, b2;
			int lower;

			/* Diagonal edge.  Convert the edge to a line equation (y = m1*x + b1), then
			 * compute a line perpendicular to this edge but passing through the point,
			 * (y = m2*x + b2).
			 */

			m1 = (p[3] - p[1]) / (p[2] - p[0]);
			b1 = p[1] - m1 * p[0];

			m2 = -1.0 / m1;
			b2 = y - m2 * x;

			px = (b2 - b1) / (m1 - m2);
			py = m1 * px + b1;

			if (p[0] > p[2]) {
				if (px > p[0]) {
					px = p[0];
					py = p[1];
				} else if (px < p[2]) {
					px = p[2];
					py = p[3];
				}
			} else {
				if (px > p[2]) {
					px = p[2];
					py = p[3];
				} else if (px < p[0]) {
					px = p[0];
					py = p[1];
				}
			}

			lower = (m1 * x + b1) > y;

			if (lower && (x >= MIN (p[0], p[2])) && (x < MAX (p[0], p[2])))
				intersections++;
		}

		/* Compute the distance to the closest point, and see if that is the best so far */

		dx = x - px;
		dy = y - py;
		dist = sqrt (dx * dx + dy * dy);
		if (dist < best)
			best = dist;
	}

	/* We've processed all the points.  If the number of intersections is odd, the point is
	 * inside the polygon.
	 */

	if (intersections & 0x1)
		return 0.0;
	else
		return best;
}

/* Here are some helper functions for aa rendering: */

/**
 * gnome_canvas_render_svp:
 * @buf: the canvas buffer to render over
 * @svp: the vector path to render
 * @rgba: the rgba color to render
 *
 * Render the svp over the buf.
 **/
void
gnome_canvas_render_svp (GnomeCanvasBuf *buf, ArtSVP *svp, guint32 rgba)
{
	guint32 fg_color, bg_color;
	int alpha;

	if (buf->is_bg) {
		bg_color = buf->bg_color;
		alpha = rgba & 0xff;
		if (alpha == 0xff)
			fg_color = rgba >> 8;
		else {
			/* composite over background color */
			int bg_r, bg_g, bg_b;
			int fg_r, fg_g, fg_b;
			int tmp;

			bg_r = (bg_color >> 16) & 0xff;
			fg_r = (rgba >> 24) & 0xff;
			tmp = (fg_r - bg_r) * alpha;
			fg_r = bg_r + ((tmp + (tmp >> 8) + 0x80) >> 8);

			bg_g = (bg_color >> 8) & 0xff;
			fg_g = (rgba >> 16) & 0xff;
			tmp = (fg_g - bg_g) * alpha;
			fg_g = bg_g + ((tmp + (tmp >> 8) + 0x80) >> 8);

			bg_b = bg_color & 0xff;
			fg_b = (rgba >> 8) & 0xff;
			tmp = (fg_b - bg_b) * alpha;
			fg_b = bg_b + ((tmp + (tmp >> 8) + 0x80) >> 8);

			fg_color = (fg_r << 16) | (fg_g << 8) | fg_b;
		}
		art_rgb_svp_aa (svp,
				buf->rect.x0, buf->rect.y0, buf->rect.x1, buf->rect.y1,
				fg_color, bg_color,
				buf->buf, buf->buf_rowstride,
				NULL);
		buf->is_bg = 0;
		buf->is_buf = 1;
	} else {
		art_rgb_svp_alpha (svp,
				   buf->rect.x0, buf->rect.y0, buf->rect.x1, buf->rect.y1,
				   rgba,
				   buf->buf, buf->buf_rowstride,
				   NULL);
	}
}

/**
 * gnome_canvas_update_svp:
 * @canvas: the canvas containing the svp that needs updating.
 * @p_svp: a pointer to the existing svp
 * @new_svp: the new svp
 *
 * Sets the svp to the new value, requesting repaint on what's changed. This
 * function takes responsibility for freeing new_svp.
 **/
void
gnome_canvas_update_svp (GnomeCanvas *canvas, ArtSVP **p_svp, ArtSVP *new_svp)
{
	ArtSVP *old_svp;
	ArtSVP *diff G_GNUC_UNUSED;
	ArtUta *repaint_uta;

	old_svp = *p_svp;

	if (old_svp != NULL) {
		ArtDRect bb;
		art_drect_svp (&bb, old_svp);
		if ((bb.x1 - bb.x0) * (bb.y1 - bb.y0) > (64 * 64)) {
			repaint_uta = art_uta_from_svp (old_svp);
			gnome_canvas_request_redraw_uta (canvas, repaint_uta);
		} else {
			ArtIRect ib;
			art_drect_to_irect (&ib, &bb);
			gnome_canvas_request_redraw (canvas, ib.x0, ib.y0, ib.x1, ib.y1);
		}
		art_svp_free (old_svp);
	}

	if (new_svp != NULL) {
		ArtDRect bb;
		art_drect_svp (&bb, new_svp);
		if ((bb.x1 - bb.x0) * (bb.y1 - bb.y0) > (64 * 64)) {
			repaint_uta = art_uta_from_svp (new_svp);
			gnome_canvas_request_redraw_uta (canvas, repaint_uta);
		} else {
			ArtIRect ib;
			art_drect_to_irect (&ib, &bb);
			gnome_canvas_request_redraw (canvas, ib.x0, ib.y0, ib.x1, ib.y1);
		}
	}

	*p_svp = new_svp;
}

/**
 * gnome_canvas_update_svp_clip:
 * @canvas: the canvas containing the svp that needs updating.
 * @p_svp: a pointer to the existing svp
 * @new_svp: the new svp
 * @clip_svp: a clip path, if non-null
 *
 * Sets the svp to the new value, clipping if necessary, and requesting repaint
 * on what's changed. This function takes responsibility for freeing new_svp.
 **/
void
gnome_canvas_update_svp_clip (GnomeCanvas *canvas, ArtSVP **p_svp, ArtSVP *new_svp, ArtSVP *clip_svp)
{
	ArtSVP *clipped_svp;

	if (clip_svp != NULL) {
		clipped_svp = art_svp_intersect (new_svp, clip_svp);
		art_svp_free (new_svp);
	} else {
		clipped_svp = new_svp;
	}
	gnome_canvas_update_svp (canvas, p_svp, clipped_svp);
}

/**
 * gnome_canvas_item_reset_bounds:
 * @item: A canvas item
 * 
 * Resets the bounding box of a canvas item to an empty rectangle.
 **/
void
gnome_canvas_item_reset_bounds (GnomeCanvasItem *item)
{
	item->x1 = 0.0;
	item->y1 = 0.0;
	item->x2 = 0.0;
	item->y2 = 0.0;
}

/**
 * gnome_canvas_item_update_svp:
 * @item: the canvas item containing the svp that needs updating.
 * @p_svp: a pointer to the existing svp
 * @new_svp: the new svp
 *
 * Sets the svp to the new value, requesting repaint on what's changed. This
 * function takes responsibility for freeing new_svp. This routine also adds the
 * svp's bbox to the item's.
 **/
void
gnome_canvas_item_update_svp (GnomeCanvasItem *item, ArtSVP **p_svp, ArtSVP *new_svp)
{
	ArtDRect bbox;

	gnome_canvas_update_svp (item->canvas, p_svp, new_svp);
	if (new_svp) {
		bbox.x0 = item->x1;
		bbox.y0 = item->y1;
		bbox.x1 = item->x2;
		bbox.y1 = item->y2;
		art_drect_svp_union (&bbox, new_svp);
		item->x1 = bbox.x0;
		item->y1 = bbox.y0;
		item->x2 = bbox.x1;
		item->y2 = bbox.y1;
	}
}

/**
 * gnome_canvas_item_update_svp_clip:
 * @item: the canvas item containing the svp that needs updating.
 * @p_svp: a pointer to the existing svp
 * @new_svp: the new svp
 * @clip_svp: a clip path, if non-null
 *
 * Sets the svp to the new value, clipping if necessary, and requesting repaint
 * on what's changed. This function takes responsibility for freeing new_svp.
 **/
void
gnome_canvas_item_update_svp_clip (GnomeCanvasItem *item, ArtSVP **p_svp, ArtSVP *new_svp,
				   ArtSVP *clip_svp)
{
	ArtSVP *clipped_svp;

	if (clip_svp != NULL) {
		clipped_svp = art_svp_intersect (new_svp, clip_svp);
		art_svp_free (new_svp);
	} else {
		clipped_svp = new_svp;
	}

	gnome_canvas_item_update_svp (item, p_svp, clipped_svp);
}

/**
 * gnome_canvas_item_request_redraw_svp
 * @item: the item containing the svp
 * @svp: the svp that needs to be redrawn
 *
 * Request redraw of the svp if in aa mode, or the entire item in in xlib mode.
 **/ 
void
gnome_canvas_item_request_redraw_svp (GnomeCanvasItem *item, const ArtSVP *svp)
{
	GnomeCanvas *canvas;
	ArtUta *uta;

	canvas = item->canvas;
	if (canvas->aa) {
		if (svp != NULL) {
			uta = art_uta_from_svp (svp);
			gnome_canvas_request_redraw_uta (canvas, uta);
		}
	} else {
		gnome_canvas_request_redraw (canvas, item->x1, item->y1, item->x2, item->y2);		
	}
}

/**
 * gnome_canvas_update_bbox:
 * @item: the canvas item needing update
 * @x1: Left coordinate of the new bounding box
 * @y1: Top coordinate of the new bounding box
 * @x2: Right coordinate of the new bounding box
 * @y2: Bottom coordinate of the new bounding box
 *
 * Sets the bbox to the new value, requesting full repaint.
 **/
void
gnome_canvas_update_bbox (GnomeCanvasItem *item, int x1, int y1, int x2, int y2)
{
	gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);
	item->x1 = x1;
	item->y1 = y1;
	item->x2 = x2;
	item->y2 = y2;
	gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);
}

/**
 * gnome_canvas_buf_ensure_buf:
 * @buf: the buf that needs to be represened in RGB format
 *
 * Ensure that the buffer is in RGB format, suitable for compositing.
 **/
void
gnome_canvas_buf_ensure_buf (GnomeCanvasBuf *buf)
{
	guchar *bufptr;
	int y;

	if (!buf->is_buf) {
		bufptr = buf->buf;
		for (y = buf->rect.y0; y < buf->rect.y1; y++) {
			art_rgb_fill_run (bufptr,
					  buf->bg_color >> 16,
					  (buf->bg_color >> 8) & 0xff,
					  buf->bg_color & 0xff,
					  buf->rect.x1 - buf->rect.x0);
			bufptr += buf->buf_rowstride;
		}
		buf->is_buf = 1;
	}
}

/**
 * gnome_canvas_join_gdk_to_art
 * @gdk_join: a join type, represented in GDK format
 *
 * Convert from GDK line join specifier to libart.
 *
 * Return value: The line join specifier in libart format.
 **/
ArtPathStrokeJoinType
gnome_canvas_join_gdk_to_art (GdkJoinStyle gdk_join)
{
	switch (gdk_join) {
	case GDK_JOIN_MITER:
		return ART_PATH_STROKE_JOIN_MITER;

	case GDK_JOIN_ROUND:
		return ART_PATH_STROKE_JOIN_ROUND;

	case GDK_JOIN_BEVEL:
		return ART_PATH_STROKE_JOIN_BEVEL;

	default:
		g_assert_not_reached ();
		return ART_PATH_STROKE_JOIN_MITER; /* shut up the compiler */
	}
}

/**
 * gnome_canvas_cap_gdk_to_art
 * @gdk_cap: a cap type, represented in GDK format
 *
 * Convert from GDK line cap specifier to libart.
 *
 * Return value: The line cap specifier in libart format.
 **/
ArtPathStrokeCapType
gnome_canvas_cap_gdk_to_art (GdkCapStyle gdk_cap)
{
	switch (gdk_cap) {
	case GDK_CAP_BUTT:
	case GDK_CAP_NOT_LAST:
		return ART_PATH_STROKE_CAP_BUTT;

	case GDK_CAP_ROUND:
		return ART_PATH_STROKE_CAP_ROUND;

	case GDK_CAP_PROJECTING:
		return ART_PATH_STROKE_CAP_SQUARE;

	default:
		g_assert_not_reached ();
		return ART_PATH_STROKE_CAP_BUTT; /* shut up the compiler */
	}
}
