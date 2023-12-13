/* GTK - The GIMP Toolkit
 * gtkfilefilter.c: Filters for selecting a file subset
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

#include "config.h"
#include <string.h>

#include "gtkfilefilter.h"
#include "gtkintl.h"
#include "gtkprivate.h"

#include "gtkalias.h"

typedef struct _GtkFileFilterClass GtkFileFilterClass;
typedef struct _FilterRule FilterRule;

#define GTK_FILE_FILTER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FILE_FILTER, GtkFileFilterClass))
#define GTK_IS_FILE_FILTER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FILE_FILTER))
#define GTK_FILE_FILTER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FILE_FILTER, GtkFileFilterClass))

typedef enum {
  FILTER_RULE_PATTERN,
  FILTER_RULE_MIME_TYPE,
  FILTER_RULE_PIXBUF_FORMATS,
  FILTER_RULE_CUSTOM
} FilterRuleType;

struct _GtkFileFilterClass
{
  GtkObjectClass parent_class;
};

struct _GtkFileFilter
{
  GtkObject parent_instance;
  
  gchar *name;
  GSList *rules;

  GtkFileFilterFlags needed;
};

struct _FilterRule
{
  FilterRuleType type;
  GtkFileFilterFlags needed;
  
  union {
    gchar *pattern;
    gchar *mime_type;
    GSList *pixbuf_formats;
    struct {
      GtkFileFilterFunc func;
      gpointer data;
      GDestroyNotify notify;
    } custom;
  } u;
};

static void gtk_file_filter_finalize   (GObject            *object);


G_DEFINE_TYPE (GtkFileFilter, gtk_file_filter, GTK_TYPE_OBJECT)

static void
gtk_file_filter_init (GtkFileFilter *object)
{
}

static void
gtk_file_filter_class_init (GtkFileFilterClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = gtk_file_filter_finalize;
}

static void
filter_rule_free (FilterRule *rule)
{
  switch (rule->type)
    {
    case FILTER_RULE_MIME_TYPE:
      g_free (rule->u.mime_type);
      break;
    case FILTER_RULE_PATTERN:
      g_free (rule->u.pattern);
      break;
    case FILTER_RULE_CUSTOM:
      if (rule->u.custom.notify)
	rule->u.custom.notify (rule->u.custom.data);
      break;
    case FILTER_RULE_PIXBUF_FORMATS:
      g_slist_free (rule->u.pixbuf_formats);
      break;
    default:
      g_assert_not_reached ();
    }

  g_slice_free (FilterRule, rule);
}

static void
gtk_file_filter_finalize (GObject  *object)
{
  GtkFileFilter *filter = GTK_FILE_FILTER (object);

  g_slist_foreach (filter->rules, (GFunc)filter_rule_free, NULL);
  g_slist_free (filter->rules);

  g_free (filter->name);

  G_OBJECT_CLASS (gtk_file_filter_parent_class)->finalize (object);
}

/**
 * gtk_file_filter_new:
 * 
 * Creates a new #GtkFileFilter with no rules added to it.
 * Such a filter doesn't accept any files, so is not
 * particularly useful until you add rules with
 * gtk_file_filter_add_mime_type(), gtk_file_filter_add_pattern(),
 * or gtk_file_filter_add_custom(). To create a filter
 * that accepts any file, use:
 * |[
 * GtkFileFilter *filter = gtk_file_filter_new ();
 * gtk_file_filter_add_pattern (filter, "*");
 * ]|
 * 
 * Return value: a new #GtkFileFilter
 * 
 * Since: 2.4
 **/
GtkFileFilter *
gtk_file_filter_new (void)
{
  return g_object_new (GTK_TYPE_FILE_FILTER, NULL);
}

/**
 * gtk_file_filter_set_name:
 * @filter: a #GtkFileFilter
 * @name: (allow-none): the human-readable-name for the filter, or %NULL
 *   to remove any existing name.
 * 
 * Sets the human-readable name of the filter; this is the string
 * that will be displayed in the file selector user interface if
 * there is a selectable list of filters.
 * 
 * Since: 2.4
 **/
