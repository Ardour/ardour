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

#undef GDK_DISABLE_DEPRECATED

#include "config.h"
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <locale.h>

#include "gdkx.h"
#include "gdkfont.h"
#include "gdkprivate-x11.h"
#include "gdkinternals.h"
#include "gdkdisplay-x11.h"
#include "gdkscreen-x11.h"
#include "gdkalias.h"

typedef struct _GdkFontPrivateX        GdkFontPrivateX;

struct _GdkFontPrivateX
{
  GdkFontPrivate base;
  /* XFontStruct *xfont; */
  /* generic pointer point to XFontStruct or XFontSet */
  gpointer xfont;
  GdkDisplay *display;

  GSList *names;
  XID xid;
};

static GHashTable *
gdk_font_name_hash_get (GdkDisplay *display)
{
  GHashTable *result;
  static GQuark font_name_quark = 0;

  if (!font_name_quark)
    font_name_quark = g_quark_from_static_string ("gdk-font-hash");

  result = g_object_get_qdata (G_OBJECT (display), font_name_quark);

  if (!result)
    {
      result = g_hash_table_new (g_str_hash, g_str_equal);
      g_object_set_qdata_full (G_OBJECT (display),
         font_name_quark, result, (GDestroyNotify) g_hash_table_destroy);
    }

  return result;
}

static GHashTable *
gdk_fontset_name_hash_get (GdkDisplay *display)
{
  GHashTable *result;
  static GQuark fontset_name_quark = 0;
  
  if (!fontset_name_quark)
    fontset_name_quark = g_quark_from_static_string ("gdk-fontset-hash");

  result = g_object_get_qdata (G_OBJECT (display), fontset_name_quark);

  if (!result)
    {
      result = g_hash_table_new (g_str_hash, g_str_equal);
      g_object_set_qdata_full (G_OBJECT (display),
         fontset_name_quark, result, (GDestroyNotify) g_hash_table_destroy);
    }

  return result;
}

/** 
 * gdk_font_get_display:
 * @font: the #GdkFont.
 *
 * Returns the #GdkDisplay for @font.
 *
 * Returns: the corresponding #GdkDisplay.
 *
 * Since: 2.2
 **/
GdkDisplay* 
gdk_font_get_display (GdkFont* font)
{
  return ((GdkFontPrivateX *)font)->display;
}

static void
gdk_font_hash_insert (GdkFontType  type, 
		      GdkFont     *font, 
		      const gchar *font_name)
{
  GdkFontPrivateX *private = (GdkFontPrivateX *)font;
  GHashTable *hash = (type == GDK_FONT_FONT) ?
    gdk_font_name_hash_get (private->display) : gdk_fontset_name_hash_get (private->display);

  private->names = g_slist_prepend (private->names, g_strdup (font_name));
  g_hash_table_insert (hash, private->names->data, font);
}

static void
gdk_font_hash_remove (GdkFontType type, 
		      GdkFont    *font)
{
  GdkFontPrivateX *private = (GdkFontPrivateX *)font;
  GSList *tmp_list;
  GHashTable *hash = (type == GDK_FONT_FONT) ?
    gdk_font_name_hash_get (private->display) : gdk_fontset_name_hash_get (private->display);

  tmp_list = private->names;
  while (tmp_list)
    {
      g_hash_table_remove (hash, tmp_list->data);
      g_free (tmp_list->data);
      
      tmp_list = tmp_list->next;
    }

  g_slist_free (private->names);
  private->names = NULL;
}

static GdkFont *
gdk_font_hash_lookup (GdkDisplay  *display, 
		      GdkFontType  type, 
		      const gchar *font_name)
{
  GdkFont *result;
  GHashTable *hash;
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  hash = (type == GDK_FONT_FONT) ? gdk_font_name_hash_get (display) : 
				   gdk_fontset_name_hash_get (display);
  if (!hash)
    return NULL;
  else
    {
      result = g_hash_table_lookup (hash, font_name);
      if (result)
	gdk_font_ref (result);
      
      return result;
    }
}

/**
 * gdk_font_load_for_display:
 * @display: a #GdkDisplay
 * @font_name: a XLFD describing the font to load.
 * @returns: a #GdkFont, or %NULL if the font could not be loaded.
 *
 * Loads a font for use on @display.
 *
 * The font may be newly loaded or looked up the font in a cache. 
 * You should make no assumptions about the initial reference count.
 *
 * Since: 2.2
 */
