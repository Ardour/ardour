/* GTK - The GIMP Toolkit
 * Copyright (C) 1998-2002 James Henstridge <james@daa.com.au>
 * Copyright (C) 2006-2007 Async Open Source,
 *                         Johan Dahlin <jdahlin@async.com.br>,
 *                         Henrique Romano <henrique@async.com.br>
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
#include <errno.h> /* errno */
#include <stdlib.h> /* strtol, strtoul */
#include <string.h> /* strlen */

#include "gtkbuilder.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkmain.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwindow.h"
#include "gtkicontheme.h"
#include "gtkstock.h"
#include "gtkalias.h"

static void gtk_builder_class_init     (GtkBuilderClass *klass);
static void gtk_builder_init           (GtkBuilder      *builder);
static void gtk_builder_finalize       (GObject         *object);
static void gtk_builder_set_property   (GObject         *object,
                                        guint            prop_id,
                                        const GValue    *value,
                                        GParamSpec      *pspec);
static void gtk_builder_get_property   (GObject         *object,
                                        guint            prop_id,
                                        GValue          *value,
                                        GParamSpec      *pspec);
static GType gtk_builder_real_get_type_from_name (GtkBuilder  *builder,
                                                  const gchar *type_name);

enum {
  PROP_0,
  PROP_TRANSLATION_DOMAIN,
};

struct _GtkBuilderPrivate
{
  gchar *domain;
  GHashTable *objects;
  GSList *delayed_properties;
  GSList *signals;
  gchar *filename;
};

G_DEFINE_TYPE (GtkBuilder, gtk_builder, G_TYPE_OBJECT)

