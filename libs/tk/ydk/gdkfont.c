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
#include "gdkdisplay.h"
#include "gdkfont.h"
#include "gdkinternals.h"
#include "gdkalias.h"

GType
gdk_font_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    our_type = g_boxed_type_register_static (g_intern_static_string ("GdkFont"),
					     (GBoxedCopyFunc)gdk_font_ref,
					     (GBoxedFreeFunc)gdk_font_unref);
  return our_type;
}

/**
 * gdk_font_ref:
 * @font: a #GdkFont
 * 
 * Increases the reference count of a font by one.
 * 
 * Return value: @font
 **/
GdkFont*
gdk_font_ref (GdkFont *font)
{
  GdkFontPrivate *private;

  g_return_val_if_fail (font != NULL, NULL);

  private = (GdkFontPrivate*) font;
  private->ref_count += 1;
  return font;
}

/**
 * gdk_font_unref:
 * @font: a #GdkFont
 * 
 * Decreases the reference count of a font by one.
 * If the result is zero, destroys the font.
 **/
void
gdk_font_unref (GdkFont *font)
{
  GdkFontPrivate *private;
  private = (GdkFontPrivate*) font;

  g_return_if_fail (font != NULL);
  g_return_if_fail (private->ref_count > 0);

  private->ref_count -= 1;
  if (private->ref_count == 0)
    _gdk_font_destroy (font);
}

/**
 * gdk_string_width:
 * @font:  a #GdkFont
 * @string: the nul-terminated string to measure
 * 
 * Determines the width of a nul-terminated string.
 * (The distance from the origin of the string to the 
 * point where the next string in a sequence of strings
 * should be drawn)
 * 
 * Return value: the width of the string in pixels.
 **/
gint
gdk_string_width (GdkFont     *font,
		  const gchar *string)
{
  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (string != NULL, -1);

  return gdk_text_width (font, string, _gdk_font_strlen (font, string));
}

/**
 * gdk_char_width:
 * @font: a #GdkFont
 * @character: the character to measure.
 * 
 * Determines the width of a given character.
 * 
 * Return value: the width of the character in pixels.
 *
 * Deprecated: 2.2: Use gdk_text_extents() instead.
 **/
gint
gdk_char_width (GdkFont *font,
		gchar    character)
{
  g_return_val_if_fail (font != NULL, -1);

  return gdk_text_width (font, &character, 1);
}

/**
 * gdk_char_width_wc:
 * @font: a #GdkFont
 * @character: the character to measure.
 * 
 * Determines the width of a given wide character. (Encoded
 * in the wide-character encoding of the current locale).
 * 
 * Return value: the width of the character in pixels.
 **/
gint
gdk_char_width_wc (GdkFont *font,
		   GdkWChar character)
{
  g_return_val_if_fail (font != NULL, -1);

  return gdk_text_width_wc (font, &character, 1);
}

/**
 * gdk_string_measure:
 * @font: a #GdkFont
 * @string: the nul-terminated string to measure.
 * 
 * Determines the distance from the origin to the rightmost
 * portion of a nul-terminated string when drawn. This is not the
 * correct value for determining the origin of the next
 * portion when drawing text in multiple pieces.
 * See gdk_string_width().
 * 
 * Return value: the right bearing of the string in pixels.
 **/
gint
gdk_string_measure (GdkFont     *font,
                    const gchar *string)
{
  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (string != NULL, -1);

  return gdk_text_measure (font, string, _gdk_font_strlen (font, string));
}

/**
 * gdk_string_extents:
 * @font: a #GdkFont.
 * @string: the nul-terminated string to measure.
 * @lbearing: the left bearing of the string.
 * @rbearing: the right bearing of the string.
 * @width: the width of the string.
 * @ascent: the ascent of the string.
 * @descent: the descent of the string.
 * 
 * Gets the metrics of a nul-terminated string.
 **/
void
gdk_string_extents (GdkFont     *font,
		    const gchar *string,
		    gint        *lbearing,
		    gint        *rbearing,
		    gint        *width,
		    gint        *ascent,
		    gint        *descent)
{
  g_return_if_fail (font != NULL);
  g_return_if_fail (string != NULL);

  gdk_text_extents (font, string, _gdk_font_strlen (font, string),
		    lbearing, rbearing, width, ascent, descent);
}


