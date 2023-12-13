/* GTK - The GIMP Toolkit
 * gtkfilesystem.c: Filesystem abstraction functions.
 * Copyright (C) 2003, Red Hat, Inc.
 * Copyright (C) 2007-2008 Carlos Garnacho
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Carlos Garnacho <carlos@imendio.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n-lib.h>

#include "gtkfilechooser.h"
#include "gtkfilesystem.h"
#include "gtkicontheme.h"
#include "gtkprivate.h"

#include "gtkalias.h"

/* #define DEBUG_MODE */
#ifdef DEBUG_MODE
#define DEBUG(x) g_debug (x);
#else
#define DEBUG(x)
#endif

#define GTK_FILE_SYSTEM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_FILE_SYSTEM, GtkFileSystemPrivate))
#define GTK_FOLDER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_FOLDER, GtkFolderPrivate))
#define FILES_PER_QUERY 100

/* The pointers we return for a GtkFileSystemVolume are opaque tokens; they are
 * really pointers to GDrive, GVolume or GMount objects.  We need an extra
 * token for the fake "File System" volume.  So, we'll return a pointer to
 * this particular string.
 */
static const gchar *root_volume_token = N_("File System");
#define IS_ROOT_VOLUME(volume) ((gpointer) (volume) == (gpointer) root_volume_token)

enum {
  PROP_0,
  PROP_FILE,
  PROP_ENUMERATOR,
  PROP_ATTRIBUTES
};

enum {
  BOOKMARKS_CHANGED,
  VOLUMES_CHANGED,
  FS_LAST_SIGNAL
};

enum {
  FILES_ADDED,
  FILES_REMOVED,
  FILES_CHANGED,
  FINISHED_LOADING,
  DELETED,
  FOLDER_LAST_SIGNAL
};

static guint fs_signals [FS_LAST_SIGNAL] = { 0, };
static guint folder_signals [FOLDER_LAST_SIGNAL] = { 0, };

typedef struct GtkFileSystemPrivate GtkFileSystemPrivate;
typedef struct GtkFolderPrivate GtkFolderPrivate;
typedef struct AsyncFuncData AsyncFuncData;

struct GtkFileSystemPrivate
{
  GVolumeMonitor *volume_monitor;

  /* This list contains elements that can be
   * of type GDrive, GVolume and GMount
   */
  GSList *volumes;

  /* This list contains GtkFileSystemBookmark structs */
  GSList *bookmarks;
  GFile *bookmarks_file;

  GFileMonitor *bookmarks_monitor;
};

struct GtkFolderPrivate
{
  GFile *folder_file;
  GHashTable *children;
  GFileMonitor *directory_monitor;
  GFileEnumerator *enumerator;
  GCancellable *cancellable;
  gchar *attributes;

  guint finished_loading : 1;
};

struct AsyncFuncData
{
  GtkFileSystem *file_system;
  GFile *file;
  GCancellable *cancellable;
  gchar *attributes;

  gpointer callback;
  gpointer data;
};

struct GtkFileSystemBookmark
{
  GFile *file;
  gchar *label;
};

G_DEFINE_TYPE (GtkFileSystem, _gtk_file_system, G_TYPE_OBJECT)

G_DEFINE_TYPE (GtkFolder, _gtk_folder, G_TYPE_OBJECT)


static void gtk_folder_set_finished_loading (GtkFolder *folder,
					     gboolean   finished_loading);
static void gtk_folder_add_file             (GtkFolder *folder,
					     GFile     *file,
					     GFileInfo *info);


/* GtkFileSystemBookmark methods */
void
_gtk_file_system_bookmark_free (GtkFileSystemBookmark *bookmark)
{
  g_object_unref (bookmark->file);
  g_free (bookmark->label);
  g_slice_free (GtkFileSystemBookmark, bookmark);
}

/* GtkFileSystem methods */
static void
volumes_changed (GVolumeMonitor *volume_monitor,
		 gpointer        volume,
		 gpointer        user_data)
{
  GtkFileSystem *file_system;

  gdk_threads_enter ();

  file_system = GTK_FILE_SYSTEM (user_data);
  g_signal_emit (file_system, fs_signals[VOLUMES_CHANGED], 0, volume);
  gdk_threads_leave ();
}

static void
gtk_file_system_dispose (GObject *object)
{
  GtkFileSystemPrivate *priv;

  DEBUG ("dispose");

  priv = GTK_FILE_SYSTEM_GET_PRIVATE (object);

  if (priv->volumes)
    {
      g_slist_foreach (priv->volumes, (GFunc) g_object_unref, NULL);
      g_slist_free (priv->volumes);
      priv->volumes = NULL;
    }

  if (priv->volume_monitor)
    {
      g_signal_handlers_disconnect_by_func (priv->volume_monitor, volumes_changed, object);
      g_object_unref (priv->volume_monitor);
      priv->volume_monitor = NULL;
    }

  G_OBJECT_CLASS (_gtk_file_system_parent_class)->dispose (object);
}

static void
gtk_file_system_finalize (GObject *object)
{
  GtkFileSystemPrivate *priv;

  DEBUG ("finalize");

  priv = GTK_FILE_SYSTEM_GET_PRIVATE (object);

  if (priv->bookmarks_monitor)
    g_object_unref (priv->bookmarks_monitor);

  if (priv->bookmarks)
    {
      g_slist_foreach (priv->bookmarks, (GFunc) _gtk_file_system_bookmark_free, NULL);
      g_slist_free (priv->bookmarks);
    }

  if (priv->bookmarks_file)
    g_object_unref (priv->bookmarks_file);

  G_OBJECT_CLASS (_gtk_file_system_parent_class)->finalize (object);
}

