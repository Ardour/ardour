/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
 * Copyright (C) 2004 Nokia Corporation
 * Copyright (C) 2006-2008 Imendio AB
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
 *
 */

#include "config.h"
#include <string.h>

#include "gdk/gdkquartz.h"
#import <Cocoa/Cocoa.h>

/* NSInteger only exists in Leopard and newer.  This check has to be
 * done after inclusion of the system headers.  If NSInteger has not
 * been defined, we know for sure that we are on 32-bit.
 */
#ifndef NSINTEGER_DEFINED
typedef int NSInteger;
typedef unsigned int NSUInteger;
#endif

#include "gtkclipboard.h"
#include "gtkinvisible.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkintl.h"
#include "gtktextbuffer.h"
#include "gtkquartz.h"
#include "gtkalias.h"

enum {
  OWNER_CHANGE,
  LAST_SIGNAL
};

@interface GtkClipboardOwner : NSObject {
  GtkClipboard *clipboard;
  @public
  gboolean setting_same_owner;
}

@end

typedef struct _GtkClipboardClass GtkClipboardClass;

struct _GtkClipboard
{
  GObject parent_instance;

  NSPasteboard *pasteboard;
  GtkClipboardOwner *owner;
  NSInteger change_count;

  GdkAtom selection;

  GtkClipboardGetFunc get_func;
  GtkClipboardClearFunc clear_func;
  gpointer user_data;
  gboolean have_owner;
  GtkTargetList *target_list;

  gboolean have_selection;
  GdkDisplay *display;

  GdkAtom *cached_targets;
  gint     n_cached_targets;

  guint      notify_signal_id;
  gboolean   storing_selection;
  GMainLoop *store_loop;
  guint      store_timeout;
  gint       n_storable_targets;
  GdkAtom   *storable_targets;
};

struct _GtkClipboardClass
{
  GObjectClass parent_class;

  void (*owner_change) (GtkClipboard        *clipboard,
			GdkEventOwnerChange *event);
};

static void gtk_clipboard_class_init   (GtkClipboardClass   *class);
static void gtk_clipboard_finalize     (GObject             *object);
static void gtk_clipboard_owner_change (GtkClipboard        *clipboard,
					GdkEventOwnerChange *event);

static void          clipboard_unset      (GtkClipboard     *clipboard);
static GtkClipboard *clipboard_peek       (GdkDisplay       *display,
					   GdkAtom           selection,
					   gboolean          only_if_exists);

@implementation GtkClipboardOwner
-(void)pasteboard:(NSPasteboard *)sender provideDataForType:(NSString *)type
{
  GtkSelectionData selection_data;
  guint info;

  if (!clipboard->target_list)
    return;

  memset (&selection_data, 0, sizeof (GtkSelectionData));

  selection_data.selection = clipboard->selection;
  selection_data.target = gdk_quartz_pasteboard_type_to_atom_libgtk_only (type);
  selection_data.display = gdk_display_get_default ();
  selection_data.length = -1;

  if (gtk_target_list_find (clipboard->target_list, selection_data.target, &info))
    {
      clipboard->get_func (clipboard, &selection_data,
                           info,
                           clipboard->user_data);

      if (selection_data.length >= 0)
        _gtk_quartz_set_selection_data_for_pasteboard (clipboard->pasteboard,
                                                       &selection_data);

      g_free (selection_data.data);
    }
}

/*  pasteboardChangedOwner is not called immediately, and it's not called
 *  reliably. It is somehow documented in the apple api docs, but the docs
 *  suck and don't really give clear instructions. Therefore we track
 *  changeCount in several places below and clear the clipboard if it
 *  changed.
 */
- (void)pasteboardChangedOwner:(NSPasteboard *)sender
{
  if (! setting_same_owner)
    clipboard_unset (clipboard);
}

- (id)initWithClipboard:(GtkClipboard *)aClipboard
{
  self = [super init];

  if (self)
    {
      clipboard = aClipboard;
      setting_same_owner = FALSE;
    }

  return self;
}

@end


static const gchar clipboards_owned_key[] = "gtk-clipboards-owned";
static GQuark clipboards_owned_key_id = 0;

static GObjectClass *parent_class;
static guint         clipboard_signals[LAST_SIGNAL] = { 0 };

