/* 
 * GTK - The GIMP Toolkit
 * Copyright (C) 1999  Red Hat, Inc.
 * Copyright (C) 2002  Anders Carlsson <andersca@gnu.org>
 * Copyright (C) 2003  Matthias Clasen <mclasen@redhat.com>
 * Copyright (C) 2005  Carlos Garnacho Parro <carlosg@gnome.org>
 *
 * All rights reserved.
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

/**
 * SECTION:gtkassistant
 * @Short_description: A widget used to guide users through multi-step operations
 * @Title: GtkAssistant
 *
 * A #GtkAssistant is a widget used to represent a generally complex
 * operation splitted in several steps, guiding the user through its pages
 * and controlling the page flow to collect the necessary data.
 *
 * <refsect2 id="GtkAssistant-BUILDER-UI">
 * <title>GtkAssistant as GtkBuildable</title>
 * <para>
 * The GtkAssistant implementation of the GtkBuildable interface exposes the
 * @action_area as internal children with the name "action_area".
 *
 * To add pages to an assistant in GtkBuilder, simply add it as a
 * &lt;child&gt; to the GtkAssistant object, and set its child properties
 * as necessary.
 * </para>
 * </refsect2>
 */

#include "config.h"

#include <atk/atk.h>

#include "gtkassistant.h"

#include "gtkaccessible.h"
#include "gtkbutton.h"
#include "gtkhbox.h"
#include "gtkhbbox.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtksizegroup.h"
#include "gtkstock.h"

#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkbuildable.h"

#include "gtkalias.h"

#define GTK_ASSISTANT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_ASSISTANT, GtkAssistantPrivate))

#define HEADER_SPACING 12
#define ACTION_AREA_SPACING 12

typedef struct _GtkAssistantPage GtkAssistantPage;

struct _GtkAssistantPage
{
  GtkWidget *page;
  GtkAssistantPageType type;
  guint      complete : 1;
  guint      complete_set : 1;

  GtkWidget *title;
  GdkPixbuf *header_image;
  GdkPixbuf *sidebar_image;
};

struct _GtkAssistantPrivate
{
  GtkWidget *header_image;
  GtkWidget *sidebar_image;

  GtkWidget *action_area;

  GList     *pages;

  GtkAssistantPage *current_page;

  GSList    *visited_pages;

  GtkSizeGroup *size_group;

  GtkAssistantPageFunc forward_function;
  gpointer forward_function_data;
  GDestroyNotify forward_data_destroy;

  guint committed : 1;
};

static void     gtk_assistant_class_init         (GtkAssistantClass *class);
static void     gtk_assistant_init               (GtkAssistant      *assistant);
static void     gtk_assistant_destroy            (GtkObject         *object);
static void     gtk_assistant_style_set          (GtkWidget         *widget,
						  GtkStyle          *old_style);
static void     gtk_assistant_size_request       (GtkWidget         *widget,
						  GtkRequisition    *requisition);
static void     gtk_assistant_size_allocate      (GtkWidget         *widget,
						  GtkAllocation     *allocation);
static void     gtk_assistant_map                (GtkWidget         *widget);
static void     gtk_assistant_unmap              (GtkWidget         *widget);
static gboolean gtk_assistant_delete_event       (GtkWidget         *widget,
						  GdkEventAny       *event);
static gboolean gtk_assistant_expose             (GtkWidget         *widget,
						  GdkEventExpose    *event);
static gboolean gtk_assistant_focus              (GtkWidget         *widget,
						  GtkDirectionType   direction);
static void     gtk_assistant_add                (GtkContainer      *container,
						  GtkWidget         *page);
static void     gtk_assistant_remove             (GtkContainer      *container,
						  GtkWidget         *page);
static void     gtk_assistant_forall             (GtkContainer      *container,
						  gboolean           include_internals,
						  GtkCallback        callback,
						  gpointer           callback_data);
static void     gtk_assistant_set_child_property (GtkContainer      *container,
						  GtkWidget         *child,
						  guint              property_id,
						  const GValue      *value,
						  GParamSpec        *pspec);
static void     gtk_assistant_get_child_property (GtkContainer      *container,
						  GtkWidget         *child,
						  guint              property_id,
						  GValue            *value,
						  GParamSpec        *pspec);

static AtkObject *gtk_assistant_get_accessible   (GtkWidget         *widget);

static void       gtk_assistant_buildable_interface_init     (GtkBuildableIface *iface);
static GObject   *gtk_assistant_buildable_get_internal_child (GtkBuildable  *buildable,
                                                              GtkBuilder    *builder,
                                                              const gchar   *childname);
static gboolean   gtk_assistant_buildable_custom_tag_start   (GtkBuildable  *buildable,
                                                              GtkBuilder    *builder,
                                                              GObject       *child,
                                                              const gchar   *tagname,
                                                              GMarkupParser *parser,
                                                              gpointer      *data);
static void       gtk_assistant_buildable_custom_finished    (GtkBuildable  *buildable,
                                                              GtkBuilder    *builder,
                                                              GObject       *child,
                                                              const gchar   *tagname,
                                                              gpointer       user_data);

static GList*     find_page                                  (GtkAssistant  *assistant,
                                                              GtkWidget     *page);

enum
{
  CHILD_PROP_0,
  CHILD_PROP_PAGE_TYPE,
  CHILD_PROP_PAGE_TITLE,
  CHILD_PROP_PAGE_HEADER_IMAGE,
  CHILD_PROP_PAGE_SIDEBAR_IMAGE,
  CHILD_PROP_PAGE_COMPLETE
};

enum
{
  CANCEL,
  PREPARE,
  APPLY,
  CLOSE,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_CODE (GtkAssistant, gtk_assistant, GTK_TYPE_WINDOW,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_assistant_buildable_interface_init))


static void
gtk_assistant_class_init (GtkAssistantClass *class)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class   = (GObjectClass *) class;
  object_class    = (GtkObjectClass *) class;
  widget_class    = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;

  object_class->destroy = gtk_assistant_destroy;

  widget_class->style_set = gtk_assistant_style_set;
  widget_class->size_request = gtk_assistant_size_request;
  widget_class->size_allocate = gtk_assistant_size_allocate;
  widget_class->map = gtk_assistant_map;
  widget_class->unmap = gtk_assistant_unmap;
  widget_class->delete_event = gtk_assistant_delete_event;
  widget_class->expose_event = gtk_assistant_expose;
  widget_class->focus = gtk_assistant_focus;
  widget_class->get_accessible = gtk_assistant_get_accessible;

  container_class->add = gtk_assistant_add;
  container_class->remove = gtk_assistant_remove;
  container_class->forall = gtk_assistant_forall;
  container_class->set_child_property = gtk_assistant_set_child_property;
  container_class->get_child_property = gtk_assistant_get_child_property;

  /**
   * GtkAssistant::cancel:
   * @assistant: the #GtkAssistant
   *
   * The ::cancel signal is emitted when then the cancel button is clicked.
   *
   * Since: 2.10
   */
  signals[CANCEL] =
    g_signal_new (I_("cancel"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkAssistantClass, cancel),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
    
  /**
   * GtkAssistant::prepare:
   * @assistant: the #GtkAssistant
   * @page: the current page
   *
   * The ::prepare signal is emitted when a new page is set as the assistant's
   * current page, before making the new page visible. A handler for this signal
   * can do any preparation which are necessary before showing @page.
   *
   * Since: 2.10
   */
  signals[PREPARE] =
    g_signal_new (I_("prepare"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkAssistantClass, prepare),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, GTK_TYPE_WIDGET);

  /**
   * GtkAssistant::apply:
   * @assistant: the #GtkAssistant
   *
   * The ::apply signal is emitted when the apply button is clicked. The default
   * behavior of the #GtkAssistant is to switch to the page after the current
   * page, unless the current page is the last one.
   *
   * A handler for the ::apply signal should carry out the actions for which
   * the wizard has collected data. If the action takes a long time to complete,
   * you might consider putting a page of type %GTK_ASSISTANT_PAGE_PROGRESS
   * after the confirmation page and handle this operation within the
   * #GtkAssistant::prepare signal of the progress page.
   *
   * Since: 2.10
   */
  signals[APPLY] =
    g_signal_new (I_("apply"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkAssistantClass, apply),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkAssistant::close:
   * @assistant: the #GtkAssistant
   *
   * The ::close signal is emitted either when the close button of
   * a summary page is clicked, or when the apply button in the last
   * page in the flow (of type %GTK_ASSISTANT_PAGE_CONFIRM) is clicked.
   *
   * Since: 2.10
   */
  signals[CLOSE] =
    g_signal_new (I_("close"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkAssistantClass, close),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("header-padding",
							     P_("Header Padding"),
							     P_("Number of pixels around the header."),
							     0,
							     G_MAXINT,
							     6,
							     GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("content-padding",
							     P_("Content Padding"),
							     P_("Number of pixels around the content pages."),
							     0,
							     G_MAXINT,
							     1,
							     GTK_PARAM_READABLE));

  /**
   * GtkAssistant:page-type:
   *
   * The type of the assistant page.
   *
   * Since: 2.10
   */
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_PAGE_TYPE,
					      g_param_spec_enum ("page-type", 
								 P_("Page type"),
								 P_("The type of the assistant page"),
								 GTK_TYPE_ASSISTANT_PAGE_TYPE,
								 GTK_ASSISTANT_PAGE_CONTENT,
								 GTK_PARAM_READWRITE));

  /**
   * GtkAssistant:title:
   *
   * The title that is displayed in the page header.
   *
   * If title and header-image are both %NULL, no header is displayed.
   *
   * Since: 2.10
   */
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_PAGE_TITLE,
					      g_param_spec_string ("title", 
								   P_("Page title"),
								   P_("The title of the assistant page"),
								   NULL,
								   GTK_PARAM_READWRITE));

  /**
   * GtkAssistant:header-image:
   *
   * The image that is displayed next to the title in the page header.
   *
   * If title and header-image are both %NULL, no header is displayed.
   *
   * Since: 2.10
   */
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_PAGE_HEADER_IMAGE,
					      g_param_spec_object ("header-image", 
								   P_("Header image"),
								   P_("Header image for the assistant page"),
								   GDK_TYPE_PIXBUF,
								   GTK_PARAM_READWRITE));

  /**
   * GtkAssistant:header-image:
   *
   * The image that is displayed next to the page.
   *
   * Set this to %NULL to make the sidebar disappear.
   *
   * Since: 2.10
   */
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_PAGE_SIDEBAR_IMAGE,
					      g_param_spec_object ("sidebar-image", 
								   P_("Sidebar image"),
								   P_("Sidebar image for the assistant page"),
								   GDK_TYPE_PIXBUF,
								   GTK_PARAM_READWRITE));
  /**
   * GtkAssistant:complete:
   *
   * Setting the "complete" child property to %TRUE marks a page as complete
   * (i.e.: all the required fields are filled out). GTK+ uses this information
   * to control the sensitivity of the navigation buttons.
   *
   * Since: 2.10
   **/
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_PAGE_COMPLETE,
					      g_param_spec_boolean ("complete", 
								    P_("Page complete"),
								    P_("Whether all required fields on the page have been filled out"),
								    FALSE,
								    G_PARAM_READWRITE));

  g_type_class_add_private (gobject_class, sizeof (GtkAssistantPrivate));
}

