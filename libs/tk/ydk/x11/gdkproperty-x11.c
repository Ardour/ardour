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
#include <X11/Xatom.h>
#include <string.h>

#include "gdk.h"          /* For gdk_error_trap_push/pop() */
#include "gdkx.h"
#include "gdkproperty.h"
#include "gdkprivate.h"
#include "gdkinternals.h"
#include "gdkdisplay-x11.h"
#include "gdkscreen-x11.h"
#include "gdkselection.h"	/* only from predefined atom */
#include "gdkalias.h"

static GPtrArray *virtual_atom_array;
static GHashTable *virtual_atom_hash;

static const gchar xatoms_string[] = 
  /* These are all the standard predefined X atoms */
  "\0"  /* leave a space for None, even though it is not a predefined atom */
  "PRIMARY\0"
  "SECONDARY\0"
  "ARC\0"
  "ATOM\0"
  "BITMAP\0"
  "CARDINAL\0"
  "COLORMAP\0"
  "CURSOR\0"
  "CUT_BUFFER0\0"
  "CUT_BUFFER1\0"
  "CUT_BUFFER2\0"
  "CUT_BUFFER3\0"
  "CUT_BUFFER4\0"
  "CUT_BUFFER5\0"
  "CUT_BUFFER6\0"
  "CUT_BUFFER7\0"
  "DRAWABLE\0"
  "FONT\0"
  "INTEGER\0"
  "PIXMAP\0"
  "POINT\0"
  "RECTANGLE\0"
  "RESOURCE_MANAGER\0"
  "RGB_COLOR_MAP\0"
  "RGB_BEST_MAP\0"
  "RGB_BLUE_MAP\0"
  "RGB_DEFAULT_MAP\0"
  "RGB_GRAY_MAP\0"
  "RGB_GREEN_MAP\0"
  "RGB_RED_MAP\0"
  "STRING\0"
  "VISUALID\0"
  "WINDOW\0"
  "WM_COMMAND\0"
  "WM_HINTS\0"
  "WM_CLIENT_MACHINE\0"
  "WM_ICON_NAME\0"
  "WM_ICON_SIZE\0"
  "WM_NAME\0"
  "WM_NORMAL_HINTS\0"
  "WM_SIZE_HINTS\0"
  "WM_ZOOM_HINTS\0"
  "MIN_SPACE\0"
  "NORM_SPACE\0"
  "MAX_SPACE\0"
  "END_SPACE\0"
  "SUPERSCRIPT_X\0"
  "SUPERSCRIPT_Y\0"
  "SUBSCRIPT_X\0"
  "SUBSCRIPT_Y\0"
  "UNDERLINE_POSITION\0"
  "UNDERLINE_THICKNESS\0"
  "STRIKEOUT_ASCENT\0"
  "STRIKEOUT_DESCENT\0"
  "ITALIC_ANGLE\0"
  "X_HEIGHT\0"
  "QUAD_WIDTH\0"
  "WEIGHT\0"
  "POINT_SIZE\0"
  "RESOLUTION\0"
  "COPYRIGHT\0"
  "NOTICE\0"
  "FONT_NAME\0"
  "FAMILY_NAME\0"
  "FULL_NAME\0"
  "CAP_HEIGHT\0"
  "WM_CLASS\0"
  "WM_TRANSIENT_FOR\0"
  /* Below here, these are our additions. Increment N_CUSTOM_PREDEFINED
   * if you add any.
   */
  "CLIPBOARD\0"			/* = 69 */
;

static const gint xatoms_offset[] = {
    0,   1,   9,  19,  23,  28,  35,  44,  53,  60,  72,  84,
   96, 108, 120, 132, 144, 156, 165, 170, 178, 185, 189, 201,
  218, 232, 245, 258, 274, 287, 301, 313, 320, 329, 336, 347,
  356, 374, 387, 400, 408, 424, 438, 452, 462, 473, 483, 493,
  507, 521, 533, 545, 564, 584, 601, 619, 632, 641, 652, 659,
  670, 681, 691, 698, 708, 720, 730, 741, 750, 767
};

