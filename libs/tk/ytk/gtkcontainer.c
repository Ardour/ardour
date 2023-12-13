/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "gtkcontainer.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkprivate.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkwindow.h"
#include "gtkintl.h"
#include "gtktoolbar.h"
#include <gobject/gobjectnotifyqueue.c>
#include <gobject/gvaluecollector.h>
#include "gtkalias.h"


enum {
  ADD,
  REMOVE,
  CHECK_RESIZE,
  SET_FOCUS_CHILD,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_BORDER_WIDTH,
  PROP_RESIZE_MODE,
  PROP_CHILD
};

#define PARAM_SPEC_PARAM_ID(pspec)              ((pspec)->param_id)
#define PARAM_SPEC_SET_PARAM_ID(pspec, id)      ((pspec)->param_id = (id))


/* --- prototypes --- */
static void     gtk_container_base_class_init      (GtkContainerClass *klass);
static void     gtk_container_base_class_finalize  (GtkContainerClass *klass);
static void     gtk_container_class_init           (GtkContainerClass *klass);
static void     gtk_container_init                 (GtkContainer      *container);
static void     gtk_container_destroy              (GtkObject         *object);
static void     gtk_container_set_property         (GObject         *object,
						    guint            prop_id,
						    const GValue    *value,
						    GParamSpec      *pspec);
static void     gtk_container_get_property         (GObject         *object,
						    guint            prop_id,
						    GValue          *value,
						    GParamSpec      *pspec);
static void     gtk_container_add_unimplemented    (GtkContainer      *container,
						    GtkWidget         *widget);
static void     gtk_container_remove_unimplemented (GtkContainer      *container,
						    GtkWidget         *widget);
static void     gtk_container_real_check_resize    (GtkContainer      *container);
static gboolean gtk_container_focus                (GtkWidget         *widget,
						    GtkDirectionType   direction);
static void     gtk_container_real_set_focus_child (GtkContainer      *container,
						    GtkWidget         *widget);

static gboolean gtk_container_focus_move           (GtkContainer      *container,
						    GList             *children,
						    GtkDirectionType   direction);
static void     gtk_container_children_callback    (GtkWidget         *widget,
						    gpointer           client_data);
static void     gtk_container_show_all             (GtkWidget         *widget);
static void     gtk_container_hide_all             (GtkWidget         *widget);
static gint     gtk_container_expose               (GtkWidget         *widget,
						    GdkEventExpose    *event);
static void     gtk_container_map                  (GtkWidget         *widget);
static void     gtk_container_unmap                (GtkWidget         *widget);

static gchar* gtk_container_child_default_composite_name (GtkContainer *container,
							  GtkWidget    *child);

/* GtkBuildable */
static void gtk_container_buildable_init           (GtkBuildableIface *iface);
static void gtk_container_buildable_add_child      (GtkBuildable *buildable,
						    GtkBuilder   *builder,
						    GObject      *child,
						    const gchar  *type);
static gboolean gtk_container_buildable_custom_tag_start (GtkBuildable  *buildable,
							  GtkBuilder    *builder,
							  GObject       *child,
							  const gchar   *tagname,
							  GMarkupParser *parser,
							  gpointer      *data);
static void    gtk_container_buildable_custom_tag_end (GtkBuildable *buildable,
						       GtkBuilder   *builder,
						       GObject      *child,
						       const gchar  *tagname,
						       gpointer     *data);


/* --- variables --- */
static const gchar           vadjustment_key[] = "gtk-vadjustment";
static guint                 vadjustment_key_id = 0;
static const gchar           hadjustment_key[] = "gtk-hadjustment";
static guint                 hadjustment_key_id = 0;
static GSList	            *container_resize_queue = NULL;
static guint                 container_signals[LAST_SIGNAL] = { 0 };
static GtkWidgetClass       *parent_class = NULL;
extern GParamSpecPool       *_gtk_widget_child_property_pool;
extern GObjectNotifyContext *_gtk_widget_child_property_notify_context;
static GtkBuildableIface    *parent_buildable_iface;


/* --- functions --- */
GType
gtk_container_get_type (void)
{
  static GType container_type = 0;

  if (!container_type)
    {
      const GTypeInfo container_info =
      {
	sizeof (GtkContainerClass),
	(GBaseInitFunc) gtk_container_base_class_init,
	(GBaseFinalizeFunc) gtk_container_base_class_finalize,
	(GClassInitFunc) gtk_container_class_init,
	NULL        /* class_finalize */,
	NULL        /* class_data */,
	sizeof (GtkContainer),
	0           /* n_preallocs */,
	(GInstanceInitFunc) gtk_container_init,
	NULL,       /* value_table */
      };

      const GInterfaceInfo buildable_info =
      {
	(GInterfaceInitFunc) gtk_container_buildable_init,
	NULL,
	NULL
      };

      container_type =
	g_type_register_static (GTK_TYPE_WIDGET, I_("GtkContainer"), 
				&container_info, G_TYPE_FLAG_ABSTRACT);

      g_type_add_interface_static (container_type,
				   GTK_TYPE_BUILDABLE,
				   &buildable_info);

    }

  return container_type;
}

static void
gtk_container_base_class_init (GtkContainerClass *class)
{
  /* reset instance specifc class fields that don't get inherited */
  class->set_child_property = NULL;
  class->get_child_property = NULL;
}

static void
gtk_container_base_class_finalize (GtkContainerClass *class)
{
  GList *list, *node;

  list = g_param_spec_pool_list_owned (_gtk_widget_child_property_pool, G_OBJECT_CLASS_TYPE (class));
  for (node = list; node; node = node->next)
    {
      GParamSpec *pspec = node->data;

      g_param_spec_pool_remove (_gtk_widget_child_property_pool, pspec);
      PARAM_SPEC_SET_PARAM_ID (pspec, 0);
      g_param_spec_unref (pspec);
    }
  g_list_free (list);
}

