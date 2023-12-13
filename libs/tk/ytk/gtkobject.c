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
#include <stdio.h>

#undef GTK_DISABLE_DEPRECATED

#include "gtkobject.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"

#include "gtkalias.h"


enum {
  DESTROY,
  LAST_SIGNAL
};
enum {
  PROP_0,
  PROP_USER_DATA
};


static void       gtk_object_base_class_init     (GtkObjectClass *class);
static void       gtk_object_base_class_finalize (GtkObjectClass *class);
static void       gtk_object_class_init          (GtkObjectClass *klass);
static void       gtk_object_init                (GtkObject      *object,
						  GtkObjectClass *klass);
static void	  gtk_object_set_property	 (GObject	 *object,
						  guint           property_id,
						  const GValue   *value,
						  GParamSpec     *pspec);
static void	  gtk_object_get_property	 (GObject	 *object,
						  guint           property_id,
						  GValue         *value,
						  GParamSpec     *pspec);
static void       gtk_object_dispose            (GObject        *object);
static void       gtk_object_real_destroy        (GtkObject      *object);
static void       gtk_object_finalize            (GObject        *object);
static void       gtk_object_notify_weaks        (GtkObject      *object);

static gpointer    parent_class = NULL;
static guint       object_signals[LAST_SIGNAL] = { 0 };
static GQuark      quark_weakrefs = 0;


/****************************************************
 * GtkObject type, class and instance initialization
 *
 ****************************************************/

GType
gtk_object_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
      {
	sizeof (GtkObjectClass),
	(GBaseInitFunc) gtk_object_base_class_init,
	(GBaseFinalizeFunc) gtk_object_base_class_finalize,
	(GClassInitFunc) gtk_object_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkObject),
	16,		/* n_preallocs */
	(GInstanceInitFunc) gtk_object_init,
	NULL,		/* value_table */
      };
      
      object_type = g_type_register_static (G_TYPE_INITIALLY_UNOWNED, I_("GtkObject"), 
					    &object_info, G_TYPE_FLAG_ABSTRACT);
    }

  return object_type;
}

static void
gtk_object_base_class_init (GtkObjectClass *class)
{
  /* reset instance specifc methods that don't get inherited */
  class->get_arg = NULL;
  class->set_arg = NULL;
}

static void
gtk_object_base_class_finalize (GtkObjectClass *class)
{
}

static inline gboolean
gtk_arg_set_from_value (GtkArg       *arg,
			const GValue *value,
			gboolean      copy_string)
{
  switch (G_TYPE_FUNDAMENTAL (arg->type))
    {
    case G_TYPE_CHAR:           GTK_VALUE_CHAR (*arg) = g_value_get_char (value);       break;
    case G_TYPE_UCHAR:          GTK_VALUE_UCHAR (*arg) = g_value_get_uchar (value);     break;
    case G_TYPE_BOOLEAN:        GTK_VALUE_BOOL (*arg) = g_value_get_boolean (value);    break;
    case G_TYPE_INT:            GTK_VALUE_INT (*arg) = g_value_get_int (value);         break;
    case G_TYPE_UINT:           GTK_VALUE_UINT (*arg) = g_value_get_uint (value);       break;
    case G_TYPE_LONG:           GTK_VALUE_LONG (*arg) = g_value_get_long (value);       break;
    case G_TYPE_ULONG:          GTK_VALUE_ULONG (*arg) = g_value_get_ulong (value);     break;
    case G_TYPE_ENUM:           GTK_VALUE_ENUM (*arg) = g_value_get_enum (value);       break;
    case G_TYPE_FLAGS:          GTK_VALUE_FLAGS (*arg) = g_value_get_flags (value);     break;
    case G_TYPE_FLOAT:          GTK_VALUE_FLOAT (*arg) = g_value_get_float (value);     break;
    case G_TYPE_DOUBLE:         GTK_VALUE_DOUBLE (*arg) = g_value_get_double (value);   break;
    case G_TYPE_BOXED:          GTK_VALUE_BOXED (*arg) = g_value_get_boxed (value);     break;
    case G_TYPE_POINTER:        GTK_VALUE_POINTER (*arg) = g_value_get_pointer (value); break;
    case G_TYPE_OBJECT:         GTK_VALUE_POINTER (*arg) = g_value_get_object (value);  break;
    case G_TYPE_STRING:         if (copy_string)
      GTK_VALUE_STRING (*arg) = g_value_dup_string (value);
    else
      GTK_VALUE_STRING (*arg) = (char *) g_value_get_string (value);
    break;
    default:
      return FALSE;
    }
  return TRUE;
}

