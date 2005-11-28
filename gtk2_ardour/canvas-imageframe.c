/* Image item type for GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */
 
 
#include <string.h> /* for memcpy() */
#include <math.h>
#include <stdio.h>
#include "libart_lgpl/art_misc.h"
#include "libart_lgpl/art_affine.h"
#include "libart_lgpl/art_pixbuf.h"
#include "libart_lgpl/art_rgb_pixbuf_affine.h"
#include "canvas-imageframe.h"
#include <libgnomecanvas/gnome-canvas-util.h>
#include "gettext.h"
#define _(Text)  dgettext (PACKAGE,Text)

//GTK2FIX
//#include <libgnomecanvas/gnome-canvastypebuiltins.h>


enum {
	PROP_0,
	PROP_PIXBUF,
	PROP_X,
	PROP_Y,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_DRAWWIDTH,
	PROP_ANCHOR
};


static void gnome_canvas_imageframe_class_init(GnomeCanvasImageFrameClass* class) ;
static void gnome_canvas_imageframe_init(GnomeCanvasImageFrame* image) ;
static void gnome_canvas_imageframe_destroy(GtkObject* object) ;
static void gnome_canvas_imageframe_set_property(GObject* object,
						 guint            prop_id,
						 const GValue   *value,
						 GParamSpec     *pspec);
static void gnome_canvas_imageframe_get_property(GObject* object,
						 guint           prop_id,
						 GValue         *value,
						 GParamSpec     *pspec);
static void gnome_canvas_imageframe_update(GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags) ;
static void gnome_canvas_imageframe_realize(GnomeCanvasItem *item) ;
static void gnome_canvas_imageframe_unrealize(GnomeCanvasItem *item) ;
static void gnome_canvas_imageframe_draw(GnomeCanvasItem *item, GdkDrawable *drawable, int x, int y, int width, int height) ;
static double gnome_canvas_imageframe_point(GnomeCanvasItem *item, double x, double y, int cx, int cy, GnomeCanvasItem **actual_item) ;
static void gnome_canvas_imageframe_bounds(GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2) ;
static void gnome_canvas_imageframe_render(GnomeCanvasItem *item, GnomeCanvasBuf *buf) ;

static GnomeCanvasItemClass *parent_class;