#define N_CUSTOM_PREDEFINED 1

#define ATOM_TO_INDEX(atom) (GPOINTER_TO_UINT(atom))
#define INDEX_TO_ATOM(atom) ((GdkAtom)GUINT_TO_POINTER(atom))

static void
insert_atom_pair (GdkDisplay *display,
		  GdkAtom     virtual_atom,
		  Atom        xatom)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);  
  
  if (!display_x11->atom_from_virtual)
    {
      display_x11->atom_from_virtual = g_hash_table_new (g_direct_hash, NULL);
      display_x11->atom_to_virtual = g_hash_table_new (g_direct_hash, NULL);
    }
  
  g_hash_table_insert (display_x11->atom_from_virtual, 
		       GDK_ATOM_TO_POINTER (virtual_atom), 
		       GUINT_TO_POINTER (xatom));
  g_hash_table_insert (display_x11->atom_to_virtual,
		       GUINT_TO_POINTER (xatom), 
		       GDK_ATOM_TO_POINTER (virtual_atom));
}

static Atom
lookup_cached_xatom (GdkDisplay *display,
		     GdkAtom     atom)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);

  if (ATOM_TO_INDEX (atom) < G_N_ELEMENTS (xatoms_offset) - N_CUSTOM_PREDEFINED)
    return ATOM_TO_INDEX (atom);
  
  if (display_x11->atom_from_virtual)
    return GPOINTER_TO_UINT (g_hash_table_lookup (display_x11->atom_from_virtual,
						  GDK_ATOM_TO_POINTER (atom)));

  return None;
}

/**
 * gdk_x11_atom_to_xatom_for_display:
 * @display: A #GdkDisplay
 * @atom: A #GdkAtom, or %GDK_NONE
 *
 * Converts from a #GdkAtom to the X atom for a #GdkDisplay
 * with the same string value. The special value %GDK_NONE
 * is converted to %None.
 *
 * Return value: the X atom corresponding to @atom, or %None
 *
 * Since: 2.2
 **/
Atom
gdk_x11_atom_to_xatom_for_display (GdkDisplay *display,
				   GdkAtom     atom)
{
  Atom xatom = None;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), None);

  if (atom == GDK_NONE)
    return None;

  if (display->closed)
    return None;

  xatom = lookup_cached_xatom (display, atom);

  if (!xatom)
    {
      char *name;

      g_return_val_if_fail (ATOM_TO_INDEX (atom) < virtual_atom_array->len, None);

      name = g_ptr_array_index (virtual_atom_array, ATOM_TO_INDEX (atom));

      xatom = XInternAtom (GDK_DISPLAY_XDISPLAY (display), name, FALSE);
      insert_atom_pair (display, atom, xatom);
    }

  return xatom;
}

void
_gdk_x11_precache_atoms (GdkDisplay          *display,
			 const gchar * const *atom_names,
			 gint                 n_atoms)
{
  Atom *xatoms;
  GdkAtom *atoms;
  const gchar **xatom_names;
  gint n_xatoms;
  gint i;

  xatoms = g_new (Atom, n_atoms);
  xatom_names = g_new (const gchar *, n_atoms);
  atoms = g_new (GdkAtom, n_atoms);

  n_xatoms = 0;
  for (i = 0; i < n_atoms; i++)
    {
      GdkAtom atom = gdk_atom_intern_static_string (atom_names[i]);
      if (lookup_cached_xatom (display, atom) == None)
	{
	  atoms[n_xatoms] = atom;
	  xatom_names[n_xatoms] = atom_names[i];
	  n_xatoms++;
	}
    }

  if (n_xatoms)
    {
#ifdef HAVE_XINTERNATOMS
      XInternAtoms (GDK_DISPLAY_XDISPLAY (display),
		    (char **)xatom_names, n_xatoms, False, xatoms);
#else
      for (i = 0; i < n_xatoms; i++)
	xatoms[i] = XInternAtom (GDK_DISPLAY_XDISPLAY (display),
				 xatom_names[i], False);
#endif
    }

  for (i = 0; i < n_xatoms; i++)
    insert_atom_pair (display, atoms[i], xatoms[i]);

  g_free (xatoms);
  g_free (xatom_names);
  g_free (atoms);
}

