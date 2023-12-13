/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#undef GDK_DISABLE_DEPRECATED

#include "gtkgc.h"
#include "gtkintl.h"
#include "gtkalias.h"


typedef struct _GtkGCKey       GtkGCKey;
typedef struct _GtkGCDrawable  GtkGCDrawable;

struct _GtkGCKey
{
  gint depth;
  GdkColormap *colormap;
  GdkGCValues values;
  GdkGCValuesMask mask;
};

struct _GtkGCDrawable
{
  gint depth;
  GdkPixmap *drawable;
};


static void      gtk_gc_init             (void);
static GtkGCKey* gtk_gc_key_dup          (GtkGCKey      *key);
static void      gtk_gc_key_destroy      (GtkGCKey      *key);
static gpointer  gtk_gc_new              (gpointer       key);
static void      gtk_gc_destroy          (gpointer       value);
static guint     gtk_gc_key_hash         (gpointer       key);
static guint     gtk_gc_value_hash       (gpointer       value);
static gint      gtk_gc_key_equal        (gpointer       a,
					  gpointer       b);
static guint     gtk_gc_drawable_hash    (GtkGCDrawable *d);
static gint      gtk_gc_drawable_equal   (GtkGCDrawable *a,
					  GtkGCDrawable *b);


static gint initialize = TRUE;
static GCache *gc_cache = NULL;
static GQuark quark_gtk_gc_drawable_ht = 0;

GdkGC*
gtk_gc_get (gint             depth,
	    GdkColormap     *colormap,
	    GdkGCValues     *values,
	    GdkGCValuesMask  values_mask)
{
  GtkGCKey key;
  GdkGC *gc;

  if (initialize)
    gtk_gc_init ();

  key.depth = depth;
  key.colormap = colormap;
  key.values = *values;
  key.mask = values_mask;

  gc = g_cache_insert (gc_cache, &key);

  return gc;
}

void
gtk_gc_release (GdkGC *gc)
{
  if (initialize)
    gtk_gc_init ();

  g_cache_remove (gc_cache, gc);
}

static void 
free_gc_drawable (gpointer data)
{
  GtkGCDrawable *drawable = data;
  g_object_unref (drawable->drawable);
  g_slice_free (GtkGCDrawable, drawable);
}

static GHashTable*
gtk_gc_get_drawable_ht (GdkScreen *screen)
{
  GHashTable *ht = g_object_get_qdata (G_OBJECT (screen), quark_gtk_gc_drawable_ht);
  if (!ht)
    {
      ht = g_hash_table_new_full ((GHashFunc) gtk_gc_drawable_hash,
				  (GEqualFunc) gtk_gc_drawable_equal,
				  NULL, free_gc_drawable);
      g_object_set_qdata_full (G_OBJECT (screen), 
			       quark_gtk_gc_drawable_ht, ht, 
			       (GDestroyNotify)g_hash_table_destroy);
    }
  
  return ht;
}

static void
gtk_gc_init (void)
{
  initialize = FALSE;

  quark_gtk_gc_drawable_ht = g_quark_from_static_string ("gtk-gc-drawable-ht");

  gc_cache = g_cache_new ((GCacheNewFunc) gtk_gc_new,
			  (GCacheDestroyFunc) gtk_gc_destroy,
			  (GCacheDupFunc) gtk_gc_key_dup,
			  (GCacheDestroyFunc) gtk_gc_key_destroy,
			  (GHashFunc) gtk_gc_key_hash,
			  (GHashFunc) gtk_gc_value_hash,
			  (GEqualFunc) gtk_gc_key_equal);
}

static GtkGCKey*
gtk_gc_key_dup (GtkGCKey *key)
{
  GtkGCKey *new_key;

  new_key = g_slice_new (GtkGCKey);

  *new_key = *key;

  return new_key;
}

static void
gtk_gc_key_destroy (GtkGCKey *key)
{
  g_slice_free (GtkGCKey, key);
}