GType
gtk_clipboard_get_type (void)
{
  static GType clipboard_type = 0;

  if (!clipboard_type)
    {
      const GTypeInfo clipboard_info =
      {
	sizeof (GtkClipboardClass),
	NULL,           /* base_init */
	NULL,           /* base_finalize */
	(GClassInitFunc) gtk_clipboard_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data */
	sizeof (GtkClipboard),
	0,              /* n_preallocs */
	(GInstanceInitFunc) NULL,
      };

      clipboard_type = g_type_register_static (G_TYPE_OBJECT, I_("GtkClipboard"),
					       &clipboard_info, 0);
    }

  return clipboard_type;
}

static void
gtk_clipboard_class_init (GtkClipboardClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  gobject_class->finalize = gtk_clipboard_finalize;

  class->owner_change = gtk_clipboard_owner_change;

  clipboard_signals[OWNER_CHANGE] =
    g_signal_new (I_("owner-change"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkClipboardClass, owner_change),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
gtk_clipboard_finalize (GObject *object)
{
  GtkClipboard *clipboard;
  GSList *clipboards;

  clipboard = GTK_CLIPBOARD (object);

  clipboards = g_object_get_data (G_OBJECT (clipboard->display), "gtk-clipboard-list");
  if (g_slist_index (clipboards, clipboard) >= 0)
    g_warning ("GtkClipboard prematurely finalized");

  clipboard_unset (clipboard);

  clipboards = g_object_get_data (G_OBJECT (clipboard->display), "gtk-clipboard-list");
  clipboards = g_slist_remove (clipboards, clipboard);
  g_object_set_data (G_OBJECT (clipboard->display), I_("gtk-clipboard-list"), clipboards);

  if (clipboard->store_loop && g_main_loop_is_running (clipboard->store_loop))
    g_main_loop_quit (clipboard->store_loop);

  if (clipboard->store_timeout != 0)
    g_source_remove (clipboard->store_timeout);

  g_free (clipboard->storable_targets);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clipboard_display_closed (GdkDisplay   *display,
			  gboolean      is_error,
			  GtkClipboard *clipboard)
{
  GSList *clipboards;

  clipboards = g_object_get_data (G_OBJECT (display), "gtk-clipboard-list");
  g_object_run_dispose (G_OBJECT (clipboard));
  clipboards = g_slist_remove (clipboards, clipboard);
  g_object_set_data (G_OBJECT (display), I_("gtk-clipboard-list"), clipboards);
  g_object_unref (clipboard);
}

GtkClipboard *
gtk_clipboard_get_for_display (GdkDisplay *display,
			       GdkAtom     selection)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (!display->closed, NULL);

  return clipboard_peek (display, selection, FALSE);
}

GtkClipboard *
gtk_clipboard_get (GdkAtom selection)
{
  return gtk_clipboard_get_for_display (gdk_display_get_default (), selection);
}

static void
clipboard_owner_destroyed (gpointer data)
{
  GSList *clipboards = data;
  GSList *tmp_list;

  tmp_list = clipboards;
  while (tmp_list)
    {
      GtkClipboard *clipboard = tmp_list->data;

      clipboard->get_func = NULL;
      clipboard->clear_func = NULL;
      clipboard->user_data = NULL;
      clipboard->have_owner = FALSE;

      if (clipboard->target_list)
        {
          gtk_target_list_unref (clipboard->target_list);
          clipboard->target_list = NULL;
        }

      gtk_clipboard_clear (clipboard);

      tmp_list = tmp_list->next;
    }

  g_slist_free (clipboards);
}

static void
clipboard_add_owner_notify (GtkClipboard *clipboard)
{
  if (!clipboards_owned_key_id)
    clipboards_owned_key_id = g_quark_from_static_string (clipboards_owned_key);

  if (clipboard->have_owner)
    g_object_set_qdata_full (clipboard->user_data, clipboards_owned_key_id,
			     g_slist_prepend (g_object_steal_qdata (clipboard->user_data,
								    clipboards_owned_key_id),
					      clipboard),
			     clipboard_owner_destroyed);
}

static void
clipboard_remove_owner_notify (GtkClipboard *clipboard)
{
  if (clipboard->have_owner)
     g_object_set_qdata_full (clipboard->user_data, clipboards_owned_key_id,
			      g_slist_remove (g_object_steal_qdata (clipboard->user_data,
								    clipboards_owned_key_id),
					      clipboard),
			      clipboard_owner_destroyed);
}

static gboolean
gtk_clipboard_set_contents (GtkClipboard         *clipboard,
			    const GtkTargetEntry *targets,
			    guint                 n_targets,
			    GtkClipboardGetFunc   get_func,
			    GtkClipboardClearFunc clear_func,
			    gpointer              user_data,
			    gboolean              have_owner)
{
  GtkClipboardOwner *owner;
  NSSet *types;
  NSAutoreleasePool *pool;

  if (!(clipboard->have_owner && have_owner) ||
      clipboard->user_data != user_data)
    {
      clipboard_unset (clipboard);

      if (clipboard->get_func)
        {
          /* Calling unset() caused the clipboard contents to be reset!
           * Avoid leaking and return
           */
          if (!(clipboard->have_owner && have_owner) ||
              clipboard->user_data != user_data)
            {
              (*clear_func) (clipboard, user_data);
              return FALSE;
            }
          else
            {
              return TRUE;
            }
        }
    }

  pool = [[NSAutoreleasePool alloc] init];

  types = _gtk_quartz_target_entries_to_pasteboard_types (targets, n_targets);

  /*  call declareTypes before setting the clipboard members because
   *  declareTypes might clear the clipboard
   */
  if (user_data && user_data == clipboard->user_data)
    {
      owner = [clipboard->owner retain];

      owner->setting_same_owner = TRUE;
      clipboard->change_count = [clipboard->pasteboard declareTypes: [types allObjects]
                                                              owner: owner];
      owner->setting_same_owner = FALSE;
    }
  else
    {
      owner = [[GtkClipboardOwner alloc] initWithClipboard:clipboard];

      clipboard->change_count = [clipboard->pasteboard declareTypes: [types allObjects]
                                                              owner: owner];
    }

  [owner release];
  [types release];
  [pool release];

  clipboard->owner = owner;
  clipboard->user_data = user_data;
  clipboard->have_owner = have_owner;
  if (have_owner)
    clipboard_add_owner_notify (clipboard);
  clipboard->get_func = get_func;
  clipboard->clear_func = clear_func;

  if (clipboard->target_list)
    gtk_target_list_unref (clipboard->target_list);
  clipboard->target_list = gtk_target_list_new (targets, n_targets);

  return TRUE;
}

gboolean
gtk_clipboard_set_with_data (GtkClipboard          *clipboard,
			     const GtkTargetEntry  *targets,
			     guint                  n_targets,
			     GtkClipboardGetFunc    get_func,
			     GtkClipboardClearFunc  clear_func,
			     gpointer               user_data)
{
  g_return_val_if_fail (clipboard != NULL, FALSE);
  g_return_val_if_fail (targets != NULL, FALSE);
  g_return_val_if_fail (get_func != NULL, FALSE);

  return gtk_clipboard_set_contents (clipboard, targets, n_targets,
				     get_func, clear_func, user_data,
				     FALSE);
}

gboolean
gtk_clipboard_set_with_owner (GtkClipboard          *clipboard,
			      const GtkTargetEntry  *targets,
			      guint                  n_targets,
			      GtkClipboardGetFunc    get_func,
			      GtkClipboardClearFunc  clear_func,
			      GObject               *owner)
{
  g_return_val_if_fail (clipboard != NULL, FALSE);
  g_return_val_if_fail (targets != NULL, FALSE);
  g_return_val_if_fail (get_func != NULL, FALSE);
  g_return_val_if_fail (G_IS_OBJECT (owner), FALSE);

  return gtk_clipboard_set_contents (clipboard, targets, n_targets,
				     get_func, clear_func, owner,
				     TRUE);
}

GObject *
gtk_clipboard_get_owner (GtkClipboard *clipboard)
{
  g_return_val_if_fail (clipboard != NULL, NULL);

  if (clipboard->change_count < [clipboard->pasteboard changeCount])
    {
      clipboard_unset (clipboard);
      clipboard->change_count = [clipboard->pasteboard changeCount];
    }

  if (clipboard->have_owner)
    return clipboard->user_data;
  else
    return NULL;
}

static void
clipboard_unset (GtkClipboard *clipboard)
{
  GtkClipboardClearFunc old_clear_func;
  gpointer old_data;
  gboolean old_have_owner;
  gint old_n_storable_targets;

  old_clear_func = clipboard->clear_func;
  old_data = clipboard->user_data;
  old_have_owner = clipboard->have_owner;
  old_n_storable_targets = clipboard->n_storable_targets;

  if (old_have_owner)
    {
      clipboard_remove_owner_notify (clipboard);
      clipboard->have_owner = FALSE;
    }

  clipboard->n_storable_targets = -1;
  g_free (clipboard->storable_targets);
  clipboard->storable_targets = NULL;

  clipboard->owner = NULL;
  clipboard->get_func = NULL;
  clipboard->clear_func = NULL;
  clipboard->user_data = NULL;

  if (old_clear_func)
    old_clear_func (clipboard, old_data);

  if (clipboard->target_list)
    {
      gtk_target_list_unref (clipboard->target_list);
      clipboard->target_list = NULL;
    }

  /* If we've transferred the clipboard data to the manager,
   * unref the owner
   */
  if (old_have_owner &&
      old_n_storable_targets != -1)
    g_object_unref (old_data);
}

void
gtk_clipboard_clear (GtkClipboard *clipboard)
{
  clipboard_unset (clipboard);

  [clipboard->pasteboard declareTypes:nil owner:nil];
}

static void
text_get_func (GtkClipboard     *clipboard,
	       GtkSelectionData *selection_data,
	       guint             info,
	       gpointer          data)
{
  gtk_selection_data_set_text (selection_data, data, -1);
}

static void
text_clear_func (GtkClipboard *clipboard,
		 gpointer      data)
{
  g_free (data);
}

void
gtk_clipboard_set_text (GtkClipboard *clipboard,
			const gchar  *text,
			gint          len)
{
  GtkTargetEntry target = { "UTF8_STRING", 0, 0 };

  g_return_if_fail (clipboard != NULL);
  g_return_if_fail (text != NULL);

  if (len < 0)
    len = strlen (text);

  gtk_clipboard_set_with_data (clipboard,
			       &target, 1,
			       text_get_func, text_clear_func,
			       g_strndup (text, len));
  gtk_clipboard_set_can_store (clipboard, NULL, 0);
}


static void
pixbuf_get_func (GtkClipboard     *clipboard,
		 GtkSelectionData *selection_data,
		 guint             info,
		 gpointer          data)
{
  gtk_selection_data_set_pixbuf (selection_data, data);
}

static void
pixbuf_clear_func (GtkClipboard *clipboard,
		   gpointer      data)
{
  g_object_unref (data);
}

void
gtk_clipboard_set_image (GtkClipboard *clipboard,
			 GdkPixbuf    *pixbuf)
{
  GtkTargetList *list;
  GList *l;
  GtkTargetEntry *targets;
  gint n_targets, i;

  g_return_if_fail (clipboard != NULL);
  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

  list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_image_targets (list, 0, TRUE);

  n_targets = g_list_length (list->list);
  targets = g_new0 (GtkTargetEntry, n_targets);
  for (l = list->list, i = 0; l; l = l->next, i++)
    {
      GtkTargetPair *pair = (GtkTargetPair *)l->data;
      targets[i].target = gdk_atom_name (pair->target);
    }

  gtk_clipboard_set_with_data (clipboard,
			       targets, n_targets,
			       pixbuf_get_func, pixbuf_clear_func,
			       g_object_ref (pixbuf));
  gtk_clipboard_set_can_store (clipboard, NULL, 0);

  for (i = 0; i < n_targets; i++)
    g_free (targets[i].target);
  g_free (targets);
  gtk_target_list_unref (list);
}

void
gtk_clipboard_request_contents (GtkClipboard            *clipboard,
				GdkAtom                  target,
				GtkClipboardReceivedFunc callback,
				gpointer                 user_data)
{
  GtkSelectionData *data;

  data = gtk_clipboard_wait_for_contents (clipboard, target);

  callback (clipboard, data, user_data);

  gtk_selection_data_free (data);
}

void
gtk_clipboard_request_text (GtkClipboard                *clipboard,
			    GtkClipboardTextReceivedFunc callback,
			    gpointer                     user_data)
{
  gchar *data = gtk_clipboard_wait_for_text (clipboard);

  callback (clipboard, data, user_data);

  g_free (data);
}

void
gtk_clipboard_request_rich_text (GtkClipboard                    *clipboard,
                                 GtkTextBuffer                   *buffer,
                                 GtkClipboardRichTextReceivedFunc callback,
                                 gpointer                         user_data)
{
  /* FIXME: Implement */
}


guint8 *
gtk_clipboard_wait_for_rich_text (GtkClipboard  *clipboard,
                                  GtkTextBuffer *buffer,
                                  GdkAtom       *format,
                                  gsize         *length)
{
  /* FIXME: Implement */
  return NULL;
}

void
gtk_clipboard_request_image (GtkClipboard                  *clipboard,
			     GtkClipboardImageReceivedFunc  callback,
			     gpointer                       user_data)
{
  GdkPixbuf *pixbuf = gtk_clipboard_wait_for_image (clipboard);

  callback (clipboard, pixbuf, user_data);

  if (pixbuf)
    g_object_unref (pixbuf);
}

void
gtk_clipboard_request_uris (GtkClipboard                *clipboard,
			    GtkClipboardURIReceivedFunc  callback,
			    gpointer                     user_data)
{
  gchar **uris = gtk_clipboard_wait_for_uris (clipboard);

  callback (clipboard, uris, user_data);

  g_strfreev (uris);
}

void
gtk_clipboard_request_targets (GtkClipboard                *clipboard,
			       GtkClipboardTargetsReceivedFunc callback,
			       gpointer                     user_data)
{
  GdkAtom *targets;
  gint n_targets;

  gtk_clipboard_wait_for_targets (clipboard, &targets, &n_targets);

  callback (clipboard, targets, n_targets, user_data);
}


GtkSelectionData *
gtk_clipboard_wait_for_contents (GtkClipboard *clipboard,
				 GdkAtom       target)
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  GtkSelectionData *selection_data = NULL;

  if (clipboard->change_count < [clipboard->pasteboard changeCount])
    {
      clipboard_unset (clipboard);
      clipboard->change_count = [clipboard->pasteboard changeCount];
    }

  if (target == gdk_atom_intern_static_string ("TARGETS"))
    {
      NSArray *types = [clipboard->pasteboard types];
      int i, length;
      GList *atom_list, *l;
      GdkAtom *atoms;

      length = [types count] * sizeof (GdkAtom);

      selection_data = g_slice_new0 (GtkSelectionData);
      selection_data->selection = clipboard->selection;
      selection_data->target = target;
      if (!selection_data->display)
	selection_data->display = gdk_display_get_default ();

      atoms = g_malloc (length);

      atom_list = _gtk_quartz_pasteboard_types_to_atom_list (types);
      for (l = atom_list, i = 0; l ; l = l->next, i++)
	atoms[i] = GDK_POINTER_TO_ATOM (l->data);
      g_list_free (atom_list);

      gtk_selection_data_set (selection_data,
                              GDK_SELECTION_TYPE_ATOM, 32,
                              (guchar *)atoms, length);

      [pool release];

      return selection_data;
    }

  selection_data = _gtk_quartz_get_selection_data_from_pasteboard (clipboard->pasteboard,
								   target,
								   clipboard->selection);

  [pool release];

  return selection_data;
}

gchar *
gtk_clipboard_wait_for_text (GtkClipboard *clipboard)
{
  GtkSelectionData *data;
  gchar *result;

  data = gtk_clipboard_wait_for_contents (clipboard,
					  gdk_atom_intern_static_string ("UTF8_STRING"));

  result = (gchar *)gtk_selection_data_get_text (data);

  gtk_selection_data_free (data);

  return result;
}

GdkPixbuf *
gtk_clipboard_wait_for_image (GtkClipboard *clipboard)
{
  GdkAtom target = gdk_atom_intern_static_string("image/tiff");
  int i;
  GtkSelectionData *data;

  data = gtk_clipboard_wait_for_contents (clipboard, target);

  if (data && data->data)
    {
      GdkPixbuf *pixbuf = gtk_selection_data_get_pixbuf (data);
      gtk_selection_data_free (data);
      return pixbuf;
    }

  return NULL;
}

gchar **
gtk_clipboard_wait_for_uris (GtkClipboard *clipboard)
{
  GtkSelectionData *data;

  data = gtk_clipboard_wait_for_contents (clipboard, gdk_atom_intern_static_string ("text/uri-list"));
  if (data)
    {
      gchar **uris;

      uris = gtk_selection_data_get_uris (data);
      gtk_selection_data_free (data);

      return uris;
    }

  return NULL;
}

GdkDisplay *
gtk_clipboard_get_display (GtkClipboard *clipboard)
{
  g_return_val_if_fail (clipboard != NULL, NULL);

  return clipboard->display;
}

gboolean
gtk_clipboard_wait_is_text_available (GtkClipboard *clipboard)
{
  GtkSelectionData *data;
  gboolean result = FALSE;

  data = gtk_clipboard_wait_for_contents (clipboard, gdk_atom_intern_static_string ("TARGETS"));
  if (data)
    {
      result = gtk_selection_data_targets_include_text (data);
      gtk_selection_data_free (data);
    }

  return result;
}

gboolean
gtk_clipboard_wait_is_rich_text_available (GtkClipboard  *clipboard,
                                           GtkTextBuffer *buffer)
{
  GtkSelectionData *data;
  gboolean result = FALSE;

  g_return_val_if_fail (GTK_IS_CLIPBOARD (clipboard), FALSE);
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);

  data = gtk_clipboard_wait_for_contents (clipboard, gdk_atom_intern_static_string ("TARGETS"));
  if (data)
    {
      result = gtk_selection_data_targets_include_rich_text (data, buffer);
      gtk_selection_data_free (data);
    }

  return result;
}

