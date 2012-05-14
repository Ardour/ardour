#include <stdio.h>
#include <math.h>
#include <cairo.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include "canvas-simpleline.h"
#include "rgb_macros.h"
#include "gettext.h"
#define _(Text)  dgettext (PACKAGE,Text)

enum {
	PROP_0,
	PROP_X1,
	PROP_Y1,
	PROP_X2,
	PROP_Y2,
	PROP_COLOR_RGBA
};

static void gnome_canvas_simpleline_class_init   (GnomeCanvasSimpleLineClass *class);

static void gnome_canvas_simpleline_init         (GnomeCanvasSimpleLine      *simpleline);

static void gnome_canvas_simpleline_destroy      (GtkObject            *object);

static void gnome_canvas_simpleline_set_property (GObject        *object,
						  guint           prop_id,
						  const GValue   *value,
						  GParamSpec     *pspec);
static void gnome_canvas_simpleline_get_property (GObject        *object,
						  guint           prop_id,
						  GValue         *value,
						  GParamSpec     *pspec);

static void   gnome_canvas_simpleline_update     (GnomeCanvasItem *item,
						  double          *affine,
						  ArtSVP          *clip_path,
						  int flags);

static void   gnome_canvas_simpleline_bounds     (GnomeCanvasItem *item,
						  double          *x1,
						  double          *y1,
						  double          *x2,
						  double          *y2);

static double gnome_canvas_simpleline_point      (GnomeCanvasItem  *item,
						  double            x,
						  double            y,
						  int               cx,
						  int               cy,
						  GnomeCanvasItem **actual_item);

static void   gnome_canvas_simpleline_render     (GnomeCanvasItem *item,
						  GnomeCanvasBuf  *buf);

static void   gnome_canvas_simpleline_draw       (GnomeCanvasItem *item,
						  GdkDrawable     *drawable,
						  int              x,
						  int              y,
						  int              w,
						  int              h);

static GnomeCanvasItemClass *parent_class;