GdkFont *
gdk_font_load_for_display (GdkDisplay  *display, 
			   const gchar *font_name)
{
  GdkFont *font;
  GdkFontPrivateX *private;
  XFontStruct *xfont;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (font_name != NULL, NULL);
  
  font = gdk_font_hash_lookup (display, GDK_FONT_FONT, font_name);
  if (font)
    return font;

  xfont = XLoadQueryFont (GDK_DISPLAY_XDISPLAY (display), font_name);
  if (xfont == NULL)
    return NULL;

  font = gdk_font_lookup_for_display (display, xfont->fid);
  if (font != NULL) 
    {
      private = (GdkFontPrivateX *) font;
      if (xfont != private->xfont)
	XFreeFont (GDK_DISPLAY_XDISPLAY (display), xfont);

      gdk_font_ref (font);
    }
  else
    {
      private = g_new (GdkFontPrivateX, 1);
      private->display = display;
      private->xfont = xfont;
      private->base.ref_count = 1;
      private->names = NULL;
      private->xid = xfont->fid | XID_FONT_BIT;
 
      font = (GdkFont*) private;
      font->type = GDK_FONT_FONT;
      font->ascent =  xfont->ascent;
      font->descent = xfont->descent;
      
      _gdk_xid_table_insert (display, &private->xid, font);
    }

  gdk_font_hash_insert (GDK_FONT_FONT, font, font_name);

  return font;
}

/**
 * gdk_font_from_description_for_display:
 * @display: a #GdkDisplay
 * @font_desc: a #PangoFontDescription.
 * 
 * Loads a #GdkFont based on a Pango font description for use on @display. 
 * This font will only be an approximation of the Pango font, and
 * internationalization will not be handled correctly. This function
 * should only be used for legacy code that cannot be easily converted
 * to use Pango. Using Pango directly will produce better results.
 * 
 * Return value: the newly loaded font, or %NULL if the font
 * cannot be loaded.
 *
 * Since: 2.2
 */
GdkFont *
gdk_font_from_description_for_display (GdkDisplay           *display,
				       PangoFontDescription *font_desc)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (font_desc != NULL, NULL);

  return gdk_font_load_for_display (display, "fixed");
}

/**
 * gdk_fontset_load_for_display:
 * @display: a #GdkDisplay
 * @fontset_name: a comma-separated list of XLFDs describing
 *   the component fonts of the fontset to load.
 * @returns: a #GdkFont, or %NULL if the fontset could not be loaded.
 * 
 * Loads a fontset for use on @display.
 *
 * The fontset may be newly loaded or looked up in a cache. 
 * You should make no assumptions about the initial reference count.
 *
 * Since: 2.2
 */
GdkFont *
gdk_fontset_load_for_display (GdkDisplay  *display,
			      const gchar *fontset_name)
{
  GdkFont *font;
  GdkFontPrivateX *private;
  XFontSet fontset;
  gint  missing_charset_count;
  gchar **missing_charset_list;
  gchar *def_string;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  
  font = gdk_font_hash_lookup (display, GDK_FONT_FONTSET, fontset_name);
  if (font)
    return font;

  private = g_new (GdkFontPrivateX, 1);
  font = (GdkFont*) private;

  private->display = display;
  fontset = XCreateFontSet (GDK_DISPLAY_XDISPLAY (display), fontset_name,
			    &missing_charset_list, &missing_charset_count,
			    &def_string);

  if (missing_charset_count)
    {
      gint i;
      g_printerr ("The font \"%s\" does not support all the required character sets for the current locale \"%s\"\n",
                 fontset_name, setlocale (LC_ALL, NULL));
      for (i=0;i<missing_charset_count;i++)
	g_printerr ("  (Missing character set \"%s\")\n",
                    missing_charset_list[i]);
      XFreeStringList (missing_charset_list);
    }

  private->base.ref_count = 1;

  if (!fontset)
    {
      g_free (font);
      return NULL;
    }
  else
    {
      gint num_fonts;
      gint i;
      XFontStruct **font_structs;
      gchar **font_names;
      
      private->xfont = fontset;
      font->type = GDK_FONT_FONTSET;
      num_fonts = XFontsOfFontSet (fontset, &font_structs, &font_names);

      font->ascent = font->descent = 0;
      
      for (i = 0; i < num_fonts; i++)
	{
	  font->ascent = MAX (font->ascent, font_structs[i]->ascent);
	  font->descent = MAX (font->descent, font_structs[i]->descent);
	}
 
      private->names = NULL;
      gdk_font_hash_insert (GDK_FONT_FONTSET, font, fontset_name);
      
      return font;
    }
}