static void
_gtk_file_system_class_init (GtkFileSystemClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = gtk_file_system_dispose;
  object_class->finalize = gtk_file_system_finalize;

  fs_signals[BOOKMARKS_CHANGED] =
    g_signal_new ("bookmarks-changed",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkFileSystemClass, bookmarks_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  fs_signals[VOLUMES_CHANGED] =
    g_signal_new ("volumes-changed",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkFileSystemClass, volumes_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  g_type_class_add_private (object_class, sizeof (GtkFileSystemPrivate));
}

static GFile *
get_legacy_bookmarks_file (void)
{
  GFile *file;
  gchar *filename;

  filename = g_build_filename (g_get_home_dir (), ".gtk-bookmarks", NULL);
  file = g_file_new_for_path (filename);
  g_free (filename);

  return file;
}

static GFile *
get_bookmarks_file (void)
{
  GFile *file;
  gchar *filename;

  filename = g_build_filename (g_get_user_config_dir (), "gtk-3.0", "bookmarks", NULL);
  file = g_file_new_for_path (filename);
  g_free (filename);

  return file;
}

static GSList *
read_bookmarks (GFile *file)
{
  gchar *contents;
  gchar **lines, *space;
  GSList *bookmarks = NULL;
  gint i;

  if (!g_file_load_contents (file, NULL, &contents,
			     NULL, NULL, NULL))
    return NULL;

  lines = g_strsplit (contents, "\n", -1);

  for (i = 0; lines[i]; i++)
    {
      GtkFileSystemBookmark *bookmark;

      if (!*lines[i])
	continue;

      if (!g_utf8_validate (lines[i], -1, NULL))
	continue;

      bookmark = g_slice_new0 (GtkFileSystemBookmark);

      if ((space = strchr (lines[i], ' ')) != NULL)
	{
	  space[0] = '\0';
	  bookmark->label = g_strdup (space + 1);
	}

      bookmark->file = g_file_new_for_uri (lines[i]);
      bookmarks = g_slist_prepend (bookmarks, bookmark);
    }

  bookmarks = g_slist_reverse (bookmarks);
  g_strfreev (lines);
  g_free (contents);

  return bookmarks;
}

static void
save_bookmarks (GFile  *bookmarks_file,
		GSList *bookmarks)
{
  GError *error = NULL;
  GString *contents;
  GSList *l;
  GFile *parent_file;
  gchar *path;

  contents = g_string_new ("");

  for (l = bookmarks; l; l = l->next)
    {
      GtkFileSystemBookmark *bookmark = l->data;
      gchar *uri;

      uri = g_file_get_uri (bookmark->file);
      if (!uri)
	continue;

      g_string_append (contents, uri);

      if (bookmark->label)
	g_string_append_printf (contents, " %s", bookmark->label);

      g_string_append_c (contents, '\n');
      g_free (uri);
    }

  parent_file = g_file_get_parent (bookmarks_file);
  path = g_file_get_path (parent_file);
  if (g_mkdir_with_parents (path, 0700) == 0)
    {
      if (!g_file_replace_contents (bookmarks_file,
                                    contents->str,
                                    strlen (contents->str),
                                    NULL, FALSE, 0, NULL,
                                    NULL, &error))
        {
          g_critical ("%s", error->message);
          g_error_free (error);
        }
    }
  g_free (path);
  g_object_unref (parent_file);
  g_string_free (contents, TRUE);
}

static void
bookmarks_file_changed (GFileMonitor      *monitor,
			GFile             *file,
			GFile             *other_file,
			GFileMonitorEvent  event,
			gpointer           data)
{
  GtkFileSystemPrivate *priv;

  priv = GTK_FILE_SYSTEM_GET_PRIVATE (data);

  switch (event)
    {
    case G_FILE_MONITOR_EVENT_CHANGED:
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    case G_FILE_MONITOR_EVENT_CREATED:
    case G_FILE_MONITOR_EVENT_DELETED:
      g_slist_foreach (priv->bookmarks, (GFunc) _gtk_file_system_bookmark_free, NULL);
      g_slist_free (priv->bookmarks);

      priv->bookmarks = read_bookmarks (file);

      gdk_threads_enter ();
      g_signal_emit (data, fs_signals[BOOKMARKS_CHANGED], 0);
      gdk_threads_leave ();
      break;
    default:
      /* ignore at the moment */
      break;
    }
}

static gboolean
mount_referenced_by_volume_activation_root (GList *volumes, GMount *mount)
{
  GList *l;
  GFile *mount_root;
  gboolean ret;

  ret = FALSE;

  mount_root = g_mount_get_root (mount);

  for (l = volumes; l != NULL; l = l->next)
    {
      GVolume *volume = G_VOLUME (l->data);
      GFile *volume_activation_root;

      volume_activation_root = g_volume_get_activation_root (volume);
      if (volume_activation_root != NULL)
        {
          if (g_file_has_prefix (volume_activation_root, mount_root))
            {
              ret = TRUE;
              g_object_unref (volume_activation_root);
              break;
            }
          g_object_unref (volume_activation_root);
        }
    }

  g_object_unref (mount_root);
  return ret;
}

static void
get_volumes_list (GtkFileSystem *file_system)
{
  GtkFileSystemPrivate *priv;
  GList *l, *ll;
  GList *drives;
  GList *volumes;
  GList *mounts;
  GDrive *drive;
  GVolume *volume;
  GMount *mount;

  priv = GTK_FILE_SYSTEM_GET_PRIVATE (file_system);

  if (priv->volumes)
    {
      g_slist_foreach (priv->volumes, (GFunc) g_object_unref, NULL);
      g_slist_free (priv->volumes);
      priv->volumes = NULL;
    }

  /* first go through all connected drives */
  drives = g_volume_monitor_get_connected_drives (priv->volume_monitor);

  for (l = drives; l != NULL; l = l->next)
    {
      drive = l->data;
      volumes = g_drive_get_volumes (drive);

      if (volumes)
        {
          for (ll = volumes; ll != NULL; ll = ll->next)
            {
              volume = ll->data;
              mount = g_volume_get_mount (volume);

              if (mount)
                {
                  /* Show mounted volume */
                  priv->volumes = g_slist_prepend (priv->volumes, g_object_ref (mount));
                  g_object_unref (mount);
                }
              else
                {
                  /* Do show the unmounted volumes in the sidebar;
                   * this is so the user can mount it (in case automounting
                   * is off).
                   *
                   * Also, even if automounting is enabled, this gives a visual
                   * cue that the user should remember to yank out the media if
                   * he just unmounted it.
                   */
                  priv->volumes = g_slist_prepend (priv->volumes, g_object_ref (volume));
                }

	      g_object_unref (volume);
            }
  
           g_list_free (volumes);
        }
      else if (g_drive_is_media_removable (drive) && !g_drive_is_media_check_automatic (drive))
	{
	  /* If the drive has no mountable volumes and we cannot detect media change.. we
	   * display the drive in the sidebar so the user can manually poll the drive by
	   * right clicking and selecting "Rescan..."
	   *
	   * This is mainly for drives like floppies where media detection doesn't
	   * work.. but it's also for human beings who like to turn off media detection
	   * in the OS to save battery juice.
	   */

	  priv->volumes = g_slist_prepend (priv->volumes, g_object_ref (drive));
	}

      g_object_unref (drive);
    }

  g_list_free (drives);

  /* add all volumes that is not associated with a drive */
  volumes = g_volume_monitor_get_volumes (priv->volume_monitor);

  for (l = volumes; l != NULL; l = l->next)
    {
      volume = l->data;
      drive = g_volume_get_drive (volume);

      if (drive)
        {
          g_object_unref (drive);
          continue;
        }

      mount = g_volume_get_mount (volume);

      if (mount)
        {
          /* show this mount */
          priv->volumes = g_slist_prepend (priv->volumes, g_object_ref (mount));
          g_object_unref (mount);
        }
      else
        {
          /* see comment above in why we add an icon for a volume */
          priv->volumes = g_slist_prepend (priv->volumes, g_object_ref (volume));
        }

      g_object_unref (volume);
    }

  /* add mounts that has no volume (/etc/mtab mounts, ftp, sftp,...) */
  mounts = g_volume_monitor_get_mounts (priv->volume_monitor);

  for (l = mounts; l != NULL; l = l->next)
    {
      mount = l->data;
      volume = g_mount_get_volume (mount);

      if (volume)
        {
          g_object_unref (volume);
          continue;
        }

      /* if there's exists one or more volumes with an activation root inside the mount,
       * don't display the mount
       */
      if (mount_referenced_by_volume_activation_root (volumes, mount))
        {
          g_object_unref (mount);
          continue;
        }

      /* show this mount */
      priv->volumes = g_slist_prepend (priv->volumes, g_object_ref (mount));
      g_object_unref (mount);
    }

  g_list_free (volumes);

  g_list_free (mounts);
}

static void
_gtk_file_system_init (GtkFileSystem *file_system)
{
  GtkFileSystemPrivate *priv;
  GFile *bookmarks_file;
  GError *error = NULL;

  DEBUG ("init");

  priv = GTK_FILE_SYSTEM_GET_PRIVATE (file_system);

  /* Volumes */
  priv->volume_monitor = g_volume_monitor_get ();

  g_signal_connect (priv->volume_monitor, "mount-added",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "mount-removed",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "mount-changed",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "volume-added",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "volume-removed",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "volume-changed",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "drive-connected",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "drive-disconnected",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "drive-changed",
		    G_CALLBACK (volumes_changed), file_system);

  /* Bookmarks */
  bookmarks_file = get_bookmarks_file ();
  priv->bookmarks = read_bookmarks (bookmarks_file);
  if (!priv->bookmarks)
    {
      /* Use the legacy file instead */
      g_object_unref (bookmarks_file);
      bookmarks_file = get_legacy_bookmarks_file ();
      priv->bookmarks = read_bookmarks (bookmarks_file);
    }

  priv->bookmarks_monitor = g_file_monitor_file (bookmarks_file,
						 G_FILE_MONITOR_NONE,
						 NULL, &error);
  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }
  else
    g_signal_connect (priv->bookmarks_monitor, "changed",
		      G_CALLBACK (bookmarks_file_changed), file_system);

  priv->bookmarks_file = g_object_ref (bookmarks_file);
}

/* GtkFileSystem public methods */
GtkFileSystem *
_gtk_file_system_new (void)
{
  return g_object_new (GTK_TYPE_FILE_SYSTEM, NULL);
}

GSList *
_gtk_file_system_list_volumes (GtkFileSystem *file_system)
{
  GtkFileSystemPrivate *priv;
  GSList *list;

  DEBUG ("list_volumes");

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);

  priv = GTK_FILE_SYSTEM_GET_PRIVATE (file_system);
  get_volumes_list (GTK_FILE_SYSTEM (file_system));

  list = g_slist_copy (priv->volumes);

#ifndef G_OS_WIN32
  /* Prepend root volume */
  list = g_slist_prepend (list, (gpointer) root_volume_token);
#endif

  return list;
}

