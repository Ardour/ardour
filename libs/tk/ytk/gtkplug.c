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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* By Owen Taylor <otaylor@gtk.org>              98/4/4 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkplug.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkplugprivate.h"

#include "gtkalias.h"

/**
 * SECTION:gtkplug
 * @Short_description: Toplevel for embedding into other processes
 * @Title: GtkPlug
 * @See_also: #GtkSocket
 *
 * Together with #GtkSocket, #GtkPlug provides the ability
 * to embed widgets from one process into another process
 * in a fashion that is transparent to the user. One
 * process creates a #GtkSocket widget and passes the
 * ID of that widget's window to the other process,
 * which then creates a #GtkPlug with that window ID.
 * Any widgets contained in the #GtkPlug then will appear
 * inside the first application's window.
 *
 * <note>
 * The #GtkPlug and #GtkSocket widgets are currently not available
 * on all platforms supported by GTK+.
 * </note>
 */

static void            gtk_plug_get_property          (GObject     *object,
						       guint        prop_id,
						       GValue      *value,
						       GParamSpec  *pspec);
static void            gtk_plug_finalize              (GObject          *object);
static void            gtk_plug_realize               (GtkWidget        *widget);
static void            gtk_plug_unrealize             (GtkWidget        *widget);
static void            gtk_plug_show                  (GtkWidget        *widget);
static void            gtk_plug_hide                  (GtkWidget        *widget);
static void            gtk_plug_map                   (GtkWidget        *widget);
static void            gtk_plug_unmap                 (GtkWidget        *widget);
static void            gtk_plug_size_allocate         (GtkWidget        *widget,
						       GtkAllocation    *allocation);
static gboolean        gtk_plug_key_press_event       (GtkWidget        *widget,
						       GdkEventKey      *event);
static gboolean        gtk_plug_focus_event           (GtkWidget        *widget,
						       GdkEventFocus    *event);
static void            gtk_plug_set_focus             (GtkWindow        *window,
						       GtkWidget        *focus);
static gboolean        gtk_plug_focus                 (GtkWidget        *widget,
						       GtkDirectionType  direction);
static void            gtk_plug_check_resize          (GtkContainer     *container);
static void            gtk_plug_keys_changed          (GtkWindow        *window);

static GtkBinClass *bin_class = NULL;

typedef struct
{
  guint			 accelerator_key;
  GdkModifierType	 accelerator_mods;
} GrabbedKey;

enum {
  PROP_0,
  PROP_EMBEDDED,
  PROP_SOCKET_WINDOW
};

enum {
  EMBEDDED,
  LAST_SIGNAL
}; 

static guint plug_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GtkPlug, gtk_plug, GTK_TYPE_WINDOW)