static gint
default_forward_function (gint current_page, gpointer data)
{
  GtkAssistant *assistant;
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  GList *page_node;

  assistant = GTK_ASSISTANT (data);
  priv = assistant->priv;

  page_node = g_list_nth (priv->pages, ++current_page);

  if (!page_node)
    return -1;

  page_info = (GtkAssistantPage *) page_node->data;

  while (page_node && !gtk_widget_get_visible (page_info->page))
    {
      page_node = page_node->next;
      current_page++;

      if (page_node)
	page_info = (GtkAssistantPage *) page_node->data;
    }

  return current_page;
}

static void
compute_last_button_state (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;
  GtkAssistantPage *page_info, *current_page_info;
  gint count, page_num, n_pages;

  count = 0;
  page_num = gtk_assistant_get_current_page (assistant);
  n_pages  = gtk_assistant_get_n_pages (assistant);
  current_page_info = page_info = g_list_nth_data (priv->pages, page_num);

  while (page_num >= 0 && page_num < n_pages &&
	 page_info->type == GTK_ASSISTANT_PAGE_CONTENT &&
	 (count == 0 || page_info->complete) &&
	 count < n_pages)
    {
      page_num = (priv->forward_function) (page_num, priv->forward_function_data);
      page_info = g_list_nth_data (priv->pages, page_num);

      count++;
    }

  /* make the last button visible if we can skip multiple
   * pages and end on a confirmation or summary page
   */
  if (count > 1 && page_info &&
      (page_info->type == GTK_ASSISTANT_PAGE_CONFIRM ||
       page_info->type == GTK_ASSISTANT_PAGE_SUMMARY))
    {
      gtk_widget_show (assistant->last);
      gtk_widget_set_sensitive (assistant->last,
				current_page_info->complete);
    }
  else
    gtk_widget_hide (assistant->last);
}

static void
compute_progress_state (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;
  gint page_num, n_pages;

  n_pages = gtk_assistant_get_n_pages (assistant);
  page_num = gtk_assistant_get_current_page (assistant);

  page_num = (priv->forward_function) (page_num, priv->forward_function_data);

  if (page_num >= 0 && page_num < n_pages)
    gtk_widget_show (assistant->forward);
  else
    gtk_widget_hide (assistant->forward);
}

static void
set_assistant_header_image (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;

  gtk_image_set_from_pixbuf (GTK_IMAGE (priv->header_image),
			     priv->current_page->header_image);
}

static void
set_assistant_sidebar_image (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;

  gtk_image_set_from_pixbuf (GTK_IMAGE (priv->sidebar_image),
			     priv->current_page->sidebar_image);

  if (priv->current_page->sidebar_image)
    gtk_widget_show (priv->sidebar_image);
  else
    gtk_widget_hide (priv->sidebar_image);
}

static void
set_assistant_buttons_state (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;

  if (!priv->current_page)
    return;
  
  switch (priv->current_page->type)
    {
    case GTK_ASSISTANT_PAGE_INTRO:
      gtk_widget_set_sensitive (assistant->cancel, TRUE);
      gtk_widget_set_sensitive (assistant->forward, priv->current_page->complete);
      gtk_widget_grab_default (assistant->forward);
      gtk_widget_show (assistant->forward);
      gtk_widget_hide (assistant->back);
      gtk_widget_hide (assistant->apply);
      gtk_widget_hide (assistant->close);
      compute_last_button_state (assistant);
      break;
    case GTK_ASSISTANT_PAGE_CONFIRM:
      gtk_widget_set_sensitive (assistant->cancel, TRUE);
      gtk_widget_set_sensitive (assistant->back, TRUE);
      gtk_widget_set_sensitive (assistant->apply, priv->current_page->complete);
      gtk_widget_grab_default (assistant->apply);
      gtk_widget_show (assistant->back);
      gtk_widget_show (assistant->apply);
      gtk_widget_hide (assistant->forward);
      gtk_widget_hide (assistant->close);
      gtk_widget_hide (assistant->last);
      break;
    case GTK_ASSISTANT_PAGE_CONTENT:
      gtk_widget_set_sensitive (assistant->cancel, TRUE);
      gtk_widget_set_sensitive (assistant->back, TRUE);
      gtk_widget_set_sensitive (assistant->forward, priv->current_page->complete);
      gtk_widget_grab_default (assistant->forward);
      gtk_widget_show (assistant->back);
      gtk_widget_show (assistant->forward);
      gtk_widget_hide (assistant->apply);
      gtk_widget_hide (assistant->close);
      compute_last_button_state (assistant);
      break;
    case GTK_ASSISTANT_PAGE_SUMMARY:
      gtk_widget_set_sensitive (assistant->close, priv->current_page->complete);
      gtk_widget_grab_default (assistant->close);
      gtk_widget_show (assistant->close);
      gtk_widget_hide (assistant->back);
      gtk_widget_hide (assistant->forward);
      gtk_widget_hide (assistant->apply);
      gtk_widget_hide (assistant->last);
      break;
    case GTK_ASSISTANT_PAGE_PROGRESS:
      gtk_widget_set_sensitive (assistant->cancel, priv->current_page->complete);
      gtk_widget_set_sensitive (assistant->back, priv->current_page->complete);
      gtk_widget_set_sensitive (assistant->forward, priv->current_page->complete);
      gtk_widget_grab_default (assistant->forward);
      gtk_widget_show (assistant->back);
      gtk_widget_hide (assistant->apply);
      gtk_widget_hide (assistant->close);
      gtk_widget_hide (assistant->last);
      compute_progress_state (assistant);
      break;
    default:
      g_assert_not_reached ();
    }

  if (priv->committed)
    gtk_widget_hide (assistant->cancel);
  else if (priv->current_page->type == GTK_ASSISTANT_PAGE_SUMMARY)
    gtk_widget_hide (assistant->cancel);
  else
    gtk_widget_show (assistant->cancel);

  /* this is quite general, we don't want to
   * go back if it's the first page */
  if (!priv->visited_pages)
    gtk_widget_hide (assistant->back);
}