/**
 * gdk_fontset_load:
 * @fontset_name: a comma-separated list of XLFDs describing
 *     the component fonts of the fontset to load.
 * 
 * Loads a fontset.
 *
 * The fontset may be newly loaded or looked up in a cache. 
 * You should make no assumptions about the initial reference count.
 * 
 * Return value: a #GdkFont, or %NULL if the fontset could not be loaded.
 **/
GdkFont*
gdk_fontset_load (const gchar *fontset_name)
{
  return gdk_fontset_load_for_display (gdk_display_get_default (), fontset_name);
}

void
_gdk_font_destroy (GdkFont *font)
{
  GdkFontPrivateX *private = (GdkFontPrivateX *)font;
  
  gdk_font_hash_remove (font->type, font);
      
  switch (font->type)
    {
    case GDK_FONT_FONT:
      _gdk_xid_table_remove (private->display, private->xid);
      XFreeFont (GDK_DISPLAY_XDISPLAY (private->display),
		  (XFontStruct *) private->xfont);
      break;
    case GDK_FONT_FONTSET:
      XFreeFontSet (GDK_DISPLAY_XDISPLAY (private->display),
		    (XFontSet) private->xfont);
      break;
    default:
      g_error ("unknown font type.");
      break;
    }
  g_free (font);
}

gint
_gdk_font_strlen (GdkFont     *font,
		  const gchar *str)
{
  GdkFontPrivateX *font_private;
  gint length = 0;

  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (str != NULL, -1);

  font_private = (GdkFontPrivateX*) font;

  if (font->type == GDK_FONT_FONT)
    {
      XFontStruct *xfont = (XFontStruct *) font_private->xfont;
      if ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0))
	{
	  length = strlen (str);
	}
      else
	{
	  guint16 *string_2b = (guint16 *)str;
	    
	  while (*(string_2b++))
	    length++;
	}
    }
  else if (font->type == GDK_FONT_FONTSET)
    {
      length = strlen (str);
    }
  else
    g_error("undefined font type\n");

  return length;
}

/**
 * gdk_font_id:
 * @font: a #GdkFont.
 * 
 * Returns the X Font ID for the given font. 
 * 
 * Return value: the numeric X Font ID
 **/
gint
gdk_font_id (const GdkFont *font)
{
  const GdkFontPrivateX *font_private;

  g_return_val_if_fail (font != NULL, 0);

  font_private = (const GdkFontPrivateX*) font;

  if (font->type == GDK_FONT_FONT)
    {
      return ((XFontStruct *) font_private->xfont)->fid;
    }
  else
    {
      return 0;
    }
}

/**
 * gdk_font_equal:
 * @fonta: a #GdkFont.
 * @fontb: another #GdkFont.
 * 
 * Compares two fonts for equality. Single fonts compare equal
 * if they have the same X font ID. This operation does
 * not currently work correctly for fontsets.
 * 
 * Return value: %TRUE if the fonts are equal.
 **/
gboolean
gdk_font_equal (const GdkFont *fonta,
                const GdkFont *fontb)
{
  const GdkFontPrivateX *privatea;
  const GdkFontPrivateX *privateb;

  g_return_val_if_fail (fonta != NULL, FALSE);
  g_return_val_if_fail (fontb != NULL, FALSE);

  privatea = (const GdkFontPrivateX*) fonta;
  privateb = (const GdkFontPrivateX*) fontb;

  if (fonta->type == GDK_FONT_FONT && fontb->type == GDK_FONT_FONT)
    {
      return (((XFontStruct *) privatea->xfont)->fid ==
	      ((XFontStruct *) privateb->xfont)->fid);
    }
  else if (fonta->type == GDK_FONT_FONTSET && fontb->type == GDK_FONT_FONTSET)
    {
      gchar *namea, *nameb;

      namea = XBaseFontNameListOfFontSet((XFontSet) privatea->xfont);
      nameb = XBaseFontNameListOfFontSet((XFontSet) privateb->xfont);
      
      return (strcmp(namea, nameb) == 0);
    }
  else
    /* fontset != font */
    return FALSE;
}

/**
 * gdk_text_width:
 * @font: a #GdkFont
 * @text: the text to measure.
 * @text_length: the length of the text in bytes.
 * 
 * Determines the width of a given string.
 * 
 * Return value: the width of the string in pixels.
 **/
