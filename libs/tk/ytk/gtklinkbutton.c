/* GTK - The GIMP Toolkit
 * gtklinkbutton.c - an hyperlink-enabled button
 * 
 * Copyright (C) 2006 Emmanuele Bassi <ebassi@gmail.com>
 * All rights reserved.
 *
 * Based on gnome-href code by:
 * 	James Henstridge <james@daa.com.au>
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Cambridge, MA 02139, USA.
 */

#include "config.h"

#include <string.h>

#include "gtkclipboard.h"
#include "gtkdnd.h"
#include "gtkimagemenuitem.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtkstock.h"
#include "gtkshow.h"
#include "gtktooltip.h"
#include "gtklinkbutton.h"
#include "gtkprivate.h"

#include "gtkintl.h"
#include "gtkalias.h"


struct _GtkLinkButtonPrivate
{
  gchar *uri;

  gboolean visited;

  GtkWidget *popup_menu;
};

enum
{
  PROP_0,
  PROP_URI,
  PROP_VISITED
};

#define GTK_LINK_BUTTON_GET_PRIVATE(obj)	(G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_LINK_BUTTON, GtkLinkButtonPrivate))

static void     gtk_link_button_finalize     (GObject          *object);
static void     gtk_link_button_get_property (GObject          *object,
					      guint             prop_id,
					      GValue           *value,
					      GParamSpec       *pspec);
static void     gtk_link_button_set_property (GObject          *object,
					      guint             prop_id,
					      const GValue     *value,
					      GParamSpec       *pspec);
static void     gtk_link_button_add          (GtkContainer     *container,
					      GtkWidget        *widget);
static gboolean gtk_link_button_button_press (GtkWidget        *widget,
					      GdkEventButton   *event);
static void     gtk_link_button_clicked      (GtkButton        *button);
static gboolean gtk_link_button_popup_menu   (GtkWidget        *widget);
static void     gtk_link_button_style_set    (GtkWidget        *widget,
					      GtkStyle         *old_style);
static gboolean gtk_link_button_enter_cb     (GtkWidget        *widget,
					      GdkEventCrossing *event,
					      gpointer          user_data);
static gboolean gtk_link_button_leave_cb     (GtkWidget        *widget,
					      GdkEventCrossing *event,
					      gpointer          user_data);
static void gtk_link_button_drag_data_get_cb (GtkWidget        *widget,
					      GdkDragContext   *context,
					      GtkSelectionData *selection,
					      guint             _info,
					      guint             _time,
					      gpointer          user_data);
static gboolean gtk_link_button_query_tooltip_cb (GtkWidget    *widget,
                                                  gint          x,
                                                  gint          y,
                                                  gboolean      keyboard_tip,
                                                  GtkTooltip   *tooltip,
                                                  gpointer      data);


static const GtkTargetEntry link_drop_types[] = {
  { "text/uri-list", 0, 0 },
  { "_NETSCAPE_URL", 0, 0 }
};

static const GdkColor default_link_color = { 0, 0, 0, 0xeeee };
static const GdkColor default_visited_link_color = { 0, 0x5555, 0x1a1a, 0x8b8b };

static GtkLinkButtonUriFunc uri_func = NULL;
static gpointer uri_func_data = NULL;
static GDestroyNotify uri_func_destroy = NULL;

G_DEFINE_TYPE (GtkLinkButton, gtk_link_button, GTK_TYPE_BUTTON)

static void
gtk_link_button_class_init (GtkLinkButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkButtonClass *button_class = GTK_BUTTON_CLASS (klass);
  
  gobject_class->set_property = gtk_link_button_set_property;
  gobject_class->get_property = gtk_link_button_get_property;
  gobject_class->finalize = gtk_link_button_finalize;
  
  widget_class->button_press_event = gtk_link_button_button_press;
  widget_class->popup_menu = gtk_link_button_popup_menu;
  widget_class->style_set = gtk_link_button_style_set;
  
  container_class->add = gtk_link_button_add;

  button_class->clicked = gtk_link_button_clicked;

  /**
   * GtkLinkButton:uri
   * 
   * The URI bound to this button. 
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
  				   PROP_URI,
  				   g_param_spec_string ("uri",
  				   			P_("URI"),
  				   			P_("The URI bound to this button"),
  				   			NULL,
  				   			G_PARAM_READWRITE));
  /**
   * GtkLinkButton:visited
   * 
   * The 'visited' state of this button. A visited link is drawn in a
   * different color.
   *
   * Since: 2.14
   */
  g_object_class_install_property (gobject_class,
  				   PROP_VISITED,
  				   g_param_spec_boolean ("visited",
                                                         P_("Visited"),
                                                         P_("Whether this link has been visited."),
                                                         FALSE,
                                                         G_PARAM_READWRITE));
  
  g_type_class_add_private (gobject_class, sizeof (GtkLinkButtonPrivate));
}

