/* gtkbuilderparser.c
 * Copyright (C) 2006-2007 Async Open Source,
 *                         Johan Dahlin <jdahlin@async.com.br>
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
#include <gmodule.h>

#include "gtktypeutils.h"
#include "gtkbuilderprivate.h"
#include "gtkbuilder.h"
#include "gtkbuildable.h"
#include "gtkdebug.h"
#include "gtkversion.h"
#include "gtktypeutils.h"
#include "gtkintl.h"
#include "gtkalias.h"

static void free_property_info (PropertyInfo *info);
static void free_object_info (ObjectInfo *info);

static inline void
state_push (ParserData *data, gpointer info)
{
  data->stack = g_slist_prepend (data->stack, info);
}

static inline gpointer
state_peek (ParserData *data)
{
  if (!data->stack)
    return NULL;

  return data->stack->data;
}

static inline gpointer
state_pop (ParserData *data)
{
  gpointer old = NULL;

  if (!data->stack)
    return NULL;

  old = data->stack->data;
  data->stack = g_slist_delete_link (data->stack, data->stack);
  return old;
}
#define state_peek_info(data, st) ((st*)state_peek(data))
#define state_pop_info(data, st) ((st*)state_pop(data))

static void
error_missing_attribute (ParserData *data,
                         const gchar *tag,
                         const gchar *attribute,
                         GError **error)
{
  gint line_number, char_number;

  g_markup_parse_context_get_position (data->ctx,
                                       &line_number,
                                       &char_number);

  g_set_error (error,
               GTK_BUILDER_ERROR,
               GTK_BUILDER_ERROR_MISSING_ATTRIBUTE,
               "%s:%d:%d <%s> requires attribute \"%s\"",
               data->filename,
               line_number, char_number, tag, attribute);
}

static void
error_invalid_attribute (ParserData *data,
                         const gchar *tag,
                         const gchar *attribute,
                         GError **error)
{
  gint line_number, char_number;

  g_markup_parse_context_get_position (data->ctx,
                                       &line_number,
                                       &char_number);

  g_set_error (error,
               GTK_BUILDER_ERROR,
               GTK_BUILDER_ERROR_INVALID_ATTRIBUTE,
               "%s:%d:%d '%s' is not a valid attribute of <%s>",
               data->filename,
               line_number, char_number, attribute, tag);
}

static void
error_invalid_tag (ParserData *data,
		   const gchar *tag,
		   const gchar *expected,
		   GError **error)
{
  gint line_number, char_number;

  g_markup_parse_context_get_position (data->ctx,
                                       &line_number,
                                       &char_number);

  if (expected)
    g_set_error (error,
		 GTK_BUILDER_ERROR,
		 GTK_BUILDER_ERROR_INVALID_TAG,
		 "%s:%d:%d '%s' is not a valid tag here, expected a '%s' tag",
		 data->filename,
		 line_number, char_number, tag, expected);
  else
    g_set_error (error,
		 GTK_BUILDER_ERROR,
		 GTK_BUILDER_ERROR_INVALID_TAG,
		 "%s:%d:%d '%s' is not a valid tag here",
		 data->filename,
		 line_number, char_number, tag);
}

gboolean
_gtk_builder_boolean_from_string (const gchar  *string,
				  gboolean     *value,
				  GError      **error)
{
  gboolean retval = TRUE;
  int length;
  
  g_assert (string != NULL);
  length = strlen (string);
  
  if (length == 0)
    retval = FALSE;
  else if (length == 1)
    {
      gchar c = g_ascii_tolower (string[0]);
      if (c == 'y' || c == 't' || c == '1')
	*value = TRUE;
      else if (c == 'n' || c == 'f' || c == '0')
	*value = FALSE;
      else
	retval = FALSE;
    }
  else
    {
      gchar *lower = g_ascii_strdown (string, length);
      
      if (strcmp (lower, "yes") == 0 || strcmp (lower, "true") == 0)
	*value = TRUE;
      else if (strcmp (lower, "no") == 0 || strcmp (lower, "false") == 0)
	*value = FALSE;
      else
	retval = FALSE;
      g_free (lower);
    }
  
  if (!retval)
    g_set_error (error,
		 GTK_BUILDER_ERROR,
		 GTK_BUILDER_ERROR_INVALID_VALUE,
		 "could not parse boolean `%s'",
		 string);

  return retval;
}

static GObject *
builder_construct (ParserData  *data,
                   ObjectInfo  *object_info,
		   GError     **error)
{
  GObject *object;

  g_assert (object_info != NULL);

  if (object_info->object)
    return object_info->object;

  object_info->properties = g_slist_reverse (object_info->properties);

  object = _gtk_builder_construct (data->builder, object_info, error);
  if (!object)
    return NULL;

  g_assert (G_IS_OBJECT (object));

  object_info->object = object;

  return object;
}

static gchar *
_get_type_by_symbol (const gchar* symbol)
{
  static GModule *module = NULL;
  GTypeGetFunc func;
  GType type;
  
  if (!module)
    module = g_module_open (NULL, 0);

  if (!g_module_symbol (module, symbol, (gpointer)&func))
    return NULL;
  
  type = func ();
  if (type == G_TYPE_INVALID)
    return NULL;

  return g_strdup (g_type_name (type));
}

static void
parse_requires (ParserData   *data,
		const gchar  *element_name,
		const gchar **names,
		const gchar **values,
		GError      **error)
{
  RequiresInfo *req_info;
  const gchar  *library = NULL;
  const gchar  *version = NULL;
  gchar       **split;
  gint          i, version_major = 0, version_minor = 0;
  gint          line_number, char_number;

  g_markup_parse_context_get_position (data->ctx,
                                       &line_number,
                                       &char_number);

  for (i = 0; names[i] != NULL; i++)
    {
      if (strcmp (names[i], "lib") == 0)
        library = values[i];
      else if (strcmp (names[i], "version") == 0)
	version = values[i];
      else
	error_invalid_attribute (data, element_name, names[i], error);
    }

  if (!library || !version)
    {
      error_missing_attribute (data, element_name, 
			       version ? "lib" : "version", error);
      return;
    }

  if (!(split = g_strsplit (version, ".", 2)) || !split[0] || !split[1])
    {
      g_set_error (error,
		   GTK_BUILDER_ERROR,
		   GTK_BUILDER_ERROR_INVALID_VALUE,
		   "%s:%d:%d <%s> attribute has malformed value \"%s\"",
		   data->filename,
		   line_number, char_number, "version", version);
      return;
    }
  version_major = g_ascii_strtoll (split[0], NULL, 10);
  version_minor = g_ascii_strtoll (split[1], NULL, 10);
  g_strfreev (split);

  req_info = g_slice_new0 (RequiresInfo);
  req_info->library = g_strdup (library);
  req_info->major   = version_major;
  req_info->minor   = version_minor;
  state_push (data, req_info);
  req_info->tag.name = element_name;
}

static gboolean
is_requested_object (const gchar *object,
                     ParserData  *data)
{
  GSList *l;

  for (l = data->requested_objects; l; l = l->next)
    {
      if (strcmp (l->data, object) == 0)
        return TRUE;
    }

  return FALSE;
}

static void
parse_object (GMarkupParseContext  *context,
              ParserData           *data,
              const gchar          *element_name,
              const gchar         **names,
              const gchar         **values,
              GError              **error)
{
  ObjectInfo *object_info;
  ChildInfo* child_info;
  int i;
  gchar *object_class = NULL;
  gchar *object_id = NULL;
  gchar *constructor = NULL;
  gint line, line2;

  child_info = state_peek_info (data, ChildInfo);
  if (child_info && strcmp (child_info->tag.name, "object") == 0)
    {
      error_invalid_tag (data, element_name, NULL, error);
      return;
    }

  for (i = 0; names[i] != NULL; i++)
    {
      if (strcmp (names[i], "class") == 0)
        object_class = g_strdup (values[i]);
      else if (strcmp (names[i], "id") == 0)
        object_id = g_strdup (values[i]);
      else if (strcmp (names[i], "constructor") == 0)
        constructor = g_strdup (values[i]);
      else if (strcmp (names[i], "type-func") == 0)
        {
	  /* Call the GType function, and return the name of the GType,
	   * it's guaranteed afterwards that g_type_from_name on the name
	   * will return our GType
	   */
          object_class = _get_type_by_symbol (values[i]);
          if (!object_class)
            {
              g_markup_parse_context_get_position (context, &line, NULL);
              g_set_error (error, GTK_BUILDER_ERROR,
                           GTK_BUILDER_ERROR_INVALID_TYPE_FUNCTION,
                           _("Invalid type function on line %d: '%s'"),
                           line, values[i]);
              return;
            }
        }
      else
	{
	  error_invalid_attribute (data, element_name, names[i], error);
	  return;
	}
    }

  if (!object_class)
    {
      error_missing_attribute (data, element_name, "class", error);
      return;
    }

  if (!object_id)
    {
      error_missing_attribute (data, element_name, "id", error);
      return;
    }

  ++data->cur_object_level;

  /* check if we reached a requested object (if it is specified) */
  if (data->requested_objects && !data->inside_requested_object)
    {
      if (is_requested_object (object_id, data))
        {
          data->requested_object_level = data->cur_object_level;

          GTK_NOTE (BUILDER, g_print ("requested object \"%s\" found at level %d\n",
                                      object_id,
                                      data->requested_object_level));

          data->inside_requested_object = TRUE;
        }
      else
        {
          g_free (object_class);
          g_free (object_id);
          g_free (constructor);
          return;
        }
    }

  object_info = g_slice_new0 (ObjectInfo);
  object_info->class_name = object_class;
  object_info->id = object_id;
  object_info->constructor = constructor;
  state_push (data, object_info);
  object_info->tag.name = element_name;

  if (child_info)
    object_info->parent = (CommonInfo*)child_info;

  g_markup_parse_context_get_position (context, &line, NULL);
  line2 = GPOINTER_TO_INT (g_hash_table_lookup (data->object_ids, object_id));
  if (line2 != 0)
    {
      g_set_error (error, GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_DUPLICATE_ID,
                   _("Duplicate object ID '%s' on line %d (previously on line %d)"),
                   object_id, line, line2);
      return;
    }


  g_hash_table_insert (data->object_ids, g_strdup (object_id), GINT_TO_POINTER (line));
}