/**
 * gdk_x11_atom_to_xatom:
 * @atom: A #GdkAtom 
 * 
 * Converts from a #GdkAtom to the X atom for the default GDK display
 * with the same string value.
 * 
 * Return value: the X atom corresponding to @atom.
 **/
Atom
gdk_x11_atom_to_xatom (GdkAtom atom)
{
  return gdk_x11_atom_to_xatom_for_display (gdk_display_get_default (), atom);
}

/**
 * gdk_x11_xatom_to_atom_for_display:
 * @display: A #GdkDisplay
 * @xatom: an X atom 
 * 
 * Convert from an X atom for a #GdkDisplay to the corresponding
 * #GdkAtom.
 * 
 * Return value: the corresponding #GdkAtom.
 *
 * Since: 2.2
 **/
GdkAtom
gdk_x11_xatom_to_atom_for_display (GdkDisplay *display,
				   Atom	       xatom)
{
  GdkDisplayX11 *display_x11;
  GdkAtom virtual_atom = GDK_NONE;
  
  g_return_val_if_fail (GDK_IS_DISPLAY (display), GDK_NONE);

  if (xatom == None)
    return GDK_NONE;

  if (display->closed)
    return GDK_NONE;

  display_x11 = GDK_DISPLAY_X11 (display);
  
  if (xatom < G_N_ELEMENTS (xatoms_offset) - N_CUSTOM_PREDEFINED)
    return INDEX_TO_ATOM (xatom);
  
  if (display_x11->atom_to_virtual)
    virtual_atom = GDK_POINTER_TO_ATOM (g_hash_table_lookup (display_x11->atom_to_virtual,
							     GUINT_TO_POINTER (xatom)));
  
  if (!virtual_atom)
    {
      /* If this atom doesn't exist, we'll die with an X error unless
       * we take precautions
       */
      char *name;
      gdk_error_trap_push ();
      name = XGetAtomName (GDK_DISPLAY_XDISPLAY (display), xatom);
      if (gdk_error_trap_pop ())
	{
	  g_warning (G_STRLOC " invalid X atom: %ld", xatom);
	}
      else
	{
	  virtual_atom = gdk_atom_intern (name, FALSE);
	  XFree (name);
	  
	  insert_atom_pair (display, virtual_atom, xatom);
	}
    }

  return virtual_atom;
}

/**
 * gdk_x11_xatom_to_atom:
 * @xatom: an X atom for the default GDK display
 * 
 * Convert from an X atom for the default display to the corresponding
 * #GdkAtom.
 * 
 * Return value: the corresponding G#dkAtom.
 **/
GdkAtom
gdk_x11_xatom_to_atom (Atom xatom)
{
  return gdk_x11_xatom_to_atom_for_display (gdk_display_get_default (), xatom);
}

static void
virtual_atom_check_init (void)
{
  if (!virtual_atom_hash)
    {
      gint i;
      
      virtual_atom_hash = g_hash_table_new (g_str_hash, g_str_equal);
      virtual_atom_array = g_ptr_array_new ();
      
      for (i = 0; i < G_N_ELEMENTS (xatoms_offset); i++)
	{
	  g_ptr_array_add (virtual_atom_array, (gchar *)(xatoms_string + xatoms_offset[i]));
	  g_hash_table_insert (virtual_atom_hash, (gchar *)(xatoms_string + xatoms_offset[i]),
			       GUINT_TO_POINTER (i));
	}
    }
}