/**
 * gdk_text_measure:
 * @font: a #GdkFont
 * @text: the text to measure.
 * @text_length: the length of the text in bytes.
 * 
 * Determines the distance from the origin to the rightmost
 * portion of a string when drawn. This is not the
 * correct value for determining the origin of the next
 * portion when drawing text in multiple pieces. 
 * See gdk_text_width().
 * 
 * Return value: the right bearing of the string in pixels.
 **/
gint
gdk_text_measure (GdkFont     *font,
                  const gchar *text,
                  gint         text_length)
{
  gint rbearing;

  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (text != NULL, -1);

  gdk_text_extents (font, text, text_length, NULL, &rbearing, NULL, NULL, NULL);
  return rbearing;
}

/**
 * gdk_char_measure:
 * @font: a #GdkFont
 * @character: the character to measure.
 * 
 * Determines the distance from the origin to the rightmost
 * portion of a character when drawn. This is not the
 * correct value for determining the origin of the next
 * portion when drawing text in multiple pieces. 
 * 
 * Return value: the right bearing of the character in pixels.
 **/
gint
gdk_char_measure (GdkFont *font,
                  gchar    character)
{
  g_return_val_if_fail (font != NULL, -1);

  return gdk_text_measure (font, &character, 1);
}

/**
 * gdk_string_height:
 * @font: a #GdkFont
 * @string: the nul-terminated string to measure.
 * 
 * Determines the total height of a given nul-terminated
 * string. This value is not generally useful, because you
 * cannot determine how this total height will be drawn in
 * relation to the baseline. See gdk_string_extents().
 * 
 * Return value: the height of the string in pixels.
 **/
gint
gdk_string_height (GdkFont     *font,
		   const gchar *string)
{
  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (string != NULL, -1);

  return gdk_text_height (font, string, _gdk_font_strlen (font, string));
}

/**
 * gdk_text_height:
 * @font: a #GdkFont
 * @text: the text to measure.
 * @text_length: the length of the text in bytes.
 * 
 * Determines the total height of a given string.
 * This value is not generally useful, because you cannot
 * determine how this total height will be drawn in
 * relation to the baseline. See gdk_text_extents().
 * 
 * Return value: the height of the string in pixels.
 **/
gint
gdk_text_height (GdkFont     *font,
		 const gchar *text,
		 gint         text_length)
{
  gint ascent, descent;

  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (text != NULL, -1);

  gdk_text_extents (font, text, text_length, NULL, NULL, NULL, &ascent, &descent);
  return ascent + descent;
}

/**
 * gdk_char_height:
 * @font: a #GdkFont
 * @character: the character to measure.
 * 
 * Determines the total height of a given character.
 * This value is not generally useful, because you cannot
 * determine how this total height will be drawn in
 * relation to the baseline. See gdk_text_extents().
 * 
 * Return value: the height of the character in pixels.
 *
 * Deprecated: 2.2: Use gdk_text_extents() instead.
 **/
gint
gdk_char_height (GdkFont *font,
		 gchar    character)
{
  g_return_val_if_fail (font != NULL, -1);

  return gdk_text_height (font, &character, 1);
}

/**
 * gdk_font_from_description:
 * @font_desc: a #PangoFontDescription.
 * 
 * Load a #GdkFont based on a Pango font description. This font will
 * only be an approximation of the Pango font, and
 * internationalization will not be handled correctly. This function
 * should only be used for legacy code that cannot be easily converted
 * to use Pango. Using Pango directly will produce better results.
 * 
 * Return value: the newly loaded font, or %NULL if the font
 * cannot be loaded.
 **/
GdkFont*
gdk_font_from_description (PangoFontDescription *font_desc)
{
  return gdk_font_from_description_for_display (gdk_display_get_default (),font_desc);
}

/**
 * gdk_font_load:
 * @font_name: a XLFD describing the font to load.
 * 
 * Loads a font.
 * 
 * The font may be newly loaded or looked up the font in a cache. 
 * You should make no assumptions about the initial reference count.
 * 
 * Return value: a #GdkFont, or %NULL if the font could not be loaded.
 **/
GdkFont*
gdk_font_load (const gchar *font_name)
{  
   return gdk_font_load_for_display (gdk_display_get_default(), font_name);
}

#define __GDK_FONT_C__
#include "gdkaliasdef.c"
