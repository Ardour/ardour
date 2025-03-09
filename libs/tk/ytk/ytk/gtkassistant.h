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

#ifndef __GTK_ASSISTANT_H__
#define __GTK_ASSISTANT_H__

#if defined(GTK_DISABLE_SINGLE_INCLUDES) && !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <ytk/ytk.h> can be included directly."
#endif

#include <ytk/gtkwindow.h>

G_BEGIN_DECLS

#define GTK_TYPE_ASSISTANT         (gtk_assistant_get_type ())
#define GTK_ASSISTANT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_ASSISTANT, GtkAssistant))
#define GTK_ASSISTANT_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST    ((c), GTK_TYPE_ASSISTANT, GtkAssistantClass))
#define GTK_IS_ASSISTANT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_ASSISTANT))
#define GTK_IS_ASSISTANT_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE    ((c), GTK_TYPE_ASSISTANT))
#define GTK_ASSISTANT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS  ((o), GTK_TYPE_ASSISTANT, GtkAssistantClass))

/**
 * GtkAssistantPageType:
 * @param GTK_ASSISTANT_PAGE_CONTENT: The page has regular contents.
 * @param GTK_ASSISTANT_PAGE_INTRO: The page contains an introduction to the
 *  assistant task.
 * @param GTK_ASSISTANT_PAGE_CONFIRM: The page lets the user confirm or deny the
 *  changes.
 * @param GTK_ASSISTANT_PAGE_SUMMARY: The page informs the user of the changes
 *  done.
 * @param GTK_ASSISTANT_PAGE_PROGRESS: Used for tasks that take a long time to
 *  complete, blocks the assistant until the page is marked as complete.
 *
 * An enum for determining the page role inside the #GtkAssistant. It's
 * used to handle buttons sensitivity and visibility.
 *
 * Note that an assistant needs to end its page flow with a page of type
 * %GTK_ASSISTANT_PAGE_CONFIRM, %GTK_ASSISTANT_PAGE_SUMMARY or
 * %GTK_ASSISTANT_PAGE_PROGRESS to be correct.
 */
typedef enum
{
  GTK_ASSISTANT_PAGE_CONTENT,
  GTK_ASSISTANT_PAGE_INTRO,
  GTK_ASSISTANT_PAGE_CONFIRM,
  GTK_ASSISTANT_PAGE_SUMMARY,
  GTK_ASSISTANT_PAGE_PROGRESS
} GtkAssistantPageType;

typedef struct _GtkAssistant        GtkAssistant;
typedef struct _GtkAssistantPrivate GtkAssistantPrivate;
typedef struct _GtkAssistantClass   GtkAssistantClass;

struct _GtkAssistant
{
  GtkWindow  parent;

  GtkWidget *GSEAL (cancel);
  GtkWidget *GSEAL (forward);
  GtkWidget *GSEAL (back);
  GtkWidget *GSEAL (apply);
  GtkWidget *GSEAL (close);
  GtkWidget *GSEAL (last);

  /*< private >*/
  GtkAssistantPrivate *GSEAL (priv);
};

struct _GtkAssistantClass
{
  GtkWindowClass parent_class;

  void (* prepare) (GtkAssistant *assistant, GtkWidget *page);
  void (* apply)   (GtkAssistant *assistant);
  void (* close)   (GtkAssistant *assistant);
  void (* cancel)  (GtkAssistant *assistant);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
  void (*_gtk_reserved5) (void);
};

/**
 * GtkAssistantPageFunc:
 * @param current_page: The page number used to calculate the next page.
 * @param data: user data.
 *
 * A function used by gtk_assistant_set_forward_page_func() to know which
 * is the next page given a current one. It's called both for computing the
 * next page when the user presses the "forward" button and for handling
 * the behavior of the "last" button.
 *
 * Returns: The next page number.
 */
typedef gint (*GtkAssistantPageFunc) (gint current_page, gpointer data);

GType                 gtk_assistant_get_type              (void) G_GNUC_CONST;
GtkWidget            *gtk_assistant_new                   (void);
gint                  gtk_assistant_get_current_page      (GtkAssistant         *assistant);
void                  gtk_assistant_set_current_page      (GtkAssistant         *assistant,
							   gint                  page_num);
gint                  gtk_assistant_get_n_pages           (GtkAssistant         *assistant);
GtkWidget            *gtk_assistant_get_nth_page          (GtkAssistant         *assistant,
							   gint                  page_num);
gint                  gtk_assistant_prepend_page          (GtkAssistant         *assistant,
							   GtkWidget            *page);
gint                  gtk_assistant_append_page           (GtkAssistant         *assistant,
							   GtkWidget            *page);
gint                  gtk_assistant_insert_page           (GtkAssistant         *assistant,
							   GtkWidget            *page,
							   gint                  position);
void                  gtk_assistant_set_forward_page_func (GtkAssistant         *assistant,
							   GtkAssistantPageFunc  page_func,
							   gpointer              data,
							   GDestroyNotify        destroy);
void                  gtk_assistant_set_page_type         (GtkAssistant         *assistant,
							   GtkWidget            *page,
							   GtkAssistantPageType  type);
GtkAssistantPageType  gtk_assistant_get_page_type         (GtkAssistant         *assistant,
							   GtkWidget            *page);
void                  gtk_assistant_set_page_title        (GtkAssistant         *assistant,
							   GtkWidget            *page,
							   const gchar          *title);
const gchar *         gtk_assistant_get_page_title        (GtkAssistant         *assistant,
							   GtkWidget            *page);
void                  gtk_assistant_set_page_header_image (GtkAssistant         *assistant,
							   GtkWidget            *page,
							   GdkPixbuf            *pixbuf);
GdkPixbuf            *gtk_assistant_get_page_header_image (GtkAssistant         *assistant,
							   GtkWidget            *page);
void                  gtk_assistant_set_page_side_image   (GtkAssistant         *assistant,
							   GtkWidget            *page,
							   GdkPixbuf            *pixbuf);
GdkPixbuf            *gtk_assistant_get_page_side_image   (GtkAssistant         *assistant,
							   GtkWidget            *page);
void                  gtk_assistant_set_page_complete     (GtkAssistant         *assistant,
							   GtkWidget            *page,
							   gboolean              complete);
gboolean              gtk_assistant_get_page_complete     (GtkAssistant         *assistant,
							   GtkWidget            *page);
void                  gtk_assistant_add_action_widget     (GtkAssistant         *assistant,
							   GtkWidget            *child);
void                  gtk_assistant_remove_action_widget  (GtkAssistant         *assistant,
							   GtkWidget            *child);

void                  gtk_assistant_update_buttons_state  (GtkAssistant *assistant);
void                  gtk_assistant_commit                (GtkAssistant *assistant);

G_END_DECLS

#endif /* __GTK_ASSISTANT_H__ */