static void
gtk_plug_get_property (GObject    *object,
		       guint       prop_id,
		       GValue     *value,
		       GParamSpec *pspec)
{
  GtkPlug *plug = GTK_PLUG (object);

  switch (prop_id)
    {
    case PROP_EMBEDDED:
      g_value_set_boolean (value, plug->socket_window != NULL);
      break;
    case PROP_SOCKET_WINDOW:
      g_value_set_object (value, plug->socket_window);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_plug_class_init (GtkPlugClass *class)
{
  GObjectClass *gobject_class = (GObjectClass *)class;
  GtkWidgetClass *widget_class = (GtkWidgetClass *)class;
  GtkWindowClass *window_class = (GtkWindowClass *)class;
  GtkContainerClass *container_class = (GtkContainerClass *)class;

  bin_class = g_type_class_peek (GTK_TYPE_BIN);

  gobject_class->get_property = gtk_plug_get_property;
  gobject_class->finalize = gtk_plug_finalize;
  
  widget_class->realize = gtk_plug_realize;
  widget_class->unrealize = gtk_plug_unrealize;
  widget_class->key_press_event = gtk_plug_key_press_event;
  widget_class->focus_in_event = gtk_plug_focus_event;
  widget_class->focus_out_event = gtk_plug_focus_event;

  widget_class->show = gtk_plug_show;
  widget_class->hide = gtk_plug_hide;
  widget_class->map = gtk_plug_map;
  widget_class->unmap = gtk_plug_unmap;
  widget_class->size_allocate = gtk_plug_size_allocate;

  widget_class->focus = gtk_plug_focus;

  container_class->check_resize = gtk_plug_check_resize;

  window_class->set_focus = gtk_plug_set_focus;
  window_class->keys_changed = gtk_plug_keys_changed;

  /**
   * GtkPlug:embedded:
   *
   * %TRUE if the plug is embedded in a socket.
   *
   * Since: 2.12
   */
  g_object_class_install_property (gobject_class,
				   PROP_EMBEDDED,
				   g_param_spec_boolean ("embedded",
							 P_("Embedded"),
							 P_("Whether or not the plug is embedded"),
							 FALSE,
							 GTK_PARAM_READABLE));

  /**
   * GtkPlug:socket-window:
   *
   * The window of the socket the plug is embedded in.
   *
   * Since: 2.14
   */
  g_object_class_install_property (gobject_class,
				   PROP_SOCKET_WINDOW,
				   g_param_spec_object ("socket-window",
							P_("Socket Window"),
							P_("The window of the socket the plug is embedded in"),
							GDK_TYPE_WINDOW,
							GTK_PARAM_READABLE));

  /**
   * GtkPlug::embedded:
   * @plug: the object on which the signal was emitted
   *
   * Gets emitted when the plug becomes embedded in a socket.
   */ 
  plug_signals[EMBEDDED] =
    g_signal_new (I_("embedded"),
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPlugClass, embedded),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
gtk_plug_init (GtkPlug *plug)
{
  GtkWindow *window;

  window = GTK_WINDOW (plug);

  window->type = GTK_WINDOW_TOPLEVEL;
}

static void
gtk_plug_set_is_child (GtkPlug  *plug,
		       gboolean  is_child)
{
  g_assert (!GTK_WIDGET (plug)->parent);
      
  if (is_child)
    {
      if (plug->modality_window)
	_gtk_plug_handle_modality_off (plug);

      if (plug->modality_group)
	{
	  gtk_window_group_remove_window (plug->modality_group, GTK_WINDOW (plug));
	  g_object_unref (plug->modality_group);
	  plug->modality_group = NULL;
	}
      
      /* As a toplevel, the MAPPED flag doesn't correspond
       * to whether the widget->window is mapped; we unmap
       * here, but don't bother remapping -- we will get mapped
       * by gtk_widget_set_parent ().
       */
      if (gtk_widget_get_mapped (GTK_WIDGET (plug)))
	gtk_widget_unmap (GTK_WIDGET (plug));
      
      _gtk_window_set_is_toplevel (GTK_WINDOW (plug), FALSE);
      gtk_container_set_resize_mode (GTK_CONTAINER (plug), GTK_RESIZE_PARENT);

      _gtk_widget_propagate_hierarchy_changed (GTK_WIDGET (plug), GTK_WIDGET (plug));
    }
  else
    {
      if (GTK_WINDOW (plug)->focus_widget)
	gtk_window_set_focus (GTK_WINDOW (plug), NULL);
      if (GTK_WINDOW (plug)->default_widget)
	gtk_window_set_default (GTK_WINDOW (plug), NULL);
	  
      plug->modality_group = gtk_window_group_new ();
      gtk_window_group_add_window (plug->modality_group, GTK_WINDOW (plug));
      
      _gtk_window_set_is_toplevel (GTK_WINDOW (plug), TRUE);
      gtk_container_set_resize_mode (GTK_CONTAINER (plug), GTK_RESIZE_QUEUE);

      _gtk_widget_propagate_hierarchy_changed (GTK_WIDGET (plug), NULL);
    }
}

/**
 * gtk_plug_get_id:
 * @plug: a #GtkPlug.
 * 
 * Gets the window ID of a #GtkPlug widget, which can then
 * be used to embed this window inside another window, for
 * instance with gtk_socket_add_id().
 * 
 * Return value: the window ID for the plug
 **/
GdkNativeWindow
gtk_plug_get_id (GtkPlug *plug)
{
  g_return_val_if_fail (GTK_IS_PLUG (plug), 0);

  if (!gtk_widget_get_realized (GTK_WIDGET (plug)))
    gtk_widget_realize (GTK_WIDGET (plug));

  return _gtk_plug_windowing_get_id (plug);
}

/**
 * gtk_plug_get_embedded:
 * @plug: a #GtkPlug
 *
 * Determines whether the plug is embedded in a socket.
 *
 * Return value: %TRUE if the plug is embedded in a socket
 *
 * Since: 2.14
 **/
gboolean
gtk_plug_get_embedded (GtkPlug *plug)
{
  g_return_val_if_fail (GTK_IS_PLUG (plug), FALSE);

  return plug->socket_window != NULL;
}

/**
 * gtk_plug_get_socket_window:
 * @plug: a #GtkPlug
 *
 * Retrieves the socket the plug is embedded in.
 *
 * Return value: (transfer none): the window of the socket, or %NULL
 *
 * Since: 2.14
 **/
GdkWindow *
gtk_plug_get_socket_window (GtkPlug *plug)
{
  g_return_val_if_fail (GTK_IS_PLUG (plug), NULL);

  return plug->socket_window;
}

/**
 * _gtk_plug_add_to_socket:
 * @plug: a #GtkPlug
 * @socket_: a #GtkSocket
 * 
 * Adds a plug to a socket within the same application.
 **/
void
_gtk_plug_add_to_socket (GtkPlug   *plug,
			 GtkSocket *socket_)
{
  GtkWidget *widget;
  gint w, h;
  
  g_return_if_fail (GTK_IS_PLUG (plug));
  g_return_if_fail (GTK_IS_SOCKET (socket_));
  g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (socket_)));

  widget = GTK_WIDGET (plug);

  gtk_plug_set_is_child (plug, TRUE);
  plug->same_app = TRUE;
  socket_->same_app = TRUE;
  socket_->plug_widget = widget;

  plug->socket_window = GTK_WIDGET (socket_)->window;
  g_object_ref (plug->socket_window);
  g_signal_emit (plug, plug_signals[EMBEDDED], 0);
  g_object_notify (G_OBJECT (plug), "embedded");

  if (gtk_widget_get_realized (widget))
    {
      w = gdk_window_get_width (widget->window);
      h = gdk_window_get_height (widget->window);
      gdk_window_reparent (widget->window, plug->socket_window, -w, -h);
    }

  gtk_widget_set_parent (widget, GTK_WIDGET (socket_));

  g_signal_emit_by_name (socket_, "plug-added");
}

/**
 * _gtk_plug_send_delete_event:
 * @widget: a #GtkWidget
 *
 * Send a GDK_DELETE event to the @widget and destroy it if
 * necessary. Internal GTK function, called from this file or the
 * backend-specific GtkPlug implementation.
 */
void
_gtk_plug_send_delete_event (GtkWidget *widget)
{
  GdkEvent *event = gdk_event_new (GDK_DELETE);

  event->any.window = g_object_ref (widget->window);
  event->any.send_event = FALSE;

  g_object_ref (widget);

  if (!gtk_widget_event (widget, event))
    gtk_widget_destroy (widget);

  g_object_unref (widget);

  gdk_event_free (event);
}

/**
 * _gtk_plug_remove_from_socket:
 * @plug: a #GtkPlug
 * @socket_: a #GtkSocket
 * 
 * Removes a plug from a socket within the same application.
 **/
void
_gtk_plug_remove_from_socket (GtkPlug   *plug,
			      GtkSocket *socket_)
{
  GtkWidget *widget;
  gboolean result;
  gboolean widget_was_visible;

  g_return_if_fail (GTK_IS_PLUG (plug));
  g_return_if_fail (GTK_IS_SOCKET (socket_));
  g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (plug)));

  widget = GTK_WIDGET (plug);

  if (GTK_WIDGET_IN_REPARENT (widget))
    return;

  g_object_ref (plug);
  g_object_ref (socket_);

  widget_was_visible = gtk_widget_get_visible (widget);
  
  gdk_window_hide (widget->window);
  GTK_PRIVATE_SET_FLAG (plug, GTK_IN_REPARENT);
  gdk_window_reparent (widget->window,
		       gtk_widget_get_root_window (widget),
		       0, 0);
  gtk_widget_unparent (GTK_WIDGET (plug));
  GTK_PRIVATE_UNSET_FLAG (plug, GTK_IN_REPARENT);
  
  socket_->plug_widget = NULL;
  if (socket_->plug_window != NULL)
    {
      g_object_unref (socket_->plug_window);
      socket_->plug_window = NULL;
    }
  
  socket_->same_app = FALSE;

  plug->same_app = FALSE;
  if (plug->socket_window != NULL)
    {
      g_object_unref (plug->socket_window);
      plug->socket_window = NULL;
    }
  gtk_plug_set_is_child (plug, FALSE);

  g_signal_emit_by_name (socket_, "plug-removed", &result);
  if (!result)
    gtk_widget_destroy (GTK_WIDGET (socket_));

  if (widget->window)
    _gtk_plug_send_delete_event (widget);

  g_object_unref (plug);

  if (widget_was_visible && gtk_widget_get_visible (GTK_WIDGET (socket_)))
    gtk_widget_queue_resize (GTK_WIDGET (socket_));

  g_object_unref (socket_);
}

