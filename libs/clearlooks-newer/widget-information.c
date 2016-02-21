#include <gtk/gtk.h>

#include "general-support.h"
#include "widget-information.h"
#include <math.h>
#include <string.h>

/* Widget Type Lookups/Macros

   Based on/modified from functions in
   Smooth-Engine.
*/
gboolean
ge_object_is_a (GObject * object, const gchar * type_name)
{
  gboolean result = FALSE;

  if ((object))
    {
      GType tmp = g_type_from_name (type_name);

      if (tmp)
	result = g_type_check_instance_is_a ((GTypeInstance *) object, tmp);
    }

  return result;
}

gboolean
ge_is_combo_box_entry (GtkWidget * widget)
{
  gboolean result = FALSE;

  if ((widget) && (widget->parent))
    {
      if (GE_IS_COMBO_BOX_ENTRY (widget->parent))
	result = TRUE;
      else
	result = ge_is_combo_box_entry (widget->parent);
    }
  return result;
}

static gboolean
ge_combo_box_is_using_list (GtkWidget * widget)
{
  gboolean result = FALSE;

  if (GE_IS_COMBO_BOX (widget))
    {
      gboolean *tmp = NULL;

      gtk_widget_style_get (widget, "appears-as-list", &result, NULL);

      if (tmp)
	result = *tmp;
    }

  return result;
}

gboolean
ge_is_combo_box (GtkWidget * widget, gboolean as_list)
{
  gboolean result = FALSE;

  if ((widget) && (widget->parent))
    {
      if (GE_IS_COMBO_BOX (widget->parent))
        {
          if (as_list)
            result = (ge_combo_box_is_using_list(widget->parent));
          else
            result = (!ge_combo_box_is_using_list(widget->parent));
        }
      else
	result = ge_is_combo_box (widget->parent, as_list);
    }
  return result;
}

gboolean
ge_is_combo (GtkWidget * widget)
{
  gboolean result = FALSE;

  if ((widget) && (widget->parent))
    {
      if (GE_IS_COMBO (widget->parent))
	result = TRUE;
      else
	result = ge_is_combo (widget->parent);
    }
  return result;
}

gboolean
ge_is_in_combo_box (GtkWidget * widget)
{
  return ((ge_is_combo (widget) || ge_is_combo_box (widget, TRUE) || ge_is_combo_box_entry (widget)));
}

gboolean
ge_is_toolbar_item (GtkWidget * widget)
{
  gboolean result = FALSE;

  if ((widget) && (widget->parent)) {
    if ((GE_IS_BONOBO_TOOLBAR (widget->parent))
	|| (GE_IS_BONOBO_DOCK_ITEM (widget->parent))
	|| (GE_IS_EGG_TOOLBAR (widget->parent))
	|| (GE_IS_TOOLBAR (widget->parent))
	|| (GE_IS_HANDLE_BOX (widget->parent)))
      result = TRUE;
    else
      result = ge_is_toolbar_item (widget->parent);
  }
  return result;
}

gboolean
ge_is_panel_widget_item (GtkWidget * widget)
{
  gboolean result = FALSE;

  if ((widget) && (widget->parent))
    {
      if (GE_IS_PANEL_WIDGET (widget->parent))
	result = TRUE;
      else
	result = ge_is_panel_widget_item (widget->parent);
    }
  return result;
}

gboolean
ge_is_bonobo_dock_item (GtkWidget * widget)
{
  gboolean result = FALSE;

  if ((widget))
    {
      if (GE_IS_BONOBO_DOCK_ITEM(widget) || GE_IS_BONOBO_DOCK_ITEM (widget->parent))
	result = TRUE;
      else if (GE_IS_BOX(widget) || GE_IS_BOX(widget->parent))
        {
          GtkContainer *box = GE_IS_BOX(widget)?GTK_CONTAINER(widget):GTK_CONTAINER(widget->parent);
          GList *children = NULL, *child = NULL;

          children = gtk_container_get_children(box);

          for (child = g_list_first(children); child; child = g_list_next(child))
            {
	      if (GE_IS_BONOBO_DOCK_ITEM_GRIP(child->data))
	        {
	          result = TRUE;
	          child = NULL;
	        }
            }

          if (children)
  	    g_list_free(children);
	}
    }
  return result;
}

