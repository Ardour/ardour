/* gtkrecentchooserutils.h - Private utility functions for implementing a
 *                           GtkRecentChooser interface
 *
 * Copyright (C) 2006 Emmanuele Bassi
 *
 * All rights reserved
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
 *
 * Based on gtkfilechooserutils.h:
 *	Copyright (C) 2003 Red Hat, Inc.
 */
 
#ifndef __GTK_RECENT_CHOOSER_UTILS_H__
#define __GTK_RECENT_CHOOSER_UTILS_H__

#include "gtkrecentchooserprivate.h"

G_BEGIN_DECLS


#define GTK_RECENT_CHOOSER_DELEGATE_QUARK	(_gtk_recent_chooser_delegate_get_quark ())

typedef enum {
  GTK_RECENT_CHOOSER_PROP_FIRST           = 0x3000,
  GTK_RECENT_CHOOSER_PROP_RECENT_MANAGER,
  GTK_RECENT_CHOOSER_PROP_SHOW_PRIVATE,
  GTK_RECENT_CHOOSER_PROP_SHOW_NOT_FOUND,
  GTK_RECENT_CHOOSER_PROP_SHOW_TIPS,
  GTK_RECENT_CHOOSER_PROP_SHOW_ICONS,
  GTK_RECENT_CHOOSER_PROP_SELECT_MULTIPLE,
  GTK_RECENT_CHOOSER_PROP_LIMIT,
  GTK_RECENT_CHOOSER_PROP_LOCAL_ONLY,
  GTK_RECENT_CHOOSER_PROP_SORT_TYPE,
  GTK_RECENT_CHOOSER_PROP_FILTER,
  GTK_RECENT_CHOOSER_PROP_LAST
} GtkRecentChooserProp;

void   _gtk_recent_chooser_install_properties  (GObjectClass          *klass);

void   _gtk_recent_chooser_delegate_iface_init (GtkRecentChooserIface *iface);
void   _gtk_recent_chooser_set_delegate        (GtkRecentChooser      *receiver,
						GtkRecentChooser      *delegate);

GQuark _gtk_recent_chooser_delegate_get_quark  (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GTK_RECENT_CHOOSER_UTILS_H__ */
