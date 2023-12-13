/* gdkapplaunchcontext-x11.c - Gtk+ implementation for GAppLaunchContext

   Copyright (C) 2007 Red Hat, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Alexander Larsson <alexl@redhat.com>
*/

#include "config.h"

#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <gio/gdesktopappinfo.h>

#include "gdkx.h"
#include "gdkapplaunchcontext.h"
#include "gdkscreen.h"
#include "gdkinternals.h"
#include "gdkintl.h"
#include "gdkalias.h"


static char *
get_display_name (GFile     *file,
                  GFileInfo *info)
{
  char *name, *tmp;

  name = NULL;
  if (info)
    name = g_strdup (g_file_info_get_display_name (info));

  if (name == NULL)
    {
      name = g_file_get_basename (file);
      if (!g_utf8_validate (name, -1, NULL))
        {
          tmp = name;
          name =
            g_uri_escape_string (name, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
          g_free (tmp);
        }
    }

  return name;
}

static GIcon *
get_icon (GFile     *file,
          GFileInfo *info)
{
  GIcon *icon;

  icon = NULL;

  if (info)
    {
      icon = g_file_info_get_icon (info);
      if (icon)
        g_object_ref (icon);
    }

  return icon;
}

static char *
gicon_to_string (GIcon *icon)
{
  GFile *file;
  const char *const *names;

  if (G_IS_FILE_ICON (icon))
    {
      file = g_file_icon_get_file (G_FILE_ICON (icon));
      if (file)
        return g_file_get_path (file);
    }
  else if (G_IS_THEMED_ICON (icon))
    {
      names = g_themed_icon_get_names (G_THEMED_ICON (icon));
      if (names)
        return g_strdup (names[0]);
    }
  else if (G_IS_EMBLEMED_ICON (icon))
    {
      GIcon *base;

      base = g_emblemed_icon_get_icon (G_EMBLEMED_ICON (icon));

      return gicon_to_string (base);
    }

  return NULL;
}

static void
end_startup_notification (GdkDisplay *display,
                          const char *startup_id)
{
  gdk_x11_display_broadcast_startup_message (display, "remove",
                                             "ID", startup_id,
                                             NULL);
}


/* This should be fairly long, as it's confusing to users if a startup
 * ends when it shouldn't (it appears that the startup failed, and
 * they have to relaunch the app). Also the timeout only matters when
 * there are bugs and apps don't end their own startup sequence.
 *
 * This timeout is a "last resort" timeout that ignores whether the
 * startup sequence has shown activity or not.  Metacity and the
 * tasklist have smarter, and correspondingly able-to-be-shorter
 * timeouts. The reason our timeout is dumb is that we don't monitor
 * the sequence (don't use an SnMonitorContext)
 */
#define STARTUP_TIMEOUT_LENGTH_SECONDS 30 
#define STARTUP_TIMEOUT_LENGTH (STARTUP_TIMEOUT_LENGTH_SECONDS * 1000)

typedef struct 
{
  GdkDisplay *display;
  char *startup_id;
  GTimeVal time;
} StartupNotificationData;

static void
free_startup_notification_data (gpointer data)
{
  StartupNotificationData *sn_data = data;

  g_object_unref (sn_data->display);
  g_free (sn_data->startup_id);
  g_free (sn_data);
}

typedef struct 
{
  GSList *contexts;
  guint timeout_id;
} StartupTimeoutData;

static void
free_startup_timeout (void *data)
{
  StartupTimeoutData *std;

  std = data;

  g_slist_foreach (std->contexts, (GFunc) free_startup_notification_data, NULL);
  g_slist_free (std->contexts);

  if (std->timeout_id != 0)
    {
      g_source_remove (std->timeout_id);
      std->timeout_id = 0;
    }

  g_free (std);
}

static gboolean
startup_timeout (void *data)
{
  StartupTimeoutData *std;
  GSList *tmp;
  GTimeVal now;
  int min_timeout;

  std = data;

  min_timeout = STARTUP_TIMEOUT_LENGTH;

  g_get_current_time (&now);

  tmp = std->contexts;
  while (tmp != NULL)
    {
      StartupNotificationData *sn_data;
      GSList *next;
      double elapsed;

      sn_data = tmp->data;
      next = tmp->next;

      elapsed =
        ((((double) now.tv_sec - sn_data->time.tv_sec) * G_USEC_PER_SEC +
          (now.tv_usec - sn_data->time.tv_usec))) / 1000.0;

      if (elapsed >= STARTUP_TIMEOUT_LENGTH)
        {
          std->contexts = g_slist_remove (std->contexts, sn_data);
          end_startup_notification (sn_data->display, sn_data->startup_id);
          free_startup_notification_data (sn_data);
        }
      else
        {
          min_timeout = MIN (min_timeout, (STARTUP_TIMEOUT_LENGTH - elapsed));
        }

      tmp = next;
    }

  if (std->contexts == NULL)
    std->timeout_id = 0;
  else
    std->timeout_id = g_timeout_add_seconds ((min_timeout + 500)/1000, startup_timeout, std);

  /* always remove this one, but we may have reinstalled another one. */
  return FALSE;
}


static void
add_startup_timeout (GdkScreen  *screen,
                     const char *startup_id)
{
  StartupTimeoutData *data;
  StartupNotificationData *sn_data;

  data = g_object_get_data (G_OBJECT (screen), "appinfo-startup-data");

  if (data == NULL)
    {
      data = g_new (StartupTimeoutData, 1);
      data->contexts = NULL;
      data->timeout_id = 0;

      g_object_set_data_full (G_OBJECT (screen), "appinfo-startup-data",
                              data, free_startup_timeout);
    }

  sn_data = g_new (StartupNotificationData, 1);
  sn_data->display = g_object_ref (gdk_screen_get_display (screen));
  sn_data->startup_id = g_strdup (startup_id);
  g_get_current_time (&sn_data->time);

  data->contexts = g_slist_prepend (data->contexts, sn_data);

  if (data->timeout_id == 0)
    data->timeout_id = g_timeout_add_seconds (STARTUP_TIMEOUT_LENGTH_SECONDS,
                                              startup_timeout, data);
}


char *
_gdk_windowing_get_startup_notify_id (GAppLaunchContext *context,
                                      GAppInfo          *info,
                                      GList             *files)
{
  static int sequence = 0;
  GdkAppLaunchContextPrivate *priv;
  GdkDisplay *display;
  GdkScreen *screen;
  int files_count;
  char *description;
  char *icon_name;
  const char *binary_name;
  const char *application_id;
  char *screen_str;
  char *workspace_str;
  GIcon *icon;
  guint32 timestamp;
  char *startup_id;
  GFileInfo *fileinfo;

  priv = GDK_APP_LAUNCH_CONTEXT (context)->priv;

  if (priv->screen)
    {
      screen = priv->screen;
      display = gdk_screen_get_display (priv->screen);
    }
  else if (priv->display)
    {
      display = priv->display;
      screen = gdk_display_get_default_screen (display);
    }
  else
    {
      display = gdk_display_get_default ();
      screen = gdk_display_get_default_screen (display);
    }

  fileinfo = NULL;

  files_count = g_list_length (files);
  if (files_count == 0)
    {
      description = g_strdup_printf (_("Starting %s"), g_app_info_get_name (info));
    }
  else if (files_count == 1)
    {
      gchar *display_name;

      if (g_file_is_native (files->data))
        fileinfo = g_file_query_info (files->data,
                                      G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
                                      G_FILE_ATTRIBUTE_STANDARD_ICON,
                                      0, NULL, NULL);

      display_name = get_display_name (files->data, fileinfo);
      description = g_strdup_printf (_("Opening %s"), display_name);
      g_free (display_name);
    }
  else
    description = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
                                                "Opening %d Item",
                                                "Opening %d Items",
                                                files_count), files_count);

  icon_name = NULL;
  if (priv->icon_name)
    icon_name = g_strdup (priv->icon_name);
  else
    {
      icon = NULL;

      if (priv->icon != NULL)
        icon = g_object_ref (priv->icon);
      else if (files_count == 1)
        icon = get_icon (files->data, fileinfo);

      if (icon == NULL)
        {
          icon = g_app_info_get_icon (info);
          if (icon != NULL)
            g_object_ref (icon);
        }

      if (icon != NULL)
        {
          icon_name = gicon_to_string (icon);
          g_object_unref (icon);
        }
    }

  binary_name = g_app_info_get_executable (info);

  timestamp = priv->timestamp;
  if (timestamp == GDK_CURRENT_TIME)
    timestamp = gdk_x11_display_get_user_time (display);

  screen_str = g_strdup_printf ("%d", gdk_screen_get_number (screen));
  if (priv->workspace > -1)
    workspace_str = g_strdup_printf ("%d", priv->workspace);
  else
    workspace_str = NULL;

  if (G_IS_DESKTOP_APP_INFO (info))
    application_id = g_desktop_app_info_get_filename (G_DESKTOP_APP_INFO (info));
  else
    application_id = NULL;

  startup_id = g_strdup_printf ("%s-%lu-%s-%s-%d_TIME%lu",
                                g_get_prgname (),
                                (unsigned long)getpid (),
                                g_get_host_name (),
                                binary_name,
                                sequence++,
                                (unsigned long)timestamp);

  gdk_x11_display_broadcast_startup_message (display, "new",
                                             "ID", startup_id,
                                             "NAME", g_app_info_get_name (info),
                                             "SCREEN", screen_str,
                                             "BIN", binary_name,
                                             "ICON", icon_name,
                                             "DESKTOP", workspace_str,
                                             "DESCRIPTION", description,
                                             "WMCLASS", NULL, /* FIXME */
                                             "APPLICATION_ID", application_id,
                                             NULL);

  g_free (description);
  g_free (screen_str);
  g_free (workspace_str);
  g_free (icon_name);
  if (fileinfo)
    g_object_unref (fileinfo);

  add_startup_timeout (screen, startup_id);

  return startup_id;
}


void
_gdk_windowing_launch_failed (GAppLaunchContext *context,
                              const char        *startup_notify_id)
{
  GdkAppLaunchContextPrivate *priv;
  GdkScreen *screen;
  StartupTimeoutData *data;
  StartupNotificationData *sn_data;
  GSList *l;

  priv = GDK_APP_LAUNCH_CONTEXT (context)->priv;

  if (priv->screen)
    screen = priv->screen;
  else if (priv->display)
    screen = gdk_display_get_default_screen (priv->display);
  else
    screen = gdk_display_get_default_screen (gdk_display_get_default ());

  data = g_object_get_data (G_OBJECT (screen), "appinfo-startup-data");

  if (data)
    {
      for (l = data->contexts; l != NULL; l = l->next)
        {
          sn_data = l->data;
          if (strcmp (startup_notify_id, sn_data->startup_id) == 0)
            {
              data->contexts = g_slist_remove (data->contexts, sn_data);
              end_startup_notification (sn_data->display, sn_data->startup_id);
              free_startup_notification_data (sn_data);

              break;
            }
        }

      if (data->contexts == NULL)
        {
          g_source_remove (data->timeout_id);
          data->timeout_id = 0;
        }
    }
}