static void
free_object_info (ObjectInfo *info)
{
  /* Do not free the signal items, which GtkBuilder takes ownership of */
  g_slist_free (info->signals);
  g_slist_foreach (info->properties,
                   (GFunc)free_property_info, NULL);
  g_slist_free (info->properties);
  g_free (info->constructor);
  g_free (info->class_name);
  g_free (info->id);
  g_slice_free (ObjectInfo, info);
}

static void
parse_child (ParserData   *data,
             const gchar  *element_name,
             const gchar **names,
             const gchar **values,
             GError      **error)

{
  ObjectInfo* object_info;
  ChildInfo *child_info;
  guint i;

  object_info = state_peek_info (data, ObjectInfo);
  if (!object_info || strcmp (object_info->tag.name, "object") != 0)
    {
      error_invalid_tag (data, element_name, NULL, error);
      return;
    }
  
  child_info = g_slice_new0 (ChildInfo);
  state_push (data, child_info);
  child_info->tag.name = element_name;
  for (i = 0; names[i]; i++)
    {
      if (strcmp (names[i], "type") == 0)
        child_info->type = g_strdup (values[i]);
      else if (strcmp (names[i], "internal-child") == 0)
        child_info->internal_child = g_strdup (values[i]);
      else
	error_invalid_attribute (data, element_name, names[i], error);
    }

  child_info->parent = (CommonInfo*)object_info;

  object_info->object = builder_construct (data, object_info, error);
}