GType
gnome_canvas_simpleline_get_type (void)
{
	static GType simpleline_type;

	if (!simpleline_type) {
		static const GTypeInfo object_info = {
			sizeof (GnomeCanvasSimpleLineClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gnome_canvas_simpleline_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,			/* class_data */
			sizeof (GnomeCanvasSimpleLine),
			0,			/* n_preallocs */
			(GInstanceInitFunc) gnome_canvas_simpleline_init,
			NULL			/* value_table */
		};

		simpleline_type = g_type_register_static (GNOME_TYPE_CANVAS_ITEM, "GnomeCanvasSimpleLine",
							  &object_info, 0);
	}

	return simpleline_type;
}

static void
gnome_canvas_simpleline_class_init (GnomeCanvasSimpleLineClass *class)
{
        GObjectClass *gobject_class;
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	gobject_class = (GObjectClass *) class;
	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	parent_class = g_type_class_peek_parent (class);

	gobject_class->set_property = gnome_canvas_simpleline_set_property;
	gobject_class->get_property = gnome_canvas_simpleline_get_property;

	g_object_class_install_property (gobject_class,
					 PROP_X1,
					 g_param_spec_double ("x1",
							      _("x1"),
							      _("x coordinate of upper left corner of rect"),
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0.0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class,
					 PROP_Y1,
					 g_param_spec_double ("y1",
							      _("y1"),
							      _("y coordinate of upper left corner of rect "),
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0.0,
							      G_PARAM_READWRITE));


	g_object_class_install_property (gobject_class,
					 PROP_X2,
					 g_param_spec_double ("x2",
							      _("x2"),
							      _("x coordinate of lower right corner of rect"),
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0.0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class,
					 PROP_Y2,
					 g_param_spec_double ("y2",
							      _("y2"),
							      _("y coordinate of lower right corner of rect "),
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0.0,
							      G_PARAM_READWRITE));
	g_object_class_install_property (gobject_class,
					 PROP_COLOR_RGBA,
					 g_param_spec_uint ("color_rgba",
							    _("color rgba"),
							    _("color of line"),
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READWRITE));

	object_class->destroy = gnome_canvas_simpleline_destroy;

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
}

static void
gnome_canvas_simpleline_destroy (GtkObject *object)
{
	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_SIMPLELINE (object));

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/*
 * CANVAS CALLBACKS
 */

static void
gnome_canvas_simpleline_set_property (GObject      *object,
				      guint         prop_id,
				      const GValue *value,
				      GParamSpec   *pspec)

{
	(void) pspec;

	GnomeCanvasSimpleLine *simpleline;
	int update = FALSE;
	int bounds_changed = FALSE;
        double d;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_SIMPLELINE (object));

	simpleline = GNOME_CANVAS_SIMPLELINE (object);

	switch (prop_id) {
	case PROP_X1:
                d = g_value_get_double (value);
	        if (simpleline->x1 != d) {
		        simpleline->x1 = d;
			bounds_changed = TRUE;
		}
		break;

	case PROP_Y1:
                d = g_value_get_double (value);
	        if (simpleline->y1 != d) {
		        simpleline->y1 = d;
			bounds_changed = TRUE;
		}
		break;

	case PROP_X2:
                d = g_value_get_double (value);
	        if (simpleline->x2 != d) {
		        simpleline->x2 = d;
			bounds_changed = TRUE;
		}
		break;

	case PROP_Y2:
                d = g_value_get_double (value);
	        if (simpleline->y2 != d) {
		        simpleline->y2 = d;
			bounds_changed = TRUE;
		}
		break;

	case PROP_COLOR_RGBA:
		if (simpleline->color != g_value_get_uint(value)) {
		        simpleline->color = g_value_get_uint(value);
			UINT_TO_RGBA (simpleline->color, &simpleline->r, &simpleline->g, &simpleline->b, &simpleline->a);
		        update = TRUE;
		}
		break;
	default:
		break;
	}

	if (update || bounds_changed) {
		gnome_canvas_item_request_update (GNOME_CANVAS_ITEM(object));
	}
}

static void
gnome_canvas_simpleline_get_property (GObject      *object,
				      guint         prop_id,
				      GValue       *value,
				      GParamSpec   *pspec)
{
        g_return_if_fail (object != NULL);
        g_return_if_fail (GNOME_IS_CANVAS_SIMPLELINE (object));

	GnomeCanvasSimpleLine *line = GNOME_CANVAS_SIMPLELINE (object);

	switch (prop_id) {
	case PROP_X1:
		g_value_set_double (value, line->x1);
		break;
	case PROP_X2:
		g_value_set_double (value, line->x2);
		break;
	case PROP_Y1:
		g_value_set_double (value, line->y1);
		break;
	case PROP_Y2:
		g_value_set_double (value, line->y2);
		break;
	case PROP_COLOR_RGBA:
		g_value_set_uint (value, line->color);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gnome_canvas_simpleline_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	GnomeCanvasSimpleLine *simpleline;
        double x1, x2, y1, y2;

	simpleline = GNOME_CANVAS_SIMPLELINE (item);

	if (parent_class->update)
		(* parent_class->update) (item, affine, clip_path, flags);

        /* redraw old location */

        gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);

        /* get current bounding box in parent-relative world coordinates */

        gnome_canvas_simpleline_bounds (item, &x1, &y1, &x2, &y2);

        /* convert parent-relative item coordinates to world coordinates */

        gnome_canvas_item_i2w (item, &x1, &y1);
        gnome_canvas_item_i2w (item, &x2, &y2);

        /* don't suffer from rounding errors */

        x1 = floor (x1);
        y1 = floor (y1);
        x2 = ceil (x2);
        y2 = ceil (y2);

        /* force non-zero dimensionality for both axes */

        if (x1 == x2) {
                x2 += 1.0;
        }

        if (y1 == y2) {
                y2 += 1.0;
        }

        /* reset item bounding box (canvas coordinates, so integral. but stored in doubles) */

        gnome_canvas_w2c_d (GNOME_CANVAS(item->canvas), x1, y1, &item->x1, &item->y1);
        gnome_canvas_w2c_d (GNOME_CANVAS(item->canvas), x2, y2, &item->x2, &item->y2);

        /* redraw new location */

        gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);

        /* store actual line coords as canvas coordinates for use in render() */

        x1 = simpleline->x1;
        y1 = simpleline->y1;
        x2 = simpleline->x2;
        y2 = simpleline->y2;
        /* convert to world */
        gnome_canvas_item_i2w (item, &x1, &y1);
        gnome_canvas_item_i2w (item, &x2, &y2);
        /* avoid rounding errors */
        x1 = (int) floor (item->x1);
        y1 = (int) floor (item->y1);
        x2 = (int) ceil (item->x2);
        y2 = (int) ceil (item->y2);
        /* convert to canvas coordinates, integral, stored in integers */
        gnome_canvas_w2c (GNOME_CANVAS(item->canvas), x1, y1, &simpleline->cx1, &simpleline->cy1);
        gnome_canvas_w2c (GNOME_CANVAS(item->canvas), x2, y2, &simpleline->cx2, &simpleline->cy2);
}

static void
gnome_canvas_simpleline_render (GnomeCanvasItem *item,
                                GnomeCanvasBuf *buf)
{
	GnomeCanvasSimpleLine *simpleline;
	int x1, x2;
        int y1, y2;

	simpleline = GNOME_CANVAS_SIMPLELINE (item);

	x1 = simpleline->cx1;
	x2 = simpleline->cx2;
        y1 = simpleline->cy1;

	if (buf->is_bg) {
		gnome_canvas_buf_ensure_buf (buf);
		buf->is_bg = FALSE;
	}

        if (simpleline->x1 != simpleline->x2) {
                PAINT_HORIZA(buf, simpleline->r, simpleline->g, simpleline->b, simpleline->a,
                             x1, x2, y1);
        } else {
                y2 = simpleline->cy2;
                PAINT_VERTA (buf, simpleline->r, simpleline->g, simpleline->b, simpleline->a,
                             x1, y1, y2);

        }
}

static void
gnome_canvas_simpleline_draw (GnomeCanvasItem* canvas,
                              GdkDrawable* drawable,
                              int x, int y,
                              int width, int height)
{
        /* XXX not implemented */
}

static void
gnome_canvas_simpleline_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	GnomeCanvasSimpleLine *simpleline = GNOME_CANVAS_SIMPLELINE (item);

        *x1 = simpleline->x1;
        *y1 = simpleline->y1;
        *x2 = simpleline->x1;
        *y2 = simpleline->y2;
}

static double
gnome_canvas_simpleline_point (GnomeCanvasItem *item, double x, double y, int cx, int cy, GnomeCanvasItem **actual_item)
{
	(void) cx;
	(void) cy;

	double x1, y1, x2, y2;
	double dx, dy;

	*actual_item = item;

	/* Find the bounds for the line */

	gnome_canvas_simpleline_bounds (item, &x1, &y1, &x2, &y2);

	/* Is point inside line */

	if ((x >= x1) && (y >= y1) && (x <= x2) && (y <= y2)) {
		return 0.0;
	}
	/* Point is outside line */

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
