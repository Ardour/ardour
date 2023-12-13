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

#include "gtkmenu.h"
#include "gtktearoffmenuitem.h"
#include "gtkintl.h"
#include "gtkalias.h"

#define ARROW_SIZE 10
#define TEAR_LENGTH 5
#define BORDER_SPACING  3

static void gtk_tearoff_menu_item_size_request (GtkWidget             *widget,
				                GtkRequisition        *requisition);
static gint gtk_tearoff_menu_item_expose     (GtkWidget             *widget,
					      GdkEventExpose        *event);
static void gtk_tearoff_menu_item_activate   (GtkMenuItem           *menu_item);
static void gtk_tearoff_menu_item_parent_set (GtkWidget             *widget,
					      GtkWidget             *previous);

G_DEFINE_TYPE (GtkTearoffMenuItem, gtk_tearoff_menu_item, GTK_TYPE_MENU_ITEM)

GtkWidget*
gtk_tearoff_menu_item_new (void)
{
  return g_object_new (GTK_TYPE_TEAROFF_MENU_ITEM, NULL);
}

static void
gtk_tearoff_menu_item_class_init (GtkTearoffMenuItemClass *klass)
{
  GtkWidgetClass *widget_class;
  GtkMenuItemClass *menu_item_class;

  widget_class = (GtkWidgetClass*) klass;
  menu_item_class = (GtkMenuItemClass*) klass;

  widget_class->expose_event = gtk_tearoff_menu_item_expose;
  widget_class->size_request = gtk_tearoff_menu_item_size_request;
  widget_class->parent_set = gtk_tearoff_menu_item_parent_set;

  menu_item_class->activate = gtk_tearoff_menu_item_activate;
}

static void
gtk_tearoff_menu_item_init (GtkTearoffMenuItem *tearoff_menu_item)
{
  tearoff_menu_item->torn_off = FALSE;
}

static void
gtk_tearoff_menu_item_size_request (GtkWidget      *widget,
				    GtkRequisition *requisition)
{
  requisition->width = (GTK_CONTAINER (widget)->border_width +
			widget->style->xthickness +
			BORDER_SPACING) * 2;
  requisition->height = (GTK_CONTAINER (widget)->border_width +
			 widget->style->ythickness) * 2;

  if (GTK_IS_MENU (widget->parent) && GTK_MENU (widget->parent)->torn_off)
    {
      requisition->height += ARROW_SIZE;
    }
  else
    {
      requisition->height += widget->style->ythickness + 4;
    }
}