GSList *
_gtk_file_system_list_bookmarks (GtkFileSystem *file_system)
{
  GtkFileSystemPrivate *priv;
  GSList *bookmarks, *files = NULL;

  DEBUG ("list_bookmarks");

  priv = GTK_FILE_SYSTEM_GET_PRIVATE (file_system);
  bookmarks = priv->bookmarks;

  while (bookmarks)
    {
      GtkFileSystemBookmark *bookmark;

      bookmark = bookmarks->data;
      bookmarks = bookmarks->next;

      files = g_slist_prepend (files, g_object_ref (bookmark->file));
    }

  return g_slist_reverse (files);
}

static void
free_async_data (AsyncFuncData *async_data)
{
  g_object_unref (async_data->file_system);
  g_object_unref (async_data->file);
  g_object_unref (async_data->cancellable);

  g_free (async_data->attributes);
  g_free (async_data);
}

static void
query_info_callback (GObject      *source_object,
		     GAsyncResult *result,
		     gpointer      user_data)
{
  AsyncFuncData *async_data;
  GError *error = NULL;
  GFileInfo *file_info;
  GFile *file;

  DEBUG ("query_info_callback");

  file = G_FILE (source_object);
  async_data = (AsyncFuncData *) user_data;
  file_info = g_file_query_info_finish (file, result, &error);

  if (async_data->callback)
    {
      gdk_threads_enter ();
      ((GtkFileSystemGetInfoCallback) async_data->callback) (async_data->cancellable,
							     file_info, error, async_data->data);
      gdk_threads_leave ();
    }

  if (file_info)
    g_object_unref (file_info);

  if (error)
    g_error_free (error);

  free_async_data (async_data);
}

