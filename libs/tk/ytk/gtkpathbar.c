/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* gtkpathbar.c
 * Copyright (C) 2004  Red Hat, Inc.,  Jonathan Blandford <jrb@gnome.org>
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
#include "gtkpathbar.h"
#include "gtktogglebutton.h"
#include "gtkalignment.h"
#include "gtkarrow.h"
#include "gtkdnd.h"
#include "gtkimage.h"
#include "gtkintl.h"
#include "gtkicontheme.h"
#include "gtkiconfactory.h"
#include "gtklabel.h"
#include "gtkhbox.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkalias.h"

enum {
  PATH_CLICKED,
  LAST_SIGNAL
};

typedef enum {
  NORMAL_BUTTON,
  ROOT_BUTTON,
  HOME_BUTTON,
  DESKTOP_BUTTON
} ButtonType;

#define BUTTON_DATA(x) ((ButtonData *)(x))

#define SCROLL_DELAY_FACTOR 5

static guint path_bar_signals [LAST_SIGNAL] = { 0 };

/* Icon size for if we can't get it from the theme */
#define FALLBACK_ICON_SIZE 16

typedef struct _ButtonData ButtonData;

struct _ButtonData
{
  GtkWidget *button;
  ButtonType type;
  char *dir_name;
  GFile *file;
  GtkWidget *image;
  GtkWidget *label;
  GCancellable *cancellable;
  guint ignore_changes : 1;
  guint file_is_hidden : 1;
};
/* This macro is used to check if a button can be used as a fake root.
 * All buttons in front of a fake root are automatically hidden when in a
 * directory below a fake root and replaced with the "<" arrow button.
 */
#define BUTTON_IS_FAKE_ROOT(button) ((button)->type == HOME_BUTTON)

G_DEFINE_TYPE (GtkPathBar, gtk_path_bar, GTK_TYPE_CONTAINER)

static void gtk_path_bar_finalize                 (GObject          *object);
static void gtk_path_bar_dispose                  (GObject          *object);
static void gtk_path_bar_realize                  (GtkWidget        *widget);
static void gtk_path_bar_unrealize                (GtkWidget        *widget);
static void gtk_path_bar_size_request             (GtkWidget        *widget,
						   GtkRequisition   *requisition);
static void gtk_path_bar_map                      (GtkWidget        *widget);
static void gtk_path_bar_unmap                    (GtkWidget        *widget);
static void gtk_path_bar_size_allocate            (GtkWidget        *widget,
						   GtkAllocation    *allocation);
static void gtk_path_bar_add                      (GtkContainer     *container,
						   GtkWidget        *widget);
static void gtk_path_bar_remove                   (GtkContainer     *container,
						   GtkWidget        *widget);
static void gtk_path_bar_forall                   (GtkContainer     *container,
						   gboolean          include_internals,
						   GtkCallback       callback,
						   gpointer          callback_data);
static gboolean gtk_path_bar_scroll               (GtkWidget        *widget,
						   GdkEventScroll   *event);
static void gtk_path_bar_scroll_up                (GtkPathBar       *path_bar);
static void gtk_path_bar_scroll_down              (GtkPathBar       *path_bar);
static void gtk_path_bar_stop_scrolling           (GtkPathBar       *path_bar);
static gboolean gtk_path_bar_slider_up_defocus    (GtkWidget        *widget,
						   GdkEventButton   *event,
						   GtkPathBar       *path_bar);
static gboolean gtk_path_bar_slider_down_defocus  (GtkWidget        *widget,
						   GdkEventButton   *event,
						   GtkPathBar       *path_bar);
static gboolean gtk_path_bar_slider_button_press  (GtkWidget        *widget,
						   GdkEventButton   *event,
						   GtkPathBar       *path_bar);
static gboolean gtk_path_bar_slider_button_release(GtkWidget        *widget,
						   GdkEventButton   *event,
						   GtkPathBar       *path_bar);
static void gtk_path_bar_grab_notify              (GtkWidget        *widget,
						   gboolean          was_grabbed);
static void gtk_path_bar_state_changed            (GtkWidget        *widget,
						   GtkStateType      previous_state);
static void gtk_path_bar_style_set                (GtkWidget        *widget,
						   GtkStyle         *previous_style);
static void gtk_path_bar_screen_changed           (GtkWidget        *widget,
						   GdkScreen        *previous_screen);
static void gtk_path_bar_check_icon_theme         (GtkPathBar       *path_bar);
static void gtk_path_bar_update_button_appearance (GtkPathBar       *path_bar,
						   ButtonData       *button_data,
						   gboolean          current_dir);

static void
on_slider_unmap (GtkWidget  *widget,
		 GtkPathBar *path_bar)
{
  if (path_bar->timer &&
      ((widget == path_bar->up_slider_button && path_bar->scrolling_up) ||
       (widget == path_bar->down_slider_button && path_bar->scrolling_down)))
    gtk_path_bar_stop_scrolling (path_bar);
}

static GtkWidget *
get_slider_button (GtkPathBar  *path_bar,
		   GtkArrowType arrow_type)
{
  GtkWidget *button;
  AtkObject *atk_obj;

  gtk_widget_push_composite_child ();

  button = gtk_button_new ();
  atk_obj = gtk_widget_get_accessible (button);
  if (arrow_type == GTK_ARROW_LEFT)
    atk_object_set_name (atk_obj, _("Up Path"));
  else
    atk_object_set_name (atk_obj, _("Down Path"));

  gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);
  gtk_container_add (GTK_CONTAINER (button),
                     gtk_arrow_new (arrow_type, GTK_SHADOW_OUT));
  gtk_container_add (GTK_CONTAINER (path_bar), button);
  gtk_widget_show_all (button);

  g_signal_connect (G_OBJECT (button), "unmap",
		    G_CALLBACK (on_slider_unmap), path_bar);

  gtk_widget_pop_composite_child ();

  return button;
}

static void
gtk_path_bar_init (GtkPathBar *path_bar)
{
  gtk_widget_set_has_window (GTK_WIDGET (path_bar), FALSE);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (path_bar), FALSE);

  path_bar->get_info_cancellable = NULL;

  path_bar->spacing = 0;
  path_bar->up_slider_button = get_slider_button (path_bar, GTK_ARROW_LEFT);
  path_bar->down_slider_button = get_slider_button (path_bar, GTK_ARROW_RIGHT);
  path_bar->icon_size = FALLBACK_ICON_SIZE;
  
  g_signal_connect_swapped (path_bar->up_slider_button, "clicked",
                            G_CALLBACK (gtk_path_bar_scroll_up), path_bar);
  g_signal_connect_swapped (path_bar->down_slider_button, "clicked",
                            G_CALLBACK (gtk_path_bar_scroll_down), path_bar);

  g_signal_connect (path_bar->up_slider_button, "focus-out-event",
                    G_CALLBACK (gtk_path_bar_slider_up_defocus), path_bar);
  g_signal_connect (path_bar->down_slider_button, "focus-out-event",
                    G_CALLBACK (gtk_path_bar_slider_down_defocus), path_bar);

  g_signal_connect (path_bar->up_slider_button, "button-press-event",
                    G_CALLBACK (gtk_path_bar_slider_button_press), path_bar);
  g_signal_connect (path_bar->up_slider_button, "button-release-event",
                    G_CALLBACK (gtk_path_bar_slider_button_release), path_bar);
  g_signal_connect (path_bar->down_slider_button, "button-press-event",
                    G_CALLBACK (gtk_path_bar_slider_button_press), path_bar);
  g_signal_connect (path_bar->down_slider_button, "button-release-event",
                    G_CALLBACK (gtk_path_bar_slider_button_release), path_bar);
}

