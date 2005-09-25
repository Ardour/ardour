#include <stdio.h>
#include <math.h>
#include <gtk-canvas.h>

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

static void gtk_canvas_simpleline_class_init (GtkCanvasSimpleLineClass *class);
static void gtk_canvas_simpleline_init       (GtkCanvasSimpleLine      *simpleline);
static void gtk_canvas_simpleline_set_arg    (GtkObject              *object,
					      GtkArg                 *arg,
					      guint                   arg_id);
static void gtk_canvas_simpleline_get_arg    (GtkObject              *object,
					      GtkArg                 *arg,
					      guint                   arg_id);

static void   gtk_canvas_simpleline_update      (GtkCanvasItem *item, double *affine, ArtSVP *clip_path, int flags);
static void   gtk_canvas_simpleline_bounds      (GtkCanvasItem *item, double *x1, double *y1, double *x2, double *y2);
static double gtk_canvas_simpleline_point (GtkCanvasItem *item, double x, double y, int cx, int cy, GtkCanvasItem **actual_item);
static void   gtk_canvas_simpleline_render (GtkCanvasItem *item, GtkCanvasBuf *buf);
static void   gtk_canvas_simpleline_draw (GtkCanvasItem *item, GdkDrawable *drawable, int x, int y, int w, int h);

static GtkCanvasItemClass *parent_class;


GtkType
gtk_canvas_simpleline_get_type (void)
{
	static GtkType simpleline_type = 0;

	if (!simpleline_type) {
		GtkTypeInfo simpleline_info = {
			"GtkCanvasSimpleLine",
			sizeof (GtkCanvasSimpleLine),
			sizeof (GtkCanvasSimpleLineClass),
			(GtkClassInitFunc) gtk_canvas_simpleline_class_init,
			(GtkObjectInitFunc) gtk_canvas_simpleline_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		simpleline_type = gtk_type_unique (gtk_canvas_item_get_type (), &simpleline_info);
	}

	return simpleline_type;
}

static void
gtk_canvas_simpleline_class_init (GtkCanvasSimpleLineClass *class)
{
	GtkObjectClass *object_class;
	GtkCanvasItemClass *item_class;

	object_class = (GtkObjectClass *) class;
	item_class = (GtkCanvasItemClass *) class;

	parent_class = gtk_type_class (gtk_canvas_item_get_type ());

	gtk_object_add_arg_type ("GtkCanvasSimpleLine::x1", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_X1);
	gtk_object_add_arg_type ("GtkCanvasSimpleLine::y1", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_Y1);
	gtk_object_add_arg_type ("GtkCanvasSimpleLine::x2", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_X2);
	gtk_object_add_arg_type ("GtkCanvasSimpleLine::y2", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_Y2);
	gtk_object_add_arg_type ("GtkCanvasSimpleLine::color_rgba", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_COLOR_RGBA);

	object_class->set_arg = gtk_canvas_simpleline_set_arg;
	object_class->get_arg = gtk_canvas_simpleline_get_arg;

	item_class->update = gtk_canvas_simpleline_update;
	item_class->bounds = gtk_canvas_simpleline_bounds;
	item_class->point = gtk_canvas_simpleline_point;
	item_class->render = gtk_canvas_simpleline_render;
	item_class->draw = gtk_canvas_simpleline_draw;
}

static void
gtk_canvas_simpleline_init (GtkCanvasSimpleLine *simpleline)
{
	simpleline->x1 = 0.0;
	simpleline->y1 = 0.0;
	simpleline->x2 = 0.0;
	simpleline->y2 = 0.0;
	simpleline->color = RGBA_TO_UINT(98,123,174,241);
	simpleline->horizontal = TRUE; /* reset in the _update() method */
	GTK_CANVAS_ITEM(simpleline)->object.flags |= GTK_CANVAS_ITEM_NO_AUTO_REDRAW;
}

static void
gtk_canvas_simpleline_bounds_world (GtkCanvasItem *item, int* ix1, int* iy1, int* ix2, int* iy2)
{
	double x1, x2, y1, y2;
	ArtPoint i1, i2;
	ArtPoint w1, w2;
	double i2w[6];
	GtkCanvasSimpleLine *simpleline = GTK_CANVAS_SIMPLELINE(item);

	gtk_canvas_simpleline_bounds (item, &x1, &y1, &x2, &y2);

	i1.x = x1;
	i1.y = y1;
	i2.x = x2;
	i2.y = y2;
	
	gtk_canvas_item_i2w_affine (item, i2w);
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
gtk_canvas_simpleline_reset_bounds (GtkCanvasItem *item)
{
	int Ix1, Ix2, Iy1, Iy2;

	gtk_canvas_simpleline_bounds_world (item, &Ix1, &Iy1, &Ix2, &Iy2);
	gtk_canvas_update_bbox (item, Ix1, Iy1, Ix2, Iy2);
}

/* 
 * CANVAS CALLBACKS 
 */

static void
gtk_canvas_simpleline_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GtkCanvasItem *item;
	GtkCanvasSimpleLine *simpleline;
	int redraw;
	int calc_bounds;

	item = GTK_CANVAS_ITEM (object);
	simpleline = GTK_CANVAS_SIMPLELINE (object);

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

		gtk_canvas_item_request_update (item);

	} else if (redraw) {

		int Ix1, Ix2, Iy1, Iy2;
		gtk_canvas_simpleline_bounds_world (item, &Ix1, &Iy1, &Ix2, &Iy2);
		gtk_canvas_request_redraw (item->canvas, Ix1, Iy1, Ix2, Iy2);
	}
}