static void
gtk_builder_class_init (GtkBuilderClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gtk_builder_finalize;
  gobject_class->set_property = gtk_builder_set_property;
  gobject_class->get_property = gtk_builder_get_property;

  klass->get_type_from_name = gtk_builder_real_get_type_from_name;

 /** 
  * GtkBuilder:translation-domain:
  *
  * The translation domain used when translating property values that
  * have been marked as translatable in interface descriptions.
  * If the translation domain is %NULL, #GtkBuilder uses gettext(),
  * otherwise g_dgettext().
  *
  * Since: 2.12
  */
  g_object_class_install_property (gobject_class,
                                   PROP_TRANSLATION_DOMAIN,
                                   g_param_spec_string ("translation-domain",
                                                        P_("Translation Domain"),
                                                        P_("The translation domain used by gettext"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  g_type_class_add_private (gobject_class, sizeof (GtkBuilderPrivate));
}

static void
gtk_builder_init (GtkBuilder *builder)
{
  builder->priv = G_TYPE_INSTANCE_GET_PRIVATE (builder, GTK_TYPE_BUILDER,
                                               GtkBuilderPrivate);
  builder->priv->domain = NULL;
  builder->priv->objects = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  g_free, g_object_unref);
}


/*
 * GObject virtual methods
 */

static void
gtk_builder_finalize (GObject *object)
{
  GtkBuilderPrivate *priv = GTK_BUILDER (object)->priv;
  
  g_free (priv->domain);
  g_free (priv->filename);
  
  g_hash_table_destroy (priv->objects);

  g_slist_foreach (priv->signals, (GFunc) _free_signal_info, NULL);
  g_slist_free (priv->signals);
  
  G_OBJECT_CLASS (gtk_builder_parent_class)->finalize (object);
}

static void
gtk_builder_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GtkBuilder *builder = GTK_BUILDER (object);

  switch (prop_id)
    {
    case PROP_TRANSLATION_DOMAIN:
      gtk_builder_set_translation_domain (builder, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_builder_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GtkBuilder *builder = GTK_BUILDER (object);

  switch (prop_id)
    {
    case PROP_TRANSLATION_DOMAIN:
      g_value_set_string (value, builder->priv->domain);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


/*
 * Try to map a type name to a _get_type function
 * and call it, eg:
 *
 * GtkWindow -> gtk_window_get_type
 * GtkHBox -> gtk_hbox_get_type
 * GtkUIManager -> gtk_ui_manager_get_type
 *
 */
static GType
_gtk_builder_resolve_type_lazily (const gchar *name)
{
  static GModule *module = NULL;
  GTypeGetFunc func;
  GString *symbol_name = g_string_new ("");
  char c, *symbol;
  int i;
  GType gtype = G_TYPE_INVALID;

  if (!module)
    module = g_module_open (NULL, 0);
  
  for (i = 0; name[i] != '\0'; i++)
    {
      c = name[i];
      /* skip if uppercase, first or previous is uppercase */
      if ((c == g_ascii_toupper (c) &&
           i > 0 && name[i-1] != g_ascii_toupper (name[i-1])) ||
          (i > 2 && name[i]   == g_ascii_toupper (name[i]) &&
           name[i-1] == g_ascii_toupper (name[i-1]) &&
           name[i-2] == g_ascii_toupper (name[i-2])))
        g_string_append_c (symbol_name, '_');
      g_string_append_c (symbol_name, g_ascii_tolower (c));
    }
  g_string_append (symbol_name, "_get_type");
  
  symbol = g_string_free (symbol_name, FALSE);

  if (g_module_symbol (module, symbol, (gpointer)&func))
    gtype = func ();
  
  g_free (symbol);

  return gtype;
}

/*
 * GtkBuilder virtual methods
 */

static GType
gtk_builder_real_get_type_from_name (GtkBuilder  *builder, 
                                     const gchar *type_name)
{
  GType gtype;

  gtype = g_type_from_name (type_name);
  if (gtype != G_TYPE_INVALID)
    return gtype;

  return _gtk_builder_resolve_type_lazily (type_name);
}

typedef struct
{
  gchar *object;
  gchar *name;
  gchar *value;
} DelayedProperty;

static void
gtk_builder_get_parameters (GtkBuilder  *builder,
                            GType        object_type,
                            const gchar *object_name,
                            GSList      *properties,
                            GArray      **parameters,
                            GArray      **construct_parameters)
{
  GSList *l;
  GParamSpec *pspec;
  GObjectClass *oclass;
  DelayedProperty *property;
  GError *error = NULL;
  
  oclass = g_type_class_ref (object_type);
  g_assert (oclass != NULL);

  *parameters = g_array_new (FALSE, FALSE, sizeof (GParameter));
  *construct_parameters = g_array_new (FALSE, FALSE, sizeof (GParameter));

  for (l = properties; l; l = l->next)
    {
      PropertyInfo *prop = (PropertyInfo*)l->data;
      GParameter parameter = { NULL };

      pspec = g_object_class_find_property (G_OBJECT_CLASS (oclass),
                                            prop->name);
      if (!pspec)
        {
          g_warning ("Unknown property: %s.%s",
                     g_type_name (object_type), prop->name);
          continue;
        }

      parameter.name = prop->name;

      if (G_IS_PARAM_SPEC_OBJECT (pspec) &&
          (G_PARAM_SPEC_VALUE_TYPE (pspec) != GDK_TYPE_PIXBUF))
        {
          GObject *object = gtk_builder_get_object (builder, prop->data);

          if (object)
            {
              g_value_init (&parameter.value, G_OBJECT_TYPE (object));
              g_value_set_object (&parameter.value, object);
            }
          else 
            {
              if (pspec->flags & G_PARAM_CONSTRUCT_ONLY)
                {
                  g_warning ("Failed to get constuct only property "
                             "%s of %s with value `%s'",
                             prop->name, object_name, prop->data);
                  continue;
                }
              /* Delay setting property */
              property = g_slice_new (DelayedProperty);
              property->object = g_strdup (object_name);
              property->name = g_strdup (prop->name);
              property->value = g_strdup (prop->data);
              builder->priv->delayed_properties =
                g_slist_prepend (builder->priv->delayed_properties, property);
              continue;
            }
        }
      else if (!gtk_builder_value_from_string (builder, pspec,
					       prop->data, &parameter.value, &error))
        {
          g_warning ("Failed to set property %s.%s to %s: %s",
                     g_type_name (object_type), prop->name, prop->data,
		     error->message);
	  g_error_free (error);
	  error = NULL;
          continue;
        }

      if (pspec->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY))
        g_array_append_val (*construct_parameters, parameter);
      else
        g_array_append_val (*parameters, parameter);
    }

  g_type_class_unref (oclass);
}

static GObject *
gtk_builder_get_internal_child (GtkBuilder  *builder,
                                ObjectInfo  *info,
                                const gchar *childname,
				GError      **error)
{
  GObject *obj = NULL;

  while (!obj)
    {
      if (!info->parent)
        break;

      info = (ObjectInfo*)((ChildInfo*)info->parent)->parent;
      if (!info)
        break;

      GTK_NOTE (BUILDER,
                g_print ("Trying to get internal child %s from %s\n",
                         childname,
                         gtk_buildable_get_name (GTK_BUILDABLE (info->object))));

      if (GTK_IS_BUILDABLE (info->object))
          obj = gtk_buildable_get_internal_child (GTK_BUILDABLE (info->object),
                                                  builder,
                                                  childname);
    };

  if (!obj)
    {
      g_set_error (error,
		   GTK_BUILDER_ERROR,
		   GTK_BUILDER_ERROR_INVALID_VALUE,
		   "Unknown internal child: %s", childname);
    }
  return obj;
}

GObject *
_gtk_builder_construct (GtkBuilder *builder,
                        ObjectInfo *info,
			GError **error)
{
  GArray *parameters, *construct_parameters;
  GType object_type;
  GObject *obj;
  int i;
  GtkBuildableIface *iface;
  gboolean custom_set_property;
  GtkBuildable *buildable;

  g_assert (info->class_name != NULL);
  object_type = gtk_builder_get_type_from_name (builder, info->class_name);
  if (object_type == G_TYPE_INVALID)
    {
      g_set_error (error,
		   GTK_BUILDER_ERROR,
		   GTK_BUILDER_ERROR_INVALID_VALUE,
		   "Invalid object type `%s'",
		   info->class_name);
      return NULL;
    }

  gtk_builder_get_parameters (builder, object_type,
                              info->id,
                              info->properties,
                              &parameters,
                              &construct_parameters);

  if (info->constructor)
    {
      GObject *constructor;

      constructor = gtk_builder_get_object (builder, info->constructor);
      if (constructor == NULL)
	{
	  g_set_error (error,
		       GTK_BUILDER_ERROR,
		       GTK_BUILDER_ERROR_INVALID_VALUE,
		       "Unknown object constructor for %s: %s",
		       info->id,
		       info->constructor);
	  g_array_free (parameters, TRUE);
	  g_array_free (construct_parameters, TRUE);
	  return NULL;
	}
      obj = gtk_buildable_construct_child (GTK_BUILDABLE (constructor),
                                           builder,
                                           info->id);
      g_assert (obj != NULL);
      if (construct_parameters->len)
        g_warning ("Can't pass in construct-only parameters to %s", info->id);
    }
  else if (info->parent && ((ChildInfo*)info->parent)->internal_child != NULL)
    {
      gchar *childname = ((ChildInfo*)info->parent)->internal_child;
      obj = gtk_builder_get_internal_child (builder, info, childname, error);
      if (!obj)
	{
	  g_array_free (parameters, TRUE);
	  g_array_free (construct_parameters, TRUE);
	  return NULL;
	}
      if (construct_parameters->len)
        g_warning ("Can't pass in construct-only parameters to %s", childname);
      g_object_ref (obj);
    }
  else
    {
      obj = g_object_newv (object_type,
                           construct_parameters->len,
                           (GParameter *)construct_parameters->data);

      /* No matter what, make sure we have a reference.
       *
       * If it's an initially unowned object, sink it.
       * If it's not initially unowned then we have the reference already.
       *
       * In the case that this is a window it will be sunk already and
       * this is effectively a call to g_object_ref().  That's what
       * we want.
       */
      if (G_IS_INITIALLY_UNOWNED (obj))
        g_object_ref_sink (obj);

      GTK_NOTE (BUILDER,
                g_print ("created %s of type %s\n", info->id, info->class_name));

      for (i = 0; i < construct_parameters->len; i++)
        {
          GParameter *param = &g_array_index (construct_parameters,
                                              GParameter, i);
          g_value_unset (&param->value);
        }
    }
  g_array_free (construct_parameters, TRUE);

  custom_set_property = FALSE;
  buildable = NULL;
  iface = NULL;
  if (GTK_IS_BUILDABLE (obj))
    {
      buildable = GTK_BUILDABLE (obj);
      iface = GTK_BUILDABLE_GET_IFACE (obj);
      if (iface->set_buildable_property)
        custom_set_property = TRUE;
    }

  for (i = 0; i < parameters->len; i++)
    {
      GParameter *param = &g_array_index (parameters, GParameter, i);
      if (custom_set_property)
        iface->set_buildable_property (buildable, builder, param->name, &param->value);
      else
        g_object_set_property (obj, param->name, &param->value);

#if G_ENABLE_DEBUG
      if (gtk_debug_flags & GTK_DEBUG_BUILDER)
        {
          gchar *str = g_strdup_value_contents ((const GValue*)&param->value);
          g_print ("set %s: %s = %s\n", info->id, param->name, str);
          g_free (str);
        }
#endif      
      g_value_unset (&param->value);
    }
  g_array_free (parameters, TRUE);
  
  if (GTK_IS_BUILDABLE (obj))
    gtk_buildable_set_name (buildable, info->id);
  else
    g_object_set_data_full (obj,
                            "gtk-builder-name",
                            g_strdup (info->id),
                            g_free);

  /* we already own a reference to obj.  put it in the hash table. */
  g_hash_table_insert (builder->priv->objects, g_strdup (info->id), obj);
  
  return obj;
}


void
_gtk_builder_add (GtkBuilder *builder,
                  ChildInfo  *child_info)
{
  GObject *object;
  GObject *parent;

  /* Internal children are already added
   * Also prevent us from being called twice.
   */
  if (!child_info ||
      child_info->internal_child ||
      child_info->added)
    return;

  object = child_info->object;
  if (!object)
    return;

  if (!child_info->parent)
    {
      g_warning ("%s: Not adding, No parent",
                 gtk_buildable_get_name (GTK_BUILDABLE (object)));
      return;
    }

  g_assert (object != NULL);

  parent = ((ObjectInfo*)child_info->parent)->object;
  g_assert (GTK_IS_BUILDABLE (parent));

  GTK_NOTE (BUILDER,
            g_print ("adding %s to %s\n",
                     gtk_buildable_get_name (GTK_BUILDABLE (object)),
                     gtk_buildable_get_name (GTK_BUILDABLE (parent))));
  
  gtk_buildable_add_child (GTK_BUILDABLE (parent), builder, object,
			   child_info->type);

  child_info->added = TRUE;
}

void
_gtk_builder_add_signals (GtkBuilder *builder,
			  GSList     *signals)
{
  builder->priv->signals = g_slist_concat (builder->priv->signals,
                                           g_slist_copy (signals));
}

static void
gtk_builder_apply_delayed_properties (GtkBuilder *builder)
{
  GSList *l, *props;
  DelayedProperty *property;
  GObject *object;
  GType object_type;
  GObjectClass *oclass;
  GParamSpec *pspec;

  /* take the list over from the builder->priv.
   *
   * g_slist_reverse does not copy the list, so the list now
   * belongs to us (and we free it at the end of this function).
   */
  props = g_slist_reverse (builder->priv->delayed_properties);
  builder->priv->delayed_properties = NULL;

  for (l = props; l; l = l->next)
    {
      property = (DelayedProperty*)l->data;
      object = g_hash_table_lookup (builder->priv->objects, property->object);
      g_assert (object != NULL);

      object_type = G_OBJECT_TYPE (object);
      g_assert (object_type != G_TYPE_INVALID);

      oclass = g_type_class_ref (object_type);
      g_assert (oclass != NULL);

      pspec = g_object_class_find_property (G_OBJECT_CLASS (oclass),
                                            property->name);
      if (!pspec)
        g_warning ("Unknown property: %s.%s", g_type_name (object_type),
                   property->name);
      else
        {
          GObject *obj;

          obj = g_hash_table_lookup (builder->priv->objects, property->value);
          if (!obj)
            g_warning ("No object called: %s", property->value);
          else
            g_object_set (object, property->name, obj, NULL);
        }
      g_free (property->value);
      g_free (property->object);
      g_free (property->name);
      g_slice_free (DelayedProperty, property);
      g_type_class_unref (oclass);
    }
  g_slist_free (props);
}

void
_gtk_builder_finish (GtkBuilder *builder)
{
  gtk_builder_apply_delayed_properties (builder);
}

/**
 * gtk_builder_new:
 *
 * Creates a new builder object.
 *
 * Return value: a new #GtkBuilder object
 *
 * Since: 2.12
 **/
GtkBuilder *
gtk_builder_new (void)
{
  return g_object_new (GTK_TYPE_BUILDER, NULL);
}

/**
 * gtk_builder_add_from_file:
 * @builder: a #GtkBuilder
 * @filename: the name of the file to parse
 * @error: (allow-none): return location for an error, or %NULL
 *
 * Parses a file containing a <link linkend="BUILDER-UI">GtkBuilder 
 * UI definition</link> and merges it with the current contents of @builder. 
 * 
 * Upon errors 0 will be returned and @error will be assigned a
 * #GError from the #GTK_BUILDER_ERROR, #G_MARKUP_ERROR or #G_FILE_ERROR 
 * domain.
 *
 * Returns: A positive value on success, 0 if an error occurred
 *
 * Since: 2.12
 **/
guint
gtk_builder_add_from_file (GtkBuilder   *builder,
                           const gchar  *filename,
                           GError      **error)
{
  gchar *buffer;
  gsize length;
  GError *tmp_error;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), 0);
  g_return_val_if_fail (filename != NULL, 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  tmp_error = NULL;

  if (!g_file_get_contents (filename, &buffer, &length, &tmp_error))
    {
      g_propagate_error (error, tmp_error);
      return 0;
    }
  
  g_free (builder->priv->filename);
  builder->priv->filename = g_strdup (filename);

  _gtk_builder_parser_parse_buffer (builder, filename,
                                    buffer, length,
                                    NULL,
                                    &tmp_error);

  g_free (buffer);

  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return 0;
    }

  return 1;
}

/**
 * gtk_builder_add_objects_from_file:
 * @builder: a #GtkBuilder
 * @filename: the name of the file to parse
 * @object_ids: nul-terminated array of objects to build
 * @error: (allow-none): return location for an error, or %NULL
 *
 * Parses a file containing a <link linkend="BUILDER-UI">GtkBuilder 
 * UI definition</link> building only the requested objects and merges
 * them with the current contents of @builder. 
 *
 * Upon errors 0 will be returned and @error will be assigned a
 * #GError from the #GTK_BUILDER_ERROR, #G_MARKUP_ERROR or #G_FILE_ERROR 
 * domain.
 *
 * <note><para>
 * If you are adding an object that depends on an object that is not 
 * its child (for instance a #GtkTreeView that depends on its
 * #GtkTreeModel), you have to explicitely list all of them in @object_ids. 
 * </para></note>
 *
 * Returns: A positive value on success, 0 if an error occurred
 *
 * Since: 2.14
 **/
guint
gtk_builder_add_objects_from_file (GtkBuilder   *builder,
                                   const gchar  *filename,
                                   gchar       **object_ids,
                                   GError      **error)
{
  gchar *buffer;
  gsize length;
  GError *tmp_error;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), 0);
  g_return_val_if_fail (filename != NULL, 0);
  g_return_val_if_fail (object_ids != NULL && object_ids[0] != NULL, 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  tmp_error = NULL;

  if (!g_file_get_contents (filename, &buffer, &length, &tmp_error))
    {
      g_propagate_error (error, tmp_error);
      return 0;
    }
  
  g_free (builder->priv->filename);
  builder->priv->filename = g_strdup (filename);

  _gtk_builder_parser_parse_buffer (builder, filename,
                                    buffer, length,
                                    object_ids,
                                    &tmp_error);

  g_free (buffer);

  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return 0;
    }

  return 1;
}

