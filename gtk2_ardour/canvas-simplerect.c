#include <stdio.h>
#include <math.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include "canvas-simplerect.h"
#include "rgb_macros.h"

enum {
	ARG_0,
	ARG_X1,
	ARG_Y1,
	ARG_X2,
	ARG_Y2,
	ARG_OUTLINE_PIXELS,
	ARG_OUTLINE_WHAT,
	ARG_FILL,
	ARG_FILL_COLOR_RGBA,
	ARG_OUTLINE_COLOR_RGBA,
	ARG_DRAW
	
};

static void gnome_canvas_simplerect_class_init (GnomeCanvasSimpleRectClass *class);
static void gnome_canvas_simplerect_init       (GnomeCanvasSimpleRect      *simplerect);
static void gnome_canvas_simplerect_set_arg    (GtkObject              *object,
					      GtkArg                 *arg,
					      guint                   arg_id);
static void gnome_canvas_simplerect_get_arg    (GtkObject              *object,
					      GtkArg                 *arg,
					      guint                   arg_id);

static void   gnome_canvas_simplerect_update      (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags);
static void   gnome_canvas_simplerect_bounds      (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2);
static double gnome_canvas_simplerect_point (GnomeCanvasItem *item, double x, double y, int cx, int cy, GnomeCanvasItem **actual_item);
static void   gnome_canvas_simplerect_render (GnomeCanvasItem *item, GnomeCanvasBuf *buf);
static void   gnome_canvas_simplerect_draw (GnomeCanvasItem *item, GdkDrawable *drawable, int x, int y, int w, int h);

static GnomeCanvasItemClass *parent_class;


GtkType
gnome_canvas_simplerect_get_type (void)
{
	static GtkType simplerect_type = 0;

	if (!simplerect_type) {
		GtkTypeInfo simplerect_info = {
			"GnomeCanvasSimpleRect",
			sizeof (GnomeCanvasSimpleRect),
			sizeof (GnomeCanvasSimpleRectClass),
			(GtkClassInitFunc) gnome_canvas_simplerect_class_init,
			(GtkObjectInitFunc) gnome_canvas_simplerect_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		simplerect_type = gtk_type_unique (gnome_canvas_item_get_type (), &simplerect_info);
	}

	return simplerect_type;
}

static void
gnome_canvas_simplerect_class_init (GnomeCanvasSimpleRectClass *class)
{
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	parent_class = gtk_type_class (gnome_canvas_item_get_type ());

	gtk_object_add_arg_type ("GnomeCanvasSimpleRect::x1", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_X1);
	gtk_object_add_arg_type ("GnomeCanvasSimpleRect::y1", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_Y1);
	gtk_object_add_arg_type ("GnomeCanvasSimpleRect::x2", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_X2);
	gtk_object_add_arg_type ("GnomeCanvasSimpleRect::y2", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_Y2);
	gtk_object_add_arg_type ("GnomeCanvasSimpleRect::fill", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_FILL);
	gtk_object_add_arg_type ("GnomeCanvasSimpleRect::draw", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_DRAW);
	gtk_object_add_arg_type ("GnomeCanvasSimpleRect::fill_color_rgba", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_FILL_COLOR_RGBA);
	gtk_object_add_arg_type ("GnomeCanvasSimpleRect::outline_color_rgba", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_OUTLINE_COLOR_RGBA);
	gtk_object_add_arg_type ("GnomeCanvasSimpleRect::outline_pixels", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_OUTLINE_PIXELS);
	gtk_object_add_arg_type ("GnomeCanvasSimpleRect::outline_what", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_OUTLINE_WHAT);

	object_class->set_arg = gnome_canvas_simplerect_set_arg;
	object_class->get_arg = gnome_canvas_simplerect_get_arg;

	item_class->update = gnome_canvas_simplerect_update;
	item_class->bounds = gnome_canvas_simplerect_bounds;
	item_class->point = gnome_canvas_simplerect_point;
	item_class->render = gnome_canvas_simplerect_render;
	item_class->draw = gnome_canvas_simplerect_draw;
}

static void
gnome_canvas_simplerect_init (GnomeCanvasSimpleRect *simplerect)
{
	simplerect->x1 = 0.0;
	simplerect->y1 = 0.0;
	simplerect->x2 = 0.0;
	simplerect->y2 = 0.0;
	simplerect->fill = TRUE;
	simplerect->draw = TRUE;
	simplerect->full_draw_on_update = TRUE;
	simplerect->fill_color = 0;
	simplerect->outline_color = 0;
	simplerect->outline_pixels = 1;
	simplerect->outline_what = 0xf;

	// GTK2FIX
	// GNOME_CANVAS_ITEM(simplerect)->object.flags |= GNOME_CANVAS_ITEM_NO_AUTO_REDRAW;
}

static void
gnome_canvas_simplerect_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	GnomeCanvasSimpleRect *simplerect = GNOME_CANVAS_SIMPLERECT (item);

	*x1 = simplerect->x1;
	*y1 = simplerect->y1;
	*x2 = simplerect->x2 + 1;
	*y2 = simplerect->y2 + 1;

}

