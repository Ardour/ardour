/* gtkrichtext.c
 *
 * Copyright (C) 2006 Imendio AB
 * Contact: Michael Natterer <mitch@imendio.com>
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

#include <string.h>

#include "gtktextbufferrichtext.h"
#include "gtktextbufferserialize.h"
#include "gtkintl.h"
#include "gtkalias.h"


typedef struct
{
  gchar          *mime_type;
  gboolean        can_create_tags;
  GdkAtom         atom;
  gpointer        function;
  gpointer        user_data;
  GDestroyNotify  user_data_destroy;
} GtkRichTextFormat;


static GList   * register_format   (GList             *formats,
                                    const gchar       *mime_type,
                                    gpointer           function,
                                    gpointer           user_data,
                                    GDestroyNotify     user_data_destroy,
                                    GdkAtom           *atom);
static GList   * unregister_format (GList             *formats,
                                    GdkAtom            atom);
static GdkAtom * get_formats       (GList             *formats,
                                    gint              *n_formats);
static void      free_format       (GtkRichTextFormat *format);
static void      free_format_list  (GList             *formats);
static GQuark    serialize_quark   (void);
static GQuark    deserialize_quark (void);


/**
 * gtk_text_buffer_register_serialize_format:
 * @buffer: a #GtkTextBuffer
 * @mime_type: the format's mime-type
 * @function: the serialize function to register
 * @user_data: %function's user_data
 * @user_data_destroy: a function to call when @user_data is no longer needed
 *
 * This function registers a rich text serialization @function along with
 * its @mime_type with the passed @buffer.
 *
 * Return value: (transfer none): the #GdkAtom that corresponds to the
 *               newly registered format's mime-type.
 *
 * Since: 2.10
 **/
GdkAtom
gtk_text_buffer_register_serialize_format (GtkTextBuffer              *buffer,
                                           const gchar                *mime_type,
                                           GtkTextBufferSerializeFunc  function,
                                           gpointer                    user_data,
                                           GDestroyNotify              user_data_destroy)
{
  GList   *formats;
  GdkAtom  atom;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), GDK_NONE);
  g_return_val_if_fail (mime_type != NULL && *mime_type != '\0', GDK_NONE);
  g_return_val_if_fail (function != NULL, GDK_NONE);

  formats = g_object_steal_qdata (G_OBJECT (buffer), serialize_quark ());

  formats = register_format (formats, mime_type,
                             (gpointer) function,
                             user_data, user_data_destroy,
                             &atom);

  g_object_set_qdata_full (G_OBJECT (buffer), serialize_quark (),
                           formats, (GDestroyNotify) free_format_list);

  g_object_notify (G_OBJECT (buffer), "copy-target-list");

  return atom;
}

/**
 * gtk_text_buffer_register_serialize_tagset:
 * @buffer: a #GtkTextBuffer
 * @tagset_name: (allow-none): an optional tagset name, on %NULL
 *
 * This function registers GTK+'s internal rich text serialization
 * format with the passed @buffer. The internal format does not comply
 * to any standard rich text format and only works between #GtkTextBuffer
 * instances. It is capable of serializing all of a text buffer's tags
 * and embedded pixbufs.
 *
 * This function is just a wrapper around
 * gtk_text_buffer_register_serialize_format(). The mime type used
 * for registering is "application/x-gtk-text-buffer-rich-text", or
 * "application/x-gtk-text-buffer-rich-text;format=@tagset_name" if a
 * @tagset_name was passed.
 *
 * The @tagset_name can be used to restrict the transfer of rich text
 * to buffers with compatible sets of tags, in order to avoid unknown
 * tags from being pasted. It is probably the common case to pass an
 * identifier != %NULL here, since the %NULL tagset requires the
 * receiving buffer to deal with with pasting of arbitrary tags.
 *
 * Return value: (transfer none): the #GdkAtom that corresponds to the
 *               newly registered format's mime-type.
 *
 * Since: 2.10
 **/