gboolean
gtk_clipboard_wait_is_image_available (GtkClipboard *clipboard)
{
  GtkSelectionData *data;
  gboolean result = FALSE;

  data = gtk_clipboard_wait_for_contents (clipboard,
					  gdk_atom_intern_static_string ("TARGETS"));
  if (data)
    {
      result = gtk_selection_data_targets_include_image (data, FALSE);
      gtk_selection_data_free (data);
    }

  return result;
}

gboolean
gtk_clipboard_wait_is_uris_available (GtkClipboard *clipboard)
{
  GtkSelectionData *data;
  gboolean result = FALSE;

  data = gtk_clipboard_wait_for_contents (clipboard,
					  gdk_atom_intern_static_string ("TARGETS"));
  if (data)
    {
      result = gtk_selection_data_targets_include_uri (data);
      gtk_selection_data_free (data);
    }

  return result;
}

gboolean
gtk_clipboard_wait_for_targets (GtkClipboard  *clipboard,
				GdkAtom      **targets,
				gint          *n_targets)
{
  GtkSelectionData *data;
  gboolean result = FALSE;

  g_return_val_if_fail (clipboard != NULL, FALSE);

  /* If the display supports change notification we cache targets */
  if (gdk_display_supports_selection_notification (gtk_clipboard_get_display (clipboard)) &&
      clipboard->n_cached_targets != -1)
    {
      if (n_targets)
 	*n_targets = clipboard->n_cached_targets;

      if (targets)
 	*targets = g_memdup (clipboard->cached_targets,
 			     clipboard->n_cached_targets * sizeof (GdkAtom));

       return TRUE;
    }

  if (n_targets)
    *n_targets = 0;

  if (targets)
    *targets = NULL;

  data = gtk_clipboard_wait_for_contents (clipboard, gdk_atom_intern_static_string ("TARGETS"));

  if (data)
    {
      GdkAtom *tmp_targets;
      gint tmp_n_targets;

      result = gtk_selection_data_get_targets (data, &tmp_targets, &tmp_n_targets);

      if (gdk_display_supports_selection_notification (gtk_clipboard_get_display (clipboard)))
 	{
 	  clipboard->n_cached_targets = tmp_n_targets;
 	  clipboard->cached_targets = g_memdup (tmp_targets,
 						tmp_n_targets * sizeof (GdkAtom));
 	}

      if (n_targets)
 	*n_targets = tmp_n_targets;

      if (targets)
 	*targets = tmp_targets;
      else
 	g_free (tmp_targets);

      gtk_selection_data_free (data);
    }

  return result;
}