/**
 * gtk_builder_add_from_string:
 * @builder: a #GtkBuilder
 * @buffer: the string to parse
 * @length: the length of @buffer (may be -1 if @buffer is nul-terminated)
 * @error: (allow-none): return location for an error, or %NULL
 *
 * Parses a string containing a <link linkend="BUILDER-UI">GtkBuilder 
 * UI definition</link> and merges it with the current contents of @builder. 
 *
 * Upon errors 0 will be returned and @error will be assigned a
 * #GError from the #GTK_BUILDER_ERROR or #G_MARKUP_ERROR domain.
 *
 * Returns: A positive value on success, 0 if an error occurred
 *
 * Since: 2.12
 **/
guint
gtk_builder_add_from_string (GtkBuilder   *builder,
                             const gchar  *buffer,
                             gsize         length,
                             GError      **error)
{
  GError *tmp_error;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), 0);
  g_return_val_if_fail (buffer != NULL, 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  tmp_error = NULL;

  g_free (builder->priv->filename);
  builder->priv->filename = g_strdup (".");

  _gtk_builder_parser_parse_buffer (builder, "<input>",
                                    buffer, length,
                                    NULL,
                                    &tmp_error);
  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return 0;
    }

  return 1;
}

/**
 * gtk_builder_add_objects_from_string:
 * @builder: a #GtkBuilder
 * @buffer: the string to parse
 * @length: the length of @buffer (may be -1 if @buffer is nul-terminated)
 * @object_ids: nul-terminated array of objects to build
 * @error: (allow-none): return location for an error, or %NULL
 *
 * Parses a string containing a <link linkend="BUILDER-UI">GtkBuilder 
 * UI definition</link> building only the requested objects and merges
 * them with the current contents of @builder. 
 *
 * Upon errors 0 will be returned and @error will be assigned a
 * #GError from the #GTK_BUILDER_ERROR or #G_MARKUP_ERROR domain.
 * 
 * <note><para>
 * If you are adding an object that depends on an object that is not 
 * its child (for instance a #GtkTreeView that depends on its
 * #GtkTreeModel), you have to explicitely list all of them in @object_ids. 
 * </para></note>
 *
 * Returns: A positive value on success, 0 if an error occurred
 *
 * Since: 2.14
 **/
