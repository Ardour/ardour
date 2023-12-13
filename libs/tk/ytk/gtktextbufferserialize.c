/* gtktextbufferserialize.c
 *
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2004 Nokia Corporation
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

/* FIXME: We should use other error codes for the
 * parts that deal with the format errors
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#undef GDK_PIXBUF_DISABLE_DEPRECATED
#include "gdk-pixbuf/gdk-pixdata.h"
#include "gtktextbufferserialize.h"
#include "gtkintl.h"
#include "gtkalias.h"


typedef struct
{
  GString *tag_table_str;
  GString *text_str;
  GHashTable *tags;
  GtkTextIter start, end;

  gint n_pixbufs;
  GList *pixbufs;
  gint tag_id;
  GHashTable *tag_id_tags;
} SerializationContext;

static gchar *
serialize_value (GValue *value)
{
  if (g_value_type_transformable (value->g_type, G_TYPE_STRING))
    {
      GValue text_value = { 0 };
      gchar *tmp;

      g_value_init (&text_value, G_TYPE_STRING);
      g_value_transform (value, &text_value);

      tmp = g_markup_escape_text (g_value_get_string (&text_value), -1);
      g_value_unset (&text_value);

      return tmp;
    }
  else if (value->g_type == GDK_TYPE_COLOR)
    {
      GdkColor *color = g_value_get_boxed (value);

      return g_strdup_printf ("%x:%x:%x", color->red, color->green, color->blue);
    }
  else if (g_type_is_a (value->g_type, GDK_TYPE_DRAWABLE))
    {
      /* Don't do anything */
    }
  else
    {
      g_warning ("Type %s is not serializable\n", g_type_name (value->g_type));
    }

  return NULL;
}

static gboolean
deserialize_value (const gchar *str,
                   GValue      *value)
{
  if (g_value_type_transformable (G_TYPE_STRING, value->g_type))
    {
      GValue text_value = { 0 };
      gboolean retval;

      g_value_init (&text_value, G_TYPE_STRING);
      g_value_set_static_string (&text_value, str);

      retval = g_value_transform (&text_value, value);
      g_value_unset (&text_value);

      return retval;
    }
  else if (value->g_type == G_TYPE_BOOLEAN)
    {
      gboolean v;

      v = strcmp (str, "TRUE") == 0;

      g_value_set_boolean (value, v);

      return TRUE;
    }
  else if (value->g_type == G_TYPE_INT)
    {
      gchar *tmp;
      int v;

      v = strtol (str, &tmp, 10);

      if (tmp == NULL || tmp == str)
	return FALSE;

      g_value_set_int (value, v);

      return TRUE;
    }
  else if (value->g_type == G_TYPE_DOUBLE)
    {
      gchar *tmp;
      gdouble v;

      v = g_ascii_strtod (str, &tmp);

      if (tmp == NULL || tmp == str)
	return FALSE;

      g_value_set_double (value, v);

      return TRUE;
    }
  else if (value->g_type == GDK_TYPE_COLOR)
    {
      GdkColor color;
      const gchar *old;
      gchar *tmp;

      old = str;
      color.red = strtol (old, &tmp, 16);

      if (tmp == NULL || tmp == old)
	return FALSE;

      old = tmp;
      if (*old++ != ':')
	return FALSE;

      color.green = strtol (old, &tmp, 16);
      if (tmp == NULL || tmp == old)
	return FALSE;

      old = tmp;
      if (*old++ != ':')
	return FALSE;

      color.blue = strtol (old, &tmp, 16);

      if (tmp == NULL || tmp == old || *tmp != '\0')
	return FALSE;

      g_value_set_boxed (value, &color);

      return TRUE;
    }
  else if (G_VALUE_HOLDS_ENUM (value))
    {
      GEnumClass *class = G_ENUM_CLASS (g_type_class_peek (value->g_type));
      GEnumValue *enum_value;

      enum_value = g_enum_get_value_by_name (class, str);

      if (enum_value)
	{
	  g_value_set_enum (value, enum_value->value);
	  return TRUE;
	}

      return FALSE;
    }
  else
    {
      g_warning ("Type %s can not be deserialized\n", g_type_name (value->g_type));
    }

  return FALSE;
}

/* Checks if a param is set, or if it's the default value */
static gboolean
is_param_set (GObject    *object,
              GParamSpec *pspec,
              GValue     *value)
{
  /* We need to special case some attributes here */
  if (strcmp (pspec->name, "background-gdk") == 0)
    {
      gboolean is_set;

      g_object_get (object, "background-set", &is_set, NULL);

      if (is_set)
	{
	  g_value_init (value, G_PARAM_SPEC_VALUE_TYPE (pspec));

	  g_object_get_property (object, pspec->name, value);

	  return TRUE;
	}

      return FALSE;
    }
  else if (strcmp (pspec->name, "foreground-gdk") == 0)
    {
      gboolean is_set;

      g_object_get (object, "foreground-set", &is_set, NULL);

      if (is_set)
	{
	  g_value_init (value, G_PARAM_SPEC_VALUE_TYPE (pspec));

	  g_object_get_property (object, pspec->name, value);

	  return TRUE;
	}

      return FALSE;
    }
  else
    {
      gboolean is_set;
      gchar *is_set_name;

      is_set_name = g_strdup_printf ("%s-set", pspec->name);

      if (g_object_class_find_property (G_OBJECT_GET_CLASS (object), is_set_name) == NULL)
	{
	  g_free (is_set_name);
	  return FALSE;
	}
      else
	{
	  g_object_get (object, is_set_name, &is_set, NULL);

	  if (!is_set)
	    {
	      g_free (is_set_name);
	      return FALSE;
	    }

	  g_free (is_set_name);

	  g_value_init (value, G_PARAM_SPEC_VALUE_TYPE (pspec));

	  g_object_get_property (object, pspec->name, value);

	  if (g_param_value_defaults (pspec, value))
	    {
	      g_value_unset (value);

	      return FALSE;
	    }
	}
      return TRUE;
    }
}