static void
gtk_path_bar_class_init (GtkPathBarClass *path_bar_class)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class = (GObjectClass *) path_bar_class;
  object_class = (GtkObjectClass *) path_bar_class;
  widget_class = (GtkWidgetClass *) path_bar_class;
  container_class = (GtkContainerClass *) path_bar_class;

  gobject_class->finalize = gtk_path_bar_finalize;
  gobject_class->dispose = gtk_path_bar_dispose;

  widget_class->size_request = gtk_path_bar_size_request;
  widget_class->realize = gtk_path_bar_realize;
  widget_class->unrealize = gtk_path_bar_unrealize;
  widget_class->map = gtk_path_bar_map;
  widget_class->unmap = gtk_path_bar_unmap;
  widget_class->size_allocate = gtk_path_bar_size_allocate;
  widget_class->style_set = gtk_path_bar_style_set;
  widget_class->screen_changed = gtk_path_bar_screen_changed;
  widget_class->grab_notify = gtk_path_bar_grab_notify;
  widget_class->state_changed = gtk_path_bar_state_changed;
  widget_class->scroll_event = gtk_path_bar_scroll;

  container_class->add = gtk_path_bar_add;
  container_class->forall = gtk_path_bar_forall;
  container_class->remove = gtk_path_bar_remove;
  /* FIXME: */
  /*  container_class->child_type = gtk_path_bar_child_type;*/

  path_bar_signals [PATH_CLICKED] =
    g_signal_new (I_("path-clicked"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkPathBarClass, path_clicked),
		  NULL, NULL,
		  _gtk_marshal_VOID__POINTER_POINTER_BOOLEAN,
		  G_TYPE_NONE, 3,
		  G_TYPE_POINTER,
		  G_TYPE_POINTER,
		  G_TYPE_BOOLEAN);
}


static void
gtk_path_bar_finalize (GObject *object)
{
  GtkPathBar *path_bar;

  path_bar = GTK_PATH_BAR (object);

  gtk_path_bar_stop_scrolling (path_bar);

  g_list_free (path_bar->button_list);
  if (path_bar->root_file)
    g_object_unref (path_bar->root_file);
  if (path_bar->home_file)
    g_object_unref (path_bar->home_file);
  if (path_bar->desktop_file)
    g_object_unref (path_bar->desktop_file);

  if (path_bar->root_icon)
    g_object_unref (path_bar->root_icon);
  if (path_bar->home_icon)
    g_object_unref (path_bar->home_icon);
  if (path_bar->desktop_icon)
    g_object_unref (path_bar->desktop_icon);

  if (path_bar->file_system)
    g_object_unref (path_bar->file_system);

  G_OBJECT_CLASS (gtk_path_bar_parent_class)->finalize (object);
}

/* Removes the settings signal handler.  It's safe to call multiple times */
static void
remove_settings_signal (GtkPathBar *path_bar,
			GdkScreen  *screen)
{
  if (path_bar->settings_signal_id)
    {
      GtkSettings *settings;

      settings = gtk_settings_get_for_screen (screen);
      g_signal_handler_disconnect (settings,
				   path_bar->settings_signal_id);
      path_bar->settings_signal_id = 0;
    }
}

static void
gtk_path_bar_dispose (GObject *object)
{
  GtkPathBar *path_bar = GTK_PATH_BAR (object);

  remove_settings_signal (path_bar, gtk_widget_get_screen (GTK_WIDGET (object)));

  if (path_bar->get_info_cancellable)
    g_cancellable_cancel (path_bar->get_info_cancellable);
  path_bar->get_info_cancellable = NULL;

  G_OBJECT_CLASS (gtk_path_bar_parent_class)->dispose (object);
}

/* Size requisition:
 * 
 * Ideally, our size is determined by another widget, and we are just filling
 * available space.
 */
static void
gtk_path_bar_size_request (GtkWidget      *widget,
			   GtkRequisition *requisition)
{
  ButtonData *button_data;
  GtkPathBar *path_bar;
  GtkRequisition child_requisition;
  GList *list;

  path_bar = GTK_PATH_BAR (widget);

  requisition->width = 0;
  requisition->height = 0;

  for (list = path_bar->button_list; list; list = list->next)
    {
      button_data = BUTTON_DATA (list->data);
      gtk_widget_size_request (button_data->button, &child_requisition);
      
      if (button_data->type == NORMAL_BUTTON)
	/* Use 2*Height as button width because of ellipsized label.  */
	requisition->width = MAX (child_requisition.height * 2, requisition->width);
      else
	requisition->width = MAX (child_requisition.width, requisition->width);

      requisition->height = MAX (child_requisition.height, requisition->height);
    }

  /* Add space for slider, if we have more than one path */
  /* Theoretically, the slider could be bigger than the other button.  But we're
   * not going to worry about that now.
   */
  path_bar->slider_width = MIN(requisition->height * 2 / 3 + 5, requisition->height);
  if (path_bar->button_list && path_bar->button_list->next != NULL)
    requisition->width += (path_bar->spacing + path_bar->slider_width) * 2;

  gtk_widget_size_request (path_bar->up_slider_button, &child_requisition);
  gtk_widget_size_request (path_bar->down_slider_button, &child_requisition);

  requisition->width += GTK_CONTAINER (widget)->border_width * 2;
  requisition->height += GTK_CONTAINER (widget)->border_width * 2;

  widget->requisition = *requisition;
}

static void
gtk_path_bar_update_slider_buttons (GtkPathBar *path_bar)
{
  if (path_bar->button_list)
    {
      GtkWidget *button;

      button = BUTTON_DATA (path_bar->button_list->data)->button;
      if (gtk_widget_get_child_visible (button))
	{
	  gtk_path_bar_stop_scrolling (path_bar);
	  gtk_widget_set_sensitive (path_bar->down_slider_button, FALSE);
	}
      else
	gtk_widget_set_sensitive (path_bar->down_slider_button, TRUE);

      button = BUTTON_DATA (g_list_last (path_bar->button_list)->data)->button;
      if (gtk_widget_get_child_visible (button))
	{
	  gtk_path_bar_stop_scrolling (path_bar);
	  gtk_widget_set_sensitive (path_bar->up_slider_button, FALSE);
	}
      else
	gtk_widget_set_sensitive (path_bar->up_slider_button, TRUE);
    }
}