static void
set_current_page (GtkAssistant     *assistant,
		  GtkAssistantPage *page)
{
  GtkAssistantPrivate *priv = assistant->priv;
  GtkAssistantPage *old_page;

  if (priv->current_page &&
      gtk_widget_is_drawable (priv->current_page->page))
    old_page = priv->current_page;
  else
    old_page = NULL;

  priv->current_page = page;

  set_assistant_buttons_state (assistant);
  set_assistant_header_image (assistant);
  set_assistant_sidebar_image (assistant);

  g_signal_emit (assistant, signals [PREPARE], 0, priv->current_page->page);

  if (gtk_widget_get_visible (priv->current_page->page) && gtk_widget_get_mapped (GTK_WIDGET (assistant)))
    {
      gtk_widget_set_child_visible (priv->current_page->page, TRUE);
      gtk_widget_map (priv->current_page->page);
      gtk_widget_map (priv->current_page->title);
    }
  
  if (old_page && gtk_widget_get_mapped (old_page->page))
    {
      gtk_widget_set_child_visible (old_page->page, FALSE);
      gtk_widget_unmap (old_page->page);
      gtk_widget_unmap (old_page->title);
    }

  if (!gtk_widget_child_focus (priv->current_page->page, GTK_DIR_TAB_FORWARD))
    {
      GtkWidget *button[6];
      gint i;

      /* find the best button to focus */
      button[0] = assistant->apply;
      button[1] = assistant->close;
      button[2] = assistant->forward;
      button[3] = assistant->back;
      button[4] = assistant->cancel;
      button[5] = assistant->last;
      for (i = 0; i < 6; i++)
        {
          if (gtk_widget_get_visible (button[i]) && gtk_widget_get_sensitive (button[i]))
            {
              gtk_widget_grab_focus (button[i]);
              break;
            }
        }
    }

  gtk_widget_queue_resize (GTK_WIDGET (assistant));
}

static gint
compute_next_step (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;
  GtkAssistantPage *page_info;
  gint current_page, n_pages, next_page;

  current_page = gtk_assistant_get_current_page (assistant);
  page_info = priv->current_page;
  n_pages = gtk_assistant_get_n_pages (assistant);

  next_page = (priv->forward_function) (current_page,
					priv->forward_function_data);

  if (next_page >= 0 && next_page < n_pages)
    {
      priv->visited_pages = g_slist_prepend (priv->visited_pages, page_info);
      set_current_page (assistant, g_list_nth_data (priv->pages, next_page));

      return TRUE;
    }

  return FALSE;
}

static void
on_assistant_close (GtkWidget    *widget,
                    GtkAssistant *assistant)
{
  g_signal_emit (assistant, signals [CLOSE], 0, NULL);
}

static void
on_assistant_apply (GtkWidget    *widget,
                    GtkAssistant *assistant)
{
  gboolean success;

  g_signal_emit (assistant, signals [APPLY], 0);

  success = compute_next_step (assistant);

  /* if the assistant hasn't switched to another page, just emit
   * the CLOSE signal, it't the last page in the assistant flow
   */
  if (!success)
    g_signal_emit (assistant, signals [CLOSE], 0);
}

static void
on_assistant_forward (GtkWidget    *widget,
                      GtkAssistant *assistant)
{
  if (!compute_next_step (assistant))
    g_critical ("Page flow is broken, you may want to end it with a page of "
		"type GTK_ASSISTANT_PAGE_CONFIRM or GTK_ASSISTANT_PAGE_SUMMARY");
}

static void
on_assistant_back (GtkWidget    *widget,
                   GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;
  GtkAssistantPage *page_info;
  GSList *page_node;

  /* skip the progress pages when going back */
  do
    {
      page_node = priv->visited_pages;

      g_return_if_fail (page_node != NULL);

      priv->visited_pages = priv->visited_pages->next;
      page_info = (GtkAssistantPage *) page_node->data;
      g_slist_free_1 (page_node);
    }
  while (page_info->type == GTK_ASSISTANT_PAGE_PROGRESS ||
	 !gtk_widget_get_visible (page_info->page));

  set_current_page (assistant, page_info);
}

static void
on_assistant_cancel (GtkWidget    *widget,
                     GtkAssistant *assistant)
{
  g_signal_emit (assistant, signals [CANCEL], 0, NULL);
}

static void
on_assistant_last (GtkWidget    *widget,
                   GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;

  while (priv->current_page->type == GTK_ASSISTANT_PAGE_CONTENT &&
	 priv->current_page->complete)
    compute_next_step (assistant);
}

static gboolean
alternative_button_order (GtkAssistant *assistant)
{
  GtkSettings *settings;
  GdkScreen *screen;
  gboolean result;

  screen   = gtk_widget_get_screen (GTK_WIDGET (assistant));
  settings = gtk_settings_get_for_screen (screen);

  g_object_get (settings,
		"gtk-alternative-button-order", &result,
		NULL);
  return result;
}

static void
gtk_assistant_init (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv;

  priv = assistant->priv = GTK_ASSISTANT_GET_PRIVATE (assistant);

  gtk_container_set_reallocate_redraws (GTK_CONTAINER (assistant), TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (assistant), 12);

  gtk_widget_push_composite_child ();

  /* Header */
  priv->header_image = gtk_image_new ();
  gtk_misc_set_alignment (GTK_MISC (priv->header_image), 1., 0.5);
  gtk_widget_set_parent (priv->header_image, GTK_WIDGET (assistant));
  gtk_widget_show (priv->header_image);

  /* Sidebar */
  priv->sidebar_image = gtk_image_new ();
  gtk_misc_set_alignment (GTK_MISC (priv->sidebar_image), 0., 0.);
  gtk_widget_set_parent (priv->sidebar_image, GTK_WIDGET (assistant));
  gtk_widget_show (priv->sidebar_image);

  /* Action area  */
  priv->action_area  = gtk_hbox_new (FALSE, 6);
  
  assistant->close   = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
  assistant->apply   = gtk_button_new_from_stock (GTK_STOCK_APPLY);
  assistant->forward = gtk_button_new_from_stock (GTK_STOCK_GO_FORWARD);
  assistant->back    = gtk_button_new_from_stock (GTK_STOCK_GO_BACK);
  assistant->cancel  = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
  assistant->last    = gtk_button_new_from_stock (GTK_STOCK_GOTO_LAST);
  gtk_widget_set_can_default (assistant->close, TRUE);
  gtk_widget_set_can_default (assistant->apply, TRUE);
  gtk_widget_set_can_default (assistant->forward, TRUE);

  priv->size_group   = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  gtk_size_group_add_widget (priv->size_group, assistant->close);
  gtk_size_group_add_widget (priv->size_group, assistant->apply);
  gtk_size_group_add_widget (priv->size_group, assistant->forward);
  gtk_size_group_add_widget (priv->size_group, assistant->back);
  gtk_size_group_add_widget (priv->size_group, assistant->cancel);
  gtk_size_group_add_widget (priv->size_group, assistant->last);

  if (!alternative_button_order (assistant))
    {
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->apply, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->forward, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->back, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->last, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->cancel, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->close, FALSE, FALSE, 0);
    }
  else
    {
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->close, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->cancel, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->apply, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->forward, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->back, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->last, FALSE, FALSE, 0);
    }

  gtk_widget_set_parent (priv->action_area, GTK_WIDGET (assistant));
  gtk_widget_show (assistant->forward);
  gtk_widget_show (assistant->back);
  gtk_widget_show (assistant->cancel);
  gtk_widget_show (priv->action_area);

  gtk_widget_pop_composite_child ();

  priv->pages = NULL;
  priv->current_page = NULL;
  priv->visited_pages = NULL;

  priv->forward_function = default_forward_function;
  priv->forward_function_data = assistant;
  priv->forward_data_destroy = NULL;

  g_signal_connect (G_OBJECT (assistant->close), "clicked",
		    G_CALLBACK (on_assistant_close), assistant);
  g_signal_connect (G_OBJECT (assistant->apply), "clicked",
		    G_CALLBACK (on_assistant_apply), assistant);
  g_signal_connect (G_OBJECT (assistant->forward), "clicked",
		    G_CALLBACK (on_assistant_forward), assistant);
  g_signal_connect (G_OBJECT (assistant->back), "clicked",
		    G_CALLBACK (on_assistant_back), assistant);
  g_signal_connect (G_OBJECT (assistant->cancel), "clicked",
		    G_CALLBACK (on_assistant_cancel), assistant);
  g_signal_connect (G_OBJECT (assistant->last), "clicked",
		    G_CALLBACK (on_assistant_last), assistant);
}

