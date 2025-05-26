/* gtkradiotoolbutton.h
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@gnome.org>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
 * Copyright (C) 2003 Soeren Sandmann <sandmann@daimi.au.dk>
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

#ifndef __GTK_RADIO_TOOL_BUTTON_H__
#define __GTK_RADIO_TOOL_BUTTON_H__

#if defined(GTK_DISABLE_SINGLE_INCLUDES) && !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <ytk/ytk.h> can be included directly."
#endif

#include <ytk/gtktoggletoolbutton.h>

G_BEGIN_DECLS

#define GTK_TYPE_RADIO_TOOL_BUTTON            (gtk_radio_tool_button_get_type ())
#define GTK_RADIO_TOOL_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_RADIO_TOOL_BUTTON, GtkRadioToolButton))
#define GTK_RADIO_TOOL_BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_RADIO_TOOL_BUTTON, GtkRadioToolButtonClass))
#define GTK_IS_RADIO_TOOL_BUTTON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_RADIO_TOOL_BUTTON))
#define GTK_IS_RADIO_TOOL_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_RADIO_TOOL_BUTTON))
#define GTK_RADIO_TOOL_BUTTON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_RADIO_TOOL_BUTTON, GtkRadioToolButtonClass))

typedef struct _GtkRadioToolButton      GtkRadioToolButton;
typedef struct _GtkRadioToolButtonClass GtkRadioToolButtonClass;

struct _GtkRadioToolButton
{
  GtkToggleToolButton parent;
};

struct _GtkRadioToolButtonClass
{
  GtkToggleToolButtonClass parent_class;

  /* Padding for future expansion */
  void (* _gtk_reserved1) (void);
  void (* _gtk_reserved2) (void);
  void (* _gtk_reserved3) (void);
  void (* _gtk_reserved4) (void);
};

GType        gtk_radio_tool_button_get_type       (void) G_GNUC_CONST;

GtkToolItem *gtk_radio_tool_button_new                        (GSList             *group);
GtkToolItem *gtk_radio_tool_button_new_from_stock             (GSList             *group,
							       const gchar        *stock_id);
GtkToolItem *gtk_radio_tool_button_new_from_widget            (GtkRadioToolButton *group);
GtkToolItem *gtk_radio_tool_button_new_with_stock_from_widget (GtkRadioToolButton *group,
							       const gchar        *stock_id);
GSList *     gtk_radio_tool_button_get_group                  (GtkRadioToolButton *button);
void         gtk_radio_tool_button_set_group                  (GtkRadioToolButton *button,
							       GSList             *group);

G_END_DECLS

#endif /* __GTK_RADIO_TOOL_BUTTON_H__ */
