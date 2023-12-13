/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* GdkPixbuf library - Simple transformations of animations
 *
 * Copyright (C) Red Hat, Inc
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <glib.h>

#include "gdk-pixbuf.h"
#include "gdk-pixbuf-io.h"
#include "gdk-pixbuf-scaled-anim.h"


struct _GdkPixbufScaledAnimClass
{
        GdkPixbufAnimationClass parent_class;
};

struct _GdkPixbufScaledAnim
{
 	GdkPixbufAnimation parent_instance;

	GdkPixbufAnimation *anim;
	gdouble xscale;
	gdouble yscale;
	gdouble tscale;

	GdkPixbuf *current;
};

struct _GdkPixbufScaledAnimIterClass
{
        GdkPixbufAnimationClass parent_class;
};

struct _GdkPixbufScaledAnimIter
{
 	GdkPixbufAnimationIter parent_instance;

	GdkPixbufScaledAnim *scaled;
        GdkPixbufAnimationIter *iter;
};

typedef struct _GdkPixbufScaledAnimIter GdkPixbufScaledAnimIter;
typedef struct _GdkPixbufScaledAnimIterClass GdkPixbufScaledAnimIterClass;

GdkPixbufScaledAnim *
_gdk_pixbuf_scaled_anim_new (GdkPixbufAnimation *anim,
                             gdouble             xscale,
                             gdouble             yscale,
                             gdouble             tscale)
{
	GdkPixbufScaledAnim *scaled;

	scaled = g_object_new (GDK_TYPE_PIXBUF_SCALED_ANIM, NULL);

	scaled->anim = g_object_ref (anim);
	scaled->xscale = xscale;
	scaled->yscale = yscale;
	scaled->tscale = tscale;

	return scaled;
}

G_DEFINE_TYPE (GdkPixbufScaledAnim, gdk_pixbuf_scaled_anim, GDK_TYPE_PIXBUF_ANIMATION);

static void
gdk_pixbuf_scaled_anim_init (GdkPixbufScaledAnim *scaled)
{
	scaled->xscale = 1.0;
	scaled->yscale = 1.0;
	scaled->tscale = 1.0;
}

static void
gdk_pixbuf_scaled_anim_finalize (GObject *object)
{
	GdkPixbufScaledAnim *scaled = (GdkPixbufScaledAnim *)object;

	if (scaled->anim) {
		g_object_unref (scaled->anim);
		scaled->anim = NULL;
	}

	if (scaled->current) {
		g_object_unref (scaled->current);
		scaled->current = NULL;
	}

	G_OBJECT_CLASS (gdk_pixbuf_scaled_anim_parent_class)->finalize (object);
}

static gboolean
is_static_image (GdkPixbufAnimation *anim)
{
	GdkPixbufScaledAnim *scaled = (GdkPixbufScaledAnim *)anim;

	return gdk_pixbuf_animation_is_static_image (scaled->anim);
}	

static GdkPixbuf *
get_scaled_pixbuf (GdkPixbufScaledAnim *scaled, 
                   GdkPixbuf           *pixbuf)
{
	GQuark  quark;
	gchar **options;

	if (scaled->current) 
		g_object_unref (scaled->current);

	/* Preserve the options associated with the original pixbuf 
	   (if present), mostly so that client programs can use the
	   "orientation" option (if present) to rotate the image 
	   appropriately. gdk_pixbuf_scale_simple (and most other
           gdk transform operations) does not preserve the attached
           options when returning a new pixbuf. */

	quark = g_quark_from_static_string ("gdk_pixbuf_options");
	options = g_object_get_qdata (G_OBJECT (pixbuf), quark);

	/* Get a new scaled pixbuf */
	scaled->current  = gdk_pixbuf_scale_simple (pixbuf, 
                        MAX((int) ((gdouble) gdk_pixbuf_get_width (pixbuf) * scaled->xscale + .5), 1),
                        MAX((int) ((gdouble) gdk_pixbuf_get_height (pixbuf) * scaled->yscale + .5), 1),
			GDK_INTERP_BILINEAR);

	/* Copy the original pixbuf options to the scaled pixbuf */
        if (options && scaled->current)
	          g_object_set_qdata_full (G_OBJECT (scaled->current), quark, 
                                           g_strdupv (options), (GDestroyNotify) g_strfreev);

	return scaled->current;
}

static GdkPixbuf *
get_static_image (GdkPixbufAnimation *anim)
{
	GdkPixbufScaledAnim *scaled = (GdkPixbufScaledAnim *)anim;
	GdkPixbuf *pixbuf;
	
	pixbuf = gdk_pixbuf_animation_get_static_image (scaled->anim);
	return get_scaled_pixbuf (scaled, pixbuf);
}