static void 
gnome_canvas_simplerect_reset_bounds (GnomeCanvasItem *item)
{
	GnomeCanvasSimpleRect* simplerect;
	double x1, x2, y1, y2;
	double old_x1, old_x2, old_y1, old_y2;
	double a, b;
	
	old_x1 = item->x1;
	old_y1 = item->y1;
	old_x2 = item->x2;
	old_y2 = item->y2;

	gnome_canvas_simplerect_bounds (item, &x1, &y1, &x2, &y2);
	gnome_canvas_item_i2w (item, &x1, &y1);
	gnome_canvas_item_i2w (item, &x2, &y2);

	item->x1 = x1;
	item->y1 = y1;
	item->x2 = x2;
	item->y2 = y2;

	/* now compute bounding box in canvas units */

	simplerect = GNOME_CANVAS_SIMPLERECT (item);

	gnome_canvas_w2c (GNOME_CANVAS(item->canvas), x1, y1, &simplerect->bbox_ulx, &simplerect->bbox_uly);
	gnome_canvas_w2c (GNOME_CANVAS(item->canvas), x2, y2, &simplerect->bbox_lrx, &simplerect->bbox_lry);

	/* now queue redraws for changed areas */

	if (item->x1 != old_x1) {
		
		/* left edge changed. redraw the area that altered */
		
		a = MIN(item->x1, old_x1); 
		b = MAX(item->x1, old_x1);
		gnome_canvas_request_redraw (item->canvas, a - 1, item->y1, b + 1, item->y2);
	}
	
	if (item->x2 != old_x2) {
		
		/* right edge changed. redraw the area that altered */
		
		a = MIN(item->x2, old_x2);
		b = MAX(item->x2, old_x2);
		gnome_canvas_request_redraw (item->canvas, a - 1, item->y1, b + 1, item->y2);
	}
	
	if (item->y1 != old_y1) {
		
		/* top edge changed. redraw the area that altered */
		
		a = MIN(item->y1, old_y1);
		b = MAX(item->y1, old_y1);
		gnome_canvas_request_redraw (item->canvas, item->x1, a - 1, item->x2, b + 1);
	}
	
	if (item->y2 != old_y2) {
		
		/* lower edge changed. redraw the area that altered */
		
		a = MIN(item->y2, old_y2);
		b = MAX(item->y2, old_y2);
		gnome_canvas_request_redraw (item->canvas, item->x1, a - 1, item->x2, b + 1);
	}
}

/* 
 * CANVAS CALLBACKS 
 */

