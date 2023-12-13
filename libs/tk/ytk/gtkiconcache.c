/* gtkiconcache.c
 * Copyright (C) 2004  Anders Carlsson <andersca@gnome.org>
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

#include "gtkdebug.h"
#include "gtkiconcache.h"
#include "gtkiconcachevalidator.h"
#include "gtkalias.h"

#include <glib/gstdio.h>

#undef GDK_PIXBUF_DISABLE_DEPRECATED
#include <gdk-pixbuf/gdk-pixdata.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef G_OS_WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>


#ifndef _O_BINARY
#define _O_BINARY 0
#endif

#define MAJOR_VERSION 1
#define MINOR_VERSION 0

#define GET_UINT16(cache, offset) (GUINT16_FROM_BE (*(guint16 *)((cache) + (offset))))
#define GET_UINT32(cache, offset) (GUINT32_FROM_BE (*(guint32 *)((cache) + (offset))))


struct _GtkIconCache {
  gint ref_count;

  GMappedFile *map;
  gchar *buffer;

  guint32 last_chain_offset;
};

GtkIconCache *
_gtk_icon_cache_ref (GtkIconCache *cache)
{
  cache->ref_count++;
  return cache;
}

void
_gtk_icon_cache_unref (GtkIconCache *cache)
{
  cache->ref_count --;

  if (cache->ref_count == 0)
    {
      GTK_NOTE (ICONTHEME, 
		g_print ("unmapping icon cache\n"));

      if (cache->map)
	g_mapped_file_unref (cache->map);
      g_free (cache);
    }
}

GtkIconCache *
_gtk_icon_cache_new_for_path (const gchar *path)
{
  GtkIconCache *cache = NULL;
  GMappedFile *map;

  gchar *cache_filename;
  gint fd = -1;
  GStatBuf st;
  GStatBuf path_st;
  CacheInfo info;

   /* Check if we have a cache file */
  cache_filename = g_build_filename (path, "icon-theme.cache", NULL);

  GTK_NOTE (ICONTHEME, 
	    g_print ("look for cache in %s\n", path));

  if (g_stat (path, &path_st) < 0)
    goto done;

  /* Open the file and map it into memory */
  fd = g_open (cache_filename, O_RDONLY|_O_BINARY, 0);

  if (fd < 0)
    goto done;
  
  /* We need to know the correct function to call for the GStatBuf st we are using here,
   * because GStatBuf may not be stat on Windows
   */
#ifdef G_OS_WIN32
  if (GLIB_CHECK_VERSION (2, 57, 3))
    {
# ifdef __MINGW64_VERSION_MAJOR
#  ifdef _WIN64
#   define gtk_fstat _fstat64
#  else
#   define gtk_fstat _fstat32
#  endif
# elif defined (_MSC_VER) && !defined (_WIN64)
#   define gtk_fstat _fstat32
# endif
    }
#endif

#ifndef gtk_fstat
#define gtk_fstat fstat
#endif

  if (gtk_fstat (fd, &st) < 0 || st.st_size < 4)
    goto done;

#undef gtk_fstat

  /* Verify cache is uptodate */
  if (st.st_mtime < path_st.st_mtime)
    {
      GTK_NOTE (ICONTHEME, 
		g_print ("cache outdated\n"));
      goto done; 
    }

  map = g_mapped_file_new (cache_filename, FALSE, NULL);

  if (!map)
    goto done;

  info.cache = g_mapped_file_get_contents (map);
  info.cache_size = g_mapped_file_get_length (map);
  info.n_directories = 0;
  info.flags = CHECK_OFFSETS|CHECK_STRINGS;

#ifdef G_ENABLE_DEBUG
  if (gtk_debug_flags & GTK_DEBUG_ICONTHEME)
    {
      if (!_gtk_icon_cache_validate (&info))
        {
          g_mapped_file_unref (map);
          g_warning ("Icon cache '%s' is invalid\n", cache_filename);

          goto done;
        }
    }
#endif 

  GTK_NOTE (ICONTHEME, g_print ("found cache for %s\n", path));

  cache = g_new0 (GtkIconCache, 1);
  cache->ref_count = 1;
  cache->map = map;
  cache->buffer = g_mapped_file_get_contents (map);

 done:
  g_free (cache_filename);  
  if (fd >= 0)
    close (fd);

  return cache;
}

