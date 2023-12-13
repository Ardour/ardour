/* GDK - The GIMP Drawing Kit
 * Copyright © 2005 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Based on code from xftdpy.c
 *
 * Copyright © 2000 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
#include <stdlib.h>
#include <string.h>

#include <fontconfig/fontconfig.h>

#ifndef FC_HINT_STYLE
#define FC_HINT_NONE        0
#define FC_HINT_SLIGHT      1
#define FC_HINT_MEDIUM      2
#define FC_HINT_FULL        3
#endif

#include <gdkscreen-x11.h>
#include <gdkx.h>

static gint
parse_boolean (char *v)
{
  gchar c0, c1;
  
  c0 = *v;
  if (g_ascii_isupper ((int)c0))
    c0 = g_ascii_tolower (c0);
  if (c0 == 't' || c0 == 'y' || c0 == '1')
    return 1;
  if (c0 == 'f' || c0 == 'n' || c0 == '0')
    return 0;
  if (c0 == 'o')
    {
      c1 = v[1];
      if (g_ascii_isupper ((int)c1))
	c1 = g_ascii_tolower (c1);
      if (c1 == 'n')
	return 1;
      if (c1 == 'f')
	return 0;
    }
  
  return -1;
}

static gboolean
get_boolean_default (Display *dpy,
		     gchar   *option,
		     gboolean *value)
{
  gchar *v;
  gint i;
  
  v = XGetDefault (dpy, "Xft", option);
  if (v)
    {
      i = parse_boolean (v);
      if (i >= 0)
	{
	  *value = i;
	  return TRUE;
	}
    }
  
  return FALSE;
}

static gboolean
get_double_default (Display *dpy,
		    gchar   *option,
		    gdouble *value)
{
  gchar    *v, *e;
  
  v = XGetDefault (dpy, "Xft", option);
  if (v)
    {
      /* Xft uses strtod, though localization probably wasn't
       * desired. For compatibility, we use the conservative
       * g_strtod() that accepts either localized or non-localized
       * decimal separator.
       */
      *value = g_strtod (v, &e);
      if (e != v)
	return TRUE;
    }
  
  return FALSE;
}

static gboolean
get_integer_default (Display *dpy,
		     gchar   *option,
		     gint    *value)
{
  gchar *v, *e;
  
  v = XGetDefault (dpy, "Xft", option);
  if (v)
    {
      if (FcNameConstant ((FcChar8 *) v, value))
	return TRUE;
      
      *value = strtol (v, &e, 0);
      if (e != v)
	return TRUE;
    }
  
  return FALSE;
}

static void
init_xft_settings (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);
  Display *xdisplay = GDK_SCREEN_XDISPLAY (screen);
  int xscreen = GDK_SCREEN_XNUMBER (screen);
  double dpi_double;

  if (screen_x11->xft_init)
    return;

  screen_x11->xft_init = TRUE;

  if (!get_boolean_default (xdisplay, "antialias", &screen_x11->xft_antialias))
    screen_x11->xft_antialias = TRUE;

  if (!get_boolean_default (xdisplay, "hinting", &screen_x11->xft_hinting))
    screen_x11->xft_hinting = TRUE;

  if (!get_integer_default (xdisplay, "hintstyle", &screen_x11->xft_hintstyle))
    screen_x11->xft_hintstyle = FC_HINT_FULL;

  if (!get_integer_default (xdisplay, "rgba", &screen_x11->xft_rgba))
			    
    {
      int subpixel = FC_RGBA_UNKNOWN;
      
#if RENDER_MAJOR > 0 || RENDER_MINOR >= 6
      if (_gdk_x11_have_render (screen_x11->display))
	{
	  int render_order = XRenderQuerySubpixelOrder (xdisplay, xscreen);
	  
	  switch (render_order)
	    {
	    default:
	    case SubPixelUnknown:
	      subpixel = FC_RGBA_UNKNOWN;
	      break;
	    case SubPixelHorizontalRGB:
	      subpixel = FC_RGBA_RGB;
	      break;
	    case SubPixelHorizontalBGR:
	      subpixel = FC_RGBA_BGR;
	      break;
	    case SubPixelVerticalRGB:
	      subpixel = FC_RGBA_VRGB;
	      break;
	    case SubPixelVerticalBGR:
	      subpixel = FC_RGBA_VBGR;
	      break;
	    case SubPixelNone:
	      subpixel = FC_RGBA_NONE;
	      break;
	    }
	}
#endif

      screen_x11->xft_rgba = subpixel;
    }

  if (!get_double_default (xdisplay, "dpi", &dpi_double))
    dpi_double = (((double) DisplayHeight (xdisplay, xscreen) * 25.4) / 
		  (double) DisplayHeightMM (xdisplay, xscreen));

  screen_x11->xft_dpi = (int)(0.5 + PANGO_SCALE * dpi_double);
}

gboolean
_gdk_x11_get_xft_setting (GdkScreen   *screen,
			  const gchar *name,
			  GValue      *value)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);
  
  if (strncmp (name, "gtk-xft-", 8) != 0)
    return FALSE;

  name += 8;

  init_xft_settings (screen);

  if (strcmp (name, "antialias") == 0)
    {
      g_value_set_int (value, screen_x11->xft_antialias);
      return TRUE;
    }
  else if (strcmp (name, "hinting") == 0)
    {
      g_value_set_int (value, screen_x11->xft_hinting);
      return TRUE;
    }
  else if (strcmp (name, "hintstyle") == 0)
    {
      const char *str;
      
      switch (screen_x11->xft_hintstyle)
	{
	case FC_HINT_NONE:
	  str = "hintnone";
	  break;
	case FC_HINT_SLIGHT:
	  str = "hintslight";
	  break;
	case FC_HINT_MEDIUM:
	  str = "hintmedium";
	  break;
	case FC_HINT_FULL:
	  str = "hintfull";
	  break;
	default:
	  return FALSE;
	}

      g_value_set_string (value, str);
      return TRUE;
    }
  else if (strcmp (name, "rgba") == 0)
    {
      const char *str;
      
      switch (screen_x11->xft_rgba)
	{
	case FC_RGBA_NONE:
	  str = "none";
	  break;
	case FC_RGBA_RGB:
	  str = "rgb";
	  break;
	case FC_RGBA_BGR:
	  str = "bgr";
	  break;
	case FC_RGBA_VRGB:
	  str = "vrgb";
	  break;
	case FC_RGBA_VBGR:
	  str = "vbgr";
	  break;
	case FC_RGBA_UNKNOWN:
	default:
	  return FALSE;
	}
	
      g_value_set_string (value, str);
      return TRUE; 
   }
  else if (strcmp (name, "dpi") == 0)
    {
      g_value_set_int (value, screen_x11->xft_dpi);
      return TRUE;
    }

  return FALSE;
}
