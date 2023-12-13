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

#include "atkstreamablecontent.h"

/**
 * SECTION:atkstreamablecontent
 * @Short_description: The ATK interface which provides access to
 *  streamable content.
 * @Title:AtkStreamableContent
 *
 * An interface whereby an object allows its backing content to be
 * streamed to clients.  Typical implementors would be images or
 * icons, HTML content, or multimedia display/rendering widgets.
 *
 * Negotiation of content type is allowed. Clients may examine the
 * backing data and transform, convert, or parse the content in order
 * to present it in an alternate form to end-users.
 *
 * The AtkStreamableContent interface is particularly useful for
 * saving, printing, or post-processing entire documents, or for
 * persisting alternate views of a document. If document content
 * itself is being serialized, stored, or converted, then use of the
 * AtkStreamableContent interface can help address performance
 * issues. Unlike most ATK interfaces, this interface is not strongly
 * tied to the current user-agent view of the a particular document,
 * but may in some cases give access to the underlying model data.
 */

GType
atk_streamable_content_get_type (void)
{
  static GType type = 0;

  if (!type) {
    GTypeInfo tinfo =
    {
      sizeof (AtkStreamableContentIface),
      (GBaseInitFunc) NULL,
      (GBaseFinalizeFunc) NULL,

    };

    type = g_type_register_static (G_TYPE_INTERFACE, "AtkStreamableContent", &tinfo, 0);
  }

  return type;
}

/**
 * atk_streamable_content_get_n_mime_types:
 * @streamable: a GObject instance that implements AtkStreamableContentIface
 *
 * Gets the number of mime types supported by this object.
 *
 * Returns: a gint which is the number of mime types supported by the object.
 **/
gint
atk_streamable_content_get_n_mime_types (AtkStreamableContent *streamable)
{
  AtkStreamableContentIface *iface;

  g_return_val_if_fail (ATK_IS_STREAMABLE_CONTENT (streamable), 0);

  iface = ATK_STREAMABLE_CONTENT_GET_IFACE (streamable);

  if (iface->get_n_mime_types)
    return (iface->get_n_mime_types) (streamable);
  else
    return 0;
}

/**
 * atk_streamable_content_get_mime_type:
 * @streamable: a GObject instance that implements AtkStreamableContent
 * @i: a gint representing the position of the mime type starting from 0
 *
 * Gets the character string of the specified mime type. The first mime
 * type is at position 0, the second at position 1, and so on.
 *
 * Returns: a gchar* representing the specified mime type; the caller
 * should not free the character string.
 **/
const gchar*
atk_streamable_content_get_mime_type (AtkStreamableContent *streamable,
                                      gint                 i)
{
  AtkStreamableContentIface *iface;

  g_return_val_if_fail (i >= 0, NULL);
  g_return_val_if_fail (ATK_IS_STREAMABLE_CONTENT (streamable), NULL);

  iface = ATK_STREAMABLE_CONTENT_GET_IFACE (streamable);

  if (iface->get_mime_type)
    return (iface->get_mime_type) (streamable, i);
  else
    return NULL;
}

/**
 * atk_streamable_content_get_stream:
 * @streamable: a GObject instance that implements AtkStreamableContentIface
 * @mime_type: a gchar* representing the mime type
 *
 * Gets the content in the specified mime type.
 *
 * Returns: (transfer full): A #GIOChannel which contains the content in the
 * specified mime type.
 **/
GIOChannel*
atk_streamable_content_get_stream (AtkStreamableContent *streamable,
                                   const gchar          *mime_type)
{
  AtkStreamableContentIface *iface;

  g_return_val_if_fail (mime_type != NULL, NULL);
  g_return_val_if_fail (ATK_IS_STREAMABLE_CONTENT (streamable), NULL);

  iface = ATK_STREAMABLE_CONTENT_GET_IFACE (streamable);

  if (iface->get_stream)
    return (iface->get_stream) (streamable, mime_type);
  else
    return NULL;
}

/**
 * atk_streamable_content_get_uri:
 * @streamable: a GObject instance that implements AtkStreamableContentIface
 * @mime_type: a gchar* representing the mime type, or NULL to request a URI 
 * for the default mime type.
 *
 * Get a string representing a URI in IETF standard format
 * (see http://www.ietf.org/rfc/rfc2396.txt) from which the object's content
 * may be streamed in the specified mime-type, if one is available.
 * If mime_type is NULL, the URI for the default (and possibly only) mime-type is
 * returned. 
 *
 * Note that it is possible for get_uri to return NULL but for
 * get_stream to work nonetheless, since not all GIOChannels connect to URIs.
 *
 * Returns: (nullable): Returns a string representing a URI, or %NULL
 * if no corresponding URI can be constructed.
 *
 * Since: 1.12
 **/
const gchar*
atk_streamable_content_get_uri (AtkStreamableContent *streamable,
				const gchar          *mime_type)
{
  AtkStreamableContentIface *iface;

  g_return_val_if_fail (mime_type != NULL, NULL);
  g_return_val_if_fail (ATK_IS_STREAMABLE_CONTENT (streamable), NULL);

  iface = ATK_STREAMABLE_CONTENT_GET_IFACE (streamable);

  if (iface->get_uri)
    return (iface->get_uri) (streamable, mime_type);
  else
    return NULL;
}