void
gtk_file_filter_set_name (GtkFileFilter *filter,
			  const gchar   *name)
{
  g_return_if_fail (GTK_IS_FILE_FILTER (filter));
  
  g_free (filter->name);

  filter->name = g_strdup (name);
}

/**
 * gtk_file_filter_get_name:
 * @filter: a #GtkFileFilter
 * 
 * Gets the human-readable name for the filter. See gtk_file_filter_set_name().
 * 
 * Return value: The human-readable name of the filter,
 *   or %NULL. This value is owned by GTK+ and must not
 *   be modified or freed.
 * 
 * Since: 2.4
 **/
const gchar *
gtk_file_filter_get_name (GtkFileFilter *filter)
{
  g_return_val_if_fail (GTK_IS_FILE_FILTER (filter), NULL);
  
  return filter->name;
}

static void
file_filter_add_rule (GtkFileFilter *filter,
		      FilterRule    *rule)
{
  filter->needed |= rule->needed;
  filter->rules = g_slist_append (filter->rules, rule);
}

/**
 * gtk_file_filter_add_mime_type:
 * @filter: A #GtkFileFilter
 * @mime_type: name of a MIME type
 * 
 * Adds a rule allowing a given mime type to @filter.
 * 
 * Since: 2.4
 **/
void
gtk_file_filter_add_mime_type (GtkFileFilter *filter,
			       const gchar   *mime_type)
{
  FilterRule *rule;
  
  g_return_if_fail (GTK_IS_FILE_FILTER (filter));
  g_return_if_fail (mime_type != NULL);

  rule = g_slice_new (FilterRule);
  rule->type = FILTER_RULE_MIME_TYPE;
  rule->needed = GTK_FILE_FILTER_MIME_TYPE;
  rule->u.mime_type = g_strdup (mime_type);

  file_filter_add_rule (filter, rule);
}

/**
 * gtk_file_filter_add_pattern:
 * @filter: a #GtkFileFilter
 * @pattern: a shell style glob
 * 
 * Adds a rule allowing a shell style glob to a filter.
 * 
 * Since: 2.4
 **/
void
gtk_file_filter_add_pattern (GtkFileFilter *filter,
			     const gchar   *pattern)
{
  FilterRule *rule;
  
  g_return_if_fail (GTK_IS_FILE_FILTER (filter));
  g_return_if_fail (pattern != NULL);

  rule = g_slice_new (FilterRule);
  rule->type = FILTER_RULE_PATTERN;
  rule->needed = GTK_FILE_FILTER_DISPLAY_NAME;
  rule->u.pattern = g_strdup (pattern);

  file_filter_add_rule (filter, rule);
}

/**
 * gtk_file_filter_add_pixbuf_formats:
 * @filter: a #GtkFileFilter
 * 
 * Adds a rule allowing image files in the formats supported
 * by GdkPixbuf.
 * 
 * Since: 2.6
 **/
void
gtk_file_filter_add_pixbuf_formats (GtkFileFilter *filter)
{
  FilterRule *rule;
  
  g_return_if_fail (GTK_IS_FILE_FILTER (filter));

  rule = g_slice_new (FilterRule);
  rule->type = FILTER_RULE_PIXBUF_FORMATS;
  rule->needed = GTK_FILE_FILTER_MIME_TYPE;
  rule->u.pixbuf_formats = gdk_pixbuf_get_formats ();
  file_filter_add_rule (filter, rule);
}


/**
 * gtk_file_filter_add_custom:
 * @filter: a #GtkFileFilter
 * @needed: bitfield of flags indicating the information that the custom
 *          filter function needs.
 * @func: callback function; if the function returns %TRUE, then
 *   the file will be displayed.
 * @data: data to pass to @func
 * @notify: function to call to free @data when it is no longer needed.
 * 
 * Adds rule to a filter that allows files based on a custom callback
 * function. The bitfield @needed which is passed in provides information
 * about what sorts of information that the filter function needs;
 * this allows GTK+ to avoid retrieving expensive information when
 * it isn't needed by the filter.
 * 
 * Since: 2.4
 **/
