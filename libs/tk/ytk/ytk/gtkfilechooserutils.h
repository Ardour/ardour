/* GTK - The GIMP Toolkit
 * gtkfilechooserutils.h: Private utility functions useful for
 *                        implementing a GtkFileChooser interface
 * Copyright (C) 2003, Red Hat, Inc.
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

#ifndef __GTK_FILE_CHOOSER_UTILS_H__
#define __GTK_FILE_CHOOSER_UTILS_H__

#include "gtkfilechooserprivate.h"

G_BEGIN_DECLS

#define GTK_FILE_CHOOSER_DELEGATE_QUARK	  (_gtk_file_chooser_delegate_get_quark ())

typedef enum {
  GTK_FILE_CHOOSER_PROP_FIRST                  = 0x1000,
  GTK_FILE_CHOOSER_PROP_ACTION                 = GTK_FILE_CHOOSER_PROP_FIRST,
  GTK_FILE_CHOOSER_PROP_FILE_SYSTEM_BACKEND,
  GTK_FILE_CHOOSER_PROP_FILTER,
  GTK_FILE_CHOOSER_PROP_LOCAL_ONLY,
  GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET, 
  GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET_ACTIVE,
  GTK_FILE_CHOOSER_PROP_USE_PREVIEW_LABEL,
  GTK_FILE_CHOOSER_PROP_EXTRA_WIDGET,
  GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE,
  GTK_FILE_CHOOSER_PROP_SHOW_HIDDEN,
  GTK_FILE_CHOOSER_PROP_DO_OVERWRITE_CONFIRMATION,
  GTK_FILE_CHOOSER_PROP_CREATE_FOLDERS,
  GTK_FILE_CHOOSER_PROP_LAST                   = GTK_FILE_CHOOSER_PROP_CREATE_FOLDERS
} GtkFileChooserProp;

void _gtk_file_chooser_install_properties (GObjectClass *klass);

void _gtk_file_chooser_delegate_iface_init (GtkFileChooserIface *iface);
void _gtk_file_chooser_set_delegate        (GtkFileChooser *receiver,
					    GtkFileChooser *delegate);

GQuark _gtk_file_chooser_delegate_get_quark (void) G_GNUC_CONST;

GList *_gtk_file_chooser_extract_recent_folders (GList *infos);

G_END_DECLS

#endif /* __GTK_FILE_CHOOSER_UTILS_H__ */