static void
serialize_tag (gpointer key,
               gpointer data,
               gpointer user_data)
{
  SerializationContext *context = user_data;
  GtkTextTag *tag = data;
  gchar *tag_name;
  gint tag_id;
  GParamSpec **pspecs;
  guint n_pspecs;
  int i;

  g_string_append (context->tag_table_str, "  <tag ");

  /* Handle anonymous tags */
  if (tag->name)
    {
      tag_name = g_markup_escape_text (tag->name, -1);
      g_string_append_printf (context->tag_table_str, "name=\"%s\"", tag_name);
      g_free (tag_name);
    }
  else
    {
      tag_id = GPOINTER_TO_INT (g_hash_table_lookup (context->tag_id_tags, tag));

      g_string_append_printf (context->tag_table_str, "id=\"%d\"", tag_id);
    }

  g_string_append_printf (context->tag_table_str, " priority=\"%d\">\n", tag->priority);

  /* Serialize properties */
  pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (tag), &n_pspecs);

  for (i = 0; i < n_pspecs; i++)
    {
      GValue value = { 0 };
      gchar *tmp, *tmp2;

      if (!(pspecs[i]->flags & G_PARAM_READABLE) ||
	  !(pspecs[i]->flags & G_PARAM_WRITABLE))
	continue;

      if (!is_param_set (G_OBJECT (tag), pspecs[i], &value))
	continue;

      /* Now serialize the attr */
      tmp2 = serialize_value (&value);

      if (tmp2)
	{
	  tmp = g_markup_escape_text (pspecs[i]->name, -1);
	  g_string_append_printf (context->tag_table_str, "   <attr name=\"%s\" ", tmp);
	  g_free (tmp);

	  tmp = g_markup_escape_text (g_type_name (pspecs[i]->value_type), -1);
	  g_string_append_printf (context->tag_table_str, "type=\"%s\" value=\"%s\" />\n", tmp, tmp2);

	  g_free (tmp);
	  g_free (tmp2);
	}

      g_value_unset (&value);
    }

  g_free (pspecs);

  g_string_append (context->tag_table_str, "  </tag>\n");
}

static void
serialize_tags (SerializationContext *context)
{
  g_string_append (context->tag_table_str, " <text_view_markup>\n");
  g_string_append (context->tag_table_str, " <tags>\n");
  g_hash_table_foreach (context->tags, serialize_tag, context);
  g_string_append (context->tag_table_str, " </tags>\n");
}

#if 0
static void
dump_tag_list (const gchar *str,
               GList       *list)
{
  g_print ("%s: ", str);

  if (!list)
    g_print ("(empty)");
  else
    {
      while (list)
	{
	  g_print ("%s ", ((GtkTextTag *)list->data)->name);
	  list = list->next;
	}
    }

  g_print ("\n");
}
#endif

static void
find_list_delta (GSList  *old_list,
                 GSList  *new_list,
		 GList  **added,
                 GList  **removed)
{
  GSList *tmp;
  GList *tmp_added, *tmp_removed;

  tmp_added = NULL;
  tmp_removed = NULL;

  /* Find added tags */
  tmp = new_list;
  while (tmp)
    {
      if (!g_slist_find (old_list, tmp->data))
	tmp_added = g_list_prepend (tmp_added, tmp->data);

      tmp = tmp->next;
    }

  *added = tmp_added;

  /* Find removed tags */
  tmp = old_list;
  while (tmp)
    {
      if (!g_slist_find (new_list, tmp->data))
	tmp_removed = g_list_prepend (tmp_removed, tmp->data);

      tmp = tmp->next;
    }

  /* We reverse the list here to match the xml semantics */
  *removed = g_list_reverse (tmp_removed);
}

static void
serialize_section_header (GString     *str,
			  const gchar *name,
			  gint         length)
{
  g_return_if_fail (strlen (name) == 26);

  g_string_append (str, name);

  g_string_append_c (str, length >> 24);

  g_string_append_c (str, (length >> 16) & 0xff);
  g_string_append_c (str, (length >> 8) & 0xff);
  g_string_append_c (str, length & 0xff);
}

