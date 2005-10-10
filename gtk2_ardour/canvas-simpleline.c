#include <stdio.h>
#include <math.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include "canvas-simpleline.h"
#include "rgb_macros.h"

enum {
	ARG_0,
	ARG_X1,
	ARG_Y1,
	ARG_X2,
	ARG_Y2,
	ARG_COLOR_RGBA
};

static void gnome_canvas_simpleline_class_init (GnomeCanvasSimpleLineClass *class);
static void gnome_canvas_simpleline_init       (GnomeCanvasSimpleLine      *simpleline);
static void gnome_canvas_simpleline_set_arg    (GtkObject              *object,
					      GtkArg                 *arg,
					      guint                   arg_id);
static void gnome_canvas_simpleline_get_arg    (GtkObject              *object,
					      GtkArg                 *arg,
					      guint                   arg_id);

static void   gnome_canvas_simpleline_update      (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags);
static void   gnome_canvas_simpleline_bounds      (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2);
static double gnome_canvas_simpleline_point (GnomeCanvasItem *item, double x, double y, int cx, int cy, GnomeCanvasItem **actual_item);
static void   gnome_canvas_simpleline_render (GnomeCanvasItem *item, GnomeCanvasBuf *buf);
static void   gnome_canvas_simpleline_draw (GnomeCanvasItem *item, GdkDrawable *drawable, int x, int y, int w, int h);

static GnomeCanvasItemClass *parent_class;