static inline gboolean
gtk_arg_to_value (GtkArg *arg,
		  GValue *value)
{
  switch (G_TYPE_FUNDAMENTAL (arg->type))
    {
    case G_TYPE_CHAR:           g_value_set_char (value, GTK_VALUE_CHAR (*arg));        break;
    case G_TYPE_UCHAR:          g_value_set_uchar (value, GTK_VALUE_UCHAR (*arg));      break;
    case G_TYPE_BOOLEAN:        g_value_set_boolean (value, GTK_VALUE_BOOL (*arg));     break;
    case G_TYPE_INT:            g_value_set_int (value, GTK_VALUE_INT (*arg));          break;
    case G_TYPE_UINT:           g_value_set_uint (value, GTK_VALUE_UINT (*arg));        break;
    case G_TYPE_LONG:           g_value_set_long (value, GTK_VALUE_LONG (*arg));        break;
    case G_TYPE_ULONG:          g_value_set_ulong (value, GTK_VALUE_ULONG (*arg));      break;
    case G_TYPE_ENUM:           g_value_set_enum (value, GTK_VALUE_ENUM (*arg));        break;
    case G_TYPE_FLAGS:          g_value_set_flags (value, GTK_VALUE_FLAGS (*arg));      break;
    case G_TYPE_FLOAT:          g_value_set_float (value, GTK_VALUE_FLOAT (*arg));      break;
    case G_TYPE_DOUBLE:         g_value_set_double (value, GTK_VALUE_DOUBLE (*arg));    break;
    case G_TYPE_STRING:         g_value_set_string (value, GTK_VALUE_STRING (*arg));    break;
    case G_TYPE_BOXED:          g_value_set_boxed (value, GTK_VALUE_BOXED (*arg));      break;
    case G_TYPE_POINTER:        g_value_set_pointer (value, GTK_VALUE_POINTER (*arg));  break;
    case G_TYPE_OBJECT:         g_value_set_object (value, GTK_VALUE_POINTER (*arg));   break;
    default:
      return FALSE;
    }
  return TRUE;
}

static void
gtk_arg_proxy_set_property (GObject      *object,
			    guint         property_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  GtkObjectClass *class = g_type_class_peek (pspec->owner_type);
  GtkArg arg;

  g_return_if_fail (class->set_arg != NULL);

  memset (&arg, 0, sizeof (arg));
  arg.type = G_VALUE_TYPE (value);
  gtk_arg_set_from_value (&arg, value, FALSE);
  arg.name = pspec->name;
  class->set_arg (GTK_OBJECT (object), &arg, property_id);
}

static void
gtk_arg_proxy_get_property (GObject     *object,
			    guint        property_id,
			    GValue      *value,
			    GParamSpec  *pspec)
{
  GtkObjectClass *class = g_type_class_peek (pspec->owner_type);
  GtkArg arg;

  g_return_if_fail (class->get_arg != NULL);

  memset (&arg, 0, sizeof (arg));
  arg.type = G_VALUE_TYPE (value);
  arg.name = pspec->name;
  class->get_arg (GTK_OBJECT (object), &arg, property_id);
  gtk_arg_to_value (&arg, value);
}

