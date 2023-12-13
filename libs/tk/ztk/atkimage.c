/* ATK -  Accessibility Toolkit
 * Copyright 2001 Sun Microsystems Inc.
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

#include "config.h"

#include "atkimage.h"

/**
 * SECTION:atkimage
 * @Short_description: The ATK Interface implemented by components
 *  which expose image or pixmap content on-screen.
 * @Title:AtkImage
 *
 * #AtkImage should be implemented by #AtkObject subtypes on behalf of
 * components which display image/pixmap information onscreen, and
 * which provide information (other than just widget borders, etc.)
 * via that image content.  For instance, icons, buttons with icons,
 * toolbar elements, and image viewing panes typically should
 * implement #AtkImage.
 *
 * #AtkImage primarily provides two types of information: coordinate
 * information (useful for screen review mode of screenreaders, and
 * for use by onscreen magnifiers), and descriptive information.  The
 * descriptive information is provided for alternative, text-only
 * presentation of the most significant information present in the
 * image.
 */

GType
atk_image_get_type (void)
{
  static GType type = 0;

  if (!type) {
    static const GTypeInfo tinfo =
    {
      sizeof (AtkImageIface),
      (GBaseInitFunc) NULL,
      (GBaseFinalizeFunc) NULL
    };

    type = g_type_register_static (G_TYPE_INTERFACE, "AtkImage", &tinfo, 0);
  }

  return type;
}

/**
 * atk_image_get_image_description:
 * @image: a #GObject instance that implements AtkImageIface
 *
 * Get a textual description of this image.
 *
 * Returns: a string representing the image description
 **/
const gchar*
atk_image_get_image_description (AtkImage *image)
{
  AtkImageIface *iface;

  g_return_val_if_fail (ATK_IS_IMAGE (image), NULL);

  iface = ATK_IMAGE_GET_IFACE (image);

  if (iface->get_image_description)
    {
      return (iface->get_image_description) (image);
    }
  else
    {
      return NULL;
    }
}

/**
 * atk_image_get_image_size:
 * @image: a #GObject instance that implements AtkImageIface
 * @width: filled with the image width, or -1 if the value cannot be obtained.
 * @height: filled with the image height, or -1 if the value cannot be obtained.
 *
 * Get the width and height in pixels for the specified image.
 * The values of @width and @height are returned as -1 if the
 * values cannot be obtained (for instance, if the object is not onscreen).
 **/
void
atk_image_get_image_size (AtkImage *image, 
                          int      *width,
                          int      *height)
{
  AtkImageIface *iface;
  gint local_width, local_height;
  gint *real_width, *real_height;

  g_return_if_fail (ATK_IS_IMAGE (image));

  if (width)
    real_width = width;
  else
    real_width = &local_width;
  if (height)
    real_height = height;
  else
    real_height = &local_height;
  
  iface = ATK_IMAGE_GET_IFACE (image);

  if (iface->get_image_size)
  {
    iface->get_image_size (image, real_width, real_height);
  }
  else
  {
    *real_width = -1;
    *real_height = -1;
  }
}

/**
 * atk_image_set_image_description:
 * @image: a #GObject instance that implements AtkImageIface
 * @description: a string description to set for @image
 *
 * Sets the textual description for this image.
 *
 * Returns: boolean TRUE, or FALSE if operation could
 * not be completed.
 **/
gboolean
atk_image_set_image_description (AtkImage        *image,
                                 const gchar     *description)
{
  AtkImageIface *iface;

  g_return_val_if_fail (ATK_IS_IMAGE (image), FALSE);

  iface = ATK_IMAGE_GET_IFACE (image);

  if (iface->set_image_description)
    {
      return (iface->set_image_description) (image, description);
    }
  else
    {
      return FALSE;
    }
}

/**
 * atk_image_get_image_position:
 * @image: a #GObject instance that implements AtkImageIface
 * @x: address of #gint to put x coordinate position; otherwise, -1 if value cannot be obtained.
 * @y: address of #gint to put y coordinate position; otherwise, -1 if value cannot be obtained.
 * @coord_type: specifies whether the coordinates are relative to the screen
 * or to the components top level window
 * 
 * Gets the position of the image in the form of a point specifying the
 * images top-left corner.
 **/
void     
atk_image_get_image_position (AtkImage *image,
                        gint *x,
		        gint *y,
    		        AtkCoordType coord_type)
{
  AtkImageIface *iface;
  gint local_x, local_y;
  gint *real_x, *real_y;

  g_return_if_fail (ATK_IS_IMAGE (image));

  if (x)
    real_x = x;
  else
    real_x = &local_x;
  if (y)
    real_y = y;
  else
    real_y = &local_y;

  iface = ATK_IMAGE_GET_IFACE (image);

  if (iface->get_image_position)
  {
    (iface->get_image_position) (image, real_x, real_y, coord_type);
  }
  else
  {
    *real_x = -1;
    *real_y = -1;
  }
}

/**
 * atk_image_get_image_locale: 
 * @image: An #AtkImage
 *
 * Since ATK 1.12
 *
 * Returns: (nullable): a string corresponding to the POSIX
 * LC_MESSAGES locale used by the image description, or %NULL if the
 * image does not specify a locale.
 *
 */
const gchar*
atk_image_get_image_locale (AtkImage   *image)
{
	
  AtkImageIface *iface;

  g_return_val_if_fail (ATK_IS_IMAGE (image), NULL);

  iface = ATK_IMAGE_GET_IFACE (image);

  if (iface->get_image_locale)
    {
      return (iface->get_image_locale) (image);
    }
  else
    {
      return NULL;
    }
}