guint
gtk_builder_add_objects_from_string (GtkBuilder   *builder,
                                     const gchar  *buffer,
                                     gsize         length,
                                     gchar       **object_ids,
                                     GError      **error)
{
  GError *tmp_error;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), 0);
  g_return_val_if_fail (buffer != NULL, 0);
  g_return_val_if_fail (object_ids != NULL && object_ids[0] != NULL, 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  tmp_error = NULL;

  g_free (builder->priv->filename);
  builder->priv->filename = g_strdup (".");

  _gtk_builder_parser_parse_buffer (builder, "<input>",
                                    buffer, length,
                                    object_ids,
                                    &tmp_error);

  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return 0;
    }

  return 1;
}

/**
 * gtk_builder_get_object:
 * @builder: a #GtkBuilder
 * @name: name of object to get
 *
 * Gets the object named @name. Note that this function does not
 * increment the reference count of the returned object. 
 *
 * Return value: (transfer none): the object named @name or %NULL if
 *    it could not be found in the object tree.
 *
 * Since: 2.12
 **/
GObject *
gtk_builder_get_object (GtkBuilder  *builder,
                        const gchar *name)
{
  g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return g_hash_table_lookup (builder->priv->objects, name);
}

static void
object_add_to_list (gchar    *object_id,
                    GObject  *object,
                    GSList  **list)
{
  *list = g_slist_prepend (*list, object);
}