gint
gdk_text_width (GdkFont      *font,
		const gchar  *text,
		gint          text_length)
{
  GdkFontPrivateX *private;
  gint width;
  XFontStruct *xfont;
  XFontSet fontset;

  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (text != NULL, -1);

  private = (GdkFontPrivateX*) font;

  switch (font->type)
    {
    case GDK_FONT_FONT:
      xfont = (XFontStruct *) private->xfont;
      if ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0))
	{
	  width = XTextWidth (xfont, text, text_length);
	}
      else
	{
	  width = XTextWidth16 (xfont, (XChar2b *) text, text_length / 2);
	}
      break;
    case GDK_FONT_FONTSET:
      fontset = (XFontSet) private->xfont;
      width = XmbTextEscapement (fontset, text, text_length);
      break;
    default:
      width = 0;
    }
  return width;
}

/**
 * gdk_text_width_wc:
 * @font: a #GdkFont
 * @text: the text to measure.
 * @text_length: the length of the text in characters.
 * 
 * Determines the width of a given wide-character string.
 * 
 * Return value: the width of the string in pixels.
 **/
gint
gdk_text_width_wc (GdkFont	  *font,
		   const GdkWChar *text,
		   gint		   text_length)
{
  GdkFontPrivateX *private;
  gint width;
  XFontStruct *xfont;
  XFontSet fontset;

  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (text != NULL, -1);

  private = (GdkFontPrivateX*) font;

  switch (font->type)
    {
    case GDK_FONT_FONT:
      xfont = (XFontStruct *) private->xfont;
      if ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0))
        {
          gchar *text_8bit;
          gint i;
          text_8bit = g_new (gchar, text_length);
          for (i=0; i<text_length; i++) text_8bit[i] = text[i];
          width = XTextWidth (xfont, text_8bit, text_length);
          g_free (text_8bit);
        }
      else
        {
          width = 0;
        }
      break;
    case GDK_FONT_FONTSET:
      if (sizeof(GdkWChar) == sizeof(wchar_t))
	{
	  fontset = (XFontSet) private->xfont;
	  width = XwcTextEscapement (fontset, (wchar_t *)text, text_length);
	}
      else
	{
	  wchar_t *text_wchar;
	  gint i;
	  fontset = (XFontSet) private->xfont;
	  text_wchar = g_new(wchar_t, text_length);
	  for (i=0; i<text_length; i++) text_wchar[i] = text[i];
	  width = XwcTextEscapement (fontset, text_wchar, text_length);
	  g_free (text_wchar);
	}
      break;
    default:
      width = 0;
    }
  return width;
}

/**
 * gdk_text_extents:
 * @font: a #GdkFont
 * @text: the text to measure
 * @text_length: the length of the text in bytes. (If the
 *    font is a 16-bit font, this is twice the length
 *    of the text in characters.)
 * @lbearing: the left bearing of the string.
 * @rbearing: the right bearing of the string.
 * @width: the width of the string.
 * @ascent: the ascent of the string.
 * @descent: the descent of the string.
 * 
 * Gets the metrics of a string.
 **/
void
gdk_text_extents (GdkFont     *font,
                  const gchar *text,
                  gint         text_length,
		  gint        *lbearing,
		  gint        *rbearing,
		  gint        *width,
		  gint        *ascent,
		  gint        *descent)
{
  GdkFontPrivateX *private;
  XCharStruct overall;
  XFontStruct *xfont;
  XFontSet    fontset;
  XRectangle  ink, logical;
  int direction;
  int font_ascent;
  int font_descent;

  g_return_if_fail (font != NULL);
  g_return_if_fail (text != NULL);

  private = (GdkFontPrivateX*) font;

  switch (font->type)
    {
    case GDK_FONT_FONT:
      xfont = (XFontStruct *) private->xfont;
      if ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0))
	{
	  XTextExtents (xfont, text, text_length,
			&direction, &font_ascent, &font_descent,
			&overall);
	}
      else
	{
	  XTextExtents16 (xfont, (XChar2b *) text, text_length / 2,
			  &direction, &font_ascent, &font_descent,
			  &overall);
	}
      if (lbearing)
	*lbearing = overall.lbearing;
      if (rbearing)
	*rbearing = overall.rbearing;
      if (width)
	*width = overall.width;
      if (ascent)
	*ascent = overall.ascent;
      if (descent)
	*descent = overall.descent;
      break;
    case GDK_FONT_FONTSET:
      fontset = (XFontSet) private->xfont;
      XmbTextExtents (fontset, text, text_length, &ink, &logical);
      if (lbearing)
	*lbearing = ink.x;
      if (rbearing)
	*rbearing = ink.x + ink.width;
      if (width)
	*width = logical.width;
      if (ascent)
	*ascent = -ink.y;
      if (descent)
	*descent = ink.y + ink.height;
      break;
    }

}