GtkIconCache *
_gtk_icon_cache_new (const gchar *data)
{
  GtkIconCache *cache;

  cache = g_new0 (GtkIconCache, 1);
  cache->ref_count = 1;
  cache->map = NULL;
  cache->buffer = (gchar *)data;
  
  return cache;
}

static gint
get_directory_index (GtkIconCache *cache,
		     const gchar *directory)
{
  guint32 dir_list_offset;
  gint n_dirs;
  gint i;
  
  dir_list_offset = GET_UINT32 (cache->buffer, 8);

  n_dirs = GET_UINT32 (cache->buffer, dir_list_offset);

  for (i = 0; i < n_dirs; i++)
    {
      guint32 name_offset = GET_UINT32 (cache->buffer, dir_list_offset + 4 + 4 * i);
      gchar *name = cache->buffer + name_offset;
      if (strcmp (name, directory) == 0)
	return i;
    }
  
  return -1;
}

gint
_gtk_icon_cache_get_directory_index (GtkIconCache *cache,
			             const gchar *directory)
{
  return get_directory_index (cache, directory);
}

static guint
icon_name_hash (gconstpointer key)
{
  const signed char *p = key;
  guint32 h = *p;

  if (h)
    for (p += 1; *p != '\0'; p++)
      h = (h << 5) - h + *p;

  return h;
}

static gint
find_image_offset (GtkIconCache *cache,
		   const gchar  *icon_name,
		   gint          directory_index)
{
  guint32 hash_offset;
  guint32 n_buckets;
  guint32 chain_offset;
  int hash;
  guint32 image_list_offset, n_images;
  int i;

  chain_offset = cache->last_chain_offset;
  if (chain_offset)
    {
      guint32 name_offset = GET_UINT32 (cache->buffer, chain_offset + 4);
      gchar *name = cache->buffer + name_offset;

      if (strcmp (name, icon_name) == 0)
        goto find_dir;
    }

  hash_offset = GET_UINT32 (cache->buffer, 4);
  n_buckets = GET_UINT32 (cache->buffer, hash_offset);
  hash = icon_name_hash (icon_name) % n_buckets;

  chain_offset = GET_UINT32 (cache->buffer, hash_offset + 4 + 4 * hash);
  while (chain_offset != 0xffffffff)
    {
      guint32 name_offset = GET_UINT32 (cache->buffer, chain_offset + 4);
      gchar *name = cache->buffer + name_offset;

      if (strcmp (name, icon_name) == 0)
        {
          cache->last_chain_offset = chain_offset;
          goto find_dir;
	}
  
      chain_offset = GET_UINT32 (cache->buffer, chain_offset);
    }

  cache->last_chain_offset = 0;
  return 0;

find_dir:
  /* We've found an icon list, now check if we have the right icon in it */
  image_list_offset = GET_UINT32 (cache->buffer, chain_offset + 8);
  n_images = GET_UINT32 (cache->buffer, image_list_offset);
  
  for (i = 0; i < n_images; i++)
    {
      if (GET_UINT16 (cache->buffer, image_list_offset + 4 + 8 * i) ==
	  directory_index) 
	return image_list_offset + 4 + 8 * i;
    }

  return 0;
}

gint
_gtk_icon_cache_get_icon_flags (GtkIconCache *cache,
				const gchar  *icon_name,
				gint          directory_index)
{
  guint32 image_offset;

  image_offset = find_image_offset (cache, icon_name, directory_index);

  if (!image_offset)
    return 0;

  return GET_UINT16 (cache->buffer, image_offset + 2);
}

