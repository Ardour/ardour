/* gtklinuxrelocation: functions used to provide relocation on Linux
 *
 * Copyright (C) 2013 Whomsoever
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

#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "gtkalias.h"

#ifdef G_OS_WIN32

/* include relevant code here  */

#elif defined (__APPLE__)

#include <Foundation/Foundation.h>

static const gchar *
get_bundle_path (void)
{
  static gchar *path = NULL;

  if (path == NULL)
    {
      NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
      gchar *resource_path = g_strdup ([[[NSBundle mainBundle] resourcePath] UTF8String]);
      gchar *base;
      [pool drain];

      base = g_path_get_basename (resource_path);
      if (strcmp (base, "bin") == 0)
	path = g_path_get_dirname (resource_path);
      else
	path = strdup (resource_path);

      g_free (resource_path);
      g_free (base);
    }

  return path;
}

#else /* linux */

#include <limits.h>

#ifndef PATH_MAX
# define PATH_MAX 2048
#endif
#ifndef SSIZE_MAX
# define SSIZE_MAX LONG_MAX
#endif

/*
 * Find the canonical filename of the executable. Returns the filename
 * (which must be freed) or NULL on error. If the parameter 'error' is
 * not NULL, the error code will be stored there, if an error occured.
 */
static char *
_br_find_exe (gint *error)
{
        char *path, *path2, *line, *result;
        size_t buf_size;
        ssize_t size;
        struct stat stat_buf;
        FILE *f;

        /* Read from /proc/self/exe (symlink) */
        if (sizeof (path) > SSIZE_MAX)
                buf_size = SSIZE_MAX - 1;
        else
                buf_size = PATH_MAX - 1;
        path = g_try_new (char, buf_size);
        if (path == NULL) {
                /* Cannot allocate memory. */
                if (error)
                        *error = ENOMEM;
                return NULL;
        }
        path2 = g_try_new (char, buf_size);
        if (path2 == NULL) {
                /* Cannot allocate memory. */
                if (error)
                        *error = ENOMEM;
                g_free (path);
                return NULL;
        }

        strncpy (path2, "/proc/self/exe", buf_size - 1);

        while (1) {
                int i;

                size = readlink (path2, path, buf_size - 1);
                if (size == -1) {
                        /* Error. */
                        g_free (path2);
                        break;
                }

                /* readlink() success. */
                path[size] = '\0';

                /* Check whether the symlink's target is also a symlink.
                 * We want to get the final target. */
                i = stat (path, &stat_buf);
                if (i == -1) {
                        /* Error. */
                        g_free (path2);
                        break;
                }

                /* stat() success. */
                if (!S_ISLNK (stat_buf.st_mode)) {
                        /* path is not a symlink. Done. */
                        g_free (path2);
                        return path;
                }

                /* path is a symlink. Continue loop and resolve this. */
                strncpy (path, path2, buf_size - 1);
        }


        /* readlink() or stat() failed; this can happen when the program is
         * running in Valgrind 2.2. Read from /proc/self/maps as fallback. */

        buf_size = PATH_MAX + 128;
        line = (char *) g_try_realloc (path, buf_size);
        if (line == NULL) {
                /* Cannot allocate memory. */
                g_free (path);
                if (error)
                        *error = ENOMEM;
                return NULL;
        }

        f = g_fopen ("/proc/self/maps", "r");
        if (f == NULL) {
                g_free (line);
                if (error)
                        *error = ENOENT;
                return NULL;
        }

        /* The first entry should be the executable name. */
        result = fgets (line, (int) buf_size, f);
        if (result == NULL) {
                fclose (f);
                g_free (line);
                if (error)
                        *error = EIO;
                return NULL;
        }

        /* Get rid of newline character. */
        buf_size = strlen (line);
        if (buf_size <= 0) {
                /* Huh? An empty string? */
                fclose (f);
                g_free (line);
                if (error)
                        *error = ENOENT;
                return NULL;
        }
        if (line[buf_size - 1] == 10)
                line[buf_size - 1] = 0;

        /* Extract the filename; it is always an absolute path. */
        path = strchr (line, '/');

        /* Sanity check. */
        if (strstr (line, " r-xp ") == NULL || path == NULL) {
                fclose (f);
                g_free (line);
                if (error)
                        *error = EIO;
                return NULL;
        }

        path = g_strdup (path);
        g_free (line);
        fclose (f);
        return path;
}

static const gchar *
get_bundle_path (void)
{
  static gchar *path = NULL;
  
  if (path == NULL)
          path = (gchar*) g_getenv ("GTK_BUNDLEDIR");     

  if (path == NULL)
    {
      int err;            
      path = _br_find_exe (&err);

      if (path) 
        {      
           char* opath = path;
           char* dir = g_path_get_dirname (path);

           path = g_path_get_dirname (dir);

           g_free (opath);

           if (dir[0] == '.' && dir[1] == '\0')
              g_free (dir);
        }

    }

  return path;
}

#endif

const gchar *
_gtk_get_datadir (void)
{
  static const gchar *path = NULL;

  if (path == NULL)
     path = g_getenv ("GTK_DATADIR");     

  if (path == NULL)
    path = g_build_filename (get_bundle_path (), "share", NULL);

  return path;
}

const gchar *
_gtk_get_libdir (void)
{
  static const gchar *path = NULL;

  if (path == NULL)
     path = g_getenv ("GTK_LIBDIR");     

  if (path == NULL)
    path = g_build_filename (get_bundle_path (), "lib", NULL);

  return path;
}

const gchar *
_gtk_get_localedir (void)
{
  static const gchar *path = NULL;

  if (path == NULL)
     path = g_getenv ("GTK_LOCALEDIR");     

  if (path == NULL)
    path = g_build_filename (get_bundle_path (), "share", "locale", NULL);

  return path;
}

const gchar *
_gtk_get_sysconfdir (void)
{
  static const gchar *path = NULL;

  if (path == NULL)
     path = g_getenv ("GTK_SYSCONFDIR");     

  if (path == NULL)
    path = g_build_filename (get_bundle_path (), "etc", NULL);

  return path;
}

const gchar *
_gtk_get_data_prefix (void)
{
  return get_bundle_path ();
}
