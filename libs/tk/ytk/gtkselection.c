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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* This file implements most of the work of the ICCCM selection protocol.
 * The code was written after an intensive study of the equivalent part
 * of John Ousterhout's Tk toolkit, and does many things in much the 
 * same way.
 *
 * The one thing in the ICCCM that isn't fully supported here (or in Tk)
 * is side effects targets. For these to be handled properly, MULTIPLE
 * targets need to be done in the order specified. This cannot be
 * guaranteed with the way we do things, since if we are doing INCR
 * transfers, the order will depend on the timing of the requestor.
 *
 * By Owen Taylor <owt1@cornell.edu>	      8/16/97
 */

/* Terminology note: when not otherwise specified, the term "incr" below
 * refers to the _sending_ part of the INCR protocol. The receiving
 * portion is referred to just as "retrieval". (Terminology borrowed
 * from Tk, because there is no good opposite to "retrieval" in English.
 * "send" can't be made into a noun gracefully and we're already using
 * "emission" for something else ....)
 */

/* The MOTIF entry widget seems to ask for the TARGETS target, then
   (regardless of the reply) ask for the TEXT target. It's slightly
   possible though that it somehow thinks we are responding negatively
   to the TARGETS request, though I don't really think so ... */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include <stdarg.h>
#include <string.h>
#include "gdk.h"

#include "gtkmain.h"
#include "gtkselection.h"
#include "gtktextbufferrichtext.h"
#include "gtkintl.h"
#include "gdk-pixbuf/gdk-pixbuf.h"

#ifdef GDK_WINDOWING_X11
#include "gdk/gdkx.h"
#endif

#ifdef GDK_WINDOWING_WIN32
#include "gdk/gdkwin32.h"
#endif

#include "gtkalias.h"

#undef DEBUG_SELECTION

/* Maximum size of a sent chunk, in bytes. Also the default size of
   our buffers */
#ifdef GDK_WINDOWING_X11
#define GTK_SELECTION_MAX_SIZE(display)                                 \
  MIN(262144,                                                           \
      XExtendedMaxRequestSize (GDK_DISPLAY_XDISPLAY (display)) == 0     \
       ? XMaxRequestSize (GDK_DISPLAY_XDISPLAY (display)) - 100         \
       : XExtendedMaxRequestSize (GDK_DISPLAY_XDISPLAY (display)) - 100)
#else
/* No chunks on Win32 */
#define GTK_SELECTION_MAX_SIZE(display) G_MAXINT
#endif

#define IDLE_ABORT_TIME 30

enum {
  INCR,
  MULTIPLE,
  TARGETS,
  TIMESTAMP,
  SAVE_TARGETS,
  LAST_ATOM
};

typedef struct _GtkSelectionInfo GtkSelectionInfo;
typedef struct _GtkIncrConversion GtkIncrConversion;
typedef struct _GtkIncrInfo GtkIncrInfo;
typedef struct _GtkRetrievalInfo GtkRetrievalInfo;

struct _GtkSelectionInfo
{
  GdkAtom	 selection;
  GtkWidget	*widget;	/* widget that owns selection */
  guint32	 time;		/* time used to acquire selection */
  GdkDisplay	*display;	/* needed in gtk_selection_remove_all */    
};

struct _GtkIncrConversion 
{
  GdkAtom	    target;	/* Requested target */
  GdkAtom	    property;	/* Property to store in */
  GtkSelectionData  data;	/* The data being supplied */
  gint		    offset;	/* Current offset in sent selection.
				 *  -1 => All done
				 *  -2 => Only the final (empty) portion
				 *	  left to send */
};

struct _GtkIncrInfo
{
  GdkWindow *requestor;		/* Requestor window - we create a GdkWindow
				   so we can receive events */
  GdkAtom    selection;		/* Selection we're sending */
  
  GtkIncrConversion *conversions; /* Information about requested conversions -
				   * With MULTIPLE requests (benighted 1980's
				   * hardware idea), there can be more than
				   * one */
  gint num_conversions;
  gint num_incrs;		/* number of remaining INCR style transactions */
  guint32 idle_time;
};


struct _GtkRetrievalInfo
{
  GtkWidget *widget;
  GdkAtom selection;		/* Selection being retrieved. */
  GdkAtom target;		/* Form of selection that we requested */
  guint32 idle_time;		/* Number of seconds since we last heard
				   from selection owner */
  guchar   *buffer;		/* Buffer in which to accumulate results */
  gint	   offset;		/* Current offset in buffer, -1 indicates
				   not yet started */
  guint32 notify_time;		/* Timestamp from SelectionNotify */
};

/* Local Functions */
static void gtk_selection_init              (void);
static gboolean gtk_selection_incr_timeout      (GtkIncrInfo      *info);
static gboolean gtk_selection_retrieval_timeout (GtkRetrievalInfo *info);
static void gtk_selection_retrieval_report  (GtkRetrievalInfo *info,
					     GdkAtom           type,
					     gint              format,
					     guchar           *buffer,
					     gint              length,
					     guint32           time);
static void gtk_selection_invoke_handler    (GtkWidget        *widget,
					     GtkSelectionData *data,
					     guint             time);
static void gtk_selection_default_handler   (GtkWidget        *widget,
					     GtkSelectionData *data);
static int  gtk_selection_bytes_per_item    (gint              format);

/* Local Data */
static gint initialize = TRUE;
static GList *current_retrievals = NULL;
static GList *current_incrs = NULL;
static GList *current_selections = NULL;

static GdkAtom gtk_selection_atoms[LAST_ATOM];
static const char gtk_selection_handler_key[] = "gtk-selection-handlers";

/****************
 * Target Lists *
 ****************/

/*
 * Target lists
 */


/**
 * gtk_target_list_new:
 * @targets: (array length=ntargets): Pointer to an array of #GtkTargetEntry
 * @ntargets: number of entries in @targets.
 * 
 * Creates a new #GtkTargetList from an array of #GtkTargetEntry.
 * 
 * Return value: (transfer full): the new #GtkTargetList.
 **/
GtkTargetList *
gtk_target_list_new (const GtkTargetEntry *targets,
		     guint                 ntargets)
{
  GtkTargetList *result = g_slice_new (GtkTargetList);
  result->list = NULL;
  result->ref_count = 1;

  if (targets)
    gtk_target_list_add_table (result, targets, ntargets);
  
  return result;
}

/**
 * gtk_target_list_ref:
 * @list:  a #GtkTargetList
 * 
 * Increases the reference count of a #GtkTargetList by one.
 *
 * Return value: the passed in #GtkTargetList.
 **/
GtkTargetList *
gtk_target_list_ref (GtkTargetList *list)
{
  g_return_val_if_fail (list != NULL, NULL);

  list->ref_count++;

  return list;
}

/**
 * gtk_target_list_unref:
 * @list: a #GtkTargetList
 * 
 * Decreases the reference count of a #GtkTargetList by one.
 * If the resulting reference count is zero, frees the list.
 **/
void               
gtk_target_list_unref (GtkTargetList *list)
{
  g_return_if_fail (list != NULL);
  g_return_if_fail (list->ref_count > 0);

  list->ref_count--;
  if (list->ref_count == 0)
    {
      GList *tmp_list = list->list;
      while (tmp_list)
	{
	  GtkTargetPair *pair = tmp_list->data;
	  g_slice_free (GtkTargetPair, pair);

	  tmp_list = tmp_list->next;
	}
      
      g_list_free (list->list);
      g_slice_free (GtkTargetList, list);
    }
}

/**
 * gtk_target_list_add:
 * @list:  a #GtkTargetList
 * @target: the interned atom representing the target
 * @flags: the flags for this target
 * @info: an ID that will be passed back to the application
 * 
 * Appends another target to a #GtkTargetList.
 **/
void 
gtk_target_list_add (GtkTargetList *list,
		     GdkAtom        target,
		     guint          flags,
		     guint          info)
{
  GtkTargetPair *pair;

  g_return_if_fail (list != NULL);
  
  pair = g_slice_new (GtkTargetPair);
  pair->target = target;
  pair->flags = flags;
  pair->info = info;

  list->list = g_list_append (list->list, pair);
}

static GdkAtom utf8_atom;
static GdkAtom text_atom;
static GdkAtom ctext_atom;
static GdkAtom text_plain_atom;
static GdkAtom text_plain_utf8_atom;
static GdkAtom text_plain_locale_atom;
static GdkAtom text_uri_list_atom;

static void 
init_atoms (void)
{
  gchar *tmp;
  const gchar *charset;

  if (!utf8_atom)
    {
      utf8_atom = gdk_atom_intern_static_string ("UTF8_STRING");
      text_atom = gdk_atom_intern_static_string ("TEXT");
      ctext_atom = gdk_atom_intern_static_string ("COMPOUND_TEXT");
      text_plain_atom = gdk_atom_intern_static_string ("text/plain");
      text_plain_utf8_atom = gdk_atom_intern_static_string ("text/plain;charset=utf-8");
      g_get_charset (&charset);
      tmp = g_strdup_printf ("text/plain;charset=%s", charset);
      text_plain_locale_atom = gdk_atom_intern (tmp, FALSE);
      g_free (tmp);

      text_uri_list_atom = gdk_atom_intern_static_string ("text/uri-list");
    }
}

/**
 * gtk_target_list_add_text_targets:
 * @list: a #GtkTargetList
 * @info: an ID that will be passed back to the application
 * 
 * Appends the text targets supported by #GtkSelection to
 * the target list. All targets are added with the same @info.
 * 
 * Since: 2.6
 **/
void 
gtk_target_list_add_text_targets (GtkTargetList *list,
				  guint          info)
{
  g_return_if_fail (list != NULL);
  
  init_atoms ();

  /* Keep in sync with gtk_selection_data_targets_include_text()
   */
  gtk_target_list_add (list, utf8_atom, 0, info);  
  gtk_target_list_add (list, ctext_atom, 0, info);  
  gtk_target_list_add (list, text_atom, 0, info);  
  gtk_target_list_add (list, GDK_TARGET_STRING, 0, info);  
  gtk_target_list_add (list, text_plain_utf8_atom, 0, info);  
  if (!g_get_charset (NULL))
    gtk_target_list_add (list, text_plain_locale_atom, 0, info);  
  gtk_target_list_add (list, text_plain_atom, 0, info);  
}

/**
 * gtk_target_list_add_rich_text_targets:
 * @list: a #GtkTargetList
 * @info: an ID that will be passed back to the application
 * @deserializable: if %TRUE, then deserializable rich text formats
 *                  will be added, serializable formats otherwise.
 * @buffer: a #GtkTextBuffer.
 *
 * Appends the rich text targets registered with
 * gtk_text_buffer_register_serialize_format() or
 * gtk_text_buffer_register_deserialize_format() to the target list. All
 * targets are added with the same @info.
 *
 * Since: 2.10
 **/
