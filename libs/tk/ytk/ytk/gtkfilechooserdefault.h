/* GTK - The GIMP Toolkit
 * gtkfilechooserdefault.h: Default implementation of GtkFileChooser
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

#ifndef __GTK_FILE_CHOOSER_DEFAULT_H__
#define __GTK_FILE_CHOOSER_DEFAULT_H__

#include "gtkfilesystem.h"
#include <ytk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_FILE_CHOOSER_DEFAULT    (_gtk_file_chooser_default_get_type ())
#define GTK_FILE_CHOOSER_DEFAULT(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_FILE_CHOOSER_DEFAULT, GtkFileChooserDefault))
#define GTK_IS_FILE_CHOOSER_DEFAULT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_FILE_CHOOSER_DEFAULT))

typedef struct _GtkFileChooserDefault      GtkFileChooserDefault;

GType      _gtk_file_chooser_default_get_type (void) G_GNUC_CONST;
GtkWidget *_gtk_file_chooser_default_new      (void);

G_END_DECLS

#endif /* __GTK_FILE_CHOOSER_DEFAULT_H__ */
