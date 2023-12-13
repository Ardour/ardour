/* GDK - The GIMP Drawing Kit
 * gdkvisual.c
 * 
 * Copyright 2001 Sun Microsystems Inc. 
 *
 * Erwann Chenede <erwann.chenede@sun.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "gdkvisual.h"
#include "gdkscreen.h"
#include "gdkalias.h"

/**
 * gdk_list_visuals:
 * 
 * Lists the available visuals for the default screen.
 * (See gdk_screen_list_visuals())
 * A visual describes a hardware image data format.
 * For example, a visual might support 24-bit color, or 8-bit color,
 * and might expect pixels to be in a certain format.
 *
 * Call g_list_free() on the return value when you're finished with it.
 * 
 * Return value: (transfer container) (element-type GdkVisual):
 *     a list of visuals; the list must be freed, but not its contents
 **/
GList*
gdk_list_visuals (void)
{
  return gdk_screen_list_visuals (gdk_screen_get_default ());
}

/**
 * gdk_visual_get_system:
 * 
 * Get the system's default visual for the default GDK screen.
 * This is the visual for the root window of the display.
 * The return value should not be freed.
 * 
 * Return value: (transfer none): system visual
 **/
GdkVisual*
gdk_visual_get_system (void)
{
  return gdk_screen_get_system_visual (gdk_screen_get_default());
}

/**
 * gdk_visual_get_visual_type:
 * @visual: A #GdkVisual.
 *
 * Returns the type of visual this is (PseudoColor, TrueColor, etc).
 *
 * Return value: A #GdkVisualType stating the type of @visual.
 *
 * Since: 2.22
 */
GdkVisualType
gdk_visual_get_visual_type (GdkVisual *visual)
{
  g_return_val_if_fail (GDK_IS_VISUAL (visual), 0);

  return visual->type;
}

/**
 * gdk_visual_get_depth:
 * @visual: A #GdkVisual.
 *
 * Returns the bit depth of this visual.
 *
 * Return value: The bit depth of this visual.
 *
 * Since: 2.22
 */
gint
gdk_visual_get_depth (GdkVisual *visual)
{
  g_return_val_if_fail (GDK_IS_VISUAL (visual), 0);

  return visual->depth;
}

/**
 * gdk_visual_get_byte_order:
 * @visual: A #GdkVisual.
 *
 * Returns the byte order of this visual.
 *
 * Return value: A #GdkByteOrder stating the byte order of @visual.
 *
 * Since: 2.22
 */
GdkByteOrder
gdk_visual_get_byte_order (GdkVisual *visual)
{
  g_return_val_if_fail (GDK_IS_VISUAL (visual), 0);

  return visual->byte_order;
}

/**
 * gdk_visual_get_colormap_size:
 * @visual: A #GdkVisual.
 *
 * Returns the size of a colormap for this visual.
 *
 * Return value: The size of a colormap that is suitable for @visual.
 *
 * Since: 2.22
 */
gint
gdk_visual_get_colormap_size (GdkVisual *visual)
{
  g_return_val_if_fail (GDK_IS_VISUAL (visual), 0);

  return visual->colormap_size;
}

/**
 * gdk_visual_get_bits_per_rgb:
 * @visual: a #GdkVisual
 *
 * Returns the number of significant bits per red, green and blue value.
 *
 * Return value: The number of significant bits per color value for @visual.
 *
 * Since: 2.22
 */
gint
gdk_visual_get_bits_per_rgb (GdkVisual *visual)
{
  g_return_val_if_fail (GDK_IS_VISUAL (visual), 0);

  return visual->bits_per_rgb;
}

/**
 * gdk_visual_get_red_pixel_details:
 * @visual: A #GdkVisual.
 * @mask: (out) (allow-none): A pointer to a #guint32 to be filled in, or %NULL.
 * @shift: (out) (allow-none): A pointer to a #gint to be filled in, or %NULL.
 * @precision: (out) (allow-none): A pointer to a #gint to be filled in, or %NULL.
 *
 * Obtains values that are needed to calculate red pixel values in TrueColor
 * and DirectColor.  The "mask" is the significant bits within the pixel.
 * The "shift" is the number of bits left we must shift a primary for it
 * to be in position (according to the "mask").  Finally, "precision" refers
 * to how much precision the pixel value contains for a particular primary.
 *
 * Since: 2.22
 */
void
gdk_visual_get_red_pixel_details (GdkVisual *visual,
                                  guint32   *mask,
                                  gint      *shift,
                                  gint      *precision)
{
  g_return_if_fail (GDK_IS_VISUAL (visual));

  if (mask)
    *mask = visual->red_mask;

  if (shift)
    *shift = visual->red_shift;

  if (precision)
    *precision = visual->red_prec;
}

/**
 * gdk_visual_get_green_pixel_details:
 * @visual: a #GdkVisual
 * @mask: (out) (allow-none): A pointer to a #guint32 to be filled in, or %NULL.
 * @shift: (out) (allow-none): A pointer to a #gint to be filled in, or %NULL.
 * @precision: (out) (allow-none): A pointer to a #gint to be filled in, or %NULL.
 *
 * Obtains values that are needed to calculate green pixel values in TrueColor
 * and DirectColor.  The "mask" is the significant bits within the pixel.
 * The "shift" is the number of bits left we must shift a primary for it
 * to be in position (according to the "mask").  Finally, "precision" refers
 * to how much precision the pixel value contains for a particular primary.
 *
 * Since: 2.22
 */
void
gdk_visual_get_green_pixel_details (GdkVisual *visual,
                                    guint32   *mask,
                                    gint      *shift,
                                    gint      *precision)
{
  g_return_if_fail (GDK_IS_VISUAL (visual));

  if (mask)
    *mask = visual->green_mask;

  if (shift)
    *shift = visual->green_shift;

  if (precision)
    *precision = visual->green_prec;
}

/**
 * gdk_visual_get_blue_pixel_details:
 * @visual: a #GdkVisual
 * @mask: (out) (allow-none): A pointer to a #guint32 to be filled in, or %NULL.
 * @shift: (out) (allow-none): A pointer to a #gint to be filled in, or %NULL.
 * @precision: (out) (allow-none): A pointer to a #gint to be filled in, or %NULL.
 *
 * Obtains values that are needed to calculate blue pixel values in TrueColor
 * and DirectColor.  The "mask" is the significant bits within the pixel.
 * The "shift" is the number of bits left we must shift a primary for it
 * to be in position (according to the "mask").  Finally, "precision" refers
 * to how much precision the pixel value contains for a particular primary.
 *
 * Since: 2.22
 */
void
gdk_visual_get_blue_pixel_details (GdkVisual *visual,
                                   guint32   *mask,
                                   gint      *shift,
                                   gint      *precision)
{
  g_return_if_fail (GDK_IS_VISUAL (visual));

  if (mask)
    *mask = visual->blue_mask;

  if (shift)
    *shift = visual->blue_shift;

  if (precision)
    *precision = visual->blue_prec;
}

#define __GDK_VISUAL_C__
#include "gdkaliasdef.c"