static void
gtk_container_class_init (GtkContainerClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  vadjustment_key_id = g_quark_from_static_string (vadjustment_key);
  hadjustment_key_id = g_quark_from_static_string (hadjustment_key);
  
  gobject_class->set_property = gtk_container_set_property;
  gobject_class->get_property = gtk_container_get_property;

  object_class->destroy = gtk_container_destroy;

  widget_class->show_all = gtk_container_show_all;
  widget_class->hide_all = gtk_container_hide_all;
  widget_class->expose_event = gtk_container_expose;
  widget_class->map = gtk_container_map;
  widget_class->unmap = gtk_container_unmap;
  widget_class->focus = gtk_container_focus;
  
  class->add = gtk_container_add_unimplemented;
  class->remove = gtk_container_remove_unimplemented;
  class->check_resize = gtk_container_real_check_resize;
  class->forall = NULL;
  class->set_focus_child = gtk_container_real_set_focus_child;
  class->child_type = NULL;
  class->composite_name = gtk_container_child_default_composite_name;

  g_object_class_install_property (gobject_class,
                                   PROP_RESIZE_MODE,
                                   g_param_spec_enum ("resize-mode",
                                                      P_("Resize mode"),
                                                      P_("Specify how resize events are handled"),
                                                      GTK_TYPE_RESIZE_MODE,
                                                      GTK_RESIZE_PARENT,
                                                      GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_BORDER_WIDTH,
                                   g_param_spec_uint ("border-width",
                                                      P_("Border width"),
                                                      P_("The width of the empty border outside the containers children"),
						      0,
						      65535,
						      0,
                                                      GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_CHILD,
                                   g_param_spec_object ("child",
                                                      P_("Child"),
                                                      P_("Can be used to add a new child to the container"),
                                                      GTK_TYPE_WIDGET,
						      GTK_PARAM_WRITABLE));
  container_signals[ADD] =
    g_signal_new (I_("add"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkContainerClass, add),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_WIDGET);
  container_signals[REMOVE] =
    g_signal_new (I_("remove"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkContainerClass, remove),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_WIDGET);
  container_signals[CHECK_RESIZE] =
    g_signal_new (I_("check-resize"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkContainerClass, check_resize),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  container_signals[SET_FOCUS_CHILD] =
    g_signal_new (I_("set-focus-child"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkContainerClass, set_focus_child),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_WIDGET);
}

static void
gtk_container_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = gtk_container_buildable_add_child;
  iface->custom_tag_start = gtk_container_buildable_custom_tag_start;
  iface->custom_tag_end = gtk_container_buildable_custom_tag_end;
}

static void
gtk_container_buildable_add_child (GtkBuildable  *buildable,
				   GtkBuilder    *builder,
				   GObject       *child,
				   const gchar   *type)
{
  if (type)
    {
      GTK_BUILDER_WARN_INVALID_CHILD_TYPE (buildable, type);
    }
  else if (GTK_IS_WIDGET (child) && GTK_WIDGET (child)->parent == NULL)
    {
      gtk_container_add (GTK_CONTAINER (buildable), GTK_WIDGET (child));
    }
  else
    g_warning ("Cannot add an object of type %s to a container of type %s", 
	       g_type_name (G_OBJECT_TYPE (child)), g_type_name (G_OBJECT_TYPE (buildable)));
}

static void
gtk_container_buildable_set_child_property (GtkContainer *container,
					    GtkBuilder   *builder,
					    GtkWidget    *child,
					    gchar        *name,
					    const gchar  *value)
{
  GParamSpec *pspec;
  GValue gvalue = { 0, };
  GError *error = NULL;
  
  pspec = gtk_container_class_find_child_property
    (G_OBJECT_GET_CLASS (container), name);
  if (!pspec)
    {
      g_warning ("%s does not have a property called %s",
		 g_type_name (G_OBJECT_TYPE (container)), name);
      return;
    }

  if (!gtk_builder_value_from_string (builder, pspec, value, &gvalue, &error))
    {
      g_warning ("Could not read property %s:%s with value %s of type %s: %s",
		 g_type_name (G_OBJECT_TYPE (container)),
		 name,
		 value,
		 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
		 error->message);
      g_error_free (error);
      return;
    }

  gtk_container_child_set_property (container, child, name, &gvalue);
  g_value_unset (&gvalue);
}

typedef struct {
  GtkBuilder   *builder;
  GtkContainer *container;
  GtkWidget    *child;
  gchar        *child_prop_name;
  gchar        *context;
  gboolean     translatable;
} PackingPropertiesData;

static void
attributes_start_element (GMarkupParseContext *context,
			  const gchar         *element_name,
			  const gchar        **names,
			  const gchar        **values,
			  gpointer             user_data,
			  GError             **error)
{
  PackingPropertiesData *parser_data = (PackingPropertiesData*)user_data;
  guint i;

  if (strcmp (element_name, "property") == 0)
    {
      for (i = 0; names[i]; i++)
	if (strcmp (names[i], "name") == 0)
	  parser_data->child_prop_name = g_strdup (values[i]);
	else if (strcmp (names[i], "translatable") == 0)
	  {
	    if (!_gtk_builder_boolean_from_string (values[1],
						   &parser_data->translatable,
						   error))
	      return;
	  }
	else if (strcmp (names[i], "comments") == 0)
	  ; /* for translators */
	else if (strcmp (names[i], "context") == 0)
	  parser_data->context = g_strdup (values[1]);
	else
	  g_warning ("Unsupported attribute for GtkContainer Child "
		     "property: %s\n", names[i]);
    }
  else if (strcmp (element_name, "packing") == 0)
    return;
  else
    g_warning ("Unsupported tag for GtkContainer: %s\n", element_name);
}

static void
attributes_text_element (GMarkupParseContext *context,
			 const gchar         *text,
			 gsize                text_len,
			 gpointer             user_data,
			 GError             **error)
{
  PackingPropertiesData *parser_data = (PackingPropertiesData*)user_data;
  gchar* value;

  if (!parser_data->child_prop_name)
    return;
  
  if (parser_data->translatable && text_len)
    {
      const gchar* domain;
      domain = gtk_builder_get_translation_domain (parser_data->builder);
      
      value = _gtk_builder_parser_translate (domain,
					     parser_data->context,
					     text);
    }
  else
    {
      value = g_strdup (text);
    }

  gtk_container_buildable_set_child_property (parser_data->container,
					      parser_data->builder,
					      parser_data->child,
					      parser_data->child_prop_name,
					      value);

  g_free (parser_data->child_prop_name);
  g_free (parser_data->context);
  g_free (value);
  parser_data->child_prop_name = NULL;
  parser_data->context = NULL;
  parser_data->translatable = FALSE;
}

static const GMarkupParser attributes_parser =
  {
    attributes_start_element,
    NULL,
    attributes_text_element,
  };

static gboolean
gtk_container_buildable_custom_tag_start (GtkBuildable  *buildable,
					  GtkBuilder    *builder,
					  GObject       *child,
					  const gchar   *tagname,
					  GMarkupParser *parser,
					  gpointer      *data)
{
  PackingPropertiesData *parser_data;

  if (parent_buildable_iface->custom_tag_start (buildable, builder, child,
						tagname, parser, data))
    return TRUE;

  if (child && strcmp (tagname, "packing") == 0)
    {
      parser_data = g_slice_new0 (PackingPropertiesData);
      parser_data->builder = builder;
      parser_data->container = GTK_CONTAINER (buildable);
      parser_data->child = GTK_WIDGET (child);
      parser_data->child_prop_name = NULL;

      *parser = attributes_parser;
      *data = parser_data;
      return TRUE;
    }

  return FALSE;
}

static void
gtk_container_buildable_custom_tag_end (GtkBuildable *buildable,
					GtkBuilder   *builder,
					GObject      *child,
					const gchar  *tagname,
					gpointer     *data)
{
  if (strcmp (tagname, "packing") == 0)
    {
      g_slice_free (PackingPropertiesData, (gpointer)data);
      return;

    }

  if (parent_buildable_iface->custom_tag_end)
    parent_buildable_iface->custom_tag_end (buildable, builder,
					    child, tagname, data);

}

/**
 * gtk_container_child_type: 
 * @container: a #GtkContainer
 *
 * Returns the type of the children supported by the container.
 *
 * Note that this may return %G_TYPE_NONE to indicate that no more
 * children can be added, e.g. for a #GtkPaned which already has two 
 * children.
 *
 * Return value: a #GType.
 **/
GType
gtk_container_child_type (GtkContainer *container)
{
  GType slot;
  GtkContainerClass *class;

  g_return_val_if_fail (GTK_IS_CONTAINER (container), 0);

  class = GTK_CONTAINER_GET_CLASS (container);
  if (class->child_type)
    slot = class->child_type (container);
  else
    slot = G_TYPE_NONE;

  return slot;
}

/* --- GtkContainer child property mechanism --- */
static inline void
container_get_child_property (GtkContainer *container,
			      GtkWidget    *child,
			      GParamSpec   *pspec,
			      GValue       *value)
{
  GtkContainerClass *class = g_type_class_peek (pspec->owner_type);
  
  class->get_child_property (container, child, PARAM_SPEC_PARAM_ID (pspec), value, pspec);
}

static inline void
container_set_child_property (GtkContainer       *container,
			      GtkWidget		 *child,
			      GParamSpec         *pspec,
			      const GValue       *value,
			      GObjectNotifyQueue *nqueue)
{
  GValue tmp_value = { 0, };
  GtkContainerClass *class = g_type_class_peek (pspec->owner_type);

  /* provide a copy to work from, convert (if necessary) and validate */
  g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  if (!g_value_transform (value, &tmp_value))
    g_warning ("unable to set child property `%s' of type `%s' from value of type `%s'",
	       pspec->name,
	       g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
	       G_VALUE_TYPE_NAME (value));
  else if (g_param_value_validate (pspec, &tmp_value) && !(pspec->flags & G_PARAM_LAX_VALIDATION))
    {
      gchar *contents = g_strdup_value_contents (value);

      g_warning ("value \"%s\" of type `%s' is invalid for property `%s' of type `%s'",
		 contents,
		 G_VALUE_TYPE_NAME (value),
		 pspec->name,
		 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
      g_free (contents);
    }
  else
    {
      class->set_child_property (container, child, PARAM_SPEC_PARAM_ID (pspec), &tmp_value, pspec);
      g_object_notify_queue_add (G_OBJECT (child), nqueue, pspec);
    }
  g_value_unset (&tmp_value);
}

/**
 * gtk_container_child_get_valist:
 * @container: a #GtkContainer
 * @child: a widget which is a child of @container
 * @first_property_name: the name of the first property to get
 * @var_args: return location for the first property, followed 
 *     optionally by more name/return location pairs, followed by %NULL
 * 
 * Gets the values of one or more child properties for @child and @container.
 **/
void
gtk_container_child_get_valist (GtkContainer *container,
				GtkWidget    *child,
				const gchar  *first_property_name,
				va_list       var_args)
{
  const gchar *name;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent == GTK_WIDGET (container));

  g_object_ref (container);
  g_object_ref (child);

  name = first_property_name;
  while (name)
    {
      GValue value = { 0, };
      GParamSpec *pspec;
      gchar *error;

      pspec = g_param_spec_pool_lookup (_gtk_widget_child_property_pool,
					name,
					G_OBJECT_TYPE (container),
					TRUE);
      if (!pspec)
	{
	  g_warning ("%s: container class `%s' has no child property named `%s'",
		     G_STRLOC,
		     G_OBJECT_TYPE_NAME (container),
		     name);
	  break;
	}
      if (!(pspec->flags & G_PARAM_READABLE))
	{
	  g_warning ("%s: child property `%s' of container class `%s' is not readable",
		     G_STRLOC,
		     pspec->name,
		     G_OBJECT_TYPE_NAME (container));
	  break;
	}
      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      container_get_child_property (container, child, pspec, &value);
      G_VALUE_LCOPY (&value, var_args, 0, &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);
	  g_value_unset (&value);
	  break;
	}
      g_value_unset (&value);
      name = va_arg (var_args, gchar*);
    }

  g_object_unref (child);
  g_object_unref (container);
}

/**
 * gtk_container_child_get_property:
 * @container: a #GtkContainer
 * @child: a widget which is a child of @container
 * @property_name: the name of the property to get
 * @value: a location to return the value
 * 
 * Gets the value of a child property for @child and @container.
 **/
void
gtk_container_child_get_property (GtkContainer *container,
				  GtkWidget    *child,
				  const gchar  *property_name,
				  GValue       *value)
{
  GParamSpec *pspec;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent == GTK_WIDGET (container));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (G_IS_VALUE (value));
  
  g_object_ref (container);
  g_object_ref (child);
  pspec = g_param_spec_pool_lookup (_gtk_widget_child_property_pool, property_name,
				    G_OBJECT_TYPE (container), TRUE);
  if (!pspec)
    g_warning ("%s: container class `%s' has no child property named `%s'",
	       G_STRLOC,
	       G_OBJECT_TYPE_NAME (container),
	       property_name);
  else if (!(pspec->flags & G_PARAM_READABLE))
    g_warning ("%s: child property `%s' of container class `%s' is not readable",
	       G_STRLOC,
	       pspec->name,
	       G_OBJECT_TYPE_NAME (container));
  else
    {
      GValue *prop_value, tmp_value = { 0, };

      /* auto-conversion of the callers value type
       */
      if (G_VALUE_TYPE (value) == G_PARAM_SPEC_VALUE_TYPE (pspec))
	{
	  g_value_reset (value);
	  prop_value = value;
	}
      else if (!g_value_type_transformable (G_PARAM_SPEC_VALUE_TYPE (pspec), G_VALUE_TYPE (value)))
	{
	  g_warning ("can't retrieve child property `%s' of type `%s' as value of type `%s'",
		     pspec->name,
		     g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
		     G_VALUE_TYPE_NAME (value));
	  g_object_unref (child);
	  g_object_unref (container);
	  return;
	}
      else
	{
	  g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
	  prop_value = &tmp_value;
	}
      container_get_child_property (container, child, pspec, prop_value);
      if (prop_value != value)
	{
	  g_value_transform (prop_value, value);
	  g_value_unset (&tmp_value);
	}
    }
  g_object_unref (child);
  g_object_unref (container);
}

