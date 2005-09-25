#include <stdio.h>
#include <math.h>
#include <gtk-canvas.h>

#include "canvas-ruler.h"
#include "rgb_macros.h"

enum {
	ARG_0,
	ARG_X1,
	ARG_Y1,
	ARG_X2,
	ARG_Y2,
	ARG_FRAMES_PER_UNIT,
	ARG_FILL_COLOR,
	ARG_TICK_COLOR

};

static void gtk_canvas_ruler_class_init (GtkCanvasRulerClass *class);
static void gtk_canvas_ruler_init       (GtkCanvasRuler      *ruler);
static void gtk_canvas_ruler_set_arg    (GtkObject              *object,
					      GtkArg                 *arg,
					      guint                   arg_id);
static void gtk_canvas_ruler_get_arg    (GtkObject              *object,
					      GtkArg                 *arg,
					      guint                   arg_id);

static void   gtk_canvas_ruler_update      (GtkCanvasItem *item, double *affine, ArtSVP *clip_path, int flags);
static void   gtk_canvas_ruler_bounds      (GtkCanvasItem *item, double *x1, double *y1, double *x2, double *y2);
static double gtk_canvas_ruler_point (GtkCanvasItem *item, double x, double y, int cx, int cy, GtkCanvasItem **actual_item);
static void   gtk_canvas_ruler_render (GtkCanvasItem *item, GtkCanvasBuf *buf);
static void   gtk_canvas_ruler_draw (GtkCanvasItem *item, GdkDrawable *drawable, int x, int y, int w, int h);

static GtkCanvasItemClass *parent_class;