static void
gtk_path_bar_map (GtkWidget *widget)
{
  gdk_window_show (GTK_PATH_BAR (widget)->event_window);

  GTK_WIDGET_CLASS (gtk_path_bar_parent_class)->map (widget);
}

static void
gtk_path_bar_unmap (GtkWidget *widget)
{
  gtk_path_bar_stop_scrolling (GTK_PATH_BAR (widget));
  gdk_window_hide (GTK_PATH_BAR (widget)->event_window);

  GTK_WIDGET_CLASS (gtk_path_bar_parent_class)->unmap (widget);
}

static void
gtk_path_bar_realize (GtkWidget *widget)
{
  GtkPathBar *path_bar;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

  path_bar = GTK_PATH_BAR (widget);
  widget->window = gtk_widget_get_parent_window (widget);
  g_object_ref (widget->window);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_SCROLL_MASK;
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  path_bar->event_window = gdk_window_new (gtk_widget_get_parent_window (widget),
							&attributes, attributes_mask);
  gdk_window_set_user_data (path_bar->event_window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
}

static void
gtk_path_bar_unrealize (GtkWidget *widget)
{
  GtkPathBar *path_bar;

  path_bar = GTK_PATH_BAR (widget);

  gdk_window_set_user_data (path_bar->event_window, NULL);
  gdk_window_destroy (path_bar->event_window);
  path_bar->event_window = NULL;

  GTK_WIDGET_CLASS (gtk_path_bar_parent_class)->unrealize (widget);
}

/* This is a tad complicated
 */
static void
gtk_path_bar_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  GtkWidget *child;
  GtkPathBar *path_bar = GTK_PATH_BAR (widget);
  GtkTextDirection direction;
  GtkAllocation child_allocation;
  GList *list, *first_button;
  gint width;
  gint allocation_width;
  gint border_width;
  gboolean need_sliders = FALSE;
  gint up_slider_offset = 0;

  widget->allocation = *allocation;

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (path_bar->event_window,
			    allocation->x, allocation->y,
			    allocation->width, allocation->height);

  /* No path is set; we don't have to allocate anything. */
  if (path_bar->button_list == NULL)
    return;

  direction = gtk_widget_get_direction (widget);
  border_width = (gint) GTK_CONTAINER (path_bar)->border_width;
  allocation_width = allocation->width - 2 * border_width;

  /* First, we check to see if we need the scrollbars. */
  if (path_bar->fake_root)
    width = path_bar->spacing + path_bar->slider_width;
  else
      width = 0;

  for (list = path_bar->button_list; list; list = list->next)
    {
      child = BUTTON_DATA (list->data)->button;

      width += child->requisition.width + path_bar->spacing;
      if (list == path_bar->fake_root)
	break;
    }

  if (width <= allocation_width)
    {
      if (path_bar->fake_root)
	first_button = path_bar->fake_root;
      else
	first_button = g_list_last (path_bar->button_list);
    }
  else
    {
      gboolean reached_end = FALSE;
      gint slider_space = 2 * (path_bar->spacing + path_bar->slider_width);

      if (path_bar->first_scrolled_button)
	first_button = path_bar->first_scrolled_button;
      else
	first_button = path_bar->button_list;
      need_sliders = TRUE;
      
      /* To see how much space we have, and how many buttons we can display.
       * We start at the first button, count forward until hit the new
       * button, then count backwards.
       */
      /* Count down the path chain towards the end. */
      width = BUTTON_DATA (first_button->data)->button->requisition.width;
      list = first_button->prev;
      while (list && !reached_end)
	{
	  child = BUTTON_DATA (list->data)->button;

	  if (width + child->requisition.width +
	      path_bar->spacing + slider_space > allocation_width)
	    reached_end = TRUE;
	  else if (list == path_bar->fake_root)
	    break;
	  else
	    width += child->requisition.width + path_bar->spacing;

	  list = list->prev;
	}

      /* Finally, we walk up, seeing how many of the previous buttons we can
       * add */
      while (first_button->next && !reached_end)
	{
	  child = BUTTON_DATA (first_button->next->data)->button;

	  if (width + child->requisition.width + path_bar->spacing + slider_space > allocation_width)
	    {
	      reached_end = TRUE;
	    }
	  else
	    {
	      width += child->requisition.width + path_bar->spacing;
	      if (first_button == path_bar->fake_root)
		break;
	      first_button = first_button->next;
	    }
	}
    }

  /* Now, we allocate space to the buttons */
  child_allocation.y = allocation->y + border_width;
  child_allocation.height = MAX (1, (gint) allocation->height - border_width * 2);

  if (direction == GTK_TEXT_DIR_RTL)
    {
      child_allocation.x = allocation->x + allocation->width - border_width;
      if (need_sliders || path_bar->fake_root)
	{
	  child_allocation.x -= (path_bar->spacing + path_bar->slider_width);
	  up_slider_offset = allocation->width - border_width - path_bar->slider_width;
	}
    }
  else
    {
      child_allocation.x = allocation->x + border_width;
      if (need_sliders || path_bar->fake_root)
	{
	  up_slider_offset = border_width;
	  child_allocation.x += (path_bar->spacing + path_bar->slider_width);
	}
    }

  for (list = first_button; list; list = list->prev)
    {
      ButtonData *button_data;

      button_data = BUTTON_DATA (list->data);
      child = button_data->button;

      child_allocation.width = MIN (child->requisition.width,
				    allocation_width - (path_bar->spacing + path_bar->slider_width) * 2);

      if (direction == GTK_TEXT_DIR_RTL)
	child_allocation.x -= child_allocation.width;

      /* Check to see if we've don't have any more space to allocate buttons */
      if (need_sliders && direction == GTK_TEXT_DIR_RTL)
	{
	  if (child_allocation.x - path_bar->spacing - path_bar->slider_width < widget->allocation.x + border_width)
	    break;
	}
      else if (need_sliders && direction == GTK_TEXT_DIR_LTR)
	{
	  if (child_allocation.x + child_allocation.width + path_bar->spacing + path_bar->slider_width >
	      widget->allocation.x + border_width + allocation_width)
	    break;
	}

      if (child_allocation.width < child->requisition.width)
	{
	  if (!gtk_widget_get_has_tooltip (child))
	    gtk_widget_set_tooltip_text (child, button_data->dir_name);
	}
      else if (gtk_widget_get_has_tooltip (child))
	gtk_widget_set_tooltip_text (child, NULL);
      
      gtk_widget_set_child_visible (child, TRUE);
      gtk_widget_size_allocate (child, &child_allocation);

      if (direction == GTK_TEXT_DIR_RTL)
	child_allocation.x -= path_bar->spacing;
      else
	child_allocation.x += child_allocation.width + path_bar->spacing;
    }
  /* Now we go hide all the widgets that don't fit */
  while (list)
    {
      gtk_widget_set_child_visible (BUTTON_DATA (list->data)->button, FALSE);
      list = list->prev;
    }
  for (list = first_button->next; list; list = list->next)
    {
      gtk_widget_set_child_visible (BUTTON_DATA (list->data)->button, FALSE);
    }

  if (need_sliders || path_bar->fake_root)
    {
      child_allocation.width = path_bar->slider_width;
      child_allocation.x = up_slider_offset + allocation->x;
      gtk_widget_size_allocate (path_bar->up_slider_button, &child_allocation);

      gtk_widget_set_child_visible (path_bar->up_slider_button, TRUE);
      gtk_widget_show_all (path_bar->up_slider_button);
    }
  else
    gtk_widget_set_child_visible (path_bar->up_slider_button, FALSE);
      
  if (need_sliders)
    {
      child_allocation.width = path_bar->slider_width;

      if (direction == GTK_TEXT_DIR_RTL)
	child_allocation.x = border_width;
      else
	child_allocation.x = allocation->width - border_width - path_bar->slider_width;

      child_allocation.x += allocation->x;
      
      gtk_widget_size_allocate (path_bar->down_slider_button, &child_allocation);

      gtk_widget_set_child_visible (path_bar->down_slider_button, TRUE);
      gtk_widget_show_all (path_bar->down_slider_button);
      gtk_path_bar_update_slider_buttons (path_bar);
    }
  else
    gtk_widget_set_child_visible (path_bar->down_slider_button, FALSE);
}