/**
 * gtk_plug_construct:
 * @plug: a #GtkPlug.
 * @socket_id: the XID of the socket's window.
 *
 * Finish the initialization of @plug for a given #GtkSocket identified by
 * @socket_id. This function will generally only be used by classes deriving from #GtkPlug.
 **/
void
gtk_plug_construct (GtkPlug         *plug,
		    GdkNativeWindow  socket_id)
{
  gtk_plug_construct_for_display (plug, gdk_display_get_default (), socket_id);
}

/**
 * gtk_plug_construct_for_display:
 * @plug: a #GtkPlug.
 * @display: the #GdkDisplay associated with @socket_id's 
 *	     #GtkSocket.
 * @socket_id: the XID of the socket's window.
 *
 * Finish the initialization of @plug for a given #GtkSocket identified by
 * @socket_id which is currently displayed on @display.
 * This function will generally only be used by classes deriving from #GtkPlug.
 *
 * Since: 2.2
 **/
void
gtk_plug_construct_for_display (GtkPlug         *plug,
				GdkDisplay	*display,
				GdkNativeWindow  socket_id)
{
  if (socket_id)
    {
      gpointer user_data = NULL;

      plug->socket_window = gdk_window_lookup_for_display (display, socket_id);
      if (plug->socket_window)
	{
	  gdk_window_get_user_data (plug->socket_window, &user_data);

	  if (user_data)
	    {
	      if (GTK_IS_SOCKET (user_data))
		_gtk_plug_add_to_socket (plug, user_data);
	      else
		{
		  g_warning (G_STRLOC "Can't create GtkPlug as child of non-GtkSocket");
		  plug->socket_window = NULL;
		}
	    }
	  else
	    g_object_ref (plug->socket_window);
	}
      else
	plug->socket_window = gdk_window_foreign_new_for_display (display, socket_id);

      if (plug->socket_window) {
	g_signal_emit (plug, plug_signals[EMBEDDED], 0);

        g_object_notify (G_OBJECT (plug), "embedded");
      }
    }
}