static gpointer
gtk_gc_new (gpointer key)
{
  GtkGCKey *keyval;
  GtkGCDrawable *drawable;
  GdkGC *gc;
  GHashTable *ht;
  GdkScreen *screen;

  keyval = key;
  screen = gdk_colormap_get_screen (keyval->colormap);
  
  ht = gtk_gc_get_drawable_ht (screen);
  drawable = g_hash_table_lookup (ht, &keyval->depth);
  if (!drawable)
    {
      drawable = g_slice_new (GtkGCDrawable);
      drawable->depth = keyval->depth;
      drawable->drawable = gdk_pixmap_new (gdk_screen_get_root_window (screen), 
					   1, 1, drawable->depth);
      g_hash_table_insert (ht, &drawable->depth, drawable);
    }

  gc = gdk_gc_new_with_values (drawable->drawable, &keyval->values, keyval->mask);
  gdk_gc_set_colormap (gc, keyval->colormap);

  return (gpointer) gc;
}

static void
gtk_gc_destroy (gpointer value)
{
  g_object_unref (value);
}

static guint
gtk_gc_key_hash (gpointer key)
{
  GtkGCKey *keyval;
  guint hash_val;

  keyval = key;
  hash_val = 0;

  if (keyval->mask & GDK_GC_FOREGROUND)
    {
      hash_val += keyval->values.foreground.pixel;
    }
  if (keyval->mask & GDK_GC_BACKGROUND)
    {
      hash_val += keyval->values.background.pixel;
    }
  if (keyval->mask & GDK_GC_FONT)
    {
      hash_val += gdk_font_id (keyval->values.font);
    }
  if (keyval->mask & GDK_GC_FUNCTION)
    {
      hash_val += (gint) keyval->values.function;
    }
  if (keyval->mask & GDK_GC_FILL)
    {
      hash_val += (gint) keyval->values.fill;
    }
  if (keyval->mask & GDK_GC_TILE)
    {
      hash_val += (gintptr) keyval->values.tile;
    }
  if (keyval->mask & GDK_GC_STIPPLE)
    {
      hash_val += (gintptr) keyval->values.stipple;
    }
  if (keyval->mask & GDK_GC_CLIP_MASK)
    {
      hash_val += (gintptr) keyval->values.clip_mask;
    }
  if (keyval->mask & GDK_GC_SUBWINDOW)
    {
      hash_val += (gint) keyval->values.subwindow_mode;
    }
  if (keyval->mask & GDK_GC_TS_X_ORIGIN)
    {
      hash_val += (gint) keyval->values.ts_x_origin;
    }
  if (keyval->mask & GDK_GC_TS_Y_ORIGIN)
    {
      hash_val += (gint) keyval->values.ts_y_origin;
    }
  if (keyval->mask & GDK_GC_CLIP_X_ORIGIN)
    {
      hash_val += (gint) keyval->values.clip_x_origin;
    }
  if (keyval->mask & GDK_GC_CLIP_Y_ORIGIN)
    {
      hash_val += (gint) keyval->values.clip_y_origin;
    }
  if (keyval->mask & GDK_GC_EXPOSURES)
    {
      hash_val += (gint) keyval->values.graphics_exposures;
    }
  if (keyval->mask & GDK_GC_LINE_WIDTH)
    {
      hash_val += (gint) keyval->values.line_width;
    }
  if (keyval->mask & GDK_GC_LINE_STYLE)
    {
      hash_val += (gint) keyval->values.line_style;
    }
  if (keyval->mask & GDK_GC_CAP_STYLE)
    {
      hash_val += (gint) keyval->values.cap_style;
    }
  if (keyval->mask & GDK_GC_JOIN_STYLE)
    {
      hash_val += (gint) keyval->values.join_style;
    }

  return hash_val;
}