GdkAtom
gtk_text_buffer_register_serialize_tagset (GtkTextBuffer *buffer,
                                           const gchar   *tagset_name)
{
  gchar   *mime_type = "application/x-gtk-text-buffer-rich-text";
  GdkAtom  format;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), GDK_NONE);
  g_return_val_if_fail (tagset_name == NULL || *tagset_name != '\0', GDK_NONE);

  if (tagset_name)
    mime_type =
      g_strdup_printf ("application/x-gtk-text-buffer-rich-text;format=%s",
                       tagset_name);

  format = gtk_text_buffer_register_serialize_format (buffer, mime_type,
                                                      _gtk_text_buffer_serialize_rich_text,
                                                      NULL, NULL);

  if (tagset_name)
    g_free (mime_type);

  return format;
}

/**
 * gtk_text_buffer_register_deserialize_format:
 * @buffer: a #GtkTextBuffer
 * @mime_type: the format's mime-type
 * @function: the deserialize function to register
 * @user_data: @function's user_data
 * @user_data_destroy: a function to call when @user_data is no longer needed
 *
 * This function registers a rich text deserialization @function along with
 * its @mime_type with the passed @buffer.
 *
 * Return value: (transfer none): the #GdkAtom that corresponds to the
 *               newly registered format's mime-type.
 *
 * Since: 2.10
 **/
GdkAtom
gtk_text_buffer_register_deserialize_format (GtkTextBuffer                *buffer,
                                             const gchar                  *mime_type,
                                             GtkTextBufferDeserializeFunc  function,
                                             gpointer                      user_data,
                                             GDestroyNotify                user_data_destroy)
{
  GList   *formats;
  GdkAtom  atom;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), GDK_NONE);
  g_return_val_if_fail (mime_type != NULL && *mime_type != '\0', GDK_NONE);
  g_return_val_if_fail (function != NULL, GDK_NONE);

  formats = g_object_steal_qdata (G_OBJECT (buffer), deserialize_quark ());

  formats = register_format (formats, mime_type,
                             (gpointer) function,
                             user_data, user_data_destroy,
                             &atom);

  g_object_set_qdata_full (G_OBJECT (buffer), deserialize_quark (),
                           formats, (GDestroyNotify) free_format_list);

  g_object_notify (G_OBJECT (buffer), "paste-target-list");

  return atom;
}

/**
 * gtk_text_buffer_register_deserialize_tagset:
 * @buffer: a #GtkTextBuffer
 * @tagset_name: (allow-none): an optional tagset name, on %NULL
 *
 * This function registers GTK+'s internal rich text serialization
 * format with the passed @buffer. See
 * gtk_text_buffer_register_serialize_tagset() for details.
 *
 * Return value: (transfer none): the #GdkAtom that corresponds to the
 *               newly registered format's mime-type.
 *
 * Since: 2.10
 **/
GdkAtom
gtk_text_buffer_register_deserialize_tagset (GtkTextBuffer *buffer,
                                             const gchar   *tagset_name)
{
  gchar   *mime_type = "application/x-gtk-text-buffer-rich-text";
  GdkAtom  format;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), GDK_NONE);
  g_return_val_if_fail (tagset_name == NULL || *tagset_name != '\0', GDK_NONE);

  if (tagset_name)
    mime_type =
      g_strdup_printf ("application/x-gtk-text-buffer-rich-text;format=%s",
                       tagset_name);

  format = gtk_text_buffer_register_deserialize_format (buffer, mime_type,
                                                        _gtk_text_buffer_deserialize_rich_text,
                                                        NULL, NULL);

  if (tagset_name)
    g_free (mime_type);

  return format;
}

/**
 * gtk_text_buffer_unregister_serialize_format:
 * @buffer: a #GtkTextBuffer
 * @format: a #GdkAtom representing a registered rich text format.
 *
 * This function unregisters a rich text format that was previously
 * registered using gtk_text_buffer_register_serialize_format() or
 * gtk_text_buffer_register_serialize_tagset()
 *
 * Since: 2.10
 **/
void
gtk_text_buffer_unregister_serialize_format (GtkTextBuffer *buffer,
                                             GdkAtom        format)
{
  GList *formats;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (format != GDK_NONE);

  formats = g_object_steal_qdata (G_OBJECT (buffer), serialize_quark ());

  formats = unregister_format (formats, format);

  g_object_set_qdata_full (G_OBJECT (buffer), serialize_quark (),
                           formats, (GDestroyNotify) free_format_list);

  g_object_notify (G_OBJECT (buffer), "copy-target-list");
}