static GdkAtom
intern_atom (const gchar *atom_name, 
	     gboolean     dup)
{
  GdkAtom result;

  virtual_atom_check_init ();
  
  result = GDK_POINTER_TO_ATOM (g_hash_table_lookup (virtual_atom_hash, atom_name));
  if (!result)
    {
      result = INDEX_TO_ATOM (virtual_atom_array->len);
      
      g_ptr_array_add (virtual_atom_array, dup ? g_strdup (atom_name) : (gchar *)atom_name);
      g_hash_table_insert (virtual_atom_hash, 
			   g_ptr_array_index (virtual_atom_array,
					      ATOM_TO_INDEX (result)),
			   GDK_ATOM_TO_POINTER (result));
    }

  return result;
}

GdkAtom
gdk_atom_intern (const gchar *atom_name, 
		 gboolean     only_if_exists)
{
  return intern_atom (atom_name, TRUE);
}

/**
 * gdk_atom_intern_static_string:
 * @atom_name: a static string
 *
 * Finds or creates an atom corresponding to a given string.
 *
 * Note that this function is identical to gdk_atom_intern() except
 * that if a new #GdkAtom is created the string itself is used rather 
 * than a copy. This saves memory, but can only be used if the string 
 * will <emphasis>always</emphasis> exist. It can be used with statically
 * allocated strings in the main program, but not with statically 
 * allocated memory in dynamically loaded modules, if you expect to
 * ever unload the module again (e.g. do not use this function in
 * GTK+ theme engines).
 *
 * Returns: the atom corresponding to @atom_name
 * 
 * Since: 2.10
 */
GdkAtom
gdk_atom_intern_static_string (const gchar *atom_name)
{
  return intern_atom (atom_name, FALSE);
}

static const char *
get_atom_name (GdkAtom atom)
{
  virtual_atom_check_init ();

  if (ATOM_TO_INDEX (atom) < virtual_atom_array->len)
    return g_ptr_array_index (virtual_atom_array, ATOM_TO_INDEX (atom));
  else
    return NULL;
}

gchar *
gdk_atom_name (GdkAtom atom)
{
  return g_strdup (get_atom_name (atom));
}

/**
 * gdk_x11_get_xatom_by_name_for_display:
 * @display: a #GdkDisplay
 * @atom_name: a string
 * 
 * Returns the X atom for a #GdkDisplay corresponding to @atom_name.
 * This function caches the result, so if called repeatedly it is much
 * faster than XInternAtom(), which is a round trip to the server each time.
 * 
 * Return value: a X atom for a #GdkDisplay
 *
 * Since: 2.2
 **/
Atom
gdk_x11_get_xatom_by_name_for_display (GdkDisplay  *display,
				       const gchar *atom_name)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), None);
  return gdk_x11_atom_to_xatom_for_display (display,
					    gdk_atom_intern (atom_name, FALSE));
}

/**
 * gdk_x11_get_xatom_by_name:
 * @atom_name: a string
 * 
 * Returns the X atom for GDK's default display corresponding to @atom_name.
 * This function caches the result, so if called repeatedly it is much
 * faster than XInternAtom(), which is a round trip to the server each time.
 * 
 * Return value: a X atom for GDK's default display.
 **/
Atom
gdk_x11_get_xatom_by_name (const gchar *atom_name)
{
  return gdk_x11_get_xatom_by_name_for_display (gdk_display_get_default (),
						atom_name);
}

/**
 * gdk_x11_get_xatom_name_for_display:
 * @display: the #GdkDisplay where @xatom is defined
 * @xatom: an X atom 
 * 
 * Returns the name of an X atom for its display. This
 * function is meant mainly for debugging, so for convenience, unlike
 * XAtomName() and gdk_atom_name(), the result doesn't need to
 * be freed. 
 *
 * Return value: name of the X atom; this string is owned by GDK,
 *   so it shouldn't be modifed or freed. 
 *
 * Since: 2.2
 **/
const gchar *
gdk_x11_get_xatom_name_for_display (GdkDisplay *display,
				    Atom        xatom)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return get_atom_name (gdk_x11_xatom_to_atom_for_display (display, xatom));
}

