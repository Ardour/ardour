#include <stdio.h>
#include <math.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include "canvas-ruler.h"
#include "rgb_macros.h"

enum {
	PROP_0,
	PROP_X1,
	PROP_Y1,
	PROP_X2,
	PROP_Y2,
	PROP_FRAMES_PER_UNIT,
	PROP_FILL_COLOR,
	PROP_TICK_COLOR

};

static void gnome_canvas_ruler_class_init (GnomeCanvasRulerClass *class);
static void gnome_canvas_ruler_init       (GnomeCanvasRuler      *ruler);
static void gnome_canvas_ruler_set_arg    (GObject              *object,
					   guint                   prop_id
					   const GValue   *value,
					   GParamSpec     *pspec);
static void gnome_canvas_ruler_get_arg    (GObject              *object,
					   guint                   prop_id
					   GValue   *value,
					   GParamSpec     *pspec);
static void   gnome_canvas_ruler_update      (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags);
static void   gnome_canvas_ruler_bounds      (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2);
static double gnome_canvas_ruler_point (GnomeCanvasItem *item, double x, double y, int cx, int cy, GnomeCanvasItem **actual_item);
static void   gnome_canvas_ruler_render (GnomeCanvasItem *item, GnomeCanvasBuf *buf);
static void   gnome_canvas_ruler_draw (GnomeCanvasItem *item, GdkDrawable *drawable, int x, int y, int w, int h);

static GnomeCanvasItemClass *parent_class;