/**
 * gtk_text_buffer_unregister_deserialize_format:
 * @buffer: a #GtkTextBuffer
 * @format: a #GdkAtom representing a registered rich text format.
 *
 * This function unregisters a rich text format that was previously
 * registered using gtk_text_buffer_register_deserialize_format() or
 * gtk_text_buffer_register_deserialize_tagset().
 *
 * Since: 2.10
 **/
void
gtk_text_buffer_unregister_deserialize_format (GtkTextBuffer *buffer,
                                               GdkAtom        format)
{
  GList *formats;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (format != GDK_NONE);

  formats = g_object_steal_qdata (G_OBJECT (buffer), deserialize_quark ());

  formats = unregister_format (formats, format);

  g_object_set_qdata_full (G_OBJECT (buffer), deserialize_quark (),
                           formats, (GDestroyNotify) free_format_list);

  g_object_notify (G_OBJECT (buffer), "paste-target-list");
}

/**
 * gtk_text_buffer_deserialize_set_can_create_tags:
 * @buffer: a #GtkTextBuffer
 * @format: a #GdkAtom representing a registered rich text format
 * @can_create_tags: whether deserializing this format may create tags
 *
 * Use this function to allow a rich text deserialization function to
 * create new tags in the receiving buffer. Note that using this
 * function is almost always a bad idea, because the rich text
 * functions you register should know how to map the rich text format
 * they handler to your text buffers set of tags.
 *
 * The ability of creating new (arbitrary!) tags in the receiving buffer
 * is meant for special rich text formats like the internal one that
 * is registered using gtk_text_buffer_register_deserialize_tagset(),
 * because that format is essentially a dump of the internal structure
 * of the source buffer, including its tag names.
 *
 * You should allow creation of tags only if you know what you are
 * doing, e.g. if you defined a tagset name for your application
 * suite's text buffers and you know that it's fine to receive new
 * tags from these buffers, because you know that your application can
 * handle the newly created tags.
 *
 * Since: 2.10
 **/
void
gtk_text_buffer_deserialize_set_can_create_tags (GtkTextBuffer *buffer,
                                                 GdkAtom        format,
                                                 gboolean       can_create_tags)
{
  GList *formats;
  GList *list;
  gchar *format_name;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (format != GDK_NONE);

  formats = g_object_get_qdata (G_OBJECT (buffer), deserialize_quark ());

  for (list = formats; list; list = g_list_next (list))
    {
      GtkRichTextFormat *fmt = list->data;

      if (fmt->atom == format)
        {
          fmt->can_create_tags = can_create_tags ? TRUE : FALSE;
          return;
        }
    }

  format_name = gdk_atom_name (format);
  g_warning ("%s: \"%s\" is not registered as deserializable format "
             "with text buffer %p",
             G_STRFUNC, format_name ? format_name : "not a GdkAtom", buffer);
  g_free (format_name);
}

/**
 * gtk_text_buffer_deserialize_get_can_create_tags:
 * @buffer: a #GtkTextBuffer
 * @format: a #GdkAtom representing a registered rich text format
 *
 * This functions returns the value set with
 * gtk_text_buffer_deserialize_set_can_create_tags()
 *
 * Return value: whether deserializing this format may create tags
 *
 * Since: 2.10
 **/
gboolean
gtk_text_buffer_deserialize_get_can_create_tags (GtkTextBuffer *buffer,
                                                 GdkAtom        format)
{
  GList *formats;
  GList *list;
  gchar *format_name;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);
  g_return_val_if_fail (format != GDK_NONE, FALSE);

  formats = g_object_get_qdata (G_OBJECT (buffer), deserialize_quark ());

  for (list = formats; list; list = g_list_next (list))
    {
      GtkRichTextFormat *fmt = list->data;

      if (fmt->atom == format)
        {
          return fmt->can_create_tags;
        }
    }

  format_name = gdk_atom_name (format);
  g_warning ("%s: \"%s\" is not registered as deserializable format "
             "with text buffer %p",
             G_STRFUNC, format_name ? format_name : "not a GdkAtom", buffer);
  g_free (format_name);

  return FALSE;
}