static void
gtk_path_bar_style_set (GtkWidget *widget,
			GtkStyle  *previous_style)
{
  GTK_WIDGET_CLASS (gtk_path_bar_parent_class)->style_set (widget, previous_style);

  gtk_path_bar_check_icon_theme (GTK_PATH_BAR (widget));
}

static void
gtk_path_bar_screen_changed (GtkWidget *widget,
			     GdkScreen *previous_screen)
{
  if (GTK_WIDGET_CLASS (gtk_path_bar_parent_class)->screen_changed)
    GTK_WIDGET_CLASS (gtk_path_bar_parent_class)->screen_changed (widget, previous_screen);

  /* We might nave a new settings, so we remove the old one */
  if (previous_screen)
    remove_settings_signal (GTK_PATH_BAR (widget), previous_screen);

  gtk_path_bar_check_icon_theme (GTK_PATH_BAR (widget));
}

static gboolean
gtk_path_bar_scroll (GtkWidget      *widget,
		     GdkEventScroll *event)
{
  switch (event->direction)
    {
    case GDK_SCROLL_RIGHT:
    case GDK_SCROLL_DOWN:
      gtk_path_bar_scroll_down (GTK_PATH_BAR (widget));
      break;
    case GDK_SCROLL_LEFT:
    case GDK_SCROLL_UP:
      gtk_path_bar_scroll_up (GTK_PATH_BAR (widget));
      break;
    }

  return TRUE;
}

static void
gtk_path_bar_add (GtkContainer *container,
		  GtkWidget    *widget)
{
  gtk_widget_set_parent (widget, GTK_WIDGET (container));
}

static void
gtk_path_bar_remove_1 (GtkContainer *container,
		       GtkWidget    *widget)
{
  gboolean was_visible = gtk_widget_get_visible (widget);
  gtk_widget_unparent (widget);
  if (was_visible)
    gtk_widget_queue_resize (GTK_WIDGET (container));
}

static void
gtk_path_bar_remove (GtkContainer *container,
		     GtkWidget    *widget)
{
  GtkPathBar *path_bar;
  GList *children;

  path_bar = GTK_PATH_BAR (container);

  if (widget == path_bar->up_slider_button)
    {
      gtk_path_bar_remove_1 (container, widget);
      path_bar->up_slider_button = NULL;
      return;
    }

  if (widget == path_bar->down_slider_button)
    {
      gtk_path_bar_remove_1 (container, widget);
      path_bar->down_slider_button = NULL;
      return;
    }

  children = path_bar->button_list;
  while (children)
    {
      if (widget == BUTTON_DATA (children->data)->button)
	{
	  gtk_path_bar_remove_1 (container, widget);
	  path_bar->button_list = g_list_remove_link (path_bar->button_list, children);
	  g_list_free (children);
	  return;
	}
      
      children = children->next;
    }
}

static void
gtk_path_bar_forall (GtkContainer *container,
		     gboolean      include_internals,
		     GtkCallback   callback,
		     gpointer      callback_data)
{
  GtkPathBar *path_bar;
  GList *children;

  g_return_if_fail (callback != NULL);
  path_bar = GTK_PATH_BAR (container);

  children = path_bar->button_list;
  while (children)
    {
      GtkWidget *child;
      child = BUTTON_DATA (children->data)->button;
      children = children->next;

      (* callback) (child, callback_data);
    }

  if (path_bar->up_slider_button)
    (* callback) (path_bar->up_slider_button, callback_data);

  if (path_bar->down_slider_button)
    (* callback) (path_bar->down_slider_button, callback_data);
}

static void
gtk_path_bar_scroll_down (GtkPathBar *path_bar)
{
  GList *list;
  GList *down_button = NULL;
  gint space_available;

  if (path_bar->ignore_click)
    {
      path_bar->ignore_click = FALSE;
      return;   
    }

  if (gtk_widget_get_child_visible (BUTTON_DATA (path_bar->button_list->data)->button))
    {
      /* Return if the last button is already visible */
      return;
    }

  gtk_widget_queue_resize (GTK_WIDGET (path_bar));

  /* We find the button at the 'down' end that we have to make
   * visible */
  for (list = path_bar->button_list; list; list = list->next)
    {
      if (list->next && gtk_widget_get_child_visible (BUTTON_DATA (list->next->data)->button))
	{
	  down_button = list;
	  break;
	}
    }

  space_available = (GTK_WIDGET (path_bar)->allocation.width
		     - 2 * GTK_CONTAINER (path_bar)->border_width
		     - 2 * path_bar->spacing - 2 * path_bar->slider_width
		     - BUTTON_DATA (down_button->data)->button->allocation.width);
  path_bar->first_scrolled_button = down_button;
  
  /* We have space_available free space that's not being used.  
   * So we walk down from the end, adding buttons until we use all free space.
   */
  while (space_available > 0)
    {
      path_bar->first_scrolled_button = down_button;
      down_button = down_button->next;
      if (!down_button)
	break;
      space_available -= (BUTTON_DATA (down_button->data)->button->allocation.width
			  + path_bar->spacing);
    }
}