static void
free_child_info (ChildInfo *info)
{
  g_free (info->type);
  g_free (info->internal_child);
  g_slice_free (ChildInfo, info);
}

static void
parse_property (ParserData   *data,
                const gchar  *element_name,
                const gchar **names,
                const gchar **values,
                GError      **error)
{
  PropertyInfo *info;
  gchar *name = NULL;
  gchar *context = NULL;
  gboolean translatable = FALSE;
  ObjectInfo *object_info;
  int i;

  object_info = state_peek_info (data, ObjectInfo);
  if (!object_info || strcmp (object_info->tag.name, "object") != 0)
    {
      error_invalid_tag (data, element_name, NULL, error);
      return;
    }

  for (i = 0; names[i] != NULL; i++)
    {
      if (strcmp (names[i], "name") == 0)
        name = g_strdelimit (g_strdup (values[i]), "_", '-');
      else if (strcmp (names[i], "translatable") == 0)
	{
	  if (!_gtk_builder_boolean_from_string (values[i], &translatable,
						 error))
	    return;
	}
      else if (strcmp (names[i], "comments") == 0)
        {
          /* do nothing, comments are for translators */
        }
      else if (strcmp (names[i], "context") == 0) 
        {
          context = g_strdup (values[i]);
        }
      else
	{
	  error_invalid_attribute (data, element_name, names[i], error);
	  return;
	}
    }

  if (!name)
    {
      error_missing_attribute (data, element_name, "name", error);
      return;
    }

  info = g_slice_new0 (PropertyInfo);
  info->name = name;
  info->translatable = translatable;
  info->context = context;
  info->text = g_string_new ("");
  state_push (data, info);

  info->tag.name = element_name;
}

