/* ATK -  Accessibility Toolkit
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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
#include "atkhyperlink.h"
#include <glib/gi18n-lib.h>

/**
 * SECTION:atkhyperlink
 * @Short_description: An ATK object which encapsulates a link or set
 *  of links in a hypertext document.
 * @Title:AtkHyperlink
 *
 * An ATK object which encapsulates a link or set of links (for
 * instance in the case of client-side image maps) in a hypertext
 * document.  It may implement the AtkAction interface.  AtkHyperlink
 * may also be used to refer to inline embedded content, since it
 * allows specification of a start and end offset within the host
 * AtkHypertext object.
 */

enum
{
  LINK_ACTIVATED,

  LAST_SIGNAL
};

enum
{
  PROP_0,  /* gobject convention */

  PROP_SELECTED_LINK,
  PROP_NUMBER_ANCHORS,
  PROP_END_INDEX,
  PROP_START_INDEX,
  PROP_LAST
};

static void atk_hyperlink_class_init (AtkHyperlinkClass *klass);
static void atk_hyperlink_init       (AtkHyperlink      *link,
                                      AtkHyperlinkClass *klass);

static void atk_hyperlink_real_get_property (GObject            *object,
                                             guint              prop_id,
                                             GValue             *value,
                                             GParamSpec         *pspec);

static void atk_hyperlink_action_iface_init (AtkActionIface *iface);

static guint atk_hyperlink_signals[LAST_SIGNAL] = { 0, };

static gpointer  parent_class = NULL;

GType
atk_hyperlink_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo typeInfo =
      {
        sizeof (AtkHyperlinkClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) atk_hyperlink_class_init,
        (GClassFinalizeFunc) NULL,
        NULL,
        sizeof (AtkHyperlink),
        0,
        (GInstanceInitFunc) atk_hyperlink_init,
      } ;

      static const GInterfaceInfo action_info =
      {
        (GInterfaceInitFunc) atk_hyperlink_action_iface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL
      };

      type = g_type_register_static (G_TYPE_OBJECT, "AtkHyperlink", &typeInfo, 0) ;
      g_type_add_interface_static (type, ATK_TYPE_ACTION, &action_info);
    }
  return type;
}