static void
gtk_path_bar_scroll_up (GtkPathBar *path_bar)
{
  GList *list;

  if (path_bar->ignore_click)
    {
      path_bar->ignore_click = FALSE;
      return;   
    }

  list = g_list_last (path_bar->button_list);

  if (gtk_widget_get_child_visible (BUTTON_DATA (list->data)->button))
    {
      /* Return if the first button is already visible */
      return;
    }

  gtk_widget_queue_resize (GTK_WIDGET (path_bar));

  for ( ; list; list = list->prev)
    {
      if (list->prev && gtk_widget_get_child_visible (BUTTON_DATA (list->prev->data)->button))
	{
	  if (list->prev == path_bar->fake_root)
	    path_bar->fake_root = NULL;
	  path_bar->first_scrolled_button = list;
	  return;
	}
    }
}

static gboolean
gtk_path_bar_scroll_timeout (GtkPathBar *path_bar)
{
  gboolean retval = FALSE;

  if (path_bar->timer)
    {
      if (path_bar->scrolling_up)
	gtk_path_bar_scroll_up (path_bar);
      else if (path_bar->scrolling_down)
	gtk_path_bar_scroll_down (path_bar);

      if (path_bar->need_timer) 
	{
          GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (path_bar));
          guint        timeout;

          g_object_get (settings, "gtk-timeout-repeat", &timeout, NULL);

	  path_bar->need_timer = FALSE;

	  path_bar->timer = gdk_threads_add_timeout (timeout * SCROLL_DELAY_FACTOR,
					   (GSourceFunc)gtk_path_bar_scroll_timeout,
					   path_bar);
	}
      else
	retval = TRUE;
    }

  return retval;
}

static void 
gtk_path_bar_stop_scrolling (GtkPathBar *path_bar)
{
  if (path_bar->timer)
    {
      g_source_remove (path_bar->timer);
      path_bar->timer = 0;
      path_bar->need_timer = FALSE;
    }
}

static gboolean
gtk_path_bar_slider_up_defocus (GtkWidget      *widget,
                                    GdkEventButton *event,
                                    GtkPathBar     *path_bar)
{
  GList *list;
  GList *up_button = NULL;

  if (event->type != GDK_FOCUS_CHANGE)
    return FALSE;

  for (list = g_list_last (path_bar->button_list); list; list = list->prev)
    {
      if (gtk_widget_get_child_visible (BUTTON_DATA (list->data)->button))
        {
          up_button = list;
          break;
        }
    }

  /* don't let the focus vanish */
  if ((!gtk_widget_is_sensitive (path_bar->up_slider_button)) ||
      (!gtk_widget_get_child_visible (path_bar->up_slider_button)))
    gtk_widget_grab_focus (BUTTON_DATA (up_button->data)->button);

  return FALSE;
}

static gboolean
gtk_path_bar_slider_down_defocus (GtkWidget      *widget,
                                    GdkEventButton *event,
                                    GtkPathBar     *path_bar)
{
  GList *list;
  GList *down_button = NULL;

  if (event->type != GDK_FOCUS_CHANGE)
    return FALSE;

  for (list = path_bar->button_list; list; list = list->next)
    {
      if (gtk_widget_get_child_visible (BUTTON_DATA (list->data)->button))
        {
          down_button = list;
          break;
        }
    }

  /* don't let the focus vanish */
  if ((!gtk_widget_is_sensitive (path_bar->down_slider_button)) ||
      (!gtk_widget_get_child_visible (path_bar->down_slider_button)))
    gtk_widget_grab_focus (BUTTON_DATA (down_button->data)->button);

  return FALSE;
}

static gboolean
gtk_path_bar_slider_button_press (GtkWidget      *widget, 
				  GdkEventButton *event,
				  GtkPathBar     *path_bar)
{
  if (event->type != GDK_BUTTON_PRESS || event->button != 1)
    return FALSE;

  path_bar->ignore_click = FALSE;

  if (widget == path_bar->up_slider_button)
    {
      path_bar->scrolling_down = FALSE;
      path_bar->scrolling_up = TRUE;
      gtk_path_bar_scroll_up (path_bar);
    }
  else if (widget == path_bar->down_slider_button)
    {
      path_bar->scrolling_up = FALSE;
      path_bar->scrolling_down = TRUE;
      gtk_path_bar_scroll_down (path_bar);
    }

  if (!path_bar->timer)
    {
      GtkSettings *settings = gtk_widget_get_settings (widget);
      guint        timeout;

      g_object_get (settings, "gtk-timeout-initial", &timeout, NULL);

      path_bar->need_timer = TRUE;
      path_bar->timer = gdk_threads_add_timeout (timeout,
				       (GSourceFunc)gtk_path_bar_scroll_timeout,
				       path_bar);
    }

  return FALSE;
}

static gboolean
gtk_path_bar_slider_button_release (GtkWidget      *widget, 
				    GdkEventButton *event,
				    GtkPathBar     *path_bar)
{
  if (event->type != GDK_BUTTON_RELEASE)
    return FALSE;

  path_bar->ignore_click = TRUE;
  gtk_path_bar_stop_scrolling (path_bar);

  return FALSE;
}

static void
gtk_path_bar_grab_notify (GtkWidget *widget,
			  gboolean   was_grabbed)
{
  if (!was_grabbed)
    gtk_path_bar_stop_scrolling (GTK_PATH_BAR (widget));
}

static void
gtk_path_bar_state_changed (GtkWidget    *widget,
			    GtkStateType  previous_state)
{
  if (!gtk_widget_is_sensitive (widget))
    gtk_path_bar_stop_scrolling (GTK_PATH_BAR (widget));
}


/* Changes the icons wherever it is needed */
static void
reload_icons (GtkPathBar *path_bar)
{
  GList *list;

  if (path_bar->root_icon)
    {
      g_object_unref (path_bar->root_icon);
      path_bar->root_icon = NULL;
    }
  if (path_bar->home_icon)
    {
      g_object_unref (path_bar->home_icon);
      path_bar->home_icon = NULL;
    }
  if (path_bar->desktop_icon)
    {
      g_object_unref (path_bar->desktop_icon);
      path_bar->desktop_icon = NULL;
    }

  for (list = path_bar->button_list; list; list = list->next)
    {
      ButtonData *button_data;
      gboolean current_dir;

      button_data = BUTTON_DATA (list->data);
      if (button_data->type != NORMAL_BUTTON)
	{
	  current_dir = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_data->button));
	  gtk_path_bar_update_button_appearance (path_bar, button_data, current_dir);
	}
    }
  
}