/**
 * gdk_x11_get_xatom_name:
 * @xatom: an X atom for GDK's default display
 * 
 * Returns the name of an X atom for GDK's default display. This
 * function is meant mainly for debugging, so for convenience, unlike
 * <function>XAtomName()</function> and gdk_atom_name(), the result 
 * doesn't need to be freed. Also, this function will never return %NULL, 
 * even if @xatom is invalid.
 * 
 * Return value: name of the X atom; this string is owned by GTK+,
 *   so it shouldn't be modifed or freed. 
 **/
const gchar *
gdk_x11_get_xatom_name (Atom xatom)
{
  return get_atom_name (gdk_x11_xatom_to_atom (xatom));
}

gboolean
gdk_property_get (GdkWindow   *window,
		  GdkAtom      property,
		  GdkAtom      type,
		  gulong       offset,
		  gulong       length,
		  gint         pdelete,
		  GdkAtom     *actual_property_type,
		  gint        *actual_format_type,
		  gint        *actual_length,
		  guchar     **data)
{
  GdkDisplay *display;
  Atom ret_prop_type;
  gint ret_format;
  gulong ret_nitems;
  gulong ret_bytes_after;
  gulong get_length;
  gulong ret_length;
  guchar *ret_data;
  Atom xproperty;
  Atom xtype;
  int res;

  g_return_val_if_fail (!window || GDK_WINDOW_IS_X11 (window), FALSE);

  if (!window)
    {
      GdkScreen *screen = gdk_screen_get_default ();
      window = gdk_screen_get_root_window (screen);
      
      GDK_NOTE (MULTIHEAD, g_message ("gdk_property_get(): window is NULL\n"));
    }
  else if (!GDK_WINDOW_IS_X11 (window))
    return FALSE;

  if (GDK_WINDOW_DESTROYED (window))
    return FALSE;

  display = gdk_drawable_get_display (window);
  xproperty = gdk_x11_atom_to_xatom_for_display (display, property);
  if (type == GDK_NONE)
    xtype = AnyPropertyType;
  else
    xtype = gdk_x11_atom_to_xatom_for_display (display, type);

  ret_data = NULL;
  
  /* 
   * Round up length to next 4 byte value.  Some code is in the (bad?)
   * habit of passing G_MAXLONG as the length argument, causing an
   * overflow to negative on the add.  In this case, we clamp the
   * value to G_MAXLONG.
   */
  get_length = length + 3;
  if (get_length > G_MAXLONG)
    get_length = G_MAXLONG;

  /* To fail, either the user passed 0 or G_MAXULONG */
  get_length = get_length / 4;
  if (get_length == 0)
    {
      g_warning ("gdk_propery-get(): invalid length 0");
      return FALSE;
    }

  res = XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display),
			    GDK_WINDOW_XWINDOW (window), xproperty,
			    offset, get_length, pdelete,
			    xtype, &ret_prop_type, &ret_format,
			    &ret_nitems, &ret_bytes_after,
			    &ret_data);

  if (res != Success || (ret_prop_type == None && ret_format == 0))
    {
      return FALSE;
    }

  if (actual_property_type)
    *actual_property_type = gdk_x11_xatom_to_atom_for_display (display, ret_prop_type);
  if (actual_format_type)
    *actual_format_type = ret_format;

  if ((xtype != AnyPropertyType) && (ret_prop_type != xtype))
    {
      XFree (ret_data);
      g_warning ("Couldn't match property type %s to %s\n", 
		 gdk_x11_get_xatom_name_for_display (display, ret_prop_type), 
		 gdk_x11_get_xatom_name_for_display (display, xtype));
      return FALSE;
    }

  /* FIXME: ignoring bytes_after could have very bad effects */

  if (data)
    {
      if (ret_prop_type == XA_ATOM ||
	  ret_prop_type == gdk_x11_get_xatom_by_name_for_display (display, "ATOM_PAIR"))
	{
	  /*
	   * data is an array of X atom, we need to convert it
	   * to an array of GDK Atoms
	   */
	  gint i;
	  GdkAtom *ret_atoms = g_new (GdkAtom, ret_nitems);
	  Atom *xatoms = (Atom *)ret_data;

	  *data = (guchar *)ret_atoms;

	  for (i = 0; i < ret_nitems; i++)
	    ret_atoms[i] = gdk_x11_xatom_to_atom_for_display (display, xatoms[i]);
	  
	  if (actual_length)
	    *actual_length = ret_nitems * sizeof (GdkAtom);
	}
      else
	{
	  switch (ret_format)
	    {
	    case 8:
	      ret_length = ret_nitems;
	      break;
	    case 16:
	      ret_length = sizeof(short) * ret_nitems;
	      break;
	    case 32:
	      ret_length = sizeof(long) * ret_nitems;
	      break;
	    default:
	      g_warning ("unknown property return format: %d", ret_format);
	      XFree (ret_data);
	      return FALSE;
	    }
	  
	  *data = g_new (guchar, ret_length);
	  memcpy (*data, ret_data, ret_length);
	  if (actual_length)
	    *actual_length = ret_length;
	}
    }

  XFree (ret_data);

  return TRUE;
}