static GtkClipboard *
clipboard_peek (GdkDisplay *display,
		GdkAtom     selection,
		gboolean    only_if_exists)
{
  GtkClipboard *clipboard = NULL;
  GSList *clipboards;
  GSList *tmp_list;

  if (selection == GDK_NONE)
    selection = GDK_SELECTION_CLIPBOARD;

  clipboards = g_object_get_data (G_OBJECT (display), "gtk-clipboard-list");

  tmp_list = clipboards;
  while (tmp_list)
    {
      clipboard = tmp_list->data;
      if (clipboard->selection == selection)
	break;

      tmp_list = tmp_list->next;
    }

  if (!tmp_list && !only_if_exists)
    {
      NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
      NSString *pasteboard_name;
      clipboard = g_object_new (GTK_TYPE_CLIPBOARD, NULL);

      if (selection == GDK_SELECTION_CLIPBOARD)
	pasteboard_name = NSGeneralPboard;
      else
	{
	  char *atom_string = gdk_atom_name (selection);

	  pasteboard_name = [NSString stringWithFormat:@"_GTK_%@",
			     [NSString stringWithUTF8String:atom_string]];
	  g_free (atom_string);
	}

      clipboard->pasteboard = [NSPasteboard pasteboardWithName:pasteboard_name];

      [pool release];

      clipboard->selection = selection;
      clipboard->display = display;
      clipboard->n_cached_targets = -1;
      clipboard->n_storable_targets = -1;
      clipboards = g_slist_prepend (clipboards, clipboard);
      g_object_set_data (G_OBJECT (display), I_("gtk-clipboard-list"), clipboards);
      g_signal_connect (display, "closed",
			G_CALLBACK (clipboard_display_closed), clipboard);
      gdk_display_request_selection_notification (display, selection);
    }

  return clipboard;
}