static void
change_icon_theme (GtkPathBar *path_bar)
{
  GtkSettings *settings;
  gint width, height;

  settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (path_bar)));

  if (gtk_icon_size_lookup_for_settings (settings, GTK_ICON_SIZE_MENU, &width, &height))
    path_bar->icon_size = MAX (width, height);
  else
    path_bar->icon_size = FALLBACK_ICON_SIZE;

  reload_icons (path_bar);
}
/* Callback used when a GtkSettings value changes */
static void
settings_notify_cb (GObject    *object,
		    GParamSpec *pspec,
		    GtkPathBar *path_bar)
{
  const char *name;

  name = g_param_spec_get_name (pspec);

  if (! strcmp (name, "gtk-icon-theme-name") ||
      ! strcmp (name, "gtk-icon-sizes"))
    change_icon_theme (path_bar);
}

static void
gtk_path_bar_check_icon_theme (GtkPathBar *path_bar)
{
  GtkSettings *settings;

  if (path_bar->settings_signal_id)
    return;

  settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (path_bar)));
  path_bar->settings_signal_id = g_signal_connect (settings, "notify", G_CALLBACK (settings_notify_cb), path_bar);

  change_icon_theme (path_bar);
}

/* Public functions and their helpers */
static void
gtk_path_bar_clear_buttons (GtkPathBar *path_bar)
{
  while (path_bar->button_list != NULL)
    {
      gtk_container_remove (GTK_CONTAINER (path_bar), BUTTON_DATA (path_bar->button_list->data)->button);
    }
  path_bar->first_scrolled_button = NULL;
  path_bar->fake_root = NULL;
}

static void
button_clicked_cb (GtkWidget *button,
		   gpointer   data)
{
  ButtonData *button_data;
  GtkPathBar *path_bar;
  GList *button_list;
  gboolean child_is_hidden;
  GFile *child_file;

  button_data = BUTTON_DATA (data);
  if (button_data->ignore_changes)
    return;

  path_bar = GTK_PATH_BAR (button->parent);

  button_list = g_list_find (path_bar->button_list, button_data);
  g_assert (button_list != NULL);

  g_signal_handlers_block_by_func (button,
				   G_CALLBACK (button_clicked_cb), data);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  g_signal_handlers_unblock_by_func (button,
				     G_CALLBACK (button_clicked_cb), data);

  if (button_list->prev)
    {
      ButtonData *child_data;

      child_data = BUTTON_DATA (button_list->prev->data);
      child_file = child_data->file;
      child_is_hidden = child_data->file_is_hidden;
    }
  else
    {
      child_file = NULL;
      child_is_hidden = FALSE;
    }

  g_signal_emit (path_bar, path_bar_signals [PATH_CLICKED], 0,
		 button_data->file, child_file, child_is_hidden);
}

struct SetButtonImageData
{
  GtkPathBar *path_bar;
  ButtonData *button_data;
};

static void
set_button_image_get_info_cb (GCancellable *cancellable,
			      GFileInfo    *info,
			      const GError *error,
			      gpointer      user_data)
{
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  GdkPixbuf *pixbuf;
  struct SetButtonImageData *data = user_data;

  if (cancellable != data->button_data->cancellable)
    goto out;

  data->button_data->cancellable = NULL;

  if (!data->button_data->button)
    {
      g_free (data->button_data);
      goto out;
    }

  if (cancelled || error)
    goto out;

  pixbuf = _gtk_file_info_render_icon (info, GTK_WIDGET (data->path_bar),
			 	       data->path_bar->icon_size);
  gtk_image_set_from_pixbuf (GTK_IMAGE (data->button_data->image), pixbuf);

  switch (data->button_data->type)
    {
      case HOME_BUTTON:
	if (data->path_bar->home_icon)
	  g_object_unref (pixbuf);
	else
	  data->path_bar->home_icon = pixbuf;
	break;

      case DESKTOP_BUTTON:
	if (data->path_bar->desktop_icon)
	  g_object_unref (pixbuf);
	else
	  data->path_bar->desktop_icon = pixbuf;
	break;

      default:
	break;
    };

out:
  g_free (data);
  g_object_unref (cancellable);
}

static void
set_button_image (GtkPathBar *path_bar,
		  ButtonData *button_data)
{
  GtkFileSystemVolume *volume;
  struct SetButtonImageData *data;

  switch (button_data->type)
    {
    case ROOT_BUTTON:

      if (path_bar->root_icon != NULL)
        {
          gtk_image_set_from_pixbuf (GTK_IMAGE (button_data->image), path_bar->root_icon);
	  break;
	}

      volume = _gtk_file_system_get_volume_for_file (path_bar->file_system, path_bar->root_file);
      if (volume == NULL)
	return;

      path_bar->root_icon = _gtk_file_system_volume_render_icon (volume,
								 GTK_WIDGET (path_bar),
								 path_bar->icon_size,
								 NULL);
      _gtk_file_system_volume_unref (volume);

      gtk_image_set_from_pixbuf (GTK_IMAGE (button_data->image), path_bar->root_icon);
      break;

    case HOME_BUTTON:
      if (path_bar->home_icon != NULL)
        {
	  gtk_image_set_from_pixbuf (GTK_IMAGE (button_data->image), path_bar->home_icon);
	  break;
	}

      data = g_new0 (struct SetButtonImageData, 1);
      data->path_bar = path_bar;
      data->button_data = button_data;

      if (button_data->cancellable)
	g_cancellable_cancel (button_data->cancellable);

      button_data->cancellable =
        _gtk_file_system_get_info (path_bar->file_system,
				   path_bar->home_file,
				   "standard::icon",
				   set_button_image_get_info_cb,
				   data);
      break;

    case DESKTOP_BUTTON:
      if (path_bar->desktop_icon != NULL)
        {
	  gtk_image_set_from_pixbuf (GTK_IMAGE (button_data->image), path_bar->desktop_icon);
	  break;
	}

      data = g_new0 (struct SetButtonImageData, 1);
      data->path_bar = path_bar;
      data->button_data = button_data;

      if (button_data->cancellable)
	g_cancellable_cancel (button_data->cancellable);

      button_data->cancellable =
        _gtk_file_system_get_info (path_bar->file_system,
				   path_bar->desktop_file,
				   "standard::icon",
				   set_button_image_get_info_cb,
				   data);
      break;
    default:
      break;
    }
}

static void
button_data_free (ButtonData *button_data)
{
  if (button_data->file)
    g_object_unref (button_data->file);
  button_data->file = NULL;

  g_free (button_data->dir_name);
  button_data->dir_name = NULL;

  button_data->button = NULL;

  if (button_data->cancellable)
    g_cancellable_cancel (button_data->cancellable);
  else
    g_free (button_data);
}

static const char *
get_dir_name (ButtonData *button_data)
{
  return button_data->dir_name;
}

/* We always want to request the same size for the label, whether
 * or not the contents are bold
 */