void
gdk_property_change (GdkWindow    *window,
		     GdkAtom       property,
		     GdkAtom       type,
		     gint          format,
		     GdkPropMode   mode,
		     const guchar *data,
		     gint          nelements)
{
  GdkDisplay *display;
  Window xwindow;
  Atom xproperty;
  Atom xtype;

  g_return_if_fail (!window || GDK_WINDOW_IS_X11 (window));

  if (!window)
    {
      GdkScreen *screen;
      
      screen = gdk_screen_get_default ();
      window = gdk_screen_get_root_window (screen);
      
      GDK_NOTE (MULTIHEAD, g_message ("gdk_property_change(): window is NULL\n"));
    }
  else if (!GDK_WINDOW_IS_X11 (window))
    return;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  gdk_window_ensure_native (window);

  display = gdk_drawable_get_display (window);
  xproperty = gdk_x11_atom_to_xatom_for_display (display, property);
  xtype = gdk_x11_atom_to_xatom_for_display (display, type);
  xwindow = GDK_WINDOW_XID (window);

  if (xtype == XA_ATOM ||
      xtype == gdk_x11_get_xatom_by_name_for_display (display, "ATOM_PAIR"))
    {
      /*
       * data is an array of GdkAtom, we need to convert it
       * to an array of X Atoms
       */
      gint i;
      GdkAtom *atoms = (GdkAtom*) data;
      Atom *xatoms;

      xatoms = g_new (Atom, nelements);
      for (i = 0; i < nelements; i++)
	xatoms[i] = gdk_x11_atom_to_xatom_for_display (display, atoms[i]);

      XChangeProperty (GDK_DISPLAY_XDISPLAY (display), xwindow,
		       xproperty, xtype,
		       format, mode, (guchar *)xatoms, nelements);
      g_free (xatoms);
    }
  else
    XChangeProperty (GDK_DISPLAY_XDISPLAY (display), xwindow, xproperty, 
		     xtype, format, mode, (guchar *)data, nelements);
}

void
gdk_property_delete (GdkWindow *window,
		     GdkAtom    property)
{
  g_return_if_fail (!window || GDK_WINDOW_IS_X11 (window));

  if (!window)
    {
      GdkScreen *screen = gdk_screen_get_default ();
      window = gdk_screen_get_root_window (screen);
      
      GDK_NOTE (MULTIHEAD, 
		g_message ("gdk_property_delete(): window is NULL\n"));
    }
  else if (!GDK_WINDOW_IS_X11 (window))
    return;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  XDeleteProperty (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XWINDOW (window),
		   gdk_x11_atom_to_xatom_for_display (GDK_WINDOW_DISPLAY (window),
						      property));
}

#define __GDK_PROPERTY_X11_C__
#include "gdkaliasdef.c"