static void
gtk_link_button_init (GtkLinkButton *link_button)
{
  link_button->priv = GTK_LINK_BUTTON_GET_PRIVATE (link_button),
  
  gtk_button_set_relief (GTK_BUTTON (link_button), GTK_RELIEF_NONE);
  
  g_signal_connect (link_button, "enter-notify-event",
  		    G_CALLBACK (gtk_link_button_enter_cb), NULL);
  g_signal_connect (link_button, "leave-notify-event",
  		    G_CALLBACK (gtk_link_button_leave_cb), NULL);
  g_signal_connect (link_button, "drag-data-get",
  		    G_CALLBACK (gtk_link_button_drag_data_get_cb), NULL);

  g_object_set (link_button, "has-tooltip", TRUE, NULL);
  g_signal_connect (link_button, "query-tooltip",
                    G_CALLBACK (gtk_link_button_query_tooltip_cb), NULL);
  
  /* enable drag source */
  gtk_drag_source_set (GTK_WIDGET (link_button),
  		       GDK_BUTTON1_MASK,
  		       link_drop_types, G_N_ELEMENTS (link_drop_types),
  		       GDK_ACTION_COPY);
}

static void
gtk_link_button_finalize (GObject *object)
{
  GtkLinkButton *link_button = GTK_LINK_BUTTON (object);
  
  g_free (link_button->priv->uri);
  
  G_OBJECT_CLASS (gtk_link_button_parent_class)->finalize (object);
}