static void
label_size_request_cb (GtkWidget      *widget,
		       GtkRequisition *requisition,
		       ButtonData     *button_data)
{
  const gchar *dir_name = get_dir_name (button_data);
  PangoLayout *layout = gtk_widget_create_pango_layout (button_data->label, dir_name);
  gint bold_width, bold_height;
  gchar *markup;

  pango_layout_get_pixel_size (layout, &requisition->width, &requisition->height);
  
  markup = g_markup_printf_escaped ("<b>%s</b>", dir_name);
  pango_layout_set_markup (layout, markup, -1);
  g_free (markup);

  pango_layout_get_pixel_size (layout, &bold_width, &bold_height);
  requisition->width = MAX (requisition->width, bold_width);
  requisition->height = MAX (requisition->height, bold_height);
  
  g_object_unref (layout);
}

static void
gtk_path_bar_update_button_appearance (GtkPathBar *path_bar,
				       ButtonData *button_data,
				       gboolean    current_dir)
{
  const gchar *dir_name = get_dir_name (button_data);

  if (button_data->label != NULL)
    {
      if (current_dir)
	{
	  char *markup;

	  markup = g_markup_printf_escaped ("<b>%s</b>", dir_name);
	  gtk_label_set_markup (GTK_LABEL (button_data->label), markup);
	  g_free (markup);
	}
      else
	{
	  gtk_label_set_text (GTK_LABEL (button_data->label), dir_name);
	}
    }

  if (button_data->image != NULL)
    {
      set_button_image (path_bar, button_data);
    }

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_data->button)) != current_dir)
    {
      button_data->ignore_changes = TRUE;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_data->button), current_dir);
      button_data->ignore_changes = FALSE;
    }
}

static ButtonType
find_button_type (GtkPathBar  *path_bar,
		  GFile       *file)
{
  if (path_bar->root_file != NULL &&
      g_file_equal (file, path_bar->root_file))
    return ROOT_BUTTON;
  if (path_bar->home_file != NULL &&
      g_file_equal (file, path_bar->home_file))
    return HOME_BUTTON;
  if (path_bar->desktop_file != NULL &&
      g_file_equal (file, path_bar->desktop_file))
    return DESKTOP_BUTTON;

 return NORMAL_BUTTON;
}

static void
button_drag_data_get_cb (GtkWidget          *widget,
			 GdkDragContext     *context,
			 GtkSelectionData   *selection_data,
			 guint               info,
			 guint               time_,
			 gpointer            data)
{
  ButtonData *button_data;
  char *uris[2];

  button_data = data;

  uris[0] = g_file_get_uri (button_data->file);
  uris[1] = NULL;

  gtk_selection_data_set_uris (selection_data, uris);
  g_free (uris[0]);
}

static ButtonData *
make_directory_button (GtkPathBar  *path_bar,
		       const char  *dir_name,
		       GFile       *file,
		       gboolean     current_dir,
		       gboolean     file_is_hidden)
{
  AtkObject *atk_obj;
  GtkWidget *child = NULL;
  GtkWidget *label_alignment = NULL;
  ButtonData *button_data;

  file_is_hidden = !! file_is_hidden;
  /* Is it a special button? */
  button_data = g_new0 (ButtonData, 1);

  button_data->type = find_button_type (path_bar, file);
  button_data->button = gtk_toggle_button_new ();
  atk_obj = gtk_widget_get_accessible (button_data->button);
  gtk_button_set_focus_on_click (GTK_BUTTON (button_data->button), FALSE);

  switch (button_data->type)
    {
    case ROOT_BUTTON:
      button_data->image = gtk_image_new ();
      child = button_data->image;
      button_data->label = NULL;
      atk_object_set_name (atk_obj, _("File System Root"));
      break;
    case HOME_BUTTON:
    case DESKTOP_BUTTON:
      button_data->image = gtk_image_new ();
      button_data->label = gtk_label_new (NULL);
      label_alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
      gtk_container_add (GTK_CONTAINER (label_alignment), button_data->label);
      child = gtk_hbox_new (FALSE, 2);
      gtk_box_pack_start (GTK_BOX (child), button_data->image, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (child), label_alignment, FALSE, FALSE, 0);
      break;
    case NORMAL_BUTTON:
    default:
      button_data->label = gtk_label_new (NULL);
      gtk_label_set_ellipsize (GTK_LABEL (button_data->label), PANGO_ELLIPSIZE_END);
      label_alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
      gtk_container_add (GTK_CONTAINER (label_alignment), button_data->label);
      child = label_alignment;
      button_data->image = NULL;
    }

  /* label_alignment is created because we can't override size-request
   * on label itself and still have the contents of the label centered
   * properly in the label's requisition
   */
  if (label_alignment)
    g_signal_connect (label_alignment, "size-request",
		      G_CALLBACK (label_size_request_cb), button_data);

  button_data->dir_name = g_strdup (dir_name);
  button_data->file = g_object_ref (file);
  button_data->file_is_hidden = file_is_hidden;

  gtk_container_add (GTK_CONTAINER (button_data->button), child);
  gtk_widget_show_all (button_data->button);

  gtk_path_bar_update_button_appearance (path_bar, button_data, current_dir);

  g_signal_connect (button_data->button, "clicked",
		    G_CALLBACK (button_clicked_cb),
		    button_data);
  g_object_weak_ref (G_OBJECT (button_data->button),
		     (GWeakNotify) button_data_free, button_data);

  gtk_drag_source_set (button_data->button,
		       GDK_BUTTON1_MASK,
		       NULL, 0,
		       GDK_ACTION_COPY);
  gtk_drag_source_add_uri_targets (button_data->button);
  g_signal_connect (button_data->button, "drag-data-get",
		    G_CALLBACK (button_drag_data_get_cb), button_data);

  return button_data;
}

static gboolean
gtk_path_bar_check_parent_path (GtkPathBar         *path_bar,
				GFile              *file,
				GtkFileSystem      *file_system)
{
  GList *list;
  GList *current_path = NULL;
  gboolean need_new_fake_root = FALSE;

  for (list = path_bar->button_list; list; list = list->next)
    {
      ButtonData *button_data;

      button_data = list->data;
      if (g_file_equal (file, button_data->file))
	{
	  current_path = list;
	  break;
	}
      if (list == path_bar->fake_root)
	need_new_fake_root = TRUE;
    }

  if (current_path)
    {
      if (need_new_fake_root)
	{
	  path_bar->fake_root = NULL;
	  for (list = current_path; list; list = list->next)
	    {
	      ButtonData *button_data;

	      button_data = list->data;
	      if (BUTTON_IS_FAKE_ROOT (button_data))
		{
		  path_bar->fake_root = list;
		  break;
		}
	    }
	}

      for (list = path_bar->button_list; list; list = list->next)
	{
	  gtk_path_bar_update_button_appearance (path_bar,
						 BUTTON_DATA (list->data),
						 (list == current_path) ? TRUE : FALSE);
	}

      if (!gtk_widget_get_child_visible (BUTTON_DATA (current_path->data)->button))
	{
	  path_bar->first_scrolled_button = current_path;
	  gtk_widget_queue_resize (GTK_WIDGET (path_bar));
	}

      return TRUE;
    }
  return FALSE;
}


