/* GDK - The GIMP Drawing Kit
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
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "gdkx.h"
#include "gdkvisual.h"
#include "gdkprivate-x11.h"
#include "gdkscreen-x11.h"
#include "gdkinternals.h"
#include "gdkalias.h"

struct _GdkVisualClass
{
  GObjectClass parent_class;
};

static void     gdk_visual_add            (GdkVisual *visual);
static void     gdk_visual_decompose_mask (gulong     mask,
					   gint      *shift,
					   gint      *prec);
static guint    gdk_visual_hash           (Visual    *key);
static gboolean gdk_visual_equal          (Visual    *a,
					   Visual    *b);


#ifdef G_ENABLE_DEBUG

static const gchar *const visual_names[] =
{
  "static gray",
  "grayscale",
  "static color",
  "pseudo color",
  "true color",
  "direct color",
};

#endif /* G_ENABLE_DEBUG */

GType
gdk_visual_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
      {
        sizeof (GdkVisualClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) NULL,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkVisualPrivate),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            g_intern_static_string ("GdkVisual"),
                                            &object_info, 0);
    }
  
  return object_type;
}


void
_gdk_visual_init (GdkScreen *screen)
{
  static const gint possible_depths[8] = { 32, 30, 24, 16, 15, 8, 4, 1 };
  static const GdkVisualType possible_types[6] =
    {
      GDK_VISUAL_DIRECT_COLOR,
      GDK_VISUAL_TRUE_COLOR,
      GDK_VISUAL_PSEUDO_COLOR,
      GDK_VISUAL_STATIC_COLOR,
      GDK_VISUAL_GRAYSCALE,
      GDK_VISUAL_STATIC_GRAY
    };

  GdkScreenX11 *screen_x11;
  XVisualInfo *visual_list;
  XVisualInfo visual_template;
  GdkVisualPrivate *temp_visual;
  Visual *default_xvisual;
  GdkVisualPrivate **visuals;
  int nxvisuals;
  int nvisuals;
  int i, j;
  
  g_return_if_fail (GDK_IS_SCREEN (screen));
  screen_x11 = GDK_SCREEN_X11 (screen);

  nxvisuals = 0;
  visual_template.screen = screen_x11->screen_num;
  visual_list = XGetVisualInfo (screen_x11->xdisplay, VisualScreenMask, &visual_template, &nxvisuals);
  
  visuals = g_new (GdkVisualPrivate *, nxvisuals);
  for (i = 0; i < nxvisuals; i++)
    visuals[i] = g_object_new (GDK_TYPE_VISUAL, NULL);

  default_xvisual = DefaultVisual (screen_x11->xdisplay, screen_x11->screen_num);

  nvisuals = 0;
  for (i = 0; i < nxvisuals; i++)
    {
      visuals[nvisuals]->screen = screen;
      
      if (visual_list[i].depth >= 1)
	{
#ifdef __cplusplus
	  switch (visual_list[i].c_class)
#else /* __cplusplus */
	  switch (visual_list[i].class)
#endif /* __cplusplus */
	    {
	    case StaticGray:
	      visuals[nvisuals]->visual.type = GDK_VISUAL_STATIC_GRAY;
	      break;
	    case GrayScale:
	      visuals[nvisuals]->visual.type = GDK_VISUAL_GRAYSCALE;
	      break;
	    case StaticColor:
	      visuals[nvisuals]->visual.type = GDK_VISUAL_STATIC_COLOR;
	      break;
	    case PseudoColor:
	      visuals[nvisuals]->visual.type = GDK_VISUAL_PSEUDO_COLOR;
	      break;
	    case TrueColor:
	      visuals[nvisuals]->visual.type = GDK_VISUAL_TRUE_COLOR;
	      break;
	    case DirectColor:
	      visuals[nvisuals]->visual.type = GDK_VISUAL_DIRECT_COLOR;
	      break;
	    }

	  visuals[nvisuals]->visual.depth = visual_list[i].depth;
	  visuals[nvisuals]->visual.byte_order =
	    (ImageByteOrder(screen_x11->xdisplay) == LSBFirst) ?
	    GDK_LSB_FIRST : GDK_MSB_FIRST;
	  visuals[nvisuals]->visual.red_mask = visual_list[i].red_mask;
	  visuals[nvisuals]->visual.green_mask = visual_list[i].green_mask;
	  visuals[nvisuals]->visual.blue_mask = visual_list[i].blue_mask;
	  visuals[nvisuals]->visual.colormap_size = visual_list[i].colormap_size;
	  visuals[nvisuals]->visual.bits_per_rgb = visual_list[i].bits_per_rgb;
	  visuals[nvisuals]->xvisual = visual_list[i].visual;

	  if ((visuals[nvisuals]->visual.type == GDK_VISUAL_TRUE_COLOR) ||
	      (visuals[nvisuals]->visual.type == GDK_VISUAL_DIRECT_COLOR))
	    {
	      gdk_visual_decompose_mask (visuals[nvisuals]->visual.red_mask,
					 &visuals[nvisuals]->visual.red_shift,
					 &visuals[nvisuals]->visual.red_prec);

	      gdk_visual_decompose_mask (visuals[nvisuals]->visual.green_mask,
					 &visuals[nvisuals]->visual.green_shift,
					 &visuals[nvisuals]->visual.green_prec);

	      gdk_visual_decompose_mask (visuals[nvisuals]->visual.blue_mask,
					 &visuals[nvisuals]->visual.blue_shift,
					 &visuals[nvisuals]->visual.blue_prec);
	    }
	  else
	    {
	      visuals[nvisuals]->visual.red_mask = 0;
	      visuals[nvisuals]->visual.red_shift = 0;
	      visuals[nvisuals]->visual.red_prec = 0;

	      visuals[nvisuals]->visual.green_mask = 0;
	      visuals[nvisuals]->visual.green_shift = 0;
	      visuals[nvisuals]->visual.green_prec = 0;

	      visuals[nvisuals]->visual.blue_mask = 0;
	      visuals[nvisuals]->visual.blue_shift = 0;
	      visuals[nvisuals]->visual.blue_prec = 0;
	    }
	  
	  nvisuals += 1;
	}
    }

  if (visual_list)
    XFree (visual_list);

  for (i = 0; i < nvisuals; i++)
    {
      for (j = i+1; j < nvisuals; j++)
	{
	  if (visuals[j]->visual.depth >= visuals[i]->visual.depth)
	    {
	      if ((visuals[j]->visual.depth == 8) && (visuals[i]->visual.depth == 8))
		{
		  if (visuals[j]->visual.type == GDK_VISUAL_PSEUDO_COLOR)
		    {
		      temp_visual = visuals[j];
		      visuals[j] = visuals[i];
		      visuals[i] = temp_visual;
		    }
		  else if ((visuals[i]->visual.type != GDK_VISUAL_PSEUDO_COLOR) &&
			   visuals[j]->visual.type > visuals[i]->visual.type)
		    {
		      temp_visual = visuals[j];
		      visuals[j] = visuals[i];
		      visuals[i] = temp_visual;
		    }
		}
	      else if ((visuals[j]->visual.depth > visuals[i]->visual.depth) ||
		       ((visuals[j]->visual.depth == visuals[i]->visual.depth) &&
			(visuals[j]->visual.type > visuals[i]->visual.type)))
		{
		  temp_visual = visuals[j];
		  visuals[j] = visuals[i];
		  visuals[i] = temp_visual;
		}
	    }
	}
    }

  for (i = 0; i < nvisuals; i++)
    {
      if (default_xvisual->visualid == visuals[i]->xvisual->visualid)
	screen_x11->system_visual = visuals[i];

      /* For now, we only support 8888 ARGB for the "rgba visual".
       * Additional formats (like ABGR) could be added later if they
       * turn up.
       */
      if (visuals[i]->visual.depth == 32 &&
	  (visuals[i]->visual.red_mask   == 0xff0000 &&
	   visuals[i]->visual.green_mask == 0x00ff00 &&
	   visuals[i]->visual.blue_mask  == 0x0000ff))
	{
	  screen_x11->rgba_visual = GDK_VISUAL (visuals[i]);
	}
    }

#ifdef G_ENABLE_DEBUG 
  if (_gdk_debug_flags & GDK_DEBUG_MISC)
    for (i = 0; i < nvisuals; i++)
      g_message ("visual: %s: %d",
		 visual_names[visuals[i]->visual.type],
		 visuals[i]->visual.depth);
#endif /* G_ENABLE_DEBUG */

  screen_x11->navailable_depths = 0;
  for (i = 0; i < G_N_ELEMENTS (possible_depths); i++)
    {
      for (j = 0; j < nvisuals; j++)
	{
	  if (visuals[j]->visual.depth == possible_depths[i])
	    {
	      screen_x11->available_depths[screen_x11->navailable_depths++] = visuals[j]->visual.depth;
	      break;
	    }
	}
    }

  if (screen_x11->navailable_depths == 0)
    g_error ("unable to find a usable depth");

  screen_x11->navailable_types = 0;
  for (i = 0; i < G_N_ELEMENTS (possible_types); i++)
    {
      for (j = 0; j < nvisuals; j++)
	{
	  if (visuals[j]->visual.type == possible_types[i])
	    {
	      screen_x11->available_types[screen_x11->navailable_types++] = visuals[j]->visual.type;
	      break;
	    }
	}
    }

  for (i = 0; i < nvisuals; i++)
    gdk_visual_add ((GdkVisual*) visuals[i]);

  if (screen_x11->navailable_types == 0)
    g_error ("unable to find a usable visual type");

  screen_x11->visuals = visuals;
  screen_x11->nvisuals = nvisuals;
}

