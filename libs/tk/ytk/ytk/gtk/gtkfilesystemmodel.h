/* GTK - The GIMP Toolkit
 * gtkfilesystemmodel.h: GtkTreeModel wrapping a GtkFileSystem
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

#ifndef __GTK_FILE_SYSTEM_MODEL_H__
#define __GTK_FILE_SYSTEM_MODEL_H__

#include <gio/gio.h>
#include <gtk/gtkfilefilter.h>
#include <gtk/gtktreemodel.h>

G_BEGIN_DECLS

#define GTK_TYPE_FILE_SYSTEM_MODEL             (_gtk_file_system_model_get_type ())
#define GTK_FILE_SYSTEM_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_FILE_SYSTEM_MODEL, GtkFileSystemModel))
#define GTK_IS_FILE_SYSTEM_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_FILE_SYSTEM_MODEL))

typedef struct _GtkFileSystemModel      GtkFileSystemModel;

GType _gtk_file_system_model_get_type (void) G_GNUC_CONST;

typedef gboolean (*GtkFileSystemModelGetValue)   (GtkFileSystemModel *model,
                                                  GFile              *file,
                                                  GFileInfo          *info,
                                                  int                 column,
                                                  GValue             *value,
                                                  gpointer            user_data);

GtkFileSystemModel *_gtk_file_system_model_new              (GtkFileSystemModelGetValue get_func,
                                                             gpointer            get_data,
                                                             guint               n_columns,
                                                             ...);
GtkFileSystemModel *_gtk_file_system_model_new_for_directory(GFile *             dir,
                                                             const gchar *       attributes,
                                                             GtkFileSystemModelGetValue get_func,
                                                             gpointer            get_data,
                                                             guint               n_columns,
                                                             ...);
GCancellable *      _gtk_file_system_model_get_cancellable  (GtkFileSystemModel *model);
gboolean            _gtk_file_system_model_iter_is_visible  (GtkFileSystemModel *model,
							     GtkTreeIter        *iter);
gboolean            _gtk_file_system_model_iter_is_filtered_out (GtkFileSystemModel *model,
								 GtkTreeIter        *iter);
GFileInfo *         _gtk_file_system_model_get_info         (GtkFileSystemModel *model,
							     GtkTreeIter        *iter);
gboolean            _gtk_file_system_model_get_iter_for_file(GtkFileSystemModel *model,
							     GtkTreeIter        *iter,
							     GFile              *file);
GFile *             _gtk_file_system_model_get_file         (GtkFileSystemModel *model,
							     GtkTreeIter        *iter);
const GValue *      _gtk_file_system_model_get_value        (GtkFileSystemModel *model,
                                                             GtkTreeIter *       iter,
                                                             int                 column);

void                _gtk_file_system_model_add_and_query_file (GtkFileSystemModel *model,
                                                             GFile              *file,
                                                             const char         *attributes);
void                _gtk_file_system_model_update_file      (GtkFileSystemModel *model,
                                                             GFile              *file,
                                                             GFileInfo          *info);

void                _gtk_file_system_model_set_show_hidden  (GtkFileSystemModel *model,
							     gboolean            show_hidden);
void                _gtk_file_system_model_set_show_folders (GtkFileSystemModel *model,
							     gboolean            show_folders);
void                _gtk_file_system_model_set_show_files   (GtkFileSystemModel *model,
							     gboolean            show_files);
void                _gtk_file_system_model_set_filter_folders (GtkFileSystemModel *model,
							     gboolean            show_folders);
void                _gtk_file_system_model_clear_cache      (GtkFileSystemModel *model,
                                                             int                 column);

void                _gtk_file_system_model_set_filter       (GtkFileSystemModel *model,
                                                             GtkFileFilter      *filter);

void _gtk_file_system_model_add_editable    (GtkFileSystemModel *model,
					     GtkTreeIter        *iter);
void _gtk_file_system_model_remove_editable (GtkFileSystemModel *model);

G_END_DECLS

#endif /* __GTK_FILE_SYSTEM_MODEL_H__ */