static GtkWidget *
ge_find_combo_box_entry_widget (GtkWidget * widget)
{
  GtkWidget *result = NULL;

  if (widget)
    {
      if (GE_IS_COMBO_BOX_ENTRY (widget))
	result = widget;
      else
	result = ge_find_combo_box_entry_widget (widget->parent);
    }

  return result;
}

static GtkWidget *
ge_find_combo_box_widget (GtkWidget * widget, gboolean as_list)
{
  GtkWidget *result = NULL;

  if (widget)
    {
      if (GE_IS_COMBO_BOX (widget))
        {
          if (as_list)
            result = (ge_combo_box_is_using_list(widget))?widget:NULL;
          else
            result = (!ge_combo_box_is_using_list(widget))?widget:NULL;
        }
      else
	result = ge_find_combo_box_widget (widget->parent, as_list);
    }
  return result;
}

static GtkWidget *
ge_find_combo_widget (GtkWidget * widget)
{
  GtkWidget *result = NULL;

  if (widget)
    {
      if (GE_IS_COMBO (widget))
	result = widget;
      else
	result = ge_find_combo_widget(widget->parent);
    }
  return result;
}

GtkWidget*
ge_find_combo_box_widget_parent (GtkWidget * widget)
{
   GtkWidget *result = NULL;

   if (!result)
     result = ge_find_combo_widget(widget);

   if (!result)
     result = ge_find_combo_box_widget(widget, TRUE);

   if (!result)
     result = ge_find_combo_box_entry_widget(widget);

  return result;
}

/***********************************************
 * option_menu_get_props -
 *
 *   Find Option Menu Size and Spacing
 *
 *   Taken from Smooth
 ***********************************************/
void
ge_option_menu_get_props (GtkWidget * widget,
		       GtkRequisition * indicator_size,
		       GtkBorder * indicator_spacing)
{
  GtkRequisition default_size = { 9, 5 };
  GtkBorder default_spacing = { 7, 5, 2, 2 };
  GtkRequisition *tmp_size = NULL;
  GtkBorder *tmp_spacing = NULL;

  if ((widget) && GE_IS_OPTION_MENU(widget))
    gtk_widget_style_get (widget,
			  "indicator_size", &tmp_size,
			  "indicator_spacing", &tmp_spacing, NULL);

  if (tmp_size)
    {
      *indicator_size = *tmp_size;
      gtk_requisition_free (tmp_size);
    }
  else
    *indicator_size = default_size;

  if (tmp_spacing)
    {
      *indicator_spacing = *tmp_spacing;
      gtk_border_free (tmp_spacing);
    }
  else
    *indicator_spacing = default_spacing;
}

void
ge_button_get_default_border (GtkWidget *widget,
                              GtkBorder *border)
{
	GtkBorder default_border = {1, 1, 1, 1};
	GtkBorder *tmp_border = NULL;

	if (widget && GE_IS_BUTTON (widget))
		gtk_widget_style_get (widget, "default-border", &tmp_border, NULL);

	if (tmp_border)
	{
		*border = *tmp_border;
		gtk_border_free (tmp_border);
	}
	else
	{
		*border = default_border;
	}
}


gboolean
ge_widget_is_ltr (GtkWidget *widget)
{
	GtkTextDirection dir = GTK_TEXT_DIR_NONE;

	if (GE_IS_WIDGET (widget))
		dir = gtk_widget_get_direction (widget);

	if (dir == GTK_TEXT_DIR_NONE)
		dir = gtk_widget_get_default_direction ();

	if (dir == GTK_TEXT_DIR_RTL)
		return FALSE;
	else
		return TRUE;
}
