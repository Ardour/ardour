/* ATK -  Accessibility Toolkit
 * Copyright 2006 Sun Microsystems Inc.
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

#include <string.h>
#include "atkhyperlinkimpl.h"

/**
 * SECTION:atkhyperlinkimpl
 * @Short_description: An interface from which the AtkHyperlink
 *  associated with an AtkObject may be obtained.
 * @Title:AtkHyperlinImpl
 *
 * AtkHyperlinkImpl allows AtkObjects to refer to their associated
 * AtkHyperlink instance, if one exists.  AtkHyperlinkImpl differs
 * from AtkHyperlink in that AtkHyperlinkImpl is an interface, whereas
 * AtkHyperlink is a object type.  The AtkHyperlinkImpl interface
 * allows a client to query an AtkObject for the availability of an
 * associated AtkHyperlink instance, and obtain that instance.  It is
 * thus particularly useful in cases where embedded content or inline
 * content within a text object is present, since the embedding text
 * object implements AtkHypertext and the inline/embedded objects are
 * exposed as children which implement AtkHyperlinkImpl, in addition
 * to their being obtainable via AtkHypertext:getLink followed by
 * AtkHyperlink:getObject.
 *
 * The AtkHyperlinkImpl interface should be supported by objects
 * exposed within the hierarchy as children of an AtkHypertext
 * container which correspond to "links" or embedded content within
 * the text.  HTML anchors are not, for instance, normally exposed
 * this way, but embedded images and components which appear inline in
 * the content of a text object are. The AtkHyperlinkIface interface
 * allows a means of determining which children are hyperlinks in this
 * sense of the word, and for obtaining their corresponding
 * AtkHyperlink object, from which the embedding range, URI, etc. can
 * be obtained.
 *
 * To some extent this interface exists because, for historical
 * reasons, AtkHyperlink was defined as an object type, not an
 * interface.  Thus, in order to interact with AtkObjects via
 * AtkHyperlink semantics, a new interface was required.
 */

GType
atk_hyperlink_impl_get_type (void)
{
  static GType type = 0;

  if (!type) {
    GTypeInfo tinfo =
    {
      sizeof (AtkHyperlinkImplIface),
      (GBaseInitFunc) NULL,
      (GBaseFinalizeFunc) NULL,

    };

    type = g_type_register_static (G_TYPE_INTERFACE, "AtkHyperlinkImpl", &tinfo, 0);
  }

  return type;
}

/**
 * atk_hyperlink_impl_get_hyperlink:
 * @impl: a #GObject instance that implements AtkHyperlinkImplIface
 *
 * Gets the hyperlink associated with this object.
 *
 * Returns: (transfer full):  an AtkHyperlink object which points to this
 * implementing AtkObject.
 *
 * Since: 1.12
 **/
AtkHyperlink *
atk_hyperlink_impl_get_hyperlink (AtkHyperlinkImpl *impl)
{
  AtkHyperlinkImplIface *iface;

  g_return_val_if_fail (impl != NULL, NULL);
  g_return_val_if_fail (ATK_IS_HYPERLINK_IMPL (impl), NULL);

  iface = ATK_HYPERLINK_IMPL_GET_IFACE (impl);

  if (iface->get_hyperlink)
    {
      return (iface->get_hyperlink) (impl);
    }
  return NULL;
}