void
gtk_object_add_arg_type (const gchar *arg_name,
			 GType        arg_type,
			 guint        arg_flags,
			 guint        arg_id)
{
  GObjectClass *oclass;
  GParamSpec *pspec;
  gchar *type_name, *pname;
  GType type;
  
  g_return_if_fail (arg_name != NULL);
  g_return_if_fail (arg_type > G_TYPE_NONE);
  g_return_if_fail (arg_id > 0);
  g_return_if_fail (arg_flags & G_PARAM_READWRITE);
  if (arg_flags & G_PARAM_CONSTRUCT)
    g_return_if_fail ((arg_flags & G_PARAM_CONSTRUCT_ONLY) == 0);
  if (arg_flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY))
    g_return_if_fail (arg_flags & G_PARAM_WRITABLE);
  g_return_if_fail ((arg_flags & ~(G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME)) == 0);

  pname = strchr (arg_name, ':');
  g_return_if_fail (pname && pname[1] == ':');

  type_name = g_strndup (arg_name, pname - arg_name);
  pname += 2;
  type = g_type_from_name (type_name);
  g_free (type_name);
  g_return_if_fail (G_TYPE_IS_OBJECT (type));

  oclass = gtk_type_class (type);
  if (arg_flags & G_PARAM_READABLE)
    {
      if (oclass->get_property && oclass->get_property != gtk_arg_proxy_get_property)
	{
	  g_warning (G_STRLOC ": GtkArg compatibility code can't be mixed with customized %s.get_property() implementation",
		     g_type_name (type));
	  return;
	}
      oclass->get_property = gtk_arg_proxy_get_property;
    }
  if (arg_flags & G_PARAM_WRITABLE)
    {
      if (oclass->set_property && oclass->set_property != gtk_arg_proxy_set_property)
	{
	  g_warning (G_STRLOC ": GtkArg compatibility code can't be mixed with customized %s.set_property() implementation",
		     g_type_name (type));
	  return;
	}
      oclass->set_property = gtk_arg_proxy_set_property;
    }
  switch (G_TYPE_FUNDAMENTAL (arg_type))
    {
    case G_TYPE_ENUM:
      pspec = g_param_spec_enum (pname, NULL, NULL, arg_type, 0, arg_flags);
      break;
    case G_TYPE_FLAGS:
      pspec = g_param_spec_flags (pname, NULL, NULL, arg_type, 0, arg_flags);
      break;
    case G_TYPE_CHAR:
      pspec = g_param_spec_char (pname, NULL, NULL, -128, 127, 0, arg_flags);
      break;
    case G_TYPE_UCHAR:
      pspec = g_param_spec_uchar (pname, NULL, NULL, 0, 255, 0, arg_flags);
      break;
    case G_TYPE_BOOLEAN:
      pspec = g_param_spec_boolean (pname, NULL, NULL, FALSE, arg_flags);
      break;
    case G_TYPE_INT:
      pspec = g_param_spec_int (pname, NULL, NULL, G_MININT, G_MAXINT, 0, arg_flags);
      break;
    case G_TYPE_UINT:
      pspec = g_param_spec_uint (pname, NULL, NULL, 0, G_MAXUINT, 0, arg_flags);
      break;
    case G_TYPE_FLOAT:
      pspec = g_param_spec_float (pname, NULL, NULL, -G_MAXFLOAT, G_MAXFLOAT, 0, arg_flags);
      break;
    case G_TYPE_DOUBLE:
      pspec = g_param_spec_double (pname, NULL, NULL, -G_MAXDOUBLE, G_MAXDOUBLE, 0, arg_flags);
      break;
    case G_TYPE_STRING:
      pspec = g_param_spec_string (pname, NULL, NULL, NULL, arg_flags);
      break;
    case G_TYPE_POINTER:
      pspec = g_param_spec_pointer (pname, NULL, NULL, arg_flags);
      break;
    case G_TYPE_OBJECT:
      pspec = g_param_spec_object (pname, NULL, NULL, arg_type, arg_flags);
      break;
    case G_TYPE_BOXED:
      if (!G_TYPE_IS_FUNDAMENTAL (arg_type))
	{
	  pspec = g_param_spec_boxed (pname, NULL, NULL, arg_type, arg_flags);
	  break;
	}
    default:
      g_warning (G_STRLOC ": Property type `%s' is not supported by the GtkArg compatibility code",
		 g_type_name (arg_type));
      return;
    }
  g_object_class_install_property (oclass, arg_id, pspec);
}

static guint (*gobject_floating_flag_handler) (GtkObject*,gint) = NULL;