static void
gnome_canvas_simplerect_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	GnomeCanvasSimpleRect *simplerect;
	int update;
	int bounds_changed;

	item = GNOME_CANVAS_ITEM (object);
	simplerect = GNOME_CANVAS_SIMPLERECT (object);

	update = FALSE;
	bounds_changed = FALSE;

	switch (arg_id) {
	case ARG_X1:
	        if (simplerect->x1 != GTK_VALUE_DOUBLE (*arg)) {
		        simplerect->x1 = GTK_VALUE_DOUBLE (*arg);
			bounds_changed = TRUE;
		}
		break;

	case ARG_Y1:
	        if (simplerect->y1 != GTK_VALUE_DOUBLE (*arg)) {
		        simplerect->y1 = GTK_VALUE_DOUBLE (*arg);
			bounds_changed = TRUE;
		}
		break;

	case ARG_X2:
	        if (simplerect->x2 != GTK_VALUE_DOUBLE (*arg)) {
		        simplerect->x2 = GTK_VALUE_DOUBLE (*arg);
			bounds_changed = TRUE;
		}
		break;

	case ARG_Y2:
	        if (simplerect->y2 != GTK_VALUE_DOUBLE (*arg)) {
		        simplerect->y2 = GTK_VALUE_DOUBLE (*arg);
			bounds_changed = TRUE;
		}
		break;

	case ARG_DRAW:
	        if (simplerect->draw != GTK_VALUE_BOOL (*arg)) {
		        simplerect->draw = GTK_VALUE_BOOL (*arg);
			update = TRUE;
		}
		break;


	case ARG_FILL:
	        if (simplerect->fill != GTK_VALUE_BOOL (*arg)) {
		        simplerect->fill = GTK_VALUE_BOOL (*arg);
			update = TRUE;
		}
		break;

	case ARG_FILL_COLOR_RGBA:
		if (simplerect->fill_color != GTK_VALUE_INT(*arg)) {
			simplerect->fill_color = GTK_VALUE_INT(*arg);
			update = TRUE;
		}
		break;

	case ARG_OUTLINE_COLOR_RGBA:
		if (simplerect->outline_color != GTK_VALUE_INT(*arg)) {
			simplerect->outline_color = GTK_VALUE_INT(*arg);
			update = TRUE;
		}
		break;

	case ARG_OUTLINE_PIXELS:
		if (simplerect->outline_pixels != GTK_VALUE_INT(*arg)) {
			simplerect->outline_pixels = GTK_VALUE_INT(*arg);
			update = TRUE;
		}
		break;

	case ARG_OUTLINE_WHAT:
		if (simplerect->outline_what != GTK_VALUE_INT(*arg)) {
			simplerect->outline_what = GTK_VALUE_INT(*arg);
			update = TRUE;
		}
		break;

	default:
		break;
	}

	simplerect->full_draw_on_update = update;

	if (update || bounds_changed) {
		gnome_canvas_item_request_update (item);
	}
}

static void
gnome_canvas_simplerect_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GnomeCanvasSimpleRect *simplerect;

	simplerect = GNOME_CANVAS_SIMPLERECT (object);

	switch (arg_id) {
	case ARG_X1:
		GTK_VALUE_DOUBLE (*arg) = simplerect->x1;
		break;
	case ARG_Y1:
		GTK_VALUE_DOUBLE (*arg) = simplerect->y1;
		break;
	case ARG_X2:
		GTK_VALUE_DOUBLE (*arg) = simplerect->x2;
		break;
	case ARG_Y2:
		GTK_VALUE_DOUBLE (*arg) = simplerect->y2;
		break;
	case ARG_DRAW:
		GTK_VALUE_BOOL (*arg) = simplerect->draw;
		break;
	case ARG_FILL:
		GTK_VALUE_BOOL (*arg) = simplerect->fill;
		break;
	case ARG_FILL_COLOR_RGBA:
		GTK_VALUE_INT (*arg) = simplerect->fill_color;
		break;
	case ARG_OUTLINE_COLOR_RGBA:
		GTK_VALUE_INT (*arg) = simplerect->outline_color;
		break;
	case ARG_OUTLINE_PIXELS:
		GTK_VALUE_INT (*arg) = simplerect->outline_pixels;
		break;
	case ARG_OUTLINE_WHAT:
		GTK_VALUE_INT (*arg) = simplerect->outline_what;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
gnome_canvas_simplerect_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	GnomeCanvasSimpleRect *simplerect;
	unsigned char foo;

	simplerect = GNOME_CANVAS_SIMPLERECT (item);

	if (parent_class->update)
		(* parent_class->update) (item, affine, clip_path, flags);

	gnome_canvas_simplerect_reset_bounds (item);

	if (simplerect->full_draw_on_update) {
		gnome_canvas_request_redraw (item->canvas, 
					   simplerect->bbox_ulx,
					   simplerect->bbox_uly,
					   simplerect->bbox_lrx+1,
					   simplerect->bbox_lry+1);
		simplerect->full_draw_on_update = FALSE;
	}

	UINT_TO_RGBA (simplerect->fill_color, &simplerect->fill_r, &simplerect->fill_g, &simplerect->fill_b, &simplerect->fill_a);
	UINT_TO_RGBA (simplerect->outline_color, &simplerect->outline_r, &simplerect->outline_g, &simplerect->outline_b, &foo);
}