/**
 * gtk_builder_get_objects:
 * @builder: a #GtkBuilder
 *
 * Gets all objects that have been constructed by @builder. Note that 
 * this function does not increment the reference counts of the returned
 * objects.
 *
 * Return value: (element-type GObject) (transfer container): a newly-allocated #GSList containing all the objects
 *   constructed by the #GtkBuilder instance. It should be freed by
 *   g_slist_free()
 *
 * Since: 2.12
 **/
GSList *
gtk_builder_get_objects (GtkBuilder *builder)
{
  GSList *objects = NULL;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);

  g_hash_table_foreach (builder->priv->objects, (GHFunc)object_add_to_list, &objects);

  return g_slist_reverse (objects);
}

/**
 * gtk_builder_set_translation_domain:
 * @builder: a #GtkBuilder
 * @domain: (allow-none): the translation domain or %NULL
 *
 * Sets the translation domain of @builder. 
 * See #GtkBuilder:translation-domain.
 *
 * Since: 2.12
 **/
void
gtk_builder_set_translation_domain (GtkBuilder  *builder,
                                    const gchar *domain)
{
  gchar *new_domain;
    
  g_return_if_fail (GTK_IS_BUILDER (builder));

  new_domain = g_strdup (domain);
  g_free (builder->priv->domain);
  builder->priv->domain = new_domain;

  g_object_notify (G_OBJECT (builder), "translation-domain");
}

/**
 * gtk_builder_get_translation_domain:
 * @builder: a #GtkBuilder
 *
 * Gets the translation domain of @builder.
 *
 * Return value: the translation domain. This string is owned
 * by the builder object and must not be modified or freed.
 *
 * Since: 2.12
 **/
const gchar *
gtk_builder_get_translation_domain (GtkBuilder *builder)
{
  g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);

  return builder->priv->domain;
}

typedef struct {
  GModule *module;
  gpointer data;
} connect_args;

static void
gtk_builder_connect_signals_default (GtkBuilder    *builder,
				     GObject       *object,
				     const gchar   *signal_name,
				     const gchar   *handler_name,
				     GObject       *connect_object,
				     GConnectFlags  flags,
				     gpointer       user_data)
{
  GCallback func;
  connect_args *args = (connect_args*)user_data;
  
  if (!g_module_symbol (args->module, handler_name, (gpointer)&func))
    {
      g_warning ("Could not find signal handler '%s'", handler_name);
      return;
    }

  if (connect_object)
    g_signal_connect_object (object, signal_name, func, connect_object, flags);
  else
    g_signal_connect_data (object, signal_name, func, args->data, NULL, flags);
}


/**
 * gtk_builder_connect_signals:
 * @builder: a #GtkBuilder
 * @user_data: a pointer to a structure sent in as user data to all signals
 *
 * This method is a simpler variation of gtk_builder_connect_signals_full().
 * It uses #GModule's introspective features (by opening the module %NULL) 
 * to look at the application's symbol table. From here it tries to match
 * the signal handler names given in the interface description with
 * symbols in the application and connects the signals.
 * 
 * Note that this function will not work correctly if #GModule is not
 * supported on the platform.
 *
 * When compiling applications for Windows, you must declare signal callbacks
 * with #G_MODULE_EXPORT, or they will not be put in the symbol table.
 * On Linux and Unices, this is not necessary; applications should instead
 * be compiled with the -Wl,--export-dynamic CFLAGS, and linked against
 * gmodule-export-2.0.
 *
 * Since: 2.12
 **/
void
gtk_builder_connect_signals (GtkBuilder *builder,
			     gpointer    user_data)
{
  connect_args *args;
  
  g_return_if_fail (GTK_IS_BUILDER (builder));
  
  if (!g_module_supported ())
    g_error ("gtk_builder_connect_signals() requires working GModule");

  args = g_slice_new0 (connect_args);
  args->module = g_module_open (NULL, G_MODULE_BIND_LAZY);
  args->data = user_data;
  
  gtk_builder_connect_signals_full (builder,
                                    gtk_builder_connect_signals_default,
                                    args);
  g_module_close (args->module);

  g_slice_free (connect_args, args);
}

