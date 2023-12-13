/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkAccelLabel: GtkLabel with accelerator monitoring facilities.
 * Copyright (C) 1998 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include <string.h>

#include "gtkaccellabel.h"
#include "gtkaccelmap.h"
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"
#include <gdk/gdkkeysyms.h>

/**
 * SECTION:gtkaccellabel
 * @Short_description: A label which displays an accelerator key on the right of the text
 * @Title: GtkAccelLabel
 * @See_also: #GtkItemFactory, #GtkAccelGroup
 *
 * The #GtkAccelLabel widget is a subclass of #GtkLabel that also displays an
 * accelerator key on the right of the label text, e.g. 'Ctl+S'.
 * It is commonly used in menus to show the keyboard short-cuts for commands.
 *
 * The accelerator key to display is not set explicitly.
 * Instead, the #GtkAccelLabel displays the accelerators which have been added to
 * a particular widget. This widget is set by calling
 * gtk_accel_label_set_accel_widget().
 *
 * For example, a #GtkMenuItem widget may have an accelerator added to emit the
 * "activate" signal when the 'Ctl+S' key combination is pressed.
 * A #GtkAccelLabel is created and added to the #GtkMenuItem, and
 * gtk_accel_label_set_accel_widget() is called with the #GtkMenuItem as the
 * second argument. The #GtkAccelLabel will now display 'Ctl+S' after its label.
 *
 * Note that creating a #GtkMenuItem with gtk_menu_item_new_with_label() (or
 * one of the similar functions for #GtkCheckMenuItem and #GtkRadioMenuItem)
 * automatically adds a #GtkAccelLabel to the #GtkMenuItem and calls
 * gtk_accel_label_set_accel_widget() to set it up for you.
 *
 * A #GtkAccelLabel will only display accelerators which have %GTK_ACCEL_VISIBLE
 * set (see #GtkAccelFlags).
 * A #GtkAccelLabel can display multiple accelerators and even signal names,
 * though it is almost always used to display just one accelerator key.
 * <example>
 * <title>Creating a simple menu item with an accelerator key.</title>
 * <programlisting>
 *   GtkWidget *save_item;
 *   GtkAccelGroup *accel_group;
 *
 *   /<!---->* Create a GtkAccelGroup and add it to the window. *<!---->/
 *   accel_group = gtk_accel_group_new (<!-- -->);
 *   gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
 *
 *   /<!---->* Create the menu item using the convenience function. *<!---->/
 *   save_item = gtk_menu_item_new_with_label ("Save");
 *   gtk_widget_show (save_item);
 *   gtk_container_add (GTK_CONTAINER (menu), save_item);
 *
 *   /<!---->* Now add the accelerator to the GtkMenuItem. Note that since we called
 *      gtk_menu_item_new_with_label(<!-- -->) to create the GtkMenuItem the
 *      GtkAccelLabel is automatically set up to display the GtkMenuItem
 *      accelerators. We just need to make sure we use GTK_ACCEL_VISIBLE here. *<!---->/
 *   gtk_widget_add_accelerator (save_item, "activate", accel_group,
 *                               GDK_s, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
 * </programlisting>
 * </example>
 */

enum {
  PROP_0,
  PROP_ACCEL_CLOSURE,
  PROP_ACCEL_WIDGET
};

static void         gtk_accel_label_set_property (GObject            *object,
						  guint               prop_id,
						  const GValue       *value,
						  GParamSpec         *pspec);
static void         gtk_accel_label_get_property (GObject            *object,
						  guint               prop_id,
						  GValue             *value,
						  GParamSpec         *pspec);
static void         gtk_accel_label_destroy      (GtkObject          *object);
static void         gtk_accel_label_finalize     (GObject            *object);
static void         gtk_accel_label_size_request (GtkWidget          *widget,
						  GtkRequisition     *requisition);
static gboolean     gtk_accel_label_expose_event (GtkWidget          *widget,
						  GdkEventExpose     *event);
static const gchar *gtk_accel_label_get_string   (GtkAccelLabel      *accel_label);