/**
 * gtk_plug_new:
 * @socket_id:  the window ID of the socket, or 0.
 * 
 * Creates a new plug widget inside the #GtkSocket identified
 * by @socket_id. If @socket_id is 0, the plug is left "unplugged" and
 * can later be plugged into a #GtkSocket by  gtk_socket_add_id().
 * 
 * Return value: the new #GtkPlug widget.
 **/
GtkWidget*
gtk_plug_new (GdkNativeWindow socket_id)
{
  return gtk_plug_new_for_display (gdk_display_get_default (), socket_id);
}

/**
 * gtk_plug_new_for_display:
 * @display: the #GdkDisplay on which @socket_id is displayed
 * @socket_id: the XID of the socket's window.
 * 
 * Create a new plug widget inside the #GtkSocket identified by socket_id.
 *
 * Return value: the new #GtkPlug widget.
 *
 * Since: 2.2
 */
GtkWidget*
gtk_plug_new_for_display (GdkDisplay	  *display,
			  GdkNativeWindow  socket_id)
{
  GtkPlug *plug;

  plug = g_object_new (GTK_TYPE_PLUG, NULL);
  gtk_plug_construct_for_display (plug, display, socket_id);
  return GTK_WIDGET (plug);
}

static void
gtk_plug_finalize (GObject *object)
{
  GtkPlug *plug = GTK_PLUG (object);

  if (plug->grabbed_keys)
    {
      g_hash_table_destroy (plug->grabbed_keys);
      plug->grabbed_keys = NULL;
    }
  
  G_OBJECT_CLASS (gtk_plug_parent_class)->finalize (object);
}