static void
gtk_tearoff_menu_item_paint (GtkWidget   *widget,
			     GdkRectangle *area)
{
  GtkMenuItem *menu_item;
  GtkShadowType shadow_type;
  gint width, height;
  gint x, y;
  gint right_max;
  GtkArrowType arrow_type;
  GtkTextDirection direction;
  
  if (gtk_widget_is_drawable (widget))
    {
      menu_item = GTK_MENU_ITEM (widget);

      direction = gtk_widget_get_direction (widget);

      x = widget->allocation.x + GTK_CONTAINER (menu_item)->border_width;
      y = widget->allocation.y + GTK_CONTAINER (menu_item)->border_width;
      width = widget->allocation.width - GTK_CONTAINER (menu_item)->border_width * 2;
      height = widget->allocation.height - GTK_CONTAINER (menu_item)->border_width * 2;
      right_max = x + width;

      if (widget->state == GTK_STATE_PRELIGHT)
	{
	  gint selected_shadow_type;
	  
	  gtk_widget_style_get (widget,
				"selected-shadow-type", &selected_shadow_type,
				NULL);
	  gtk_paint_box (widget->style,
			 widget->window,
			 GTK_STATE_PRELIGHT,
			 selected_shadow_type,
			 area, widget, "menuitem",
			 x, y, width, height);
	}
      else
	gdk_window_clear_area (widget->window, area->x, area->y, area->width, area->height);

      if (GTK_IS_MENU (widget->parent) && GTK_MENU (widget->parent)->torn_off)
	{
	  gint arrow_x;

	  if (widget->state == GTK_STATE_PRELIGHT)
	    shadow_type = GTK_SHADOW_IN;
	  else
	    shadow_type = GTK_SHADOW_OUT;

	  if (menu_item->toggle_size > ARROW_SIZE)
	    {
	      if (direction == GTK_TEXT_DIR_LTR) {
		arrow_x = x + (menu_item->toggle_size - ARROW_SIZE)/2;
		arrow_type = GTK_ARROW_LEFT;
	      }
	      else {
		arrow_x = x + width - menu_item->toggle_size + (menu_item->toggle_size - ARROW_SIZE)/2; 
		arrow_type = GTK_ARROW_RIGHT;	    
	      }
	      x += menu_item->toggle_size + BORDER_SPACING;
	    }
	  else
	    {
	      if (direction == GTK_TEXT_DIR_LTR) {
		arrow_x = ARROW_SIZE / 2;
		arrow_type = GTK_ARROW_LEFT;
	      }
	      else {
		arrow_x = x + width - 2 * ARROW_SIZE + ARROW_SIZE / 2; 
		arrow_type = GTK_ARROW_RIGHT;	    
	      }
	      x += 2 * ARROW_SIZE;
	    }


	  gtk_paint_arrow (widget->style, widget->window,
			   widget->state, shadow_type,
			   NULL, widget, "tearoffmenuitem",
			   arrow_type, FALSE,
			   arrow_x, y + height / 2 - 5, 
			   ARROW_SIZE, ARROW_SIZE);
	}

      while (x < right_max)
	{
	  gint x1, x2;

	  if (direction == GTK_TEXT_DIR_LTR) {
	    x1 = x;
	    x2 = MIN (x + TEAR_LENGTH, right_max);
	  }
	  else {
	    x1 = right_max - x;
	    x2 = MAX (right_max - x - TEAR_LENGTH, 0);
	  }
	  
	  gtk_paint_hline (widget->style, widget->window, GTK_STATE_NORMAL,
			   NULL, widget, "tearoffmenuitem",
			   x1, x2, y + (height - widget->style->ythickness) / 2);
	  x += 2 * TEAR_LENGTH;
	}
    }
}

static gint
gtk_tearoff_menu_item_expose (GtkWidget      *widget,
			    GdkEventExpose *event)
{
  gtk_tearoff_menu_item_paint (widget, &event->area);

  return FALSE;
}

static void
gtk_tearoff_menu_item_activate (GtkMenuItem *menu_item)
{
  if (GTK_IS_MENU (GTK_WIDGET (menu_item)->parent))
    {
      GtkMenu *menu = GTK_MENU (GTK_WIDGET (menu_item)->parent);
      
      gtk_widget_queue_resize (GTK_WIDGET (menu_item));
      gtk_menu_set_tearoff_state (GTK_MENU (GTK_WIDGET (menu_item)->parent),
				  !menu->torn_off);
    }
}

static void
tearoff_state_changed (GtkMenu            *menu,
		       GParamSpec         *pspec,
		       gpointer            data)
{
  GtkTearoffMenuItem *tearoff_menu_item = GTK_TEAROFF_MENU_ITEM (data);

  tearoff_menu_item->torn_off = gtk_menu_get_tearoff_state (menu);
}

static void
gtk_tearoff_menu_item_parent_set (GtkWidget *widget,
				  GtkWidget *previous)
{
  GtkTearoffMenuItem *tearoff_menu_item = GTK_TEAROFF_MENU_ITEM (widget);
  GtkMenu *menu = GTK_IS_MENU (widget->parent) ? GTK_MENU (widget->parent) : NULL;

  if (previous)
    g_signal_handlers_disconnect_by_func (previous, 
					  tearoff_state_changed, 
					  tearoff_menu_item);
  
  if (menu)
    {
      tearoff_menu_item->torn_off = gtk_menu_get_tearoff_state (menu);
      g_signal_connect (menu, "notify::tearoff-state", 
			G_CALLBACK (tearoff_state_changed), 
			tearoff_menu_item);
    }  
}

#define __GTK_TEAROFF_MENU_ITEM_C__
#include "gtkaliasdef.c"
