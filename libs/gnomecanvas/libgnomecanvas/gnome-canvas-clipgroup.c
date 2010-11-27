#define GNOME_CANVAS_CLIPGROUP_C

/* Clipping group for GnomeCanvas
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
 *
 * Copyright (C) 1998,1999 The Free Software Foundation
 *
 * Author:
 *          Lauris Kaplinski <lauris@ximian.com>
 */

/* These includes are set up for standalone compile. If/when this codebase
   is integrated into libgnomeui, the includes will need to change. */

#include <math.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_rect.h>
#include <libart_lgpl/art_vpath.h>
#include <libart_lgpl/art_bpath.h>
#include <libart_lgpl/art_vpath.h>
#include <libart_lgpl/art_vpath_bpath.h>
#include <libart_lgpl/art_svp.h>
#include <libart_lgpl/art_svp_vpath.h>
#include <libart_lgpl/art_rect_svp.h>
#include <libart_lgpl/art_gray_svp.h>
#include <libart_lgpl/art_svp_intersect.h>
#include <libart_lgpl/art_svp_ops.h>

#include "gnome-canvas.h"
#include "gnome-canvas-util.h"
#include "gnome-canvas-clipgroup.h"

enum {
	PROP_0,
	PROP_PATH,
	PROP_WIND
};

static void gnome_canvas_clipgroup_class_init      (GnomeCanvasClipgroupClass *klass);
static void gnome_canvas_clipgroup_init            (GnomeCanvasClipgroup      *clipgroup);
static void gnome_canvas_clipgroup_destroy         (GtkObject                 *object);
static void gnome_canvas_clipgroup_set_property    (GObject                   *object,
                                                    guint                      param_id,
                                                    const GValue              *value,
                                                    GParamSpec                *pspec);
static void gnome_canvas_clipgroup_get_property    (GObject                   *object,
                                                    guint                      param_id,
                                                    GValue                    *value,
                                                    GParamSpec                *pspec);
static void gnome_canvas_clipgroup_update          (GnomeCanvasItem           *item,
                                                    double                    *affine,
                                                    ArtSVP                    *clip_path,
                                                    int                        flags);

/*
 * Generic clipping stuff
 *
 * This is somewhat slow and memory-hungry - we add extra
 * composition, extra SVP render and allocate 65536
 * bytes for each clip level. It could be done more
 * efficently per-object basis - but to make clipping
 * universal, there is no alternative to double
 * buffering (although it should be done into RGBA
 * buffer by other method than ::render to make global
 * opacity possible).
 * Using art-render could possibly optimize that a bit,
 * although I am not sure.
 */

#define GCG_BUF_WIDTH 128
#define GCG_BUF_HEIGHT 128
#define GCG_BUF_PIXELS (GCG_BUF_WIDTH * GCG_BUF_HEIGHT)
#define GCG_BUF_SIZE (GCG_BUF_WIDTH * GCG_BUF_HEIGHT * 3)

#define noSHOW_SHADOW

static guchar *gcg_buf_new (void);
static void gcg_buf_free (guchar *buf);
static guchar *gcg_mask_new (void);
static void gcg_mask_free (guchar *mask);

static void gnome_canvas_clipgroup_render (GnomeCanvasItem *item, GnomeCanvasBuf *buf);

static GnomeCanvasGroupClass *parent_class;