/**
 * gtk_container_child_set_valist:
 * @container: a #GtkContainer
 * @child: a widget which is a child of @container
 * @first_property_name: the name of the first property to set
 * @var_args: a %NULL-terminated list of property names and values, starting
 *           with @first_prop_name
 * 
 * Sets one or more child properties for @child and @container.
 **/
void
gtk_container_child_set_valist (GtkContainer *container,
				GtkWidget    *child,
				const gchar  *first_property_name,
				va_list       var_args)
{
  GObjectNotifyQueue *nqueue;
  const gchar *name;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent == GTK_WIDGET (container));

  g_object_ref (container);
  g_object_ref (child);

  nqueue = g_object_notify_queue_freeze (G_OBJECT (child), _gtk_widget_child_property_notify_context);
  name = first_property_name;
  while (name)
    {
      GValue value = { 0, };
      gchar *error = NULL;
      GParamSpec *pspec = g_param_spec_pool_lookup (_gtk_widget_child_property_pool,
						    name,
						    G_OBJECT_TYPE (container),
						    TRUE);
      if (!pspec)
	{
	  g_warning ("%s: container class `%s' has no child property named `%s'",
		     G_STRLOC,
		     G_OBJECT_TYPE_NAME (container),
		     name);
	  break;
	}
      if (!(pspec->flags & G_PARAM_WRITABLE))
	{
	  g_warning ("%s: child property `%s' of container class `%s' is not writable",
		     G_STRLOC,
		     pspec->name,
		     G_OBJECT_TYPE_NAME (container));
	  break;
	}
      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      G_VALUE_COLLECT (&value, var_args, 0, &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);

	  /* we purposely leak the value here, it might not be
	   * in a sane state if an error condition occoured
	   */
	  break;
	}
      container_set_child_property (container, child, pspec, &value, nqueue);
      g_value_unset (&value);
      name = va_arg (var_args, gchar*);
    }
  g_object_notify_queue_thaw (G_OBJECT (child), nqueue);

  g_object_unref (container);
  g_object_unref (child);
}

/**
 * gtk_container_child_set_property:
 * @container: a #GtkContainer
 * @child: a widget which is a child of @container
 * @property_name: the name of the property to set
 * @value: the value to set the property to
 * 
 * Sets a child property for @child and @container.
 **/
void
gtk_container_child_set_property (GtkContainer *container,
				  GtkWidget    *child,
				  const gchar  *property_name,
				  const GValue *value)
{
  GObjectNotifyQueue *nqueue;
  GParamSpec *pspec;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent == GTK_WIDGET (container));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (G_IS_VALUE (value));
  
  g_object_ref (container);
  g_object_ref (child);

  nqueue = g_object_notify_queue_freeze (G_OBJECT (child), _gtk_widget_child_property_notify_context);
  pspec = g_param_spec_pool_lookup (_gtk_widget_child_property_pool, property_name,
				    G_OBJECT_TYPE (container), TRUE);
  if (!pspec)
    g_warning ("%s: container class `%s' has no child property named `%s'",
	       G_STRLOC,
	       G_OBJECT_TYPE_NAME (container),
	       property_name);
  else if (!(pspec->flags & G_PARAM_WRITABLE))
    g_warning ("%s: child property `%s' of container class `%s' is not writable",
	       G_STRLOC,
	       pspec->name,
	       G_OBJECT_TYPE_NAME (container));
  else
    {
      container_set_child_property (container, child, pspec, value, nqueue);
    }
  g_object_notify_queue_thaw (G_OBJECT (child), nqueue);
  g_object_unref (container);
  g_object_unref (child);
}

/**
 * gtk_container_add_with_properties:
 * @container: a #GtkContainer 
 * @widget: a widget to be placed inside @container 
 * @first_prop_name: the name of the first child property to set 
 * @Varargs: a %NULL-terminated list of property names and values, starting
 *           with @first_prop_name
 * 
 * Adds @widget to @container, setting child properties at the same time.
 * See gtk_container_add() and gtk_container_child_set() for more details.
 **/
void
gtk_container_add_with_properties (GtkContainer *container,
				   GtkWidget    *widget,
				   const gchar  *first_prop_name,
				   ...)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (widget->parent == NULL);

  g_object_ref (container);
  g_object_ref (widget);
  gtk_widget_freeze_child_notify (widget);

  g_signal_emit (container, container_signals[ADD], 0, widget);
  if (widget->parent)
    {
      va_list var_args;

      va_start (var_args, first_prop_name);
      gtk_container_child_set_valist (container, widget, first_prop_name, var_args);
      va_end (var_args);
    }

  gtk_widget_thaw_child_notify (widget);
  g_object_unref (widget);
  g_object_unref (container);
}

/**
 * gtk_container_child_set:
 * @container: a #GtkContainer
 * @child: a widget which is a child of @container
 * @first_prop_name: the name of the first property to set
 * @Varargs: a %NULL-terminated list of property names and values, starting
 *           with @first_prop_name
 * 
 * Sets one or more child properties for @child and @container.
 **/
void
gtk_container_child_set (GtkContainer      *container,
			 GtkWidget         *child,
			 const gchar       *first_prop_name,
			 ...)
{
  va_list var_args;
  
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent == GTK_WIDGET (container));

  va_start (var_args, first_prop_name);
  gtk_container_child_set_valist (container, child, first_prop_name, var_args);
  va_end (var_args);
}

/**
 * gtk_container_child_get:
 * @container: a #GtkContainer
 * @child: a widget which is a child of @container
 * @first_prop_name: the name of the first property to get
 * @Varargs: return location for the first property, followed 
 *     optionally by more name/return location pairs, followed by %NULL
 * 
 * Gets the values of one or more child properties for @child and @container.
 **/
void
gtk_container_child_get (GtkContainer      *container,
			 GtkWidget         *child,
			 const gchar       *first_prop_name,
			 ...)
{
  va_list var_args;
  
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent == GTK_WIDGET (container));

  va_start (var_args, first_prop_name);
  gtk_container_child_get_valist (container, child, first_prop_name, var_args);
  va_end (var_args);
}

/**
 * gtk_container_class_install_child_property:
 * @cclass: a #GtkContainerClass
 * @property_id: the id for the property
 * @pspec: the #GParamSpec for the property
 * 
 * Installs a child property on a container class. 
 **/
void
gtk_container_class_install_child_property (GtkContainerClass *cclass,
					    guint              property_id,
					    GParamSpec        *pspec)
{
  g_return_if_fail (GTK_IS_CONTAINER_CLASS (cclass));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  if (pspec->flags & G_PARAM_WRITABLE)
    g_return_if_fail (cclass->set_child_property != NULL);
  if (pspec->flags & G_PARAM_READABLE)
    g_return_if_fail (cclass->get_child_property != NULL);
  g_return_if_fail (property_id > 0);
  g_return_if_fail (PARAM_SPEC_PARAM_ID (pspec) == 0);  /* paranoid */
  if (pspec->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY))
    g_return_if_fail ((pspec->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY)) == 0);

  if (g_param_spec_pool_lookup (_gtk_widget_child_property_pool, pspec->name, G_OBJECT_CLASS_TYPE (cclass), FALSE))
    {
      g_warning (G_STRLOC ": class `%s' already contains a child property named `%s'",
		 G_OBJECT_CLASS_NAME (cclass),
		 pspec->name);
      return;
    }
  g_param_spec_ref (pspec);
  g_param_spec_sink (pspec);
  PARAM_SPEC_SET_PARAM_ID (pspec, property_id);
  g_param_spec_pool_insert (_gtk_widget_child_property_pool, pspec, G_OBJECT_CLASS_TYPE (cclass));
}

/**
 * gtk_container_class_find_child_property:
 * @cclass: (type GtkContainerClass): a #GtkContainerClass
 * @property_name: the name of the child property to find
 * @returns: (transfer none): the #GParamSpec of the child property or
 *           %NULL if @class has no child property with that name.
 *
 * Finds a child property of a container class by name.
 */