static void
serialize_text (GtkTextBuffer        *buffer,
                SerializationContext *context)
{
  GtkTextIter iter, old_iter;
  GSList *tag_list, *new_tag_list;
  GSList *active_tags;

  g_string_append (context->text_str, "<text>");

  iter = context->start;
  tag_list = NULL;
  active_tags = NULL;

  do
    {
      GList *added, *removed;
      GList *tmp;
      gchar *tmp_text, *escaped_text;

      new_tag_list = gtk_text_iter_get_tags (&iter);
      find_list_delta (tag_list, new_tag_list, &added, &removed);

      /* Handle removed tags */
      for (tmp = removed; tmp; tmp = tmp->next)
	{
	  GtkTextTag *tag = tmp->data;

          /* Only close the tag if we didn't close it before (by using
           * the stack logic in the while() loop below)
           */
          if (g_slist_find (active_tags, tag))
            {
              g_string_append (context->text_str, "</apply_tag>");

              /* Drop all tags that were opened after this one (which are
               * above this on in the stack)
               */
              while (active_tags->data != tag)
                {
                  added = g_list_prepend (added, active_tags->data);
                  active_tags = g_slist_remove (active_tags, active_tags->data);
                  g_string_append_printf (context->text_str, "</apply_tag>");
                }

              active_tags = g_slist_remove (active_tags, active_tags->data);
            }
	}

      /* Handle added tags */
      for (tmp = added; tmp; tmp = tmp->next)
	{
	  GtkTextTag *tag = tmp->data;
	  gchar *tag_name;

	  /* Add it to the tag hash table */
	  g_hash_table_insert (context->tags, tag, tag);

	  if (tag->name)
	    {
	      tag_name = g_markup_escape_text (tag->name, -1);

	      g_string_append_printf (context->text_str, "<apply_tag name=\"%s\">", tag_name);
	      g_free (tag_name);
	    }
	  else
	    {
	      gpointer tag_id;

	      /* We've got an anonymous tag, find out if it's been
		 used before */
	      if (!g_hash_table_lookup_extended (context->tag_id_tags, tag, NULL, &tag_id))
		{
		  tag_id = GINT_TO_POINTER (context->tag_id++);

		  g_hash_table_insert (context->tag_id_tags, tag, tag_id);
		}

	      g_string_append_printf (context->text_str, "<apply_tag id=\"%d\">", GPOINTER_TO_INT (tag_id));
	    }

	  active_tags = g_slist_prepend (active_tags, tag);
	}

      g_slist_free (tag_list);
      tag_list = new_tag_list;

      g_list_free (added);
      g_list_free (removed);

      old_iter = iter;

      /* Now try to go to either the next tag toggle, or if a pixbuf appears */
      while (TRUE)
	{
	  gunichar ch = gtk_text_iter_get_char (&iter);

	  if (ch == 0xFFFC)
	    {
	      GdkPixbuf *pixbuf = gtk_text_iter_get_pixbuf (&iter);

	      if (pixbuf)
		{
		  /* Append the text before the pixbuf */
		  tmp_text = gtk_text_iter_get_slice (&old_iter, &iter);
		  escaped_text = g_markup_escape_text (tmp_text, -1);
		  g_free (tmp_text);

		  /* Forward so we don't get the 0xfffc char */
		  gtk_text_iter_forward_char (&iter);
		  old_iter = iter;

		  g_string_append (context->text_str, escaped_text);
		  g_free (escaped_text);

		  g_string_append_printf (context->text_str, "<pixbuf index=\"%d\" />", context->n_pixbufs);

		  context->n_pixbufs++;
		  context->pixbufs = g_list_prepend (context->pixbufs, pixbuf);
		}
	    }
          else if (ch == 0)
            {
                break;
            }
	  else
	    gtk_text_iter_forward_char (&iter);

	  if (gtk_text_iter_toggles_tag (&iter, NULL))
	    break;
	}

      /* We might have moved too far */
      if (gtk_text_iter_compare (&iter, &context->end) > 0)
	iter = context->end;

      /* Append the text */
      tmp_text = gtk_text_iter_get_slice (&old_iter, &iter);
      escaped_text = g_markup_escape_text (tmp_text, -1);
      g_free (tmp_text);

      g_string_append (context->text_str, escaped_text);
      g_free (escaped_text);
    }
  while (!gtk_text_iter_equal (&iter, &context->end));

  /* Close any open tags */
  for (tag_list = active_tags; tag_list; tag_list = tag_list->next)
    g_string_append (context->text_str, "</apply_tag>");

  g_slist_free (active_tags);
  g_string_append (context->text_str, "</text>\n</text_view_markup>\n");
}

static void
serialize_pixbufs (SerializationContext *context,
		   GString              *text)
{
  GList *list;

  for (list = context->pixbufs; list != NULL; list = list->next)
    {
      GdkPixbuf *pixbuf = list->data;
      GdkPixdata pixdata;
      guint8 *tmp;
      guint len;

      gdk_pixdata_from_pixbuf (&pixdata, pixbuf, FALSE);
      tmp = gdk_pixdata_serialize (&pixdata, &len);

      serialize_section_header (text, "GTKTEXTBUFFERPIXBDATA-0001", len);
      g_string_append_len (text, (gchar *) tmp, len);
      g_free (tmp);
    }
}

guint8 *
_gtk_text_buffer_serialize_rich_text (GtkTextBuffer     *register_buffer,
                                      GtkTextBuffer     *content_buffer,
                                      const GtkTextIter *start,
                                      const GtkTextIter *end,
                                      gsize             *length,
                                      gpointer           user_data)
{
  SerializationContext context;
  GString *text;

  context.tags = g_hash_table_new (NULL, NULL);
  context.text_str = g_string_new (NULL);
  context.tag_table_str = g_string_new (NULL);
  context.start = *start;
  context.end = *end;
  context.n_pixbufs = 0;
  context.pixbufs = NULL;
  context.tag_id = 0;
  context.tag_id_tags = g_hash_table_new (NULL, NULL);

  /* We need to serialize the text before the tag table so we know
     what tags are used */
  serialize_text (content_buffer, &context);
  serialize_tags (&context);

  text = g_string_new (NULL);
  serialize_section_header (text, "GTKTEXTBUFFERCONTENTS-0001",
                            context.tag_table_str->len + context.text_str->len);

  g_string_append_len (text, context.tag_table_str->str, context.tag_table_str->len);
  g_string_append_len (text, context.text_str->str, context.text_str->len);

  context.pixbufs = g_list_reverse (context.pixbufs);
  serialize_pixbufs (&context, text);

  g_hash_table_destroy (context.tags);
  g_list_free (context.pixbufs);
  g_string_free (context.text_str, TRUE);
  g_string_free (context.tag_table_str, TRUE);
  g_hash_table_destroy (context.tag_id_tags);

  *length = text->len;

  return (guint8 *) g_string_free (text, FALSE);
}