static void
free_property_info (PropertyInfo *info)
{
  g_free (info->data);
  g_free (info->name);
  g_slice_free (PropertyInfo, info);
}

static void
parse_signal (ParserData   *data,
              const gchar  *element_name,
              const gchar **names,
              const gchar **values,
              GError      **error)
{
  SignalInfo *info;
  gchar *name = NULL;
  gchar *handler = NULL;
  gchar *object = NULL;
  gboolean after = FALSE;
  gboolean swapped = FALSE;
  gboolean swapped_set = FALSE;
  ObjectInfo *object_info;
  int i;

  object_info = state_peek_info (data, ObjectInfo);
  if (!object_info || strcmp (object_info->tag.name, "object") != 0)
    {
      error_invalid_tag (data, element_name, NULL, error);
      return;
    }

  for (i = 0; names[i] != NULL; i++)
    {
      if (strcmp (names[i], "name") == 0)
        name = g_strdup (values[i]);
      else if (strcmp (names[i], "handler") == 0)
        handler = g_strdup (values[i]);
      else if (strcmp (names[i], "after") == 0)
	{
	  if (!_gtk_builder_boolean_from_string (values[i], &after, error))
	    return;
	}
      else if (strcmp (names[i], "swapped") == 0)
	{
	  if (!_gtk_builder_boolean_from_string (values[i], &swapped, error))
	    return;
	  swapped_set = TRUE;
	}
      else if (strcmp (names[i], "object") == 0)
        object = g_strdup (values[i]);
      else if (strcmp (names[i], "last_modification_time") == 0)
	/* parse but ignore */
	;
      else
	{
	  error_invalid_attribute (data, element_name, names[i], error);
	  return;
	}
    }

  if (!name)
    {
      error_missing_attribute (data, element_name, "name", error);
      return;
    }
  else if (!handler)
    {
      error_missing_attribute (data, element_name, "handler", error);
      return;
    }

  /* Swapped defaults to FALSE except when object is set */
  if (object && !swapped_set)
    swapped = TRUE;
  
  info = g_slice_new0 (SignalInfo);
  info->name = name;
  info->handler = handler;
  if (after)
    info->flags |= G_CONNECT_AFTER;
  if (swapped)
    info->flags |= G_CONNECT_SWAPPED;
  info->connect_object_name = object;
  state_push (data, info);

  info->tag.name = element_name;
}

/* Called by GtkBuilder */
void
_free_signal_info (SignalInfo *info,
                   gpointer user_data)
{
  g_free (info->name);
  g_free (info->handler);
  g_free (info->connect_object_name);
  g_free (info->object_name);
  g_slice_free (SignalInfo, info);
}

void
_free_requires_info (RequiresInfo *info,
		     gpointer user_data)
{
  g_free (info->library);
  g_slice_free (RequiresInfo, info);
}

