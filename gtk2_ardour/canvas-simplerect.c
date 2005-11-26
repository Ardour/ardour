#include <stdio.h>
#include <math.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include "canvas-simplerect.h"
#include "rgb_macros.h"
#include "gettext.h"
#define _(Text)  dgettext (PACKAGE,Text)

enum {
	PROP_0,
	PROP_X1,
	PROP_Y1,
	PROP_X2,
	PROP_Y2,
	PROP_OUTLINE_PIXELS,
	PROP_OUTLINE_WHAT,
	PROP_FILL,
	PROP_FILL_COLOR_RGBA,
	PROP_OUTLINE_COLOR_RGBA,
	PROP_DRAW
	
};

static void  gnome_canvas_simplerect_class_init (GnomeCanvasSimpleRectClass *class);
static void   gnome_canvas_simplerect_init       (GnomeCanvasSimpleRect      *simplerect);
static void   gnome_canvas_simplerect_set_property  (GObject        *object,
						     guint            prop_id,
						     const GValue   *value,
						     GParamSpec     *pspec);
static void   gnome_canvas_simplerect_get_property  (GObject        *object,
						     guint           prop_id,
						     GValue         *value,
						     GParamSpec     *pspec);
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
	GObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = G_OBJECT_CLASS (class);
	item_class = (GnomeCanvasItemClass *) class;

	parent_class = gtk_type_class (gnome_canvas_item_get_type ());

	object_class->set_property = gnome_canvas_simplerect_set_property;
	object_class->get_property = gnome_canvas_simplerect_get_property;
	
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
					 PROP_OUTLINE_PIXELS,
					 g_param_spec_uint ("outline_pixels",
							      _("outline pixels"),
							      _("width in pixels of outline"),
							      0,
							      G_MAXINT,
							      0,
							      G_PARAM_READWRITE));  
	

	g_object_class_install_property (object_class,
					 PROP_OUTLINE_WHAT,
					 g_param_spec_uint ("outline_what",
							      _("outline what"),
							      _("which boundaries to outline (mask)"),
							      0,
							      G_MAXINT,
							      0,
							      G_PARAM_READWRITE));  
	


	g_object_class_install_property (object_class,
					 PROP_FILL,
					 g_param_spec_boolean ("fill",
							       _("fill"),
							       _("fill rectangle"),
							       TRUE,
							       G_PARAM_READWRITE));  
	
	g_object_class_install_property (object_class,
					 PROP_DRAW,
					 g_param_spec_boolean ("draw",
							       _("draw"),
							       _("draw rectangle"),
							       TRUE,
							       G_PARAM_READWRITE));  
	

	g_object_class_install_property (object_class,
					 PROP_OUTLINE_COLOR_RGBA,
					 g_param_spec_uint ("outline_color_rgba",
							      _("outline color rgba"),
							      _("color of outline"),
							      0,
							      G_MAXINT,
							      0,
							      G_PARAM_READWRITE));  
	

	g_object_class_install_property (object_class,
					 PROP_FILL_COLOR_RGBA,
					 g_param_spec_uint ("fill_color_rgba",
							      _("fill color rgba"),
							      _("color of fill"),
							      0,
							      G_MAXINT,
							      0,
							      G_PARAM_READWRITE));  
	
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
gnome_canvas_simplerect_set_property (GObject      *object,
				      guint         prop_id,
				      const GValue *value,
				      GParamSpec   *pspec)

{
	GnomeCanvasSimpleRect *simplerect;
	int update;
	int bounds_changed;

	simplerect = GNOME_CANVAS_SIMPLERECT (object);

	update = FALSE;
	bounds_changed = FALSE;

	switch (prop_id) {
	case PROP_X1:
	        if (simplerect->x1 != g_value_get_double (value)) {
		        simplerect->x1 = g_value_get_double (value);
			bounds_changed = TRUE;
		}
		break;

	case PROP_Y1:
	        if (simplerect->y1 != g_value_get_double (value)) {
		        simplerect->y1 = g_value_get_double (value);
			bounds_changed = TRUE;
		}
		break;

	case PROP_X2:
	        if (simplerect->x2 != g_value_get_double (value)) {
		        simplerect->x2 = g_value_get_double (value);
			bounds_changed = TRUE;
		}
		break;

	case PROP_Y2:
	        if (simplerect->y2 != g_value_get_double (value)) {
		        simplerect->y2 = g_value_get_double (value);
			bounds_changed = TRUE;
		}
		break;

	case PROP_DRAW:
	        if (simplerect->draw != g_value_get_boolean (value)) {
		        simplerect->draw = g_value_get_boolean (value);
			update = TRUE;
		}
		break;


	case PROP_FILL:
	        if (simplerect->fill != g_value_get_boolean (value)) {
		        simplerect->fill = g_value_get_boolean (value);
			update = TRUE;
		}
		break;

	case PROP_FILL_COLOR_RGBA:
		if (simplerect->fill_color != g_value_get_uint(value)) {
			simplerect->fill_color = g_value_get_uint(value);
			update = TRUE;
		}
		break;

	case PROP_OUTLINE_COLOR_RGBA:
		if (simplerect->outline_color != g_value_get_uint(value)) {
			simplerect->outline_color = g_value_get_uint(value);
			update = TRUE;
		}
		break;

	case PROP_OUTLINE_PIXELS:
		if (simplerect->outline_pixels != g_value_get_uint(value)) {
			simplerect->outline_pixels = g_value_get_uint(value);
			update = TRUE;
		}
		break;

	case PROP_OUTLINE_WHAT:
		if (simplerect->outline_what != g_value_get_uint(value)) {
			simplerect->outline_what = g_value_get_uint(value);
			update = TRUE;
		}
		break;

	default:
		break;
	}

	simplerect->full_draw_on_update = update;

	if (update || bounds_changed) {
		gnome_canvas_item_request_update (GNOME_CANVAS_ITEM(object));
	}
}

static void
gnome_canvas_simplerect_get_property (GObject      *object,
				      guint         prop_id,
				      GValue       *value,
				      GParamSpec   *pspec)
{
	GnomeCanvasSimpleRect *rect = GNOME_CANVAS_SIMPLERECT (object);
	
	switch (prop_id) {
	case PROP_X1:
		g_value_set_double (value, rect->x1);
		break;
	case PROP_X2:
		g_value_set_double (value, rect->x2);
		break;
	case PROP_Y1:
		g_value_set_double (value, rect->y1);
		break;
	case PROP_Y2:
		g_value_set_double (value, rect->y2);
		break;
	case PROP_OUTLINE_WHAT:
		g_value_set_uint (value, rect->outline_what);
		break;
	case PROP_FILL:
		g_value_set_boolean (value, rect->fill);
		break;
	case PROP_OUTLINE_PIXELS:
		g_value_set_uint (value, rect->outline_pixels);
		break;
	case PROP_FILL_COLOR_RGBA:
		g_value_set_uint (value, rect->fill_color);
		break;
	case PROP_OUTLINE_COLOR_RGBA:
		g_value_set_uint (value, rect->outline_color);
		break;
	case PROP_DRAW:
		g_value_set_boolean (value, rect->draw);
		break;
		
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
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