static void
gtk_assistant_set_child_property (GtkContainer    *container,
				  GtkWidget       *child,
				  guint            property_id,
				  const GValue    *value,
				  GParamSpec      *pspec)
{
  switch (property_id)
    {
    case CHILD_PROP_PAGE_TYPE:
      gtk_assistant_set_page_type (GTK_ASSISTANT (container), child,
				   g_value_get_enum (value));
      break;
    case CHILD_PROP_PAGE_TITLE:
      gtk_assistant_set_page_title (GTK_ASSISTANT (container), child,
				    g_value_get_string (value));
      break;
    case CHILD_PROP_PAGE_HEADER_IMAGE:
      gtk_assistant_set_page_header_image (GTK_ASSISTANT (container), child,
					   g_value_get_object (value));
      break;
    case CHILD_PROP_PAGE_SIDEBAR_IMAGE:
      gtk_assistant_set_page_side_image (GTK_ASSISTANT (container), child,
					 g_value_get_object (value));
      break;
    case CHILD_PROP_PAGE_COMPLETE:
      gtk_assistant_set_page_complete (GTK_ASSISTANT (container), child,
				       g_value_get_boolean (value));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_assistant_get_child_property (GtkContainer *container,
				  GtkWidget    *child,
				  guint         property_id,
				  GValue       *value,
				  GParamSpec   *pspec)
{
  switch (property_id)
    {
    case CHILD_PROP_PAGE_TYPE:
      g_value_set_enum (value,
			gtk_assistant_get_page_type (GTK_ASSISTANT (container), child));
      break;
    case CHILD_PROP_PAGE_TITLE:
      g_value_set_string (value,
			  gtk_assistant_get_page_title (GTK_ASSISTANT (container), child));
      break;
    case CHILD_PROP_PAGE_HEADER_IMAGE:
      g_value_set_object (value,
			  gtk_assistant_get_page_header_image (GTK_ASSISTANT (container), child));
      break;
    case CHILD_PROP_PAGE_SIDEBAR_IMAGE:
      g_value_set_object (value,
			  gtk_assistant_get_page_side_image (GTK_ASSISTANT (container), child));
      break;
    case CHILD_PROP_PAGE_COMPLETE:
      g_value_set_boolean (value,
			   gtk_assistant_get_page_complete (GTK_ASSISTANT (container), child));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
on_page_notify_visibility (GtkWidget  *widget,
			   GParamSpec *arg,
			   gpointer    data)
{
  GtkAssistant *assistant = GTK_ASSISTANT (data);

  /* update buttons state, flow may have changed */
  if (gtk_widget_get_mapped (GTK_WIDGET (assistant)))
    set_assistant_buttons_state (assistant);
}

static void
remove_page (GtkAssistant *assistant, 
	     GList        *element)
{
  GtkAssistantPrivate *priv = assistant->priv;
  GtkAssistantPage *page_info;
  GList *page_node;

  page_info = element->data;

  /* If this is the current page, we need to switch away. */
  if (page_info == priv->current_page)
    {
      if (!compute_next_step (assistant))
        {
  	  /* The best we can do at this point is probably to pick the first
	   * visible page.
	   */
	  page_node = priv->pages;

	  while (page_node && !gtk_widget_get_visible (((GtkAssistantPage *) page_node->data)->page))
	    page_node = page_node->next;

          if (page_node == element)
            page_node = page_node->next;

	  if (page_node)
	    priv->current_page = page_node->data;
	  else
	    priv->current_page = NULL;
        }
    }

  priv->pages = g_list_remove_link (priv->pages, element);
  priv->visited_pages = g_slist_remove_all (priv->visited_pages, page_info);

  g_signal_handlers_disconnect_by_func (page_info->page, on_page_notify_visibility, assistant);
  gtk_widget_unparent (page_info->page);

  if (page_info->header_image)
    g_object_unref (page_info->header_image);

  if (page_info->sidebar_image)
    g_object_unref (page_info->sidebar_image);

  gtk_widget_destroy (page_info->title);
  g_slice_free (GtkAssistantPage, page_info);
  g_list_free_1 (element);
}

static void
gtk_assistant_destroy (GtkObject *object)
{
  GtkAssistant *assistant = GTK_ASSISTANT (object);
  GtkAssistantPrivate *priv = assistant->priv;

  if (priv->header_image)
    {
      gtk_widget_destroy (priv->header_image);
      priv->header_image = NULL;
    }

  if (priv->sidebar_image)
    {
      gtk_widget_destroy (priv->sidebar_image);
      priv->sidebar_image = NULL;
    }

  if (priv->action_area)
    {
      gtk_widget_destroy (priv->action_area);
      priv->action_area = NULL;
    }

  if (priv->size_group)
    {
      g_object_unref (priv->size_group);
      priv->size_group = NULL;
    }

  if (priv->forward_function)
    {
      if (priv->forward_function_data &&
	  priv->forward_data_destroy)
	priv->forward_data_destroy (priv->forward_function_data);

      priv->forward_function = NULL;
      priv->forward_function_data = NULL;
      priv->forward_data_destroy = NULL;
    }

  if (priv->visited_pages)
    {
      g_slist_free (priv->visited_pages);
      priv->visited_pages = NULL;
    }

  /* We set current to NULL so that the remove code doesn't try
   * to do anything funny */
  priv->current_page = NULL;

  while (priv->pages)
    remove_page (GTK_ASSISTANT (object), priv->pages);
      
  GTK_OBJECT_CLASS (gtk_assistant_parent_class)->destroy (object);
}

static GList*
find_page (GtkAssistant  *assistant,
	   GtkWidget     *page)
{
  GtkAssistantPrivate *priv = assistant->priv;
  GList *child = priv->pages;
  
  while (child)
    {
      GtkAssistantPage *page_info = child->data;
      if (page_info->page == page)
	return child;

      child = child->next;
    }
  
  return NULL;
}

static void
set_title_colors (GtkWidget *assistant,
		  GtkWidget *title_label)
{
  GtkStyle *style;

  gtk_widget_ensure_style (assistant);
  style = gtk_widget_get_style (assistant);

  /* change colors schema, for making the header text visible */
  gtk_widget_modify_bg (title_label, GTK_STATE_NORMAL, &style->bg[GTK_STATE_SELECTED]);
  gtk_widget_modify_fg (title_label, GTK_STATE_NORMAL, &style->fg[GTK_STATE_SELECTED]);
}

static void
set_title_font (GtkWidget *assistant,
		GtkWidget *title_label)
{
  PangoFontDescription *desc;
  gint size;

  desc = pango_font_description_new ();
  size = pango_font_description_get_size (assistant->style->font_desc);

  pango_font_description_set_weight (desc, PANGO_WEIGHT_ULTRABOLD);
  pango_font_description_set_size   (desc, size * PANGO_SCALE_XX_LARGE);

  gtk_widget_modify_font (title_label, desc);
  pango_font_description_free (desc);
}

static void
gtk_assistant_style_set (GtkWidget *widget,
			 GtkStyle  *old_style)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GtkAssistantPrivate *priv = assistant->priv;
  GList *list;

  list = priv->pages;

  while (list)
    {
      GtkAssistantPage *page = list->data;

      set_title_colors (widget, page->title);
      set_title_font (widget, page->title);

      list = list->next;
    }
}

static void
gtk_assistant_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GtkAssistantPrivate *priv = assistant->priv;
  GtkRequisition child_requisition;
  gint header_padding, content_padding;
  gint width, height, header_width, header_height;
  GList *list;

  gtk_widget_style_get (widget,
			"header-padding", &header_padding,
			"content-padding", &content_padding,
			NULL);
  width = height = 0;
  header_width = header_height = 0;
  list  = priv->pages;

  while (list)
    {
      GtkAssistantPage *page = list->data;
      gint w, h;

      gtk_widget_size_request (page->page, &child_requisition);
      width  = MAX (width,  child_requisition.width);
      height = MAX (height, child_requisition.height);

      gtk_widget_size_request (page->title, &child_requisition);
      w = child_requisition.width;
      h = child_requisition.height;

      if (page->header_image)
	{
	  w += gdk_pixbuf_get_width (page->header_image) + HEADER_SPACING;
	  h  = MAX (h, gdk_pixbuf_get_height (page->header_image));
	}

      header_width  = MAX (header_width, w);
      header_height = MAX (header_height, h);

      list = list->next;
    }

  gtk_widget_size_request (priv->sidebar_image, &child_requisition);
  width  += child_requisition.width;
  height  = MAX (height, child_requisition.height);

  gtk_widget_set_size_request (priv->header_image, header_width, header_height);
  gtk_widget_size_request (priv->header_image, &child_requisition);
  width   = MAX (width, header_width) + 2 * header_padding;
  height += header_height + 2 * header_padding;

  gtk_widget_size_request (priv->action_area, &child_requisition);
  width   = MAX (width, child_requisition.width);
  height += child_requisition.height + ACTION_AREA_SPACING;

  width += GTK_CONTAINER (widget)->border_width * 2 + content_padding * 2;
  height += GTK_CONTAINER (widget)->border_width * 2 + content_padding * 2;

  requisition->width = width;
  requisition->height = height;
}

static void
gtk_assistant_size_allocate (GtkWidget      *widget,
			     GtkAllocation  *allocation)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GtkAssistantPrivate *priv = assistant->priv;
  GtkRequisition header_requisition;
  GtkAllocation child_allocation, header_allocation;
  gint header_padding, content_padding;
  gboolean rtl;
  GList *pages;

  rtl   = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);
  pages = priv->pages;

  gtk_widget_style_get (widget,
			"header-padding", &header_padding,
			"content-padding", &content_padding,
			NULL);

  widget->allocation = *allocation;

  /* Header */
  gtk_widget_get_child_requisition (priv->header_image, &header_requisition);

  header_allocation.x = GTK_CONTAINER (widget)->border_width + header_padding;
  header_allocation.y = GTK_CONTAINER (widget)->border_width + header_padding;
  header_allocation.width  = allocation->width - 2 * GTK_CONTAINER (widget)->border_width - 2 * header_padding;
  header_allocation.height = header_requisition.height;

  gtk_widget_size_allocate (priv->header_image, &header_allocation);

  /* Action area */
  child_allocation.x = GTK_CONTAINER (widget)->border_width;
  child_allocation.y = allocation->height -
    GTK_CONTAINER (widget)->border_width - priv->action_area->requisition.height;
  child_allocation.width  = allocation->width - 2 * GTK_CONTAINER (widget)->border_width;
  child_allocation.height = priv->action_area->requisition.height;

  gtk_widget_size_allocate (priv->action_area, &child_allocation);

  /* Sidebar */
  if (rtl)
    child_allocation.x = allocation->width -
      GTK_CONTAINER (widget)->border_width - priv->sidebar_image->requisition.width;
  else
    child_allocation.x = GTK_CONTAINER (widget)->border_width;

  child_allocation.y = GTK_CONTAINER (widget)->border_width +
    priv->header_image->allocation.height + 2 * header_padding;
  child_allocation.width = priv->sidebar_image->requisition.width;
  child_allocation.height = allocation->height - 2 * GTK_CONTAINER (widget)->border_width -
    priv->header_image->allocation.height - 2 * header_padding - priv->action_area->allocation.height;

  gtk_widget_size_allocate (priv->sidebar_image, &child_allocation);

  /* Pages */
  child_allocation.x = GTK_CONTAINER (widget)->border_width + content_padding;
  child_allocation.y = GTK_CONTAINER (widget)->border_width +
    priv->header_image->allocation.height + 2 * header_padding + content_padding;
  child_allocation.width  = allocation->width - 2 * GTK_CONTAINER (widget)->border_width - 2 * content_padding;
  child_allocation.height = allocation->height - 2 * GTK_CONTAINER (widget)->border_width -
    priv->header_image->allocation.height - 2 * header_padding - ACTION_AREA_SPACING - priv->action_area->allocation.height - 2 * content_padding;

  if (gtk_widget_get_visible (priv->sidebar_image))
    {
      if (!rtl)
	child_allocation.x += priv->sidebar_image->allocation.width;

      child_allocation.width -= priv->sidebar_image->allocation.width;
    }

  while (pages)
    {
      GtkAssistantPage *page = pages->data;

      gtk_widget_size_allocate (page->page, &child_allocation);
      gtk_widget_size_allocate (page->title, &header_allocation);
      pages = pages->next;
    }
}