/**
 * gdk_visual_get_best_depth:
 * 
 * Get the best available depth for the default GDK screen.  "Best"
 * means "largest," i.e. 32 preferred over 24 preferred over 8 bits
 * per pixel.
 * 
 * Return value: best available depth
 **/
gint
gdk_visual_get_best_depth (void)
{
  GdkScreen *screen = gdk_screen_get_default();
  
  return GDK_SCREEN_X11 (screen)->available_depths[0];
}

/**
 * gdk_visual_get_best_type:
 * 
 * Return the best available visual type for the default GDK screen.
 * 
 * Return value: best visual type
 **/
GdkVisualType
gdk_visual_get_best_type (void)
{
  GdkScreen *screen = gdk_screen_get_default();
  
  return GDK_SCREEN_X11 (screen)->available_types[0];
}

/**
 * gdk_screen_get_system_visual:
 * @screen: a #GdkScreen.
 * 
 * Get the system's default visual for @screen.
 * This is the visual for the root window of the display.
 * The return value should not be freed.
 * 
 * Return value: (transfer none): the system visual
 *
 * Since: 2.2
 **/
GdkVisual *
gdk_screen_get_system_visual (GdkScreen * screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return ((GdkVisual *) GDK_SCREEN_X11 (screen)->system_visual);
}