/**
 * gtk_text_buffer_get_serialize_formats:
 * @buffer: a #GtkTextBuffer
 * @n_formats: return location for the number of formats
 *
 * This function returns the rich text serialize formats registered
 * with @buffer using gtk_text_buffer_register_serialize_format() or
 * gtk_text_buffer_register_serialize_tagset()
 *
 * Return value: (array length=n_formats) (transfer container): an array of
 *               #GdkAtom<!-- -->s representing the registered formats.
 *
 * Since: 2.10
 **/
GdkAtom *
gtk_text_buffer_get_serialize_formats (GtkTextBuffer *buffer,
                                       gint          *n_formats)
{
  GList *formats;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);
  g_return_val_if_fail (n_formats != NULL, NULL);

  formats = g_object_get_qdata (G_OBJECT (buffer), serialize_quark ());

  return get_formats (formats, n_formats);
}

/**
 * gtk_text_buffer_get_deserialize_formats:
 * @buffer: a #GtkTextBuffer
 * @n_formats: return location for the number of formats
 *
 * This function returns the rich text deserialize formats registered
 * with @buffer using gtk_text_buffer_register_deserialize_format() or
 * gtk_text_buffer_register_deserialize_tagset()
 *
 * Return value: (array length=n_formats) (transfer container): an array of
 *               #GdkAtom<!-- -->s representing the registered formats.
 *
 * Since: 2.10
 **/
GdkAtom *
gtk_text_buffer_get_deserialize_formats (GtkTextBuffer *buffer,
                                         gint          *n_formats)
{
  GList *formats;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);
  g_return_val_if_fail (n_formats != NULL, NULL);

  formats = g_object_get_qdata (G_OBJECT (buffer), deserialize_quark ());

  return get_formats (formats, n_formats);
}

/**
 * gtk_text_buffer_serialize:
 * @register_buffer: the #GtkTextBuffer @format is registered with
 * @content_buffer: the #GtkTextBuffer to serialize
 * @format: the rich text format to use for serializing
 * @start: start of block of text to serialize
 * @end: end of block of test to serialize
 * @length: return location for the length of the serialized data
 *
 * This function serializes the portion of text between @start
 * and @end in the rich text format represented by @format.
 *
 * @format<!-- -->s to be used must be registered using
 * gtk_text_buffer_register_serialize_format() or
 * gtk_text_buffer_register_serialize_tagset() beforehand.
 *
 * Return value: (array length=length) (transfer full): the serialized
 *               data, encoded as @format
 *
 * Since: 2.10
 **/
guint8 *
gtk_text_buffer_serialize (GtkTextBuffer     *register_buffer,
                           GtkTextBuffer     *content_buffer,
                           GdkAtom            format,
                           const GtkTextIter *start,
                           const GtkTextIter *end,
                           gsize             *length)
{
  GList *formats;
  GList *list;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (register_buffer), NULL);
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (content_buffer), NULL);
  g_return_val_if_fail (format != GDK_NONE, NULL);
  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (end != NULL, NULL);
  g_return_val_if_fail (length != NULL, NULL);

  *length = 0;

  formats = g_object_get_qdata (G_OBJECT (register_buffer),
                                serialize_quark ());

  for (list = formats; list; list = g_list_next (list))
    {
      GtkRichTextFormat *fmt = list->data;

      if (fmt->atom == format)
        {
          GtkTextBufferSerializeFunc function = fmt->function;

          return function (register_buffer, content_buffer,
                           start, end, length, fmt->user_data);
        }
    }

  return NULL;
}

/**
 * gtk_text_buffer_deserialize:
 * @register_buffer: the #GtkTextBuffer @format is registered with
 * @content_buffer: the #GtkTextBuffer to deserialize into
 * @format: the rich text format to use for deserializing
 * @iter: insertion point for the deserialized text
 * @data: (array length=length): data to deserialize
 * @length: length of @data
 * @error: return location for a #GError
 *
 * This function deserializes rich text in format @format and inserts
 * it at @iter.
 *
 * @format<!-- -->s to be used must be registered using
 * gtk_text_buffer_register_deserialize_format() or
 * gtk_text_buffer_register_deserialize_tagset() beforehand.
 *
 * Return value: %TRUE on success, %FALSE otherwise.
 *
 * Since: 2.10
 **/