struct SetFileInfo
{
  GFile *file;
  GFile *parent_file;
  GtkPathBar *path_bar;
  GList *new_buttons;
  GList *fake_root;
  gboolean first_directory;
};

static void
gtk_path_bar_set_file_finish (struct SetFileInfo *info,
                              gboolean            result)
{
  if (result)
    {
      GList *l;

      gtk_path_bar_clear_buttons (info->path_bar);
      info->path_bar->button_list = g_list_reverse (info->new_buttons);
      info->path_bar->fake_root = info->fake_root;

      for (l = info->path_bar->button_list; l; l = l->next)
	{
	  GtkWidget *button = BUTTON_DATA (l->data)->button;
	  gtk_container_add (GTK_CONTAINER (info->path_bar), button);
	}
    }
  else
    {
      GList *l;

      for (l = info->new_buttons; l; l = l->next)
	{
	  ButtonData *button_data;

	  button_data = BUTTON_DATA (l->data);
	  gtk_widget_destroy (button_data->button);
	}

      g_list_free (info->new_buttons);
    }

  if (info->file)
    g_object_unref (info->file);
  if (info->parent_file)
    g_object_unref (info->parent_file);
  g_free (info);
}

static void
gtk_path_bar_get_info_callback (GCancellable *cancellable,
			        GFileInfo    *info,
			        const GError *error,
			        gpointer      data)
{
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  struct SetFileInfo *file_info = data;
  ButtonData *button_data;
  const gchar *display_name;
  gboolean is_hidden;

  if (cancellable != file_info->path_bar->get_info_cancellable)
    {
      gtk_path_bar_set_file_finish (file_info, FALSE);
      g_object_unref (cancellable);
      return;
    }

  g_object_unref (cancellable);
  file_info->path_bar->get_info_cancellable = NULL;

  if (cancelled || !info)
    {
      gtk_path_bar_set_file_finish (file_info, FALSE);
      return;
    }

  display_name = g_file_info_get_display_name (info);
  is_hidden = g_file_info_get_is_hidden (info) || g_file_info_get_is_backup (info);

  gtk_widget_push_composite_child ();
  button_data = make_directory_button (file_info->path_bar, display_name,
                                       file_info->file,
				       file_info->first_directory, is_hidden);
  gtk_widget_pop_composite_child ();
  g_object_unref (file_info->file);

  file_info->new_buttons = g_list_prepend (file_info->new_buttons, button_data);

  if (BUTTON_IS_FAKE_ROOT (button_data))
    file_info->fake_root = file_info->new_buttons;

  file_info->file = file_info->parent_file;
  file_info->first_directory = FALSE;

  if (!file_info->file)
    {
      gtk_path_bar_set_file_finish (file_info, TRUE);
      return;
    }

  file_info->parent_file = g_file_get_parent (file_info->file);

  file_info->path_bar->get_info_cancellable =
    _gtk_file_system_get_info (file_info->path_bar->file_system,
			       file_info->file,
			       "standard::display-name,standard::is-hidden,standard::is-backup",
			       gtk_path_bar_get_info_callback,
			       file_info);
}

gboolean
_gtk_path_bar_set_file (GtkPathBar         *path_bar,
			GFile              *file,
			const gboolean      keep_trail,
			GError            **error)
{
  struct SetFileInfo *info;

  g_return_val_if_fail (GTK_IS_PATH_BAR (path_bar), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  /* Check whether the new path is already present in the pathbar as buttons.
   * This could be a parent directory or a previous selected subdirectory.
   */
  if (keep_trail &&
      gtk_path_bar_check_parent_path (path_bar, file, path_bar->file_system))
    return TRUE;

  info = g_new0 (struct SetFileInfo, 1);
  info->file = g_object_ref (file);
  info->path_bar = path_bar;
  info->first_directory = TRUE;
  info->parent_file = g_file_get_parent (info->file);

  if (path_bar->get_info_cancellable)
    g_cancellable_cancel (path_bar->get_info_cancellable);

  path_bar->get_info_cancellable =
    _gtk_file_system_get_info (path_bar->file_system,
			       info->file,
			       "standard::display-name,standard::is-hidden,standard::is-backup",
			       gtk_path_bar_get_info_callback,
			       info);

  return TRUE;
}

/* FIXME: This should be a construct-only property */
void
_gtk_path_bar_set_file_system (GtkPathBar    *path_bar,
			       GtkFileSystem *file_system)
{
  const char *home;

  g_return_if_fail (GTK_IS_PATH_BAR (path_bar));

  g_assert (path_bar->file_system == NULL);

  path_bar->file_system = g_object_ref (file_system);

  home = g_get_home_dir ();
  if (home != NULL)
    {
      const gchar *desktop;

      path_bar->home_file = g_file_new_for_path (home);
      /* FIXME: Need file system backend specific way of getting the
       * Desktop path.
       */
      desktop = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
      if (desktop != NULL)
        path_bar->desktop_file = g_file_new_for_path (desktop);
      else 
        path_bar->desktop_file = NULL;
    }
  else
    {
      path_bar->home_file = NULL;
      path_bar->desktop_file = NULL;
    }
  path_bar->root_file = g_file_new_for_path ("/");
}

/**
 * _gtk_path_bar_up:
 * @path_bar: a #GtkPathBar
 * 
 * If the selected button in the pathbar is not the furthest button "up" (in the
 * root direction), act as if the user clicked on the next button up.
 **/
void
_gtk_path_bar_up (GtkPathBar *path_bar)
{
  GList *l;

  for (l = path_bar->button_list; l; l = l->next)
    {
      GtkWidget *button = BUTTON_DATA (l->data)->button;
      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
	{
	  if (l->next)
	    {
	      GtkWidget *next_button = BUTTON_DATA (l->next->data)->button;
	      button_clicked_cb (next_button, l->next->data);
	    }
	  break;
	}
    }
}

/**
 * _gtk_path_bar_down:
 * @path_bar: a #GtkPathBar
 * 
 * If the selected button in the pathbar is not the furthest button "down" (in the
 * leaf direction), act as if the user clicked on the next button down.
 **/
void
_gtk_path_bar_down (GtkPathBar *path_bar)
{
  GList *l;

  for (l = path_bar->button_list; l; l = l->next)
    {
      GtkWidget *button = BUTTON_DATA (l->data)->button;
      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
	{
	  if (l->prev)
	    {
	      GtkWidget *prev_button = BUTTON_DATA (l->prev->data)->button;
	      button_clicked_cb (prev_button, l->prev->data);
	    }
	  break;
	}
    }
}

#define __GTK_PATH_BAR_C__
#include "gtkaliasdef.c"
