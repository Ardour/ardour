/* ATK - The Accessibility Toolkit for GTK+
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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

#include "atkhypertext.h"

/**
 * SECTION:atkhypertext
 * @Short_description: The ATK interface which provides standard
 *  mechanism for manipulating hyperlinks.
 * @Title:AtkHypertext
 *
 * An interface used for objects which implement linking between
 * multiple resource or content locations, or multiple 'markers'
 * within a single document.  A Hypertext instance is associated with
 * one or more Hyperlinks, which are associated with particular
 * offsets within the Hypertext's included content.  While this
 * interface is derived from Text, there is no requirement that
 * Hypertext instances have textual content; they may implement Image
 * as well, and Hyperlinks need not have non-zero text offsets.
 */

enum {
  LINK_SELECTED,
  LAST_SIGNAL
};

static void atk_hypertext_base_init (AtkHypertextIface *class);

static guint atk_hypertext_signals[LAST_SIGNAL] = { 0 };


GType
atk_hypertext_get_type (void)
{
  static GType type = 0;

  if (!type) {
    static const GTypeInfo tinfo =
    {
      sizeof (AtkHypertextIface),
      (GBaseInitFunc) atk_hypertext_base_init,
      (GBaseFinalizeFunc) NULL,

    };

    type = g_type_register_static (G_TYPE_INTERFACE, "AtkHypertext", &tinfo, 0);
  }

  return type;
}

static void
atk_hypertext_base_init (AtkHypertextIface *class)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      /**
       * AtkHypertext::link-selected:
       * @atkhypertext: the object which received the signal.
       * @arg1: the index of the hyperlink which is selected
       *
       * The "link-selected" signal is emitted by an AtkHyperText
       * object when one of the hyperlinks associated with the object
       * is selected.
       */
      atk_hypertext_signals[LINK_SELECTED] =
        g_signal_new ("link_selected",
                      ATK_TYPE_HYPERTEXT,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (AtkHypertextIface, link_selected),
                      (GSignalAccumulator) NULL, NULL,
                      g_cclosure_marshal_VOID__INT,
                      G_TYPE_NONE,
                      1, G_TYPE_INT);

      initialized = TRUE;
    }
}

/**
 * atk_hypertext_get_link:
 * @hypertext: an #AtkHypertext
 * @link_index: an integer specifying the desired link
 *
 * Gets the link in this hypertext document at index 
 * @link_index
 *
 * Returns: (transfer none): the link in this hypertext document at
 * index @link_index
 **/
AtkHyperlink* 
atk_hypertext_get_link (AtkHypertext  *hypertext,
                        gint          link_index)
{
  AtkHypertextIface *iface;

  g_return_val_if_fail (ATK_IS_HYPERTEXT (hypertext), NULL);

  if (link_index < 0)
    return NULL;

  iface = ATK_HYPERTEXT_GET_IFACE (hypertext);

  if (iface->get_link)
    return (*(iface->get_link)) (hypertext, link_index);
  else
    return NULL;
}

/**
 * atk_hypertext_get_n_links:
 * @hypertext: an #AtkHypertext
 *
 * Gets the number of links within this hypertext document.
 *
 * Returns: the number of links within this hypertext document
 **/
gint 
atk_hypertext_get_n_links (AtkHypertext  *hypertext)
{
  AtkHypertextIface *iface;

  g_return_val_if_fail (ATK_IS_HYPERTEXT (hypertext), 0);

  iface = ATK_HYPERTEXT_GET_IFACE (hypertext);

  if (iface->get_n_links)
    return (*(iface->get_n_links)) (hypertext);
  else
    return 0;
}

/**
 * atk_hypertext_get_link_index:
 * @hypertext: an #AtkHypertext
 * @char_index: a character index
 *
 * Gets the index into the array of hyperlinks that is associated with
 * the character specified by @char_index.
 *
 * Returns: an index into the array of hyperlinks in @hypertext,
 * or -1 if there is no hyperlink associated with this character.
 **/
gint 
atk_hypertext_get_link_index (AtkHypertext  *hypertext,
                              gint          char_index)
{
  AtkHypertextIface *iface;

  g_return_val_if_fail (ATK_IS_HYPERTEXT (hypertext), -1);

  if (char_index < 0)
    return -1;

  iface = ATK_HYPERTEXT_GET_IFACE (hypertext);

  if (iface->get_link_index)
    return (*(iface->get_link_index)) (hypertext, char_index);
  else
    return -1;
}