GtkType
gtk_canvas_ruler_get_type (void)
{
	static GtkType ruler_type = 0;

	if (!ruler_type) {
		GtkTypeInfo ruler_info = {
			"GtkCanvasRuler",
			sizeof (GtkCanvasRuler),
			sizeof (GtkCanvasRulerClass),
			(GtkClassInitFunc) gtk_canvas_ruler_class_init,
			(GtkObjectInitFunc) gtk_canvas_ruler_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		ruler_type = gtk_type_unique (gtk_canvas_item_get_type (), &ruler_info);
	}

	return ruler_type;
}

static void
gtk_canvas_ruler_class_init (GtkCanvasRulerClass *class)
{
	GtkObjectClass *object_class;
	GtkCanvasItemClass *item_class;

	object_class = (GtkObjectClass *) class;
	item_class = (GtkCanvasItemClass *) class;

	parent_class = gtk_type_class (gtk_canvas_item_get_type ());

	gtk_object_add_arg_type ("GtkCanvasRuler::x1", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_X1);
	gtk_object_add_arg_type ("GtkCanvasRuler::y1", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_Y1);
	gtk_object_add_arg_type ("GtkCanvasRuler::x2", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_X2);
	gtk_object_add_arg_type ("GtkCanvasRuler::y2", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_Y2);
	gtk_object_add_arg_type ("GtkCanvasRuler::frames_per_unit", GTK_TYPE_LONG, GTK_ARG_READWRITE, ARG_FRAMES_PER_UNIT);
	gtk_object_add_arg_type ("GtkCanvasRuler::fill_color", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_FILL_COLOR);
	gtk_object_add_arg_type ("GtkCanvasRuler::tick_color", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_TICK_COLOR);

	object_class->set_arg = gtk_canvas_ruler_set_arg;
	object_class->get_arg = gtk_canvas_ruler_get_arg;

	item_class->update = gtk_canvas_ruler_update;
	item_class->bounds = gtk_canvas_ruler_bounds;
	item_class->point = gtk_canvas_ruler_point;
	item_class->render = gtk_canvas_ruler_render;
	item_class->draw = gtk_canvas_ruler_draw;
}

static void
gtk_canvas_ruler_init (GtkCanvasRuler *ruler)
{
	ruler->x1 = 0.0;
	ruler->y1 = 0.0;
	ruler->x2 = 0.0;
	ruler->y2 = 0.0;
	ruler->frames_per_unit = 1;
	ruler->fill_color = 0;
	ruler->tick_color = 0;

	GTK_CANVAS_ITEM(ruler)->object.flags |= GTK_CANVAS_ITEM_NO_AUTO_REDRAW;
}

static void 
gtk_canvas_ruler_reset_bounds (GtkCanvasItem *item)

{
	double x1, x2, y1, y2;
	ArtPoint i1, i2;
	ArtPoint w1, w2;
	int Ix1, Ix2, Iy1, Iy2;
	double i2w[6];

	gtk_canvas_ruler_bounds (item, &x1, &y1, &x2, &y2);

	i1.x = x1;
	i1.y = y1;
	i2.x = x2;
	i2.y = y2;

	gtk_canvas_item_i2w_affine (item, i2w);
	art_affine_point (&w1, &i1, i2w);
	art_affine_point (&w2, &i2, i2w);

	Ix1 = (int) rint(w1.x);
	Ix2 = (int) rint(w2.x);
	Iy1 = (int) rint(w1.y);
	Iy2 = (int) rint(w2.y);

	gtk_canvas_update_bbox (item, Ix1, Iy1, Ix2, Iy2);
}

/* 
 * CANVAS CALLBACKS 
 */

static void
gtk_canvas_ruler_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GtkCanvasItem *item;
	GtkCanvasRuler *ruler;
	int redraw;
	int calc_bounds;

	item = GTK_CANVAS_ITEM (object);
	ruler = GTK_CANVAS_RULER (object);

	redraw = FALSE;
	calc_bounds = FALSE;

	switch (arg_id) {
	case ARG_X1:
	        if (ruler->x1 != GTK_VALUE_DOUBLE (*arg)) {
		        ruler->x1 = GTK_VALUE_DOUBLE (*arg);
			calc_bounds = TRUE;
		}
		break;

	case ARG_Y1:
	        if (ruler->y1 != GTK_VALUE_DOUBLE (*arg)) {
		        ruler->y1 = GTK_VALUE_DOUBLE (*arg);
			calc_bounds = TRUE;
		}
		break;

	case ARG_X2:
	        if (ruler->x2 != GTK_VALUE_DOUBLE (*arg)) {
		        ruler->x2 = GTK_VALUE_DOUBLE (*arg);
			calc_bounds = TRUE;
		}
		break;

	case ARG_Y2:
	        if (ruler->y2 != GTK_VALUE_DOUBLE (*arg)) {
		        ruler->y2 = GTK_VALUE_DOUBLE (*arg);
			calc_bounds = TRUE;
		}
		break;

	case ARG_FRAMES_PER_UNIT:
		if (ruler->frames_per_unit != GTK_VALUE_LONG(*arg)) {
			ruler->frames_per_unit = GTK_VALUE_LONG(*arg);
			redraw = TRUE;
		}
		break;

	case ARG_FILL_COLOR:
		if (ruler->fill_color != GTK_VALUE_INT(*arg)) {
			ruler->fill_color = GTK_VALUE_INT(*arg);
			redraw = TRUE;
		}
		break;

	case ARG_TICK_COLOR:
		if (ruler->tick_color != GTK_VALUE_INT(*arg)) {
			ruler->tick_color = GTK_VALUE_INT(*arg);
			redraw = TRUE;
		}
		break;

	default:
		break;
	}

	if (calc_bounds) {
		gtk_canvas_ruler_reset_bounds (item);
	}

	if (redraw) {
		gtk_canvas_item_request_update (item);
	}

}