GType
gnome_canvas_imageframe_get_type (void)
{
	static GType imageframe_type = 0;

	if (!imageframe_type) {
		GtkTypeInfo imageframe_info = {
			"GnomeCanvasImageFrame",
			sizeof (GnomeCanvasImageFrame),
			sizeof (GnomeCanvasImageFrameClass),
			(GtkClassInitFunc) gnome_canvas_imageframe_class_init,
			(GtkObjectInitFunc) gnome_canvas_imageframe_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		imageframe_type = gtk_type_unique (gnome_canvas_item_get_type (), &imageframe_info);
	}

	return imageframe_type;
}

static void
gnome_canvas_imageframe_class_init (GnomeCanvasImageFrameClass *class)
{
	GObjectClass *gobject_class;
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	gobject_class = (GObjectClass *) class;
	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	parent_class = gtk_type_class (gnome_canvas_item_get_type ());

	gobject_class->set_property = gnome_canvas_imageframe_set_property;
	gobject_class->get_property = gnome_canvas_imageframe_get_property;

	g_object_class_install_property (gobject_class,
					 PROP_PIXBUF,
					 g_param_spec_pointer ("pixbuf",
							     _("pixbuf"),
							     _("the pixbuf"),
							     G_PARAM_WRITABLE)); 
	g_object_class_install_property (gobject_class,
					 PROP_X,
					 g_param_spec_double ("x",
							      _("x"),
							      _("x coordinate of upper left corner of rect"),
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0.0,
							      G_PARAM_READWRITE));  
	
	g_object_class_install_property (gobject_class,
					 PROP_Y,
					 g_param_spec_double ("y",
							      _("y"),
							      _("y coordinate of upper left corner of rect "),
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0.0,
							      G_PARAM_READWRITE));  
	g_object_class_install_property (gobject_class,
					 PROP_WIDTH,
					 g_param_spec_double ("width",
							      _("width"),
							      _("the width"),
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0.0,
							      G_PARAM_READWRITE));  
	
	g_object_class_install_property (gobject_class,
					 PROP_DRAWWIDTH,
					 g_param_spec_double ("drawwidth",
							      _("drawwidth"),
							      _("drawn width"),
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0.0,
							      G_PARAM_READWRITE));  
	g_object_class_install_property (gobject_class,
					 PROP_HEIGHT,
					 g_param_spec_double ("height",
							      _("height"),
							      _("the height"),
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0.0,
							      G_PARAM_READWRITE));  
	g_object_class_install_property (gobject_class,
					 PROP_ANCHOR,
					 g_param_spec_enum ("anchor",
							    _("anchor"),
							    _("the anchor"),
							    GTK_TYPE_ANCHOR_TYPE,
							    GTK_ANCHOR_NW,
							    G_PARAM_READWRITE));  

	object_class->destroy = gnome_canvas_imageframe_destroy;

	item_class->update = gnome_canvas_imageframe_update;
	item_class->realize = gnome_canvas_imageframe_realize;
	item_class->unrealize = gnome_canvas_imageframe_unrealize;
	item_class->draw = gnome_canvas_imageframe_draw;
	item_class->point = gnome_canvas_imageframe_point;
	item_class->bounds = gnome_canvas_imageframe_bounds;
	item_class->render = gnome_canvas_imageframe_render;
}

static void
gnome_canvas_imageframe_init (GnomeCanvasImageFrame *image)
{
	image->x = 0.0;
	image->y = 0.0;
	image->width = 0.0;
	image->height = 0.0;
	image->drawwidth = 0.0;
	image->anchor = GTK_ANCHOR_CENTER;
	// GTK2FIX
	// GNOME_CANVAS_ITEM(image)->object.flags |= GNOME_CANVAS_ITEM_NO_AUTO_REDRAW;
}

static void
gnome_canvas_imageframe_destroy (GtkObject *object)
{
	GnomeCanvasImageFrame *image;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_CANVAS_IS_CANVAS_IMAGEFRAME (object));

	image = GNOME_CANVAS_IMAGEFRAME (object);
	
	image->cwidth = 0;
	image->cheight = 0;

	if (image->pixbuf)
	{
		art_pixbuf_free (image->pixbuf);
		image->pixbuf = NULL;
	}

	if(GTK_OBJECT_CLASS (parent_class)->destroy)
	{
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
	}
}

/* Get's the image bounds expressed as item-relative coordinates. */
static void
get_bounds_item_relative (GnomeCanvasImageFrame *image, double *px1, double *py1, double *px2, double *py2)
{
	GnomeCanvasItem *item;
	double x, y;

	item = GNOME_CANVAS_ITEM (image);

	/* Get item coordinates */

	x = image->x;
	y = image->y;

	/* Anchor image */

	switch (image->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_SW:
		break;

	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		x -= image->width / 2;
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		x -= image->width;
		break;
	}

	switch (image->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_N:
	case GTK_ANCHOR_NE:
		break;

	case GTK_ANCHOR_W:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_E:
		y -= image->height / 2;
		break;

	case GTK_ANCHOR_SW:
	case GTK_ANCHOR_S:
	case GTK_ANCHOR_SE:
		y -= image->height;
		break;
	}

	/* Bounds */

	*px1 = x;
	*py1 = y;
	*px2 = x + image->width;
	*py2 = y + image->height;
}