typedef enum
{
  STATE_START,
  STATE_TEXT_VIEW_MARKUP,
  STATE_TAGS,
  STATE_TAG,
  STATE_ATTR,
  STATE_TEXT,
  STATE_APPLY_TAG,
  STATE_PIXBUF
} ParseState;

typedef struct
{
  gchar *text;
  GdkPixbuf *pixbuf;
  GSList *tags;
} TextSpan;

typedef struct
{
  GtkTextTag *tag;
  gint prio;
} TextTagPrio;

typedef struct
{
  GSList *states;

  GList *headers;

  GtkTextBuffer *buffer;

  /* Tags that are defined in <tag> elements */
  GHashTable *defined_tags;

  /* Tags that are anonymous */
  GHashTable *anonymous_tags;

  /* Tag name substitutions */
  GHashTable *substitutions;

  /* Current tag */
  GtkTextTag *current_tag;

  /* Priority of current tag */
  gint current_tag_prio;

  /* Id of current tag */
  gint current_tag_id;

  /* Tags and their priorities */
  GList *tag_priorities;

  GSList *tag_stack;

  GList *spans;

  gboolean create_tags;

  gboolean parsed_text;
  gboolean parsed_tags;
} ParseInfo;

static void
set_error (GError              **err,
           GMarkupParseContext  *context,
           int                   error_domain,
           int                   error_code,
           const char           *format,
           ...)
{
  int line, ch;
  va_list args;
  char *str;

  g_markup_parse_context_get_position (context, &line, &ch);

  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);

  g_set_error (err, error_domain, error_code,
               ("Line %d character %d: %s"),
               line, ch, str);

  g_free (str);
}

static void
push_state (ParseInfo  *info,
            ParseState  state)
{
  info->states = g_slist_prepend (info->states, GINT_TO_POINTER (state));
}

static void
pop_state (ParseInfo *info)
{
  g_return_if_fail (info->states != NULL);

  info->states = g_slist_remove (info->states, info->states->data);
}

static ParseState
peek_state (ParseInfo *info)
{
  g_return_val_if_fail (info->states != NULL, STATE_START);

  return GPOINTER_TO_INT (info->states->data);
}

#define ELEMENT_IS(name) (strcmp (element_name, (name)) == 0)


static gboolean
check_id_or_name (GMarkupParseContext  *context,
		  const gchar          *element_name,
		  const gchar         **attribute_names,
		  const gchar         **attribute_values,
		  gint                 *id,
		  const gchar         **name,
		  GError              **error)
{
  gboolean has_id = FALSE;
  gboolean has_name = FALSE;
  int i;

  *id = 0;
  *name = NULL;

  for (i = 0; attribute_names[i] != NULL; i++)
    {
      if (strcmp (attribute_names[i], "name") == 0)
	{
	  *name = attribute_values[i];

	  if (has_id)
	    {
	      set_error (error, context,
			 G_MARKUP_ERROR,
			 G_MARKUP_ERROR_PARSE,
			 _("Both \"id\" and \"name\" were found on the <%s> element"),
			 element_name);
	      return FALSE;
	    }

	  if (has_name)
	    {
	      set_error (error, context,
			 G_MARKUP_ERROR,
			 G_MARKUP_ERROR_PARSE,
			 _("The attribute \"%s\" was found twice on the <%s> element"),
			 "name", element_name);
	      return FALSE;
	    }

	  has_name = TRUE;
	}
      else if (strcmp (attribute_names[i], "id") == 0)
	{
	  gchar *tmp;

	  if (has_name)
	    {
	      set_error (error, context,
			 G_MARKUP_ERROR,
			 G_MARKUP_ERROR_PARSE,
			 _("Both \"id\" and \"name\" were found on the <%s> element"),
			 element_name);
	      return FALSE;
	    }

	  if (has_id)
	    {
	      set_error (error, context,
			 G_MARKUP_ERROR,
			 G_MARKUP_ERROR_PARSE,
			 _("The attribute \"%s\" was found twice on the <%s> element"),
			 "id", element_name);
	      return FALSE;
	    }

	  has_id = TRUE;

	  /* Try parsing the integer */
	  *id = strtol (attribute_values[i], &tmp, 10);

	  if (tmp == NULL || tmp == attribute_values[i])
	    {
	      set_error (error, context,
			 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
			 _("<%s> element has invalid ID \"%s\""), attribute_values[i]);
	      return FALSE;
	    }
	}
    }

  if (!has_id && !has_name)
    {
      set_error (error, context,
		 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
		 _("<%s> element has neither a \"name\" nor an \"id\" attribute"), element_name);
      return FALSE;
    }

  return TRUE;
}

typedef struct
{
  const char  *name;
  const char **retloc;
} LocateAttr;