static void
gtk_plug_unrealize (GtkWidget *widget)
{
  GtkPlug *plug = GTK_PLUG (widget);

  if (plug->socket_window != NULL)
    {
      gdk_window_set_user_data (plug->socket_window, NULL);
      g_object_unref (plug->socket_window);
      plug->socket_window = NULL;

      g_object_notify (G_OBJECT (widget), "embedded");
    }

  if (!plug->same_app)
    {
      if (plug->modality_window)
	_gtk_plug_handle_modality_off (plug);

      gtk_window_group_remove_window (plug->modality_group, GTK_WINDOW (plug));
      g_object_unref (plug->modality_group);
    }

  GTK_WIDGET_CLASS (gtk_plug_parent_class)->unrealize (widget);
}

static void
gtk_plug_realize (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkPlug *plug = GTK_PLUG (widget);
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

  attributes.window_type = GDK_WINDOW_CHILD;	/* XXX GDK_WINDOW_PLUG ? */
  attributes.title = window->title;
  attributes.wmclass_name = window->wmclass_name;
  attributes.wmclass_class = window->wmclass_class;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;

  /* this isn't right - we should match our parent's visual/colormap.
   * though that will require handling "foreign" colormaps */
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_KEY_PRESS_MASK |
			    GDK_KEY_RELEASE_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK |
			    GDK_STRUCTURE_MASK);

  attributes_mask = GDK_WA_VISUAL | GDK_WA_COLORMAP;
  attributes_mask |= (window->title ? GDK_WA_TITLE : 0);
  attributes_mask |= (window->wmclass_name ? GDK_WA_WMCLASS : 0);

  if (gtk_widget_is_toplevel (widget))
    {
      attributes.window_type = GDK_WINDOW_TOPLEVEL;

      gdk_error_trap_push ();
      if (plug->socket_window)
	widget->window = gdk_window_new (plug->socket_window, 
					 &attributes, attributes_mask);
      else /* If it's a passive plug, we use the root window */
	widget->window = gdk_window_new (gtk_widget_get_root_window (widget),
					 &attributes, attributes_mask);

      gdk_display_sync (gtk_widget_get_display (widget));
      if (gdk_error_trap_pop ()) /* Uh-oh */
	{
	  gdk_error_trap_push ();
	  gdk_window_destroy (widget->window);
	  gdk_flush ();
	  gdk_error_trap_pop ();
	  widget->window = gdk_window_new (gtk_widget_get_root_window (widget),
					   &attributes, attributes_mask);
	}
      
      gdk_window_add_filter (widget->window,
			     _gtk_plug_windowing_filter_func,
			     widget);

      plug->modality_group = gtk_window_group_new ();
      gtk_window_group_add_window (plug->modality_group, window);
      
      _gtk_plug_windowing_realize_toplevel (plug);
    }
  else
    widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), 
				     &attributes, attributes_mask);      
  
  gdk_window_set_user_data (widget->window, window);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  gdk_window_enable_synchronized_configure (widget->window);
}