static void
gtk_assistant_map (GtkWidget *widget)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GtkAssistantPrivate *priv = assistant->priv;
  GList *page_node;
  GtkAssistantPage *page;

  gtk_widget_set_mapped (widget, TRUE);

  gtk_widget_map (priv->header_image);
  gtk_widget_map (priv->action_area);

  if (gtk_widget_get_visible (priv->sidebar_image) &&
      !gtk_widget_get_mapped (priv->sidebar_image))
    gtk_widget_map (priv->sidebar_image);

  /* if there's no default page, pick the first one */
  page = NULL;
  if (!priv->current_page)
    {
      page_node = priv->pages;

      while (page_node && !gtk_widget_get_visible (((GtkAssistantPage *) page_node->data)->page))
	page_node = page_node->next;

      if (page_node)
	page = page_node->data;
    }

  if (page &&
      gtk_widget_get_visible (page->page) &&
      !gtk_widget_get_mapped (page->page))
    set_current_page (assistant, page);

  GTK_WIDGET_CLASS (gtk_assistant_parent_class)->map (widget);
}

static void
gtk_assistant_unmap (GtkWidget *widget)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GtkAssistantPrivate *priv = assistant->priv;

  gtk_widget_set_mapped (widget, FALSE);

  gtk_widget_unmap (priv->header_image);
  gtk_widget_unmap (priv->action_area);

  if (gtk_widget_is_drawable (priv->sidebar_image))
    gtk_widget_unmap (priv->sidebar_image);

  if (priv->current_page &&
      gtk_widget_is_drawable (priv->current_page->page))
    gtk_widget_unmap (priv->current_page->page);

  g_slist_free (priv->visited_pages);
  priv->visited_pages = NULL;
  priv->current_page  = NULL;

  GTK_WIDGET_CLASS (gtk_assistant_parent_class)->unmap (widget);
}

static gboolean
gtk_assistant_delete_event (GtkWidget   *widget,
			    GdkEventAny *event)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GtkAssistantPrivate *priv = assistant->priv;

  /* Do not allow cancelling in the middle of a progress page */
  if (priv->current_page &&
      (priv->current_page->type != GTK_ASSISTANT_PAGE_PROGRESS ||
       priv->current_page->complete))
    g_signal_emit (widget, signals [CANCEL], 0, NULL);

  return TRUE;
}

static void
assistant_paint_colored_box (GtkWidget *widget)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GtkAssistantPrivate *priv = assistant->priv;
  gint border_width, header_padding, content_padding;
  cairo_t *cr;
  gint content_x, content_width;
  gboolean rtl;

  cr   = gdk_cairo_create (widget->window);
  rtl  = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);
  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  gtk_widget_style_get (widget,
			"header-padding",  &header_padding,
			"content-padding", &content_padding,
			NULL);

  /* colored box */
  gdk_cairo_set_source_color (cr, &widget->style->bg[GTK_STATE_SELECTED]);
  cairo_rectangle (cr,
		   border_width,
		   border_width,
		   widget->allocation.width - 2 * border_width,
		   widget->allocation.height - priv->action_area->allocation.height - 2 * border_width - ACTION_AREA_SPACING);
  cairo_fill (cr);

  /* content box */
  content_x = content_padding + border_width;
  content_width = widget->allocation.width - 2 * content_padding - 2 * border_width;

  if (gtk_widget_get_visible (priv->sidebar_image))
    {
      if (!rtl)
	content_x += priv->sidebar_image->allocation.width;
      content_width -= priv->sidebar_image->allocation.width;
    }
  
  gdk_cairo_set_source_color (cr, &widget->style->bg[GTK_STATE_NORMAL]);

  cairo_rectangle (cr,
		   content_x,
		   priv->header_image->allocation.height + content_padding + 2 * header_padding + border_width,
		   content_width,
		   widget->allocation.height - 2 * border_width - priv->action_area->allocation.height -
		   priv->header_image->allocation.height - 2 * content_padding - 2 * header_padding - ACTION_AREA_SPACING);
  cairo_fill (cr);

  cairo_destroy (cr);
}

static gboolean
gtk_assistant_expose (GtkWidget      *widget,
		      GdkEventExpose *event)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GtkAssistantPrivate *priv = assistant->priv;
  GtkContainer *container;

  if (gtk_widget_is_drawable (widget))
    {
      container = GTK_CONTAINER (widget);

      assistant_paint_colored_box (widget);

      gtk_container_propagate_expose (container, priv->header_image, event);
      gtk_container_propagate_expose (container, priv->sidebar_image, event);
      gtk_container_propagate_expose (container, priv->action_area, event);

      if (priv->current_page)
	{
	  gtk_container_propagate_expose (container, priv->current_page->page, event);
	  gtk_container_propagate_expose (container, priv->current_page->title, event);
	}
    }

  return FALSE;
}

static gboolean
gtk_assistant_focus (GtkWidget        *widget,
		     GtkDirectionType  direction)
{
  GtkAssistantPrivate *priv;
  GtkContainer *container;

  container = GTK_CONTAINER (widget);
  priv = GTK_ASSISTANT (widget)->priv;

  /* we only have to care about 2 widgets, action area and the current page */
  if (container->focus_child == priv->action_area)
    {
      if (!gtk_widget_child_focus (priv->action_area, direction) &&
	  (priv->current_page == NULL ||
	   !gtk_widget_child_focus (priv->current_page->page, direction)))
	{
	  /* if we're leaving the action area and the current page hasn't
	     any focusable widget, clear focus and go back to the action area */
	  gtk_container_set_focus_child (GTK_CONTAINER (priv->action_area), NULL);
	  gtk_widget_child_focus (priv->action_area, direction);
	}
    }
  else
    {
      if ((priv->current_page ==  NULL ||
	   !gtk_widget_child_focus (priv->current_page->page, direction)) &&
	  !gtk_widget_child_focus (priv->action_area, direction))
	{
	  /* if we're leaving the current page and there isn't nothing focusable
	     in the action area, try to clear focus and go back to the page */
	  gtk_window_set_focus (GTK_WINDOW (widget), NULL);
	  if (priv->current_page != NULL)
	    gtk_widget_child_focus (priv->current_page->page, direction);
	}
    }

  return TRUE;
}