static gboolean
locate_attributes (GMarkupParseContext  *context,
                   const char           *element_name,
                   const char          **attribute_names,
                   const char          **attribute_values,
		   gboolean              allow_unknown_attrs,
                   GError              **error,
                   const char           *first_attribute_name,
                   const char          **first_attribute_retloc,
                   ...)
{
  va_list args;
  const char *name;
  const char **retloc;
  int n_attrs;
#define MAX_ATTRS 24
  LocateAttr attrs[MAX_ATTRS];
  gboolean retval;
  int i;

  g_return_val_if_fail (first_attribute_name != NULL, FALSE);
  g_return_val_if_fail (first_attribute_retloc != NULL, FALSE);

  retval = TRUE;

  n_attrs = 1;
  attrs[0].name = first_attribute_name;
  attrs[0].retloc = first_attribute_retloc;
  *first_attribute_retloc = NULL;

  va_start (args, first_attribute_retloc);

  name = va_arg (args, const char*);
  retloc = va_arg (args, const char**);

  while (name != NULL)
    {
      g_return_val_if_fail (retloc != NULL, FALSE);

      g_assert (n_attrs < MAX_ATTRS);

      attrs[n_attrs].name = name;
      attrs[n_attrs].retloc = retloc;
      n_attrs += 1;
      *retloc = NULL;

      name = va_arg (args, const char*);
      retloc = va_arg (args, const char**);
    }

  va_end (args);

  if (!retval)
    return retval;

  i = 0;
  while (attribute_names[i])
    {
      int j;
      gboolean found;

      found = FALSE;
      j = 0;
      while (j < n_attrs)
        {
          if (strcmp (attrs[j].name, attribute_names[i]) == 0)
            {
              retloc = attrs[j].retloc;

              if (*retloc != NULL)
                {
                  set_error (error, context,
                             G_MARKUP_ERROR,
                             G_MARKUP_ERROR_PARSE,
                             _("Attribute \"%s\" repeated twice on the same <%s> element"),
                             attrs[j].name, element_name);
                  retval = FALSE;
                  goto out;
                }

              *retloc = attribute_values[i];
              found = TRUE;
            }

          ++j;
        }

      if (!found && !allow_unknown_attrs)
        {
          set_error (error, context,
                     G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Attribute \"%s\" is invalid on <%s> element in this context"),
                     attribute_names[i], element_name);
          retval = FALSE;
          goto out;
        }

      ++i;
    }

 out:
  return retval;
}

static gboolean
check_no_attributes (GMarkupParseContext  *context,
                     const char           *element_name,
                     const char          **attribute_names,
                     const char          **attribute_values,
                     GError              **error)
{
  if (attribute_names[0] != NULL)
    {
      set_error (error, context,
                 G_MARKUP_ERROR,
                 G_MARKUP_ERROR_PARSE,
                 _("Attribute \"%s\" is invalid on <%s> element in this context"),
                 attribute_names[0], element_name);
      return FALSE;
    }

  return TRUE;
}

static GtkTextTag *
tag_exists (GMarkupParseContext *context,
	    const gchar         *name,
	    gint                 id,
	    ParseInfo           *info,
	    GError             **error)
{
  const gchar *real_name;

  if (info->create_tags)
    {
      /* If we have an anonymous tag, just return it directly */
      if (!name)
	return g_hash_table_lookup (info->anonymous_tags,
				    GINT_TO_POINTER (id));

      /* First, try the substitutions */
      real_name = g_hash_table_lookup (info->substitutions, name);

      if (real_name)
	return gtk_text_tag_table_lookup (info->buffer->tag_table, real_name);

      /* Next, try the list of defined tags */
      if (g_hash_table_lookup (info->defined_tags, name) != NULL)
	return gtk_text_tag_table_lookup (info->buffer->tag_table, name);

      set_error (error, context,
		 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
		 _("Tag \"%s\" has not been defined."), name);

      return NULL;
    }
  else
    {
      GtkTextTag *tag;

      if (!name)
	{
	  set_error (error, context,
		     G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
		     _("Anonymous tag found and tags can not be created."));
	  return NULL;
	}

      tag = gtk_text_tag_table_lookup (info->buffer->tag_table, name);

      if (tag)
	return tag;

      set_error (error, context,
		 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
		 _("Tag \"%s\" does not exist in buffer and tags can not be created."), name);

      return NULL;
    }
}

typedef struct
{
  const gchar *id;
  gint length;
  const gchar *start;
} Header;

static GdkPixbuf *
get_pixbuf_from_headers (GList   *headers,
                         int      id,
                         GError **error)
{
  Header *header;
  GdkPixdata pixdata;
  GdkPixbuf *pixbuf;

  header = g_list_nth_data (headers, id);

  if (!header)
    return NULL;

  if (!gdk_pixdata_deserialize (&pixdata, header->length,
                                (const guint8 *) header->start, error))
    return NULL;

  pixbuf = gdk_pixbuf_from_pixdata (&pixdata, TRUE, error);

  return pixbuf;
}

static void
parse_apply_tag_element (GMarkupParseContext  *context,
			 const gchar          *element_name,
			 const gchar         **attribute_names,
			 const gchar         **attribute_values,
			 ParseInfo            *info,
			 GError              **error)
{
  const gchar *name, *priority;
  gint id;
  GtkTextTag *tag;

  g_assert (peek_state (info) == STATE_TEXT ||
	    peek_state (info) == STATE_APPLY_TAG);

  if (ELEMENT_IS ("apply_tag"))
    {
      if (!locate_attributes (context, element_name, attribute_names, attribute_values, TRUE, error,
			      "priority", &priority, NULL))
	return;

      if (!check_id_or_name (context, element_name, attribute_names, attribute_values,
			     &id, &name, error))
	return;


      tag = tag_exists (context, name, id, info, error);

      if (!tag)
	return;

      info->tag_stack = g_slist_prepend (info->tag_stack, tag);

      push_state (info, STATE_APPLY_TAG);
    }
  else if (ELEMENT_IS ("pixbuf"))
    {
      int int_id;
      GdkPixbuf *pixbuf;
      TextSpan *span;
      const gchar *pixbuf_id;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values, FALSE, error,
			      "index", &pixbuf_id, NULL))
	return;

      int_id = atoi (pixbuf_id);
      pixbuf = get_pixbuf_from_headers (info->headers, int_id, error);

      span = g_new0 (TextSpan, 1);
      span->pixbuf = pixbuf;
      span->tags = NULL;

      info->spans = g_list_prepend (info->spans, span);

      if (!pixbuf)
	return;

      push_state (info, STATE_PIXBUF);
    }
  else
    set_error (error, context,
	       G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
	       _("Element <%s> is not allowed below <%s>"),
	       element_name, peek_state(info) == STATE_TEXT ? "text" : "apply_tag");
}