static void
gtk_clipboard_owner_change (GtkClipboard        *clipboard,
			    GdkEventOwnerChange *event)
{
  if (clipboard->n_cached_targets != -1)
    {
      clipboard->n_cached_targets = -1;
      g_free (clipboard->cached_targets);
    }
}

gboolean
gtk_clipboard_wait_is_target_available (GtkClipboard *clipboard,
					GdkAtom       target)
{
  GdkAtom *targets;
  gint i, n_targets;
  gboolean retval = FALSE;

  if (!gtk_clipboard_wait_for_targets (clipboard, &targets, &n_targets))
    return FALSE;

  for (i = 0; i < n_targets; i++)
    {
      if (targets[i] == target)
	{
	  retval = TRUE;
	  break;
	}
    }

  g_free (targets);

  return retval;
}

void
_gtk_clipboard_handle_event (GdkEventOwnerChange *event)
{
}

void
gtk_clipboard_set_can_store (GtkClipboard         *clipboard,
 			     const GtkTargetEntry *targets,
 			     gint                  n_targets)
{
  /* FIXME: Implement */
}

void
gtk_clipboard_store (GtkClipboard *clipboard)
{
  int i;
  int n_targets = 0;
  GtkTargetEntry *targets;

  g_return_if_fail (GTK_IS_CLIPBOARD (clipboard));

  if (!clipboard->target_list || !clipboard->get_func)
    return;

  /* We simply store all targets into the OS X clipboard. We should be
   * using the functions gdk_display_supports_clipboard_persistence() and
   * gdk_display_store_clipboard(), but since for OS X the clipboard support
   * was implemented in GTK+ and not through GdkSelections, we do it this
   * way. Doing this properly could be worthwhile to implement in the future.
   */

  targets = gtk_target_table_new_from_list (clipboard->target_list,
                                            &n_targets);
  for (i = 0; i < n_targets; i++)
    {
      GtkSelectionData selection_data;

      /* in each loop iteration, check if the content is still
       * there, because calling get_func() can do anything to
       * the clipboard
       */
      if (!clipboard->target_list || !clipboard->get_func)
        break;

      memset (&selection_data, 0, sizeof (GtkSelectionData));

      selection_data.selection = clipboard->selection;
      selection_data.target = gdk_atom_intern_static_string (targets[i].target);
      selection_data.display = gdk_display_get_default ();
      selection_data.length = -1;

      clipboard->get_func (clipboard, &selection_data,
                           targets[i].info, clipboard->user_data);

      if (selection_data.length >= 0)
        _gtk_quartz_set_selection_data_for_pasteboard (clipboard->pasteboard,
                                                       &selection_data);

      g_free (selection_data.data);
    }

  if (targets)
    gtk_target_table_free (targets, n_targets);
}

void
_gtk_clipboard_store_all (void)
{
  GtkClipboard *clipboard;
  GSList *displays, *list;

  displays = gdk_display_manager_list_displays (gdk_display_manager_get ());

  list = displays;
  while (list)
    {
      GdkDisplay *display = list->data;

      clipboard = clipboard_peek (display, GDK_SELECTION_CLIPBOARD, TRUE);

      if (clipboard)
        gtk_clipboard_store (clipboard);

      list = list->next;
    }
  g_slist_free (displays);
}

#define __GTK_CLIPBOARD_C__
#include "gtkaliasdef.c"