GParamSpec*
gtk_container_class_find_child_property (GObjectClass *cclass,
					 const gchar  *property_name)
{
  g_return_val_if_fail (GTK_IS_CONTAINER_CLASS (cclass), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  return g_param_spec_pool_lookup (_gtk_widget_child_property_pool,
				   property_name,
				   G_OBJECT_CLASS_TYPE (cclass),
				   TRUE);
}

/**
 * gtk_container_class_list_child_properties:
 * @cclass: (type GtkContainerClass): a #GtkContainerClass
 * @n_properties: location to return the number of child properties found
 * @returns: (array length=n_properties) (transfer container):  a newly 
 *           allocated %NULL-terminated array of #GParamSpec*. 
 *           The array must be freed with g_free().
 *
 * Returns all child properties of a container class.
 */
GParamSpec**
gtk_container_class_list_child_properties (GObjectClass *cclass,
					   guint        *n_properties)
{
  GParamSpec **pspecs;
  guint n;

  g_return_val_if_fail (GTK_IS_CONTAINER_CLASS (cclass), NULL);

  pspecs = g_param_spec_pool_list (_gtk_widget_child_property_pool,
				   G_OBJECT_CLASS_TYPE (cclass),
				   &n);
  if (n_properties)
    *n_properties = n;

  return pspecs;
}

static void
gtk_container_add_unimplemented (GtkContainer     *container,
				 GtkWidget        *widget)
{
  g_warning ("GtkContainerClass::add not implemented for `%s'", g_type_name (G_TYPE_FROM_INSTANCE (container)));
}

static void
gtk_container_remove_unimplemented (GtkContainer     *container,
				    GtkWidget        *widget)
{
  g_warning ("GtkContainerClass::remove not implemented for `%s'", g_type_name (G_TYPE_FROM_INSTANCE (container)));
}

static void
gtk_container_init (GtkContainer *container)
{
  container->focus_child = NULL;
  container->border_width = 0;
  container->need_resize = FALSE;
  container->resize_mode = GTK_RESIZE_PARENT;
  container->reallocate_redraws = FALSE;
}

static void
gtk_container_destroy (GtkObject *object)
{
  GtkContainer *container = GTK_CONTAINER (object);

  if (GTK_CONTAINER_RESIZE_PENDING (container))
    _gtk_container_dequeue_resize_handler (container);

  if (container->focus_child)
    {
      g_object_unref (container->focus_child);
      container->focus_child = NULL;
    }

  /* do this before walking child widgets, to avoid
   * removing children from focus chain one by one.
   */
  if (container->has_focus_chain)
    gtk_container_unset_focus_chain (container);

  gtk_container_foreach (container, (GtkCallback) gtk_widget_destroy, NULL);

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gtk_container_set_property (GObject         *object,
			    guint            prop_id,
			    const GValue    *value,
			    GParamSpec      *pspec)
{
  GtkContainer *container = GTK_CONTAINER (object);

  switch (prop_id)
    {
    case PROP_BORDER_WIDTH:
      gtk_container_set_border_width (container, g_value_get_uint (value));
      break;
    case PROP_RESIZE_MODE:
      gtk_container_set_resize_mode (container, g_value_get_enum (value));
      break;
    case PROP_CHILD:
      gtk_container_add (container, GTK_WIDGET (g_value_get_object (value)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_container_get_property (GObject         *object,
			    guint            prop_id,
			    GValue          *value,
			    GParamSpec      *pspec)
{
  GtkContainer *container = GTK_CONTAINER (object);
  
  switch (prop_id)
    {
    case PROP_BORDER_WIDTH:
      g_value_set_uint (value, container->border_width);
      break;
    case PROP_RESIZE_MODE:
      g_value_set_enum (value, container->resize_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_container_set_border_width:
 * @container: a #GtkContainer
 * @border_width: amount of blank space to leave <emphasis>outside</emphasis> 
 *   the container. Valid values are in the range 0-65535 pixels.
 *
 * Sets the border width of the container.
 *
 * The border width of a container is the amount of space to leave
 * around the outside of the container. The only exception to this is
 * #GtkWindow; because toplevel windows can't leave space outside,
 * they leave the space inside. The border is added on all sides of
 * the container. To add space to only one side, one approach is to
 * create a #GtkAlignment widget, call gtk_widget_set_size_request()
 * to give it a size, and place it on the side of the container as
 * a spacer.
 **/
void
gtk_container_set_border_width (GtkContainer *container,
				guint         border_width)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));

  if (container->border_width != border_width)
    {
      container->border_width = border_width;
      g_object_notify (G_OBJECT (container), "border-width");
      
      if (gtk_widget_get_realized (GTK_WIDGET (container)))
	gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}

/**
 * gtk_container_get_border_width:
 * @container: a #GtkContainer
 * 
 * Retrieves the border width of the container. See
 * gtk_container_set_border_width().
 *
 * Return value: the current border width
 **/
guint
gtk_container_get_border_width (GtkContainer *container)
{
  g_return_val_if_fail (GTK_IS_CONTAINER (container), 0);

  return container->border_width;
}

/**
 * gtk_container_add:
 * @container: a #GtkContainer
 * @widget: a widget to be placed inside @container
 * 
 * Adds @widget to @container. Typically used for simple containers
 * such as #GtkWindow, #GtkFrame, or #GtkButton; for more complicated
 * layout containers such as #GtkBox or #GtkTable, this function will
 * pick default packing parameters that may not be correct.  So
 * consider functions such as gtk_box_pack_start() and
 * gtk_table_attach() as an alternative to gtk_container_add() in
 * those cases. A widget may be added to only one container at a time;
 * you can't place the same widget inside two different containers.
 **/
void
gtk_container_add (GtkContainer *container,
		   GtkWidget    *widget)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (widget->parent != NULL)
    {
      g_warning ("Attempting to add a widget with type %s to a container of "
                 "type %s, but the widget is already inside a container of type %s, "
                 "the GTK+ FAQ at http://library.gnome.org/devel/gtk-faq/stable/ "
                 "explains how to reparent a widget.",
                 g_type_name (G_OBJECT_TYPE (widget)),
                 g_type_name (G_OBJECT_TYPE (container)),
                 g_type_name (G_OBJECT_TYPE (widget->parent)));
      return;
    }

  g_signal_emit (container, container_signals[ADD], 0, widget);
}

/**
 * gtk_container_remove:
 * @container: a #GtkContainer
 * @widget: a current child of @container
 * 
 * Removes @widget from @container. @widget must be inside @container.
 * Note that @container will own a reference to @widget, and that this
 * may be the last reference held; so removing a widget from its
 * container can destroy that widget. If you want to use @widget
 * again, you need to add a reference to it while it's not inside
 * a container, using g_object_ref(). If you don't want to use @widget
 * again it's usually more efficient to simply destroy it directly
 * using gtk_widget_destroy() since this will remove it from the
 * container and help break any circular reference count cycles.
 **/
void
gtk_container_remove (GtkContainer *container,
		      GtkWidget    *widget)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  /* When using the deprecated API of the toolbar, it is possible
   * to legitimately call this function with a widget that is not
   * a direct child of the container.
   */
  g_return_if_fail (GTK_IS_TOOLBAR (container) ||
		    widget->parent == GTK_WIDGET (container));
  
  g_signal_emit (container, container_signals[REMOVE], 0, widget);
}

void
_gtk_container_dequeue_resize_handler (GtkContainer *container)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_CONTAINER_RESIZE_PENDING (container));

  container_resize_queue = g_slist_remove (container_resize_queue, container);
  GTK_PRIVATE_UNSET_FLAG (container, GTK_RESIZE_PENDING);
}

/**
 * gtk_container_set_resize_mode:
 * @container: a #GtkContainer
 * @resize_mode: the new resize mode
 * 
 * Sets the resize mode for the container.
 *
 * The resize mode of a container determines whether a resize request 
 * will be passed to the container's parent, queued for later execution
 * or executed immediately.
 **/
void
gtk_container_set_resize_mode (GtkContainer  *container,
			       GtkResizeMode  resize_mode)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (resize_mode <= GTK_RESIZE_IMMEDIATE);
  
  if (gtk_widget_is_toplevel (GTK_WIDGET (container)) &&
      resize_mode == GTK_RESIZE_PARENT)
    {
      resize_mode = GTK_RESIZE_QUEUE;
    }
  
  if (container->resize_mode != resize_mode)
    {
      container->resize_mode = resize_mode;
      
      gtk_widget_queue_resize (GTK_WIDGET (container));
      g_object_notify (G_OBJECT (container), "resize-mode");
    }
}

/**
 * gtk_container_get_resize_mode:
 * @container: a #GtkContainer
 * 
 * Returns the resize mode for the container. See
 * gtk_container_set_resize_mode ().
 *
 * Return value: the current resize mode
 **/
GtkResizeMode
gtk_container_get_resize_mode (GtkContainer *container)
{
  g_return_val_if_fail (GTK_IS_CONTAINER (container), GTK_RESIZE_PARENT);

  return container->resize_mode;
}

/**
 * gtk_container_set_reallocate_redraws:
 * @container: a #GtkContainer
 * @needs_redraws: the new value for the container's @reallocate_redraws flag
 *
 * Sets the @reallocate_redraws flag of the container to the given value.
 * 
 * Containers requesting reallocation redraws get automatically
 * redrawn if any of their children changed allocation. 
 **/ 
void
gtk_container_set_reallocate_redraws (GtkContainer *container,
				      gboolean      needs_redraws)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));

  container->reallocate_redraws = needs_redraws ? TRUE : FALSE;
}

static GtkContainer*
gtk_container_get_resize_container (GtkContainer *container)
{
  GtkWidget *widget = GTK_WIDGET (container);

  while (widget->parent)
    {
      widget = widget->parent;
      if (GTK_IS_RESIZE_CONTAINER (widget))
	break;
    }

  return GTK_IS_RESIZE_CONTAINER (widget) ? (GtkContainer*) widget : NULL;
}

static gboolean
gtk_container_idle_sizer (gpointer data)
{
  /* we may be invoked with a container_resize_queue of NULL, because
   * queue_resize could have been adding an extra idle function while
   * the queue still got processed. we better just ignore such case
   * than trying to explicitely work around them with some extra flags,
   * since it doesn't cause any actual harm.
   */
  while (container_resize_queue)
    {
      GSList *slist;
      GtkWidget *widget;

      slist = container_resize_queue;
      container_resize_queue = slist->next;
      widget = slist->data;
      g_slist_free_1 (slist);

      GTK_PRIVATE_UNSET_FLAG (widget, GTK_RESIZE_PENDING);
      gtk_container_check_resize (GTK_CONTAINER (widget));
    }

  gdk_window_process_all_updates ();

  return FALSE;
}