static void
gtk_link_button_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
  GtkLinkButton *link_button = GTK_LINK_BUTTON (object);
  
  switch (prop_id)
    {
    case PROP_URI:
      g_value_set_string (value, link_button->priv->uri);
      break;
    case PROP_VISITED:
      g_value_set_boolean (value, link_button->priv->visited);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_link_button_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  GtkLinkButton *link_button = GTK_LINK_BUTTON (object);
  
  switch (prop_id)
    {
    case PROP_URI:
      gtk_link_button_set_uri (link_button, g_value_get_string (value));
      break;
    case PROP_VISITED:
      gtk_link_button_set_visited (link_button, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
set_link_color (GtkLinkButton *link_button)
{
  GdkColor *link_color = NULL;
  GtkWidget *label;

  label = gtk_bin_get_child (GTK_BIN (link_button));
  if (!GTK_IS_LABEL (label))
    return;

  if (link_button->priv->visited)
    {
      gtk_widget_style_get (GTK_WIDGET (link_button),
			    "visited-link-color", &link_color, NULL);
      if (!link_color)
	link_color = (GdkColor *) &default_visited_link_color;
    }
  else
    {
      gtk_widget_style_get (GTK_WIDGET (link_button),
			    "link-color", &link_color, NULL);
      if (!link_color)
	link_color = (GdkColor *) &default_link_color;
    }

  gtk_widget_modify_fg (label, GTK_STATE_NORMAL, link_color);
  gtk_widget_modify_fg (label, GTK_STATE_ACTIVE, link_color);
  gtk_widget_modify_fg (label, GTK_STATE_PRELIGHT, link_color);
  gtk_widget_modify_fg (label, GTK_STATE_SELECTED, link_color);

  if (link_color != &default_link_color &&
      link_color != &default_visited_link_color)
    gdk_color_free (link_color);
}

static void
set_link_underline (GtkLinkButton *link_button)
{
  GtkWidget *label;
  
  label = gtk_bin_get_child (GTK_BIN (link_button));
  if (GTK_IS_LABEL (label))
    {
      PangoAttrList *attributes;
      PangoAttribute *uline;

      uline = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
      uline->start_index = 0;
      uline->end_index = G_MAXUINT;
      attributes = pango_attr_list_new ();
      pango_attr_list_insert (attributes, uline); 
      gtk_label_set_attributes (GTK_LABEL (label), attributes);
      pango_attr_list_unref (attributes);
    }
}

static void
gtk_link_button_add (GtkContainer *container,
		     GtkWidget    *widget)
{
  GTK_CONTAINER_CLASS (gtk_link_button_parent_class)->add (container, widget);

  set_link_color (GTK_LINK_BUTTON (container));
  set_link_underline (GTK_LINK_BUTTON (container));
}

static void
gtk_link_button_style_set (GtkWidget *widget,
			   GtkStyle  *old_style)
{
  GtkLinkButton *link_button = GTK_LINK_BUTTON (widget);

  set_link_color (link_button);
}

static void
set_hand_cursor (GtkWidget *widget,
		 gboolean   show_hand)
{
  GdkDisplay *display;
  GdkCursor *cursor;

  display = gtk_widget_get_display (widget);

  cursor = NULL;
  if (show_hand)
    cursor = gdk_cursor_new_for_display (display, GDK_HAND2);

  gdk_window_set_cursor (widget->window, cursor);
  gdk_display_flush (display);

  if (cursor)
    gdk_cursor_unref (cursor);
}

static void
popup_menu_detach (GtkWidget *attach_widget,
		   GtkMenu   *menu)
{
  GtkLinkButton *link_button = GTK_LINK_BUTTON (attach_widget);

  link_button->priv->popup_menu = NULL;
}

static void
popup_position_func (GtkMenu  *menu,
		     gint     *x,
		     gint     *y,
		     gboolean *push_in,
		     gpointer  user_data)
{
  GtkLinkButton *link_button = GTK_LINK_BUTTON (user_data);
  GtkLinkButtonPrivate *priv = link_button->priv;
  GtkWidget *widget = GTK_WIDGET (link_button);
  GdkScreen *screen = gtk_widget_get_screen (widget);
  GtkRequisition req;
  gint monitor_num;
  GdkRectangle monitor;
  
  g_return_if_fail (gtk_widget_get_realized (widget));

  gdk_window_get_origin (widget->window, x, y);

  gtk_widget_size_request (priv->popup_menu, &req);

  *x += widget->allocation.width / 2;
  *y += widget->allocation.height;

  monitor_num = gdk_screen_get_monitor_at_point (screen, *x, *y);
  gtk_menu_set_monitor (menu, monitor_num);
  gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

  *x = CLAMP (*x, monitor.x, monitor.x + MAX (0, monitor.width - req.width));
  *y = CLAMP (*y, monitor.y, monitor.y + MAX (0, monitor.height - req.height));

  *push_in = FALSE;
}

static void
copy_activate_cb (GtkWidget     *widget,
		  GtkLinkButton *link_button)
{
  GtkLinkButtonPrivate *priv = link_button->priv;
  
  gtk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (link_button),
			  			    GDK_SELECTION_CLIPBOARD),
		  	  priv->uri, -1);
}

static void
gtk_link_button_do_popup (GtkLinkButton  *link_button,
			  GdkEventButton *event)
{
  GtkLinkButtonPrivate *priv = link_button->priv;
  gint button;
  guint time;
  
  if (event)
    {
      button = event->button;
      time = event->time;
    }
  else
    {
      button = 0;
      time = gtk_get_current_event_time ();
    }

  if (gtk_widget_get_realized (GTK_WIDGET (link_button)))
    {
      GtkWidget *menu_item;
      
      if (priv->popup_menu)
	gtk_widget_destroy (priv->popup_menu);

      priv->popup_menu = gtk_menu_new ();
      
      gtk_menu_attach_to_widget (GTK_MENU (priv->popup_menu),
		      		 GTK_WIDGET (link_button),
				 popup_menu_detach);

      menu_item = gtk_image_menu_item_new_with_mnemonic (_("Copy URL"));
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item),
		      		     gtk_image_new_from_stock (GTK_STOCK_COPY,
							       GTK_ICON_SIZE_MENU));
      g_signal_connect (menu_item, "activate",
		        G_CALLBACK (copy_activate_cb), link_button);
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), menu_item);
      
      if (button)
        gtk_menu_popup (GTK_MENU (priv->popup_menu), NULL, NULL,
		        NULL, NULL,
			button, time);
      else
        {
          gtk_menu_popup (GTK_MENU (priv->popup_menu), NULL, NULL,
			  popup_position_func, link_button,
			  button, time);
	  gtk_menu_shell_select_first (GTK_MENU_SHELL (priv->popup_menu), FALSE);
	}
    }
}