static void
parse_interface (ParserData   *data,
		 const gchar  *element_name,
		 const gchar **names,
		 const gchar **values,
		 GError      **error)
{
  int i;

  for (i = 0; names[i] != NULL; i++)
    {
      if (strcmp (names[i], "domain") == 0)
	{
	  if (data->domain)
	    {
	      if (strcmp (data->domain, values[i]) == 0)
		continue;
	      else
		g_warning ("%s: interface domain '%s' overrides "
			   "programically set domain '%s'",
			   data->filename,
			   values[i],
			   data->domain
			   );
	      
	      g_free (data->domain);
	    }
	  
 	  data->domain = g_strdup (values[i]);
	  gtk_builder_set_translation_domain (data->builder, data->domain);
	}
      else
	error_invalid_attribute (data, "interface", names[i], error);
    }
}

static SubParser *
create_subparser (GObject       *object,
		  GObject       *child,
		  const gchar   *element_name,
		  GMarkupParser *parser,
		  gpointer       user_data)
{
  SubParser *subparser;

  subparser = g_slice_new0 (SubParser);
  subparser->object = object;
  subparser->child = child;
  subparser->tagname = g_strdup (element_name);
  subparser->start = element_name;
  subparser->parser = g_memdup (parser, sizeof (GMarkupParser));
  subparser->data = user_data;

  return subparser;
}

static void
free_subparser (SubParser *subparser)
{
  g_free (subparser->tagname);
  g_slice_free (SubParser, subparser);
}

static gboolean
subparser_start (GMarkupParseContext *context,
		 const gchar         *element_name,
		 const gchar        **names,
		 const gchar        **values,
		 ParserData          *data,
		 GError             **error)
{
  SubParser *subparser = data->subparser;

  if (!subparser->start &&
      strcmp (element_name, subparser->tagname) == 0)
    subparser->start = element_name;

  if (subparser->start)
    {
      if (subparser->parser->start_element)
	subparser->parser->start_element (context,
					  element_name, names, values,
					  subparser->data, error);
      return FALSE;
    }
  return TRUE;
}

static void
subparser_end (GMarkupParseContext *context,
	       const gchar         *element_name,
	       ParserData          *data,
	       GError             **error)
{
  if (data->subparser->parser->end_element)
    data->subparser->parser->end_element (context, element_name,
					  data->subparser->data, error);
  
  if (!strcmp (data->subparser->start, element_name) == 0)
    return;
    
  gtk_buildable_custom_tag_end (GTK_BUILDABLE (data->subparser->object),
				data->builder,
				data->subparser->child,
				element_name,
				data->subparser->data);
  g_free (data->subparser->parser);
      
  if (GTK_BUILDABLE_GET_IFACE (data->subparser->object)->custom_finished)
    data->custom_finalizers = g_slist_prepend (data->custom_finalizers,
					       data->subparser);
  else
    free_subparser (data->subparser);
  
  data->subparser = NULL;
}

static gboolean
parse_custom (GMarkupParseContext *context,
	      const gchar         *element_name,
	      const gchar        **names,
	      const gchar        **values,
	      ParserData          *data,
	      GError             **error)
{
  CommonInfo* parent_info;
  GMarkupParser parser;
  gpointer subparser_data;
  GObject *object;
  GObject *child;

  parent_info = state_peek_info (data, CommonInfo);
  if (!parent_info)
    return FALSE;

  if (strcmp (parent_info->tag.name, "object") == 0)
    {
      ObjectInfo* object_info = (ObjectInfo*)parent_info;
      if (!object_info->object)
	{
	  object_info->properties = g_slist_reverse (object_info->properties);
	  object_info->object = _gtk_builder_construct (data->builder,
							object_info,
							error);
	  if (!object_info->object)
	    return TRUE; /* A GError is already set */
	}
      g_assert (object_info->object);
      object = object_info->object;
      child = NULL;
    }
  else if (strcmp (parent_info->tag.name, "child") == 0)
    {
      ChildInfo* child_info = (ChildInfo*)parent_info;
      
      _gtk_builder_add (data->builder, child_info);
      
      object = ((ObjectInfo*)child_info->parent)->object;
      child  = child_info->object;
    }
  else
    return FALSE;

  if (!gtk_buildable_custom_tag_start (GTK_BUILDABLE (object),
				       data->builder,
				       child,
				       element_name,
				       &parser,
				       &subparser_data))
    return FALSE;
      
  data->subparser = create_subparser (object, child, element_name,
				      &parser, subparser_data);
  
  if (parser.start_element)
    parser.start_element (context,
			  element_name, names, values,
			  subparser_data, error);
  return TRUE;
}