gboolean
gtk_text_buffer_deserialize (GtkTextBuffer  *register_buffer,
                             GtkTextBuffer  *content_buffer,
                             GdkAtom         format,
                             GtkTextIter    *iter,
                             const guint8   *data,
                             gsize           length,
                             GError        **error)
{
  GList    *formats;
  GList    *list;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (register_buffer), FALSE);
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (content_buffer), FALSE);
  g_return_val_if_fail (format != GDK_NONE, FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (length > 0, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  formats = g_object_get_qdata (G_OBJECT (register_buffer),
                                deserialize_quark ());

  for (list = formats; list; list = g_list_next (list))
    {
      GtkRichTextFormat *fmt = list->data;

      if (fmt->atom == format)
        {
          GtkTextBufferDeserializeFunc function = fmt->function;
          gboolean                     success;
          GSList                      *split_tags;
          GSList                      *list;
          GtkTextMark                 *left_end        = NULL;
          GtkTextMark                 *right_start     = NULL;
          GSList                      *left_start_list = NULL;
          GSList                      *right_end_list  = NULL;

          /*  We don't want the tags that are effective at the insertion
           *  point to affect the pasted text, therefore we remove and
           *  remember them, so they can be re-applied left and right of
           *  the inserted text after pasting
           */
          split_tags = gtk_text_iter_get_tags (iter);

          list = split_tags;
          while (list)
            {
              GtkTextTag *tag = list->data;

              list = g_slist_next (list);

              /*  If a tag begins at the insertion point, ignore it
               *  because it doesn't affect the pasted text
               */
              if (gtk_text_iter_begins_tag (iter, tag))
                split_tags = g_slist_remove (split_tags, tag);
            }

          if (split_tags)
            {
              /*  Need to remember text marks, because text iters
               *  don't survive pasting
               */
              left_end = gtk_text_buffer_create_mark (content_buffer,
                                                      NULL, iter, TRUE);
              right_start = gtk_text_buffer_create_mark (content_buffer,
                                                         NULL, iter, FALSE);

              for (list = split_tags; list; list = g_slist_next (list))
                {
                  GtkTextTag  *tag             = list->data;
                  GtkTextIter *backward_toggle = gtk_text_iter_copy (iter);
                  GtkTextIter *forward_toggle  = gtk_text_iter_copy (iter);
                  GtkTextMark *left_start      = NULL;
                  GtkTextMark *right_end       = NULL;

                  gtk_text_iter_backward_to_tag_toggle (backward_toggle, tag);
                  left_start = gtk_text_buffer_create_mark (content_buffer,
                                                            NULL,
                                                            backward_toggle,
                                                            FALSE);

                  gtk_text_iter_forward_to_tag_toggle (forward_toggle, tag);
                  right_end = gtk_text_buffer_create_mark (content_buffer,
                                                           NULL,
                                                           forward_toggle,
                                                           TRUE);

                  left_start_list = g_slist_prepend (left_start_list, left_start);
                  right_end_list = g_slist_prepend (right_end_list, right_end);

                  gtk_text_buffer_remove_tag (content_buffer, tag,
                                              backward_toggle,
                                              forward_toggle);

                  gtk_text_iter_free (forward_toggle);
                  gtk_text_iter_free (backward_toggle);
                }

              left_start_list = g_slist_reverse (left_start_list);
              right_end_list = g_slist_reverse (right_end_list);
            }

          success = function (register_buffer, content_buffer,
                              iter, data, length,
                              fmt->can_create_tags,
                              fmt->user_data,
                              error);

          if (!success && error != NULL && *error == NULL)
            g_set_error (error, 0, 0,
                         _("Unknown error when trying to deserialize %s"),
                         gdk_atom_name (format));

          if (split_tags)
            {
              GSList      *left_list;
              GSList      *right_list;
              GtkTextIter  left_e;
              GtkTextIter  right_s;

              /*  Turn the remembered marks back into iters so they
               *  can by used to re-apply the remembered tags
               */
              gtk_text_buffer_get_iter_at_mark (content_buffer,
                                                &left_e, left_end);
              gtk_text_buffer_get_iter_at_mark (content_buffer,
                                                &right_s, right_start);

              for (list = split_tags,
                     left_list = left_start_list,
                     right_list = right_end_list;
                   list && left_list && right_list;
                   list = g_slist_next (list),
                     left_list = g_slist_next (left_list),
                     right_list = g_slist_next (right_list))
                {
                  GtkTextTag  *tag        = list->data;
                  GtkTextMark *left_start = left_list->data;
                  GtkTextMark *right_end  = right_list->data;
                  GtkTextIter  left_s;
                  GtkTextIter  right_e;

                  gtk_text_buffer_get_iter_at_mark (content_buffer,
                                                    &left_s, left_start);
                  gtk_text_buffer_get_iter_at_mark (content_buffer,
                                                    &right_e, right_end);

                  gtk_text_buffer_apply_tag (content_buffer, tag,
                                             &left_s, &left_e);
                  gtk_text_buffer_apply_tag (content_buffer, tag,
                                             &right_s, &right_e);

                  gtk_text_buffer_delete_mark (content_buffer, left_start);
                  gtk_text_buffer_delete_mark (content_buffer, right_end);
                }

              gtk_text_buffer_delete_mark (content_buffer, left_end);
              gtk_text_buffer_delete_mark (content_buffer, right_start);

              g_slist_free (split_tags);
              g_slist_free (left_start_list);
              g_slist_free (right_end_list);
            }

          return success;
        }
    }

  g_set_error (error, 0, 0,
               _("No deserialize function found for format %s"),
               gdk_atom_name (format));

  return FALSE;
}


/*  private functions  */

static GList *
register_format (GList          *formats,
                 const gchar    *mime_type,
                 gpointer        function,
                 gpointer        user_data,
                 GDestroyNotify  user_data_destroy,
                 GdkAtom        *atom)
{
  GtkRichTextFormat *format;

  *atom = gdk_atom_intern (mime_type, FALSE);

  formats = unregister_format (formats, *atom);

  format = g_new0 (GtkRichTextFormat, 1);

  format->mime_type         = g_strdup (mime_type);
  format->can_create_tags   = FALSE;
  format->atom              = *atom;
  format->function          = function;
  format->user_data         = user_data;
  format->user_data_destroy = user_data_destroy;

  return g_list_append (formats, format);
}

static GList *
unregister_format (GList   *formats,
                   GdkAtom  atom)
{
  GList *list;

  for (list = formats; list; list = g_list_next (list))
    {
      GtkRichTextFormat *format = list->data;

      if (format->atom == atom)
        {
          free_format (format);

          return g_list_delete_link (formats, list);
        }
    }

  return formats;
}

static GdkAtom *
get_formats (GList *formats,
             gint  *n_formats)
{
  GdkAtom *array;
  GList   *list;
  gint     i;

  *n_formats = g_list_length (formats);
  array = g_new0 (GdkAtom, *n_formats);

  for (list = formats, i = 0; list; list = g_list_next (list), i++)
    {
      GtkRichTextFormat *format = list->data;

      array[i] = format->atom;
    }

  return array;
}

static void
free_format (GtkRichTextFormat *format)
{
  if (format->user_data_destroy)
    format->user_data_destroy (format->user_data);

  g_free (format->mime_type);
  g_free (format);
}

static void
free_format_list (GList *formats)
{
  g_list_foreach (formats, (GFunc) free_format, NULL);
  g_list_free (formats);
}

static GQuark
serialize_quark (void)
{
  static GQuark quark = 0;

  if (! quark)
    quark = g_quark_from_static_string ("gtk-text-buffer-serialize-formats");

  return quark;
}

static GQuark
deserialize_quark (void)
{
  static GQuark quark = 0;

  if (! quark)
    quark = g_quark_from_static_string ("gtk-text-buffer-deserialize-formats");

  return quark;
}

#define __GTK_TEXT_BUFFER_RICH_TEXT_C__
#include "gtkaliasdef.c"