void
gtk_file_filter_add_custom (GtkFileFilter         *filter,
			    GtkFileFilterFlags     needed,
			    GtkFileFilterFunc      func,
			    gpointer               data,
			    GDestroyNotify         notify)
{
  FilterRule *rule;
  
  g_return_if_fail (GTK_IS_FILE_FILTER (filter));
  g_return_if_fail (func != NULL);

  rule = g_slice_new (FilterRule);
  rule->type = FILTER_RULE_CUSTOM;
  rule->needed = needed;
  rule->u.custom.func = func;
  rule->u.custom.data = data;
  rule->u.custom.notify = notify;

  file_filter_add_rule (filter, rule);
}

/**
 * gtk_file_filter_get_needed:
 * @filter: a #GtkFileFilter
 * 
 * Gets the fields that need to be filled in for the structure
 * passed to gtk_file_filter_filter()
 * 
 * This function will not typically be used by applications; it
 * is intended principally for use in the implementation of
 * #GtkFileChooser.
 * 
 * Return value: bitfield of flags indicating needed fields when
 *   calling gtk_file_filter_filter()
 * 
 * Since: 2.4
 **/
GtkFileFilterFlags
gtk_file_filter_get_needed (GtkFileFilter *filter)
{
  return filter->needed;
}

/**
 * gtk_file_filter_filter:
 * @filter: a #GtkFileFilter
 * @filter_info: a #GtkFileFilterInfo structure containing information
 *  about a file.
 * 
 * Tests whether a file should be displayed according to @filter.
 * The #GtkFileFilterInfo structure @filter_info should include
 * the fields returned from gtk_file_filter_get_needed().
 *
 * This function will not typically be used by applications; it
 * is intended principally for use in the implementation of
 * #GtkFileChooser.
 * 
 * Return value: %TRUE if the file should be displayed
 * 
 * Since: 2.4
 **/
gboolean
gtk_file_filter_filter (GtkFileFilter           *filter,
			const GtkFileFilterInfo *filter_info)
{
  GSList *tmp_list;

  for (tmp_list = filter->rules; tmp_list; tmp_list = tmp_list->next)
    {
      FilterRule *rule = tmp_list->data;

      if ((filter_info->contains & rule->needed) != rule->needed)
	continue;

      switch (rule->type)
	{
	case FILTER_RULE_MIME_TYPE:
          if (filter_info->mime_type != NULL)
            {
              gchar *filter_content_type, *rule_content_type;
              gboolean match;

              filter_content_type = g_content_type_from_mime_type (filter_info->mime_type);
              rule_content_type = g_content_type_from_mime_type (rule->u.mime_type);
              match = g_content_type_is_a (filter_content_type, rule_content_type);
              g_free (filter_content_type);
              g_free (rule_content_type);

              if (match)
                return TRUE;
            }
	  break;
	case FILTER_RULE_PATTERN:
	  if (filter_info->display_name != NULL &&
	      _gtk_fnmatch (rule->u.pattern, filter_info->display_name, FALSE))
	    return TRUE;
	  break;
	case FILTER_RULE_PIXBUF_FORMATS:
	  {
	    GSList *list;

	    if (!filter_info->mime_type)
	      break;

	    for (list = rule->u.pixbuf_formats; list; list = list->next)
	      {
		int i;
		gchar **mime_types;

		mime_types = gdk_pixbuf_format_get_mime_types (list->data);

		for (i = 0; mime_types[i] != NULL; i++)
		  {
		    if (strcmp (mime_types[i], filter_info->mime_type) == 0)
		      {
			g_strfreev (mime_types);
			return TRUE;
		      }
		  }

		g_strfreev (mime_types);
	      }
	    break;
	  }
	case FILTER_RULE_CUSTOM:
	  if (rule->u.custom.func (filter_info, rule->u.custom.data))
	    return TRUE;
	  break;
	}
    }

  return FALSE;
}

#define __GTK_FILE_FILTER_C__
#include "gtkaliasdef.c"