void
gtk_target_list_add_rich_text_targets (GtkTargetList  *list,
                                       guint           info,
                                       gboolean        deserializable,
                                       GtkTextBuffer  *buffer)
{
  GdkAtom *atoms;
  gint     n_atoms;
  gint     i;

  g_return_if_fail (list != NULL);
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  if (deserializable)
    atoms = gtk_text_buffer_get_deserialize_formats (buffer, &n_atoms);
  else
    atoms = gtk_text_buffer_get_serialize_formats (buffer, &n_atoms);

  for (i = 0; i < n_atoms; i++)
    gtk_target_list_add (list, atoms[i], 0, info);

  g_free (atoms);
}

/**
 * gtk_target_list_add_image_targets:
 * @list: a #GtkTargetList
 * @info: an ID that will be passed back to the application
 * @writable: whether to add only targets for which GTK+ knows
 *   how to convert a pixbuf into the format
 * 
 * Appends the image targets supported by #GtkSelection to
 * the target list. All targets are added with the same @info.
 * 
 * Since: 2.6
 **/
void 
gtk_target_list_add_image_targets (GtkTargetList *list,
				   guint          info,
				   gboolean       writable)
{
  GSList *formats, *f;
  gchar **mimes, **m;
  GdkAtom atom;

  g_return_if_fail (list != NULL);

  formats = gdk_pixbuf_get_formats ();

  /* Make sure png comes first */
  for (f = formats; f; f = f->next)
    {
      GdkPixbufFormat *fmt = f->data;
      gchar *name; 
 
      name = gdk_pixbuf_format_get_name (fmt);
      if (strcmp (name, "png") == 0)
	{
	  formats = g_slist_delete_link (formats, f);
	  formats = g_slist_prepend (formats, fmt);

	  g_free (name);

	  break;
	}

      g_free (name);
    }  

  for (f = formats; f; f = f->next)
    {
      GdkPixbufFormat *fmt = f->data;

      if (writable && !gdk_pixbuf_format_is_writable (fmt))
	continue;
      
      mimes = gdk_pixbuf_format_get_mime_types (fmt);
      for (m = mimes; *m; m++)
	{
	  atom = gdk_atom_intern (*m, FALSE);
	  gtk_target_list_add (list, atom, 0, info);  
	}
      g_strfreev (mimes);
    }

  g_slist_free (formats);
}

/**
 * gtk_target_list_add_uri_targets:
 * @list: a #GtkTargetList
 * @info: an ID that will be passed back to the application
 * 
 * Appends the URI targets supported by #GtkSelection to
 * the target list. All targets are added with the same @info.
 * 
 * Since: 2.6
 **/
void 
gtk_target_list_add_uri_targets (GtkTargetList *list,
				 guint          info)
{
  g_return_if_fail (list != NULL);
  
  init_atoms ();

  gtk_target_list_add (list, text_uri_list_atom, 0, info);  
}

/**
 * gtk_target_list_add_table:
 * @list: a #GtkTargetList
 * @targets: (array length=ntargets): the table of #GtkTargetEntry
 * @ntargets: number of targets in the table
 * 
 * Prepends a table of #GtkTargetEntry to a target list.
 **/
void               
gtk_target_list_add_table (GtkTargetList        *list,
			   const GtkTargetEntry *targets,
			   guint                 ntargets)
{
  gint i;

  for (i=ntargets-1; i >= 0; i--)
    {
      GtkTargetPair *pair = g_slice_new (GtkTargetPair);
      pair->target = gdk_atom_intern (targets[i].target, FALSE);
      pair->flags = targets[i].flags;
      pair->info = targets[i].info;
      
      list->list = g_list_prepend (list->list, pair);
    }
}

/**
 * gtk_target_list_remove:
 * @list: a #GtkTargetList
 * @target: the interned atom representing the target
 * 
 * Removes a target from a target list.
 **/
void 
gtk_target_list_remove (GtkTargetList *list,
			GdkAtom            target)
{
  GList *tmp_list;

  g_return_if_fail (list != NULL);

  tmp_list = list->list;
  while (tmp_list)
    {
      GtkTargetPair *pair = tmp_list->data;
      
      if (pair->target == target)
	{
	  g_slice_free (GtkTargetPair, pair);

	  list->list = g_list_remove_link (list->list, tmp_list);
	  g_list_free_1 (tmp_list);

	  return;
	}
      
      tmp_list = tmp_list->next;
    }
}

/**
 * gtk_target_list_find:
 * @list: a #GtkTargetList
 * @target: an interned atom representing the target to search for
 * @info: a pointer to the location to store application info for target,
 *        or %NULL
 *
 * Looks up a given target in a #GtkTargetList.
 *
 * Return value: %TRUE if the target was found, otherwise %FALSE
 **/
gboolean
gtk_target_list_find (GtkTargetList *list,
		      GdkAtom        target,
		      guint         *info)
{
  GList *tmp_list;

  g_return_val_if_fail (list != NULL, FALSE);

  tmp_list = list->list;
  while (tmp_list)
    {
      GtkTargetPair *pair = tmp_list->data;

      if (pair->target == target)
	{
          if (info)
            *info = pair->info;

	  return TRUE;
	}

      tmp_list = tmp_list->next;
    }

  return FALSE;
}

/**
 * gtk_target_table_new_from_list:
 * @list: a #GtkTargetList
 * @n_targets: return location for the number ot targets in the table
 *
 * This function creates an #GtkTargetEntry array that contains the
 * same targets as the passed %list. The returned table is newly
 * allocated and should be freed using gtk_target_table_free() when no
 * longer needed.
 *
 * Return value: (array length=n_targets) (transfer full): the new table.
 *
 * Since: 2.10
 **/
GtkTargetEntry *
gtk_target_table_new_from_list (GtkTargetList *list,
                                gint          *n_targets)
{
  GtkTargetEntry *targets;
  GList          *tmp_list;
  gint            i;

  g_return_val_if_fail (list != NULL, NULL);
  g_return_val_if_fail (n_targets != NULL, NULL);

  *n_targets = g_list_length (list->list);
  targets = g_new0 (GtkTargetEntry, *n_targets);

  for (i = 0, tmp_list = list->list;
       i < *n_targets;
       i++, tmp_list = g_list_next (tmp_list))
    {
      GtkTargetPair *pair = tmp_list->data;

      targets[i].target = gdk_atom_name (pair->target);
      targets[i].flags  = pair->flags;
      targets[i].info   = pair->info;
    }

  return targets;
}

/**
 * gtk_target_table_free:
 * @targets: (array length=n_targets): a #GtkTargetEntry array
 * @n_targets: the number of entries in the array
 *
 * This function frees a target table as returned by
 * gtk_target_table_new_from_list()
 *
 * Since: 2.10
 **/
void
gtk_target_table_free (GtkTargetEntry *targets,
                       gint            n_targets)
{
  gint i;

  g_return_if_fail (targets == NULL || n_targets > 0);

  for (i = 0; i < n_targets; i++)
    g_free (targets[i].target);

  g_free (targets);
}

/**
 * gtk_selection_owner_set_for_display:
 * @display: the #Gdkdisplay where the selection is set
 * @widget: (allow-none): new selection owner (a #GdkWidget), or %NULL.
 * @selection: an interned atom representing the selection to claim.
 * @time_: timestamp with which to claim the selection
 *
 * Claim ownership of a given selection for a particular widget, or,
 * if @widget is %NULL, release ownership of the selection.
 *
 * Return value: TRUE if the operation succeeded 
 * 
 * Since: 2.2
 */
