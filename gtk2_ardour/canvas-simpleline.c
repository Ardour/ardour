#include <stdio.h>
#include <math.h>
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
	simpleline->horizontal = TRUE; /* reset in the _update() method */
	// GTK2FIX
	// GNOME_CANVAS_ITEM(simpleline)->object.flags |= GNOME_CANVAS_ITEM_NO_AUTO_REDRAW;
}

static void
gnome_canvas_simpleline_destroy (GtkObject *object)
{
	GnomeCanvasSimpleLine *line;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_SIMPLELINE (object));

	line = GNOME_CANVAS_SIMPLELINE (object);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
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
gnome_canvas_simpleline_set_property (GObject      *object,
				      guint         prop_id,
				      const GValue *value,
				      GParamSpec   *pspec)

{
	GnomeCanvasSimpleLine *simpleline;
	int update = FALSE;
	int bounds_changed = FALSE;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_SIMPLELINE (object));

	simpleline = GNOME_CANVAS_SIMPLELINE (object);

	switch (prop_id) {
	case PROP_X1:
	        if (simpleline->x1 != g_value_get_double (value)) {
		        simpleline->x1 = g_value_get_double (value);
			bounds_changed = TRUE;
		}
		break;

	case PROP_Y1:
	        if (simpleline->y1 != g_value_get_double (value)) {
		        simpleline->y1 = g_value_get_double (value);
			bounds_changed = TRUE;
		}
		break;

	case PROP_X2:
	        if (simpleline->x2 != g_value_get_double (value)) {
		        simpleline->x2 = g_value_get_double (value);
			bounds_changed = TRUE;
		}
		break;

	case PROP_Y2:
	        if (simpleline->y2 != g_value_get_double (value)) {
		        simpleline->y2 = g_value_get_double (value);
			bounds_changed = TRUE;
		}
		break;
		
	case PROP_COLOR_RGBA:
		if (simpleline->color != g_value_get_uint(value)) {
		        simpleline->color = g_value_get_uint(value);
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