#define SIMPLERECT_FAST_RENDERER
#ifdef SIMPLERECT_FAST_RENDERER

static void
gnome_canvas_simplerect_render (GnomeCanvasItem *item,
			      GnomeCanvasBuf *buf)
{
	GnomeCanvasSimpleRect *simplerect;
	int end, begin;
	int ey, sy;
	unsigned int i;
	ArtIRect intersection;
	ArtIRect self;

	simplerect = GNOME_CANVAS_SIMPLERECT (item);

	if (parent_class->render) {
		(*parent_class->render) (item, buf);
	}

	if (buf->is_bg) {

		// this can be useful for debugging/understanding how the canvas redraws
		// stuff.

		// gint randr, randg, randb;
		// randr = random() % 255;
		// randg = random() % 255;
		// randb = random() % 255;
		// PAINT_BOX(buf, randr, randg, randb, 255, buf->rect.x0, buf->rect.y0, buf->rect.x1, buf->rect.y1);

		gnome_canvas_buf_ensure_buf (buf);
		buf->is_bg = FALSE;
	}

	if (!simplerect->draw) {
		return;
	}
	
	self.x0 = simplerect->bbox_ulx;
	self.y0 = simplerect->bbox_uly;
	self.x1 = simplerect->bbox_lrx;
	self.y1 = simplerect->bbox_lry;

	art_irect_intersect (&intersection, &self, &buf->rect);

	begin = MAX(simplerect->bbox_ulx, buf->rect.x0);
	end = MIN((simplerect->bbox_lrx-1), buf->rect.x1);

	sy = simplerect->bbox_uly;
	ey = simplerect->bbox_lry-1;

	if (simplerect->fill) {
		
		// this can be useful for debugging/understanding how the canvas redraws
		// stuff.
		
		// gint randr, randg, randb;
		// randr = random() % 255;
		// randg = random() % 255;
		// randb = random() % 255;
		// PAINT_BOX(buf, randr, randg, randb, simplerect->fill_a, begin, sy, end, ey);
		
		FAST_PAINT_BOX (buf, simplerect->fill_r, simplerect->fill_g, simplerect->fill_b, simplerect->fill_a, 
				intersection.x0, intersection.y0,
				intersection.x1, intersection.y1);
	 	
	}

	for (i = 0; i < simplerect->outline_pixels; ++i) {

		if (simplerect->outline_what & 0x1) {
			if (begin == simplerect->bbox_ulx) {
				PAINT_VERT(buf, simplerect->outline_r, simplerect->outline_g, simplerect->outline_b, begin + i, sy, ey);
			}
		}

		if (simplerect->outline_what & 0x2) {
			if (end == (simplerect->bbox_lrx - 1)) {
				PAINT_VERT(buf, simplerect->outline_r, simplerect->outline_g, simplerect->outline_b, end - i, sy, ey + 1);
			}
		}

		if (simplerect->outline_what & 0x4) {
			PAINT_HORIZ(buf, simplerect->outline_r, simplerect->outline_g, simplerect->outline_b, begin, end, sy+i);
		}
	
		if (simplerect->outline_what & 0x8) {
			PAINT_HORIZ(buf, simplerect->outline_r, simplerect->outline_g, simplerect->outline_b, begin, end + 1, ey-i);
		}
	}
}

#else /* SIMPLERECT_FAST_RENDERER */