static guint
gtk_object_floating_flag_handler (GtkObject *object,
                                  gint       job)
{
  /* FIXME: remove this whole thing once GTK+ breaks ABI */
  if (!GTK_IS_OBJECT (object))
    return gobject_floating_flag_handler (object, job);
  switch (job)
    {
      guint32 oldvalue;
    case +1:    /* force floating if possible */
      do
        oldvalue = g_atomic_int_get (&object->flags);
      while (!g_atomic_int_compare_and_exchange ((gint *)&object->flags, oldvalue, oldvalue | GTK_FLOATING));
      return oldvalue & GTK_FLOATING;
    case -1:    /* sink if possible */
      do
        oldvalue = g_atomic_int_get (&object->flags);
      while (!g_atomic_int_compare_and_exchange ((gint *)&object->flags, oldvalue, oldvalue & ~(guint32) GTK_FLOATING));
      return oldvalue & GTK_FLOATING;
    default:    /* check floating */
      return 0 != (g_atomic_int_get (&object->flags) & GTK_FLOATING);
    }
}

static void
gtk_object_class_init (GtkObjectClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  gboolean is_glib_2_10_1;

  parent_class = g_type_class_ref (G_TYPE_OBJECT);

  is_glib_2_10_1 = g_object_compat_control (3, &gobject_floating_flag_handler);
  if (!is_glib_2_10_1)
    g_error ("this version of Gtk+ requires GLib-2.10.1");
  g_object_compat_control (2, gtk_object_floating_flag_handler);

  gobject_class->set_property = gtk_object_set_property;
  gobject_class->get_property = gtk_object_get_property;
  gobject_class->dispose = gtk_object_dispose;
  gobject_class->finalize = gtk_object_finalize;

  class->destroy = gtk_object_real_destroy;

  g_object_class_install_property (gobject_class,
				   PROP_USER_DATA,
				   g_param_spec_pointer ("user-data", 
							 P_("User Data"),
							 P_("Anonymous User Data Pointer"),
							 GTK_PARAM_READWRITE));
  object_signals[DESTROY] =
    g_signal_new (I_("destroy"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
		  G_STRUCT_OFFSET (GtkObjectClass, destroy),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
gtk_object_init (GtkObject      *object,
		 GtkObjectClass *klass)
{
  gboolean was_floating;
  /* sink the GInitiallyUnowned floating flag */
  was_floating = gobject_floating_flag_handler (object, -1);
  /* set GTK_FLOATING via gtk_object_floating_flag_handler */
  if (was_floating)
    g_object_force_floating (G_OBJECT (object));
}

/********************************************
 * Functions to end a GtkObject's life time
 *
 ********************************************/
void
gtk_object_destroy (GtkObject *object)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));
  
  if (!(GTK_OBJECT_FLAGS (object) & GTK_IN_DESTRUCTION))
    g_object_run_dispose (G_OBJECT (object));
}

static void
gtk_object_dispose (GObject *gobject)
{
  GtkObject *object = GTK_OBJECT (gobject);

  /* guard against reinvocations during
   * destruction with the GTK_IN_DESTRUCTION flag.
   */
  if (!(GTK_OBJECT_FLAGS (object) & GTK_IN_DESTRUCTION))
    {
      GTK_OBJECT_SET_FLAGS (object, GTK_IN_DESTRUCTION);
      
      g_signal_emit (object, object_signals[DESTROY], 0);
      
      GTK_OBJECT_UNSET_FLAGS (object, GTK_IN_DESTRUCTION);
    }

  G_OBJECT_CLASS (parent_class)->dispose (gobject);
}

static void
gtk_object_real_destroy (GtkObject *object)
{
  g_signal_handlers_destroy (object);
}