/**
 * gdk_visual_get_best:
 *
 * Get the visual with the most available colors for the default
 * GDK screen. The return value should not be freed.
 * 
 * Return value: (transfer none): best visual
 **/
GdkVisual*
gdk_visual_get_best (void)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (gdk_screen_get_default());

  return (GdkVisual *)screen_x11->visuals[0];
}

/**
 * gdk_visual_get_best_with_depth:
 * @depth: a bit depth
 * 
 * Get the best visual with depth @depth for the default GDK screen.
 * Color visuals and visuals with mutable colormaps are preferred
 * over grayscale or fixed-colormap visuals. The return value should not
 * be freed. %NULL may be returned if no visual supports @depth.
 * 
 * Return value: (transfer none): best visual for the given depth
 **/
GdkVisual*
gdk_visual_get_best_with_depth (gint depth)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (gdk_screen_get_default ());
  GdkVisual *return_val;
  int i;
  
  return_val = NULL;
  for (i = 0; i < screen_x11->nvisuals; i++)
    if (depth == screen_x11->visuals[i]->visual.depth)
      {
	return_val = (GdkVisual *) screen_x11->visuals[i];
	break;
      }

  return return_val;
}

/**
 * gdk_visual_get_best_with_type:
 * @visual_type: a visual type
 *
 * Get the best visual of the given @visual_type for the default GDK screen.
 * Visuals with higher color depths are considered better. The return value
 * should not be freed. %NULL may be returned if no visual has type
 * @visual_type.
 * 
 * Return value: (transfer none): best visual of the given type
 **/