static void
gnome_canvas_simplerect_render (GnomeCanvasItem *item,
			      GnomeCanvasBuf *buf)
{
	GnomeCanvasSimpleRect *simplerect;
	int end, begin;
	int ey, sy;
	unsigned int i;

	simplerect = GNOME_CANVAS_SIMPLERECT (item);

	if (parent_class->render) {
		(*parent_class->render) (item, buf);
	}

	if (buf->is_bg) {

		// this can be useful for debugging/understanding how the canvas redraws
		// stuff.

		// gint randr, randg, randb;
		// randr = random() % 255;
		// randg = random() % 255;
		// randb = random() % 255;
		// PAINT_BOX(buf, randr, randg, randb, 255, buf->rect.x0, buf->rect.y0, buf->rect.x1, buf->rect.y1);

		gnome_canvas_buf_ensure_buf (buf);
		buf->is_bg = FALSE;
	}

	if (!simplerect->draw) {
		return;
	}

	begin = MAX(simplerect->bbox_ulx,buf->rect.x0);
	end = MIN((simplerect->bbox_lrx-1),buf->rect.x1);

	sy = simplerect->bbox_uly;
	ey = simplerect->bbox_lry-1;

	if (simplerect->fill) {
		
		// this can be useful for debugging/understanding how the canvas redraws
		// stuff.
		
		// gint randr, randg, randb;
		// randr = random() % 255;
		// randg = random() % 255;
		// randb = random() % 255;
		// PAINT_BOX(buf, randr, randg, randb, simplerect->fill_a, begin, sy, end, ey);
		
		PAINT_BOX(buf, simplerect->fill_r, simplerect->fill_g, simplerect->fill_b, simplerect->fill_a, begin, sy, end, ey);
	}

	for (i = 0; i < simplerect->outline_pixels; ++i) {

		if (simplerect->outline_what & 0x1) {
			if (begin == simplerect->bbox_ulx) {
				PAINT_VERT(buf, simplerect->outline_r, simplerect->outline_g, simplerect->outline_b, begin + i, sy, ey);
			}
		}

		if (simplerect->outline_what & 0x2) {
			if (end == (simplerect->bbox_lrx - 1)) {
				PAINT_VERT(buf, simplerect->outline_r, simplerect->outline_g, simplerect->outline_b, end - i, sy, ey + 1);
			}
		}

		if (simplerect->outline_what & 0x4) {
			PAINT_HORIZ(buf, simplerect->outline_r, simplerect->outline_g, simplerect->outline_b, begin, end, sy+i);
		}
	
		if (simplerect->outline_what & 0x8) {
			PAINT_HORIZ(buf, simplerect->outline_r, simplerect->outline_g, simplerect->outline_b, begin, end + 1, ey-i);
		}
	}
}
#endif /* SIMPLERECT_FAST_RENDERER */

static void
gnome_canvas_simplerect_draw (GnomeCanvasItem *item,
			    GdkDrawable *drawable,
			    int x, int y,
			    int width, int height)
{
	fprintf (stderr, "please don't use the CanvasSimpleRect item in a non-aa Canvas\n");
	abort ();
}

static double
gnome_canvas_simplerect_point (GnomeCanvasItem *item, double x, double y, int cx, int cy, GnomeCanvasItem **actual_item)
{
	GnomeCanvasSimpleRect *simplerect;
	double x1, y1, x2, y2;
	double dx, dy;

	simplerect = GNOME_CANVAS_SIMPLERECT (item);

	*actual_item = item;

	/* Find the bounds for the rectangle plus its outline width */

	gnome_canvas_simplerect_bounds (item, &x1, &y1, &x2, &y2);

	/* Is point inside rectangle */
	
	if ((x >= x1) && (y >= y1) && (x <= x2) && (y <= y2)) {
		return 0.0;
	}

	/* Point is outside rectangle */

	if (x < x1)
		dx = x1 - x;
	else if (x > x2)
		dx = x - x2;
	else
		dx = 0.0;

	if (y < y1)
		dy = y1 - y;
	else if (y > y2)
		dy = y - y2;
	else
		dy = 0.0;

	return sqrt (dx * dx + dy * dy);
}
