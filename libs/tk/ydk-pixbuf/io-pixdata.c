/* GdkPixdata loader
 *
 * Copyright (c) 2012 Alexander Larsson <alexl@redhat.com>
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "gdk-pixbuf-private.h"
#include "gdk-pixdata.h"

G_MODULE_EXPORT void fill_vtable (GdkPixbufModule * module);
G_MODULE_EXPORT void fill_info (GdkPixbufFormat * info);

struct pixdata_context {
  GdkPixbufModuleSizeFunc size_func;
  GdkPixbufModuleUpdatedFunc updated_func;
  GdkPixbufModulePreparedFunc prepared_func;
  gpointer user_data;

  GString *data;

  GdkPixdata pixdata;
  gboolean got_header;
  gboolean got_pixbuf;
};

static void
free_pixdata_context (struct pixdata_context *context)
{
  if (!context)
    return;

  g_free (context);
}

static gpointer
pixdata_image_begin_load (GdkPixbufModuleSizeFunc size_func,
			  GdkPixbufModulePreparedFunc prepared_func,
			  GdkPixbufModuleUpdatedFunc updated_func,
			  gpointer user_data, GError **error)
{
  struct pixdata_context *context;

  context = g_new0 (struct pixdata_context, 1);
  if (!context)
    return NULL;

  context->size_func = size_func;
  context->updated_func = updated_func;
  context->prepared_func = prepared_func;
  context->user_data = user_data;

  context->data = g_string_new ("");

  return context;
}

static gboolean try_load (struct pixdata_context *context, GError **error)
{
  GdkPixbuf *pixbuf;

  if (context->got_pixbuf)
    return TRUE;

  if (!gdk_pixdata_deserialize (&context->pixdata,
				context->data->len,
				(guchar *)context->data->str,
				error))
    return FALSE;

  pixbuf = gdk_pixbuf_from_pixdata (&context->pixdata,
				    TRUE, error);
  if (pixbuf == NULL)
    return FALSE;

  context->got_pixbuf = TRUE;

  if (context->prepared_func)
    (* context->prepared_func) (pixbuf,
				NULL,
				context->user_data);
  if (context->updated_func)
    (* context->updated_func) (pixbuf, 0, 0, pixbuf->width, pixbuf->height, context->user_data);

  return TRUE;
}

static gboolean
pixdata_image_stop_load (gpointer data, GError **error)
{
  struct pixdata_context *context = (struct pixdata_context *) data;
  gboolean res;

  res = try_load (context, error);

  g_string_free (context->data, TRUE);

  free_pixdata_context (context);

  return res;
}

static gboolean
pixdata_image_load_increment (gpointer data, const guchar *buf, guint size, GError **error)
{
  struct pixdata_context *context = (struct pixdata_context *) data;

  g_string_append_len (context->data, (char *)buf, size);

  if (!context->got_header && context->data->len >= GDK_PIXDATA_HEADER_LENGTH)
    {
      /* This never reads past the header anyway, and we know we have at least
	 the header size, so we pass it a really large size to avoid any error reporting
	 due to missing data */
      if (!gdk_pixdata_deserialize (&context->pixdata,
				    G_MAXUINT,
				    (guchar *)context->data->str,
				    error))
	return FALSE;

      context->got_header = TRUE;

      if (context->size_func)
	{
	  gint w = context->pixdata.width;
	  gint h = context->pixdata.height;
	  (* context->size_func) (&w, &h, context->user_data);

	  if (w == 0 || h == 0)
	    {
	      g_set_error_literal (error,
				   GDK_PIXBUF_ERROR,
				   GDK_PIXBUF_ERROR_FAILED,
				   _("Transformed pixbuf has zero width or height."));
	      return FALSE;
	    }
	}
    }

  try_load (context, NULL);

  return TRUE;
}

/* Always included */
#define MODULE_ENTRY(function) void _gdk_pixbuf__pixdata_ ## function

MODULE_ENTRY (fill_vtable) (GdkPixbufModule * module)
{
	module->begin_load = pixdata_image_begin_load;
	module->stop_load = pixdata_image_stop_load;
	module->load_increment = pixdata_image_load_increment;
}

MODULE_ENTRY (fill_info) (GdkPixbufFormat * info)
{
	static const GdkPixbufModulePattern signature[] = {
		{ "GdkP", NULL, 100 },		/* file begins with 'GdkP' at offset 0 */
		{ NULL, NULL, 0 }
	};
	static const gchar *mime_types[] = {
		"image/x-gdkpixdata",
		NULL
	};
	static const gchar *extensions[] = {
		"gdkp",
		NULL
	};

	info->name = "GdkPixdata";
	info->signature = (GdkPixbufModulePattern *) signature;
	info->description = N_("The GdkPixdata format");
	info->mime_types = (gchar **) mime_types;
	info->extensions = (gchar **) extensions;
	info->flags = GDK_PIXBUF_FORMAT_THREADSAFE;
	info->license = "LGPL";
	info->disabled = FALSE;
}