gboolean
gtk_selection_owner_set_for_display (GdkDisplay   *display,
				     GtkWidget    *widget,
				     GdkAtom       selection,
				     guint32       time)
{
  GList *tmp_list;
  GtkWidget *old_owner;
  GtkSelectionInfo *selection_info = NULL;
  GdkWindow *window;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);
  g_return_val_if_fail (selection != GDK_NONE, FALSE);
  g_return_val_if_fail (widget == NULL || gtk_widget_get_realized (widget), FALSE);
  g_return_val_if_fail (widget == NULL || gtk_widget_get_display (widget) == display, FALSE);
  
  if (widget == NULL)
    window = NULL;
  else
    window = widget->window;

  tmp_list = current_selections;
  while (tmp_list)
    {
      if (((GtkSelectionInfo *)tmp_list->data)->selection == selection)
	{
	  selection_info = tmp_list->data;
	  break;
	}
      
      tmp_list = tmp_list->next;
    }
  
  if (gdk_selection_owner_set_for_display (display, window, selection, time, TRUE))
    {
      old_owner = NULL;
      
      if (widget == NULL)
	{
	  if (selection_info)
	    {
	      old_owner = selection_info->widget;
	      current_selections = g_list_remove_link (current_selections,
						       tmp_list);
	      g_list_free (tmp_list);
	      g_slice_free (GtkSelectionInfo, selection_info);
	    }
	}
      else
	{
	  if (selection_info == NULL)
	    {
	      selection_info = g_slice_new (GtkSelectionInfo);
	      selection_info->selection = selection;
	      selection_info->widget = widget;
	      selection_info->time = time;
	      selection_info->display = display;
	      current_selections = g_list_prepend (current_selections,
						   selection_info);
	    }
	  else
	    {
	      old_owner = selection_info->widget;
	      selection_info->widget = widget;
	      selection_info->time = time;
	      selection_info->display = display;
	    }
	}
      /* If another widget in the application lost the selection,
       *  send it a GDK_SELECTION_CLEAR event.
       */
      if (old_owner && old_owner != widget)
	{
	  GdkEvent *event = gdk_event_new (GDK_SELECTION_CLEAR);
	  
	  event->selection.window = g_object_ref (old_owner->window);
	  event->selection.selection = selection;
	  event->selection.time = time;
	  
	  gtk_widget_event (old_owner, event);

	  gdk_event_free (event);
	}
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gtk_selection_owner_set:
 * @widget: (allow-none):  a #GtkWidget, or %NULL.
 * @selection:  an interned atom representing the selection to claim
 * @time_: timestamp with which to claim the selection
 * 
 * Claims ownership of a given selection for a particular widget,
 * or, if @widget is %NULL, release ownership of the selection.
 * 
 * Return value: %TRUE if the operation succeeded
 **/
gboolean
gtk_selection_owner_set (GtkWidget *widget,
			 GdkAtom    selection,
			 guint32    time)
{
  GdkDisplay *display;
  
  g_return_val_if_fail (widget == NULL || gtk_widget_get_realized (widget), FALSE);
  g_return_val_if_fail (selection != GDK_NONE, FALSE);

  if (widget)
    display = gtk_widget_get_display (widget);
  else
    {
      GTK_NOTE (MULTIHEAD,
		g_warning ("gtk_selection_owner_set (NULL,...) is not multihead safe"));
		 
      display = gdk_display_get_default ();
    }
  
  return gtk_selection_owner_set_for_display (display, widget,
					      selection, time);
}

typedef struct _GtkSelectionTargetList GtkSelectionTargetList;

struct _GtkSelectionTargetList {
  GdkAtom selection;
  GtkTargetList *list;
};

static GtkTargetList *
gtk_selection_target_list_get (GtkWidget    *widget,
			       GdkAtom       selection)
{
  GtkSelectionTargetList *sellist;
  GList *tmp_list;
  GList *lists;

  lists = g_object_get_data (G_OBJECT (widget), gtk_selection_handler_key);
  
  tmp_list = lists;
  while (tmp_list)
    {
      sellist = tmp_list->data;
      if (sellist->selection == selection)
	return sellist->list;
      tmp_list = tmp_list->next;
    }

  sellist = g_slice_new (GtkSelectionTargetList);
  sellist->selection = selection;
  sellist->list = gtk_target_list_new (NULL, 0);

  lists = g_list_prepend (lists, sellist);
  g_object_set_data (G_OBJECT (widget), I_(gtk_selection_handler_key), lists);

  return sellist->list;
}

static void
gtk_selection_target_list_remove (GtkWidget    *widget)
{
  GtkSelectionTargetList *sellist;
  GList *tmp_list;
  GList *lists;

  lists = g_object_get_data (G_OBJECT (widget), gtk_selection_handler_key);
  
  tmp_list = lists;
  while (tmp_list)
    {
      sellist = tmp_list->data;

      gtk_target_list_unref (sellist->list);

      g_slice_free (GtkSelectionTargetList, sellist);
      tmp_list = tmp_list->next;
    }

  g_list_free (lists);
  g_object_set_data (G_OBJECT (widget), I_(gtk_selection_handler_key), NULL);
}

/**
 * gtk_selection_clear_targets:
 * @widget:    a #GtkWidget
 * @selection: an atom representing a selection
 *
 * Remove all targets registered for the given selection for the
 * widget.
 **/
void 
gtk_selection_clear_targets (GtkWidget *widget,
			     GdkAtom    selection)
{
  GtkSelectionTargetList *sellist;
  GList *tmp_list;
  GList *lists;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (selection != GDK_NONE);

  lists = g_object_get_data (G_OBJECT (widget), gtk_selection_handler_key);
  
  tmp_list = lists;
  while (tmp_list)
    {
      sellist = tmp_list->data;
      if (sellist->selection == selection)
	{
	  lists = g_list_delete_link (lists, tmp_list);
	  gtk_target_list_unref (sellist->list);
	  g_slice_free (GtkSelectionTargetList, sellist);

	  break;
	}
      
      tmp_list = tmp_list->next;
    }
  
  g_object_set_data (G_OBJECT (widget), I_(gtk_selection_handler_key), lists);
}

/**
 * gtk_selection_add_target:
 * @widget:  a #GtkTarget
 * @selection: the selection
 * @target: target to add.
 * @info: A unsigned integer which will be passed back to the application.
 * 
 * Appends a specified target to the list of supported targets for a 
 * given widget and selection.
 **/
void 
gtk_selection_add_target (GtkWidget	    *widget, 
			  GdkAtom	     selection,
			  GdkAtom	     target,
			  guint              info)
{
  GtkTargetList *list;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (selection != GDK_NONE);

  list = gtk_selection_target_list_get (widget, selection);
  gtk_target_list_add (list, target, 0, info);
#ifdef GDK_WINDOWING_WIN32
  gdk_win32_selection_add_targets (widget->window, selection, 1, &target);
#endif
}

/**
 * gtk_selection_add_targets:
 * @widget: a #GtkWidget
 * @selection: the selection
 * @targets: (array length=ntargets): a table of targets to add
 * @ntargets:  number of entries in @targets
 * 
 * Prepends a table of targets to the list of supported targets
 * for a given widget and selection.
 **/
void 
gtk_selection_add_targets (GtkWidget            *widget, 
			   GdkAtom               selection,
			   const GtkTargetEntry *targets,
			   guint                 ntargets)
{
  GtkTargetList *list;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (selection != GDK_NONE);
  g_return_if_fail (targets != NULL);
  
  list = gtk_selection_target_list_get (widget, selection);
  gtk_target_list_add_table (list, targets, ntargets);

#ifdef GDK_WINDOWING_WIN32
  {
    int i;
    GdkAtom *atoms = g_new (GdkAtom, ntargets);

    for (i = 0; i < ntargets; ++i)
      atoms[i] = gdk_atom_intern (targets[i].target, FALSE);
    gdk_win32_selection_add_targets (widget->window, selection, ntargets, atoms);
    g_free (atoms);
  }
#endif
}


/**
 * gtk_selection_remove_all:
 * @widget: a #GtkWidget 
 * 
 * Removes all handlers and unsets ownership of all 
 * selections for a widget. Called when widget is being
 * destroyed. This function will not generally be
 * called by applications.
 **/
void
gtk_selection_remove_all (GtkWidget *widget)
{
  GList *tmp_list;
  GList *next;
  GtkSelectionInfo *selection_info;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  /* Remove pending requests/incrs for this widget */
  
  tmp_list = current_retrievals;
  while (tmp_list)
    {
      next = tmp_list->next;
      if (((GtkRetrievalInfo *)tmp_list->data)->widget == widget)
	{
	  current_retrievals = g_list_remove_link (current_retrievals, 
						   tmp_list);
	  /* structure will be freed in timeout */
	  g_list_free (tmp_list);
	}
      tmp_list = next;
    }
  
  /* Disclaim ownership of any selections */
  
  tmp_list = current_selections;
  while (tmp_list)
    {
      next = tmp_list->next;
      selection_info = (GtkSelectionInfo *)tmp_list->data;
      
      if (selection_info->widget == widget)
	{	
	  gdk_selection_owner_set_for_display (selection_info->display,
					       NULL, 
					       selection_info->selection,
				               GDK_CURRENT_TIME, FALSE);
	  current_selections = g_list_remove_link (current_selections,
						   tmp_list);
	  g_list_free (tmp_list);
	  g_slice_free (GtkSelectionInfo, selection_info);
	}
      
      tmp_list = next;
    }

  /* Remove all selection lists */
  gtk_selection_target_list_remove (widget);
}


/**
 * gtk_selection_convert:
 * @widget: The widget which acts as requestor
 * @selection: Which selection to get
 * @target: Form of information desired (e.g., STRING)
 * @time_: Time of request (usually of triggering event)
       In emergency, you could use #GDK_CURRENT_TIME
 * 
 * Requests the contents of a selection. When received, 
 * a "selection-received" signal will be generated.
 * 
 * Return value: %TRUE if requested succeeded. %FALSE if we could not process
 *          request. (e.g., there was already a request in process for
 *          this widget).
 **/
gboolean
gtk_selection_convert (GtkWidget *widget, 
		       GdkAtom	  selection, 
		       GdkAtom	  target,
		       guint32	  time_)
{
  GtkRetrievalInfo *info;
  GList *tmp_list;
  GdkWindow *owner_window;
  GdkDisplay *display;
  
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (selection != GDK_NONE, FALSE);
  
  if (initialize)
    gtk_selection_init ();
  
  if (!gtk_widget_get_realized (widget))
    gtk_widget_realize (widget);
  
  /* Check to see if there are already any retrievals in progress for
     this widget. If we changed GDK to use the selection for the 
     window property in which to store the retrieved information, then
     we could support multiple retrievals for different selections.
     This might be useful for DND. */
  
  tmp_list = current_retrievals;
  while (tmp_list)
    {
      info = (GtkRetrievalInfo *)tmp_list->data;
      if (info->widget == widget)
	return FALSE;
      tmp_list = tmp_list->next;
    }
  
  info = g_slice_new (GtkRetrievalInfo);
  
  info->widget = widget;
  info->selection = selection;
  info->target = target;
  info->idle_time = 0;
  info->buffer = NULL;
  info->offset = -1;
  
  /* Check if this process has current owner. If so, call handler
     procedure directly to avoid deadlocks with INCR. */

  display = gtk_widget_get_display (widget);
  owner_window = gdk_selection_owner_get_for_display (display, selection);
  
  if (owner_window != NULL)
    {
      GtkWidget *owner_widget;
      gpointer owner_widget_ptr;
      GtkSelectionData selection_data;
      
      selection_data.selection = selection;
      selection_data.target = target;
      selection_data.data = NULL;
      selection_data.length = -1;
      selection_data.display = display;
      
      gdk_window_get_user_data (owner_window, &owner_widget_ptr);
      owner_widget = owner_widget_ptr;
      
      if (owner_widget != NULL)
	{
	  gtk_selection_invoke_handler (owner_widget, 
					&selection_data,
					time_);
	  
	  gtk_selection_retrieval_report (info,
					  selection_data.type, 
					  selection_data.format,
					  selection_data.data,
					  selection_data.length,
					  time_);
	  
	  g_free (selection_data.data);
          selection_data.data = NULL;
          selection_data.length = -1;
	  
	  g_slice_free (GtkRetrievalInfo, info);
	  return TRUE;
	}
    }
  
  /* Otherwise, we need to go through X */
  
  current_retrievals = g_list_append (current_retrievals, info);
  gdk_selection_convert (widget->window, selection, target, time_);
  gdk_threads_add_timeout (1000,
      (GSourceFunc) gtk_selection_retrieval_timeout, info);
  
  return TRUE;
}

/**
 * gtk_selection_data_get_selection:
 * @selection_data: a pointer to a #GtkSelectionData structure.
 *
 * Retrieves the selection #GdkAtom of the selection data.
 *
 * Returns: (transfer none): the selection #GdkAtom of the selection data.
 *
 * Since: 2.16
 **/
GdkAtom
gtk_selection_data_get_selection (GtkSelectionData *selection_data)
{
  g_return_val_if_fail (selection_data != NULL, 0);

  return selection_data->selection;
}

/**
 * gtk_selection_data_get_target:
 * @selection_data: a pointer to a #GtkSelectionData structure.
 *
 * Retrieves the target of the selection.
 *
 * Returns: (transfer none): the target of the selection.
 *
 * Since: 2.14
 **/
GdkAtom
gtk_selection_data_get_target (GtkSelectionData *selection_data)
{
  g_return_val_if_fail (selection_data != NULL, 0);

  return selection_data->target;
}

/**
 * gtk_selection_data_get_data_type:
 * @selection_data: a pointer to a #GtkSelectionData structure.
 *
 * Retrieves the data type of the selection.
 *
 * Returns: (transfer none): the data type of the selection.
 *
 * Since: 2.14
 **/
GdkAtom
gtk_selection_data_get_data_type (GtkSelectionData *selection_data)
{
  g_return_val_if_fail (selection_data != NULL, 0);

  return selection_data->type;
}

/**
 * gtk_selection_data_get_format:
 * @selection_data: a pointer to a #GtkSelectionData structure.
 *
 * Retrieves the format of the selection.
 *
 * Returns: the format of the selection.
 *
 * Since: 2.14
 **/
gint
gtk_selection_data_get_format (GtkSelectionData *selection_data)
{
  g_return_val_if_fail (selection_data != NULL, 0);

  return selection_data->format;
}

/**
 * gtk_selection_data_get_data:
 * @selection_data: a pointer to a #GtkSelectionData structure.
 *
 * Retrieves the raw data of the selection.
 *
 * Returns: the raw data of the selection.
 *
 * Since: 2.14
 **/
const guchar*
gtk_selection_data_get_data (GtkSelectionData *selection_data)
{
  g_return_val_if_fail (selection_data != NULL, NULL);

  return selection_data->data;
}

/**
 * gtk_selection_data_get_length:
 * @selection_data: a pointer to a #GtkSelectionData structure.
 *
 * Retrieves the length of the raw data of the selection.
 *
 * Returns: the length of the data of the selection.
 *
 * Since: 2.14
 */
gint
gtk_selection_data_get_length (GtkSelectionData *selection_data)
{
  g_return_val_if_fail (selection_data != NULL, -1);

  return selection_data->length;
}

/**
 * gtk_selection_data_get_display:
 * @selection_data: a pointer to a #GtkSelectionData structure.
 *
 * Retrieves the display of the selection.
 *
 * Returns: (transfer none): the display of the selection.
 *
 * Since: 2.14
 **/
GdkDisplay *
gtk_selection_data_get_display (GtkSelectionData *selection_data)
{
  g_return_val_if_fail (selection_data != NULL, NULL);

  return selection_data->display;
}

/**
 * gtk_selection_data_set:
 * @selection_data: a pointer to a #GtkSelectionData structure.
 * @type: the type of selection data
 * @format: format (number of bits in a unit)
 * @data: (array length=length): pointer to the data (will be copied)
 * @length: length of the data
 * 
 * Stores new data into a #GtkSelectionData object. Should
 * <emphasis>only</emphasis> be called from a selection handler callback.
 * Zero-terminates the stored data.
 **/
void 
gtk_selection_data_set (GtkSelectionData *selection_data,
			GdkAtom		  type,
			gint		  format,
			const guchar	 *data,
			gint		  length)
{
  g_return_if_fail (selection_data != NULL);

  g_free (selection_data->data);
  
  selection_data->type = type;
  selection_data->format = format;
  
  if (data)
    {
      selection_data->data = g_new (guchar, length+1);
      memcpy (selection_data->data, data, length);
      selection_data->data[length] = 0;
    }
  else
    {
      g_return_if_fail (length <= 0);
      
      if (length < 0)
	selection_data->data = NULL;
      else
	selection_data->data = (guchar *) g_strdup ("");
    }
  
  selection_data->length = length;
}

static gboolean
selection_set_string (GtkSelectionData *selection_data,
		      const gchar      *str,
		      gint              len)
{
  gchar *tmp = g_strndup (str, len);
  gchar *latin1 = gdk_utf8_to_string_target (tmp);
  g_free (tmp);
  
  if (latin1)
    {
      gtk_selection_data_set (selection_data,
			      GDK_SELECTION_TYPE_STRING,
			      8, (guchar *) latin1, strlen (latin1));
      g_free (latin1);
      
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
selection_set_compound_text (GtkSelectionData *selection_data,
			     const gchar      *str,
			     gint              len)
{
  gchar *tmp;
  guchar *text;
  GdkAtom encoding;
  gint format;
  gint new_length;
  gboolean result = FALSE;

#ifdef GDK_WINDOWING_X11
  tmp = g_strndup (str, len);
  if (gdk_x11_display_utf8_to_compound_text (selection_data->display, tmp,
                                             &encoding, &format, &text, &new_length))
    {
      gtk_selection_data_set (selection_data, encoding, format, text, new_length);
      gdk_x11_free_compound_text (text);

      result = TRUE;
    }
  g_free (tmp);
#elif defined(GDK_WINDOWING_WIN32) || defined(GDK_WINDOWING_QUARTZ)
  result = FALSE; /* not needed on Win32 or Quartz */
#else
  g_warning ("%s is not implemented", G_STRFUNC);
  result = FALSE;
#endif

  return result;
}

/* Normalize \r and \n into \r\n
 */
static gchar *
normalize_to_crlf (const gchar *str, 
		   gint         len)
{
  GString *result = g_string_sized_new (len);
  const gchar *p = str;
  const gchar *end = str + len;

  while (p < end)
    {
      if (*p == '\n')
	g_string_append_c (result, '\r');

      if (*p == '\r')
	{
	  g_string_append_c (result, *p);
	  p++;
	  if (p == end || *p != '\n')
	    g_string_append_c (result, '\n');
	  if (p == end)
	    break;
	}

      g_string_append_c (result, *p);
      p++;
    }

  return g_string_free (result, FALSE);  
}

/* Normalize \r and \r\n into \n
 */
static gchar *
normalize_to_lf (gchar *str, 
		 gint   len)
{
  GString *result = g_string_sized_new (len);
  const gchar *p = str;

  while (1)
    {
      if (*p == '\r')
	{
	  p++;
	  if (*p != '\n')
	    g_string_append_c (result, '\n');
	}

      if (*p == '\0')
	break;

      g_string_append_c (result, *p);
      p++;
    }

  return g_string_free (result, FALSE);  
}

static gboolean
selection_set_text_plain (GtkSelectionData *selection_data,
			  const gchar      *str,
			  gint              len)
{
  const gchar *charset = NULL;
  gchar *result;
  GError *error = NULL;

  result = normalize_to_crlf (str, len);
  if (selection_data->target == text_plain_atom)
    charset = "ASCII";
  else if (selection_data->target == text_plain_locale_atom)
    g_get_charset (&charset);

  if (charset)
    {
      gchar *tmp = result;
      result = g_convert_with_fallback (tmp, -1, 
					charset, "UTF-8", 
					NULL, NULL, NULL, &error);
      g_free (tmp);
    }

  if (!result)
    {
      g_warning ("Error converting from %s to %s: %s",
		 "UTF-8", charset, error->message);
      g_error_free (error);
      
      return FALSE;
    }
  
  gtk_selection_data_set (selection_data,
			  selection_data->target, 
			  8, (guchar *) result, strlen (result));
  g_free (result);
  
  return TRUE;
}

static guchar *
selection_get_text_plain (GtkSelectionData *selection_data)
{
  const gchar *charset = NULL;
  gchar *str, *result;
  gsize len;
  GError *error = NULL;

  str = g_strdup ((const gchar *) selection_data->data);
  len = selection_data->length;
  
  if (selection_data->type == text_plain_atom)
    charset = "ISO-8859-1";
  else if (selection_data->type == text_plain_locale_atom)
    g_get_charset (&charset);

  if (charset)
    {
      gchar *tmp = str;
      str = g_convert_with_fallback (tmp, len, 
				     "UTF-8", charset,
				     NULL, NULL, &len, &error);
      g_free (tmp);

      if (!str)
	{
	  g_warning ("Error converting from %s to %s: %s",
		     charset, "UTF-8", error->message);
	  g_error_free (error);

	  return NULL;
	}
    }
  else if (!g_utf8_validate (str, -1, NULL))
    {
      g_warning ("Error converting from %s to %s: %s",
		 "text/plain;charset=utf-8", "UTF-8", "invalid UTF-8");
      g_free (str);

      return NULL;
    }

  result = normalize_to_lf (str, len);
  g_free (str);

  return (guchar *) result;
}

/**
 * gtk_selection_data_set_text:
 * @selection_data: a #GtkSelectionData
 * @str: a UTF-8 string
 * @len: the length of @str, or -1 if @str is nul-terminated.
 * 
 * Sets the contents of the selection from a UTF-8 encoded string.
 * The string is converted to the form determined by
 * @selection_data->target.
 * 
 * Return value: %TRUE if the selection was successfully set,
 *   otherwise %FALSE.
 **/
gboolean
gtk_selection_data_set_text (GtkSelectionData     *selection_data,
			     const gchar          *str,
			     gint                  len)
{
  g_return_val_if_fail (selection_data != NULL, FALSE);

  if (len < 0)
    len = strlen (str);
  
  init_atoms ();

  if (selection_data->target == utf8_atom)
    {
      gtk_selection_data_set (selection_data,
			      utf8_atom,
			      8, (guchar *)str, len);
      return TRUE;
    }
  else if (selection_data->target == GDK_TARGET_STRING)
    {
      return selection_set_string (selection_data, str, len);
    }
  else if (selection_data->target == ctext_atom ||
	   selection_data->target == text_atom)
    {
      if (selection_set_compound_text (selection_data, str, len))
	return TRUE;
      else if (selection_data->target == text_atom)
	return selection_set_string (selection_data, str, len);
    }
  else if (selection_data->target == text_plain_atom ||
	   selection_data->target == text_plain_utf8_atom ||
	   selection_data->target == text_plain_locale_atom)
    {
      return selection_set_text_plain (selection_data, str, len);
    }

  return FALSE;
}

/**
 * gtk_selection_data_get_text:
 * @selection_data: a #GtkSelectionData
 * 
 * Gets the contents of the selection data as a UTF-8 string.
 * 
 * Return value: if the selection data contained a recognized
 *   text type and it could be converted to UTF-8, a newly allocated
 *   string containing the converted text, otherwise %NULL.
 *   If the result is non-%NULL it must be freed with g_free().
 **/
guchar *
gtk_selection_data_get_text (GtkSelectionData *selection_data)
{
  guchar *result = NULL;

  g_return_val_if_fail (selection_data != NULL, NULL);

  init_atoms ();
  
  if (selection_data->length >= 0 &&
      (selection_data->type == GDK_TARGET_STRING ||
       selection_data->type == ctext_atom ||
       selection_data->type == utf8_atom))
    {
      gchar **list;
      gint i;
      gint count = gdk_text_property_to_utf8_list_for_display (selection_data->display,
      							       selection_data->type,
						   	       selection_data->format, 
						               selection_data->data,
						               selection_data->length,
						               &list);
      if (count > 0)
	result = (guchar *) list[0];

      for (i = 1; i < count; i++)
	g_free (list[i]);
      g_free (list);
    }
  else if (selection_data->length >= 0 &&
	   (selection_data->type == text_plain_atom ||
	    selection_data->type == text_plain_utf8_atom ||
	    selection_data->type == text_plain_locale_atom))
    {
      result = selection_get_text_plain (selection_data);
    }

  return result;
}

/**
 * gtk_selection_data_set_pixbuf:
 * @selection_data: a #GtkSelectionData
 * @pixbuf: a #GdkPixbuf
 * 
 * Sets the contents of the selection from a #GdkPixbuf
 * The pixbuf is converted to the form determined by
 * @selection_data->target.
 * 
 * Return value: %TRUE if the selection was successfully set,
 *   otherwise %FALSE.
 *
 * Since: 2.6
 **/
gboolean
gtk_selection_data_set_pixbuf (GtkSelectionData *selection_data,
			       GdkPixbuf        *pixbuf)
{
  GSList *formats, *f;
  gchar **mimes, **m;
  GdkAtom atom;
  gboolean result;
  gchar *str, *type;
  gsize len;

  g_return_val_if_fail (selection_data != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), FALSE);

  formats = gdk_pixbuf_get_formats ();

  for (f = formats; f; f = f->next)
    {
      GdkPixbufFormat *fmt = f->data;

      mimes = gdk_pixbuf_format_get_mime_types (fmt);
      for (m = mimes; *m; m++)
	{
	  atom = gdk_atom_intern (*m, FALSE);
	  if (selection_data->target == atom)
	    {
	      str = NULL;
	      type = gdk_pixbuf_format_get_name (fmt);
	      result = gdk_pixbuf_save_to_buffer (pixbuf, &str, &len,
						  type, NULL,
                                                  ((strcmp (type, "png") == 0) ?
                                                   "compression" : NULL), "2",
                                                  NULL);
	      if (result)
		gtk_selection_data_set (selection_data,
					atom, 8, (guchar *)str, len);
	      g_free (type);
	      g_free (str);
	      g_strfreev (mimes);
	      g_slist_free (formats);

	      return result;
	    }
	}

      g_strfreev (mimes);
    }

  g_slist_free (formats);
 
  return FALSE;
}

/**
 * gtk_selection_data_get_pixbuf:
 * @selection_data: a #GtkSelectionData
 * 
 * Gets the contents of the selection data as a #GdkPixbuf.
 * 
 * Return value: (transfer full): if the selection data contained a recognized
 *   image type and it could be converted to a #GdkPixbuf, a 
 *   newly allocated pixbuf is returned, otherwise %NULL.
 *   If the result is non-%NULL it must be freed with g_object_unref().
 *
 * Since: 2.6
 **/
GdkPixbuf *
gtk_selection_data_get_pixbuf (GtkSelectionData *selection_data)
{
  GdkPixbufLoader *loader;
  GdkPixbuf *result = NULL;

  g_return_val_if_fail (selection_data != NULL, NULL);

  if (selection_data->length > 0)
    {
      loader = gdk_pixbuf_loader_new ();
      
      gdk_pixbuf_loader_write (loader, 
			       selection_data->data,
			       selection_data->length,
			       NULL);
      gdk_pixbuf_loader_close (loader, NULL);
      result = gdk_pixbuf_loader_get_pixbuf (loader);
      
      if (result)
	g_object_ref (result);
      
      g_object_unref (loader);
    }

  return result;
}

/**
 * gtk_selection_data_set_uris:
 * @selection_data: a #GtkSelectionData
 * @uris: (array zero-terminated=1): a %NULL-terminated array of
 *     strings holding URIs
 * 
 * Sets the contents of the selection from a list of URIs.
 * The string is converted to the form determined by
 * @selection_data->target.
 * 
 * Return value: %TRUE if the selection was successfully set,
 *   otherwise %FALSE.
 *
 * Since: 2.6
 **/
gboolean
gtk_selection_data_set_uris (GtkSelectionData  *selection_data,
			     gchar            **uris)
{
  g_return_val_if_fail (selection_data != NULL, FALSE);
  g_return_val_if_fail (uris != NULL, FALSE);

  init_atoms ();

  if (selection_data->target == text_uri_list_atom)
    {
      GString *list;
      gint i;
      gchar *result;
      gsize length;
      
      list = g_string_new (NULL);
      for (i = 0; uris[i]; i++)
	{
	  g_string_append (list, uris[i]);
	  g_string_append (list, "\r\n");
	}

      result = g_convert (list->str, list->len,
			  "ASCII", "UTF-8", 
			  NULL, &length, NULL);
      g_string_free (list, TRUE);
      
      if (result)
	{
	  gtk_selection_data_set (selection_data,
				  text_uri_list_atom,
				  8, (guchar *)result, length);
	  
	  g_free (result);

	  return TRUE;
	}
    }

  return FALSE;
}

/**
 * gtk_selection_data_get_uris:
 * @selection_data: a #GtkSelectionData
 * 
 * Gets the contents of the selection data as array of URIs.
 *
 * Return value:  (array zero-terminated=1) (element-type utf8) (transfer full): if
 *   the selection data contains a list of
 *   URIs, a newly allocated %NULL-terminated string array
 *   containing the URIs, otherwise %NULL. If the result is
 *   non-%NULL it must be freed with g_strfreev().
 *
 * Since: 2.6
 **/
gchar **
gtk_selection_data_get_uris (GtkSelectionData *selection_data)
{
  gchar **result = NULL;

  g_return_val_if_fail (selection_data != NULL, NULL);

  init_atoms ();
  
  if (selection_data->length >= 0 &&
      selection_data->type == text_uri_list_atom)
    {
      gchar **list;
      gint count = gdk_text_property_to_utf8_list_for_display (selection_data->display,
      							       utf8_atom,
						   	       selection_data->format, 
						               selection_data->data,
						               selection_data->length,
						               &list);
      if (count > 0)
	result = g_uri_list_extract_uris (list[0]);
      
      g_strfreev (list);
    }

  return result;
}


/**
 * gtk_selection_data_get_targets:
 * @selection_data: a #GtkSelectionData object
 * @targets: (out) (array length=n_atoms) (transfer container):
 *           location to store an array of targets. The result
 *           stored here must be freed with g_free().
 * @n_atoms: location to store number of items in @targets.
 * 
 * Gets the contents of @selection_data as an array of targets.
 * This can be used to interpret the results of getting
 * the standard TARGETS target that is always supplied for
 * any selection.
 * 
 * Return value: %TRUE if @selection_data contains a valid
 *    array of targets, otherwise %FALSE.
 **/
gboolean
gtk_selection_data_get_targets (GtkSelectionData  *selection_data,
				GdkAtom          **targets,
				gint              *n_atoms)
{
  g_return_val_if_fail (selection_data != NULL, FALSE);

  if (selection_data->length >= 0 &&
      selection_data->format == 32 &&
      selection_data->type == GDK_SELECTION_TYPE_ATOM)
    {
      if (targets)
	*targets = g_memdup (selection_data->data, selection_data->length);
      if (n_atoms)
	*n_atoms = selection_data->length / sizeof (GdkAtom);

      return TRUE;
    }
  else
    {
      if (targets)
	*targets = NULL;
      if (n_atoms)
	*n_atoms = -1;

      return FALSE;
    }
}

/**
 * gtk_targets_include_text:
 * @targets: (array length=n_targets): an array of #GdkAtom<!-- -->s
 * @n_targets: the length of @targets
 * 
 * Determines if any of the targets in @targets can be used to
 * provide text.
 * 
 * Return value: %TRUE if @targets include a suitable target for text,
 *   otherwise %FALSE.
 *
 * Since: 2.10
 **/
gboolean 
gtk_targets_include_text (GdkAtom *targets,
                          gint     n_targets)
{
  gint i;
  gboolean result = FALSE;

  g_return_val_if_fail (targets != NULL || n_targets == 0, FALSE);

  /* Keep in sync with gtk_target_list_add_text_targets()
   */
 
  init_atoms ();
 
  for (i = 0; i < n_targets; i++)
    {
      if (targets[i] == utf8_atom ||
	  targets[i] == text_atom ||
	  targets[i] == GDK_TARGET_STRING ||
	  targets[i] == ctext_atom ||
	  targets[i] == text_plain_atom ||
	  targets[i] == text_plain_utf8_atom ||
	  targets[i] == text_plain_locale_atom)
	{
	  result = TRUE;
	  break;
	}
    }
  
  return result;
}

/**
 * gtk_targets_include_rich_text:
 * @targets: (array length=n_targets): an array of #GdkAtom<!-- -->s
 * @n_targets: the length of @targets
 * @buffer: a #GtkTextBuffer
 *
 * Determines if any of the targets in @targets can be used to
 * provide rich text.
 *
 * Return value: %TRUE if @targets include a suitable target for rich text,
 *               otherwise %FALSE.
 *
 * Since: 2.10
 **/
gboolean
gtk_targets_include_rich_text (GdkAtom       *targets,
                               gint           n_targets,
                               GtkTextBuffer *buffer)
{
  GdkAtom *rich_targets;
  gint n_rich_targets;
  gint i, j;
  gboolean result = FALSE;

  g_return_val_if_fail (targets != NULL || n_targets == 0, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);

  init_atoms ();

  rich_targets = gtk_text_buffer_get_deserialize_formats (buffer,
                                                          &n_rich_targets);

  for (i = 0; i < n_targets; i++)
    {
      for (j = 0; j < n_rich_targets; j++)
        {
          if (targets[i] == rich_targets[j])
            {
              result = TRUE;
              goto done;
            }
        }
    }

 done:
  g_free (rich_targets);

  return result;
}

/**
 * gtk_selection_data_targets_include_text:
 * @selection_data: a #GtkSelectionData object
 * 
 * Given a #GtkSelectionData object holding a list of targets,
 * determines if any of the targets in @targets can be used to
 * provide text.
 * 
 * Return value: %TRUE if @selection_data holds a list of targets,
 *   and a suitable target for text is included, otherwise %FALSE.
 **/
gboolean
gtk_selection_data_targets_include_text (GtkSelectionData *selection_data)
{
  GdkAtom *targets;
  gint n_targets;
  gboolean result = FALSE;

  g_return_val_if_fail (selection_data != NULL, FALSE);

  init_atoms ();

  if (gtk_selection_data_get_targets (selection_data, &targets, &n_targets))
    {
      result = gtk_targets_include_text (targets, n_targets);
      g_free (targets);
    }

  return result;
}

/**
 * gtk_selection_data_targets_include_rich_text:
 * @selection_data: a #GtkSelectionData object
 * @buffer: a #GtkTextBuffer
 *
 * Given a #GtkSelectionData object holding a list of targets,
 * determines if any of the targets in @targets can be used to
 * provide rich text.
 *
 * Return value: %TRUE if @selection_data holds a list of targets,
 *               and a suitable target for rich text is included,
 *               otherwise %FALSE.
 *
 * Since: 2.10
 **/
gboolean
gtk_selection_data_targets_include_rich_text (GtkSelectionData *selection_data,
                                              GtkTextBuffer    *buffer)
{
  GdkAtom *targets;
  gint n_targets;
  gboolean result = FALSE;

  g_return_val_if_fail (selection_data != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);

  init_atoms ();

  if (gtk_selection_data_get_targets (selection_data, &targets, &n_targets))
    {
      result = gtk_targets_include_rich_text (targets, n_targets, buffer);
      g_free (targets);
    }

  return result;
}

/**
 * gtk_targets_include_image:
 * @targets: (array length=n_targets): an array of #GdkAtom<!-- -->s
 * @n_targets: the length of @targets
 * @writable: whether to accept only targets for which GTK+ knows
 *   how to convert a pixbuf into the format
 * 
 * Determines if any of the targets in @targets can be used to
 * provide a #GdkPixbuf.
 * 
 * Return value: %TRUE if @targets include a suitable target for images,
 *   otherwise %FALSE.
 *
 * Since: 2.10
 **/
gboolean 
gtk_targets_include_image (GdkAtom *targets,
			   gint     n_targets,
			   gboolean writable)
{
  GtkTargetList *list;
  GList *l;
  gint i;
  gboolean result = FALSE;

  g_return_val_if_fail (targets != NULL || n_targets == 0, FALSE);

  list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_image_targets (list, 0, writable);
  for (i = 0; i < n_targets && !result; i++)
    {
      for (l = list->list; l; l = l->next)
	{
	  GtkTargetPair *pair = (GtkTargetPair *)l->data;
	  if (pair->target == targets[i])
	    {
	      result = TRUE;
	      break;
	    }
	}
    }
  gtk_target_list_unref (list);

  return result;
}
				    
/**
 * gtk_selection_data_targets_include_image:
 * @selection_data: a #GtkSelectionData object
 * @writable: whether to accept only targets for which GTK+ knows
 *   how to convert a pixbuf into the format
 * 
 * Given a #GtkSelectionData object holding a list of targets,
 * determines if any of the targets in @targets can be used to
 * provide a #GdkPixbuf.
 * 
 * Return value: %TRUE if @selection_data holds a list of targets,
 *   and a suitable target for images is included, otherwise %FALSE.
 *
 * Since: 2.6
 **/
gboolean 
gtk_selection_data_targets_include_image (GtkSelectionData *selection_data,
					  gboolean          writable)
{
  GdkAtom *targets;
  gint n_targets;
  gboolean result = FALSE;

  g_return_val_if_fail (selection_data != NULL, FALSE);

  init_atoms ();

  if (gtk_selection_data_get_targets (selection_data, &targets, &n_targets))
    {
      result = gtk_targets_include_image (targets, n_targets, writable);
      g_free (targets);
    }

  return result;
}

/**
 * gtk_targets_include_uri:
 * @targets: (array length=n_targets): an array of #GdkAtom<!-- -->s
 * @n_targets: the length of @targets
 * 
 * Determines if any of the targets in @targets can be used to
 * provide an uri list.
 * 
 * Return value: %TRUE if @targets include a suitable target for uri lists,
 *   otherwise %FALSE.
 *
 * Since: 2.10
 **/
gboolean 
gtk_targets_include_uri (GdkAtom *targets,
			 gint     n_targets)
{
  gint i;
  gboolean result = FALSE;

  g_return_val_if_fail (targets != NULL || n_targets == 0, FALSE);

  /* Keep in sync with gtk_target_list_add_uri_targets()
   */

  init_atoms ();

  for (i = 0; i < n_targets; i++)
    {
      if (targets[i] == text_uri_list_atom)
	{
	  result = TRUE;
	  break;
	}
    }
  
  return result;
}

/**
 * gtk_selection_data_targets_include_uri:
 * @selection_data: a #GtkSelectionData object
 * 
 * Given a #GtkSelectionData object holding a list of targets,
 * determines if any of the targets in @targets can be used to
 * provide a list or URIs.
 * 
 * Return value: %TRUE if @selection_data holds a list of targets,
 *   and a suitable target for URI lists is included, otherwise %FALSE.
 *
 * Since: 2.10
 **/
gboolean
gtk_selection_data_targets_include_uri (GtkSelectionData *selection_data)
{
  GdkAtom *targets;
  gint n_targets;
  gboolean result = FALSE;

  g_return_val_if_fail (selection_data != NULL, FALSE);

  init_atoms ();

  if (gtk_selection_data_get_targets (selection_data, &targets, &n_targets))
    {
      result = gtk_targets_include_uri (targets, n_targets);
      g_free (targets);
    }

  return result;
}

	  
/*************************************************************
 * gtk_selection_init:
 *     Initialize local variables
 *   arguments:
 *     
 *   results:
 *************************************************************/

static void
gtk_selection_init (void)
{
  gtk_selection_atoms[INCR] = gdk_atom_intern_static_string ("INCR");
  gtk_selection_atoms[MULTIPLE] = gdk_atom_intern_static_string ("MULTIPLE");
  gtk_selection_atoms[TIMESTAMP] = gdk_atom_intern_static_string ("TIMESTAMP");
  gtk_selection_atoms[TARGETS] = gdk_atom_intern_static_string ("TARGETS");
  gtk_selection_atoms[SAVE_TARGETS] = gdk_atom_intern_static_string ("SAVE_TARGETS");

  initialize = FALSE;
}

/**
 * gtk_selection_clear:
 * @widget: a #GtkWidget
 * @event: the event
 * 
 * The default handler for the #GtkWidget::selection-clear-event
 * signal. 
 * 
 * Return value: %TRUE if the event was handled, otherwise false
 * 
 * Since: 2.2
 *
 * Deprecated: 2.4: Instead of calling this function, chain up from
 * your selection-clear-event handler. Calling this function
 * from any other context is illegal. 
 **/
gboolean
gtk_selection_clear (GtkWidget         *widget,
		     GdkEventSelection *event)
{
  /* Note that we filter clear events in gdkselection-x11.c, so
   * that we only will get here if the clear event actually
   * represents a change that we didn't do ourself.
   */
  GList *tmp_list;
  GtkSelectionInfo *selection_info = NULL;
  
  tmp_list = current_selections;
  while (tmp_list)
    {
      selection_info = (GtkSelectionInfo *)tmp_list->data;
      
      if ((selection_info->selection == event->selection) &&
	  (selection_info->widget == widget))
	break;
      
      tmp_list = tmp_list->next;
    }
  
  if (tmp_list)
    {
      current_selections = g_list_remove_link (current_selections, tmp_list);
      g_list_free (tmp_list);
      g_slice_free (GtkSelectionInfo, selection_info);
    }
  
  return TRUE;
}


/*************************************************************
 * _gtk_selection_request:
 *     Handler for "selection_request_event" 
 *   arguments:
 *     widget:
 *     event:
 *   results:
 *************************************************************/

gboolean
_gtk_selection_request (GtkWidget *widget,
			GdkEventSelection *event)
{
  GdkDisplay *display = gtk_widget_get_display (widget);
  GtkIncrInfo *info;
  GList *tmp_list;
  int i;
  gulong selection_max_size;

  if (initialize)
    gtk_selection_init ();
  
  selection_max_size = GTK_SELECTION_MAX_SIZE (display);

  /* Check if we own selection */
  
  tmp_list = current_selections;
  while (tmp_list)
    {
      GtkSelectionInfo *selection_info = (GtkSelectionInfo *)tmp_list->data;
      
      if ((selection_info->selection == event->selection) &&
	  (selection_info->widget == widget))
	break;
      
      tmp_list = tmp_list->next;
    }
  
  if (tmp_list == NULL)
    return FALSE;
  
  info = g_slice_new (GtkIncrInfo);

  g_object_ref (widget);
  
  info->selection = event->selection;
  info->num_incrs = 0;

  /* Create GdkWindow structure for the requestor */
#ifdef GDK_WINDOWING_X11
  info->requestor = gdk_x11_window_foreign_new_for_display (display, event->requestor);
#elif defined GDK_WINDOWING_WIN32
  info->requestor = gdk_win32_window_lookup_for_display (display, event->requestor);
  if (!info->requestor)
    info->requestor = gdk_win32_window_foreign_new_for_display (display, event->requestor);
#else
  g_warning ("%s is not implemented", G_STRFUNC);
  info->requestor = NULL;
#endif

  /* Determine conversions we need to perform */
  
  if (event->target == gtk_selection_atoms[MULTIPLE])
    {
      GdkAtom  type;
      guchar  *mult_atoms;
      gint     format;
      gint     length;
      
      mult_atoms = NULL;
      
      gdk_error_trap_push ();
      if (!gdk_property_get (info->requestor, event->property, GDK_NONE, /* AnyPropertyType */
			     0, selection_max_size, FALSE,
			     &type, &format, &length, &mult_atoms))
	{
	  gdk_selection_send_notify_for_display (display,
						 event->requestor, 
						 event->selection,
						 event->target, 
						 GDK_NONE, 
						 event->time);
	  g_free (mult_atoms);
	  g_slice_free (GtkIncrInfo, info);
          gdk_error_trap_pop ();
	  return TRUE;
	}
      gdk_error_trap_pop ();

      /* This is annoying; the ICCCM doesn't specify the property type
       * used for the property contents, so the autoconversion for
       * ATOM / ATOM_PAIR in GDK doesn't work properly.
       */
#ifdef GDK_WINDOWING_X11
      if (type != GDK_SELECTION_TYPE_ATOM &&
	  type != gdk_atom_intern_static_string ("ATOM_PAIR"))
	{
	  info->num_conversions = length / (2*sizeof (glong));
	  info->conversions = g_new (GtkIncrConversion, info->num_conversions);
	  
	  for (i=0; i<info->num_conversions; i++)
	    {
	      info->conversions[i].target = gdk_x11_xatom_to_atom_for_display (display,
									       ((glong *)mult_atoms)[2*i]);
	      info->conversions[i].property = gdk_x11_xatom_to_atom_for_display (display,
										 ((glong *)mult_atoms)[2*i + 1]);
	    }

	  g_free (mult_atoms);
	}
      else
#endif
	{
	  info->num_conversions = length / (2*sizeof (GdkAtom));
	  info->conversions = g_new (GtkIncrConversion, info->num_conversions);
	  
	  for (i=0; i<info->num_conversions; i++)
	    {
	      info->conversions[i].target = ((GdkAtom *)mult_atoms)[2*i];
	      info->conversions[i].property = ((GdkAtom *)mult_atoms)[2*i+1];
	    }

	  g_free (mult_atoms);
	}
    }
  else				/* only a single conversion */
    {
      info->conversions = g_new (GtkIncrConversion, 1);
      info->num_conversions = 1;
      info->conversions[0].target = event->target;
      info->conversions[0].property = event->property;
    }
  
  /* Loop through conversions and determine which of these are big
     enough to require doing them via INCR */
  for (i=0; i<info->num_conversions; i++)
    {
      GtkSelectionData data;
      glong items;
      
      data.selection = event->selection;
      data.target = info->conversions[i].target;
      data.data = NULL;
      data.length = -1;
      data.display = gtk_widget_get_display (widget);
      
#ifdef DEBUG_SELECTION
      g_message ("Selection %ld, target %ld (%s) requested by 0x%x (property = %ld)",
		 event->selection, 
		 info->conversions[i].target,
		 gdk_atom_name (info->conversions[i].target),
		 event->requestor, info->conversions[i].property);
#endif
      
      gtk_selection_invoke_handler (widget, &data, event->time);
      if (data.length < 0)
	{
	  info->conversions[i].property = GDK_NONE;
	  continue;
	}
      
      g_return_val_if_fail ((data.format >= 8) && (data.format % 8 == 0), FALSE);
      
      items = data.length / gtk_selection_bytes_per_item (data.format);
      
      if (data.length > selection_max_size)
	{
	  /* Sending via INCR */
#ifdef DEBUG_SELECTION
	  g_message ("Target larger (%d) than max. request size (%ld), sending incrementally\n",
		     data.length, selection_max_size);
#endif
	  
	  info->conversions[i].offset = 0;
	  info->conversions[i].data = data;
	  info->num_incrs++;
	  
	  gdk_property_change (info->requestor, 
			       info->conversions[i].property,
			       gtk_selection_atoms[INCR],
			       32,
			       GDK_PROP_MODE_REPLACE,
			       (guchar *)&items, 1);
	}
      else
	{
	  info->conversions[i].offset = -1;
	  
	  gdk_property_change (info->requestor, 
			       info->conversions[i].property,
			       data.type,
			       data.format,
			       GDK_PROP_MODE_REPLACE,
			       data.data, items);
	  
	  g_free (data.data);
	}
    }
  
  /* If we have some INCR's, we need to send the rest of the data in
     a callback */
  
  if (info->num_incrs > 0)
    {
      /* FIXME: this could be dangerous if window doesn't still
	 exist */
      
#ifdef DEBUG_SELECTION
      g_message ("Starting INCR...");
#endif
      
      gdk_window_set_events (info->requestor,
			     gdk_window_get_events (info->requestor) |
			     GDK_PROPERTY_CHANGE_MASK);
      current_incrs = g_list_append (current_incrs, info);
      gdk_threads_add_timeout (1000, (GSourceFunc) gtk_selection_incr_timeout, info);
    }
  
  /* If it was a MULTIPLE request, set the property to indicate which
     conversions succeeded */
  if (event->target == gtk_selection_atoms[MULTIPLE])
    {
      GdkAtom *mult_atoms = g_new (GdkAtom, 2 * info->num_conversions);
      for (i = 0; i < info->num_conversions; i++)
	{
	  mult_atoms[2*i] = info->conversions[i].target;
	  mult_atoms[2*i+1] = info->conversions[i].property;
	}
      
      gdk_property_change (info->requestor, event->property,
			   gdk_atom_intern_static_string ("ATOM_PAIR"), 32, 
			   GDK_PROP_MODE_REPLACE,
			   (guchar *)mult_atoms, 2*info->num_conversions);
      g_free (mult_atoms);
    }

  if (info->num_conversions == 1 &&
      info->conversions[0].property == GDK_NONE)
    {
      /* Reject the entire conversion */
      gdk_selection_send_notify_for_display (gtk_widget_get_display (widget),
					     event->requestor, 
					     event->selection, 
					     event->target, 
					     GDK_NONE, 
					     event->time);
    }
  else
    {
      gdk_selection_send_notify_for_display (gtk_widget_get_display (widget),
					     event->requestor, 
					     event->selection,
					     event->target,
					     event->property, 
					     event->time);
    }

  if (info->num_incrs == 0)
    {
      g_free (info->conversions);
      g_slice_free (GtkIncrInfo, info);
    }

  g_object_unref (widget);
  
  return TRUE;
}

/*************************************************************
 * _gtk_selection_incr_event:
 *     Called whenever an PropertyNotify event occurs for an 
 *     GdkWindow with user_data == NULL. These will be notifications
 *     that a window we are sending the selection to via the
 *     INCR protocol has deleted a property and is ready for
 *     more data.
 *
 *   arguments:
 *     window:	the requestor window
 *     event:	the property event structure
 *
 *   results:
 *************************************************************/

gboolean
_gtk_selection_incr_event (GdkWindow	   *window,
			   GdkEventProperty *event)
{
  GList *tmp_list;
  GtkIncrInfo *info = NULL;
  gint num_bytes;
  guchar *buffer;
  gulong selection_max_size;
  
  int i;
  
  if (event->state != GDK_PROPERTY_DELETE)
    return FALSE;
  
#ifdef DEBUG_SELECTION
  g_message ("PropertyDelete, property %ld", event->atom);
#endif

  selection_max_size = GTK_SELECTION_MAX_SIZE (gdk_window_get_display (window));

  /* Now find the appropriate ongoing INCR */
  tmp_list = current_incrs;
  while (tmp_list)
    {
      info = (GtkIncrInfo *)tmp_list->data;
      if (info->requestor == event->window)
	break;
      
      tmp_list = tmp_list->next;
    }
  
  if (tmp_list == NULL)
    return FALSE;
  
  /* Find out which target this is for */
  for (i=0; i<info->num_conversions; i++)
    {
      if (info->conversions[i].property == event->atom &&
	  info->conversions[i].offset != -1)
	{
	  int bytes_per_item;
	  
	  info->idle_time = 0;
	  
	  if (info->conversions[i].offset == -2) /* only the last 0-length
						    piece*/
	    {
	      num_bytes = 0;
	      buffer = NULL;
	    }
	  else
	    {
	      num_bytes = info->conversions[i].data.length -
		info->conversions[i].offset;
	      buffer = info->conversions[i].data.data + 
		info->conversions[i].offset;
	      
	      if (num_bytes > selection_max_size)
		{
		  num_bytes = selection_max_size;
		  info->conversions[i].offset += selection_max_size;
		}
	      else
		info->conversions[i].offset = -2;
	    }
#ifdef DEBUG_SELECTION
	  g_message ("INCR: put %d bytes (offset = %d) into window 0x%lx , property %ld",
		     num_bytes, info->conversions[i].offset, 
		     GDK_WINDOW_XWINDOW(info->requestor), event->atom);
#endif

	  bytes_per_item = gtk_selection_bytes_per_item (info->conversions[i].data.format);
	  gdk_property_change (info->requestor, event->atom,
			       info->conversions[i].data.type,
			       info->conversions[i].data.format,
			       GDK_PROP_MODE_REPLACE,
			       buffer,
			       num_bytes / bytes_per_item);
	  
	  if (info->conversions[i].offset == -2)
	    {
	      g_free (info->conversions[i].data.data);
	      info->conversions[i].data.data = NULL;
	    }
	  
	  if (num_bytes == 0)
	    {
	      info->num_incrs--;
	      info->conversions[i].offset = -1;
	    }
	}
    }
  
  /* Check if we're finished with all the targets */
  
  if (info->num_incrs == 0)
    {
      current_incrs = g_list_remove_link (current_incrs, tmp_list);
      g_list_free (tmp_list);
      /* Let the timeout free it */
    }
  
  return TRUE;
}

/*************************************************************
 * gtk_selection_incr_timeout:
 *     Timeout callback for the sending portion of the INCR
 *     protocol
 *   arguments:
 *     info:	Information about this incr
 *   results:
 *************************************************************/

static gint
gtk_selection_incr_timeout (GtkIncrInfo *info)
{
  GList *tmp_list;
  gboolean retval;

  /* Determine if retrieval has finished by checking if it still in
     list of pending retrievals */
  
  tmp_list = current_incrs;
  while (tmp_list)
    {
      if (info == (GtkIncrInfo *)tmp_list->data)
	break;
      tmp_list = tmp_list->next;
    }
  
  /* If retrieval is finished */
  if (!tmp_list || info->idle_time >= IDLE_ABORT_TIME)
    {
      if (tmp_list && info->idle_time >= IDLE_ABORT_TIME)
	{
	  current_incrs = g_list_remove_link (current_incrs, tmp_list);
	  g_list_free (tmp_list);
	}
      
      g_free (info->conversions);
      /* FIXME: we should check if requestor window is still in use,
	 and if not, remove it? */
      
      g_slice_free (GtkIncrInfo, info);
      
      retval =  FALSE;		/* remove timeout */
    }
  else
    {
      info->idle_time++;
      
      retval = TRUE;		/* timeout will happen again */
    }
  
  return retval;
}

/*************************************************************
 * _gtk_selection_notify:
 *     Handler for "selection-notify-event" signals on windows
 *     where a retrieval is currently in process. The selection
 *     owner has responded to our conversion request.
 *   arguments:
 *     widget:		Widget getting signal
 *     event:		Selection event structure
 *     info:		Information about this retrieval
 *   results:
 *     was event handled?
 *************************************************************/

gboolean
_gtk_selection_notify (GtkWidget	       *widget,
		       GdkEventSelection *event)
{
  GList *tmp_list;
  GtkRetrievalInfo *info = NULL;
  guchar  *buffer = NULL;
  gint length;
  GdkAtom type;
  gint	  format;
  
#ifdef DEBUG_SELECTION
  g_message ("Initial receipt of selection %ld, target %ld (property = %ld)",
	     event->selection, event->target, event->property);
#endif
  
  tmp_list = current_retrievals;
  while (tmp_list)
    {
      info = (GtkRetrievalInfo *)tmp_list->data;
      if (info->widget == widget && info->selection == event->selection)
	break;
      tmp_list = tmp_list->next;
    }
  
  if (!tmp_list)		/* no retrieval in progress */
    return FALSE;

  if (event->property != GDK_NONE)
    length = gdk_selection_property_get (widget->window, &buffer, 
					 &type, &format);
  else
    length = 0; /* silence gcc */
  
  if (event->property == GDK_NONE || buffer == NULL)
    {
      current_retrievals = g_list_remove_link (current_retrievals, tmp_list);
      g_list_free (tmp_list);
      /* structure will be freed in timeout */
      gtk_selection_retrieval_report (info,
				      GDK_NONE, 0, NULL, -1, event->time);
      
      return TRUE;
    }
  
  if (type == gtk_selection_atoms[INCR])
    {
      /* The remainder of the selection will come through PropertyNotify
	 events */

      info->notify_time = event->time;
      info->idle_time = 0;
      info->offset = 0;		/* Mark as OK to proceed */
      gdk_window_set_events (widget->window,
			     gdk_window_get_events (widget->window)
			     | GDK_PROPERTY_CHANGE_MASK);
    }
  else
    {
      /* We don't delete the info structure - that will happen in timeout */
      current_retrievals = g_list_remove_link (current_retrievals, tmp_list);
      g_list_free (tmp_list);
      
      info->offset = length;
      gtk_selection_retrieval_report (info,
				      type, format, 
				      buffer, length, event->time);
    }
  
  gdk_property_delete (widget->window, event->property);
  
  g_free (buffer);
  
  return TRUE;
}

/*************************************************************
 * _gtk_selection_property_notify:
 *     Handler for "property-notify-event" signals on windows
 *     where a retrieval is currently in process. The selection
 *     owner has added more data.
 *   arguments:
 *     widget:		Widget getting signal
 *     event:		Property event structure
 *     info:		Information about this retrieval
 *   results:
 *     was event handled?
 *************************************************************/

gboolean
_gtk_selection_property_notify (GtkWidget	*widget,
				GdkEventProperty *event)
{
  GList *tmp_list;
  GtkRetrievalInfo *info = NULL;
  guchar *new_buffer;
  int length;
  GdkAtom type;
  gint	  format;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

#if defined(GDK_WINDOWING_WIN32) || defined(GDK_WINDOWING_X11)
  if ((event->state != GDK_PROPERTY_NEW_VALUE) ||  /* property was deleted */
      (event->atom != gdk_atom_intern_static_string ("GDK_SELECTION"))) /* not the right property */
#endif
    return FALSE;
  
#ifdef DEBUG_SELECTION
  g_message ("PropertyNewValue, property %ld",
	     event->atom);
#endif
  
  tmp_list = current_retrievals;
  while (tmp_list)
    {
      info = (GtkRetrievalInfo *)tmp_list->data;
      if (info->widget == widget)
	break;
      tmp_list = tmp_list->next;
    }
  
  if (!tmp_list)		/* No retrieval in progress */
    return FALSE;
  
  if (info->offset < 0)		/* We haven't got the SelectionNotify
				   for this retrieval yet */
    return FALSE;
  
  info->idle_time = 0;
  
  length = gdk_selection_property_get (widget->window, &new_buffer, 
				       &type, &format);
  gdk_property_delete (widget->window, event->atom);
  
  /* We could do a lot better efficiency-wise by paying attention to
     what length was sent in the initial INCR transaction, instead of
     doing memory allocation at every step. But its only guaranteed to
     be a _lower bound_ (pretty useless!) */
  
  if (length == 0 || type == GDK_NONE)		/* final zero length portion */
    {
      /* Info structure will be freed in timeout */
      current_retrievals = g_list_remove_link (current_retrievals, tmp_list);
      g_list_free (tmp_list);
      gtk_selection_retrieval_report (info,
				      type, format, 
				      (type == GDK_NONE) ?  NULL : info->buffer,
				      (type == GDK_NONE) ?  -1 : info->offset,
				      info->notify_time);
    }
  else				/* append on newly arrived data */
    {
      if (!info->buffer)
	{
#ifdef DEBUG_SELECTION
	  g_message ("Start - Adding %d bytes at offset 0",
		     length);
#endif
	  info->buffer = new_buffer;
	  info->offset = length;
	}
      else
	{
	  
#ifdef DEBUG_SELECTION
	  g_message ("Appending %d bytes at offset %d",
		     length,info->offset);
#endif
	  /* We copy length+1 bytes to preserve guaranteed null termination */
	  info->buffer = g_realloc (info->buffer, info->offset+length+1);
	  memcpy (info->buffer + info->offset, new_buffer, length+1);
	  info->offset += length;
	  g_free (new_buffer);
	}
    }
  
  return TRUE;
}

/*************************************************************
 * gtk_selection_retrieval_timeout:
 *     Timeout callback while receiving a selection.
 *   arguments:
 *     info:	Information about this retrieval
 *   results:
 *************************************************************/

static gboolean
gtk_selection_retrieval_timeout (GtkRetrievalInfo *info)
{
  GList *tmp_list;
  gboolean retval;

  /* Determine if retrieval has finished by checking if it still in
     list of pending retrievals */
  
  tmp_list = current_retrievals;
  while (tmp_list)
    {
      if (info == (GtkRetrievalInfo *)tmp_list->data)
	break;
      tmp_list = tmp_list->next;
    }
  
  /* If retrieval is finished */
  if (!tmp_list || info->idle_time >= IDLE_ABORT_TIME)
    {
      if (tmp_list && info->idle_time >= IDLE_ABORT_TIME)
	{
	  current_retrievals = g_list_remove_link (current_retrievals, tmp_list);
	  g_list_free (tmp_list);
	  gtk_selection_retrieval_report (info, GDK_NONE, 0, NULL, -1, GDK_CURRENT_TIME);
	}
      
      g_free (info->buffer);
      g_slice_free (GtkRetrievalInfo, info);
      
      retval =  FALSE;		/* remove timeout */
    }
  else
    {
      info->idle_time++;
      
      retval =  TRUE;		/* timeout will happen again */
    }

  return retval;
}

/*************************************************************
 * gtk_selection_retrieval_report:
 *     Emits a "selection-received" signal.
 *   arguments:
 *     info:	  information about the retrieval that completed
 *     buffer:	  buffer containing data (NULL => errror)
 *     time:      timestamp for data in buffer
 *   results:
 *************************************************************/

static void
gtk_selection_retrieval_report (GtkRetrievalInfo *info,
				GdkAtom type, gint format, 
				guchar *buffer, gint length,
				guint32 time)
{
  GtkSelectionData data;
  
  data.selection = info->selection;
  data.target = info->target;
  data.type = type;
  data.format = format;
  
  data.length = length;
  data.data = buffer;
  data.display = gtk_widget_get_display (info->widget);
  
  g_signal_emit_by_name (info->widget,
			 "selection-received", 
			 &data, time);
}

/*************************************************************
 * gtk_selection_invoke_handler:
 *     Finds and invokes handler for specified
 *     widget/selection/target combination, calls 
 *     gtk_selection_default_handler if none exists.
 *
 *   arguments:
 *     widget:	    selection owner
 *     data:	    selection data [INOUT]
 *     time:        time from requeset
 *     
 *   results:
 *     Number of bytes written to buffer, -1 if error
 *************************************************************/

static void
gtk_selection_invoke_handler (GtkWidget	       *widget,
			      GtkSelectionData *data,
			      guint             time)
{
  GtkTargetList *target_list;
  guint info;
  

  g_return_if_fail (widget != NULL);

  target_list = gtk_selection_target_list_get (widget, data->selection);
  if (data->target != gtk_selection_atoms[SAVE_TARGETS] &&
      target_list &&
      gtk_target_list_find (target_list, data->target, &info))
    {
      g_signal_emit_by_name (widget,
			     "selection-get",
			     data,
			     info, time);
    }
  else
    gtk_selection_default_handler (widget, data);
}

/*************************************************************
 * gtk_selection_default_handler:
 *     Handles some default targets that exist for any widget
 *     If it can't fit results into buffer, returns -1. This
 *     won't happen in any conceivable case, since it would
 *     require 1000 selection targets!
 *
 *   arguments:
 *     widget:	    selection owner
 *     data:	    selection data [INOUT]
 *
 *************************************************************/

static void
gtk_selection_default_handler (GtkWidget	*widget,
			       GtkSelectionData *data)
{
  if (data->target == gtk_selection_atoms[TIMESTAMP])
    {
      /* Time which was used to obtain selection */
      GList *tmp_list;
      GtkSelectionInfo *selection_info;
      
      tmp_list = current_selections;
      while (tmp_list)
	{
	  selection_info = (GtkSelectionInfo *)tmp_list->data;
	  if ((selection_info->widget == widget) &&
	      (selection_info->selection == data->selection))
	    {
	      gulong time = selection_info->time;

	      gtk_selection_data_set (data,
				      GDK_SELECTION_TYPE_INTEGER,
				      32,
				      (guchar *)&time,
				      sizeof (time));
	      return;
	    }
	  
	  tmp_list = tmp_list->next;
	}
      
      data->length = -1;
    }
  else if (data->target == gtk_selection_atoms[TARGETS])
    {
      /* List of all targets supported for this widget/selection pair */
      GdkAtom *p;
      guint count;
      GList *tmp_list;
      GtkTargetList *target_list;
      GtkTargetPair *pair;
      
      target_list = gtk_selection_target_list_get (widget,
						   data->selection);
      count = g_list_length (target_list->list) + 3;
      
      data->type = GDK_SELECTION_TYPE_ATOM;
      data->format = 32;
      data->length = count * sizeof (GdkAtom);

      /* selection data is always terminated by a trailing \0
       */
      p = g_malloc (data->length + 1);
      data->data = (guchar *)p;
      data->data[data->length] = '\0';
      
      *p++ = gtk_selection_atoms[TIMESTAMP];
      *p++ = gtk_selection_atoms[TARGETS];
      *p++ = gtk_selection_atoms[MULTIPLE];
      
      tmp_list = target_list->list;
      while (tmp_list)
	{
	  pair = (GtkTargetPair *)tmp_list->data;
	  *p++ = pair->target;
	  
	  tmp_list = tmp_list->next;
	}
    }
  else if (data->target == gtk_selection_atoms[SAVE_TARGETS])
    {
      gtk_selection_data_set (data,
			      gdk_atom_intern_static_string ("NULL"),
			      32, NULL, 0);
    }
  else
    {
      data->length = -1;
    }
}


/**
 * gtk_selection_data_copy:
 * @data: a pointer to a #GtkSelectionData structure.
 * 
 * Makes a copy of a #GtkSelectionData structure and its data.
 * 
 * Return value: a pointer to a copy of @data.
 **/
GtkSelectionData*
gtk_selection_data_copy (GtkSelectionData *data)
{
  GtkSelectionData *new_data;
  
  g_return_val_if_fail (data != NULL, NULL);
  
  new_data = g_slice_new (GtkSelectionData);
  *new_data = *data;

  if (data->data)
    {
      new_data->data = g_malloc (data->length + 1);
      memcpy (new_data->data, data->data, data->length + 1);
    }
  
  return new_data;
}

/**
 * gtk_selection_data_free:
 * @data: a pointer to a #GtkSelectionData structure.
 * 
 * Frees a #GtkSelectionData structure returned from
 * gtk_selection_data_copy().
 **/
void
gtk_selection_data_free (GtkSelectionData *data)
{
  g_return_if_fail (data != NULL);
  
  g_free (data->data);
  
  g_slice_free (GtkSelectionData, data);
}

GType
gtk_selection_data_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    our_type = g_boxed_type_register_static (I_("GtkSelectionData"),
					     (GBoxedCopyFunc) gtk_selection_data_copy,
					     (GBoxedFreeFunc) gtk_selection_data_free);

  return our_type;
}

GType
gtk_target_list_get_type (void)
{
  static GType our_type = 0;

  if (our_type == 0)
    our_type = g_boxed_type_register_static (I_("GtkTargetList"),
					     (GBoxedCopyFunc) gtk_target_list_ref,
					     (GBoxedFreeFunc) gtk_target_list_unref);

  return our_type;
}

static int 
gtk_selection_bytes_per_item (gint format)
{
  switch (format)
    {
    case 8:
      return sizeof (char);
      break;
    case 16:
      return sizeof (short);
      break;
    case 32:
      return sizeof (long);
      break;
    default:
      g_assert_not_reached();
    }
  return 0;
}

#define __GTK_SELECTION_C__
#include "gtkaliasdef.c"