GtkType
gnome_canvas_simpleline_get_type (void)
{
	static GtkType simpleline_type = 0;

	if (!simpleline_type) {
		GtkTypeInfo simpleline_info = {
			"GnomeCanvasSimpleLine",
			sizeof (GnomeCanvasSimpleLine),
			sizeof (GnomeCanvasSimpleLineClass),
			(GtkClassInitFunc) gnome_canvas_simpleline_class_init,
			(GtkObjectInitFunc) gnome_canvas_simpleline_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		simpleline_type = gtk_type_unique (gnome_canvas_item_get_type (), &simpleline_info);
	}

	return simpleline_type;
}

static void
gnome_canvas_simpleline_class_init (GnomeCanvasSimpleLineClass *class)
{
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	parent_class = gtk_type_class (gnome_canvas_item_get_type ());

	gtk_object_add_arg_type ("GnomeCanvasSimpleLine::x1", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_X1);
	gtk_object_add_arg_type ("GnomeCanvasSimpleLine::y1", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_Y1);
	gtk_object_add_arg_type ("GnomeCanvasSimpleLine::x2", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_X2);
	gtk_object_add_arg_type ("GnomeCanvasSimpleLine::y2", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_Y2);
	gtk_object_add_arg_type ("GnomeCanvasSimpleLine::color_rgba", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_COLOR_RGBA);

	object_class->set_arg = gnome_canvas_simpleline_set_arg;
	object_class->get_arg = gnome_canvas_simpleline_get_arg;

	item_class->update = gnome_canvas_simpleline_update;
	item_class->bounds = gnome_canvas_simpleline_bounds;
	item_class->point = gnome_canvas_simpleline_point;
	item_class->render = gnome_canvas_simpleline_render;
	item_class->draw = gnome_canvas_simpleline_draw;
}

static void
gnome_canvas_simpleline_init (GnomeCanvasSimpleLine *simpleline)
{
	simpleline->x1 = 0.0;
	simpleline->y1 = 0.0;
	simpleline->x2 = 0.0;
	simpleline->y2 = 0.0;
	simpleline->color = RGBA_TO_UINT(98,123,174,241);
	simpleline->horizontal = TRUE; /* reset in the _update() method */
	// GTK2FIX
	// GNOME_CANVAS_ITEM(simpleline)->object.flags |= GNOME_CANVAS_ITEM_NO_AUTO_REDRAW;
}

static void
gnome_canvas_simpleline_bounds_world (GnomeCanvasItem *item, int* ix1, int* iy1, int* ix2, int* iy2)
{
	double x1, x2, y1, y2;
	ArtPoint i1, i2;
	ArtPoint w1, w2;
	double i2w[6];
	GnomeCanvasSimpleLine *simpleline = GNOME_CANVAS_SIMPLELINE(item);

	gnome_canvas_simpleline_bounds (item, &x1, &y1, &x2, &y2);

	i1.x = x1;
	i1.y = y1;
	i2.x = x2;
	i2.y = y2;
	
	gnome_canvas_item_i2w_affine (item, i2w);
	art_affine_point (&w1, &i1, i2w);
	art_affine_point (&w2, &i2, i2w);

	*ix1 = (int) rint(w1.x);
	*ix2 = (int) rint(w2.x);
	*iy1 = (int) rint(w1.y);
	*iy2 = (int) rint(w2.y);

	/* the update rect has to be of non-zero width and height */

	if (x1 == x2) {
		simpleline->horizontal = FALSE;
		*ix2 += 1;
	} else {
		simpleline->horizontal = TRUE;
		*iy2 += 1;
	}
}

static void 
gnome_canvas_simpleline_reset_bounds (GnomeCanvasItem *item)
{
	int Ix1, Ix2, Iy1, Iy2;

	gnome_canvas_simpleline_bounds_world (item, &Ix1, &Iy1, &Ix2, &Iy2);
	gnome_canvas_update_bbox (item, Ix1, Iy1, Ix2, Iy2);
}

/* 
 * CANVAS CALLBACKS 
 */

static void
gnome_canvas_simpleline_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	GnomeCanvasSimpleLine *simpleline;
	int redraw;
	int calc_bounds;

	item = GNOME_CANVAS_ITEM (object);
	simpleline = GNOME_CANVAS_SIMPLELINE (object);

	redraw = FALSE;
	calc_bounds = FALSE;

	switch (arg_id) {
	case ARG_X1:
	        if (simpleline->x1 != GTK_VALUE_DOUBLE (*arg)) {
		        simpleline->x1 = GTK_VALUE_DOUBLE (*arg);
			calc_bounds = TRUE;
		}
		break;

	case ARG_Y1:
	        if (simpleline->y1 != GTK_VALUE_DOUBLE (*arg)) {
		        simpleline->y1 = GTK_VALUE_DOUBLE (*arg);
			calc_bounds = TRUE;
		}
		break;

	case ARG_X2:
	        if (simpleline->x2 != GTK_VALUE_DOUBLE (*arg)) {
		        simpleline->x2 = GTK_VALUE_DOUBLE (*arg);
			calc_bounds = TRUE;
		}
		break;

	case ARG_Y2:
	        if (simpleline->y2 != GTK_VALUE_DOUBLE (*arg)) {
		        simpleline->y2 = GTK_VALUE_DOUBLE (*arg);
			calc_bounds = TRUE;
		}
		break;

	case ARG_COLOR_RGBA:
		if (simpleline->color != GTK_VALUE_INT(*arg)) {
			simpleline->color = GTK_VALUE_INT(*arg);
			UINT_TO_RGBA (simpleline->color, &simpleline->r, &simpleline->g, &simpleline->b, &simpleline->a);
			redraw = TRUE;
		}
		break;

	default:
		break;
	}
	
	if (calc_bounds) {

		gnome_canvas_item_request_update (item);

	} else if (redraw) {

		int Ix1, Ix2, Iy1, Iy2;
		gnome_canvas_simpleline_bounds_world (item, &Ix1, &Iy1, &Ix2, &Iy2);
		gnome_canvas_request_redraw (item->canvas, Ix1, Iy1, Ix2, Iy2);
	}
}