void
_gtk_container_queue_resize (GtkContainer *container)
{
  GtkContainer *resize_container;
  GtkWidget *widget;
  
  g_return_if_fail (GTK_IS_CONTAINER (container));

  widget = GTK_WIDGET (container);
  resize_container = gtk_container_get_resize_container (container);
  
  while (TRUE)
    {
      GTK_PRIVATE_SET_FLAG (widget, GTK_ALLOC_NEEDED);
      GTK_PRIVATE_SET_FLAG (widget, GTK_REQUEST_NEEDED);
      if ((resize_container && widget == GTK_WIDGET (resize_container)) ||
	  !widget->parent)
	break;
      
      widget = widget->parent;
    }
      
  if (resize_container)
    {
      if (gtk_widget_get_visible (GTK_WIDGET (resize_container)) &&
          (gtk_widget_is_toplevel (GTK_WIDGET (resize_container)) ||
           gtk_widget_get_realized (GTK_WIDGET (resize_container))))
	{
	  switch (resize_container->resize_mode)
	    {
	    case GTK_RESIZE_QUEUE:
	      if (!GTK_CONTAINER_RESIZE_PENDING (resize_container))
		{
		  GTK_PRIVATE_SET_FLAG (resize_container, GTK_RESIZE_PENDING);
		  if (container_resize_queue == NULL)
		    gdk_threads_add_idle_full (GTK_PRIORITY_RESIZE,
				     gtk_container_idle_sizer,
				     NULL, NULL);
		  container_resize_queue = g_slist_prepend (container_resize_queue, resize_container);
		}
	      break;

	    case GTK_RESIZE_IMMEDIATE:
	      gtk_container_check_resize (resize_container);
	      break;

	    case GTK_RESIZE_PARENT:
	      g_assert_not_reached ();
	      break;
	    }
	}
      else
	{
	  /* we need to let hidden resize containers know that something
	   * changed while they where hidden (currently only evaluated by
	   * toplevels).
	   */
	  resize_container->need_resize = TRUE;
	}
    }
}

void
gtk_container_check_resize (GtkContainer *container)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  
  g_signal_emit (container, container_signals[CHECK_RESIZE], 0);
}

static void
gtk_container_real_check_resize (GtkContainer *container)
{
  GtkWidget *widget = GTK_WIDGET (container);
  GtkRequisition requisition;
  
  gtk_widget_size_request (widget, &requisition);
  
  if (requisition.width > widget->allocation.width ||
      requisition.height > widget->allocation.height)
    {
      if (GTK_IS_RESIZE_CONTAINER (container))
	gtk_widget_size_allocate (GTK_WIDGET (container),
				  &GTK_WIDGET (container)->allocation);
      else
	gtk_widget_queue_resize (widget);
    }
  else
    {
      gtk_container_resize_children (container);
    }
}

/* The container hasn't changed size but one of its children
 *  queued a resize request. Which means that the allocation
 *  is not sufficient for the requisition of some child.
 *  We've already performed a size request at this point,
 *  so we simply need to reallocate and let the allocation
 *  trickle down via GTK_WIDGET_ALLOC_NEEDED flags. 
 */
void
gtk_container_resize_children (GtkContainer *container)
{
  GtkWidget *widget;
  
  /* resizing invariants:
   * toplevels have *always* resize_mode != GTK_RESIZE_PARENT set.
   * containers that have an idle sizer pending must be flagged with
   * RESIZE_PENDING.
   */
  g_return_if_fail (GTK_IS_CONTAINER (container));

  widget = GTK_WIDGET (container);
  gtk_widget_size_allocate (widget, &widget->allocation);
}

/**
 * gtk_container_forall:
 * @container: a #GtkContainer
 * @callback: a callback
 * @callback_data: callback user data
 * 
 * Invokes @callback on each child of @container, including children
 * that are considered "internal" (implementation details of the
 * container). "Internal" children generally weren't added by the user
 * of the container, but were added by the container implementation
 * itself.  Most applications should use gtk_container_foreach(),
 * rather than gtk_container_forall().
 **/
void
gtk_container_forall (GtkContainer *container,
		      GtkCallback   callback,
		      gpointer      callback_data)
{
  GtkContainerClass *class;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (callback != NULL);

  class = GTK_CONTAINER_GET_CLASS (container);

  if (class->forall)
    class->forall (container, TRUE, callback, callback_data);
}

/**
 * gtk_container_foreach:
 * @container: a #GtkContainer
 * @callback: (scope call):  a callback
 * @callback_data: callback user data
 * 
 * Invokes @callback on each non-internal child of @container. See
 * gtk_container_forall() for details on what constitutes an
 * "internal" child.  Most applications should use
 * gtk_container_foreach(), rather than gtk_container_forall().
 **/
void
gtk_container_foreach (GtkContainer *container,
		       GtkCallback   callback,
		       gpointer      callback_data)
{
  GtkContainerClass *class;
  
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (callback != NULL);

  class = GTK_CONTAINER_GET_CLASS (container);

  if (class->forall)
    class->forall (container, FALSE, callback, callback_data);
}

typedef struct _GtkForeachData	GtkForeachData;
struct _GtkForeachData
{
  GtkObject         *container;
  GtkCallbackMarshal callback;
  gpointer           callback_data;
};

static void
gtk_container_foreach_unmarshal (GtkWidget *child,
				 gpointer data)
{
  GtkForeachData *fdata = (GtkForeachData*) data;
  GtkArg args[2];
  
  /* first argument */
  args[0].name = NULL;
  args[0].type = G_TYPE_FROM_INSTANCE (child);
  GTK_VALUE_OBJECT (args[0]) = GTK_OBJECT (child);
  
  /* location for return value */
  args[1].name = NULL;
  args[1].type = G_TYPE_NONE;
  
  fdata->callback (fdata->container, fdata->callback_data, 1, args);
}

void
gtk_container_foreach_full (GtkContainer       *container,
			    GtkCallback         callback,
			    GtkCallbackMarshal  marshal,
			    gpointer            callback_data,
			    GDestroyNotify      notify)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));

  if (marshal)
    {
      GtkForeachData fdata;
  
      fdata.container     = GTK_OBJECT (container);
      fdata.callback      = marshal;
      fdata.callback_data = callback_data;

      gtk_container_foreach (container, gtk_container_foreach_unmarshal, &fdata);
    }
  else
    {
      g_return_if_fail (callback != NULL);

      gtk_container_foreach (container, callback, &callback_data);
    }

  if (notify)
    notify (callback_data);
}

/**
 * gtk_container_set_focus_child:
 * @container: a #GtkContainer
 * @child: (allow-none): a #GtkWidget, or %NULL
 *
 * Sets, or unsets if @child is %NULL, the focused child of @container.
 *
 * This function emits the GtkContainer::set_focus_child signal of
 * @container. Implementations of #GtkContainer can override the
 * default behaviour by overriding the class closure of this signal.
 *
 * This is function is mostly meant to be used by widgets. Applications can use
 * gtk_widget_grab_focus() to manualy set the focus to a specific widget.
 */
void
gtk_container_set_focus_child (GtkContainer *container,
			       GtkWidget    *child)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  if (child)
    g_return_if_fail (GTK_IS_WIDGET (child));

  g_signal_emit (container, container_signals[SET_FOCUS_CHILD], 0, child);
}

/**
 * gtk_container_get_focus_child:
 * @container: a #GtkContainer
 *
 * Returns the current focus child widget inside @container. This is not the
 * currently focused widget. That can be obtained by calling
 * gtk_window_get_focus().
 *
 * Returns: (transfer none): The child widget which will receive the
 *          focus inside @container when the @conatiner is focussed,
 *          or %NULL if none is set.
 *
 * Since: 2.14
 **/
GtkWidget *
gtk_container_get_focus_child (GtkContainer *container)
{
  g_return_val_if_fail (GTK_IS_CONTAINER (container), NULL);

  return container->focus_child;
}

/**
 * gtk_container_get_children:
 * @container: a #GtkContainer
 * 
 * Returns the container's non-internal children. See
 * gtk_container_forall() for details on what constitutes an "internal" child.
 *
 * Return value: (element-type GtkWidget) (transfer container): a newly-allocated list of the container's non-internal children.
 **/
GList*
gtk_container_get_children (GtkContainer *container)
{
  GList *children = NULL;

  gtk_container_foreach (container,
			 gtk_container_children_callback,
			 &children);

  return g_list_reverse (children);
}

static void
gtk_container_child_position_callback (GtkWidget *widget,
				       gpointer   client_data)
{
  struct {
    GtkWidget *child;
    guint i;
    guint index;
  } *data = client_data;

  data->i++;
  if (data->child == widget)
    data->index = data->i;
}

static gchar*
gtk_container_child_default_composite_name (GtkContainer *container,
					    GtkWidget    *child)
{
  struct {
    GtkWidget *child;
    guint i;
    guint index;
  } data;
  gchar *name;

  /* fallback implementation */
  data.child = child;
  data.i = 0;
  data.index = 0;
  gtk_container_forall (container,
			gtk_container_child_position_callback,
			&data);
  
  name = g_strdup_printf ("%s-%u",
			  g_type_name (G_TYPE_FROM_INSTANCE (child)),
			  data.index);

  return name;
}

gchar*
_gtk_container_child_composite_name (GtkContainer *container,
				    GtkWidget    *child)
{
  gboolean composite_child;

  g_return_val_if_fail (GTK_IS_CONTAINER (container), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);
  g_return_val_if_fail (child->parent == GTK_WIDGET (container), NULL);

  g_object_get (child, "composite-child", &composite_child, NULL);
  if (composite_child)
    {
      static GQuark quark_composite_name = 0;
      gchar *name;

      if (!quark_composite_name)
	quark_composite_name = g_quark_from_static_string ("gtk-composite-name");

      name = g_object_get_qdata (G_OBJECT (child), quark_composite_name);
      if (!name)
	{
	  GtkContainerClass *class;

	  class = GTK_CONTAINER_GET_CLASS (container);
	  if (class->composite_name)
	    name = class->composite_name (container, child);
	}
      else
	name = g_strdup (name);

      return name;
    }
  
  return NULL;
}