static void
gtk_assistant_add (GtkContainer *container,
		   GtkWidget    *page)
{
  gtk_assistant_append_page (GTK_ASSISTANT (container), page);
}

static void
gtk_assistant_remove (GtkContainer *container,
		      GtkWidget    *page)
{
  GtkAssistant *assistant = (GtkAssistant*) container;
  GList *element;

  element = find_page (assistant, page);

  if (element)
    {
      remove_page (assistant, element);
      gtk_widget_queue_resize ((GtkWidget *) container);
    }
}

static void
gtk_assistant_forall (GtkContainer *container,
		      gboolean      include_internals,
		      GtkCallback   callback,
		      gpointer      callback_data)
{
  GtkAssistant *assistant = (GtkAssistant*) container;
  GtkAssistantPrivate *priv = assistant->priv;
  GList *pages;

  if (include_internals)
    {
      (*callback) (priv->header_image, callback_data);
      (*callback) (priv->sidebar_image, callback_data);
      (*callback) (priv->action_area, callback_data);
    }

  pages = priv->pages;

  while (pages)
    {
      GtkAssistantPage *page = (GtkAssistantPage *) pages->data;

      (*callback) (page->page, callback_data);

      if (include_internals)
	(*callback) (page->title, callback_data);

      pages = pages->next;
    }
}

/**
 * gtk_assistant_new:
 * 
 * Creates a new #GtkAssistant.
 *
 * Return value: a newly created #GtkAssistant
 *
 * Since: 2.10
 **/
GtkWidget*
gtk_assistant_new (void)
{
  GtkWidget *assistant;

  assistant = g_object_new (GTK_TYPE_ASSISTANT, NULL);

  return assistant;
}

/**
 * gtk_assistant_get_current_page:
 * @assistant: a #GtkAssistant
 *
 * Returns the page number of the current page
 *
 * Return value: The index (starting from 0) of the current page in
 * the @assistant, if the @assistant has no pages, -1 will be returned
 *
 * Since: 2.10
 **/
gint
gtk_assistant_get_current_page (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), -1);

  priv = assistant->priv;

  if (!priv->pages || !priv->current_page)
    return -1;

  return g_list_index (priv->pages, priv->current_page);
}

/**
 * gtk_assistant_set_current_page:
 * @assistant: a #GtkAssistant
 * @page_num: index of the page to switch to, starting from 0.
 *            If negative, the last page will be used. If greater
 *            than the number of pages in the @assistant, nothing
 *            will be done.
 *
 * Switches the page to @page_num. Note that this will only be necessary
 * in custom buttons, as the @assistant flow can be set with
 * gtk_assistant_set_forward_page_func().
 *
 * Since: 2.10
 **/
void
gtk_assistant_set_current_page (GtkAssistant *assistant,
				gint          page_num)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));

  priv = assistant->priv;

  if (page_num >= 0)
    page = (GtkAssistantPage *) g_list_nth_data (priv->pages, page_num);
  else
    page = (GtkAssistantPage *) g_list_last (priv->pages)->data;

  g_return_if_fail (page != NULL);

  if (priv->current_page == page)
    return;

  /* only add the page to the visited list if the
   * assistant is mapped, if not, just use it as an
   * initial page setting, for the cases where the
   * initial page is != to 0
   */
  if (gtk_widget_get_mapped (GTK_WIDGET (assistant)))
    priv->visited_pages = g_slist_prepend (priv->visited_pages,
					   priv->current_page);

  set_current_page (assistant, page);
}

/**
 * gtk_assistant_get_n_pages:
 * @assistant: a #GtkAssistant
 *
 * Returns the number of pages in the @assistant
 *
 * Return value: The number of pages in the @assistant.
 *
 * Since: 2.10
 **/
gint
gtk_assistant_get_n_pages (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), 0);

  priv = assistant->priv;

  return g_list_length (priv->pages);
}

/**
 * gtk_assistant_get_nth_page:
 * @assistant: a #GtkAssistant
 * @page_num: The index of a page in the @assistant, or -1 to get the last page;
 *
 * Returns the child widget contained in page number @page_num.
 *
 * Return value: (transfer none): The child widget, or %NULL
 *     if @page_num is out of bounds.
 *
 * Since: 2.10
 **/
GtkWidget*
gtk_assistant_get_nth_page (GtkAssistant *assistant,
			    gint          page_num)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page;
  GList *elem;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), NULL);
  g_return_val_if_fail (page_num >= -1, NULL);

  priv = assistant->priv;

  if (page_num == -1)
    elem = g_list_last (priv->pages);
  else
    elem = g_list_nth (priv->pages, page_num);

  if (!elem)
    return NULL;

  page = (GtkAssistantPage *) elem->data;

  return page->page;
}

/**
 * gtk_assistant_prepend_page:
 * @assistant: a #GtkAssistant
 * @page: a #GtkWidget
 *
 * Prepends a page to the @assistant.
 *
 * Return value: the index (starting at 0) of the inserted page
 *
 * Since: 2.10
 **/
gint
gtk_assistant_prepend_page (GtkAssistant *assistant,
			    GtkWidget    *page)
{
  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), 0);
  g_return_val_if_fail (GTK_IS_WIDGET (page), 0);

  return gtk_assistant_insert_page (assistant, page, 0);
}

/**
 * gtk_assistant_append_page:
 * @assistant: a #GtkAssistant
 * @page: a #GtkWidget
 *
 * Appends a page to the @assistant.
 *
 * Return value: the index (starting at 0) of the inserted page
 *
 * Since: 2.10
 **/
gint
gtk_assistant_append_page (GtkAssistant *assistant,
			   GtkWidget    *page)
{
  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), 0);
  g_return_val_if_fail (GTK_IS_WIDGET (page), 0);

  return gtk_assistant_insert_page (assistant, page, -1);
}

/**
 * gtk_assistant_insert_page:
 * @assistant: a #GtkAssistant
 * @page: a #GtkWidget
 * @position: the index (starting at 0) at which to insert the page,
 *            or -1 to append the page to the @assistant
 *
 * Inserts a page in the @assistant at a given position.
 *
 * Return value: the index (starting from 0) of the inserted page
 *
 * Since: 2.10
 **/
gint
gtk_assistant_insert_page (GtkAssistant *assistant,
			   GtkWidget    *page,
			   gint          position)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  gint n_pages;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), 0);
  g_return_val_if_fail (GTK_IS_WIDGET (page), 0);
  g_return_val_if_fail (page->parent == NULL, 0);
  g_return_val_if_fail (!gtk_widget_is_toplevel (page), 0);

  priv = assistant->priv;

  page_info = g_slice_new0 (GtkAssistantPage);
  page_info->page  = page;
  page_info->title = gtk_label_new (NULL);

  g_signal_connect (G_OBJECT (page), "notify::visible",
		    G_CALLBACK (on_page_notify_visibility), assistant);

  gtk_misc_set_alignment (GTK_MISC (page_info->title), 0.,0.5);
  set_title_colors (GTK_WIDGET (assistant), page_info->title);
  set_title_font   (GTK_WIDGET (assistant), page_info->title);
  gtk_widget_show  (page_info->title);

  n_pages = g_list_length (priv->pages);

  if (position < 0 || position > n_pages)
    position = n_pages;

  priv->pages = g_list_insert (priv->pages, page_info, position);

  gtk_widget_set_child_visible (page_info->page, FALSE);
  gtk_widget_set_parent (page_info->page,  GTK_WIDGET (assistant));
  gtk_widget_set_parent (page_info->title, GTK_WIDGET (assistant));

  if (gtk_widget_get_realized (GTK_WIDGET (assistant)))
    {
      gtk_widget_realize (page_info->page);
      gtk_widget_realize (page_info->title);
    }

  gtk_widget_queue_resize (GTK_WIDGET (assistant));

  return position;
}

/**
 * gtk_assistant_set_forward_page_func:
 * @assistant: a #GtkAssistant
 * @page_func: (allow-none): the #GtkAssistantPageFunc, or %NULL to use the default one
 * @data: user data for @page_func
 * @destroy: destroy notifier for @data
 *
 * Sets the page forwarding function to be @page_func, this function will
 * be used to determine what will be the next page when the user presses
 * the forward button. Setting @page_func to %NULL will make the assistant
 * to use the default forward function, which just goes to the next visible 
 * page.
 *
 * Since: 2.10
 **/