/**
 * gdk_text_extents_wc:
 * @font: a #GdkFont
 * @text: the text to measure.
 * @text_length: the length of the text in character.
 * @lbearing: the left bearing of the string.
 * @rbearing: the right bearing of the string.
 * @width: the width of the string.
 * @ascent: the ascent of the string.
 * @descent: the descent of the string.
 * 
 * Gets the metrics of a string of wide characters.
 **/
void
gdk_text_extents_wc (GdkFont        *font,
		     const GdkWChar *text,
		     gint            text_length,
		     gint           *lbearing,
		     gint           *rbearing,
		     gint           *width,
		     gint           *ascent,
		     gint           *descent)
{
  GdkFontPrivateX *private;
  XCharStruct overall;
  XFontStruct *xfont;
  XFontSet    fontset;
  XRectangle  ink, logical;
  int direction;
  int font_ascent;
  int font_descent;

  g_return_if_fail (font != NULL);
  g_return_if_fail (text != NULL);

  private = (GdkFontPrivateX*) font;

  switch (font->type)
    {
    case GDK_FONT_FONT:
      {
	gchar *text_8bit;
	gint i;

	xfont = (XFontStruct *) private->xfont;
	g_return_if_fail ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0));

	text_8bit = g_new (gchar, text_length);
	for (i=0; i<text_length; i++) 
	  text_8bit[i] = text[i];

	XTextExtents (xfont, text_8bit, text_length,
		      &direction, &font_ascent, &font_descent,
		      &overall);
	g_free (text_8bit);
	
	if (lbearing)
	  *lbearing = overall.lbearing;
	if (rbearing)
	  *rbearing = overall.rbearing;
	if (width)
	  *width = overall.width;
	if (ascent)
	  *ascent = overall.ascent;
	if (descent)
	  *descent = overall.descent;
	break;
      }
    case GDK_FONT_FONTSET:
      fontset = (XFontSet) private->xfont;

      if (sizeof(GdkWChar) == sizeof(wchar_t))
	XwcTextExtents (fontset, (wchar_t *)text, text_length, &ink, &logical);
      else
	{
	  wchar_t *text_wchar;
	  gint i;
	  
	  text_wchar = g_new (wchar_t, text_length);
	  for (i = 0; i < text_length; i++)
	    text_wchar[i] = text[i];
	  XwcTextExtents (fontset, text_wchar, text_length, &ink, &logical);
	  g_free (text_wchar);
	}
      if (lbearing)
	*lbearing = ink.x;
      if (rbearing)
	*rbearing = ink.x + ink.width;
      if (width)
	*width = logical.width;
      if (ascent)
	*ascent = -ink.y;
      if (descent)
	*descent = ink.y + ink.height;
      break;
    }

}

/**
 * gdk_x11_font_get_xdisplay:
 * @font: a #GdkFont.
 * 
 * Returns the display of a #GdkFont.
 * 
 * Return value:  an Xlib <type>Display*</type>.
 **/
Display *
gdk_x11_font_get_xdisplay (GdkFont *font)
{
  g_return_val_if_fail (font != NULL, NULL);

  return GDK_DISPLAY_XDISPLAY (((GdkFontPrivateX *)font)->display);
}

/**
 * gdk_x11_font_get_xfont:
 * @font: a #GdkFont.
 * 
 * Returns the X font belonging to a #GdkFont.
 * 
 * Return value: an Xlib <type>XFontStruct*</type> or an <type>XFontSet</type>.
 **/
gpointer
gdk_x11_font_get_xfont (GdkFont *font)
{
  g_return_val_if_fail (font != NULL, NULL);

  return ((GdkFontPrivateX *)font)->xfont;
}

/**
 * gdk_x11_font_get_name:
 * @font: a #GdkFont.
 * 
 * Return the X Logical Font Description (for font->type == GDK_FONT_FONT)
 * or comma separated list of XLFDs (for font->type == GDK_FONT_FONTSET)
 * that was used to load the font. If the same font was loaded
 * via multiple names, which name is returned is undefined.
 * 
 * Return value: the name of the font. This string is owned
 *   by GDK and must not be modified or freed.
 **/
const char *
gdk_x11_font_get_name (GdkFont *font)
{
  GdkFontPrivateX *private = (GdkFontPrivateX *)font;

  g_return_val_if_fail (font != NULL, NULL);

  g_assert (private->names);

  return private->names->data;
}
     
#define __GDK_FONT_X11_C__
#include "gdkaliasdef.c"
