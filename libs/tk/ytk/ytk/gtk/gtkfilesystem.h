/* GTK - The GIMP Toolkit
 * gtkfilesystem.h: Filesystem abstraction functions.
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

#ifndef __GTK_FILE_SYSTEM_H__
#define __GTK_FILE_SYSTEM_H__

#include <gio/gio.h>
#include <gtk/gtkwidget.h>	/* For icon handling */

G_BEGIN_DECLS

#define GTK_TYPE_FILE_SYSTEM         (_gtk_file_system_get_type ())
#define GTK_FILE_SYSTEM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_FILE_SYSTEM, GtkFileSystem))
#define GTK_FILE_SYSTEM_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST    ((c), GTK_TYPE_FILE_SYSTEM, GtkFileSystemClass))
#define GTK_IS_FILE_SYSTEM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_FILE_SYSTEM))
#define GTK_IS_FILE_SYSTEM_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE    ((c), GTK_TYPE_FILE_SYSTEM))
#define GTK_FILE_SYSTEM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS  ((o), GTK_TYPE_FILE_SYSTEM, GtkFileSystemClass))

#define GTK_TYPE_FOLDER         (_gtk_folder_get_type ())
#define GTK_FOLDER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_FOLDER, GtkFolder))
#define GTK_FOLDER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST    ((c), GTK_TYPE_FOLDER, GtkFolderClass))
#define GTK_IS_FOLDER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_FOLDER))
#define GTK_IS_FOLDER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE    ((c), GTK_TYPE_FOLDER))
#define GTK_FOLDER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS  ((o), GTK_TYPE_FOLDER, GtkFolderClass))

typedef struct GtkFileSystemClass GtkFileSystemClass;
typedef struct GtkFileSystem GtkFileSystem;
typedef struct GtkFolderClass GtkFolderClass;
typedef struct GtkFolder GtkFolder;
typedef struct GtkFileSystemVolume GtkFileSystemVolume; /* opaque struct */
typedef struct GtkFileSystemBookmark GtkFileSystemBookmark; /* opaque struct */

struct GtkFileSystemClass
{
  GObjectClass parent_class;

  void (*bookmarks_changed) (GtkFileSystem *file_system);
  void (*volumes_changed)   (GtkFileSystem *file_system);
};

struct GtkFileSystem
{
  GObject parent_object;
};

struct GtkFolderClass
{
  GObjectClass parent_class;

  void (*files_added)      (GtkFolder *folder,
			    GList     *paths);
  void (*files_removed)    (GtkFolder *folder,
			    GList     *paths);
  void (*files_changed)    (GtkFolder *folder,
			    GList     *paths);
  void (*finished_loading) (GtkFolder *folder);
  void (*deleted)          (GtkFolder *folder);
};

struct GtkFolder
{
  GObject parent_object;
};

typedef void (* GtkFileSystemGetFolderCallback)    (GCancellable        *cancellable,
						    GtkFolder           *folder,
						    const GError        *error,
						    gpointer             data);
typedef void (* GtkFileSystemGetInfoCallback)      (GCancellable        *cancellable,
						    GFileInfo           *file_info,
						    const GError        *error,
						    gpointer             data);
typedef void (* GtkFileSystemVolumeMountCallback)  (GCancellable        *cancellable,
						    GtkFileSystemVolume *volume,
						    const GError        *error,
						    gpointer             data);

/* GtkFileSystem methods */
GType           _gtk_file_system_get_type     (void) G_GNUC_CONST;

GtkFileSystem * _gtk_file_system_new          (void);

GSList *        _gtk_file_system_list_volumes   (GtkFileSystem *file_system);
GSList *        _gtk_file_system_list_bookmarks (GtkFileSystem *file_system);

GCancellable *  _gtk_file_system_get_info               (GtkFileSystem                     *file_system,
							 GFile                             *file,
							 const gchar                       *attributes,
							 GtkFileSystemGetInfoCallback       callback,
							 gpointer                           data);
GCancellable *  _gtk_file_system_mount_volume           (GtkFileSystem                     *file_system,
							 GtkFileSystemVolume               *volume,
							 GMountOperation                   *mount_operation,
							 GtkFileSystemVolumeMountCallback   callback,
							 gpointer                           data);
GCancellable *  _gtk_file_system_mount_enclosing_volume (GtkFileSystem                     *file_system,
							 GFile                             *file,
							 GMountOperation                   *mount_operation,
							 GtkFileSystemVolumeMountCallback   callback,
							 gpointer                           data);

gboolean        _gtk_file_system_insert_bookmark    (GtkFileSystem      *file_system,
						     GFile              *file,
						     gint                position,
						     GError            **error);
gboolean        _gtk_file_system_remove_bookmark    (GtkFileSystem      *file_system,
						     GFile              *file,
						     GError            **error);

gchar *         _gtk_file_system_get_bookmark_label (GtkFileSystem *file_system,
						     GFile         *file);
void            _gtk_file_system_set_bookmark_label (GtkFileSystem *file_system,
						     GFile         *file,
						     const gchar   *label);

GtkFileSystemVolume * _gtk_file_system_get_volume_for_file (GtkFileSystem       *file_system,
							    GFile               *file);

/* GtkFolder functions */
GSList *     _gtk_folder_list_children (GtkFolder  *folder);
GFileInfo *  _gtk_folder_get_info      (GtkFolder  *folder,
				        GFile      *file);

gboolean     _gtk_folder_is_finished_loading (GtkFolder *folder);


/* GtkFileSystemVolume methods */
gchar *               _gtk_file_system_volume_get_display_name (GtkFileSystemVolume *volume);
gboolean              _gtk_file_system_volume_is_mounted       (GtkFileSystemVolume *volume);
GFile *               _gtk_file_system_volume_get_root         (GtkFileSystemVolume *volume);
GdkPixbuf *           _gtk_file_system_volume_render_icon      (GtkFileSystemVolume  *volume,
							        GtkWidget            *widget,
							        gint                  icon_size,
							        GError              **error);

GtkFileSystemVolume  *_gtk_file_system_volume_ref              (GtkFileSystemVolume *volume);
void                  _gtk_file_system_volume_unref            (GtkFileSystemVolume *volume);

/* GtkFileSystemBookmark methods */
void                   _gtk_file_system_bookmark_free          (GtkFileSystemBookmark *bookmark);

/* GFileInfo helper functions */
GdkPixbuf *     _gtk_file_info_render_icon (GFileInfo *info,
					    GtkWidget *widget,
					    gint       icon_size);

gboolean	_gtk_file_info_consider_as_directory (GFileInfo *info);

/* GFile helper functions */
gboolean	_gtk_file_has_native_path (GFile *file);

G_END_DECLS

#endif /* __GTK_FILE_SYSTEM_H__ */