static void
start_element (GMarkupParseContext *context,
               const gchar         *element_name,
               const gchar        **names,
               const gchar        **values,
               gpointer             user_data,
               GError             **error)
{
  ParserData *data = (ParserData*)user_data;

#ifdef GTK_ENABLE_DEBUG
  if (gtk_debug_flags & GTK_DEBUG_BUILDER)
    {
      GString *tags = g_string_new ("");
      int i;
      for (i = 0; names[i]; i++)
        g_string_append_printf (tags, "%s=\"%s\" ", names[i], values[i]);

      if (i)
        {
          g_string_insert_c (tags, 0, ' ');
          g_string_truncate (tags, tags->len - 1);
        }
      g_print ("<%s%s>\n", element_name, tags->str);
      g_string_free (tags, TRUE);
    }
#endif

  if (!data->last_element && strcmp (element_name, "interface") != 0)
    {
      g_set_error (error, GTK_BUILDER_ERROR, 
		   GTK_BUILDER_ERROR_UNHANDLED_TAG,
		   _("Invalid root element: '%s'"),
		   element_name);
      return;
    }
  data->last_element = element_name;

  if (data->subparser)
    if (!subparser_start (context, element_name, names, values,
			  data, error))
      return;

  if (strcmp (element_name, "requires") == 0)
    parse_requires (data, element_name, names, values, error);
  else if (strcmp (element_name, "object") == 0)
    parse_object (context, data, element_name, names, values, error);
  else if (data->requested_objects && !data->inside_requested_object)
    {
      /* If outside a requested object, simply ignore this tag */
      return;
    }
  else if (strcmp (element_name, "child") == 0)
    parse_child (data, element_name, names, values, error);
  else if (strcmp (element_name, "property") == 0)
    parse_property (data, element_name, names, values, error);
  else if (strcmp (element_name, "signal") == 0)
    parse_signal (data, element_name, names, values, error);
  else if (strcmp (element_name, "interface") == 0)
    parse_interface (data, element_name, names, values, error);
  else if (strcmp (element_name, "placeholder") == 0)
    {
      /* placeholder has no special treatmeant, but it needs an
       * if clause to avoid an error below.
       */
    }
  else
    if (!parse_custom (context, element_name, names, values,
		       data, error))
      g_set_error (error, GTK_BUILDER_ERROR, 
		   GTK_BUILDER_ERROR_UNHANDLED_TAG,
		   _("Unhandled tag: '%s'"),
		   element_name);
}

gchar *
_gtk_builder_parser_translate (const gchar *domain,
			       const gchar *context,
			       const gchar *text)
{
  const char *s;

  if (context)
    s = g_dpgettext2 (domain, context, text);
  else
    s = g_dgettext (domain, text);

  return g_strdup (s);
}