GtkType
gnome_canvas_ruler_get_type (void)
{
	static GtkType ruler_type = 0;

	if (!ruler_type) {
		GtkTypeInfo ruler_info = {
			"GnomeCanvasRuler",
			sizeof (GnomeCanvasRuler),
			sizeof (GnomeCanvasRulerClass),
			(GtkClassInitFunc) gnome_canvas_ruler_class_init,
			(GtkObjectInitFunc) gnome_canvas_ruler_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		ruler_type = gtk_type_unique (gnome_canvas_item_get_type (), &ruler_info);
	}

	return ruler_type;
}

static void
gnome_canvas_ruler_class_init (GnomeCanvasRulerClass *class)
{
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = G_OBJECT_CLASS  (class);
	item_class = (GnomeCanvasItemClass *) class;

	parent_class = gtk_type_class (gnome_canvas_item_get_type ());

	object_class->set_property = gnome_canvas_ruler_set_property;
	object_class->get_property = gnome_canvas_ruler_get_property;

	g_object_class_install_property (object_class,
					 PROP_X1,
					 g_param_spec_double ("x1",
							      _("x1"),
							      _("x coordinate of upper left corner of rect"),
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0.0,
							      G_PARAM_READWRITE));  
	
	g_object_class_install_property (object_class,
					 PROP_Y1,
					 g_param_spec_double ("y1",
							      _("y1"),
							      _("y coordinate of upper left corner of rect "),
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0.0,
							      G_PARAM_READWRITE));  
	
	
	g_object_class_install_property (object_class,
					 PROP_X2,
					 g_param_spec_double ("x2",
							      _("x2"),
							      _("x coordinate of lower right corner of rect"),
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0.0,
							      G_PARAM_READWRITE));  
	
	g_object_class_install_property (object_class,
					 PROP_Y2,
					 g_param_spec_double ("y2",
							      _("y2"),
							      _("y coordinate of lower right corner of rect "),
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0.0,
							      G_PARAM_READWRITE));  
	
	
	g_object_class_install_property (object_class,
					 PROP_FRAMES_PER_UNIT,
					 g_param_spec_long ("frames_per_unit",
							    _("frames_per_unit"),
							    _("frames_per_unit of ruler"),
							    -G_MAXLONG,
							    G_MAXLONG,
							    0,
							    G_PARAM_READWRITE)); 
	
	
	g_object_class_install_property (object_class,
					 PROP_FILL_COLOR,
					 g_param_spec_uint ("fill_color",
							    _("fill color"),
							    _("color of fill"),
							    0,
							    G_MAXINT,
							    0,
							    G_PARAM_READWRITE)); 
	
	
	g_object_class_install_property (object_class,
					 PROP_TICK_COLOR,
					 g_param_spec_uint ("tick_color",
							    _("tick color"),
							    _("color of tick"),
							    0,
							    G_MAXINT,
							    0,
							    G_PARAM_READWRITE)); 
	item_class->update = gnome_canvas_ruler_update;
	item_class->bounds = gnome_canvas_ruler_bounds;
	item_class->point = gnome_canvas_ruler_point;
	item_class->render = gnome_canvas_ruler_render;
	item_class->draw = gnome_canvas_ruler_draw;
}

static void
gnome_canvas_ruler_init (GnomeCanvasRuler *ruler)
{
	ruler->x1 = 0.0;
	ruler->y1 = 0.0;
	ruler->x2 = 0.0;
	ruler->y2 = 0.0;
	ruler->frames_per_unit = 1;
	ruler->fill_color = 0;
	ruler->tick_color = 0;

	// GTK2FIX
	//GNOME_CANVAS_ITEM(ruler)->object.flags |= GNOME_CANVAS_ITEM_NO_AUTO_REDRAW;
}

static void 
gnome_canvas_ruler_reset_bounds (GnomeCanvasItem *item)

{
	double x1, x2, y1, y2;
	ArtPoint i1, i2;
	ArtPoint w1, w2;
	int Ix1, Ix2, Iy1, Iy2;
	double i2w[6];

	gnome_canvas_ruler_bounds (item, &x1, &y1, &x2, &y2);

	i1.x = x1;
	i1.y = y1;
	i2.x = x2;
	i2.y = y2;

	gnome_canvas_item_i2w_affine (item, i2w);
	art_affine_point (&w1, &i1, i2w);
	art_affine_point (&w2, &i2, i2w);

	Ix1 = (int) rint(w1.x);
	Ix2 = (int) rint(w2.x);
	Iy1 = (int) rint(w1.y);
	Iy2 = (int) rint(w2.y);

	gnome_canvas_update_bbox (item, Ix1, Iy1, Ix2, Iy2);
}

/* 
 * CANVAS CALLBACKS 
 */

static void
gnome_canvas_ruler_set_property (GObject *object,
guint         prop_id,
				      const GValue *value,
				      GParamSpec   *pspec)
{
	GnomeCanvasItem *item;
	GnomeCanvasRuler *ruler;
	int redraw;
	int calc_bounds;

	item = GNOME_CANVAS_ITEM (object);
	ruler = GNOME_CANVAS_RULER (object);

	redraw = FALSE;
	calc_bounds = FALSE;

	switch (prop_id) {
	case PROP_X1:
	        if (ruler->x1 != g_value_get_double (value)) {
		        ruler->x1 = g_value_get_double (value);
			calc_bounds = TRUE;
		}
		break;

	case PROP_Y1:
	        if (ruler->y1 != g_value_get_double (value)) {
		        ruler->y1 = g_value_get_double (value);
			calc_bounds = TRUE;
		}
		break;

	case PROP_X2:
	        if (ruler->x2 != g_value_get_double (value)) {
		        ruler->x2 = g_value_get_double (value);
			calc_bounds = TRUE;
		}
		break;

	case PROP_Y2:
	        if (ruler->y2 != g_value_get_double (value)) {
		        ruler->y2 = g_value_get_double (value);
			calc_bounds = TRUE;
		}
		break;

	case PROP_FRAMES_PER_UNIT:
		if (ruler->frames_per_unit != g_value_get_long(value)) {
			ruler->frames_per_unit = g_value_get_long(value);
			redraw = TRUE;
		}
		break;

	case PROP_FILL_COLOR:
		if (ruler->fill_color != g_value_get_uint(value)) {
			ruler->fill_color = g_value_get_uint(value);
			redraw = TRUE;
		}
		break;

	case PROP_TICK_COLOR:
		if (ruler->tick_color != g_value_get_uint(value)) {
			ruler->tick_color = g_value_get_uint(value);
			redraw = TRUE;
		}
		break;

	default:
		break;
	}

	if (calc_bounds) {
		gnome_canvas_ruler_reset_bounds (item);
	}

	if (redraw) {
		gnome_canvas_item_request_update (item);
	}

}

static void
gnome_canvas_ruler_get_property (GObject *object,
  guint         prop_id,
				      GValue       *value,
				      GParamSpec   *pspec)

{
	GnomeCanvasRuler *ruler;

	ruler = GNOME_CANVAS_RULER (object);

	switch (prop_id) {
	case PROP_X1:
	  g_value_set_double (value, ruler->x1);
		break;
	case PROP_Y1:
	  g_value_set_double (value, ruler->y1);
		break;
	case PROP_X2:
	  g_value_set_double (value, ruler->x2);
		break;
	case PROP_Y2:
	  g_value_set_double (value, ruler->y2);
		break;
	case PROP_FRAMES_PER_UNIT:
	  g_value_set_long (value, ruler->frames_per_unit);
		break;
	case PROP_FILL_COLOR:
	  g_value_set_uint (value, ruler->fill_color);
		break;
	case PROP_TICK_COLOR:
	  g_value_set_uint (value, ruler->tick_color);
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
gnome_canvas_ruler_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	GnomeCanvasRuler *ruler;
	double x;
	double y;

	ruler = GNOME_CANVAS_RULER (item);

	if (parent_class->update)
		(* parent_class->update) (item, affine, clip_path, flags);

	gnome_canvas_ruler_reset_bounds (item);

	x = ruler->x1;
	y = ruler->y1;

	gnome_canvas_item_i2w (item, &x, &y);
	gnome_canvas_w2c (GNOME_CANVAS(item->canvas), x, y, &ruler->bbox_ulx, &ruler->bbox_uly);

	x = ruler->x2;
	y = ruler->y2;

	gnome_canvas_item_i2w (item, &x, &y);
	gnome_canvas_w2c (GNOME_CANVAS(item->canvas), x, y, &ruler->bbox_lrx, &ruler->bbox_lry);

	UINT_TO_RGB (ruler->tick_color, &ruler->tick_r, &ruler->tick_g, &ruler->tick_b);
	UINT_TO_RGB (ruler->fill_color, &ruler->fill_r, &ruler->fill_g, &ruler->fill_b);
}

static void
gnome_canvas_ruler_render (GnomeCanvasItem *item,
			      GnomeCanvasBuf *buf)
{
	GnomeCanvasRuler *ruler;
	int end, begin;

	ruler = GNOME_CANVAS_RULER (item);

	if (parent_class->render) {
		(*parent_class->render) (item, buf);
	}

	if (buf->is_bg) {
		gnome_canvas_buf_ensure_buf (buf);
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
gnome_canvas_ruler_draw (GnomeCanvasItem *item,
			    GdkDrawable *drawable,
			    int x, int y,
			    int width, int height)
{
	GnomeCanvasRuler *ruler;

	ruler = GNOME_CANVAS_RULER (item);

	if (parent_class->draw) {
		(* parent_class->draw) (item, drawable, x, y, width, height);
	}

	fprintf (stderr, "please don't use the CanvasRuler item in a non-aa Canvas\n");
	abort ();
}

static void
gnome_canvas_ruler_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	GnomeCanvasRuler *ruler = GNOME_CANVAS_RULER (item);

	*x1 = ruler->x1;
	*y1 = ruler->y1;
	*x2 = ruler->x2;
	*y2 = ruler->y2;
}

static double
gnome_canvas_ruler_point (GnomeCanvasItem *item, double x, double y, int cx, int cy, GnomeCanvasItem **actual_item)
{
	GnomeCanvasRuler *ruler;
	double x1, y1, x2, y2;
	double dx, dy;

	ruler = GNOME_CANVAS_RULER (item);

	*actual_item = item;

	/* Find the bounds for the rectangle plus its outline width */

	gnome_canvas_ruler_bounds (item, &x1, &y1, &x2, &y2);

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