/**
 * GtkBuilderConnectFunc:
 * @builder: a #GtkBuilder
 * @object: object to connect a signal to
 * @signal_name: name of the signal
 * @handler_name: name of the handler
 * @connect_object: a #GObject, if non-%NULL, use g_signal_connect_object()
 * @flags: #GConnectFlags to use
 * @user_data: user data
 *
 * This is the signature of a function used to connect signals.  It is used
 * by the gtk_builder_connect_signals() and gtk_builder_connect_signals_full()
 * methods.  It is mainly intended for interpreted language bindings, but
 * could be useful where the programmer wants more control over the signal
 * connection process.
 *
 * Since: 2.12
 */

/**
 * gtk_builder_connect_signals_full:
 * @builder: a #GtkBuilder
 * @func: (scope call): the function used to connect the signals
 * @user_data: arbitrary data that will be passed to the connection function
 *
 * This function can be thought of the interpreted language binding
 * version of gtk_builder_connect_signals(), except that it does not
 * require GModule to function correctly.
 *
 * Since: 2.12
 */
void
gtk_builder_connect_signals_full (GtkBuilder            *builder,
                                  GtkBuilderConnectFunc  func,
                                  gpointer               user_data)
{
  GSList *l;
  GObject *object;
  GObject *connect_object;
  
  g_return_if_fail (GTK_IS_BUILDER (builder));
  g_return_if_fail (func != NULL);
  
  if (!builder->priv->signals)
    return;

  builder->priv->signals = g_slist_reverse (builder->priv->signals);
  for (l = builder->priv->signals; l; l = l->next)
    {
      SignalInfo *signal = (SignalInfo*)l->data;

      g_assert (signal != NULL);
      g_assert (signal->name != NULL);

      object = g_hash_table_lookup (builder->priv->objects,
				    signal->object_name);
      g_assert (object != NULL);

      connect_object = NULL;
      
      if (signal->connect_object_name)
	{
	  connect_object = g_hash_table_lookup (builder->priv->objects,
						signal->connect_object_name);
	  if (!connect_object)
	      g_warning ("Could not lookup object %s on signal %s of object %s",
			 signal->connect_object_name, signal->name,
			 signal->object_name);
	}
						  
      func (builder, object, signal->name, signal->handler, 
	    connect_object, signal->flags, user_data);
    }

  g_slist_foreach (builder->priv->signals, (GFunc)_free_signal_info, NULL);
  g_slist_free (builder->priv->signals);
  builder->priv->signals = NULL;
}

/**
 * gtk_builder_value_from_string:
 * @builder: a #GtkBuilder
 * @pspec: the #GParamSpec for the property
 * @string: the string representation of the value
 * @value: (out): the #GValue to store the result in
 * @error: (allow-none): return location for an error, or %NULL
 *
 * This function demarshals a value from a string. This function
 * calls g_value_init() on the @value argument, so it need not be
 * initialised beforehand.
 *
 * This function can handle char, uchar, boolean, int, uint, long,
 * ulong, enum, flags, float, double, string, #GdkColor and
 * #GtkAdjustment type values. Support for #GtkWidget type values is
 * still to come.
 *
 * Upon errors %FALSE will be returned and @error will be assigned a
 * #GError from the #GTK_BUILDER_ERROR domain.
 *
 * Returns: %TRUE on success
 *
 * Since: 2.12
 */