static void
gtk_canvas_ruler_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GtkCanvasRuler *ruler;

	ruler = GTK_CANVAS_RULER (object);

	switch (arg_id) {
	case ARG_X1:
		GTK_VALUE_DOUBLE (*arg) = ruler->x1;
		break;
	case ARG_Y1:
		GTK_VALUE_DOUBLE (*arg) = ruler->y1;
		break;
	case ARG_X2:
		GTK_VALUE_DOUBLE (*arg) = ruler->x2;
		break;
	case ARG_Y2:
		GTK_VALUE_DOUBLE (*arg) = ruler->y2;
		break;
	case ARG_FRAMES_PER_UNIT:
		GTK_VALUE_LONG (*arg) = ruler->frames_per_unit;
		break;
	case ARG_FILL_COLOR:
		GTK_VALUE_INT (*arg) = ruler->fill_color;
		break;
	case ARG_TICK_COLOR:
		GTK_VALUE_INT (*arg) = ruler->tick_color;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
gtk_canvas_ruler_update (GtkCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	GtkCanvasRuler *ruler;
	double x;
	double y;

	ruler = GTK_CANVAS_RULER (item);

	if (parent_class->update)
		(* parent_class->update) (item, affine, clip_path, flags);

	gtk_canvas_ruler_reset_bounds (item);

	x = ruler->x1;
	y = ruler->y1;

	gtk_canvas_item_i2w (item, &x, &y);
	gtk_canvas_w2c (GTK_CANVAS(item->canvas), x, y, &ruler->bbox_ulx, &ruler->bbox_uly);

	x = ruler->x2;
	y = ruler->y2;

	gtk_canvas_item_i2w (item, &x, &y);
	gtk_canvas_w2c (GTK_CANVAS(item->canvas), x, y, &ruler->bbox_lrx, &ruler->bbox_lry);

	UINT_TO_RGB (ruler->tick_color, &ruler->tick_r, &ruler->tick_g, &ruler->tick_b);
	UINT_TO_RGB (ruler->fill_color, &ruler->fill_r, &ruler->fill_g, &ruler->fill_b);
}

static void
gtk_canvas_ruler_render (GtkCanvasItem *item,
			      GtkCanvasBuf *buf)
{
	GtkCanvasRuler *ruler;
	int end, begin;

	ruler = GTK_CANVAS_RULER (item);

	if (parent_class->render) {
		(*parent_class->render) (item, buf);
	}

	if (buf->is_bg) {
		gtk_canvas_buf_ensure_buf (buf);
		buf->is_bg = FALSE;
	}

	begin = MAX(ruler->bbox_ulx,buf->rect.x0);

	if (ruler->bbox_lrx >= 0) {
		end = MIN(ruler->bbox_lrx,buf->rect.x1);
	} else {
		end = buf->rect.x1;
	}

	if (begin == end) {
		return;
	}

	PAINT_BOX (buf, ruler->fill_r, ruler->fill_g, ruler->fill_b, 255, begin, ruler->bbox_uly, end, ruler->bbox_lry - 1);
	PAINT_HORIZ (buf, ruler->tick_r, ruler->tick_g, ruler->tick_b, begin, end, ruler->bbox_lry - 1);
}

static void
gtk_canvas_ruler_draw (GtkCanvasItem *item,
			    GdkDrawable *drawable,
			    int x, int y,
			    int width, int height)
{
	GtkCanvasRuler *ruler;

	ruler = GTK_CANVAS_RULER (item);

	if (parent_class->draw) {
		(* parent_class->draw) (item, drawable, x, y, width, height);
	}

	fprintf (stderr, "please don't use the CanvasRuler item in a non-aa Canvas\n");
	abort ();
}

static void
gtk_canvas_ruler_bounds (GtkCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	GtkCanvasRuler *ruler = GTK_CANVAS_RULER (item);

	*x1 = ruler->x1;
	*y1 = ruler->y1;
	*x2 = ruler->x2;
	*y2 = ruler->y2;
}

static double
gtk_canvas_ruler_point (GtkCanvasItem *item, double x, double y, int cx, int cy, GtkCanvasItem **actual_item)
{
	GtkCanvasRuler *ruler;
	double x1, y1, x2, y2;
	double dx, dy;

	ruler = GTK_CANVAS_RULER (item);

	*actual_item = item;

	/* Find the bounds for the rectangle plus its outline width */

	gtk_canvas_ruler_bounds (item, &x1, &y1, &x2, &y2);

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