GType
gnome_canvas_clipgroup_get_type (void)
{
	static GType clipgroup_type;

	if (!clipgroup_type) {
		const GTypeInfo object_info = {
			sizeof (GnomeCanvasClipgroupClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gnome_canvas_clipgroup_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,			/* class_data */
			sizeof (GnomeCanvasClipgroup),
			0,			/* n_preallocs */
			(GInstanceInitFunc) gnome_canvas_clipgroup_init,
			NULL			/* value_table */
		};

		clipgroup_type = g_type_register_static (GNOME_TYPE_CANVAS_GROUP, "GnomeCanvasClipgroup",
							 &object_info, 0);
	}

	return clipgroup_type;
}

static void
gnome_canvas_clipgroup_class_init (GnomeCanvasClipgroupClass *klass)
{
        GObjectClass *gobject_class;
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

        gobject_class = (GObjectClass *) klass;
	object_class = (GtkObjectClass *) klass;
	item_class = (GnomeCanvasItemClass *) klass;
	parent_class = g_type_class_ref (GNOME_TYPE_CANVAS_GROUP);

	object_class->destroy	    = gnome_canvas_clipgroup_destroy;
	gobject_class->set_property = gnome_canvas_clipgroup_set_property;
	gobject_class->get_property = gnome_canvas_clipgroup_get_property;
	item_class->update	    = gnome_canvas_clipgroup_update;
	item_class->render	    = gnome_canvas_clipgroup_render;

        g_object_class_install_property (gobject_class,
                                         PROP_PATH,
                                         g_param_spec_pointer ("path", NULL, NULL,
                                                               (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_WIND,
                                         g_param_spec_uint ("wind", NULL, NULL,
                                                            0, G_MAXUINT, 0,
                                                            (G_PARAM_READABLE | G_PARAM_WRITABLE)));
}

static void
gnome_canvas_clipgroup_init (GnomeCanvasClipgroup *clipgroup)
{
	clipgroup->path = NULL;
	clipgroup->wind = ART_WIND_RULE_NONZERO; /* default winding rule */
	clipgroup->svp = NULL;
}

static void
gnome_canvas_clipgroup_destroy (GtkObject *object)
{
	GnomeCanvasClipgroup *clipgroup;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_CLIPGROUP (object));

	clipgroup = GNOME_CANVAS_CLIPGROUP (object);

	if (clipgroup->path) {
		gnome_canvas_path_def_unref (clipgroup->path);
		clipgroup->path = NULL;
	}
	
	if (clipgroup->svp) {
		art_svp_free (clipgroup->svp);
		clipgroup->svp = NULL;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
gnome_canvas_clipgroup_set_property (GObject      *object,
                                     guint         param_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
	GnomeCanvasItem *item;
	GnomeCanvasClipgroup *cgroup;
	GnomeCanvasPathDef *gpp;

	item = GNOME_CANVAS_ITEM (object);
	cgroup = GNOME_CANVAS_CLIPGROUP (object);

	switch (param_id) {
	case PROP_PATH:
		gpp = g_value_get_pointer (value);

		if (cgroup->path) {
			gnome_canvas_path_def_unref (cgroup->path);
			cgroup->path = NULL;
		}
		if (gpp != NULL) {
			cgroup->path = gnome_canvas_path_def_closed_parts (gpp);
		}

		gnome_canvas_item_request_update (item);
		break;

	case PROP_WIND:
		cgroup->wind = g_value_get_uint (value);
		gnome_canvas_item_request_update (item);
		break;

	default:
		break;
	}
}

static void
gnome_canvas_clipgroup_get_property (GObject    *object,
                                     guint       param_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
	GnomeCanvasClipgroup * cgroup;

	cgroup = GNOME_CANVAS_CLIPGROUP (object);

	switch (param_id) {
	case PROP_PATH:
		g_value_set_pointer (value, cgroup->path);
		break;

	case PROP_WIND:
		g_value_set_uint (value, cgroup->wind);
		break;

	default:
	        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gnome_canvas_clipgroup_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	GnomeCanvasClipgroup *clipgroup;
	ArtSvpWriter *swr;
	ArtBpath *bp;
	ArtBpath *bpath;
	ArtVpath *vpath;
	ArtSVP *svp, *svp1, *svp2;

	clipgroup = GNOME_CANVAS_CLIPGROUP (item);

	if (clipgroup->svp) {
		art_svp_free (clipgroup->svp);
		clipgroup->svp = NULL;
	}

	if (clipgroup->path) {
		bp = gnome_canvas_path_def_bpath (clipgroup->path);
		bpath = art_bpath_affine_transform (bp, affine);

		vpath = art_bez_path_to_vec (bpath, 0.25);
		art_free (bpath);

		svp1 = art_svp_from_vpath (vpath);
		art_free (vpath);
		
		swr = art_svp_writer_rewind_new (clipgroup->wind);
		art_svp_intersector (svp1, swr);

		svp2 = art_svp_writer_rewind_reap (swr);
		art_svp_free (svp1);
		
		if (clip_path != NULL) {
			svp = art_svp_intersect (svp2, clip_path);
			art_svp_free (svp2);
		} else {
			svp = svp2;
		}

		clipgroup->svp = svp;
	}

	if (GNOME_CANVAS_ITEM_CLASS (parent_class)->update)
		(GNOME_CANVAS_ITEM_CLASS (parent_class)->update) (item, affine, NULL, flags);

	if (clipgroup->svp) {
		ArtDRect cbox;
		art_drect_svp (&cbox, clipgroup->svp);
		item->x1 = MAX (item->x1, cbox.x0 - 1.0);
		item->y1 = MAX (item->y1, cbox.y0 - 1.0);
		item->x2 = MIN (item->x2, cbox.x1 + 1.0);
		item->y2 = MIN (item->y2, cbox.y1 + 1.0);
	}
}

/* non-premultiplied composition into RGB */

#define COMPOSEN11(fc,fa,bc) (((255 - (guint) (fa)) * (guint) (bc) + (guint) (fc) * (guint) (fa) + 127) / 255)

static void
gnome_canvas_clipgroup_render (GnomeCanvasItem *item, GnomeCanvasBuf *buf)
{
	GnomeCanvasClipgroup *cg;
	GnomeCanvasBuf lbuf;
	guchar *mask;

	cg = GNOME_CANVAS_CLIPGROUP (item);

	if (cg->svp) {
		gint bw, bh, sw, sh;
		gint x, y;

		/* fixme: We could optimize background handling (lauris) */

		if (buf->is_bg) {
			gnome_canvas_buf_ensure_buf (buf);
			buf->is_bg = FALSE;
			buf->is_buf = TRUE;
		}

		bw = buf->rect.x1 - buf->rect.x0;
		bh = buf->rect.y1 - buf->rect.y0;
		if ((bw < 1) || (bh < 1)) return;

		if (bw * bh <= GCG_BUF_PIXELS) {
			/* We can go with single buffer */
			sw = bw;
			sh = bh;
		} else if (bw <= (GCG_BUF_PIXELS >> 3)) {
			/* Go with row buffer */
			sw = bw;
			sh =  GCG_BUF_PIXELS / bw;
		} else if (bh <= (GCG_BUF_PIXELS >> 3)) {
			/* Go with column buffer */
			sw = GCG_BUF_PIXELS / bh;
			sh = bh;
		} else {
			/* Tile buffer */
			sw = GCG_BUF_WIDTH;
			sh = GCG_BUF_HEIGHT;
		}

		/* Set up local buffer */
		lbuf.buf = gcg_buf_new ();
		lbuf.bg_color = buf->bg_color;
		lbuf.is_bg = FALSE;
		lbuf.is_buf = TRUE;
		/* Allocate mask */
		mask = gcg_mask_new ();

		for (y = buf->rect.y0; y < buf->rect.y1; y += sh) {
			for (x = buf->rect.x0; x < buf->rect.x1; x += sw) {
				gint r, xx, yy;
				/* Set up local buffer */
				lbuf.rect.x0 = x;
				lbuf.rect.y0 = y;
				lbuf.rect.x1 = MIN (x + sw, buf->rect.x1);
				lbuf.rect.y1 = MIN (y + sh, buf->rect.y1);
				lbuf.buf_rowstride = 3 * (lbuf.rect.x1 - lbuf.rect.x0);
				/* Copy background */
				for (r = lbuf.rect.y0; r < lbuf.rect.y1; r++) {
					memcpy (lbuf.buf + (r - lbuf.rect.y0) * lbuf.buf_rowstride,
						buf->buf + (r - buf->rect.y0) * buf->buf_rowstride + (x - buf->rect.x0) * 3,
						(lbuf.rect.x1 - lbuf.rect.x0) * 3);
				}
				/* Invoke render method */
				if (((GnomeCanvasItemClass *) parent_class)->render)
					((GnomeCanvasItemClass *) parent_class)->render (item, &lbuf);
				/* Render mask */
				art_gray_svp_aa (cg->svp, lbuf.rect.x0, lbuf.rect.y0, lbuf.rect.x1, lbuf.rect.y1,
						 mask, lbuf.rect.x1 - lbuf.rect.x0);
				/* Combine */
				for (yy = lbuf.rect.y0; yy < lbuf.rect.y1; yy++) {
					guchar *s, *m, *d;
					s = lbuf.buf + (yy - lbuf.rect.y0) * lbuf.buf_rowstride;
					m = mask + (yy - lbuf.rect.y0) * (lbuf.rect.x1 - lbuf.rect.x0);
					d = buf->buf + (yy - buf->rect.y0) * buf->buf_rowstride + (x - buf->rect.x0) * 3;
					for (xx = lbuf.rect.x0; xx < lbuf.rect.x1; xx++) {
#ifndef SHOW_SHADOW
						d[0] = COMPOSEN11 (s[0], m[0], d[0]);
						d[1] = COMPOSEN11 (s[1], m[0], d[1]);
						d[2] = COMPOSEN11 (s[2], m[0], d[2]);
#else
						d[0] = COMPOSEN11 (s[0], m[0] | 0x7f, d[0]);
						d[1] = COMPOSEN11 (s[1], m[0] | 0x7f, d[1]);
						d[2] = COMPOSEN11 (s[2], m[0] | 0x7f, d[2]);
#endif
						s += 3;
						m += 1;
						d += 3;
					}
				}
			}
		}
		/* Free buffers */
		gcg_mask_free (mask);
		gcg_buf_free (lbuf.buf);
	} else {
		if (((GnomeCanvasItemClass *) parent_class)->render)
			((GnomeCanvasItemClass *) parent_class)->render (item, buf);
	}
}

static GSList *gcg_buffers = NULL;
static GSList *gcg_masks = NULL;

static guchar *
gcg_buf_new (void)
{
	guchar *buf;

	if (!gcg_buffers) {
		buf = g_new (guchar, GCG_BUF_SIZE);
	} else {
		buf = (guchar *) gcg_buffers->data;
		gcg_buffers = g_slist_remove (gcg_buffers, buf);
	}

	return buf;
}

static void
gcg_buf_free (guchar *buf)
{
	gcg_buffers = g_slist_prepend (gcg_buffers, buf);
}

static guchar *
gcg_mask_new (void)
{
	guchar *mask;

	if (!gcg_masks) {
		mask = g_new (guchar, GCG_BUF_PIXELS);
	} else {
		mask = (guchar *) gcg_masks->data;
		gcg_masks = g_slist_remove (gcg_masks, mask);
	}

	return mask;
}

static void
gcg_mask_free (guchar *mask)
{
	gcg_masks = g_slist_prepend (gcg_masks, mask);
}