static void
gtk_plug_show (GtkWidget *widget)
{
  if (gtk_widget_is_toplevel (widget))
    GTK_WIDGET_CLASS (gtk_plug_parent_class)->show (widget);
  else
    GTK_WIDGET_CLASS (bin_class)->show (widget);
}

static void
gtk_plug_hide (GtkWidget *widget)
{
  if (gtk_widget_is_toplevel (widget))
    GTK_WIDGET_CLASS (gtk_plug_parent_class)->hide (widget);
  else
    GTK_WIDGET_CLASS (bin_class)->hide (widget);
}

/* From gdkinternals.h */
void gdk_synthesize_window_state (GdkWindow     *window,
                                  GdkWindowState unset_flags,
                                  GdkWindowState set_flags);

static void
gtk_plug_map (GtkWidget *widget)
{
  if (gtk_widget_is_toplevel (widget))
    {
      GtkBin *bin = GTK_BIN (widget);
      GtkPlug *plug = GTK_PLUG (widget);
      
      gtk_widget_set_mapped (widget, TRUE);

      if (bin->child &&
	  gtk_widget_get_visible (bin->child) &&
	  !gtk_widget_get_mapped (bin->child))
	gtk_widget_map (bin->child);

      _gtk_plug_windowing_map_toplevel (plug);
      
      gdk_synthesize_window_state (widget->window,
				   GDK_WINDOW_STATE_WITHDRAWN,
				   0);
    }
  else
    GTK_WIDGET_CLASS (bin_class)->map (widget);
}

static void
gtk_plug_unmap (GtkWidget *widget)
{
  if (gtk_widget_is_toplevel (widget))
    {
      GtkPlug *plug = GTK_PLUG (widget);

      gtk_widget_set_mapped (widget, FALSE);

      gdk_window_hide (widget->window);

      _gtk_plug_windowing_unmap_toplevel (plug);
      
      gdk_synthesize_window_state (widget->window,
				   0,
				   GDK_WINDOW_STATE_WITHDRAWN);
    }
  else
    GTK_WIDGET_CLASS (bin_class)->unmap (widget);
}

static void
gtk_plug_size_allocate (GtkWidget     *widget,
			GtkAllocation *allocation)
{
  if (gtk_widget_is_toplevel (widget))
    GTK_WIDGET_CLASS (gtk_plug_parent_class)->size_allocate (widget, allocation);
  else
    {
      GtkBin *bin = GTK_BIN (widget);

      widget->allocation = *allocation;

      if (gtk_widget_get_realized (widget))
	gdk_window_move_resize (widget->window,
				allocation->x, allocation->y,
				allocation->width, allocation->height);

      if (bin->child && gtk_widget_get_visible (bin->child))
	{
	  GtkAllocation child_allocation;
	  
	  child_allocation.x = child_allocation.y = GTK_CONTAINER (widget)->border_width;
	  child_allocation.width =
	    MAX (1, (gint)allocation->width - child_allocation.x * 2);
	  child_allocation.height =
	    MAX (1, (gint)allocation->height - child_allocation.y * 2);
	  
	  gtk_widget_size_allocate (bin->child, &child_allocation);
	}
      
    }
}

static gboolean
gtk_plug_key_press_event (GtkWidget   *widget,
			  GdkEventKey *event)
{
  if (gtk_widget_is_toplevel (widget))
    return GTK_WIDGET_CLASS (gtk_plug_parent_class)->key_press_event (widget, event);
  else
    return FALSE;
}

static gboolean
gtk_plug_focus_event (GtkWidget      *widget,
		      GdkEventFocus  *event)
{
  /* We eat focus-in events and focus-out events, since they
   * can be generated by something like a keyboard grab on
   * a child of the plug.
   */
  return FALSE;
}