GCancellable *
_gtk_file_system_get_info (GtkFileSystem                *file_system,
			   GFile                        *file,
			   const gchar                  *attributes,
			   GtkFileSystemGetInfoCallback  callback,
			   gpointer                      data)
{
  GCancellable *cancellable;
  AsyncFuncData *async_data;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  cancellable = g_cancellable_new ();

  async_data = g_new0 (AsyncFuncData, 1);
  async_data->file_system = g_object_ref (file_system);
  async_data->file = g_object_ref (file);
  async_data->cancellable = g_object_ref (cancellable);

  async_data->callback = callback;
  async_data->data = data;

  g_file_query_info_async (file,
			   attributes,
			   G_FILE_QUERY_INFO_NONE,
			   G_PRIORITY_DEFAULT,
			   cancellable,
			   query_info_callback,
			   async_data);

  return cancellable;
}

static void
drive_poll_for_media_cb (GObject      *source_object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  AsyncFuncData *async_data;
  GError *error = NULL;

  g_drive_poll_for_media_finish (G_DRIVE (source_object), result, &error);
  async_data = (AsyncFuncData *) user_data;

  gdk_threads_enter ();
  ((GtkFileSystemVolumeMountCallback) async_data->callback) (async_data->cancellable,
							     (GtkFileSystemVolume *) source_object,
							     error, async_data->data);
  gdk_threads_leave ();

  if (error)
    g_error_free (error);
}

static void
volume_mount_cb (GObject      *source_object,
		 GAsyncResult *result,
		 gpointer      user_data)
{
  AsyncFuncData *async_data;
  GError *error = NULL;

  g_volume_mount_finish (G_VOLUME (source_object), result, &error);
  async_data = (AsyncFuncData *) user_data;

  gdk_threads_enter ();
  ((GtkFileSystemVolumeMountCallback) async_data->callback) (async_data->cancellable,
							     (GtkFileSystemVolume *) source_object,
							     error, async_data->data);
  gdk_threads_leave ();

  if (error)
    g_error_free (error);
}

GCancellable *
_gtk_file_system_mount_volume (GtkFileSystem                    *file_system,
			       GtkFileSystemVolume              *volume,
			       GMountOperation                  *mount_operation,
			       GtkFileSystemVolumeMountCallback  callback,
			       gpointer                          data)
{
  GCancellable *cancellable;
  AsyncFuncData *async_data;
  gboolean handled = FALSE;

  DEBUG ("volume_mount");

  cancellable = g_cancellable_new ();

  async_data = g_new0 (AsyncFuncData, 1);
  async_data->file_system = g_object_ref (file_system);
  async_data->cancellable = g_object_ref (cancellable);

  async_data->callback = callback;
  async_data->data = data;

  if (G_IS_DRIVE (volume))
    {
      /* this path happens for drives that are not polled by the OS and where the last media
       * check indicated that no media was available. So the thing to do here is to
       * invoke poll_for_media() on the drive
       */
      g_drive_poll_for_media (G_DRIVE (volume), cancellable, drive_poll_for_media_cb, async_data);
      handled = TRUE;
    }
  else if (G_IS_VOLUME (volume))
    {
      g_volume_mount (G_VOLUME (volume), G_MOUNT_MOUNT_NONE, mount_operation, cancellable, volume_mount_cb, async_data);
      handled = TRUE;
    }

  if (!handled)
    free_async_data (async_data);

  return cancellable;
}

static void
enclosing_volume_mount_cb (GObject      *source_object,
			   GAsyncResult *result,
			   gpointer      user_data)
{
  GtkFileSystemVolume *volume;
  AsyncFuncData *async_data;
  GError *error = NULL;

  async_data = (AsyncFuncData *) user_data;
  g_file_mount_enclosing_volume_finish (G_FILE (source_object), result, &error);
  volume = _gtk_file_system_get_volume_for_file (async_data->file_system, G_FILE (source_object));

  /* Silently drop G_IO_ERROR_ALREADY_MOUNTED error for gvfs backends without visible mounts. */
  /* Better than doing query_info with additional I/O every time. */
  if (error && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_ALREADY_MOUNTED))
    g_clear_error (&error);

  gdk_threads_enter ();
  ((GtkFileSystemVolumeMountCallback) async_data->callback) (async_data->cancellable, volume,
							     error, async_data->data);
  gdk_threads_leave ();

  if (error)
    g_error_free (error);

  _gtk_file_system_volume_unref (volume);
}