static void
atk_hyperlink_class_init (AtkHyperlinkClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->get_property = atk_hyperlink_real_get_property;

  klass->link_activated = NULL;

  /**
   * AtkHyperlink:selected-link:
   *
   * Selected link
   *
   * Deprecated: Since 1.8. This property is deprecated since ATK
   * version 1.8. Please use ATK_STATE_FOCUSABLE for all links, and
   * ATK_STATE_FOCUSED for focused links.
   *
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SELECTED_LINK,
                                   g_param_spec_boolean ("selected-link",
                                                         _("Selected Link"),
                                                         _("Specifies whether the AtkHyperlink object is selected"),
                                                         FALSE,
                                                         G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_NUMBER_ANCHORS,
                                   g_param_spec_int ("number-of-anchors",
                                                     _("Number of Anchors"),
                                                     _("The number of anchors associated with the AtkHyperlink object"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_END_INDEX,
                                   g_param_spec_int ("end-index",
                                                     _("End index"),
                                                     _("The end index of the AtkHyperlink object"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_START_INDEX,
                                   g_param_spec_int ("start-index",
                                                     _("Start index"),
                                                     _("The start index of the AtkHyperlink object"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE));

  /**
   * AtkHyperlink::link-activated:
   * @atkhyperlink: the object which received the signal.
   *
   * The signal link-activated is emitted when a link is activated.
   */
  atk_hyperlink_signals[LINK_ACTIVATED] =
    g_signal_new ("link_activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (AtkHyperlinkClass, link_activated),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

}

static void
atk_hyperlink_init  (AtkHyperlink        *link,
                     AtkHyperlinkClass   *klass)
{
}

static void
atk_hyperlink_real_get_property (GObject    *object,
                                 guint      prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  AtkHyperlink* link;

  link = ATK_HYPERLINK (object);

  switch (prop_id)
    {
    case PROP_SELECTED_LINK:
      // This property is deprecated, also the method to get the value
      g_value_set_boolean (value, FALSE);
      break;
    case PROP_NUMBER_ANCHORS:
      g_value_set_int (value,  atk_hyperlink_get_n_anchors (link));
      break;
    case PROP_END_INDEX:
      g_value_set_int (value, atk_hyperlink_get_end_index (link));
      break;
    case PROP_START_INDEX:
      g_value_set_int (value, atk_hyperlink_get_start_index (link));
      break;
    default:
      break;
    }
}

/**
 * atk_hyperlink_get_uri:
 * @link_: an #AtkHyperlink
 * @i: a (zero-index) integer specifying the desired anchor
 *
 * Get a the URI associated with the anchor specified 
 * by @i of @link_. 
 *
 * Multiple anchors are primarily used by client-side image maps.
 *
 * Returns: a string specifying the URI 
 **/
gchar*
atk_hyperlink_get_uri (AtkHyperlink *link,
                       gint         i)
{
  AtkHyperlinkClass *klass;

  g_return_val_if_fail (ATK_IS_HYPERLINK (link), NULL);

  klass = ATK_HYPERLINK_GET_CLASS (link);
  if (klass->get_uri)
    return (klass->get_uri) (link, i);
  else
    return NULL;
}

/**
 * atk_hyperlink_get_object:
 * @link_: an #AtkHyperlink
 * @i: a (zero-index) integer specifying the desired anchor
 *
 * Returns the item associated with this hyperlinks nth anchor.
 * For instance, the returned #AtkObject will implement #AtkText
 * if @link_ is a text hyperlink, #AtkImage if @link_ is an image
 * hyperlink etc. 
 * 
 * Multiple anchors are primarily used by client-side image maps.
 *
 * Returns: (transfer none): an #AtkObject associated with this hyperlinks
 * i-th anchor
 **/
AtkObject*
atk_hyperlink_get_object (AtkHyperlink *link,
                          gint         i)
{
  AtkHyperlinkClass *klass;

  g_return_val_if_fail (ATK_IS_HYPERLINK (link), NULL);

  klass = ATK_HYPERLINK_GET_CLASS (link);
  if (klass->get_object)
    return (klass->get_object) (link, i);
  else
    return NULL;
}

/**
 * atk_hyperlink_get_end_index:
 * @link_: an #AtkHyperlink
 *
 * Gets the index with the hypertext document at which this link ends.
 *
 * Returns: the index with the hypertext document at which this link ends
 **/
gint
atk_hyperlink_get_end_index (AtkHyperlink *link)
{
  AtkHyperlinkClass *klass;

  g_return_val_if_fail (ATK_IS_HYPERLINK (link), 0);

  klass = ATK_HYPERLINK_GET_CLASS (link);
  if (klass->get_end_index)
    return (klass->get_end_index) (link);
  else
    return 0;
}

/**
 * atk_hyperlink_get_start_index:
 * @link_: an #AtkHyperlink
 *
 * Gets the index with the hypertext document at which this link begins.
 *
 * Returns: the index with the hypertext document at which this link begins
 **/
gint
atk_hyperlink_get_start_index (AtkHyperlink *link)
{
  AtkHyperlinkClass *klass;

  g_return_val_if_fail (ATK_IS_HYPERLINK (link), 0);

  klass = ATK_HYPERLINK_GET_CLASS (link);
  if (klass->get_start_index)
    return (klass->get_start_index) (link);
  else
    return 0;
}

/**
 * atk_hyperlink_is_valid:
 * @link_: an #AtkHyperlink
 *
 * Since the document that a link is associated with may have changed
 * this method returns %TRUE if the link is still valid (with
 * respect to the document it references) and %FALSE otherwise.
 *
 * Returns: whether or not this link is still valid
 **/
gboolean
atk_hyperlink_is_valid (AtkHyperlink *link)
{
  AtkHyperlinkClass *klass;

  g_return_val_if_fail (ATK_IS_HYPERLINK (link), FALSE);

  klass = ATK_HYPERLINK_GET_CLASS (link);
  if (klass->is_valid)
    return (klass->is_valid) (link);
  else
    return FALSE;
}

/**
 * atk_hyperlink_is_inline:
 * @link_: an #AtkHyperlink
 *
 * Indicates whether the link currently displays some or all of its
 *           content inline.  Ordinary HTML links will usually return
 *           %FALSE, but an inline &lt;src&gt; HTML element will return
 *           %TRUE.
 *
 * Returns: whether or not this link displays its content inline.
 *
 **/
gboolean
atk_hyperlink_is_inline (AtkHyperlink *link)
{
  AtkHyperlinkClass *klass;

  g_return_val_if_fail (ATK_IS_HYPERLINK (link), FALSE);

  klass = ATK_HYPERLINK_GET_CLASS (link);
  if (klass->link_state)
    return (klass->link_state (link) & ATK_HYPERLINK_IS_INLINE);
  else
    return FALSE;
}

/**
 * atk_hyperlink_get_n_anchors:
 * @link_: an #AtkHyperlink
 *
 * Gets the number of anchors associated with this hyperlink.
 *
 * Returns: the number of anchors associated with this hyperlink
 **/
gint
atk_hyperlink_get_n_anchors (AtkHyperlink *link)
{
  AtkHyperlinkClass *klass;

  g_return_val_if_fail (ATK_IS_HYPERLINK (link), 0);

  klass = ATK_HYPERLINK_GET_CLASS (link);
  if (klass->get_n_anchors)
    return (klass->get_n_anchors) (link);
  else
    return 0;
}

/**
 * atk_hyperlink_is_selected_link:
 * @link_: an #AtkHyperlink
 *
 * Determines whether this AtkHyperlink is selected
 *
 * Since: 1.4
 *
 * Deprecated: This method is deprecated since ATK version 1.8.
 * Please use ATK_STATE_FOCUSABLE for all links, and ATK_STATE_FOCUSED
 * for focused links.
 *
 * Returns: True if the AtkHyperlink is selected, False otherwise
 **/
gboolean
atk_hyperlink_is_selected_link (AtkHyperlink *link)
{
  AtkHyperlinkClass *klass;

  g_return_val_if_fail (ATK_IS_HYPERLINK (link), FALSE);

  klass = ATK_HYPERLINK_GET_CLASS (link);
  if (klass->is_selected_link)
    return (klass->is_selected_link) (link);
  else
    return FALSE;
}

static void atk_hyperlink_action_iface_init (AtkActionIface *iface)
{
  /*
   * We do nothing here
   *
   * When we come to derive a class from AtkHyperlink we will provide an
   * implementation of the AtkAction interface. 
   */
}