static void
gtk_plug_set_focus (GtkWindow *window,
		    GtkWidget *focus)
{
  GtkPlug *plug = GTK_PLUG (window);

  GTK_WINDOW_CLASS (gtk_plug_parent_class)->set_focus (window, focus);
  
  /* Ask for focus from embedder
   */

  if (focus && !window->has_toplevel_focus)
    _gtk_plug_windowing_set_focus (plug);
}

static guint
grabbed_key_hash (gconstpointer a)
{
  const GrabbedKey *key = a;
  guint h;
  
  h = key->accelerator_key << 16;
  h ^= key->accelerator_key >> 16;
  h ^= key->accelerator_mods;

  return h;
}

static gboolean
grabbed_key_equal (gconstpointer a, gconstpointer b)
{
  const GrabbedKey *keya = a;
  const GrabbedKey *keyb = b;

  return (keya->accelerator_key == keyb->accelerator_key &&
	  keya->accelerator_mods == keyb->accelerator_mods);
}

static void
add_grabbed_key (gpointer key, gpointer val, gpointer data)
{
  GrabbedKey *grabbed_key = key;
  GtkPlug *plug = data;

  if (!plug->grabbed_keys ||
      !g_hash_table_lookup (plug->grabbed_keys, grabbed_key))
    {
      _gtk_plug_windowing_add_grabbed_key (plug,
					   grabbed_key->accelerator_key,
					   grabbed_key->accelerator_mods);
    }
}

static void
add_grabbed_key_always (gpointer key,
			gpointer val,
			gpointer data)
{
  GrabbedKey *grabbed_key = key;
  GtkPlug *plug = data;

  _gtk_plug_windowing_add_grabbed_key (plug,
				       grabbed_key->accelerator_key,
				       grabbed_key->accelerator_mods);
}

/**
 * _gtk_plug_add_all_grabbed_keys:
 *
 * @plug: a #GtkPlug
 *
 * Calls _gtk_plug_windowing_add_grabbed_key() on all the grabbed keys
 * in the @plug.
 */
void
_gtk_plug_add_all_grabbed_keys (GtkPlug *plug)
{
  if (plug->grabbed_keys)
    g_hash_table_foreach (plug->grabbed_keys, add_grabbed_key_always, plug);
}

static void
remove_grabbed_key (gpointer key, gpointer val, gpointer data)
{
  GrabbedKey *grabbed_key = key;
  GtkPlug *plug = data;

  if (!plug->grabbed_keys ||
      !g_hash_table_lookup (plug->grabbed_keys, grabbed_key))
    {
      _gtk_plug_windowing_remove_grabbed_key (plug, 
					      grabbed_key->accelerator_key,
					      grabbed_key->accelerator_mods);
    }
}

static void
keys_foreach (GtkWindow      *window,
	      guint           keyval,
	      GdkModifierType modifiers,
	      gboolean        is_mnemonic,
	      gpointer        data)
{
  GHashTable *new_grabbed_keys = data;
  GrabbedKey *key = g_slice_new (GrabbedKey);

  key->accelerator_key = keyval;
  key->accelerator_mods = modifiers;
  
  g_hash_table_replace (new_grabbed_keys, key, key);
}

static void
grabbed_key_free (gpointer data)
{
  g_slice_free (GrabbedKey, data);
}

static void
gtk_plug_keys_changed (GtkWindow *window)
{
  GHashTable *new_grabbed_keys, *old_grabbed_keys;
  GtkPlug *plug = GTK_PLUG (window);

  new_grabbed_keys = g_hash_table_new_full (grabbed_key_hash, grabbed_key_equal, (GDestroyNotify)grabbed_key_free, NULL);
  _gtk_window_keys_foreach (window, keys_foreach, new_grabbed_keys);

  if (plug->socket_window)
    g_hash_table_foreach (new_grabbed_keys, add_grabbed_key, plug);

  old_grabbed_keys = plug->grabbed_keys;
  plug->grabbed_keys = new_grabbed_keys;

  if (old_grabbed_keys)
    {
      if (plug->socket_window)
	g_hash_table_foreach (old_grabbed_keys, remove_grabbed_key, plug);
      g_hash_table_destroy (old_grabbed_keys);
    }
}