GCancellable *
_gtk_file_system_mount_enclosing_volume (GtkFileSystem                     *file_system,
					 GFile                             *file,
					 GMountOperation                   *mount_operation,
					 GtkFileSystemVolumeMountCallback   callback,
					 gpointer                           data)
{
  GCancellable *cancellable;
  AsyncFuncData *async_data;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  DEBUG ("mount_enclosing_volume");

  cancellable = g_cancellable_new ();

  async_data = g_new0 (AsyncFuncData, 1);
  async_data->file_system = g_object_ref (file_system);
  async_data->file = g_object_ref (file);
  async_data->cancellable = g_object_ref (cancellable);

  async_data->callback = callback;
  async_data->data = data;

  g_file_mount_enclosing_volume (file,
				 G_MOUNT_MOUNT_NONE,
				 mount_operation,
				 cancellable,
				 enclosing_volume_mount_cb,
				 async_data);
  return cancellable;
}

gboolean
_gtk_file_system_insert_bookmark (GtkFileSystem  *file_system,
				  GFile          *file,
				  gint            position,
				  GError        **error)
{
  GtkFileSystemPrivate *priv;
  GSList *bookmarks;
  GtkFileSystemBookmark *bookmark;
  gboolean result = TRUE;

  priv = GTK_FILE_SYSTEM_GET_PRIVATE (file_system);
  bookmarks = priv->bookmarks;

  while (bookmarks)
    {
      bookmark = bookmarks->data;
      bookmarks = bookmarks->next;

      if (g_file_equal (bookmark->file, file))
	{
	  /* File is already in bookmarks */
	  result = FALSE;
	  break;
	}
    }

  if (!result)
    {
      gchar *uri = g_file_get_uri (file);

      g_set_error (error,
		   GTK_FILE_CHOOSER_ERROR,
		   GTK_FILE_CHOOSER_ERROR_ALREADY_EXISTS,
		   "%s already exists in the bookmarks list",
		   uri);

      g_free (uri);

      return FALSE;
    }

  bookmark = g_slice_new0 (GtkFileSystemBookmark);
  bookmark->file = g_object_ref (file);

  priv->bookmarks = g_slist_insert (priv->bookmarks, bookmark, position);
  save_bookmarks (priv->bookmarks_file, priv->bookmarks);

  g_signal_emit (file_system, fs_signals[BOOKMARKS_CHANGED], 0);

  return TRUE;
}

gboolean
_gtk_file_system_remove_bookmark (GtkFileSystem  *file_system,
				  GFile          *file,
				  GError        **error)
{
  GtkFileSystemPrivate *priv;
  GtkFileSystemBookmark *bookmark;
  GSList *bookmarks;
  gboolean result = FALSE;

  priv = GTK_FILE_SYSTEM_GET_PRIVATE (file_system);

  if (!priv->bookmarks)
    return FALSE;

  bookmarks = priv->bookmarks;

  while (bookmarks)
    {
      bookmark = bookmarks->data;

      if (g_file_equal (bookmark->file, file))
	{
	  result = TRUE;
	  priv->bookmarks = g_slist_remove_link (priv->bookmarks, bookmarks);
	  _gtk_file_system_bookmark_free (bookmark);
	  g_slist_free_1 (bookmarks);
	  break;
	}

      bookmarks = bookmarks->next;
    }

  if (!result)
    {
      gchar *uri = g_file_get_uri (file);

      g_set_error (error,
		   GTK_FILE_CHOOSER_ERROR,
		   GTK_FILE_CHOOSER_ERROR_NONEXISTENT,
		   "%s does not exist in the bookmarks list",
		   uri);

      g_free (uri);

      return FALSE;
    }

  save_bookmarks (priv->bookmarks_file, priv->bookmarks);

  g_signal_emit (file_system, fs_signals[BOOKMARKS_CHANGED], 0);

  return TRUE;
}

gchar *
_gtk_file_system_get_bookmark_label (GtkFileSystem *file_system,
				     GFile         *file)
{
  GtkFileSystemPrivate *priv;
  GSList *bookmarks;
  gchar *label = NULL;

  DEBUG ("get_bookmark_label");

  priv = GTK_FILE_SYSTEM_GET_PRIVATE (file_system);
  bookmarks = priv->bookmarks;

  while (bookmarks)
    {
      GtkFileSystemBookmark *bookmark;

      bookmark = bookmarks->data;
      bookmarks = bookmarks->next;

      if (g_file_equal (file, bookmark->file))
	{
	  label = g_strdup (bookmark->label);
	  break;
	}
    }

  return label;
}

void
_gtk_file_system_set_bookmark_label (GtkFileSystem *file_system,
				     GFile         *file,
				     const gchar   *label)
{
  GtkFileSystemPrivate *priv;
  gboolean changed = FALSE;
  GSList *bookmarks;

  DEBUG ("set_bookmark_label");

  priv = GTK_FILE_SYSTEM_GET_PRIVATE (file_system);
  bookmarks = priv->bookmarks;

  while (bookmarks)
    {
      GtkFileSystemBookmark *bookmark;

      bookmark = bookmarks->data;
      bookmarks = bookmarks->next;

      if (g_file_equal (file, bookmark->file))
	{
          g_free (bookmark->label);
	  bookmark->label = g_strdup (label);
	  changed = TRUE;
	  break;
	}
    }

  save_bookmarks (priv->bookmarks_file, priv->bookmarks);

  if (changed)
    g_signal_emit_by_name (file_system, "bookmarks-changed", 0);
}