static void
gtk_container_real_set_focus_child (GtkContainer     *container,
				    GtkWidget        *child)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));

  if (child != container->focus_child)
    {
      if (container->focus_child)
	g_object_unref (container->focus_child);

      container->focus_child = child;

      if (container->focus_child)
	g_object_ref (container->focus_child);
    }

  /* Check for h/v adjustments and scroll to show the focus child if possible */
  if (container->focus_child)
    {
      GtkAdjustment *hadj;
      GtkAdjustment *vadj;
      GtkWidget *focus_child;
      gint x, y;

      hadj = g_object_get_qdata (G_OBJECT (container), hadjustment_key_id);   
      vadj = g_object_get_qdata (G_OBJECT (container), vadjustment_key_id);
      if (hadj || vadj) 
	{
	  focus_child = container->focus_child;
	  while (GTK_IS_CONTAINER (focus_child) && 
		 GTK_CONTAINER (focus_child)->focus_child)
	    {
	      focus_child = GTK_CONTAINER (focus_child)->focus_child;
	    }
	  
           if (!gtk_widget_translate_coordinates (focus_child, container->focus_child,
                                                  0, 0, &x, &y))
             return;

	   x += container->focus_child->allocation.x;
	   y += container->focus_child->allocation.y;
	  
	  if (vadj)
	    gtk_adjustment_clamp_page (vadj, y, y + focus_child->allocation.height);
	  
	  if (hadj)
	    gtk_adjustment_clamp_page (hadj, x, x + focus_child->allocation.width);
	}
    }
}

static GList*
get_focus_chain (GtkContainer *container)
{
  return g_object_get_data (G_OBJECT (container), "gtk-container-focus-chain");
}

/* same as gtk_container_get_children, except it includes internals
 */
static GList *
gtk_container_get_all_children (GtkContainer *container)
{
  GList *children = NULL;

  gtk_container_forall (container,
			 gtk_container_children_callback,
			 &children);

  return children;
}

static gboolean
gtk_container_focus (GtkWidget        *widget,
                     GtkDirectionType  direction)
{
  GList *children;
  GList *sorted_children;
  gint return_val;
  GtkContainer *container;

  g_return_val_if_fail (GTK_IS_CONTAINER (widget), FALSE);

  container = GTK_CONTAINER (widget);

  return_val = FALSE;

  if (gtk_widget_get_can_focus (widget))
    {
      if (!gtk_widget_has_focus (widget))
	{
	  gtk_widget_grab_focus (widget);
	  return_val = TRUE;
	}
    }
  else
    {
      /* Get a list of the containers children, allowing focus
       * chain to override.
       */
      if (container->has_focus_chain)
	children = g_list_copy (get_focus_chain (container));
      else
	children = gtk_container_get_all_children (container);

      if (container->has_focus_chain &&
	  (direction == GTK_DIR_TAB_FORWARD ||
	   direction == GTK_DIR_TAB_BACKWARD))
	{
	  sorted_children = g_list_copy (children);
	  
	  if (direction == GTK_DIR_TAB_BACKWARD)
	    sorted_children = g_list_reverse (sorted_children);
	}
      else
	sorted_children = _gtk_container_focus_sort (container, children, direction, NULL);
      
      return_val = gtk_container_focus_move (container, sorted_children, direction);

      g_list_free (sorted_children);
      g_list_free (children);
    }

  return return_val;
}

static gint
tab_compare (gconstpointer a,
	     gconstpointer b,
	     gpointer      data)
{
  const GtkWidget *child1 = a;
  const GtkWidget *child2 = b;
  GtkTextDirection text_direction = GPOINTER_TO_INT (data);

  gint y1 = child1->allocation.y + child1->allocation.height / 2;
  gint y2 = child2->allocation.y + child2->allocation.height / 2;

  if (y1 == y2)
    {
      gint x1 = child1->allocation.x + child1->allocation.width / 2;
      gint x2 = child2->allocation.x + child2->allocation.width / 2;
      
      if (text_direction == GTK_TEXT_DIR_RTL) 
	return (x1 < x2) ? 1 : ((x1 == x2) ? 0 : -1);
      else
	return (x1 < x2) ? -1 : ((x1 == x2) ? 0 : 1);
    }
  else
    return (y1 < y2) ? -1 : 1;
}

static GList *
gtk_container_focus_sort_tab (GtkContainer     *container,
			      GList            *children,
			      GtkDirectionType  direction,
			      GtkWidget        *old_focus)
{
  GtkTextDirection text_direction = gtk_widget_get_direction (GTK_WIDGET (container));
  children = g_list_sort_with_data (children, tab_compare, GINT_TO_POINTER (text_direction));

  /* if we are going backwards then reverse the order
   *  of the children.
   */
  if (direction == GTK_DIR_TAB_BACKWARD)
    children = g_list_reverse (children);

  return children;
}

/* Get coordinates of @widget's allocation with respect to
 * allocation of @container.
 */
static gboolean
get_allocation_coords (GtkContainer  *container,
		       GtkWidget     *widget,
		       GdkRectangle  *allocation)
{
  *allocation = widget->allocation;

  return gtk_widget_translate_coordinates (widget, GTK_WIDGET (container),
					   0, 0, &allocation->x, &allocation->y);
}

/* Look for a child in @children that is intermediate between
 * the focus widget and container. This widget, if it exists,
 * acts as the starting widget for focus navigation.
 */
static GtkWidget *
find_old_focus (GtkContainer *container,
		GList        *children)
{
  GList *tmp_list = children;
  while (tmp_list)
    {
      GtkWidget *child = tmp_list->data;
      GtkWidget *widget = child;

      while (widget && widget != (GtkWidget *)container)
	{
	  GtkWidget *parent = widget->parent;
	  if (parent && ((GtkContainer *)parent)->focus_child != widget)
	    goto next;

	  widget = parent;
	}

      return child;

    next:
      tmp_list = tmp_list->next;
    }

  return NULL;
}

static gboolean
old_focus_coords (GtkContainer *container,
		  GdkRectangle *old_focus_rect)
{
  GtkWidget *widget = GTK_WIDGET (container);
  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);

  if (GTK_IS_WINDOW (toplevel) && GTK_WINDOW (toplevel)->focus_widget)
    {
      GtkWidget *old_focus = GTK_WINDOW (toplevel)->focus_widget;
      
      return get_allocation_coords (container, old_focus, old_focus_rect);
    }
  else
    return FALSE;
}

typedef struct _CompareInfo CompareInfo;

struct _CompareInfo
{
  GtkContainer *container;
  gint x;
  gint y;
  gboolean reverse;
};

static gint
up_down_compare (gconstpointer a,
		 gconstpointer b,
		 gpointer      data)
{
  GdkRectangle allocation1;
  GdkRectangle allocation2;
  CompareInfo *compare = data;
  gint y1, y2;

  get_allocation_coords (compare->container, (GtkWidget *)a, &allocation1);
  get_allocation_coords (compare->container, (GtkWidget *)b, &allocation2);

  y1 = allocation1.y + allocation1.height / 2;
  y2 = allocation2.y + allocation2.height / 2;

  if (y1 == y2)
    {
      gint x1 = abs (allocation1.x + allocation1.width / 2 - compare->x);
      gint x2 = abs (allocation2.x + allocation2.width / 2 - compare->x);

      if (compare->reverse)
	return (x1 < x2) ? 1 : ((x1 == x2) ? 0 : -1);
      else
	return (x1 < x2) ? -1 : ((x1 == x2) ? 0 : 1);
    }
  else
    return (y1 < y2) ? -1 : 1;
}

static GList *
gtk_container_focus_sort_up_down (GtkContainer     *container,
				  GList            *children,
				  GtkDirectionType  direction,
				  GtkWidget        *old_focus)
{
  CompareInfo compare;
  GList *tmp_list;
  GdkRectangle old_allocation;

  compare.container = container;
  compare.reverse = (direction == GTK_DIR_UP);

  if (!old_focus)
      old_focus = find_old_focus (container, children);
  
  if (old_focus && get_allocation_coords (container, old_focus, &old_allocation))
    {
      gint compare_x1;
      gint compare_x2;
      gint compare_y;

      /* Delete widgets from list that don't match minimum criteria */

      compare_x1 = old_allocation.x;
      compare_x2 = old_allocation.x + old_allocation.width;

      if (direction == GTK_DIR_UP)
	compare_y = old_allocation.y;
      else
	compare_y = old_allocation.y + old_allocation.height;
      
      tmp_list = children;
      while (tmp_list)
	{
	  GtkWidget *child = tmp_list->data;
	  GList *next = tmp_list->next;
	  gint child_x1, child_x2;
	  GdkRectangle child_allocation;
	  
	  if (child != old_focus)
	    {
	      if (get_allocation_coords (container, child, &child_allocation))
		{
		  child_x1 = child_allocation.x;
		  child_x2 = child_allocation.x + child_allocation.width;
		  
		  if ((child_x2 <= compare_x1 || child_x1 >= compare_x2) /* No horizontal overlap */ ||
		      (direction == GTK_DIR_DOWN && child_allocation.y + child_allocation.height < compare_y) || /* Not below */
		      (direction == GTK_DIR_UP && child_allocation.y > compare_y)) /* Not above */
		    {
		      children = g_list_delete_link (children, tmp_list);
		    }
		}
	      else
		children = g_list_delete_link (children, tmp_list);
	    }
	  
	  tmp_list = next;
	}

      compare.x = (compare_x1 + compare_x2) / 2;
      compare.y = old_allocation.y + old_allocation.height / 2;
    }
  else
    {
      /* No old focus widget, need to figure out starting x,y some other way
       */
      GtkWidget *widget = GTK_WIDGET (container);
      GdkRectangle old_focus_rect;

      if (old_focus_coords (container, &old_focus_rect))
	{
	  compare.x = old_focus_rect.x + old_focus_rect.width / 2;
	}
      else
	{
	  if (!gtk_widget_get_has_window (widget))
	    compare.x = widget->allocation.x + widget->allocation.width / 2;
	  else
	    compare.x = widget->allocation.width / 2;
	}
      
      if (!gtk_widget_get_has_window (widget))
	compare.y = (direction == GTK_DIR_DOWN) ? widget->allocation.y : widget->allocation.y + widget->allocation.height;
      else
	compare.y = (direction == GTK_DIR_DOWN) ? 0 : + widget->allocation.height;
    }

  children = g_list_sort_with_data (children, up_down_compare, &compare);

  if (compare.reverse)
    children = g_list_reverse (children);

  return children;
}