static void
get_size (GdkPixbufAnimation *anim,
	  int                *width,
	  int 		     *height)
{
	GdkPixbufScaledAnim *scaled = (GdkPixbufScaledAnim *)anim;

        GDK_PIXBUF_ANIMATION_GET_CLASS (scaled->anim)->get_size (scaled->anim, width, height);
	if (width) 
		*width = (int)(*width * scaled->xscale + .5);
	if (height)
		*height = (int)(*height * scaled->yscale + .5);
}

static GdkPixbufAnimationIter *
get_iter (GdkPixbufAnimation *anim,
          const GTimeVal     *start_time)
{
	GdkPixbufScaledAnim *scaled = (GdkPixbufScaledAnim *)anim;
	GdkPixbufScaledAnimIter *iter;

	iter = g_object_new (GDK_TYPE_PIXBUF_SCALED_ANIM_ITER, NULL);

	iter->scaled = g_object_ref (scaled);
	iter->iter = gdk_pixbuf_animation_get_iter (scaled->anim, start_time);
	
	return (GdkPixbufAnimationIter*)iter;
}

static void
gdk_pixbuf_scaled_anim_class_init (GdkPixbufScaledAnimClass *klass)
{
        GObjectClass *object_class;
        GdkPixbufAnimationClass *anim_class;

        object_class = G_OBJECT_CLASS (klass);
        anim_class = GDK_PIXBUF_ANIMATION_CLASS (klass);
        
        object_class->finalize = gdk_pixbuf_scaled_anim_finalize;
        
        anim_class->is_static_image = is_static_image;
        anim_class->get_static_image = get_static_image;
        anim_class->get_size = get_size;
        anim_class->get_iter = get_iter;
}


G_DEFINE_TYPE (GdkPixbufScaledAnimIter, gdk_pixbuf_scaled_anim_iter, GDK_TYPE_PIXBUF_ANIMATION_ITER);

static void
gdk_pixbuf_scaled_anim_iter_init (GdkPixbufScaledAnimIter *iter)
{
}

static int
get_delay_time (GdkPixbufAnimationIter *iter)
{
	GdkPixbufScaledAnimIter *scaled = (GdkPixbufScaledAnimIter *)iter;
	int delay;

	delay = gdk_pixbuf_animation_iter_get_delay_time (scaled->iter);
	delay = (int)(delay * scaled->scaled->tscale);

	return delay;
}

static GdkPixbuf *
get_pixbuf (GdkPixbufAnimationIter *iter)
{
	GdkPixbufScaledAnimIter *scaled = (GdkPixbufScaledAnimIter *)iter;
	GdkPixbuf *pixbuf;

	pixbuf = gdk_pixbuf_animation_iter_get_pixbuf (scaled->iter);
	return get_scaled_pixbuf (scaled->scaled, pixbuf);
}

static gboolean 
on_currently_loading_frame (GdkPixbufAnimationIter *iter)
{
	GdkPixbufScaledAnimIter *scaled = (GdkPixbufScaledAnimIter *)iter;

	return gdk_pixbuf_animation_iter_on_currently_loading_frame (scaled->iter);
}

static gboolean
advance (GdkPixbufAnimationIter *iter,
	 const GTimeVal         *current_time)
{
	GdkPixbufScaledAnimIter *scaled = (GdkPixbufScaledAnimIter *)iter;

	return gdk_pixbuf_animation_iter_advance (scaled->iter, current_time);
}

static void
gdk_pixbuf_scaled_anim_iter_finalize (GObject *object)
{
        GdkPixbufScaledAnimIter *iter = (GdkPixbufScaledAnimIter *)object;
        
	g_object_unref (iter->iter);
   	g_object_unref (iter->scaled);

	G_OBJECT_CLASS (gdk_pixbuf_scaled_anim_iter_parent_class)->finalize (object);
}

static void
gdk_pixbuf_scaled_anim_iter_class_init (GdkPixbufScaledAnimIterClass *klass)
{
        GObjectClass *object_class;
        GdkPixbufAnimationIterClass *anim_iter_class;

        object_class = G_OBJECT_CLASS (klass);
        anim_iter_class = GDK_PIXBUF_ANIMATION_ITER_CLASS (klass);
        
        object_class->finalize = gdk_pixbuf_scaled_anim_iter_finalize;
        
        anim_iter_class->get_delay_time = get_delay_time;
        anim_iter_class->get_pixbuf = get_pixbuf;
        anim_iter_class->on_currently_loading_frame = on_currently_loading_frame;
        anim_iter_class->advance = advance;
}