static void
gtk_canvas_simpleline_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GtkCanvasSimpleLine *simpleline;

	simpleline = GTK_CANVAS_SIMPLELINE (object);

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
gtk_canvas_simpleline_update (GtkCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	GtkCanvasSimpleLine *simpleline;
	double x;
	double y;

	simpleline = GTK_CANVAS_SIMPLELINE (item);

	if (parent_class->update)
		(* parent_class->update) (item, affine, clip_path, flags);

	gtk_canvas_simpleline_reset_bounds (item);

	x = simpleline->x1;
	y = simpleline->y1;

	gtk_canvas_item_i2w (item, &x, &y);
	gtk_canvas_w2c (GTK_CANVAS(item->canvas), x, y, &simpleline->bbox_ulx, &simpleline->bbox_uly);

	x = simpleline->x2;
	y = simpleline->y2;

	gtk_canvas_item_i2w (item, &x, &y);
	gtk_canvas_w2c (GTK_CANVAS(item->canvas), x, y, &simpleline->bbox_lrx, &simpleline->bbox_lry);
}

static void
gtk_canvas_simpleline_render (GtkCanvasItem *item,
			      GtkCanvasBuf *buf)
{
	GtkCanvasSimpleLine *simpleline;
	int end, begin;

	simpleline = GTK_CANVAS_SIMPLELINE (item);

	if (parent_class->render) {
		(*parent_class->render) (item, buf);
	}

	if (buf->is_bg) {
		gtk_canvas_buf_ensure_buf (buf);
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
gtk_canvas_simpleline_draw (GtkCanvasItem *item,
			    GdkDrawable *drawable,
			    int x, int y,
			    int width, int height)
{
	GtkCanvasSimpleLine *simpleline;

	simpleline = GTK_CANVAS_SIMPLELINE (item);

	if (parent_class->draw) {
		(* parent_class->draw) (item, drawable, x, y, width, height);
	}

	fprintf (stderr, "please don't use the CanvasSimpleLine item in a non-aa Canvas\n");
	abort ();
}

static void
gtk_canvas_simpleline_bounds (GtkCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	GtkCanvasSimpleLine *simpleline = GTK_CANVAS_SIMPLELINE (item);

	*x1 = simpleline->x1;
	*y1 = simpleline->y1;
	*x2 = simpleline->x2;
	*y2 = simpleline->y2;
}

static double
gtk_canvas_simpleline_point (GtkCanvasItem *item, double x, double y, int cx, int cy, GtkCanvasItem **actual_item)
{
	GtkCanvasSimpleLine *simpleline;
	double x1, y1, x2, y2;
	double dx, dy;

	simpleline = GTK_CANVAS_SIMPLELINE (item);

	*actual_item = item;

	/* Find the bounds for the rectangle plus its outline width */

	gtk_canvas_simpleline_bounds (item, &x1, &y1, &x2, &y2);

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