static gint
left_right_compare (gconstpointer a,
		    gconstpointer b,
		    gpointer      data)
{
  GdkRectangle allocation1;
  GdkRectangle allocation2;
  CompareInfo *compare = data;
  gint x1, x2;

  get_allocation_coords (compare->container, (GtkWidget *)a, &allocation1);
  get_allocation_coords (compare->container, (GtkWidget *)b, &allocation2);

  x1 = allocation1.x + allocation1.width / 2;
  x2 = allocation2.x + allocation2.width / 2;

  if (x1 == x2)
    {
      gint y1 = abs (allocation1.y + allocation1.height / 2 - compare->y);
      gint y2 = abs (allocation2.y + allocation2.height / 2 - compare->y);

      if (compare->reverse)
	return (y1 < y2) ? 1 : ((y1 == y2) ? 0 : -1);
      else
	return (y1 < y2) ? -1 : ((y1 == y2) ? 0 : 1);
    }
  else
    return (x1 < x2) ? -1 : 1;
}

static GList *
gtk_container_focus_sort_left_right (GtkContainer     *container,
				     GList            *children,
				     GtkDirectionType  direction,
				     GtkWidget        *old_focus)
{
  CompareInfo compare;
  GList *tmp_list;
  GdkRectangle old_allocation;

  compare.container = container;
  compare.reverse = (direction == GTK_DIR_LEFT);

  if (!old_focus)
    old_focus = find_old_focus (container, children);
  
  if (old_focus && get_allocation_coords (container, old_focus, &old_allocation))
    {
      gint compare_y1;
      gint compare_y2;
      gint compare_x;
      
      /* Delete widgets from list that don't match minimum criteria */

      compare_y1 = old_allocation.y;
      compare_y2 = old_allocation.y + old_allocation.height;

      if (direction == GTK_DIR_LEFT)
	compare_x = old_allocation.x;
      else
	compare_x = old_allocation.x + old_allocation.width;
      
      tmp_list = children;
      while (tmp_list)
	{
	  GtkWidget *child = tmp_list->data;
	  GList *next = tmp_list->next;
	  gint child_y1, child_y2;
	  GdkRectangle child_allocation;
	  
	  if (child != old_focus)
	    {
	      if (get_allocation_coords (container, child, &child_allocation))
		{
		  child_y1 = child_allocation.y;
		  child_y2 = child_allocation.y + child_allocation.height;
		  
		  if ((child_y2 <= compare_y1 || child_y1 >= compare_y2) /* No vertical overlap */ ||
		      (direction == GTK_DIR_RIGHT && child_allocation.x + child_allocation.width < compare_x) || /* Not to left */
		      (direction == GTK_DIR_LEFT && child_allocation.x > compare_x)) /* Not to right */
		    {
		      children = g_list_delete_link (children, tmp_list);
		    }
		}
	      else
		children = g_list_delete_link (children, tmp_list);
	    }
	  
	  tmp_list = next;
	}

      compare.y = (compare_y1 + compare_y2) / 2;
      compare.x = old_allocation.x + old_allocation.width / 2;
    }
  else
    {
      /* No old focus widget, need to figure out starting x,y some other way
       */
      GtkWidget *widget = GTK_WIDGET (container);
      GdkRectangle old_focus_rect;

      if (old_focus_coords (container, &old_focus_rect))
	{
	  compare.y = old_focus_rect.y + old_focus_rect.height / 2;
	}
      else
	{
	  if (!gtk_widget_get_has_window (widget))
	    compare.y = widget->allocation.y + widget->allocation.height / 2;
	  else
	    compare.y = widget->allocation.height / 2;
	}
      
      if (!gtk_widget_get_has_window (widget))
	compare.x = (direction == GTK_DIR_RIGHT) ? widget->allocation.x : widget->allocation.x + widget->allocation.width;
      else
	compare.x = (direction == GTK_DIR_RIGHT) ? 0 : widget->allocation.width;
    }

  children = g_list_sort_with_data (children, left_right_compare, &compare);

  if (compare.reverse)
    children = g_list_reverse (children);

  return children;
}

/**
 * gtk_container_focus_sort:
 * @container: a #GtkContainer
 * @children:  a list of descendents of @container (they don't
 *             have to be direct children)
 * @direction: focus direction
 * @old_focus: (allow-none): widget to use for the starting position, or %NULL
 *             to determine this automatically.
 *             (Note, this argument isn't used for GTK_DIR_TAB_*,
 *              which is the only @direction we use currently,
 *              so perhaps this argument should be removed)
 * 
 * Sorts @children in the correct order for focusing with
 * direction type @direction.
 * 
 * Return value: a copy of @children, sorted in correct focusing order,
 *   with children that aren't suitable for focusing in this direction
 *   removed.
 **/
GList *
_gtk_container_focus_sort (GtkContainer     *container,
			   GList            *children,
			   GtkDirectionType  direction,
			   GtkWidget        *old_focus)
{
  GList *visible_children = NULL;

  while (children)
    {
      if (gtk_widget_get_realized (children->data))
	visible_children = g_list_prepend (visible_children, children->data);
      children = children->next;
    }
  
  switch (direction)
    {
    case GTK_DIR_TAB_FORWARD:
    case GTK_DIR_TAB_BACKWARD:
      return gtk_container_focus_sort_tab (container, visible_children, direction, old_focus);
    case GTK_DIR_UP:
    case GTK_DIR_DOWN:
      return gtk_container_focus_sort_up_down (container, visible_children, direction, old_focus);
    case GTK_DIR_LEFT:
    case GTK_DIR_RIGHT:
      return gtk_container_focus_sort_left_right (container, visible_children, direction, old_focus);
    }

  g_assert_not_reached ();

  return NULL;
}

static gboolean
gtk_container_focus_move (GtkContainer     *container,
			  GList            *children,
			  GtkDirectionType  direction)
{
  GtkWidget *focus_child;
  GtkWidget *child;

  focus_child = container->focus_child;

  while (children)
    {
      child = children->data;
      children = children->next;

      if (!child)
	continue;
      
      if (focus_child)
        {
          if (focus_child == child)
            {
              focus_child = NULL;

		if (gtk_widget_child_focus (child, direction))
		  return TRUE;
            }
        }
      else if (gtk_widget_is_drawable (child) &&
               gtk_widget_is_ancestor (child, GTK_WIDGET (container)))
        {
          if (gtk_widget_child_focus (child, direction))
            return TRUE;
        }
    }

  return FALSE;
}


static void
gtk_container_children_callback (GtkWidget *widget,
				 gpointer   client_data)
{
  GList **children;

  children = (GList**) client_data;
  *children = g_list_prepend (*children, widget);
}

static void
chain_widget_destroyed (GtkWidget *widget,
                        gpointer   user_data)
{
  GtkContainer *container;
  GList *chain;
  
  container = GTK_CONTAINER (user_data);

  chain = g_object_get_data (G_OBJECT (container),
                             "gtk-container-focus-chain");

  chain = g_list_remove (chain, widget);

  g_signal_handlers_disconnect_by_func (widget,
                                        chain_widget_destroyed,
                                        user_data);
  
  g_object_set_data (G_OBJECT (container),
                     I_("gtk-container-focus-chain"),
                     chain);  
}

/**
 * gtk_container_set_focus_chain:
 * @container: a #GtkContainer
 * @focusable_widgets: (transfer none) (element-type GtkWidget):
 *     the new focus chain
 *
 * Sets a focus chain, overriding the one computed automatically by GTK+.
 * 
 * In principle each widget in the chain should be a descendant of the 
 * container, but this is not enforced by this method, since it's allowed 
 * to set the focus chain before you pack the widgets, or have a widget 
 * in the chain that isn't always packed. The necessary checks are done 
 * when the focus chain is actually traversed.
 **/
void
gtk_container_set_focus_chain (GtkContainer *container,
                               GList        *focusable_widgets)
{
  GList *chain;
  GList *tmp_list;
  
  g_return_if_fail (GTK_IS_CONTAINER (container));
  
  if (container->has_focus_chain)
    gtk_container_unset_focus_chain (container);

  container->has_focus_chain = TRUE;
  
  chain = NULL;
  tmp_list = focusable_widgets;
  while (tmp_list != NULL)
    {
      g_return_if_fail (GTK_IS_WIDGET (tmp_list->data));
      
      /* In principle each widget in the chain should be a descendant
       * of the container, but we don't want to check that here, it's
       * expensive and also it's allowed to set the focus chain before
       * you pack the widgets, or have a widget in the chain that isn't
       * always packed. So we check for ancestor during actual traversal.
       */

      chain = g_list_prepend (chain, tmp_list->data);

      g_signal_connect (tmp_list->data,
                        "destroy",
                        G_CALLBACK (chain_widget_destroyed),
                        container);
      
      tmp_list = g_list_next (tmp_list);
    }

  chain = g_list_reverse (chain);
  
  g_object_set_data (G_OBJECT (container),
                     I_("gtk-container-focus-chain"),
                     chain);
}