static void
parse_attr_element (GMarkupParseContext  *context,
		    const gchar          *element_name,
		    const gchar         **attribute_names,
		    const gchar         **attribute_values,
		    ParseInfo            *info,
		    GError              **error)
{
  const gchar *name, *type, *value;
  GType gtype;
  GValue gvalue = { 0 };
  GParamSpec *pspec;

  g_assert (peek_state (info) == STATE_TAG);

  if (ELEMENT_IS ("attr"))
    {
      if (!locate_attributes (context, element_name, attribute_names, attribute_values, FALSE, error,
			      "name", &name, "type", &type, "value", &value, NULL))
	return;

      gtype = g_type_from_name (type);

      if (gtype == G_TYPE_INVALID)
	{
	  set_error (error, context,
		     G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
		     _("\"%s\" is not a valid attribute type"), type);
	  return;
	}

      if (!(pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (info->current_tag), name)))
	{
	  set_error (error, context,
		     G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
		     _("\"%s\" is not a valid attribute name"), name);
	  return;
	}

      g_value_init (&gvalue, gtype);

      if (!deserialize_value (value, &gvalue))
	{
	  set_error (error, context,
		     G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
		     _("\"%s\" could not be converted to a value of type \"%s\" for attribute \"%s\""),
		     value, type, name);
	  return;
	}

      if (g_param_value_validate (pspec, &gvalue))
	{
	  set_error (error, context,
		     G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
		     _("\"%s\" is not a valid value for attribute \"%s\""),
		     value, name);
	  g_value_unset (&gvalue);
	  return;
	}

      g_object_set_property (G_OBJECT (info->current_tag),
			     name, &gvalue);

      g_value_unset (&gvalue);

      push_state (info, STATE_ATTR);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "tag");
    }
}


static gchar *
get_tag_name (ParseInfo   *info,
	      const gchar *tag_name)
{
  gchar *name;
  gint i;

  name = g_strdup (tag_name);

  if (!info->create_tags)
    return name;

  i = 0;

  while (gtk_text_tag_table_lookup (info->buffer->tag_table, name) != NULL)
    {
      g_free (name);
      name = g_strdup_printf ("%s-%d", tag_name, ++i);
    }

  if (i != 0)
    {
      g_hash_table_insert (info->substitutions, g_strdup (tag_name), g_strdup (name));
    }

  return name;
}

static void
parse_tag_element (GMarkupParseContext  *context,
		   const gchar          *element_name,
		   const gchar         **attribute_names,
		   const gchar         **attribute_values,
		   ParseInfo            *info,
		   GError              **error)
{
  const gchar *name, *priority;
  gchar *tag_name;
  gint id;
  gint prio;
  gchar *tmp;

  g_assert (peek_state (info) == STATE_TAGS);

  if (ELEMENT_IS ("tag"))
    {
      if (!locate_attributes (context, element_name, attribute_names, attribute_values, TRUE, error,
			      "priority", &priority, NULL))
	return;

      if (!check_id_or_name (context, element_name, attribute_names, attribute_values,
			     &id, &name, error))
	return;

      if (name)
	{
	  if (g_hash_table_lookup (info->defined_tags, name) != NULL)
	    {
	      set_error (error, context,
			 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
			 _("Tag \"%s\" already defined"), name);
	      return;
	    }
	}

      prio = strtol (priority, &tmp, 10);

      if (tmp == NULL || tmp == priority)
	{
	  set_error (error, context,
		     G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
		     _("Tag \"%s\" has invalid priority \"%s\""), name, priority);
	  return;
	}

      if (name)
	{
	  tag_name = get_tag_name (info, name);
	  info->current_tag = gtk_text_tag_new (tag_name);
	  g_free (tag_name);
	}
      else
	{
	  info->current_tag = gtk_text_tag_new (NULL);
	  info->current_tag_id = id;
	}

      info->current_tag_prio = prio;

      push_state (info, STATE_TAG);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "tags");
    }
}

static void
start_element_handler (GMarkupParseContext  *context,
		       const gchar          *element_name,
		       const gchar         **attribute_names,
		       const gchar         **attribute_values,
		       gpointer              user_data,
		       GError              **error)
{
  ParseInfo *info = user_data;

  switch (peek_state (info))
    {
    case STATE_START:
      if (ELEMENT_IS ("text_view_markup"))
	{
	  if (!check_no_attributes (context, element_name,
				    attribute_names, attribute_values, error))
	    return;

	  push_state (info, STATE_TEXT_VIEW_MARKUP);
	  break;
	}
      else
        set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                   _("Outermost element in text must be <text_view_markup> not <%s>"),
                   element_name);
      break;
    case STATE_TEXT_VIEW_MARKUP:
      if (ELEMENT_IS ("tags"))
	{
	  if (info->parsed_tags)
	    {
	      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
			 _("A <%s> element has already been specified"), "tags");
	      return;
	    }

	  if (!check_no_attributes (context, element_name,
				    attribute_names, attribute_values, error))
	    return;

	  push_state (info, STATE_TAGS);
	  break;
	}
      else if (ELEMENT_IS ("text"))
	{
	  if (info->parsed_text)
	    {
	      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
			 _("A <%s> element has already been specified"), "text");
	      return;
	    }
	  else if (!info->parsed_tags)
	    {
	      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
			 _("A <text> element can't occur before a <tags> element"));
	      return;
	    }

	  if (!check_no_attributes (context, element_name,
				    attribute_names, attribute_values, error))
	    return;

	  push_state (info, STATE_TEXT);
	  break;
	}
      else
	set_error (error, context,
		   G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
		   element_name, "text_view_markup");
      break;
    case STATE_TAGS:
      parse_tag_element (context, element_name,
			 attribute_names, attribute_values,
			 info, error);
      break;
    case STATE_TAG:
      parse_attr_element (context, element_name,
			  attribute_names, attribute_values,
			  info, error);
      break;
    case STATE_TEXT:
    case STATE_APPLY_TAG:
      parse_apply_tag_element (context, element_name,
			       attribute_names, attribute_values,
			       info, error);
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static gint
sort_tag_prio (TextTagPrio *a,
	       TextTagPrio *b)
{
  if (a->prio < b->prio)
    return -1;
  else if (a->prio > b->prio)
    return 1;
  else
    return 0;
}