void
_gtk_icon_cache_add_icons (GtkIconCache *cache,
			   const gchar  *directory,
			   GHashTable   *hash_table)
{
  int directory_index;
  guint32 hash_offset, n_buckets;
  guint32 chain_offset;
  guint32 image_list_offset, n_images;
  int i, j;
  
  directory_index = get_directory_index (cache, directory);

  if (directory_index == -1)
    return;
  
  hash_offset = GET_UINT32 (cache->buffer, 4);
  n_buckets = GET_UINT32 (cache->buffer, hash_offset);

  for (i = 0; i < n_buckets; i++)
    {
      chain_offset = GET_UINT32 (cache->buffer, hash_offset + 4 + 4 * i);
      while (chain_offset != 0xffffffff)
	{
	  guint32 name_offset = GET_UINT32 (cache->buffer, chain_offset + 4);
	  gchar *name = cache->buffer + name_offset;
	  
	  image_list_offset = GET_UINT32 (cache->buffer, chain_offset + 8);
	  n_images = GET_UINT32 (cache->buffer, image_list_offset);
  
	  for (j = 0; j < n_images; j++)
	    {
	      if (GET_UINT16 (cache->buffer, image_list_offset + 4 + 8 * j) ==
		  directory_index)
		g_hash_table_insert (hash_table, name, NULL);
	    }

	  chain_offset = GET_UINT32 (cache->buffer, chain_offset);
	}
    }  
}

gboolean
_gtk_icon_cache_has_icon (GtkIconCache *cache,
			  const gchar  *icon_name)
{
  guint32 hash_offset;
  guint32 n_buckets;
  guint32 chain_offset;
  gint hash;
  
  hash_offset = GET_UINT32 (cache->buffer, 4);
  n_buckets = GET_UINT32 (cache->buffer, hash_offset);

  hash = icon_name_hash (icon_name) % n_buckets;

  chain_offset = GET_UINT32 (cache->buffer, hash_offset + 4 + 4 * hash);
  while (chain_offset != 0xffffffff)
    {
      guint32 name_offset = GET_UINT32 (cache->buffer, chain_offset + 4);
      gchar *name = cache->buffer + name_offset;

      if (strcmp (name, icon_name) == 0)
	return TRUE;
	  
      chain_offset = GET_UINT32 (cache->buffer, chain_offset);
    }

  return FALSE;
}

gboolean
_gtk_icon_cache_has_icon_in_directory (GtkIconCache *cache,
				       const gchar  *icon_name,
				       const gchar  *directory)
{
  guint32 hash_offset;
  guint32 n_buckets;
  guint32 chain_offset;
  gint hash;
  gboolean found_icon = FALSE;
  gint directory_index;

  directory_index = get_directory_index (cache, directory);

  if (directory_index == -1)
    return FALSE;
  
  hash_offset = GET_UINT32 (cache->buffer, 4);
  n_buckets = GET_UINT32 (cache->buffer, hash_offset);

  hash = icon_name_hash (icon_name) % n_buckets;

  chain_offset = GET_UINT32 (cache->buffer, hash_offset + 4 + 4 * hash);
  while (chain_offset != 0xffffffff)
    {
      guint32 name_offset = GET_UINT32 (cache->buffer, chain_offset + 4);
      gchar *name = cache->buffer + name_offset;

      if (strcmp (name, icon_name) == 0)
	{
	  found_icon = TRUE;
	  break;
	}
	  
      chain_offset = GET_UINT32 (cache->buffer, chain_offset);
    }

  if (found_icon)
    {
      guint32 image_list_offset = GET_UINT32 (cache->buffer, chain_offset + 8);
      guint32 n_images =  GET_UINT32 (cache->buffer, image_list_offset);
      guint32 image_offset = image_list_offset + 4;
      gint i;
      for (i = 0; i < n_images; i++)
	{
	  guint16 index = GET_UINT16 (cache->buffer, image_offset);
	  
	  if (index == directory_index)
	    return TRUE;
	  image_offset += 8;
	}
    }

  return FALSE;
}

static void
pixbuf_destroy_cb (guchar   *pixels, 
		   gpointer  data)
{
  GtkIconCache *cache = data;

  _gtk_icon_cache_unref (cache);
}