/**
 * gtk_container_get_focus_chain:
 * @container:         a #GtkContainer
 * @focusable_widgets: (element-type GtkWidget) (out) (transfer container): location
 *                     to store the focus chain of the
 *                     container, or %NULL. You should free this list
 *                     using g_list_free() when you are done with it, however
 *                     no additional reference count is added to the
 *                     individual widgets in the focus chain.
 * 
 * Retrieves the focus chain of the container, if one has been
 * set explicitly. If no focus chain has been explicitly
 * set, GTK+ computes the focus chain based on the positions
 * of the children. In that case, GTK+ stores %NULL in
 * @focusable_widgets and returns %FALSE.
 *
 * Return value: %TRUE if the focus chain of the container 
 * has been set explicitly.
 **/
gboolean
gtk_container_get_focus_chain (GtkContainer *container,
			       GList       **focus_chain)
{
  g_return_val_if_fail (GTK_IS_CONTAINER (container), FALSE);

  if (focus_chain)
    {
      if (container->has_focus_chain)
	*focus_chain = g_list_copy (get_focus_chain (container));
      else
	*focus_chain = NULL;
    }

  return container->has_focus_chain;
}

/**
 * gtk_container_unset_focus_chain:
 * @container: a #GtkContainer
 * 
 * Removes a focus chain explicitly set with gtk_container_set_focus_chain().
 **/
void
gtk_container_unset_focus_chain (GtkContainer  *container)
{  
  g_return_if_fail (GTK_IS_CONTAINER (container));

  if (container->has_focus_chain)
    {
      GList *chain;
      GList *tmp_list;
      
      chain = get_focus_chain (container);
      
      container->has_focus_chain = FALSE;
      
      g_object_set_data (G_OBJECT (container), 
                         I_("gtk-container-focus-chain"),
                         NULL);

      tmp_list = chain;
      while (tmp_list != NULL)
        {
          g_signal_handlers_disconnect_by_func (tmp_list->data,
                                                chain_widget_destroyed,
                                                container);
          
          tmp_list = g_list_next (tmp_list);
        }

      g_list_free (chain);
    }
}

/**
 * gtk_container_set_focus_vadjustment:
 * @container: a #GtkContainer
 * @adjustment: an adjustment which should be adjusted when the focus 
 *   is moved among the descendents of @container
 * 
 * Hooks up an adjustment to focus handling in a container, so when a 
 * child of the container is focused, the adjustment is scrolled to 
 * show that widget. This function sets the vertical alignment. See 
 * gtk_scrolled_window_get_vadjustment() for a typical way of obtaining 
 * the adjustment and gtk_container_set_focus_hadjustment() for setting
 * the horizontal adjustment.
 *
 * The adjustments have to be in pixel units and in the same coordinate 
 * system as the allocation for immediate children of the container. 
 */
void
gtk_container_set_focus_vadjustment (GtkContainer  *container,
				     GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  if (adjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (adjustment)
    g_object_ref (adjustment);

  g_object_set_qdata_full (G_OBJECT (container),
			   vadjustment_key_id,
			   adjustment,
			   g_object_unref);
}

/**
 * gtk_container_get_focus_vadjustment:
 * @container: a #GtkContainer
 *
 * Retrieves the vertical focus adjustment for the container. See
 * gtk_container_set_focus_vadjustment().
 *
 * Return value: (transfer none): the vertical focus adjustment, or %NULL if
 *   none has been set.
 **/
GtkAdjustment *
gtk_container_get_focus_vadjustment (GtkContainer *container)
{
  GtkAdjustment *vadjustment;
    
  g_return_val_if_fail (GTK_IS_CONTAINER (container), NULL);

  vadjustment = g_object_get_qdata (G_OBJECT (container), vadjustment_key_id);

  return vadjustment;
}

/**
 * gtk_container_set_focus_hadjustment:
 * @container: a #GtkContainer
 * @adjustment: an adjustment which should be adjusted when the focus is 
 *   moved among the descendents of @container
 * 
 * Hooks up an adjustment to focus handling in a container, so when a child 
 * of the container is focused, the adjustment is scrolled to show that 
 * widget. This function sets the horizontal alignment. 
 * See gtk_scrolled_window_get_hadjustment() for a typical way of obtaining 
 * the adjustment and gtk_container_set_focus_vadjustment() for setting
 * the vertical adjustment.
 *
 * The adjustments have to be in pixel units and in the same coordinate 
 * system as the allocation for immediate children of the container. 
 */
void
gtk_container_set_focus_hadjustment (GtkContainer  *container,
				     GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_CONTAINER (container));
  if (adjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (adjustment)
    g_object_ref (adjustment);

  g_object_set_qdata_full (G_OBJECT (container),
			   hadjustment_key_id,
			   adjustment,
			   g_object_unref);
}

/**
 * gtk_container_get_focus_hadjustment:
 * @container: a #GtkContainer
 *
 * Retrieves the horizontal focus adjustment for the container. See
 * gtk_container_set_focus_hadjustment ().
 *
 * Return value: (transfer none): the horizontal focus adjustment, or %NULL if
 *   none has been set.
 **/
GtkAdjustment *
gtk_container_get_focus_hadjustment (GtkContainer *container)
{
  GtkAdjustment *hadjustment;

  g_return_val_if_fail (GTK_IS_CONTAINER (container), NULL);

  hadjustment = g_object_get_qdata (G_OBJECT (container), hadjustment_key_id);

  return hadjustment;
}


static void
gtk_container_show_all (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_CONTAINER (widget));

  gtk_container_foreach (GTK_CONTAINER (widget),
			 (GtkCallback) gtk_widget_show_all,
			 NULL);
  gtk_widget_show (widget);
}

static void
gtk_container_hide_all (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_CONTAINER (widget));

  gtk_widget_hide (widget);
  gtk_container_foreach (GTK_CONTAINER (widget),
			 (GtkCallback) gtk_widget_hide_all,
			 NULL);
}


static void
gtk_container_expose_child (GtkWidget *child,
			    gpointer   client_data)
{
  struct {
    GtkWidget *container;
    GdkEventExpose *event;
  } *data = client_data;
  
  gtk_container_propagate_expose (GTK_CONTAINER (data->container),
				  child,
				  data->event);
}

static gint 
gtk_container_expose (GtkWidget      *widget,
		      GdkEventExpose *event)
{
  struct {
    GtkWidget *container;
    GdkEventExpose *event;
  } data;

  g_return_val_if_fail (GTK_IS_CONTAINER (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  
  if (gtk_widget_is_drawable (widget))
    {
      data.container = widget;
      data.event = event;
      
      gtk_container_forall (GTK_CONTAINER (widget),
			    gtk_container_expose_child,
			    &data);
    }   
  
  return FALSE;
}

static void
gtk_container_map_child (GtkWidget *child,
			 gpointer   client_data)
{
  if (gtk_widget_get_visible (child) &&
      GTK_WIDGET_CHILD_VISIBLE (child) &&
      !gtk_widget_get_mapped (child))
    gtk_widget_map (child);
}

static void
gtk_container_map (GtkWidget *widget)
{
  gtk_widget_set_mapped (widget, TRUE);

  gtk_container_forall (GTK_CONTAINER (widget),
			gtk_container_map_child,
			NULL);

  if (gtk_widget_get_has_window (widget))
    gdk_window_show (widget->window);
}

static void
gtk_container_unmap (GtkWidget *widget)
{
  gtk_widget_set_mapped (widget, FALSE);

  if (gtk_widget_get_has_window (widget))
    gdk_window_hide (widget->window);
  else
    gtk_container_forall (GTK_CONTAINER (widget),
			  (GtkCallback)gtk_widget_unmap,
			  NULL);
}

/**
 * gtk_container_propagate_expose:
 * @container: a #GtkContainer
 * @child: a child of @container
 * @event: a expose event sent to container
 *
 * When a container receives an expose event, it must send synthetic
 * expose events to all children that don't have their own #GdkWindows.
 * This function provides a convenient way of doing this. A container,
 * when it receives an expose event, calls gtk_container_propagate_expose()
 * once for each child, passing in the event the container received.
 *
 * gtk_container_propagate_expose() takes care of deciding whether
 * an expose event needs to be sent to the child, intersecting
 * the event's area with the child area, and sending the event.
 *
 * In most cases, a container can simply either simply inherit the
 * #GtkWidget::expose implementation from #GtkContainer, or, do some drawing
 * and then chain to the ::expose implementation from #GtkContainer.
 *
 * Note that the ::expose-event signal has been replaced by a ::draw
 * signal in GTK+ 3, and consequently, gtk_container_propagate_expose()
 * has been replaced by gtk_container_propagate_draw().
 * The <link linkend="http://library.gnome.org/devel/gtk3/3.0/gtk-migrating-2-to-3.html">GTK+ 3 migration guide</link>
 * for hints on how to port from ::expose-event to ::draw.
 *
 **/
void
gtk_container_propagate_expose (GtkContainer   *container,
				GtkWidget      *child,
				GdkEventExpose *event)
{
  GdkEvent *child_event;

  g_return_if_fail (GTK_IS_CONTAINER (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (event != NULL);

  g_assert (child->parent == GTK_WIDGET (container));
  
  if (gtk_widget_is_drawable (child) &&
      !gtk_widget_get_has_window (child) &&
      (child->window == event->window))
    {
      child_event = gdk_event_new (GDK_EXPOSE);
      child_event->expose = *event;
      g_object_ref (child_event->expose.window);

      child_event->expose.region = gtk_widget_region_intersect (child, event->region);
      if (!gdk_region_empty (child_event->expose.region))
	{
	  gdk_region_get_clipbox (child_event->expose.region, &child_event->expose.area);
	  gtk_widget_send_expose (child, child_event);
	}
      gdk_event_free (child_event);
    }
}

#define __GTK_CONTAINER_C__
#include "gtkaliasdef.c"