/* Called for close tags </foo> */
static void
end_element (GMarkupParseContext *context,
             const gchar         *element_name,
             gpointer             user_data,
             GError             **error)
{
  ParserData *data = (ParserData*)user_data;

  GTK_NOTE (BUILDER, g_print ("</%s>\n", element_name));

  if (data->subparser && data->subparser->start)
    {
      subparser_end (context, element_name, data, error);
      return;
    }

  if (strcmp (element_name, "requires") == 0)
    {
      RequiresInfo *req_info = state_pop_info (data, RequiresInfo);

      /* TODO: Allow third party widget developers to check thier
       * required versions, possibly throw a signal allowing them
       * to check thier library versions here.
       */
      if (!strcmp (req_info->library, "gtk+"))
	{
	  if (!GTK_CHECK_VERSION (req_info->major, req_info->minor, 0))
	    g_set_error (error,
			 GTK_BUILDER_ERROR,
			 GTK_BUILDER_ERROR_VERSION_MISMATCH,
			 "%s: required %s version %d.%d, current version is %d.%d",
			 data->filename, req_info->library,
			 req_info->major, req_info->minor,
			 GTK_MAJOR_VERSION, GTK_MINOR_VERSION);
	}
      _free_requires_info (req_info, NULL);
    }
  else if (strcmp (element_name, "interface") == 0)
    {
    }
  else if (data->requested_objects && !data->inside_requested_object)
    {
      /* If outside a requested object, simply ignore this tag */
      return;
    }
  else if (strcmp (element_name, "object") == 0)
    {
      ObjectInfo *object_info = state_pop_info (data, ObjectInfo);
      ChildInfo* child_info = state_peek_info (data, ChildInfo);

      if (data->requested_objects && data->inside_requested_object &&
          (data->cur_object_level == data->requested_object_level))
        {
          GTK_NOTE (BUILDER, g_print ("requested object end found at level %d\n",
                                      data->requested_object_level));

          data->inside_requested_object = FALSE;
        }

      --data->cur_object_level;

      g_assert (data->cur_object_level >= 0);

      object_info->object = builder_construct (data, object_info, error);
      if (!object_info->object)
	{
	  free_object_info (object_info);
	  return;
	}
      if (child_info)
        child_info->object = object_info->object;

      if (GTK_IS_BUILDABLE (object_info->object) &&
          GTK_BUILDABLE_GET_IFACE (object_info->object)->parser_finished)
        data->finalizers = g_slist_prepend (data->finalizers, object_info->object);
      _gtk_builder_add_signals (data->builder, object_info->signals);

      free_object_info (object_info);
    }
  else if (strcmp (element_name, "property") == 0)
    {
      PropertyInfo *prop_info = state_pop_info (data, PropertyInfo);
      CommonInfo *info = state_peek_info (data, CommonInfo);

      /* Normal properties */
      if (strcmp (info->tag.name, "object") == 0)
        {
          ObjectInfo *object_info = (ObjectInfo*)info;

          if (prop_info->translatable && prop_info->text->len)
            {
	      prop_info->data = _gtk_builder_parser_translate (data->domain,
							       prop_info->context,
							       prop_info->text->str);
              g_string_free (prop_info->text, TRUE);
            }
          else
            {
              prop_info->data = g_string_free (prop_info->text, FALSE);
              
            }

          object_info->properties =
            g_slist_prepend (object_info->properties, prop_info);
        }
      else
        g_assert_not_reached ();
    }
  else if (strcmp (element_name, "child") == 0)
    {
      ChildInfo *child_info = state_pop_info (data, ChildInfo);

      _gtk_builder_add (data->builder, child_info);

      free_child_info (child_info);
    }
  else if (strcmp (element_name, "signal") == 0)
    {
      SignalInfo *signal_info = state_pop_info (data, SignalInfo);
      ObjectInfo *object_info = (ObjectInfo*)state_peek_info (data, CommonInfo);
      signal_info->object_name = g_strdup (object_info->id);
      object_info->signals =
        g_slist_prepend (object_info->signals, signal_info);
    }
  else if (strcmp (element_name, "placeholder") == 0)
    {
    }
  else
    {
      g_assert_not_reached ();
    }
}

/* Called for character data */
/* text is not nul-terminated */
static void
text (GMarkupParseContext *context,
      const gchar         *text,
      gsize                text_len,
      gpointer             user_data,
      GError             **error)
{
  ParserData *data = (ParserData*)user_data;
  CommonInfo *info;

  if (data->subparser && data->subparser->start)
    {
      GError *tmp_error = NULL;
      
      if (data->subparser->parser->text)
        data->subparser->parser->text (context, text, text_len,
                                       data->subparser->data, &tmp_error);
      if (tmp_error)
	g_propagate_error (error, tmp_error);
      return;
    }

  if (!data->stack)
    return;

  info = state_peek_info (data, CommonInfo);
  g_assert (info != NULL);

  if (strcmp (g_markup_parse_context_get_element (context), "property") == 0)
    {
      PropertyInfo *prop_info = (PropertyInfo*)info;

      g_string_append_len (prop_info->text, text, text_len);
    }
}