GdkPixbuf *
_gtk_icon_cache_get_icon (GtkIconCache *cache,
			  const gchar  *icon_name,
			  gint          directory_index)
{
  guint32 offset, image_data_offset, pixel_data_offset;
  guint32 length, type;
  GdkPixbuf *pixbuf;
  GdkPixdata pixdata;
  GError *error = NULL;

  offset = find_image_offset (cache, icon_name, directory_index);
  
  image_data_offset = GET_UINT32 (cache->buffer, offset + 4);
  
  if (!image_data_offset)
    return NULL;

  pixel_data_offset = GET_UINT32 (cache->buffer, image_data_offset);

  type = GET_UINT32 (cache->buffer, pixel_data_offset);

  if (type != 0)
    {
      GTK_NOTE (ICONTHEME,
		g_print ("invalid pixel data type %u\n", type));
      return NULL;
    }

  length = GET_UINT32 (cache->buffer, pixel_data_offset + 4);
  
  if (!gdk_pixdata_deserialize (&pixdata, length, 
				(guchar *)(cache->buffer + pixel_data_offset + 8),
				&error))
    {
      GTK_NOTE (ICONTHEME,
		g_print ("could not deserialize data: %s\n", error->message));
      g_error_free (error);

      return NULL;
    }

  pixbuf = gdk_pixbuf_new_from_data (pixdata.pixel_data, GDK_COLORSPACE_RGB,
				     (pixdata.pixdata_type & GDK_PIXDATA_COLOR_TYPE_MASK) == GDK_PIXDATA_COLOR_TYPE_RGBA,
				     8, pixdata.width, pixdata.height, pixdata.rowstride,
				     (GdkPixbufDestroyNotify)pixbuf_destroy_cb, 
				     cache);
  if (!pixbuf)
    {
      GTK_NOTE (ICONTHEME,
		g_print ("could not convert pixdata to pixbuf: %s\n", error->message));
      g_error_free (error);

      return NULL;
    }

  _gtk_icon_cache_ref (cache);

  return pixbuf;
}

GtkIconData  *
_gtk_icon_cache_get_icon_data  (GtkIconCache *cache,
				const gchar  *icon_name,
				gint          directory_index)
{
  guint32 offset, image_data_offset, meta_data_offset;
  GtkIconData *data;
  int i;

  offset = find_image_offset (cache, icon_name, directory_index);
  if (!offset)
    return NULL;

  image_data_offset = GET_UINT32 (cache->buffer, offset + 4);
  if (!image_data_offset)
    return NULL;

  meta_data_offset = GET_UINT32 (cache->buffer, image_data_offset + 4);
  
  if (!meta_data_offset)
    return NULL;

  data = g_slice_new0 (GtkIconData);

  offset = GET_UINT32 (cache->buffer, meta_data_offset);
  if (offset)
    {
      data->has_embedded_rect = TRUE;
      data->x0 = GET_UINT16 (cache->buffer, offset);
      data->y0 = GET_UINT16 (cache->buffer, offset + 2);
      data->x1 = GET_UINT16 (cache->buffer, offset + 4);
      data->y1 = GET_UINT16 (cache->buffer, offset + 6);
    }

  offset = GET_UINT32 (cache->buffer, meta_data_offset + 4);
  if (offset)
    {
      data->n_attach_points = GET_UINT32 (cache->buffer, offset);
      data->attach_points = g_new (GdkPoint, data->n_attach_points);
      for (i = 0; i < data->n_attach_points; i++)
	{
	  data->attach_points[i].x = GET_UINT16 (cache->buffer, offset + 4 + 4 * i); 
	  data->attach_points[i].y = GET_UINT16 (cache->buffer, offset + 4 + 4 * i + 2); 
	}
    }

  offset = GET_UINT32 (cache->buffer, meta_data_offset + 8);
  if (offset)
    {
      gint n_names;
      gchar *lang, *name;
      gchar **langs;
      GHashTable *table = g_hash_table_new (g_str_hash, g_str_equal);

      n_names = GET_UINT32 (cache->buffer, offset);
      
      for (i = 0; i < n_names; i++)
	{
	  lang = cache->buffer + GET_UINT32 (cache->buffer, offset + 4 + 8 * i);
	  name = cache->buffer + GET_UINT32 (cache->buffer, offset + 4 + 8 * i + 4);
	  
	  g_hash_table_insert (table, lang, name);
	}
      
      langs = (gchar **)g_get_language_names ();
      for (i = 0; langs[i]; i++)
	{
	  name = g_hash_table_lookup (table, langs[i]);
	  if (name)
	    {
	      data->display_name = g_strdup (name);
	      break;
	    }
	}

      g_hash_table_destroy (table);
    }

  return data;
}