GtkFileSystemVolume *
_gtk_file_system_get_volume_for_file (GtkFileSystem *file_system,
				      GFile         *file)
{
  GMount *mount;

  DEBUG ("get_volume_for_file");

  mount = g_file_find_enclosing_mount (file, NULL, NULL);

  if (!mount && g_file_is_native (file))
    return (GtkFileSystemVolume *) root_volume_token;

  return (GtkFileSystemVolume *) mount;
}

/* GtkFolder methods */
static void
gtk_folder_set_property (GObject      *object,
			 guint         prop_id,
			 const GValue *value,
			 GParamSpec   *pspec)
{
  GtkFolderPrivate *priv;

  priv = GTK_FOLDER_GET_PRIVATE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      priv->folder_file = g_value_dup_object (value);
      break;
    case PROP_ENUMERATOR:
      priv->enumerator = g_value_dup_object (value);
      break;
    case PROP_ATTRIBUTES:
      priv->attributes = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_folder_get_property (GObject    *object,
			 guint       prop_id,
			 GValue     *value,
			 GParamSpec *pspec)
{
  GtkFolderPrivate *priv;

  priv = GTK_FOLDER_GET_PRIVATE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, priv->folder_file);
      break;
    case PROP_ENUMERATOR:
      g_value_set_object (value, priv->enumerator);
      break;
    case PROP_ATTRIBUTES:
      g_value_set_string (value, priv->attributes);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
query_created_file_info_callback (GObject      *source_object,
				  GAsyncResult *result,
				  gpointer      user_data)
{
  GFile *file = G_FILE (source_object);
  GError *error = NULL;
  GFileInfo *info;
  GtkFolder *folder;
  GSList *files;

  info = g_file_query_info_finish (file, result, &error);

  if (error)
    {
      g_error_free (error);
      return;
    }

  gdk_threads_enter ();

  folder = GTK_FOLDER (user_data);
  gtk_folder_add_file (folder, file, info);

  files = g_slist_prepend (NULL, file);
  g_signal_emit (folder, folder_signals[FILES_ADDED], 0, files);
  g_slist_free (files);

  g_object_unref (info);
  gdk_threads_leave ();
}

static void
directory_monitor_changed (GFileMonitor      *monitor,
			   GFile             *file,
			   GFile             *other_file,
			   GFileMonitorEvent  event,
			   gpointer           data)
{
  GtkFolderPrivate *priv;
  GtkFolder *folder;
  GSList *files;

  folder = GTK_FOLDER (data);
  priv = GTK_FOLDER_GET_PRIVATE (folder);
  files = g_slist_prepend (NULL, file);

  gdk_threads_enter ();

  switch (event)
    {
    case G_FILE_MONITOR_EVENT_CREATED:
      g_file_query_info_async (file,
			       priv->attributes,
			       G_FILE_QUERY_INFO_NONE,
			       G_PRIORITY_DEFAULT,
			       priv->cancellable,
			       query_created_file_info_callback,
			       folder);
      break;
    case G_FILE_MONITOR_EVENT_DELETED:
      if (g_file_equal (file, priv->folder_file))
	g_signal_emit (folder, folder_signals[DELETED], 0);
      else
	g_signal_emit (folder, folder_signals[FILES_REMOVED], 0, files);
      break;
    default:
      break;
    }

  gdk_threads_leave ();

  g_slist_free (files);
}

static void
enumerator_files_callback (GObject      *source_object,
			   GAsyncResult *result,
			   gpointer      user_data)
{
  GFileEnumerator *enumerator;
  GtkFolderPrivate *priv;
  GtkFolder *folder;
  GError *error = NULL;
  GSList *files = NULL;
  GList *file_infos, *f;

  enumerator = G_FILE_ENUMERATOR (source_object);
  file_infos = g_file_enumerator_next_files_finish (enumerator, result, &error);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  folder = GTK_FOLDER (user_data);
  priv = GTK_FOLDER_GET_PRIVATE (folder);

  if (!file_infos)
    {
      g_file_enumerator_close_async (enumerator,
				     G_PRIORITY_DEFAULT,
				     NULL, NULL, NULL);

      gtk_folder_set_finished_loading (folder, TRUE);
      return;
    }

  g_file_enumerator_next_files_async (enumerator, FILES_PER_QUERY,
				      G_PRIORITY_DEFAULT,
				      priv->cancellable,
				      enumerator_files_callback,
				      folder);

  for (f = file_infos; f; f = f->next)
    {
      GFileInfo *info;
      GFile *child_file;

      info = f->data;
      child_file = g_file_get_child (priv->folder_file, g_file_info_get_name (info));
      gtk_folder_add_file (folder, child_file, info);
      files = g_slist_prepend (files, child_file);
    }

  gdk_threads_enter ();
  g_signal_emit (folder, folder_signals[FILES_ADDED], 0, files);
  gdk_threads_leave ();

  g_list_foreach (file_infos, (GFunc) g_object_unref, NULL);
  g_list_free (file_infos);

  g_slist_foreach (files, (GFunc) g_object_unref, NULL);
  g_slist_free (files);
}

static void
gtk_folder_constructed (GObject *object)
{
  GtkFolderPrivate *priv;
  GError *error = NULL;

  priv = GTK_FOLDER_GET_PRIVATE (object);
  priv->directory_monitor = g_file_monitor_directory (priv->folder_file, G_FILE_MONITOR_NONE, NULL, &error);

  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }
  else
    g_signal_connect (priv->directory_monitor, "changed",
		      G_CALLBACK (directory_monitor_changed), object);

  g_file_enumerator_next_files_async (priv->enumerator,
				      FILES_PER_QUERY,
				      G_PRIORITY_DEFAULT,
				      priv->cancellable,
				      enumerator_files_callback,
				      object);
  /* This isn't needed anymore */
  g_object_unref (priv->enumerator);
  priv->enumerator = NULL;
}