static void
end_element_handler (GMarkupParseContext  *context,
		     const gchar          *element_name,
		     gpointer              user_data,
		     GError              **error)
{
  ParseInfo *info = user_data;
  gchar *tmp;
  GList *list;

  switch (peek_state (info))
    {
    case STATE_TAGS:
      pop_state (info);
      g_assert (peek_state (info) == STATE_TEXT_VIEW_MARKUP);

      info->parsed_tags = TRUE;

      /* Sort list and add the tags */
      info->tag_priorities = g_list_sort (info->tag_priorities,
					  (GCompareFunc)sort_tag_prio);
      list = info->tag_priorities;
      while (list)
	{
	  TextTagPrio *prio = list->data;

	  if (info->create_tags)
	    gtk_text_tag_table_add (info->buffer->tag_table, prio->tag);

	  g_object_unref (prio->tag);
	  prio->tag = NULL;

	  list = list->next;
	}

      break;
    case STATE_TAG:
      pop_state (info);
      g_assert (peek_state (info) == STATE_TAGS);

      if (info->current_tag->name)
	{
	  /* Add tag to defined tags hash */
	  tmp = g_strdup (info->current_tag->name);
	  g_hash_table_insert (info->defined_tags,
			       tmp, tmp);
	}
      else
	{
	  g_hash_table_insert (info->anonymous_tags,
			       GINT_TO_POINTER (info->current_tag_id),
			       info->current_tag);
	}

      if (info->create_tags)
	{
	  TextTagPrio *prio;

	  /* add the tag to the list */
	  prio = g_new0 (TextTagPrio, 1);
	  prio->prio = info->current_tag_prio;
	  prio->tag = info->current_tag;

 	  info->tag_priorities = g_list_prepend (info->tag_priorities, prio);
	}

      info->current_tag = NULL;
      break;
    case STATE_ATTR:
      pop_state (info);
      g_assert (peek_state (info) == STATE_TAG);
      break;
    case STATE_APPLY_TAG:
      pop_state (info);
      g_assert (peek_state (info) == STATE_APPLY_TAG ||
		peek_state (info) == STATE_TEXT);

      /* Pop tag */
      info->tag_stack = g_slist_delete_link (info->tag_stack,
					     info->tag_stack);

      break;
    case STATE_TEXT:
      pop_state (info);
      g_assert (peek_state (info) == STATE_TEXT_VIEW_MARKUP);

      info->spans = g_list_reverse (info->spans);
      info->parsed_text = TRUE;
      break;
    case STATE_TEXT_VIEW_MARKUP:
      pop_state (info);
      g_assert (peek_state (info) == STATE_START);
      break;
    case STATE_PIXBUF:
      pop_state (info);
      g_assert (peek_state (info) == STATE_APPLY_TAG ||
		peek_state (info) == STATE_TEXT);
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static gboolean
all_whitespace (const char *text,
                int         text_len)
{
  const char *p;
  const char *end;

  p = text;
  end = text + text_len;

  while (p != end)
    {
      if (!g_ascii_isspace (*p))
        return FALSE;

      p = g_utf8_next_char (p);
    }

  return TRUE;
}

static void
text_handler (GMarkupParseContext  *context,
	      const gchar          *text,
	      gsize                 text_len,
	      gpointer              user_data,
	      GError              **error)
{
  ParseInfo *info = user_data;
  TextSpan *span;

  if (all_whitespace (text, text_len) &&
      peek_state (info) != STATE_TEXT &&
      peek_state (info) != STATE_APPLY_TAG)
    return;

  switch (peek_state (info))
    {
    case STATE_START:
      g_assert_not_reached (); /* gmarkup shouldn't do this */
      break;
    case STATE_TEXT:
    case STATE_APPLY_TAG:
      if (text_len == 0)
	return;

      span = g_new0 (TextSpan, 1);
      span->text = g_strndup (text, text_len);
      span->tags = g_slist_copy (info->tag_stack);

      info->spans = g_list_prepend (info->spans, span);
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
parse_info_init (ParseInfo     *info,
		 GtkTextBuffer *buffer,
		 gboolean       create_tags,
		 GList         *headers)
{
  info->states = g_slist_prepend (NULL, GINT_TO_POINTER (STATE_START));

  info->create_tags = create_tags;
  info->headers = headers;
  info->defined_tags = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  info->substitutions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  info->anonymous_tags = g_hash_table_new_full (NULL, NULL, NULL, NULL);
  info->tag_stack = NULL;
  info->spans = NULL;
  info->parsed_text = FALSE;
  info->parsed_tags = FALSE;
  info->current_tag = NULL;
  info->current_tag_prio = -1;
  info->tag_priorities = NULL;

  info->buffer = buffer;
}

static void
text_span_free (TextSpan *span)
{
  g_free (span->text);
  g_slist_free (span->tags);
  g_free (span);
}

static void
parse_info_free (ParseInfo *info)
{
  GList *list;

  g_slist_free (info->tag_stack);
  g_slist_free (info->states);

  g_hash_table_destroy (info->substitutions);
  g_hash_table_destroy (info->defined_tags);

  if (info->current_tag)
    g_object_unref (info->current_tag);

  list = info->spans;
  while (list)
    {
      text_span_free (list->data);

      list = list->next;
    }
  g_list_free (info->spans);

  list = info->tag_priorities;
  while (list)
    {
      TextTagPrio *prio = list->data;

      if (prio->tag)
	g_object_unref (prio->tag);
      g_free (prio);

      list = list->next;
    }
  g_list_free (info->tag_priorities);

}

static void
insert_text (ParseInfo   *info,
	     GtkTextIter *iter)
{
  GtkTextIter start_iter;
  GtkTextMark *mark;
  GList *tmp;
  GSList *tags;

  start_iter = *iter;

  mark = gtk_text_buffer_create_mark (info->buffer, "deserialize_insert_point",
  				      &start_iter, TRUE);

  tmp = info->spans;
  while (tmp)
    {
      TextSpan *span = tmp->data;

      if (span->text)
	gtk_text_buffer_insert (info->buffer, iter, span->text, -1);
      else
	{
	  gtk_text_buffer_insert_pixbuf (info->buffer, iter, span->pixbuf);
	  g_object_unref (span->pixbuf);
	}
      gtk_text_buffer_get_iter_at_mark (info->buffer, &start_iter, mark);

      /* Apply tags */
      tags = span->tags;
      while (tags)
	{
	  GtkTextTag *tag = tags->data;

	  gtk_text_buffer_apply_tag (info->buffer, tag,
				     &start_iter, iter);

	  tags = tags->next;
	}

      gtk_text_buffer_move_mark (info->buffer, mark, iter);

      tmp = tmp->next;
    }

  gtk_text_buffer_delete_mark (info->buffer, mark);
}



static int
read_int (const guchar *start)
{
  int result;

  result =
    start[0] << 24 |
    start[1] << 16 |
    start[2] << 8 |
    start[3];

  return result;
}

static gboolean
header_is (Header      *header,
           const gchar *id)
{
  return (strncmp (header->id, id, strlen (id)) == 0);
}

static GList *
read_headers (const gchar *start,
	      gint         len,
	      GError     **error)
{
  int i = 0;
  int section_len;
  Header *header;
  GList *headers = NULL;

  while (i < len)
    {
      if (i + 30 >= len)
	goto error;

      if (strncmp (start + i, "GTKTEXTBUFFERCONTENTS-0001", 26) == 0 ||
	  strncmp (start + i, "GTKTEXTBUFFERPIXBDATA-0001", 26) == 0)
	{
	  section_len = read_int ((const guchar *) start + i + 26);

	  if (i + 30 + section_len > len)
	    goto error;

	  header = g_new0 (Header, 1);
	  header->id = start + i;
	  header->length = section_len;
	  header->start = start + i + 30;

	  i += 30 + section_len;

	  headers = g_list_prepend (headers, header);
	}
      else
	break;
    }

  return g_list_reverse (headers);

 error:
  g_list_foreach (headers, (GFunc) g_free, NULL);
  g_list_free (headers);

  g_set_error_literal (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_PARSE,
                       _("Serialized data is malformed"));

  return NULL;
}

static gboolean
deserialize_text (GtkTextBuffer *buffer,
		  GtkTextIter   *iter,
		  const gchar   *text,
		  gint           len,
		  gboolean       create_tags,
		  GError       **error,
		  GList         *headers)
{
  GMarkupParseContext *context;
  ParseInfo info;
  gboolean retval = FALSE;

  static const GMarkupParser rich_text_parser = {
    start_element_handler,
    end_element_handler,
    text_handler,
    NULL,
    NULL
  };

  parse_info_init (&info, buffer, create_tags, headers);

  context = g_markup_parse_context_new (&rich_text_parser,
                                        0, &info, NULL);

  if (!g_markup_parse_context_parse (context,
                                     text,
                                     len,
                                     error))
    goto out;

  if (!g_markup_parse_context_end_parse (context, error))
    goto out;

  retval = TRUE;

  /* Now insert the text */
  insert_text (&info, iter);

 out:
  parse_info_free (&info);

  g_markup_parse_context_free (context);

  return retval;
}

gboolean
_gtk_text_buffer_deserialize_rich_text (GtkTextBuffer *register_buffer,
                                        GtkTextBuffer *content_buffer,
                                        GtkTextIter   *iter,
                                        const guint8  *text,
                                        gsize          length,
                                        gboolean       create_tags,
                                        gpointer       user_data,
                                        GError       **error)
{
  GList *headers;
  Header *header;
  gboolean retval;

  headers = read_headers ((gchar *) text, length, error);

  if (!headers)
    return FALSE;

  header = headers->data;
  if (!header_is (header, "GTKTEXTBUFFERCONTENTS-0001"))
    {
      g_set_error_literal (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_PARSE,
                           _("Serialized data is malformed. First section isn't GTKTEXTBUFFERCONTENTS-0001"));

      retval = FALSE;
      goto out;
    }

  retval = deserialize_text (content_buffer, iter,
			     header->start, header->length,
			     create_tags, error, headers->next);

 out:
  g_list_foreach (headers, (GFunc)g_free, NULL);
  g_list_free (headers);

  return retval;
}