static void
free_info (CommonInfo *info)
{
  if (strcmp (info->tag.name, "object") == 0) 
    free_object_info ((ObjectInfo *)info);
  else if (strcmp (info->tag.name, "child") == 0) 
    free_child_info ((ChildInfo *)info);
  else if (strcmp (info->tag.name, "property") == 0) 
    free_property_info ((PropertyInfo *)info);
  else if (strcmp (info->tag.name, "signal") == 0) 
    _free_signal_info ((SignalInfo *)info, NULL);
  else if (strcmp (info->tag.name, "requires") == 0) 
    _free_requires_info ((RequiresInfo *)info, NULL);
  else 
    g_assert_not_reached ();
}

static const GMarkupParser parser = {
  start_element,
  end_element,
  text,
  NULL,
  NULL
};

void
_gtk_builder_parser_parse_buffer (GtkBuilder   *builder,
                                  const gchar  *filename,
                                  const gchar  *buffer,
                                  gsize         length,
                                  gchar       **requested_objs,
                                  GError      **error)
{
  const gchar* domain;
  ParserData *data;
  GSList *l;
  
  /* Store the original domain so that interface domain attribute can be
   * applied for the builder and the original domain can be restored after
   * parsing has finished. This allows subparsers to translate elements with
   * gtk_builder_get_translation_domain() without breaking the ABI or API
   */
  domain = gtk_builder_get_translation_domain (builder);

  data = g_new0 (ParserData, 1);
  data->builder = builder;
  data->filename = filename;
  data->domain = g_strdup (domain);
  data->object_ids = g_hash_table_new_full (g_str_hash, g_str_equal,
					    (GDestroyNotify)g_free, NULL);

  data->requested_objects = NULL;
  if (requested_objs)
    {
      gint i;

      data->inside_requested_object = FALSE;
      for (i = 0; requested_objs[i]; ++i)
        {
          data->requested_objects = g_slist_prepend (data->requested_objects,
                                                     g_strdup (requested_objs[i]));	
        }
    }
  else
    {
      /* get all the objects */
      data->inside_requested_object = TRUE;
    }

  data->ctx = g_markup_parse_context_new (&parser, 
                                          G_MARKUP_TREAT_CDATA_AS_TEXT, 
                                          data, NULL);

  if (!g_markup_parse_context_parse (data->ctx, buffer, length, error))
    goto out;

  _gtk_builder_finish (builder);

  /* Custom parser_finished */
  data->custom_finalizers = g_slist_reverse (data->custom_finalizers);
  for (l = data->custom_finalizers; l; l = l->next)
    {
      SubParser *sub = (SubParser*)l->data;
      
      gtk_buildable_custom_finished (GTK_BUILDABLE (sub->object),
                                     builder,
                                     sub->child,
                                     sub->tagname,
                                     sub->data);
    }
  
  /* Common parser_finished, for all created objects */
  data->finalizers = g_slist_reverse (data->finalizers);
  for (l = data->finalizers; l; l = l->next)
    {
      GtkBuildable *buildable = (GtkBuildable*)l->data;
      gtk_buildable_parser_finished (GTK_BUILDABLE (buildable), builder);
    }

 out:

  g_slist_foreach (data->stack, (GFunc)free_info, NULL);
  g_slist_free (data->stack);
  g_slist_foreach (data->custom_finalizers, (GFunc)free_subparser, NULL);
  g_slist_free (data->custom_finalizers);
  g_slist_free (data->finalizers);
  g_slist_foreach (data->requested_objects, (GFunc) g_free, NULL);
  g_slist_free (data->requested_objects);
  g_free (data->domain);
  g_hash_table_destroy (data->object_ids);
  g_markup_parse_context_free (data->ctx);
  g_free (data);

  /* restore the original domain */
  gtk_builder_set_translation_domain (builder, domain);
}