G_DEFINE_TYPE (GtkAccelLabel, gtk_accel_label, GTK_TYPE_LABEL)

static void
gtk_accel_label_class_init (GtkAccelLabelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  
  gobject_class->finalize = gtk_accel_label_finalize;
  gobject_class->set_property = gtk_accel_label_set_property;
  gobject_class->get_property = gtk_accel_label_get_property;
  
  object_class->destroy = gtk_accel_label_destroy;
   
  widget_class->size_request = gtk_accel_label_size_request;
  widget_class->expose_event = gtk_accel_label_expose_event;

  class->signal_quote1 = g_strdup ("<:");
  class->signal_quote2 = g_strdup (":>");

#ifndef GDK_WINDOWING_QUARTZ
  /* This is the text that should appear next to menu accelerators
   * that use the shift key. If the text on this key isn't typically
   * translated on keyboards used for your language, don't translate
   * this.
   */
  class->mod_name_shift = g_strdup (C_("keyboard label", "Shift"));
  /* This is the text that should appear next to menu accelerators
   * that use the control key. If the text on this key isn't typically
   * translated on keyboards used for your language, don't translate
   * this.
   */
  class->mod_name_control = g_strdup (C_("keyboard label", "Ctrl"));
  /* This is the text that should appear next to menu accelerators
   * that use the alt key. If the text on this key isn't typically
   * translated on keyboards used for your language, don't translate
   * this.
   */
  class->mod_name_alt = g_strdup (C_("keyboard label", "Alt"));
  class->mod_separator = g_strdup ("+");
#else /* GDK_WINDOWING_QUARTZ */

  /* U+21E7 UPWARDS WHITE ARROW */
  class->mod_name_shift = g_strdup ("\xe2\x87\xa7");
  /* U+2303 UP ARROWHEAD */
  class->mod_name_control = g_strdup ("\xe2\x8c\x83");
  /* U+2325 OPTION KEY */
  class->mod_name_alt = g_strdup ("\xe2\x8c\xa5");
  class->mod_separator = g_strdup ("");

#endif /* GDK_WINDOWING_QUARTZ */

  class->accel_seperator = g_strdup (" / ");
  class->latin1_to_char = TRUE;
  
  g_object_class_install_property (gobject_class,
                                   PROP_ACCEL_CLOSURE,
                                   g_param_spec_boxed ("accel-closure",
						       P_("Accelerator Closure"),
						       P_("The closure to be monitored for accelerator changes"),
						       G_TYPE_CLOSURE,
						       GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_ACCEL_WIDGET,
                                   g_param_spec_object ("accel-widget",
                                                        P_("Accelerator Widget"),
                                                        P_("The widget to be monitored for accelerator changes"),
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE));
}

static void
gtk_accel_label_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  GtkAccelLabel  *accel_label;

  accel_label = GTK_ACCEL_LABEL (object);

  switch (prop_id)
    {
    case PROP_ACCEL_CLOSURE:
      gtk_accel_label_set_accel_closure (accel_label, g_value_get_boxed (value));
      break;
    case PROP_ACCEL_WIDGET:
      gtk_accel_label_set_accel_widget (accel_label, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_accel_label_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
  GtkAccelLabel  *accel_label;

  accel_label = GTK_ACCEL_LABEL (object);

  switch (prop_id)
    {
    case PROP_ACCEL_CLOSURE:
      g_value_set_boxed (value, accel_label->accel_closure);
      break;
    case PROP_ACCEL_WIDGET:
      g_value_set_object (value, accel_label->accel_widget);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_accel_label_init (GtkAccelLabel *accel_label)
{
  accel_label->accel_padding = 3;
  accel_label->accel_widget = NULL;
  accel_label->accel_closure = NULL;
  accel_label->accel_group = NULL;
  accel_label->accel_string = NULL;
}

/**
 * gtk_accel_label_new:
 * @string: the label string. Must be non-%NULL.
 *
 * Creates a new #GtkAccelLabel.
 *
 * Returns: a new #GtkAccelLabel.
 */
GtkWidget*
gtk_accel_label_new (const gchar *string)
{
  GtkAccelLabel *accel_label;
  
  g_return_val_if_fail (string != NULL, NULL);
  
  accel_label = g_object_new (GTK_TYPE_ACCEL_LABEL, NULL);
  
  gtk_label_set_text (GTK_LABEL (accel_label), string);
  
  return GTK_WIDGET (accel_label);
}

static void
gtk_accel_label_destroy (GtkObject *object)
{
  GtkAccelLabel *accel_label = GTK_ACCEL_LABEL (object);

  gtk_accel_label_set_accel_widget (accel_label, NULL);
  gtk_accel_label_set_accel_closure (accel_label, NULL);
  
  GTK_OBJECT_CLASS (gtk_accel_label_parent_class)->destroy (object);
}

static void
gtk_accel_label_finalize (GObject *object)
{
  GtkAccelLabel *accel_label = GTK_ACCEL_LABEL (object);

  g_free (accel_label->accel_string);
  
  G_OBJECT_CLASS (gtk_accel_label_parent_class)->finalize (object);
}

/**
 * gtk_accel_label_get_accel_widget:
 * @accel_label: a #GtkAccelLabel
 *
 * Fetches the widget monitored by this accelerator label. See
 * gtk_accel_label_set_accel_widget().
 *
 * Returns: (transfer none): the object monitored by the accelerator label, or %NULL.
 **/
GtkWidget*
gtk_accel_label_get_accel_widget (GtkAccelLabel *accel_label)
{
  g_return_val_if_fail (GTK_IS_ACCEL_LABEL (accel_label), NULL);

  return accel_label->accel_widget;
}

/**
 * gtk_accel_label_get_accel_width:
 * @accel_label: a #GtkAccelLabel.
 *
 * Returns the width needed to display the accelerator key(s).
 * This is used by menus to align all of the #GtkMenuItem widgets, and shouldn't
 * be needed by applications.
 *
 * Returns: the width needed to display the accelerator key(s).
 */
guint
gtk_accel_label_get_accel_width (GtkAccelLabel *accel_label)
{
  g_return_val_if_fail (GTK_IS_ACCEL_LABEL (accel_label), 0);
  
  return (accel_label->accel_string_width +
	  (accel_label->accel_string_width ? accel_label->accel_padding : 0));
}

static void
gtk_accel_label_size_request (GtkWidget	     *widget,
			      GtkRequisition *requisition)
{
  GtkAccelLabel *accel_label = GTK_ACCEL_LABEL (widget);
  PangoLayout *layout;
  gint width;

  GTK_WIDGET_CLASS (gtk_accel_label_parent_class)->size_request (widget, requisition);

  layout = gtk_widget_create_pango_layout (widget, gtk_accel_label_get_string (accel_label));
  pango_layout_get_pixel_size (layout, &width, NULL);
  accel_label->accel_string_width = width;
  
  g_object_unref (layout);
}

static gint
get_first_baseline (PangoLayout *layout)
{
  PangoLayoutIter *iter;
  gint result;

  iter = pango_layout_get_iter (layout);
  result = pango_layout_iter_get_baseline (iter);
  pango_layout_iter_free (iter);

  return PANGO_PIXELS (result);
}

static gboolean 
gtk_accel_label_expose_event (GtkWidget      *widget,
			      GdkEventExpose *event)
{
  GtkAccelLabel *accel_label = GTK_ACCEL_LABEL (widget);
  GtkMisc *misc = GTK_MISC (accel_label);
  GtkTextDirection direction;

  direction = gtk_widget_get_direction (widget);

  if (gtk_widget_is_drawable (widget))
    {
      guint ac_width;
      
      ac_width = gtk_accel_label_get_accel_width (accel_label);
      
      if (widget->allocation.width >= widget->requisition.width + ac_width)
	{
	  PangoLayout *label_layout;
	  PangoLayout *accel_layout;
	  GtkLabel *label = GTK_LABEL (widget);

	  gint x;
	  gint y;
	  
	  label_layout = gtk_label_get_layout (GTK_LABEL (accel_label));

	  if (direction == GTK_TEXT_DIR_RTL)
	    widget->allocation.x += ac_width;
	  widget->allocation.width -= ac_width;
	  if (gtk_label_get_ellipsize (label))
	    pango_layout_set_width (label_layout,
				    pango_layout_get_width (label_layout) 
				    - ac_width * PANGO_SCALE);
	  
	  if (GTK_WIDGET_CLASS (gtk_accel_label_parent_class)->expose_event)
	    GTK_WIDGET_CLASS (gtk_accel_label_parent_class)->expose_event (widget, event);
	  if (direction == GTK_TEXT_DIR_RTL)
	    widget->allocation.x -= ac_width;
	  widget->allocation.width += ac_width;
	  if (gtk_label_get_ellipsize (label))
	    pango_layout_set_width (label_layout,
				    pango_layout_get_width (label_layout) 
				    + ac_width * PANGO_SCALE);
	  
	  if (direction == GTK_TEXT_DIR_RTL)
	    x = widget->allocation.x + misc->xpad;
	  else
	    x = widget->allocation.x + widget->allocation.width - misc->xpad - ac_width;

	  gtk_label_get_layout_offsets (GTK_LABEL (accel_label), NULL, &y);

	  accel_layout = gtk_widget_create_pango_layout (widget, gtk_accel_label_get_string (accel_label));

	  y += get_first_baseline (label_layout) - get_first_baseline (accel_layout);

          gtk_paint_layout (widget->style,
                            widget->window,
                            gtk_widget_get_state (widget),
			    FALSE,
                            &event->area,
                            widget,
                            "accellabel",
                            x, y,
                            accel_layout);                            

          g_object_unref (accel_layout);
	}
      else
	{
	  if (GTK_WIDGET_CLASS (gtk_accel_label_parent_class)->expose_event)
	    GTK_WIDGET_CLASS (gtk_accel_label_parent_class)->expose_event (widget, event);
	}
    }
  
  return FALSE;
}

static void
refetch_widget_accel_closure (GtkAccelLabel *accel_label)
{
  GClosure *closure = NULL;
  GList *clist, *list;
  
  g_return_if_fail (GTK_IS_ACCEL_LABEL (accel_label));
  g_return_if_fail (GTK_IS_WIDGET (accel_label->accel_widget));
  
  clist = gtk_widget_list_accel_closures (accel_label->accel_widget);
  for (list = clist; list; list = list->next)
    {
      /* we just take the first closure used */
      closure = list->data;
      break;
    }
  g_list_free (clist);
  gtk_accel_label_set_accel_closure (accel_label, closure);
}

/**
 * gtk_accel_label_set_accel_widget:
 * @accel_label: a #GtkAccelLabel
 * @accel_widget: the widget to be monitored.
 *
 * Sets the widget to be monitored by this accelerator label. 
 **/
void
gtk_accel_label_set_accel_widget (GtkAccelLabel *accel_label,
				  GtkWidget     *accel_widget)
{
  g_return_if_fail (GTK_IS_ACCEL_LABEL (accel_label));
  if (accel_widget)
    g_return_if_fail (GTK_IS_WIDGET (accel_widget));
    
  if (accel_widget != accel_label->accel_widget)
    {
      if (accel_label->accel_widget)
	{
	  gtk_accel_label_set_accel_closure (accel_label, NULL);
	  g_signal_handlers_disconnect_by_func (accel_label->accel_widget,
						refetch_widget_accel_closure,
						accel_label);
	  g_object_unref (accel_label->accel_widget);
	}
      accel_label->accel_widget = accel_widget;
      if (accel_label->accel_widget)
	{
	  g_object_ref (accel_label->accel_widget);
	  g_signal_connect_object (accel_label->accel_widget, "accel-closures-changed",
				   G_CALLBACK (refetch_widget_accel_closure),
				   accel_label, G_CONNECT_SWAPPED);
	  refetch_widget_accel_closure (accel_label);
	}
      g_object_notify (G_OBJECT (accel_label), "accel-widget");
    }
}

static void
gtk_accel_label_reset (GtkAccelLabel *accel_label)
{
  if (accel_label->accel_string)
    {
      g_free (accel_label->accel_string);
      accel_label->accel_string = NULL;
    }
  
  gtk_widget_queue_resize (GTK_WIDGET (accel_label));
}

static void
check_accel_changed (GtkAccelGroup  *accel_group,
		     guint           keyval,
		     GdkModifierType modifier,
		     GClosure       *accel_closure,
		     GtkAccelLabel  *accel_label)
{
  if (accel_closure == accel_label->accel_closure)
    gtk_accel_label_reset (accel_label);
}

/**
 * gtk_accel_label_set_accel_closure:
 * @accel_label: a #GtkAccelLabel
 * @accel_closure: the closure to monitor for accelerator changes.
 *
 * Sets the closure to be monitored by this accelerator label. The closure
 * must be connected to an accelerator group; see gtk_accel_group_connect().
 **/
void
gtk_accel_label_set_accel_closure (GtkAccelLabel *accel_label,
				   GClosure      *accel_closure)
{
  g_return_if_fail (GTK_IS_ACCEL_LABEL (accel_label));
  if (accel_closure)
    g_return_if_fail (gtk_accel_group_from_accel_closure (accel_closure) != NULL);

  if (accel_closure != accel_label->accel_closure)
    {
      if (accel_label->accel_closure)
	{
	  g_signal_handlers_disconnect_by_func (accel_label->accel_group,
						check_accel_changed,
						accel_label);
	  accel_label->accel_group = NULL;
	  g_closure_unref (accel_label->accel_closure);
	}
      accel_label->accel_closure = accel_closure;
      if (accel_label->accel_closure)
	{
	  g_closure_ref (accel_label->accel_closure);
	  accel_label->accel_group = gtk_accel_group_from_accel_closure (accel_closure);
	  g_signal_connect_object (accel_label->accel_group, "accel-changed",
				   G_CALLBACK (check_accel_changed),
				   accel_label, 0);
	}
      gtk_accel_label_reset (accel_label);
      g_object_notify (G_OBJECT (accel_label), "accel-closure");
    }
}

static gboolean
find_accel (GtkAccelKey *key,
	    GClosure    *closure,
	    gpointer     data)
{
  return data == (gpointer) closure;
}

static const gchar *
gtk_accel_label_get_string (GtkAccelLabel *accel_label)
{
  if (!accel_label->accel_string)
    gtk_accel_label_refetch (accel_label);
  
  return accel_label->accel_string;
}

/* Underscores in key names are better displayed as spaces
 * E.g., Page_Up should be "Page Up"
 */
static void
substitute_underscores (char *str)
{
  char *p;

  for (p = str; *p; p++)
    if (*p == '_')
      *p = ' ';
}

/* On Mac, if the key has symbolic representation (e.g. arrow keys),
 * append it to gstring and return TRUE; otherwise return FALSE.
 * See http://docs.info.apple.com/article.html?path=Mac/10.5/en/cdb_symbs.html 
 * for the list of special keys. */
static gboolean
append_keyval_symbol (guint    accelerator_key,
                      GString *gstring)
{
#ifdef GDK_WINDOWING_QUARTZ
  switch (accelerator_key)
  {
  case GDK_Return:
    /* U+21A9 LEFTWARDS ARROW WITH HOOK */
    g_string_append (gstring, "\xe2\x86\xa9");
    return TRUE;

  case GDK_ISO_Enter:
    /* U+2324 UP ARROWHEAD BETWEEN TWO HORIZONTAL BARS */
    g_string_append (gstring, "\xe2\x8c\xa4");
    return TRUE;

  case GDK_Left:
    /* U+2190 LEFTWARDS ARROW */
    g_string_append (gstring, "\xe2\x86\x90");
    return TRUE;

  case GDK_Up:
    /* U+2191 UPWARDS ARROW */
    g_string_append (gstring, "\xe2\x86\x91");
    return TRUE;

  case GDK_Right:
    /* U+2192 RIGHTWARDS ARROW */
    g_string_append (gstring, "\xe2\x86\x92");
    return TRUE;

  case GDK_Down:
    /* U+2193 DOWNWARDS ARROW */
    g_string_append (gstring, "\xe2\x86\x93");
    return TRUE;

  case GDK_Page_Up:
    /* U+21DE UPWARDS ARROW WITH DOUBLE STROKE */
    g_string_append (gstring, "\xe2\x87\x9e");
    return TRUE;

  case GDK_Page_Down:
    /* U+21DF DOWNWARDS ARROW WITH DOUBLE STROKE */
    g_string_append (gstring, "\xe2\x87\x9f");
    return TRUE;

  case GDK_Home:
    /* U+2196 NORTH WEST ARROW */
    g_string_append (gstring, "\xe2\x86\x96");
    return TRUE;

  case GDK_End:
    /* U+2198 SOUTH EAST ARROW */
    g_string_append (gstring, "\xe2\x86\x98");
    return TRUE;

  case GDK_Escape:
    /* U+238B BROKEN CIRCLE WITH NORTHWEST ARROW */
    g_string_append (gstring, "\xe2\x8e\x8b");
    return TRUE;

  case GDK_BackSpace:
    /* U+232B ERASE TO THE LEFT */
    g_string_append (gstring, "\xe2\x8c\xab");
    return TRUE;

  case GDK_Delete:
    /* U+2326 ERASE TO THE RIGHT */
    g_string_append (gstring, "\xe2\x8c\xa6");
    return TRUE;

  default:
    return FALSE;
  }
#else /* !GDK_WINDOWING_QUARTZ */
  return FALSE;
#endif
}

gchar *
_gtk_accel_label_class_get_accelerator_label (GtkAccelLabelClass *klass,
					      guint               accelerator_key,
					      GdkModifierType     accelerator_mods)
{
  GString *gstring;
  gboolean seen_mod = FALSE;
  gunichar ch;
  
  gstring = g_string_new ("");
  
  if (accelerator_mods & GDK_SHIFT_MASK)
    {
      g_string_append (gstring, klass->mod_name_shift);
      seen_mod = TRUE;
    }
  if (accelerator_mods & GDK_CONTROL_MASK)
    {
      if (seen_mod)
	g_string_append (gstring, klass->mod_separator);
      g_string_append (gstring, klass->mod_name_control);
      seen_mod = TRUE;
    }
  if (accelerator_mods & GDK_MOD1_MASK)
    {
      if (seen_mod)
	g_string_append (gstring, klass->mod_separator);
      g_string_append (gstring, klass->mod_name_alt);
      seen_mod = TRUE;
    }
  if (accelerator_mods & GDK_MOD2_MASK)
    {
      if (seen_mod)
	g_string_append (gstring, klass->mod_separator);
      
#ifndef GDK_WINDOWING_QUARTZ
      g_string_append (gstring, "Mod2");
#else
      /* Command key symbol U+2318 PLACE OF INTEREST SIGN */
      g_string_append (gstring, "\xe2\x8c\x98");
#endif
      seen_mod = TRUE;
    }
  if (accelerator_mods & GDK_MOD3_MASK)
    {
      if (seen_mod)
	g_string_append (gstring, klass->mod_separator);

      g_string_append (gstring, "Mod3");
      seen_mod = TRUE;
    }
  if (accelerator_mods & GDK_MOD4_MASK)
    {
      if (seen_mod)
	g_string_append (gstring, klass->mod_separator);

      g_string_append (gstring, "Mod4");
      seen_mod = TRUE;
    }
  if (accelerator_mods & GDK_MOD5_MASK)
    {
      if (seen_mod)
	g_string_append (gstring, klass->mod_separator);

      g_string_append (gstring, "Mod5");
      seen_mod = TRUE;
    }
  if (accelerator_mods & GDK_SUPER_MASK)
    {
      if (seen_mod)
	g_string_append (gstring, klass->mod_separator);

      /* This is the text that should appear next to menu accelerators
       * that use the super key. If the text on this key isn't typically
       * translated on keyboards used for your language, don't translate
       * this.
       */
      g_string_append (gstring, C_("keyboard label", "Super"));
      seen_mod = TRUE;
    }
  if (accelerator_mods & GDK_HYPER_MASK)
    {
      if (seen_mod)
	g_string_append (gstring, klass->mod_separator);

      /* This is the text that should appear next to menu accelerators
       * that use the hyper key. If the text on this key isn't typically
       * translated on keyboards used for your language, don't translate
       * this.
       */
      g_string_append (gstring, C_("keyboard label", "Hyper"));
      seen_mod = TRUE;
    }
  if (accelerator_mods & GDK_META_MASK)
    {
      if (seen_mod)
	g_string_append (gstring, klass->mod_separator);

      g_string_append (gstring, C_("keyboard label", "Meta"));
      seen_mod = TRUE;
    }
  if (seen_mod)
    g_string_append (gstring, klass->mod_separator);
  
  ch = gdk_keyval_to_unicode (accelerator_key);
  if (ch && (g_unichar_isgraph (ch) || ch == ' ') &&
      (ch < 0x80 || klass->latin1_to_char))
    {
      switch (ch)
	{
	case ' ':
	  g_string_append (gstring, C_("keyboard label", "Space"));
	  break;
	case '\\':
	  g_string_append (gstring, C_("keyboard label", "Backslash"));
	  break;
	default:
	  g_string_append_unichar (gstring, g_unichar_toupper (ch));
	  break;
	}
    }
  else if (!append_keyval_symbol (accelerator_key, gstring))
    {
      gchar *tmp;

      tmp = gdk_keyval_name (gdk_keyval_to_lower (accelerator_key));
      if (tmp != NULL)
	{
	  if (tmp[0] != 0 && tmp[1] == 0)
	    g_string_append_c (gstring, g_ascii_toupper (tmp[0]));
	  else
	    {
	      const gchar *str;
              str = g_dpgettext2 (GETTEXT_PACKAGE, "keyboard label", tmp);
	      if (str == tmp)
		{
		  g_string_append (gstring, tmp);
		  substitute_underscores (gstring->str);
		}
	      else
		g_string_append (gstring, str);
	    }
	}
    }
  return g_string_free (gstring, FALSE);
}

/**
 * gtk_accel_label_refetch:
 * @accel_label: a #GtkAccelLabel.
 *
 * Recreates the string representing the accelerator keys.
 * This should not be needed since the string is automatically updated whenever
 * accelerators are added or removed from the associated widget.
 *
 * Returns: always returns %FALSE.
 */
gboolean
gtk_accel_label_refetch (GtkAccelLabel *accel_label)
{
  gboolean enable_accels;

  g_return_val_if_fail (GTK_IS_ACCEL_LABEL (accel_label), FALSE);

  if (accel_label->accel_string)
    {
      g_free (accel_label->accel_string);
      accel_label->accel_string = NULL;
    }

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (accel_label)),
                "gtk-enable-accels", &enable_accels,
                NULL);

  if (enable_accels && accel_label->accel_closure)
    {
      GtkAccelKey *key = gtk_accel_group_find (accel_label->accel_group, find_accel, accel_label->accel_closure);

      if (key && key->accel_flags & GTK_ACCEL_VISIBLE)
	{
	  GtkAccelLabelClass *klass;
	  gchar *tmp;

	  klass = GTK_ACCEL_LABEL_GET_CLASS (accel_label);
	  tmp = _gtk_accel_label_class_get_accelerator_label (klass,
							      key->accel_key,
							      key->accel_mods);
	  accel_label->accel_string = g_strconcat ("   ", tmp, NULL);
	  g_free (tmp);
	}
      if (!accel_label->accel_string)
	accel_label->accel_string = g_strdup ("-/-");
    }
  
  if (!accel_label->accel_string)
    accel_label->accel_string = g_strdup ("");

  gtk_widget_queue_resize (GTK_WIDGET (accel_label));

  return FALSE;
}

#define __GTK_ACCEL_LABEL_C__
#include "gtkaliasdef.c"