static void
gnome_canvas_imageframe_set_property (GObject *object,
				      guint            prop_id,
				      const GValue   *value,
				      GParamSpec     *pspec)
{
	GnomeCanvasItem *item;
	GnomeCanvasImageFrame *image;
	int update;
	int calc_bounds;

	item = GNOME_CANVAS_ITEM (object);
	image = GNOME_CANVAS_IMAGEFRAME (object);

	update = FALSE;
	calc_bounds = FALSE;

	switch (prop_id) {
	case PROP_PIXBUF:
		if (item->canvas->aa && g_value_get_pointer (value)) {
			if (image->pixbuf != NULL)
				art_pixbuf_free (image->pixbuf);
			image->pixbuf = g_value_get_pointer (value);
		}
		update = TRUE;
		break;

	case PROP_X:
		image->x = g_value_get_double (value);
		update = TRUE;
		break;

	case PROP_Y:
		image->y = g_value_get_double (value);
		update = TRUE;
		break;

	case PROP_WIDTH:
		image->width = fabs (g_value_get_double (value));
		update = TRUE;
		break;

	case PROP_HEIGHT:
		image->height = fabs (g_value_get_double (value));
		update = TRUE;
		break;
		
	case PROP_DRAWWIDTH:
		image->drawwidth = fabs (g_value_get_double (value));
		update = TRUE;
		break;

	case PROP_ANCHOR:
		image->anchor = g_value_get_enum (value);
		update = TRUE;
		break;

	default:
		break;
	}

	if (update)
		gnome_canvas_item_request_update (item);
}

static void
gnome_canvas_imageframe_get_property (GObject *object,
				      guint            prop_id,
				      GValue   *value,
				      GParamSpec     *pspec)
{
	GnomeCanvasImageFrame *image;

	image = GNOME_CANVAS_IMAGEFRAME (object);

	switch (prop_id) {

	case PROP_X:
	        g_value_set_double (value, image->x);
		break;

	case PROP_Y:
	        g_value_set_double (value, image->y);
		break;

	case PROP_WIDTH:
	        g_value_set_double (value, image->width);
		break;
	  
	case PROP_HEIGHT:
                g_value_set_double (value, image->height);
		break;
		
	case PROP_DRAWWIDTH:
	        g_value_set_double (value, image->drawwidth);
		break;

	case PROP_ANCHOR:
	        g_value_set_enum (value, image->anchor);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gnome_canvas_imageframe_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	GnomeCanvasImageFrame *image;
	ArtDRect i_bbox, c_bbox;
	int w = 0;
	int h = 0;

	image = GNOME_CANVAS_IMAGEFRAME (item);

	if (parent_class->update)
		(* parent_class->update) (item, affine, clip_path, flags);

	/* only works for non-rotated, non-skewed transforms */
	image->cwidth = (int) (image->width * affine[0] + 0.5);
	image->cheight = (int) (image->height * affine[3] + 0.5);

	if (image->pixbuf) {
		image->need_recalc = TRUE ;
	}

	get_bounds_item_relative (image, &i_bbox.x0, &i_bbox.y0, &i_bbox.x1, &i_bbox.y1);
	art_drect_affine_transform (&c_bbox, &i_bbox, affine);

	/* these values only make sense in the non-rotated, non-skewed case */
	image->cx = c_bbox.x0;
	image->cy = c_bbox.y0;

	/* add a fudge factor */
	c_bbox.x0--;
	c_bbox.y0--;
	c_bbox.x1++;
	c_bbox.y1++;

	gnome_canvas_update_bbox (item, c_bbox.x0, c_bbox.y0, c_bbox.x1, c_bbox.y1);

	if (image->pixbuf) {
		w = image->pixbuf->width;
		h = image->pixbuf->height;
	}

	image->affine[0] = (affine[0] * image->width) / w;
	image->affine[1] = (affine[1] * image->height) / h;
	image->affine[2] = (affine[2] * image->width) / w;
	image->affine[3] = (affine[3] * image->height) / h;
	image->affine[4] = i_bbox.x0 * affine[0] + i_bbox.y0 * affine[2] + affine[4];
	image->affine[5] = i_bbox.x0 * affine[1] + i_bbox.y0 * affine[3] + affine[5];
}

static void
gnome_canvas_imageframe_realize (GnomeCanvasItem *item)
{
	GnomeCanvasImageFrame *image;

	image = GNOME_CANVAS_IMAGEFRAME (item);

	if (parent_class->realize)
		(* parent_class->realize) (item);

}

static void
gnome_canvas_imageframe_unrealize (GnomeCanvasItem *item)
{
	GnomeCanvasImageFrame *image;

	image = GNOME_CANVAS_IMAGEFRAME(item);

	if (parent_class->unrealize)
		(* parent_class->unrealize) (item);
}

static void
recalc_if_needed (GnomeCanvasImageFrame *image)
{}

static void
gnome_canvas_imageframe_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
			 int x, int y, int width, int height)
{
	fprintf(stderr, "please don't use the CanvasImageFrame item in a non-aa Canvas\n") ;
	abort() ;
}