gboolean
gtk_builder_value_from_string (GtkBuilder   *builder,
			       GParamSpec   *pspec,
                               const gchar  *string,
                               GValue       *value,
			       GError      **error)
{
  g_return_val_if_fail (GTK_IS_BUILDER (builder), FALSE);
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (string != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /*
   * GParamSpecUnichar has the internal type G_TYPE_UINT,
   * so we cannot handle this in the switch, do it separately
   */
  if (G_IS_PARAM_SPEC_UNICHAR (pspec))
    {
      gunichar c;
      g_value_init (value, G_TYPE_UINT);
      c = g_utf8_get_char_validated (string, strlen (string));
      if (c > 0)
        g_value_set_uint (value, c);
      return TRUE;
    }

  return gtk_builder_value_from_string_type (builder,
					     G_PARAM_SPEC_VALUE_TYPE (pspec),
                                             string, value, error);
}

/**
 * gtk_builder_value_from_string_type:
 * @builder: a #GtkBuilder
 * @type: the #GType of the value
 * @string: the string representation of the value
 * @value: (out): the #GValue to store the result in
 * @error: (allow-none): return location for an error, or %NULL
 *
 * Like gtk_builder_value_from_string(), this function demarshals 
 * a value from a string, but takes a #GType instead of #GParamSpec.
 * This function calls g_value_init() on the @value argument, so it 
 * need not be initialised beforehand.
 *
 * Upon errors %FALSE will be returned and @error will be assigned a
 * #GError from the #GTK_BUILDER_ERROR domain.
 *
 * Returns: %TRUE on success
 *
 * Since: 2.12
 */
gboolean
gtk_builder_value_from_string_type (GtkBuilder   *builder,
				    GType         type,
                                    const gchar  *string,
                                    GValue       *value,
				    GError      **error)
{
  gboolean ret = TRUE;

  g_return_val_if_fail (type != G_TYPE_INVALID, FALSE);
  g_return_val_if_fail (string != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  g_value_init (value, type);

  switch (G_TYPE_FUNDAMENTAL (type))
    {
    case G_TYPE_CHAR:
      g_value_set_char (value, string[0]);
      break;
    case G_TYPE_UCHAR:
      g_value_set_uchar (value, (guchar)string[0]);
      break;
    case G_TYPE_BOOLEAN:
      {
        gboolean b;

	if (!_gtk_builder_boolean_from_string (string, &b, error))
	  {
	    ret = FALSE;
	    break;
          }
        g_value_set_boolean (value, b);
        break;
      }
    case G_TYPE_INT:
    case G_TYPE_LONG:
      {
        long l;
        gchar *endptr;
        errno = 0;
        l = strtol (string, &endptr, 0);
        if (errno || endptr == string)
          {
	    g_set_error (error,
			 GTK_BUILDER_ERROR,
			 GTK_BUILDER_ERROR_INVALID_VALUE,
			 "Could not parse integer `%s'",
			 string);
            ret = FALSE;
            break;
          }
        if (G_VALUE_HOLDS_INT (value))
          g_value_set_int (value, l);
        else
          g_value_set_long (value, l);
        break;
      }
    case G_TYPE_UINT:
    case G_TYPE_ULONG:
      {
        gulong ul;
        gchar *endptr;
        errno = 0;
        ul = strtoul (string, &endptr, 0);
        if (errno || endptr == string)
          {
	    g_set_error (error,
			 GTK_BUILDER_ERROR,
			 GTK_BUILDER_ERROR_INVALID_VALUE,
			 "Could not parse unsigned integer `%s'",
			 string);
            ret = FALSE;
            break;
          }
        if (G_VALUE_HOLDS_UINT (value))
          g_value_set_uint (value, ul);
        else 
          g_value_set_ulong (value, ul);
        break;
      }
    case G_TYPE_ENUM:
      {
	gint enum_value;
	if (!_gtk_builder_enum_from_string (type, string, &enum_value, error))
	  {
	    ret = FALSE;
	    break;
          }
	g_value_set_enum (value, enum_value);
	break;
      }
    case G_TYPE_FLAGS:
      {
	guint flags_value;

	if (!_gtk_builder_flags_from_string (type, string, &flags_value, error))
	  {
	    ret = FALSE;
	    break;
          }
	g_value_set_flags (value, flags_value);
	break;
      }
    case G_TYPE_FLOAT:
    case G_TYPE_DOUBLE:
      {
        gdouble d;
        gchar *endptr;
        errno = 0;
        d = g_ascii_strtod (string, &endptr);
        if (errno || endptr == string)
          {
	    g_set_error (error,
			 GTK_BUILDER_ERROR,
			 GTK_BUILDER_ERROR_INVALID_VALUE,
			 "Could not parse double `%s'",
			 string);
            ret = FALSE;
            break;
          }
        if (G_VALUE_HOLDS_FLOAT (value))
          g_value_set_float (value, d);
        else
          g_value_set_double (value, d);
        break;
      }
    case G_TYPE_STRING:
      g_value_set_string (value, string);
      break;
    case G_TYPE_BOXED:
      if (G_VALUE_HOLDS (value, GDK_TYPE_COLOR))
        {
          GdkColor colour = { 0, };

          if (gdk_color_parse (string, &colour) &&
              gdk_colormap_alloc_color (gtk_widget_get_default_colormap (),
                                        &colour, FALSE, TRUE))
            g_value_set_boxed (value, &colour);
          else
            {
	      g_set_error (error,
			   GTK_BUILDER_ERROR,
			   GTK_BUILDER_ERROR_INVALID_VALUE,
			   "Could not parse color `%s'",
			   string);
              ret = FALSE;
            }
        }
      else if (G_VALUE_HOLDS (value, G_TYPE_STRV))
        {
          gchar **vector = g_strsplit (string, "\n", 0);
          g_value_take_boxed (value, vector);
        }
      else
        {
          g_set_error (error,
                       GTK_BUILDER_ERROR,
                       GTK_BUILDER_ERROR_INVALID_VALUE,
                       "Could not parse '%s' as a %s",
                       string, G_VALUE_TYPE_NAME (value));
          ret = FALSE;
        }
      break;
    case G_TYPE_OBJECT:
      if (G_VALUE_HOLDS (value, GDK_TYPE_PIXBUF))
        {
          gchar *filename;
          GError *tmp_error = NULL;
          GdkPixbuf *pixbuf;
       
          if (gtk_builder_get_object (builder, string))
            {
              g_set_error (error,
                           GTK_BUILDER_ERROR,
                           GTK_BUILDER_ERROR_INVALID_VALUE,
                           "Could not load image '%s': "
                           " '%s' is already used as object id",
                           string, string);
              return FALSE;
            }

	  filename = _gtk_builder_get_absolute_filename (builder, string);
          pixbuf = gdk_pixbuf_new_from_file (filename, &tmp_error);

          if (pixbuf == NULL)
            {
              GtkIconTheme *theme;

              g_warning ("Could not load image '%s': %s", 
                         string, tmp_error->message);
              g_error_free (tmp_error);

              /* fall back to a missing image */
              theme = gtk_icon_theme_get_default ();
              pixbuf = gtk_icon_theme_load_icon (theme, 
                                                 GTK_STOCK_MISSING_IMAGE,
                                                 16,
                                                 GTK_ICON_LOOKUP_USE_BUILTIN,
                                                 NULL);
            }
 
          if (pixbuf)
            {
              g_value_set_object (value, pixbuf);
              g_object_unref (G_OBJECT (pixbuf));
            }

          g_free (filename);

          ret = TRUE;
        }
      else
        ret = FALSE;
      break;
    default:
      ret = FALSE;
      break;
    }
 
  /* Catch unassigned error for object types as well as any unsupported types.
   * While parsing GtkBuilder; object types are deserialized
   * without calling gtk_builder_value_from_string_type().
   */
  if (!ret && error && *error == NULL) 
    g_set_error (error,
		 GTK_BUILDER_ERROR,
		 GTK_BUILDER_ERROR_INVALID_VALUE,
		 "Unsupported GType `%s'", g_type_name (type));

  return ret;
}

gboolean
_gtk_builder_enum_from_string (GType         type, 
                               const gchar  *string,
			       gint         *enum_value,
			       GError      **error)
{
  GEnumClass *eclass;
  GEnumValue *ev;
  gchar *endptr;
  gint value;
  gboolean ret;
  
  g_return_val_if_fail (G_TYPE_IS_ENUM (type), FALSE);
  g_return_val_if_fail (string != NULL, FALSE);
  
  ret = TRUE;

  value = strtoul (string, &endptr, 0);
  if (endptr != string) /* parsed a number */
    *enum_value = value;
  else
    {
      eclass = g_type_class_ref (type);
      ev = g_enum_get_value_by_name (eclass, string);
      if (!ev)
	ev = g_enum_get_value_by_nick (eclass, string);

      if (ev)
	*enum_value = ev->value;
      else
	{
	  g_set_error (error,
		       GTK_BUILDER_ERROR,
		       GTK_BUILDER_ERROR_INVALID_VALUE,
		       "Could not parse enum: `%s'",
		       string);
	  ret = FALSE;
	}
      
      g_type_class_unref (eclass);
    }
  
  return ret;
}

gboolean
_gtk_builder_flags_from_string (GType         type, 
                                const gchar  *string,
				guint        *flags_value,
				GError      **error)
{
  GFlagsClass *fclass;
  gchar *endptr, *prevptr;
  guint i, j, value;
  gchar *flagstr;
  GFlagsValue *fv;
  const gchar *flag;
  gunichar ch;
  gboolean eos, ret;

  g_return_val_if_fail (G_TYPE_IS_FLAGS (type), FALSE);
  g_return_val_if_fail (string != 0, FALSE);

  ret = TRUE;
  
  value = strtoul (string, &endptr, 0);
  if (endptr != string) /* parsed a number */
    *flags_value = value;
  else
    {
      fclass = g_type_class_ref (type);

      flagstr = g_strdup (string);
      for (value = i = j = 0; ; i++)
	{
	  
	  eos = flagstr[i] == '\0';
	  
	  if (!eos && flagstr[i] != '|')
	    continue;
	  
	  flag = &flagstr[j];
	  endptr = &flagstr[i];
	  
	  if (!eos)
	    {
	      flagstr[i++] = '\0';
	      j = i;
	    }
	  
	  /* trim spaces */
	  for (;;)
	    {
	      ch = g_utf8_get_char (flag);
	      if (!g_unichar_isspace (ch))
		break;
	      flag = g_utf8_next_char (flag);
	    }
	  
	  while (endptr > flag)
	    {
	      prevptr = g_utf8_prev_char (endptr);
	      ch = g_utf8_get_char (prevptr);
	      if (!g_unichar_isspace (ch))
		break;
	      endptr = prevptr;
	    }
	  
	  if (endptr > flag)
	    {
	      *endptr = '\0';
	      fv = g_flags_get_value_by_name (fclass, flag);
	      
	      if (!fv)
		fv = g_flags_get_value_by_nick (fclass, flag);
	      
	      if (fv)
		value |= fv->value;
	      else
		{
		  g_set_error (error,
			       GTK_BUILDER_ERROR,
			       GTK_BUILDER_ERROR_INVALID_VALUE,
			       "Unknown flag: `%s'",
			       flag);
		  ret = FALSE;
		  break;
		}
	    }
	  
	  if (eos)
	    {
	      *flags_value = value;
	      break;
	    }
	}
      
      g_free (flagstr);
      
      g_type_class_unref (fclass);
    }

  return ret;
}

/**
 * gtk_builder_get_type_from_name:
 * @builder: a #GtkBuilder
 * @type_name: type name to lookup
 *
 * Looks up a type by name, using the virtual function that 
 * #GtkBuilder has for that purpose. This is mainly used when
 * implementing the #GtkBuildable interface on a type.
 *
 * Returns: the #GType found for @type_name or #G_TYPE_INVALID 
 *   if no type was found
 *
 * Since: 2.12
 */
GType
gtk_builder_get_type_from_name (GtkBuilder  *builder, 
                                const gchar *type_name)
{
  g_return_val_if_fail (GTK_IS_BUILDER (builder), G_TYPE_INVALID);
  g_return_val_if_fail (type_name != NULL, G_TYPE_INVALID);

  return GTK_BUILDER_GET_CLASS (builder)->get_type_from_name (builder, type_name);
}

GQuark
gtk_builder_error_quark (void)
{
  return g_quark_from_static_string ("gtk-builder-error-quark");
}

gchar *
_gtk_builder_get_absolute_filename (GtkBuilder *builder, const gchar *string)
{
  gchar *filename;
  gchar *dirname = NULL;
  
  if (g_path_is_absolute (string))
    return g_strdup (string);

  if (builder->priv->filename &&
      strcmp (builder->priv->filename, ".") != 0) 
    {
      dirname = g_path_get_dirname (builder->priv->filename);

      if (strcmp (dirname, ".") == 0)
	{
	  g_free (dirname);
	  dirname = g_get_current_dir ();
	}
    }
  else
    dirname = g_get_current_dir ();
    
  filename = g_build_filename (dirname, string, NULL);
  g_free (dirname);
  
  return filename;
}

#define __GTK_BUILDER_C__
#include "gtkaliasdef.c"