GdkVisual*
gdk_visual_get_best_with_type (GdkVisualType visual_type)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (gdk_screen_get_default ());
  GdkVisual *return_val;
  int i;

  return_val = NULL;
  for (i = 0; i < screen_x11->nvisuals; i++)
    if (visual_type == screen_x11->visuals[i]->visual.type)
      {
	return_val = (GdkVisual *) screen_x11->visuals[i];
	break;
      }

  return return_val;
}

/**
 * gdk_visual_get_best_with_both:
 * @depth: a bit depth
 * @visual_type: a visual type
 *
 * Combines gdk_visual_get_best_with_depth() and gdk_visual_get_best_with_type().
 * 
 * Return value: (transfer none): best visual with both @depth and
 *     @visual_type, or %NULL if none
 **/
GdkVisual*
gdk_visual_get_best_with_both (gint          depth,
			       GdkVisualType visual_type)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (gdk_screen_get_default ());
  GdkVisual *return_val;
  int i;

  return_val = NULL;
  for (i = 0; i < screen_x11->nvisuals; i++)
    if ((depth == screen_x11->visuals[i]->visual.depth) &&
	(visual_type == screen_x11->visuals[i]->visual.type))
      {
	return_val = (GdkVisual *) screen_x11->visuals[i];
	break;
      }

  return return_val;
}

/**
 * gdk_query_depths:
 * @depths: (out) (array): return location for available depths
 * @count: (out): return location for number of available depths
 *
 * This function returns the available bit depths for the default
 * screen. It's equivalent to listing the visuals
 * (gdk_list_visuals()) and then looking at the depth field in each
 * visual, removing duplicates.
 * 
 * The array returned by this function should not be freed.
 * 
 **/
void
gdk_query_depths  (gint **depths,
		   gint  *count)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (gdk_screen_get_default ());
  
  *count = screen_x11->navailable_depths;
  *depths = screen_x11->available_depths;
}

/**
 * gdk_query_visual_types:
 * @visual_types: return location for the available visual types
 * @count: return location for the number of available visual types
 *
 * This function returns the available visual types for the default
 * screen. It's equivalent to listing the visuals
 * (gdk_list_visuals()) and then looking at the type field in each
 * visual, removing duplicates.
 * 
 * The array returned by this function should not be freed.
 **/
void
gdk_query_visual_types (GdkVisualType **visual_types,
			gint           *count)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (gdk_screen_get_default ());
  
  *count = screen_x11->navailable_types;
  *visual_types = screen_x11->available_types;
}

/**
 * gdk_screen_list_visuals:
 * @screen: the relevant #GdkScreen.
 *  
 * Lists the available visuals for the specified @screen.
 * A visual describes a hardware image data format.
 * For example, a visual might support 24-bit color, or 8-bit color,
 * and might expect pixels to be in a certain format.
 *
 * Call g_list_free() on the return value when you're finished with it.
 * 
 * Return value: (transfer container) (element-type GdkVisual):
 *     a list of visuals; the list must be freed, but not its contents
 *
 * Since: 2.2
 **/