static double
gnome_canvas_imageframe_point (GnomeCanvasItem *item, double x, double y,
			  int cx, int cy, GnomeCanvasItem **actual_item)
{
	GnomeCanvasImageFrame *image;
	int x1, y1, x2, y2;
	int dx, dy;

	image = GNOME_CANVAS_IMAGEFRAME (item);

	*actual_item = item;

	recalc_if_needed (image);

	x1 = image->cx - item->canvas->close_enough;
	y1 = image->cy - item->canvas->close_enough;
	x2 = image->cx + image->cwidth - 1 + item->canvas->close_enough;
	y2 = image->cy + image->cheight - 1 + item->canvas->close_enough;

	/* Hard case: is point inside image's gravity region? */

	//if ((cx >= x1) && (cy >= y1) && (cx <= x2) && (cy <= y2))
		//return dist_to_mask (image, cx, cy) / item->canvas->pixels_per_unit;

	/* Point is outside image */

	x1 += item->canvas->close_enough;
	y1 += item->canvas->close_enough;
	x2 -= item->canvas->close_enough;
	y2 -= item->canvas->close_enough;

	if (cx < x1)
		dx = x1 - cx;
	else if (cx > x2)
		dx = cx - x2;
	else
		dx = 0;

	if (cy < y1)
		dy = y1 - cy;
	else if (cy > y2)
		dy = cy - y2;
	else
		dy = 0;

	return sqrt (dx * dx + dy * dy) / item->canvas->pixels_per_unit;
}

static void
gnome_canvas_imageframe_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	GnomeCanvasImageFrame *image;

	image = GNOME_CANVAS_IMAGEFRAME (item);

	*x1 = image->x;
	*y1 = image->y;

	switch (image->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_SW:
		break;

	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		*x1 -= image->width / 2.0;
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		*x1 -= image->width;
		break;
	}

	switch (image->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_N:
	case GTK_ANCHOR_NE:
		break;

	case GTK_ANCHOR_W:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_E:
		*y1 -= image->height / 2.0;
		break;

	case GTK_ANCHOR_SW:
	case GTK_ANCHOR_S:
	case GTK_ANCHOR_SE:
		*y1 -= image->height;
		break;
	}

	*x2 = *x1 + image->width;
	*y2 = *y1 + image->height;
}

static void
gnome_canvas_imageframe_render      (GnomeCanvasItem *item, GnomeCanvasBuf *buf)
{
	GnomeCanvasImageFrame *image;

	image = GNOME_CANVAS_IMAGEFRAME (item);

        gnome_canvas_buf_ensure_buf (buf);

#ifdef VERBOSE
	{
		char str[128];
		art_affine_to_string (str, image->affine);
		g_print ("gnome_canvas_imageframe_render %s\n", str);
	}
#endif

	art_rgb_pixbuf_affine (buf->buf,
			buf->rect.x0, buf->rect.y0, buf->rect.x1, buf->rect.y1,
			buf->buf_rowstride,
			image->pixbuf,
			image->affine,
			ART_FILTER_NEAREST, NULL);

	buf->is_bg = 0;
}