static void
gtk_object_finalize (GObject *gobject)
{
  GtkObject *object = GTK_OBJECT (gobject);

  if (g_object_is_floating (object))
    {
      g_warning ("A floating object was finalized. This means that someone\n"
		 "called g_object_unref() on an object that had only a floating\n"
		 "reference; the initial floating reference is not owned by anyone\n"
		 "and must be removed with g_object_ref_sink().");
    }
  
  gtk_object_notify_weaks (object);
  
  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

/*****************************************
 * GtkObject argument handlers
 *
 *****************************************/

static void
gtk_object_set_property (GObject      *object,
			 guint         property_id,
			 const GValue *value,
			 GParamSpec   *pspec)
{
  switch (property_id)
    {
    case PROP_USER_DATA:
      g_object_set_data (G_OBJECT (object), I_("user_data"), g_value_get_pointer (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_object_get_property (GObject     *object,
			 guint        property_id,
			 GValue      *value,
			 GParamSpec  *pspec)
{
  switch (property_id)
    {
    case PROP_USER_DATA:
      g_value_set_pointer (value, g_object_get_data (G_OBJECT (object), "user_data"));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

void
gtk_object_sink (GtkObject *object)
{
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_object_ref_sink (object);
  g_object_unref (object);
}

/*****************************************
 * Weak references.
 *
 * Weak refs are very similar to the old "destroy" signal.  They allow
 * one to register a callback that is called when the weakly
 * referenced object is finalized.
 *  
 * They are not implemented as a signal because they really are
 * special and need to be used with great care.  Unlike signals, which
 * should be able to execute any code whatsoever.
 * 
 * A weakref callback is not allowed to retain a reference to the
 * object.  Object data keys may be retrieved in a weak reference
 * callback.
 * 
 * A weakref callback is called at most once.
 *
 *****************************************/

typedef struct _GtkWeakRef	GtkWeakRef;

struct _GtkWeakRef
{
  GtkWeakRef	 *next;
  GDestroyNotify  notify;
  gpointer        data;
};

void
gtk_object_weakref (GtkObject      *object,
		    GDestroyNotify  notify,
		    gpointer        data)
{
  GtkWeakRef *weak;

  g_return_if_fail (notify != NULL);
  g_return_if_fail (GTK_IS_OBJECT (object));

  if (!quark_weakrefs)
    quark_weakrefs = g_quark_from_static_string ("gtk-weakrefs");

  weak = g_new (GtkWeakRef, 1);
  weak->next = g_object_get_qdata (G_OBJECT (object), quark_weakrefs);
  weak->notify = notify;
  weak->data = data;
  g_object_set_qdata (G_OBJECT (object), quark_weakrefs, weak);
}

void
gtk_object_weakunref (GtkObject      *object,
		      GDestroyNotify  notify,
		      gpointer        data)
{
  GtkWeakRef *weaks, *w, **wp;

  g_return_if_fail (GTK_IS_OBJECT (object));

  if (!quark_weakrefs)
    return;

  weaks = g_object_get_qdata (G_OBJECT (object), quark_weakrefs);
  for (wp = &weaks; *wp; wp = &(*wp)->next)
    {
      w = *wp;
      if (w->notify == notify && w->data == data)
	{
	  if (w == weaks)
	    g_object_set_qdata (G_OBJECT (object), quark_weakrefs, w->next);
	  else
	    *wp = w->next;
	  g_free (w);
	  return;
	}
    }
}

static void
gtk_object_notify_weaks (GtkObject *object)
{
  if (quark_weakrefs)
    {
      GtkWeakRef *w1, *w2;
      
      w1 = g_object_get_qdata (G_OBJECT (object), quark_weakrefs);
      
      while (w1)
	{
	  w1->notify (w1->data);
	  w2 = w1->next;
	  g_free (w1);
	  w1 = w2;
	}
    }
}

GtkObject*
gtk_object_new (GType        object_type,
		const gchar *first_property_name,
		...)
{
  GtkObject *object;
  va_list var_args;

  g_return_val_if_fail (G_TYPE_IS_OBJECT (object_type), NULL);

  va_start (var_args, first_property_name);
  object = (GtkObject *)g_object_new_valist (object_type, first_property_name, var_args);
  va_end (var_args);

  return object;
}

void
gtk_object_get (GtkObject   *object,
		const gchar *first_property_name,
		...)
{
  va_list var_args;
  
  g_return_if_fail (GTK_IS_OBJECT (object));
  
  va_start (var_args, first_property_name);
  g_object_get_valist (G_OBJECT (object), first_property_name, var_args);
  va_end (var_args);
}

void
gtk_object_set (GtkObject   *object,
		const gchar *first_property_name,
		...)
{
  va_list var_args;
  
  g_return_if_fail (GTK_IS_OBJECT (object));
  
  va_start (var_args, first_property_name);
  g_object_set_valist (G_OBJECT (object), first_property_name, var_args);
  va_end (var_args);
}

/*****************************************
 * GtkObject object_data mechanism
 *
 *****************************************/

void
gtk_object_set_data_by_id (GtkObject        *object,
			   GQuark	     data_id,
			   gpointer          data)
{
  g_return_if_fail (GTK_IS_OBJECT (object));
  
  g_datalist_id_set_data (&G_OBJECT (object)->qdata, data_id, data);
}

void
gtk_object_set_data (GtkObject        *object,
		     const gchar      *key,
		     gpointer          data)
{
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (key != NULL);
  
  g_datalist_set_data (&G_OBJECT (object)->qdata, key, data);
}

void
gtk_object_set_data_by_id_full (GtkObject      *object,
				GQuark		data_id,
				gpointer        data,
				GDestroyNotify  destroy)
{
  g_return_if_fail (GTK_IS_OBJECT (object));

  g_datalist_id_set_data_full (&G_OBJECT (object)->qdata, data_id, data, destroy);
}

void
gtk_object_set_data_full (GtkObject      *object,
			  const gchar    *key,
			  gpointer        data,
			  GDestroyNotify  destroy)
{
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (key != NULL);

  g_datalist_set_data_full (&G_OBJECT (object)->qdata, key, data, destroy);
}

gpointer
gtk_object_get_data_by_id (GtkObject   *object,
			   GQuark       data_id)
{
  g_return_val_if_fail (GTK_IS_OBJECT (object), NULL);

  return g_datalist_id_get_data (&G_OBJECT (object)->qdata, data_id);
}

gpointer
gtk_object_get_data (GtkObject   *object,
		     const gchar *key)
{
  g_return_val_if_fail (GTK_IS_OBJECT (object), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return g_datalist_get_data (&G_OBJECT (object)->qdata, key);
}

void
gtk_object_remove_data_by_id (GtkObject   *object,
			      GQuark       data_id)
{
  g_return_if_fail (GTK_IS_OBJECT (object));

  g_datalist_id_remove_data (&G_OBJECT (object)->qdata, data_id);
}

void
gtk_object_remove_data (GtkObject   *object,
			const gchar *key)
{
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (key != NULL);

  g_datalist_remove_data (&G_OBJECT (object)->qdata, key);
}

void
gtk_object_remove_no_notify_by_id (GtkObject      *object,
				   GQuark          key_id)
{
  g_return_if_fail (GTK_IS_OBJECT (object));

  g_datalist_id_remove_no_notify (&G_OBJECT (object)->qdata, key_id);
}

void
gtk_object_remove_no_notify (GtkObject       *object,
			     const gchar     *key)
{
  g_return_if_fail (GTK_IS_OBJECT (object));
  g_return_if_fail (key != NULL);

  g_datalist_remove_no_notify (&G_OBJECT (object)->qdata, key);
}

void
gtk_object_set_user_data (GtkObject *object,
			  gpointer   data)
{
  g_return_if_fail (GTK_IS_OBJECT (object));

  g_object_set_data (G_OBJECT (object), "user_data", data);
}

gpointer
gtk_object_get_user_data (GtkObject *object)
{
  g_return_val_if_fail (GTK_IS_OBJECT (object), NULL);

  return g_object_get_data (G_OBJECT (object), "user_data");
}

GtkObject*
gtk_object_ref (GtkObject *object)
{
  g_return_val_if_fail (GTK_IS_OBJECT (object), NULL);

  return (GtkObject*) g_object_ref ((GObject*) object);
}

void
gtk_object_unref (GtkObject *object)
{
  g_return_if_fail (GTK_IS_OBJECT (object));

  g_object_unref ((GObject*) object);
}

#define __GTK_OBJECT_C__
#include "gtkaliasdef.c"