static gboolean
gtk_link_button_button_press (GtkWidget      *widget,
			      GdkEventButton *event)
{
  if (!gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  if (_gtk_button_event_triggers_context_menu (event))
    {
      gtk_link_button_do_popup (GTK_LINK_BUTTON (widget), event);
      
      return TRUE;
    }

  if (GTK_WIDGET_CLASS (gtk_link_button_parent_class)->button_press_event)
    return GTK_WIDGET_CLASS (gtk_link_button_parent_class)->button_press_event (widget, event);
  
  return FALSE;
}

static void
gtk_link_button_clicked (GtkButton *button)
{
  GtkLinkButton *link_button = GTK_LINK_BUTTON (button);

  if (uri_func)
    (* uri_func) (link_button, link_button->priv->uri, uri_func_data);
  else
    {
      GdkScreen *screen;
      GError *error;

      if (gtk_widget_has_screen (GTK_WIDGET (button)))
        screen = gtk_widget_get_screen (GTK_WIDGET (button));
      else
        screen = NULL;

      error = NULL;
      gtk_show_uri (screen, link_button->priv->uri, GDK_CURRENT_TIME, &error);
      if (error)
        {
          g_warning ("Unable to show '%s': %s",
                     link_button->priv->uri,
                     error->message);
          g_error_free (error);
        }
    }

  gtk_link_button_set_visited (link_button, TRUE);
}

static gboolean
gtk_link_button_popup_menu (GtkWidget *widget)
{
  gtk_link_button_do_popup (GTK_LINK_BUTTON (widget), NULL);

  return TRUE; 
}

static gboolean
gtk_link_button_enter_cb (GtkWidget        *widget,
			  GdkEventCrossing *crossing,
			  gpointer          user_data)
{
  set_hand_cursor (widget, TRUE);
  
  return FALSE;
}

static gboolean
gtk_link_button_leave_cb (GtkWidget        *widget,
			  GdkEventCrossing *crossing,
			  gpointer          user_data)
{
  set_hand_cursor (widget, FALSE);
  
  return FALSE;
}

static void
gtk_link_button_drag_data_get_cb (GtkWidget        *widget,
				  GdkDragContext   *context,
				  GtkSelectionData *selection,
				  guint             _info,
				  guint             _time,
				  gpointer          user_data)
{
  GtkLinkButton *link_button = GTK_LINK_BUTTON (widget);
  gchar *uri;
  
  uri = g_strdup_printf ("%s\r\n", link_button->priv->uri);
  gtk_selection_data_set (selection,
  			  selection->target,
  			  8,
  			  (guchar *) uri,
			  strlen (uri));
  
  g_free (uri);
}

/**
 * gtk_link_button_new:
 * @uri: a valid URI
 *
 * Creates a new #GtkLinkButton with the URI as its text.
 *
 * Return value: a new link button widget.
 *
 * Since: 2.10
 */
GtkWidget *
gtk_link_button_new (const gchar *uri)
{
  gchar *utf8_uri = NULL;
  GtkWidget *retval;
  
  g_return_val_if_fail (uri != NULL, NULL);
  
  if (g_utf8_validate (uri, -1, NULL))
    {
      utf8_uri = g_strdup (uri);
    }
  else
    {
      GError *conv_err = NULL;
    
      utf8_uri = g_locale_to_utf8 (uri, -1, NULL, NULL, &conv_err);
      if (conv_err)
        {
          g_warning ("Attempting to convert URI `%s' to UTF-8, but failed "
                     "with error: %s\n",
                     uri,
                     conv_err->message);
          g_error_free (conv_err);
        
          utf8_uri = g_strdup (_("Invalid URI"));
        }
    }
  
  retval = g_object_new (GTK_TYPE_LINK_BUTTON,
  			 "label", utf8_uri,
  			 "uri", uri,
  			 NULL);
  
  g_free (utf8_uri);
  
  return retval;
}

/**
 * gtk_link_button_new_with_label:
 * @uri: a valid URI
 * @label: (allow-none): the text of the button
 *
 * Creates a new #GtkLinkButton containing a label.
 *
 * Return value: (transfer none): a new link button widget.
 *
 * Since: 2.10
 */
GtkWidget *
gtk_link_button_new_with_label (const gchar *uri,
				const gchar *label)
{
  GtkWidget *retval;
  
  g_return_val_if_fail (uri != NULL, NULL);
  
  if (!label)
    return gtk_link_button_new (uri);

  retval = g_object_new (GTK_TYPE_LINK_BUTTON,
		         "label", label,
			 "uri", uri,
			 NULL);

  return retval;
}

static gboolean 
gtk_link_button_query_tooltip_cb (GtkWidget    *widget,
                                  gint          x,
                                  gint          y,
                                  gboolean      keyboard_tip,
                                  GtkTooltip   *tooltip,
                                  gpointer      data)
{
  GtkLinkButton *link_button = GTK_LINK_BUTTON (widget);
  const gchar *label, *uri;

  label = gtk_button_get_label (GTK_BUTTON (link_button));
  uri = link_button->priv->uri;

  if (!gtk_widget_get_tooltip_text (widget)
    && !gtk_widget_get_tooltip_markup (widget)
    && label && *label != '\0' && uri && strcmp (label, uri) != 0)
    {
      gtk_tooltip_set_text (tooltip, uri);
      return TRUE;
    }

  return FALSE;
}


/**
 * gtk_link_button_set_uri:
 * @link_button: a #GtkLinkButton
 * @uri: a valid URI
 *
 * Sets @uri as the URI where the #GtkLinkButton points. As a side-effect
 * this unsets the 'visited' state of the button.
 *
 * Since: 2.10
 */
void
gtk_link_button_set_uri (GtkLinkButton *link_button,
			 const gchar   *uri)
{
  GtkLinkButtonPrivate *priv;

  g_return_if_fail (GTK_IS_LINK_BUTTON (link_button));
  g_return_if_fail (uri != NULL);

  priv = link_button->priv;

  g_free (priv->uri);
  priv->uri = g_strdup (uri);

  g_object_notify (G_OBJECT (link_button), "uri");

  gtk_link_button_set_visited (link_button, FALSE);
}

/**
 * gtk_link_button_get_uri:
 * @link_button: a #GtkLinkButton
 *
 * Retrieves the URI set using gtk_link_button_set_uri().
 *
 * Return value: a valid URI.  The returned string is owned by the link button
 *   and should not be modified or freed.
 *
 * Since: 2.10
 */
const gchar *
gtk_link_button_get_uri (GtkLinkButton *link_button)
{
  g_return_val_if_fail (GTK_IS_LINK_BUTTON (link_button), NULL);
  
  return link_button->priv->uri;
}

/**
 * gtk_link_button_set_uri_hook:
 * @func: (allow-none): a function called each time a #GtkLinkButton is clicked, or %NULL
 * @data: (allow-none): user data to be passed to @func, or %NULL
 * @destroy: (allow-none): a #GDestroyNotify that gets called when @data is no longer needed, or %NULL
 *
 * Sets @func as the function that should be invoked every time a user clicks
 * a #GtkLinkButton. This function is called before every callback registered
 * for the "clicked" signal.
 *
 * If no uri hook has been set, GTK+ defaults to calling gtk_show_uri().
 *
 * Return value: the previously set hook function.
 *
 * Since: 2.10
 *
 * Deprecated: 2.24: Use the #GtkButton::clicked signal instead
 */
GtkLinkButtonUriFunc
gtk_link_button_set_uri_hook (GtkLinkButtonUriFunc func,
			      gpointer             data,
			      GDestroyNotify       destroy)
{
  GtkLinkButtonUriFunc old_uri_func;

  if (uri_func_destroy)
    (* uri_func_destroy) (uri_func_data);

  old_uri_func = uri_func;

  uri_func = func;
  uri_func_data = data;
  uri_func_destroy = destroy;

  return old_uri_func;
}

/**
 * gtk_link_button_set_visited:
 * @link_button: a #GtkLinkButton
 * @visited: the new 'visited' state
 *
 * Sets the 'visited' state of the URI where the #GtkLinkButton
 * points.  See gtk_link_button_get_visited() for more details.
 *
 * Since: 2.14
 */
void
gtk_link_button_set_visited (GtkLinkButton *link_button,
                             gboolean       visited)
{
  g_return_if_fail (GTK_IS_LINK_BUTTON (link_button));

  visited = visited != FALSE;

  if (link_button->priv->visited != visited)
    {
      link_button->priv->visited = visited;

      set_link_color (link_button);

      g_object_notify (G_OBJECT (link_button), "visited");
    }
}

/**
 * gtk_link_button_get_visited:
 * @link_button: a #GtkLinkButton
 *
 * Retrieves the 'visited' state of the URI where the #GtkLinkButton
 * points. The button becomes visited when it is clicked. If the URI
 * is changed on the button, the 'visited' state is unset again.
 *
 * The state may also be changed using gtk_link_button_set_visited().
 *
 * Return value: %TRUE if the link has been visited, %FALSE otherwise
 *
 * Since: 2.14
 */
gboolean
gtk_link_button_get_visited (GtkLinkButton *link_button)
{
  g_return_val_if_fail (GTK_IS_LINK_BUTTON (link_button), FALSE);
  
  return link_button->priv->visited;
}


#define __GTK_LINK_BUTTON_C__
#include "gtkaliasdef.c"