GList *
gdk_screen_list_visuals (GdkScreen *screen)
{
  GList *list;
  GdkScreenX11 *screen_x11;
  guint i;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  screen_x11 = GDK_SCREEN_X11 (screen);
  
  list = NULL;

  for (i = 0; i < screen_x11->nvisuals; ++i)
    list = g_list_append (list, screen_x11->visuals[i]);

  return list;
}

/**
 * gdk_x11_screen_lookup_visual:
 * @screen: a #GdkScreen.
 * @xvisualid: an X Visual ID.
 *
 * Looks up the #GdkVisual for a particular screen and X Visual ID.
 *
 * Returns: (transfer none): the #GdkVisual (owned by the screen
 *   object), or %NULL if the visual ID wasn't found.
 *
 * Since: 2.2
 */
GdkVisual *
gdk_x11_screen_lookup_visual (GdkScreen *screen,
			      VisualID   xvisualid)
{
  int i;
  GdkScreenX11 *screen_x11;
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  screen_x11 = GDK_SCREEN_X11 (screen);

  for (i = 0; i < screen_x11->nvisuals; i++)
    if (xvisualid == screen_x11->visuals[i]->xvisual->visualid)
      return (GdkVisual *)  screen_x11->visuals[i];

  return NULL;
}

/**
 * gdkx_visual_get:
 * @xvisualid: a X visual id.
 *
 * Returns a #GdkVisual corresponding to a X visual. 
 *
 * Return value: the #GdkVisual.
 *
 * Deprecated:2.24: Use gdk_x11_screen_lookup_visual() instead
 */
GdkVisual*
gdkx_visual_get (VisualID xvisualid)
{
  return gdk_x11_screen_lookup_visual (gdk_screen_get_default (), xvisualid);
}

static void
gdk_visual_add (GdkVisual *visual)
{
  GdkVisualPrivate *private = (GdkVisualPrivate *) visual;
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (private->screen);
  
  if (!screen_x11->visual_hash)
    screen_x11->visual_hash = g_hash_table_new ((GHashFunc) gdk_visual_hash,
                                                (GEqualFunc) gdk_visual_equal);

  g_hash_table_insert (screen_x11->visual_hash, private->xvisual, visual);
}

static void
gdk_visual_decompose_mask (gulong  mask,
			   gint   *shift,
			   gint   *prec)
{
  *shift = 0;
  *prec = 0;

  if (mask == 0)
    {
      g_warning ("Mask is 0 in visual. Server bug ?");
      return;
    }

  while (!(mask & 0x1))
    {
      (*shift)++;
      mask >>= 1;
    }

  while (mask & 0x1)
    {
      (*prec)++;
      mask >>= 1;
    }
}

static guint
gdk_visual_hash (Visual *key)
{
  return key->visualid;
}

static gboolean
gdk_visual_equal (Visual *a,
		  Visual *b)
{
  return (a->visualid == b->visualid);
}

/**
 * gdk_x11_visual_get_xvisual:
 * @visual: a #GdkVisual.
 * 
 * Returns the X visual belonging to a #GdkVisual.
 * 
 * Return value: an Xlib <type>Visual*</type>.
 **/
Visual *
gdk_x11_visual_get_xvisual (GdkVisual *visual)
{
  g_return_val_if_fail (visual != NULL, NULL);

  return  ((GdkVisualPrivate*) visual)->xvisual;
}

/**
 * gdk_visual_get_screen:
 * @visual: a #GdkVisual
 * 
 * Gets the screen to which this visual belongs
 * 
 * Return value: (transfer none): the screen to which this visual belongs.
 *
 * Since: 2.2
 **/
GdkScreen *
gdk_visual_get_screen (GdkVisual *visual)
{
  g_return_val_if_fail (GDK_IS_VISUAL (visual), NULL);

  return  ((GdkVisualPrivate*) visual)->screen;
}

#define __GDK_VISUAL_X11_C__
#include "gdkaliasdef.c"