static void
gnome_canvas_simpleline_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GnomeCanvasSimpleLine *simpleline;

	simpleline = GNOME_CANVAS_SIMPLELINE (object);

	switch (arg_id) {
	case ARG_X1:
		GTK_VALUE_DOUBLE (*arg) = simpleline->x1;
		break;
	case ARG_Y1:
		GTK_VALUE_DOUBLE (*arg) = simpleline->y1;
		break;
	case ARG_X2:
		GTK_VALUE_DOUBLE (*arg) = simpleline->x2;
		break;
	case ARG_Y2:
		GTK_VALUE_DOUBLE (*arg) = simpleline->y2;
		break;
	case ARG_COLOR_RGBA:
		GTK_VALUE_INT (*arg) = simpleline->color;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
gnome_canvas_simpleline_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	GnomeCanvasSimpleLine *simpleline;
	double x;
	double y;

	simpleline = GNOME_CANVAS_SIMPLELINE (item);

	if (parent_class->update)
		(* parent_class->update) (item, affine, clip_path, flags);

	gnome_canvas_simpleline_reset_bounds (item);

	x = simpleline->x1;
	y = simpleline->y1;

	gnome_canvas_item_i2w (item, &x, &y);
	gnome_canvas_w2c (GNOME_CANVAS(item->canvas), x, y, &simpleline->bbox_ulx, &simpleline->bbox_uly);

	x = simpleline->x2;
	y = simpleline->y2;

	gnome_canvas_item_i2w (item, &x, &y);
	gnome_canvas_w2c (GNOME_CANVAS(item->canvas), x, y, &simpleline->bbox_lrx, &simpleline->bbox_lry);
}

static void
gnome_canvas_simpleline_render (GnomeCanvasItem *item,
			      GnomeCanvasBuf *buf)
{
	GnomeCanvasSimpleLine *simpleline;
	int end, begin;

	simpleline = GNOME_CANVAS_SIMPLELINE (item);

	if (parent_class->render) {
		(*parent_class->render) (item, buf);
	}

	if (buf->is_bg) {
		gnome_canvas_buf_ensure_buf (buf);
		buf->is_bg = FALSE;
	}

	// begin = MAX(simpleline->bbox_ulx,buf->rect.x0);
	// end = MIN(simpleline->bbox_lrx,buf->rect.x1);
	
	begin = simpleline->bbox_ulx;
	end = simpleline->bbox_lrx;

	if (simpleline->color != 0) {
		if (simpleline->horizontal) {
			PAINT_HORIZA(buf, simpleline->r, simpleline->g, simpleline->b, simpleline->a, 
				     begin, end, simpleline->bbox_uly);
		} else {
			PAINT_VERTA(buf, simpleline->r, simpleline->g, simpleline->b, simpleline->a, 
				    begin, simpleline->bbox_uly, simpleline->bbox_lry);
		}
	}
}

static void
gnome_canvas_simpleline_draw (GnomeCanvasItem *item,
			    GdkDrawable *drawable,
			    int x, int y,
			    int width, int height)
{
	GnomeCanvasSimpleLine *simpleline;

	simpleline = GNOME_CANVAS_SIMPLELINE (item);

	if (parent_class->draw) {
		(* parent_class->draw) (item, drawable, x, y, width, height);
	}

	fprintf (stderr, "please don't use the CanvasSimpleLine item in a non-aa Canvas\n");
	abort ();
}

static void
gnome_canvas_simpleline_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	GnomeCanvasSimpleLine *simpleline = GNOME_CANVAS_SIMPLELINE (item);

	*x1 = simpleline->x1;
	*y1 = simpleline->y1;
	*x2 = simpleline->x2;
	*y2 = simpleline->y2;
}

static double
gnome_canvas_simpleline_point (GnomeCanvasItem *item, double x, double y, int cx, int cy, GnomeCanvasItem **actual_item)
{
	GnomeCanvasSimpleLine *simpleline;
	double x1, y1, x2, y2;
	double dx, dy;

	simpleline = GNOME_CANVAS_SIMPLELINE (item);

	*actual_item = item;

	/* Find the bounds for the rectangle plus its outline width */

	gnome_canvas_simpleline_bounds (item, &x1, &y1, &x2, &y2);

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