void
gtk_assistant_set_forward_page_func (GtkAssistant         *assistant,
				     GtkAssistantPageFunc  page_func,
				     gpointer              data,
				     GDestroyNotify        destroy)
{
  GtkAssistantPrivate *priv;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));

  priv = assistant->priv;

  if (priv->forward_data_destroy &&
      priv->forward_function_data)
    (*priv->forward_data_destroy) (priv->forward_function_data);

  if (page_func)
    {
      priv->forward_function = page_func;
      priv->forward_function_data = data;
      priv->forward_data_destroy = destroy;
    }
  else
    {
      priv->forward_function = default_forward_function;
      priv->forward_function_data = assistant;
      priv->forward_data_destroy = NULL;
    }

  /* Page flow has possibly changed, so the
     buttons state might need to change too */
  set_assistant_buttons_state (assistant);
}

/**
 * gtk_assistant_add_action_widget:
 * @assistant: a #GtkAssistant
 * @child: a #GtkWidget
 * 
 * Adds a widget to the action area of a #GtkAssistant.
 *
 * Since: 2.10
 **/
void
gtk_assistant_add_action_widget (GtkAssistant *assistant,
				 GtkWidget    *child)
{
  GtkAssistantPrivate *priv;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (child));

  priv = assistant->priv;

  if (GTK_IS_BUTTON (child))
    gtk_size_group_add_widget (priv->size_group, child);

  gtk_box_pack_end (GTK_BOX (priv->action_area), child, FALSE, FALSE, 0);
}

/**
 * gtk_assistant_remove_action_widget:
 * @assistant: a #GtkAssistant
 * @child: a #GtkWidget
 *
 * Removes a widget from the action area of a #GtkAssistant.
 *
 * Since: 2.10
 **/
void
gtk_assistant_remove_action_widget (GtkAssistant *assistant,
				    GtkWidget    *child)
{
  GtkAssistantPrivate *priv;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (child));

  priv = assistant->priv;

  if (GTK_IS_BUTTON (child))
    gtk_size_group_remove_widget (priv->size_group, child);

  gtk_container_remove (GTK_CONTAINER (priv->action_area), child);
}

/**
 * gtk_assistant_set_page_title:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 * @title: the new title for @page
 * 
 * Sets a title for @page. The title is displayed in the header
 * area of the assistant when @page is the current page.
 *
 * Since: 2.10
 **/
void
gtk_assistant_set_page_title (GtkAssistant *assistant,
			      GtkWidget    *page,
			      const gchar  *title)
{
  GtkAssistantPage *page_info;
  GList *child;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (page));

  child = find_page (assistant, page);

  g_return_if_fail (child != NULL);

  page_info = (GtkAssistantPage*) child->data;

  gtk_label_set_text ((GtkLabel*) page_info->title, title);
  gtk_widget_queue_resize (GTK_WIDGET (assistant));
  gtk_widget_child_notify (page, "title");
}

/**
 * gtk_assistant_get_page_title:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 * 
 * Gets the title for @page. 
 * 
 * Return value: the title for @page.
 *
 * Since: 2.10
 **/
const gchar*
gtk_assistant_get_page_title (GtkAssistant *assistant,
			      GtkWidget    *page)
{
  GtkAssistantPage *page_info;
  GList *child;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (page), NULL);

  child = find_page (assistant, page);

  g_return_val_if_fail (child != NULL, NULL);

  page_info = (GtkAssistantPage*) child->data;

  return gtk_label_get_text ((GtkLabel*) page_info->title);
}

/**
 * gtk_assistant_set_page_type:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 * @type: the new type for @page
 * 
 * Sets the page type for @page. The page type determines the page
 * behavior in the @assistant.
 *
 * Since: 2.10
 **/
void
gtk_assistant_set_page_type (GtkAssistant         *assistant,
			     GtkWidget            *page,
			     GtkAssistantPageType  type)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  GList *child;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (page));

  priv = assistant->priv;
  child = find_page (assistant, page);

  g_return_if_fail (child != NULL);

  page_info = (GtkAssistantPage*) child->data;

  if (type != page_info->type)
    {
      page_info->type = type;

      /* backwards compatibility to the era before fixing bug 604289 */
      if (type == GTK_ASSISTANT_PAGE_SUMMARY && !page_info->complete_set)
        {
          gtk_assistant_set_page_complete (assistant, page, TRUE);
          page_info->complete_set = FALSE;
        }

      /* Always set buttons state, a change in a future page
	 might change current page buttons */
      set_assistant_buttons_state (assistant);

      gtk_widget_child_notify (page, "page-type");
    }
}

/**
 * gtk_assistant_get_page_type:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 *
 * Gets the page type of @page.
 *
 * Return value: the page type of @page.
 *
 * Since: 2.10
 **/
GtkAssistantPageType
gtk_assistant_get_page_type (GtkAssistant *assistant,
			     GtkWidget    *page)
{
  GtkAssistantPage *page_info;
  GList *child;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), GTK_ASSISTANT_PAGE_CONTENT);
  g_return_val_if_fail (GTK_IS_WIDGET (page), GTK_ASSISTANT_PAGE_CONTENT);

  child = find_page (assistant, page);

  g_return_val_if_fail (child != NULL, GTK_ASSISTANT_PAGE_CONTENT);

  page_info = (GtkAssistantPage*) child->data;

  return page_info->type;
}

/**
 * gtk_assistant_set_page_header_image:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 * @pixbuf: (allow-none): the new header image @page
 *
 * Sets a header image for @page. This image is displayed in the header
 * area of the assistant when @page is the current page.
 *
 * Since: 2.10
 **/
void
gtk_assistant_set_page_header_image (GtkAssistant *assistant,
				     GtkWidget    *page,
				     GdkPixbuf    *pixbuf)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  GList *child;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (page));
  g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

  priv = assistant->priv;
  child = find_page (assistant, page);

  g_return_if_fail (child != NULL);

  page_info = (GtkAssistantPage*) child->data;

  if (pixbuf != page_info->header_image)
    {
      if (page_info->header_image)
	{
	  g_object_unref (page_info->header_image);
	  page_info->header_image = NULL;
	}

      if (pixbuf)
	page_info->header_image = g_object_ref (pixbuf);

      if (page_info == priv->current_page)
	set_assistant_header_image (assistant);

      gtk_widget_child_notify (page, "header-image");
    }
}

/**
 * gtk_assistant_get_page_header_image:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 *
 * Gets the header image for @page.
 *
 * Return value: (transfer none): the header image for @page, or %NULL
 *     if there's no header image for the page.
 *
 * Since: 2.10
 **/
GdkPixbuf*
gtk_assistant_get_page_header_image (GtkAssistant *assistant,
				     GtkWidget    *page)
{
  GtkAssistantPage *page_info;
  GList *child;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (page), NULL);

  child = find_page (assistant, page);

  g_return_val_if_fail (child != NULL, NULL);

  page_info = (GtkAssistantPage*) child->data;

  return page_info->header_image;
}

/**
 * gtk_assistant_set_page_side_image:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 * @pixbuf: (allow-none): the new header image @page
 *
 * Sets a header image for @page. This image is displayed in the side
 * area of the assistant when @page is the current page.
 *
 * Since: 2.10
 **/
void
gtk_assistant_set_page_side_image (GtkAssistant *assistant,
				   GtkWidget    *page,
				   GdkPixbuf    *pixbuf)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  GList *child;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (page));
  g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

  priv = assistant->priv;
  child = find_page (assistant, page);

  g_return_if_fail (child != NULL);

  page_info = (GtkAssistantPage*) child->data;

  if (pixbuf != page_info->sidebar_image)
    {
      if (page_info->sidebar_image)
	{
	  g_object_unref (page_info->sidebar_image);
	  page_info->sidebar_image = NULL;
	}

      if (pixbuf)
	page_info->sidebar_image = g_object_ref (pixbuf);

      if (page_info == priv->current_page)
	set_assistant_sidebar_image (assistant);

      gtk_widget_child_notify (page, "sidebar-image");
    }
}

/**
 * gtk_assistant_get_page_side_image:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 *
 * Gets the header image for @page.
 *
 * Return value: (transfer none): the side image for @page, or %NULL
 *     if there's no side image for the page.
 *
 * Since: 2.10
 **/
GdkPixbuf*
gtk_assistant_get_page_side_image (GtkAssistant *assistant,
				   GtkWidget    *page)
{
  GtkAssistantPage *page_info;
  GList *child;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (page), NULL);

  child = find_page (assistant, page);

  g_return_val_if_fail (child != NULL, NULL);

  page_info = (GtkAssistantPage*) child->data;

  return page_info->sidebar_image;
}

/**
 * gtk_assistant_set_page_complete:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 * @complete: the completeness status of the page
 * 
 * Sets whether @page contents are complete. This will make
 * @assistant update the buttons state to be able to continue the task.
 *
 * Since: 2.10
 **/