static gboolean
gtk_plug_focus (GtkWidget        *widget,
		GtkDirectionType  direction)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkPlug *plug = GTK_PLUG (widget);
  GtkWindow *window = GTK_WINDOW (widget);
  GtkContainer *container = GTK_CONTAINER (widget);
  GtkWidget *old_focus_child = container->focus_child;
  GtkWidget *parent;
  
  /* We override GtkWindow's behavior, since we don't want wrapping here.
   */
  if (old_focus_child)
    {
      if (gtk_widget_child_focus (old_focus_child, direction))
	return TRUE;

      if (window->focus_widget)
	{
	  /* Wrapped off the end, clear the focus setting for the toplevel */
	  parent = window->focus_widget->parent;
	  while (parent)
	    {
	      gtk_container_set_focus_child (GTK_CONTAINER (parent), NULL);
	      parent = GTK_WIDGET (parent)->parent;
	    }
	  
	  gtk_window_set_focus (GTK_WINDOW (container), NULL);
	}
    }
  else
    {
      /* Try to focus the first widget in the window */
      if (bin->child && gtk_widget_child_focus (bin->child, direction))
        return TRUE;
    }

  if (!GTK_CONTAINER (window)->focus_child)
    _gtk_plug_windowing_focus_to_parent (plug, direction);

  return FALSE;
}

static void
gtk_plug_check_resize (GtkContainer *container)
{
  if (gtk_widget_is_toplevel (GTK_WIDGET (container)))
    GTK_CONTAINER_CLASS (gtk_plug_parent_class)->check_resize (container);
  else
    GTK_CONTAINER_CLASS (bin_class)->check_resize (container);
}

/**
 * _gtk_plug_handle_modality_on:
 *
 * @plug: a #GtkPlug
 *
 * Called from the GtkPlug backend when the corresponding socket has
 * told the plug that it modality has toggled on.
 */
void
_gtk_plug_handle_modality_on (GtkPlug *plug)
{
  if (!plug->modality_window)
    {
      plug->modality_window = gtk_window_new (GTK_WINDOW_POPUP);
      gtk_window_set_screen (GTK_WINDOW (plug->modality_window),
			     gtk_widget_get_screen (GTK_WIDGET (plug)));
      gtk_widget_realize (plug->modality_window);
      gtk_window_group_add_window (plug->modality_group, GTK_WINDOW (plug->modality_window));
      gtk_grab_add (plug->modality_window);
    }
}

/**
 * _gtk_plug_handle_modality_off:
 *
 * @plug: a #GtkPlug
 *
 * Called from the GtkPlug backend when the corresponding socket has
 * told the plug that it modality has toggled off.
 */
void
_gtk_plug_handle_modality_off (GtkPlug *plug)
{
  if (plug->modality_window)
    {
      gtk_widget_destroy (plug->modality_window);
      plug->modality_window = NULL;
    }
}

/**
 * _gtk_plug_focus_first_last:
 *
 * @plug: a #GtkPlug
 * @direction: a direction
 *
 * Called from the GtkPlug backend when the corresponding socket has
 * told the plug that it has received the focus.
 */
void
_gtk_plug_focus_first_last (GtkPlug          *plug,
			    GtkDirectionType  direction)
{
  GtkWindow *window = GTK_WINDOW (plug);
  GtkWidget *parent;

  if (window->focus_widget)
    {
      parent = window->focus_widget->parent;
      while (parent)
	{
	  gtk_container_set_focus_child (GTK_CONTAINER (parent), NULL);
	  parent = GTK_WIDGET (parent)->parent;
	}
      
      gtk_window_set_focus (GTK_WINDOW (plug), NULL);
    }

  gtk_widget_child_focus (GTK_WIDGET (plug), direction);
}

#define __GTK_PLUG_C__
#include "gtkaliasdef.c"