static guint
gtk_gc_value_hash (gpointer value)
{
  return (gulong) value;
}

static gboolean
gtk_gc_key_equal (gpointer a,
		  gpointer b)
{
  GtkGCKey *akey;
  GtkGCKey *bkey;
  GdkGCValues *avalues;
  GdkGCValues *bvalues;

  akey = a;
  bkey = b;

  avalues = &akey->values;
  bvalues = &bkey->values;

  if (akey->mask != bkey->mask)
    return FALSE;

  if (akey->depth != bkey->depth)
    return FALSE;

  if (akey->colormap != bkey->colormap)
    return FALSE;

  if (akey->mask & GDK_GC_FOREGROUND)
    {
      if (avalues->foreground.pixel != bvalues->foreground.pixel)
	return FALSE;
    }
  if (akey->mask & GDK_GC_BACKGROUND)
    {
      if (avalues->background.pixel != bvalues->background.pixel)
	return FALSE;
    }
  if (akey->mask & GDK_GC_FONT)
    {
      if (!gdk_font_equal (avalues->font, bvalues->font))
	return FALSE;
    }
  if (akey->mask & GDK_GC_FUNCTION)
    {
      if (avalues->function != bvalues->function)
	return FALSE;
    }
  if (akey->mask & GDK_GC_FILL)
    {
      if (avalues->fill != bvalues->fill)
	return FALSE;
    }
  if (akey->mask & GDK_GC_TILE)
    {
      if (avalues->tile != bvalues->tile)
	return FALSE;
    }
  if (akey->mask & GDK_GC_STIPPLE)
    {
      if (avalues->stipple != bvalues->stipple)
	return FALSE;
    }
  if (akey->mask & GDK_GC_CLIP_MASK)
    {
      if (avalues->clip_mask != bvalues->clip_mask)
	return FALSE;
    }
  if (akey->mask & GDK_GC_SUBWINDOW)
    {
      if (avalues->subwindow_mode != bvalues->subwindow_mode)
	return FALSE;
    }
  if (akey->mask & GDK_GC_TS_X_ORIGIN)
    {
      if (avalues->ts_x_origin != bvalues->ts_x_origin)
	return FALSE;
    }
  if (akey->mask & GDK_GC_TS_Y_ORIGIN)
    {
      if (avalues->ts_y_origin != bvalues->ts_y_origin)
	return FALSE;
    }
  if (akey->mask & GDK_GC_CLIP_X_ORIGIN)
    {
      if (avalues->clip_x_origin != bvalues->clip_x_origin)
	return FALSE;
    }
  if (akey->mask & GDK_GC_CLIP_Y_ORIGIN)
    {
      if (avalues->clip_y_origin != bvalues->clip_y_origin)
	return FALSE;
    }
  if (akey->mask & GDK_GC_EXPOSURES)
    {
      if (avalues->graphics_exposures != bvalues->graphics_exposures)
	return FALSE;
    }
  if (akey->mask & GDK_GC_LINE_WIDTH)
    {
      if (avalues->line_width != bvalues->line_width)
	return FALSE;
    }
  if (akey->mask & GDK_GC_LINE_STYLE)
    {
      if (avalues->line_style != bvalues->line_style)
	return FALSE;
    }
  if (akey->mask & GDK_GC_CAP_STYLE)
    {
      if (avalues->cap_style != bvalues->cap_style)
	return FALSE;
    }
  if (akey->mask & GDK_GC_JOIN_STYLE)
    {
      if (avalues->join_style != bvalues->join_style)
	return FALSE;
    }

  return TRUE;
}

static guint
gtk_gc_drawable_hash (GtkGCDrawable *d)
{
  return d->depth;
}

static gboolean
gtk_gc_drawable_equal (GtkGCDrawable *a,
		       GtkGCDrawable *b)
{
  return (a->depth == b->depth);
}

#define __GTK_GC_C__
#include "gtkaliasdef.c"