void
gtk_assistant_set_page_complete (GtkAssistant *assistant,
				 GtkWidget    *page,
				 gboolean      complete)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  GList *child;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (page));

  priv = assistant->priv;
  child = find_page (assistant, page);

  g_return_if_fail (child != NULL);

  page_info = (GtkAssistantPage*) child->data;

  if (complete != page_info->complete)
    {
      page_info->complete = complete;
      page_info->complete_set = TRUE;

      /* Always set buttons state, a change in a future page
	 might change current page buttons */
      set_assistant_buttons_state (assistant);

      gtk_widget_child_notify (page, "complete");
    }
}

/**
 * gtk_assistant_get_page_complete:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 * 
 * Gets whether @page is complete.
 * 
 * Return value: %TRUE if @page is complete.
 *
 * Since: 2.10
 **/
gboolean
gtk_assistant_get_page_complete (GtkAssistant *assistant,
				 GtkWidget    *page)
{
  GtkAssistantPage *page_info;
  GList *child;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (page), FALSE);

  child = find_page (assistant, page);

  g_return_val_if_fail (child != NULL, FALSE);

  page_info = (GtkAssistantPage*) child->data;

  return page_info->complete;
}

/**
 * gtk_assistant_update_buttons_state:
 * @assistant: a #GtkAssistant
 * 
 * Forces @assistant to recompute the buttons state.
 * 
 * GTK+ automatically takes care of this in most situations, 
 * e.g. when the user goes to a different page, or when the
 * visibility or completeness of a page changes.
 *
 * One situation where it can be necessary to call this
 * function is when changing a value on the current page
 * affects the future page flow of the assistant.
 *
 * Since: 2.10
 **/
void
gtk_assistant_update_buttons_state (GtkAssistant *assistant)
{
  g_return_if_fail (GTK_IS_ASSISTANT (assistant));

  set_assistant_buttons_state (assistant);
}

/**
 * gtk_assistant_commit:
 * @assistant: a #GtkAssistant
 *
 * Erases the visited page history so the back button is not
 * shown on the current page, and removes the cancel button
 * from subsequent pages.
 *
 * Use this when the information provided up to the current
 * page is hereafter deemed permanent and cannot be modified
 * or undone.  For example, showing a progress page to track
 * a long-running, unreversible operation after the user has
 * clicked apply on a confirmation page.
 *
 * Since: 2.22
 **/
void
gtk_assistant_commit (GtkAssistant *assistant)
{
  g_return_if_fail (GTK_IS_ASSISTANT (assistant));

  g_slist_free (assistant->priv->visited_pages);
  assistant->priv->visited_pages = NULL;

  assistant->priv->committed = TRUE;

  set_assistant_buttons_state (assistant);
}



/* accessible implementation */

static gint
gtk_assistant_accessible_get_n_children (AtkObject *accessible)
{
  GtkAssistant *assistant;
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (accessible)->widget;

  if (!widget)
    return 0;

  assistant = GTK_ASSISTANT (widget);
   
  return g_list_length (assistant->priv->pages) + 1;
}


static AtkObject *
gtk_assistant_accessible_ref_child (AtkObject *accessible,
				    gint       index)
{
  GtkAssistant *assistant;
  GtkAssistantPrivate *priv;
  GtkWidget *widget, *child;
  gint n_pages;
  AtkObject *obj;
  const gchar *title;

  widget = GTK_ACCESSIBLE (accessible)->widget;
  if (!widget)
    return NULL;

  assistant = GTK_ASSISTANT (widget);
  priv = assistant->priv;
  n_pages = g_list_length (priv->pages);

  if (index < 0)
    return NULL;
  else if (index < n_pages)
    {
      GtkAssistantPage *page = g_list_nth_data (priv->pages, index);

      child = page->page;
      title = gtk_assistant_get_page_title (assistant, child);
    }
  else if (index == n_pages)
    {
      child = priv->action_area;
      title = NULL;
    }
  else
    return NULL;
  
  obj = gtk_widget_get_accessible (child);

  if (title)
    atk_object_set_name (obj, title);

  return g_object_ref (obj);
}

static void
gtk_assistant_accessible_class_init (AtkObjectClass *class)
{
  class->get_n_children = gtk_assistant_accessible_get_n_children;
  class->ref_child = gtk_assistant_accessible_ref_child;
}

static GType
gtk_assistant_accessible_get_type (void)
{
  static GType type = 0;
  
  if (!type)
    {
      /*
       * Figure out the size of the class and instance
       * we are deriving from
       */
      AtkObjectFactory *factory;
      GType derived_type;
      GTypeQuery query;
      GType derived_atk_type;

      derived_type = g_type_parent (GTK_TYPE_ASSISTANT);
      factory = atk_registry_get_factory (atk_get_default_registry (),
					  derived_type);
      derived_atk_type = atk_object_factory_get_accessible_type (factory);
      g_type_query (derived_atk_type, &query);

      type = g_type_register_static_simple (derived_atk_type,
					    I_("GtkAssistantAccessible"),
					    query.class_size,
					    (GClassInitFunc) gtk_assistant_accessible_class_init,
					    query.instance_size,
					    NULL, 0);
    }

  return type;
}

static AtkObject *
gtk_assistant_accessible_new (GObject *obj)
{
  AtkObject *accessible;

  g_return_val_if_fail (GTK_IS_ASSISTANT (obj), NULL);

  accessible = g_object_new (gtk_assistant_accessible_get_type (), NULL);
  atk_object_initialize (accessible, obj);

  return accessible;
}

static GType
gtk_assistant_accessible_factory_get_accessible_type (void)
{
  return gtk_assistant_accessible_get_type ();
}

static AtkObject*
gtk_assistant_accessible_factory_create_accessible (GObject *obj)
{
  return gtk_assistant_accessible_new (obj);
}

static void
gtk_assistant_accessible_factory_class_init (AtkObjectFactoryClass *class)
{
  class->create_accessible = gtk_assistant_accessible_factory_create_accessible;
  class->get_accessible_type = gtk_assistant_accessible_factory_get_accessible_type;
}

static GType
gtk_assistant_accessible_factory_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      type = g_type_register_static_simple (ATK_TYPE_OBJECT_FACTORY,
					    I_("GtkAssistantAccessibleFactory"),
					    sizeof (AtkObjectFactoryClass),
					    (GClassInitFunc) gtk_assistant_accessible_factory_class_init,
					    sizeof (AtkObjectFactory),
					    NULL, 0);
    }

  return type;
}

static AtkObject *
gtk_assistant_get_accessible (GtkWidget *widget)
{
  static gboolean first_time = TRUE;

  if (first_time)
    {
      AtkObjectFactory *factory;
      AtkRegistry *registry;
      GType derived_type;
      GType derived_atk_type;

      /*
       * Figure out whether accessibility is enabled by looking at the
       * type of the accessible object which would be created for
       * the parent type of GtkAssistant.
       */
      derived_type = g_type_parent (GTK_TYPE_ASSISTANT);

      registry = atk_get_default_registry ();
      factory = atk_registry_get_factory (registry,
					  derived_type);
      derived_atk_type = atk_object_factory_get_accessible_type (factory);
      if (g_type_is_a (derived_atk_type, GTK_TYPE_ACCESSIBLE))
	{
	  atk_registry_set_factory_type (registry,
					 GTK_TYPE_ASSISTANT,
					 gtk_assistant_accessible_factory_get_type ());
	}
      first_time = FALSE;
    }

  return GTK_WIDGET_CLASS (gtk_assistant_parent_class)->get_accessible (widget);
}


static GtkBuildableIface *parent_buildable_iface;

static void
gtk_assistant_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->get_internal_child = gtk_assistant_buildable_get_internal_child;
  iface->custom_tag_start = gtk_assistant_buildable_custom_tag_start;
  iface->custom_finished = gtk_assistant_buildable_custom_finished;
}

static GObject *
gtk_assistant_buildable_get_internal_child (GtkBuildable *buildable,
                                            GtkBuilder   *builder,
                                            const gchar  *childname)
{
    if (strcmp (childname, "action_area") == 0)
      return G_OBJECT (GTK_ASSISTANT (buildable)->priv->action_area);

    return parent_buildable_iface->get_internal_child (buildable,
                                                       builder,
                                                       childname);
}

gboolean
gtk_assistant_buildable_custom_tag_start (GtkBuildable  *buildable,
                                          GtkBuilder    *builder,
                                          GObject       *child,
                                          const gchar   *tagname,
                                          GMarkupParser *parser,
                                          gpointer      *data)
{
  return parent_buildable_iface->custom_tag_start (buildable, builder, child,
                                                   tagname, parser, data);
}

static void
gtk_assistant_buildable_custom_finished (GtkBuildable *buildable,
                                         GtkBuilder   *builder,
                                         GObject      *child,
                                         const gchar  *tagname,
                                         gpointer      user_data)
{
  parent_buildable_iface->custom_finished (buildable, builder, child,
                                           tagname, user_data);
}


#define __GTK_ASSISTANT_C__
#include "gtkaliasdef.c"