static void
gtk_folder_finalize (GObject *object)
{
  GtkFolderPrivate *priv;

  priv = GTK_FOLDER_GET_PRIVATE (object);

  g_hash_table_unref (priv->children);

  if (priv->folder_file)
    g_object_unref (priv->folder_file);

  if (priv->directory_monitor)
    g_object_unref (priv->directory_monitor);

  g_cancellable_cancel (priv->cancellable);
  g_object_unref (priv->cancellable);
  g_free (priv->attributes);

  G_OBJECT_CLASS (_gtk_folder_parent_class)->finalize (object);
}

static void
_gtk_folder_class_init (GtkFolderClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->set_property = gtk_folder_set_property;
  object_class->get_property = gtk_folder_get_property;
  object_class->constructed = gtk_folder_constructed;
  object_class->finalize = gtk_folder_finalize;

  g_object_class_install_property (object_class,
				   PROP_FILE,
				   g_param_spec_object ("file",
							"File",
							"GFile for the folder",
							G_TYPE_FILE,
							GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
				   PROP_ENUMERATOR,
				   g_param_spec_object ("enumerator",
							"Enumerator",
							"GFileEnumerator to list files",
							G_TYPE_FILE_ENUMERATOR,
							GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
				   PROP_ATTRIBUTES,
				   g_param_spec_string ("attributes",
							"Attributes",
							"Attributes to query for",
							NULL,
							GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  folder_signals[FILES_ADDED] =
    g_signal_new ("files-added",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkFolderClass, files_added),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__POINTER,
		  G_TYPE_NONE, 1, G_TYPE_POINTER);
  folder_signals[FILES_REMOVED] =
    g_signal_new ("files-removed",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkFolderClass, files_removed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__POINTER,
		  G_TYPE_NONE, 1, G_TYPE_POINTER);
  folder_signals[FILES_CHANGED] =
    g_signal_new ("files-changed",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkFolderClass, files_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__POINTER,
		  G_TYPE_NONE, 1, G_TYPE_POINTER);
  folder_signals[FINISHED_LOADING] =
    g_signal_new ("finished-loading",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkFolderClass, finished_loading),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  folder_signals[DELETED] =
    g_signal_new ("deleted",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkFolderClass, deleted),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  g_type_class_add_private (object_class, sizeof (GtkFolderPrivate));
}

static void
_gtk_folder_init (GtkFolder *folder)
{
  GtkFolderPrivate *priv;

  priv = GTK_FOLDER_GET_PRIVATE (folder);

  priv->children = g_hash_table_new_full (g_file_hash,
					  (GEqualFunc) g_file_equal,
					  (GDestroyNotify) g_object_unref,
					  (GDestroyNotify) g_object_unref);
  priv->cancellable = g_cancellable_new ();
}

static void
gtk_folder_set_finished_loading (GtkFolder *folder,
				 gboolean   finished_loading)
{
  GtkFolderPrivate *priv;

  priv = GTK_FOLDER_GET_PRIVATE (folder);
  priv->finished_loading = (finished_loading == TRUE);

  gdk_threads_enter ();
  g_signal_emit (folder, folder_signals[FINISHED_LOADING], 0);
  gdk_threads_leave ();
}

static void
gtk_folder_add_file (GtkFolder *folder,
		     GFile     *file,
		     GFileInfo *info)
{
  GtkFolderPrivate *priv;

  priv = GTK_FOLDER_GET_PRIVATE (folder);

  g_hash_table_insert (priv->children,
		       g_object_ref (file),
		       g_object_ref (info));
}

GSList *
_gtk_folder_list_children (GtkFolder *folder)
{
  GtkFolderPrivate *priv;
  GList *files, *elem;
  GSList *children = NULL;

  priv = GTK_FOLDER_GET_PRIVATE (folder);
  files = g_hash_table_get_keys (priv->children);
  children = NULL;

  for (elem = files; elem; elem = elem->next)
    children = g_slist_prepend (children, g_object_ref (elem->data));

  g_list_free (files);

  return children;
}

GFileInfo *
_gtk_folder_get_info (GtkFolder  *folder,
		      GFile      *file)
{
  GtkFolderPrivate *priv;
  GFileInfo *info;

  priv = GTK_FOLDER_GET_PRIVATE (folder);
  info = g_hash_table_lookup (priv->children, file);

  if (!info)
    return NULL;

  return g_object_ref (info);
}

gboolean
_gtk_folder_is_finished_loading (GtkFolder *folder)
{
  GtkFolderPrivate *priv;

  priv = GTK_FOLDER_GET_PRIVATE (folder);

  return priv->finished_loading;
}

/* GtkFileSystemVolume public methods */
gchar *
_gtk_file_system_volume_get_display_name (GtkFileSystemVolume *volume)
{
  DEBUG ("volume_get_display_name");

  if (IS_ROOT_VOLUME (volume))
    return g_strdup (_(root_volume_token));
  if (G_IS_DRIVE (volume))
    return g_drive_get_name (G_DRIVE (volume));
  else if (G_IS_MOUNT (volume))
    return g_mount_get_name (G_MOUNT (volume));
  else if (G_IS_VOLUME (volume))
    return g_volume_get_name (G_VOLUME (volume));

  return NULL;
}

gboolean
_gtk_file_system_volume_is_mounted (GtkFileSystemVolume *volume)
{
  gboolean mounted;

  DEBUG ("volume_is_mounted");

  if (IS_ROOT_VOLUME (volume))
    return TRUE;

  mounted = FALSE;

  if (G_IS_MOUNT (volume))
    mounted = TRUE;
  else if (G_IS_VOLUME (volume))
    {
      GMount *mount;

      mount = g_volume_get_mount (G_VOLUME (volume));

      if (mount)
        {
          mounted = TRUE;
          g_object_unref (mount);
        }
    }

  return mounted;
}

GFile *
_gtk_file_system_volume_get_root (GtkFileSystemVolume *volume)
{
  GFile *file = NULL;

  DEBUG ("volume_get_base");

  if (IS_ROOT_VOLUME (volume))
    return g_file_new_for_uri ("file:///");

  if (G_IS_MOUNT (volume))
    file = g_mount_get_root (G_MOUNT (volume));
  else if (G_IS_VOLUME (volume))
    {
      GMount *mount;

      mount = g_volume_get_mount (G_VOLUME (volume));

      if (mount)
	{
	  file = g_mount_get_root (mount);
	  g_object_unref (mount);
	}
    }

  return file;
}

static GdkPixbuf *
get_pixbuf_from_gicon (GIcon      *icon,
		       GtkWidget  *widget,
		       gint        icon_size,
		       GError    **error)
{
  GdkScreen *screen;
  GtkIconTheme *icon_theme;
  GtkIconInfo *icon_info;
  GdkPixbuf *pixbuf;

  screen = gtk_widget_get_screen (GTK_WIDGET (widget));
  icon_theme = gtk_icon_theme_get_for_screen (screen);

  icon_info = gtk_icon_theme_lookup_by_gicon (icon_theme,
					      icon,
					      icon_size,
					      GTK_ICON_LOOKUP_USE_BUILTIN);

  if (!icon_info)
    return NULL;

  pixbuf = gtk_icon_info_load_icon (icon_info, error);
  gtk_icon_info_free (icon_info);

  return pixbuf;
}

GdkPixbuf *
_gtk_file_system_volume_render_icon (GtkFileSystemVolume  *volume,
				     GtkWidget            *widget,
				     gint                  icon_size,
				     GError              **error)
{
  GIcon *icon = NULL;
  GdkPixbuf *pixbuf;

  DEBUG ("volume_get_icon_name");

  if (IS_ROOT_VOLUME (volume))
    icon = g_themed_icon_new ("drive-harddisk");
  else if (G_IS_DRIVE (volume))
    icon = g_drive_get_icon (G_DRIVE (volume));
  else if (G_IS_VOLUME (volume))
    icon = g_volume_get_icon (G_VOLUME (volume));
  else if (G_IS_MOUNT (volume))
    icon = g_mount_get_icon (G_MOUNT (volume));

  if (!icon)
    return NULL;

  pixbuf = get_pixbuf_from_gicon (icon, widget, icon_size, error);

  g_object_unref (icon);

  return pixbuf;
}

GtkFileSystemVolume *
_gtk_file_system_volume_ref (GtkFileSystemVolume *volume)
{
  if (IS_ROOT_VOLUME (volume))
    return volume;

  if (G_IS_MOUNT (volume)  ||
      G_IS_VOLUME (volume) ||
      G_IS_DRIVE (volume))
    g_object_ref (volume);

  return volume;
}

void
_gtk_file_system_volume_unref (GtkFileSystemVolume *volume)
{
  /* Root volume doesn't need to be freed */
  if (IS_ROOT_VOLUME (volume))
    return;

  if (G_IS_MOUNT (volume)  ||
      G_IS_VOLUME (volume) ||
      G_IS_DRIVE (volume))
    g_object_unref (volume);
}

/* GFileInfo helper functions */
GdkPixbuf *
_gtk_file_info_render_icon (GFileInfo *info,
			   GtkWidget *widget,
			   gint       icon_size)
{
  GIcon *icon;
  GdkPixbuf *pixbuf = NULL;
  const gchar *thumbnail_path;

  thumbnail_path = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);

  if (thumbnail_path)
    pixbuf = gdk_pixbuf_new_from_file_at_size (thumbnail_path,
					       icon_size, icon_size,
					       NULL);

  if (!pixbuf)
    {
      icon = g_file_info_get_icon (info);

      if (icon)
	pixbuf = get_pixbuf_from_gicon (icon, widget, icon_size, NULL);

      if (!pixbuf)
	{
	   /* Use general fallback for all files without icon */
	  icon = g_themed_icon_new ("text-x-generic");
	  pixbuf = get_pixbuf_from_gicon (icon, widget, icon_size, NULL);
	  g_object_unref (icon);
	}
    }

  return pixbuf;
}

gboolean
_gtk_file_info_consider_as_directory (GFileInfo *info)
{
  GFileType type = g_file_info_get_file_type (info);
  
  return (type == G_FILE_TYPE_DIRECTORY ||
          type == G_FILE_TYPE_MOUNTABLE ||
          type == G_FILE_TYPE_SHORTCUT);
}

gboolean
_gtk_file_has_native_path (GFile *file)
{
  char *local_file_path;
  gboolean has_native_path;

  /* Don't use g_file_is_native(), as we want to support FUSE paths if available */
  local_file_path = g_file_get_path (file);
  has_native_path = (local_file_path != NULL);
  g_free (local_file_path);

  return has_native_path;
}
