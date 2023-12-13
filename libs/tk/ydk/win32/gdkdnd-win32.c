/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2001 Archaeopteryx Software Inc.
 * Copyright (C) 1998-2002 Tor Lillqvist
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
#include <string.h>

#include <io.h>
#include <fcntl.h>

/*
 * Comment from the old OLE2 DND code that is being merged in. Note
 * that this comment might not fully reflect reality as the code
 * obviously will have to be modified in this merge. Especially the
 * talk about supporting other than UTF-8 text is bogus, that will not
 * happen.
 *
 * Support for OLE-2 drag and drop added at Archaeopteryx Software, 2001
 * For more information, contact Stephan R.A. Deibel (sdeibel@archaeopteryx.com)
 *
 * Notes on implementation:
 *
 * This is a first pass at OLE2 support. It only supports text and unicode text
 * data types, and file list dnd (which is handled seperately as it predates OLE2
 * both in this implementation and on Windows in general).
 *
 * As such, the data type conversion from gdk selection targets to OLE2 CF_* data
 * type specifiers is partially hardwired. Fixing this is complicated by (a) the
 * fact that the widget's declared selection types aren't accessible in calls here
 * that need to declare the corresponding OLE2 data types, and (b) there isn't a
 * 1-1 correspondence between gdk target types and OLE2 types. The former needs
 * some redesign in gtk dnd (something a gdk/gtk expert should do; I have tried
 * and failed!). As an example of the latter: gdk STRING, TEXT, COMPOUND_TEXT map
 * to CF_TEXT, CF_OEMTEXT, and CF_UNICODETEXT but as a group and with conversions
 * necessary for various combinations. Currently, the code here (and in
 * gdkdnd-win32.c) can handle gdk STRING and TEXT but not COMPOUND_TEXT, and OLE2
 * CF_TEXT and CF_UNICODETEXT but not CF_OEMTEXT. The necessary conversions are
 * supplied by the implementation here.
 *
 * Note that in combination with another hack originated by Archaeopteryx
 * Software, the text conversions here may go to utf-8 unicode as the standard
 * within-gtk target or to single-byte ascii when the USE_ACP_TEXT compilation
 * flag is TRUE. This mode was added to support applications that aren't using
 * utf-8 across the gtk/gdk API but instead use single-byte ascii according to
 * the current Windows code page. See gdkim-win32.c for more info on that.
 *
 */
 
/* The mingw.org compiler does not export GUIDS in it's import library. To work
 * around that, define INITGUID to have the GUIDS declared. */
#if defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR)
#define INITGUID
#endif

#include "gdkdnd.h"
#include "gdkproperty.h"
#include "gdkinternals.h"
#include "gdkprivate-win32.h"

#include <ole2.h>

#include <shlobj.h>
#include <shlguid.h>

#include <gdk/gdk.h>
#include <glib/gstdio.h>

typedef struct _GdkDragContextPrivateWin32 GdkDragContextPrivateWin32;

typedef enum {
  GDK_DRAG_STATUS_DRAG,
  GDK_DRAG_STATUS_MOTION_WAIT,
  GDK_DRAG_STATUS_ACTION_WAIT,
  GDK_DRAG_STATUS_DROP
} GdkDragStatus;

/* Structure that holds information about a drag in progress.
 * this is used on both source and destination sides.
 */
struct _GdkDragContextPrivateWin32 {
  gboolean being_finalized;
  gint ref_count;
  IUnknown *iface;
  DWORD last_key_state;
  POINT last_pt;		/* Coordinates from last event */
  guint drag_status : 4;	/* Current status of drag */
  guint drop_failed : 1;	/* Whether the drop was unsuccessful */
};

#define PRIVATE_DATA(context) ((GdkDragContextPrivateWin32 *) GDK_DRAG_CONTEXT (context)->windowing_data)

static GList *contexts;
static GdkDragContext *current_dest_drag = NULL;

static void gdk_drag_context_init       (GdkDragContext      *dragcontext);
static void gdk_drag_context_class_init (GdkDragContextClass *klass);
static void gdk_drag_context_finalize   (GObject              *object);

static gpointer parent_class = NULL;

static gboolean use_ole2_dnd = FALSE;

G_DEFINE_TYPE (GdkDragContext, gdk_drag_context, G_TYPE_OBJECT)

static void
gdk_drag_context_init (GdkDragContext *dragcontext)
{
  GdkDragContextPrivateWin32 *private;

  private = G_TYPE_INSTANCE_GET_PRIVATE (dragcontext,
					 GDK_TYPE_DRAG_CONTEXT,
					 GdkDragContextPrivateWin32);

  dragcontext->windowing_data = private;

  if (!use_ole2_dnd)
    {
      contexts = g_list_prepend (contexts, dragcontext);
    }
  else
    {
      private->being_finalized = FALSE;
      private->ref_count = 1;
      private->iface = NULL;
    }

  GDK_NOTE (DND, g_print ("gdk_drag_context_init %p\n", dragcontext));
}

static void
gdk_drag_context_class_init (GdkDragContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_drag_context_finalize;

  g_type_class_add_private (object_class, sizeof (GdkDragContextPrivateWin32));
}

static void
gdk_drag_context_finalize (GObject *object)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (object);

  GDK_NOTE (DND, g_print ("gdk_drag_context_finalize %p\n", object));

  g_list_free (context->targets);

  if (context->source_window)
    g_object_unref (context->source_window);

  if (context->dest_window)
    g_object_unref (context->dest_window);

  if (!use_ole2_dnd)
    {
      contexts = g_list_remove (contexts, context);

      if (context == current_dest_drag)
	current_dest_drag = NULL;
    }
  else
    {
      GdkDragContextPrivateWin32 *private = PRIVATE_DATA (context);
      if (private->iface)
	{
	  private->being_finalized = TRUE;
	  private->iface->lpVtbl->Release (private->iface);
	  private->iface = NULL;
	}
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* Drag Contexts */

GdkDragContext *
gdk_drag_context_new (void)
{
  return g_object_new (GDK_TYPE_DRAG_CONTEXT, NULL);
}

void
gdk_drag_context_ref (GdkDragContext *context)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  g_object_ref (context);
}

void
gdk_drag_context_unref (GdkDragContext *context)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  g_object_unref (context);
}

static GdkDragContext *
gdk_drag_context_find (gboolean   is_source,
		       GdkWindow *source,
		       GdkWindow *dest)
{
  GList *tmp_list = contexts;
  GdkDragContext *context;
  GdkDragContextPrivateWin32 *private;

  while (tmp_list)
    {
      context = (GdkDragContext *)tmp_list->data;
      private = PRIVATE_DATA (context);

      if ((!context->is_source == !is_source) &&
	  ((source == NULL) || (context->source_window && (context->source_window == source))) &&
	  ((dest == NULL) || (context->dest_window && (context->dest_window == dest))))
	return context;

      tmp_list = tmp_list->next;
    }

  return NULL;
}

#define PRINT_GUID(guid) \
  g_print ("%.08lx-%.04x-%.04x-%.02x%.02x-%.02x%.02x%.02x%.02x%.02x%.02x", \
	   ((gulong *)  guid)[0], \
	   ((gushort *) guid)[2], \
	   ((gushort *) guid)[3], \
	   ((guchar *)  guid)[8], \
	   ((guchar *)  guid)[9], \
	   ((guchar *)  guid)[10], \
	   ((guchar *)  guid)[11], \
	   ((guchar *)  guid)[12], \
	   ((guchar *)  guid)[13], \
	   ((guchar *)  guid)[14], \
	   ((guchar *)  guid)[15]);


static FORMATETC *formats;
static int nformats;

typedef struct {
  IDropTarget idt;
  GdkDragContext *context;
} target_drag_context;

typedef struct {
  IDropSource ids;
  GdkDragContext *context;
} source_drag_context;

typedef struct {
  IDataObject ido;
  int ref_count;
  GdkDragContext *context;
} data_object;

typedef struct {
  IEnumFORMATETC ief;
  int ref_count;
  int ix;
} enum_formats;

static source_drag_context *pending_src_context = NULL;
static IDataObject *dnd_data = NULL;

static enum_formats *enum_formats_new (void);

/* map windows -> target drag contexts. The table
 * owns a ref to both objects.
 */
static GHashTable* target_ctx_for_window = NULL;

static ULONG STDMETHODCALLTYPE
idroptarget_addref (LPDROPTARGET This)
{
  target_drag_context *ctx = (target_drag_context *) This;
  GdkDragContextPrivateWin32 *private = PRIVATE_DATA (ctx->context);
  int ref_count = ++private->ref_count;

  GDK_NOTE (DND, g_print ("idroptarget_addref %p %d\n", This, ref_count));
  g_object_ref (G_OBJECT (ctx->context));

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
idroptarget_queryinterface (LPDROPTARGET This,
			    REFIID       riid,
			    LPVOID      *ppvObject)
{
  GDK_NOTE (DND, {
      g_print ("idroptarget_queryinterface %p ", This);
      PRINT_GUID (riid);
    });

  *ppvObject = NULL;

  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      GDK_NOTE (DND, g_print ("...IUnknown S_OK\n"));
      idroptarget_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IDropTarget))
    {
      GDK_NOTE (DND, g_print ("...IDropTarget S_OK\n"));
      idroptarget_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else
    {
      GDK_NOTE (DND, g_print ("...E_NOINTERFACE\n"));
      return E_NOINTERFACE;
    }
}

static ULONG STDMETHODCALLTYPE
idroptarget_release (LPDROPTARGET This)
{
  target_drag_context *ctx = (target_drag_context *) This;
  GdkDragContextPrivateWin32 *private = PRIVATE_DATA (ctx->context);
  int ref_count = --private->ref_count;

  GDK_NOTE (DND, g_print ("idroptarget_release %p %d\n", This, ref_count));

  if (!private->being_finalized)
    g_object_unref (G_OBJECT (ctx->context));

  if (ref_count == 0)
    g_free (This);

  return ref_count;
}

#if 0

static GdkAtom
cf_to_atom (CLIPFORMAT cf)
{
  switch (cf)
    {
    case CF_UNICODETEXT:
      return _utf8_string;
    case CF_HDROP:
      return _text_uri_list;
    case CF_DIB:
      return _image_bmp;
    }

  if (cf == _cf_url)
    return _text_uri_list;

  if (cf == _cf_html_format || cf == _cf_text_html)
    return _text_html;

  return GDK_NONE;
}

#endif

static GdkDragAction
get_suggested_action (DWORD grfKeyState)
{
  /* This is the yucky Windows standard: Force link action if both
   * Control and Alt are down, copy if Control is down alone, move if
   * Alt is down alone, or use default of move within the app or copy
   * when origin of the drag is in another app.
   */
  if (grfKeyState & MK_CONTROL && grfKeyState & MK_SHIFT)
    return GDK_ACTION_LINK; /* Link action not supported */
  else if (grfKeyState & MK_CONTROL)
    return GDK_ACTION_COPY;
  else if (grfKeyState & MK_ALT)
    return GDK_ACTION_MOVE;
#if 0 /* Default is always copy for now */
  else if (_dnd_source_state == GDK_WIN32_DND_DRAGGING)
    return GDK_ACTION_MOVE;
#endif
  else
    return GDK_ACTION_COPY;
  /* Any way to determine when to add in DROPEFFECT_SCROLL? */
}

/* Process pending events -- we don't want to service non-GUI events
 * forever so do one iteration and then do more only if there's a
 * pending GDK event.
 */
static void
process_pending_events ()
{
  g_main_context_iteration (NULL, FALSE);
  while (_gdk_event_queue_find_first (_gdk_display))
    g_main_context_iteration (NULL, FALSE);
}

static DWORD
drop_effect_for_action (GdkDragAction action)
{
  switch (action)
    {
    case GDK_ACTION_MOVE:
      return DROPEFFECT_MOVE;
    case GDK_ACTION_LINK:
      return DROPEFFECT_LINK;
    case GDK_ACTION_COPY:
      return DROPEFFECT_COPY;
    default:
      return DROPEFFECT_NONE;
    }
}

static void
dnd_event_put (GdkEventType    type,
	       GdkDragContext *context,
	       const POINTL    pt,
	       gboolean        to_dest_window)
{
  GdkEvent e;
  e.type = type;
  if (to_dest_window)
    e.dnd.window = context->dest_window;
  else
    e.dnd.window = context->source_window;
  e.dnd.send_event = FALSE;
  e.dnd.context = context;
  e.dnd.time = GDK_CURRENT_TIME;
  e.dnd.x_root = pt.x + _gdk_offset_x;
  e.dnd.y_root = pt.x + _gdk_offset_y;

  g_object_ref (e.dnd.context);
  if (e.dnd.window != NULL)
    g_object_ref (e.dnd.window);

  GDK_NOTE (EVENTS, _gdk_win32_print_event (&e));
  gdk_event_put (&e);
}

static HRESULT STDMETHODCALLTYPE
idroptarget_dragenter (LPDROPTARGET This,
		       LPDATAOBJECT pDataObj,
		       DWORD        grfKeyState,
		       POINTL       pt,
		       LPDWORD      pdwEffect)
{
  target_drag_context *ctx = (target_drag_context *) This;

  GDK_NOTE (DND, g_print ("idroptarget_dragenter %p S_OK\n", This));

  ctx->context->suggested_action = get_suggested_action (grfKeyState);
  dnd_event_put (GDK_DRAG_ENTER, ctx->context, pt, TRUE);
  process_pending_events ();
  *pdwEffect = drop_effect_for_action (ctx->context->action);

  /* Assume that target can accept the data: In fact it may fail but
   * we are not really set up to query the target!
   */
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
idroptarget_dragover (LPDROPTARGET This,
		      DWORD        grfKeyState,
		      POINTL       pt,
		      LPDWORD      pdwEffect)
{
  target_drag_context *ctx = (target_drag_context *) This;

  GDK_NOTE (DND, g_print ("idroptarget_dragover %p S_OK\n", This));

  ctx->context->suggested_action = get_suggested_action (grfKeyState);
  dnd_event_put (GDK_DRAG_MOTION, ctx->context, pt, TRUE);
  process_pending_events ();
  *pdwEffect = drop_effect_for_action (ctx->context->action);

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
idroptarget_dragleave (LPDROPTARGET This)
{
  target_drag_context *ctx = (target_drag_context *) This;
  POINTL pt = { 0, 0 };

  GDK_NOTE (DND, g_print ("idroptarget_dragleave %p S_OK\n", This));

  dnd_event_put (GDK_DRAG_LEAVE, ctx->context, pt, TRUE);
  process_pending_events ();

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
idroptarget_drop (LPDROPTARGET This,
		  LPDATAOBJECT pDataObj,
		  DWORD        grfKeyState,
		  POINTL       pt,
		  LPDWORD      pdwEffect)
{
  target_drag_context *ctx = (target_drag_context *) This;

  GDK_NOTE (DND, g_print ("idroptarget_drop %p ", This));

  if (pDataObj == NULL)
    {
      GDK_NOTE (DND, g_print ("E_POINTER\n"));
      return E_POINTER;
    }

  dnd_data = pDataObj;

  ctx->context->suggested_action = get_suggested_action (grfKeyState);
  dnd_event_put (GDK_DROP_START, ctx->context, pt, TRUE);
  process_pending_events ();

  dnd_data = NULL;

  /* Notify OLE of copy or move */
  if (_dnd_target_state != GDK_WIN32_DND_DROPPED)
    *pdwEffect = DROPEFFECT_NONE;
  else
    *pdwEffect = drop_effect_for_action (ctx->context->action);

  GDK_NOTE (DND, g_print ("S_OK\n"));

  return S_OK;
}

static ULONG STDMETHODCALLTYPE
idropsource_addref (LPDROPSOURCE This)
{
  source_drag_context *ctx = (source_drag_context *) This;
  GdkDragContextPrivateWin32 *private = PRIVATE_DATA (ctx->context);
  int ref_count = ++private->ref_count;

  GDK_NOTE (DND, g_print ("idropsource_addref %p %d\n", This, ref_count));
  g_object_ref (G_OBJECT (ctx->context));

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
idropsource_queryinterface (LPDROPSOURCE This,
			    REFIID       riid,
			    LPVOID      *ppvObject)
{
  GDK_NOTE (DND, {
      g_print ("idropsource_queryinterface %p ", This);
      PRINT_GUID (riid);
    });

  *ppvObject = NULL;

  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      GDK_NOTE (DND, g_print ("...IUnknown S_OK\n"));
      idropsource_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IDropSource))
    {
      GDK_NOTE (DND, g_print ("...IDropSource S_OK\n"));
      idropsource_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else
    {
      GDK_NOTE (DND, g_print ("...E_NOINTERFACE\n"));
      return E_NOINTERFACE;
    }
}

static ULONG STDMETHODCALLTYPE
idropsource_release (LPDROPSOURCE This)
{
  source_drag_context *ctx = (source_drag_context *) This;
  GdkDragContextPrivateWin32 *private = PRIVATE_DATA (ctx->context);
  int ref_count = --private->ref_count;

  GDK_NOTE (DND, g_print ("idropsource_release %p %d\n", This, ref_count));

  if (!private->being_finalized)
    g_object_unref (G_OBJECT (ctx->context));

  if (ref_count == 0)
    g_free (This);

  return ref_count;
}

/* Emit GDK events for any changes in mouse events or control key
 * state since the last recorded state. Return true if any events
 * have been emitted and false otherwise.
 */
static gboolean
send_change_events (GdkDragContext *ctx,
		    DWORD           key_state,
		    gboolean        esc_pressed)
{
  GdkDragContextPrivateWin32 *private = PRIVATE_DATA (ctx);
  POINT pt;
  gboolean changed = FALSE;
  HWND hwnd = GDK_WINDOW_HWND (ctx->source_window);
  LPARAM lparam;
  WPARAM wparam;

  if (!API_CALL (GetCursorPos, (&pt)))
    return FALSE;

  if (!API_CALL (ScreenToClient, (hwnd, &pt)))
    return FALSE;

  if (pt.x != private->last_pt.x || pt.y != private->last_pt.y ||
      key_state != private->last_key_state)
    {
      lparam = MAKELPARAM (pt.x, pt.y);
      wparam = key_state;
      if (pt.x != private->last_pt.x || pt.y != private->last_pt.y)
	{
	  GDK_NOTE (DND, g_print ("Sending WM_MOUSEMOVE (%ld,%ld)\n", pt.x, pt.y));
      	  SendMessage (hwnd, WM_MOUSEMOVE, wparam, lparam);
	}

      if ((key_state & MK_LBUTTON) != (private->last_key_state & MK_LBUTTON))
	{
	  if (key_state & MK_LBUTTON)
	    SendMessage (hwnd, WM_LBUTTONDOWN, wparam, lparam);
	  else
	    SendMessage (hwnd, WM_LBUTTONUP, wparam, lparam);
	}
      if ((key_state & MK_MBUTTON) != (private->last_key_state & MK_MBUTTON))
	{
	  if (key_state & MK_MBUTTON)
	    SendMessage (hwnd, WM_MBUTTONDOWN, wparam, lparam);
	  else
	    SendMessage (hwnd, WM_MBUTTONUP, wparam, lparam);
	}
      if ((key_state & MK_RBUTTON) != (private->last_key_state & MK_RBUTTON))
	{
	  if (key_state & MK_RBUTTON)
	    SendMessage (hwnd, WM_RBUTTONDOWN, wparam, lparam);
	  else
	    SendMessage (hwnd, WM_RBUTTONUP, wparam, lparam);
	}
      if ((key_state & MK_CONTROL) != (private->last_key_state & MK_CONTROL))
	{
	  if (key_state & MK_CONTROL)
	    SendMessage (hwnd, WM_KEYDOWN, VK_CONTROL, 0);
	  else
	    SendMessage (hwnd, WM_KEYUP, VK_CONTROL, 0);
	}
      if ((key_state & MK_SHIFT) != (private->last_key_state & MK_SHIFT))
	{
	  if (key_state & MK_CONTROL)
	    SendMessage (hwnd, WM_KEYDOWN, VK_SHIFT, 0);
	  else
	    SendMessage (hwnd, WM_KEYUP, VK_SHIFT, 0);
	}

      changed = TRUE;
      private->last_key_state = key_state;
      private->last_pt = pt;
    }

  if (esc_pressed)
    {
      GDK_NOTE (DND, g_print ("Sending a escape key down message to %p\n", hwnd));
      SendMessage (hwnd, WM_KEYDOWN, VK_ESCAPE, 0);
      changed = TRUE;
    }

  return changed;
}

static HRESULT STDMETHODCALLTYPE
idropsource_querycontinuedrag (LPDROPSOURCE This,
			       BOOL         fEscapePressed,
			       DWORD        grfKeyState)
{
  source_drag_context *ctx = (source_drag_context *) This;

  GDK_NOTE (DND, g_print ("idropsource_querycontinuedrag %p ", This));

  if (send_change_events (ctx->context, grfKeyState, fEscapePressed))
    process_pending_events ();

  if (_dnd_source_state == GDK_WIN32_DND_DROPPED)
    {
      GDK_NOTE (DND, g_print ("DRAGDROP_S_DROP\n"));
      return DRAGDROP_S_DROP;
    }
  else if (_dnd_source_state == GDK_WIN32_DND_NONE)
    {
      GDK_NOTE (DND, g_print ("DRAGDROP_S_CANCEL\n"));
      return DRAGDROP_S_CANCEL;
    }
  else
    {
      GDK_NOTE (DND, g_print ("S_OK\n"));
      return S_OK;
    }
}

static HRESULT STDMETHODCALLTYPE
idropsource_givefeedback (LPDROPSOURCE This,
			  DWORD        dwEffect)
{
  source_drag_context *ctx = (source_drag_context *) This;
  GdkDragAction suggested_action;

  GDK_NOTE (DND, g_print ("idropsource_givefeedback %p DRAGDROP_S_USEDEFAULTCURSORS\n", This));

  if (dwEffect == DROPEFFECT_MOVE)
    suggested_action = GDK_ACTION_MOVE;
  else
    suggested_action = GDK_ACTION_COPY;
  ctx->context->action = suggested_action;

  if (dwEffect == DROPEFFECT_NONE)
    {
      if (ctx->context->dest_window != NULL)
	{
	  g_object_unref (ctx->context->dest_window);
	  ctx->context->dest_window = NULL;
	}
    }
  else
    {
      if (ctx->context->dest_window == NULL)
	ctx->context->dest_window = g_object_ref (_gdk_root);
    }

  return DRAGDROP_S_USEDEFAULTCURSORS;
}

static ULONG STDMETHODCALLTYPE
idataobject_addref (LPDATAOBJECT This)
{
  data_object *dobj = (data_object *) This;
  int ref_count = ++dobj->ref_count;

  GDK_NOTE (DND, g_print ("idataobject_addref %p %d\n", This, ref_count));

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
idataobject_queryinterface (LPDATAOBJECT This,
			    REFIID       riid,
			    LPVOID      *ppvObject)
{
  GDK_NOTE (DND, {
      g_print ("idataobject_queryinterface %p ", This);
      PRINT_GUID (riid);
    });

  *ppvObject = NULL;

  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      GDK_NOTE (DND, g_print ("...IUnknown S_OK\n"));
      idataobject_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IDataObject))
    {
      GDK_NOTE (DND, g_print ("...IDataObject S_OK\n"));
      idataobject_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else
    {
      GDK_NOTE (DND, g_print ("...E_NOINTERFACE\n"));
      return E_NOINTERFACE;
    }
}

static ULONG STDMETHODCALLTYPE
idataobject_release (LPDATAOBJECT This)
{
  data_object *dobj = (data_object *) This;
  int ref_count = --dobj->ref_count;

  GDK_NOTE (DND, g_print ("idataobject_release %p %d\n", This, ref_count));

  if (ref_count == 0)
    g_free (This);

  return ref_count;
}

static HRESULT
query (LPDATAOBJECT This,
       LPFORMATETC  pFormatEtc)
{
  int i;

  if (!pFormatEtc)
    return DV_E_FORMATETC;

  if (pFormatEtc->lindex != -1)
    return DV_E_LINDEX;

  if ((pFormatEtc->tymed & TYMED_HGLOBAL) == 0)
    return DV_E_TYMED;

  if ((pFormatEtc->dwAspect & DVASPECT_CONTENT) == 0)
    return DV_E_DVASPECT;

  for (i = 0; i < nformats; i++)
    if (pFormatEtc->cfFormat == formats[i].cfFormat)
      return S_OK;

  return DV_E_FORMATETC;
}

static FORMATETC *active_pFormatEtc = NULL;
static STGMEDIUM *active_pMedium = NULL;

static HRESULT STDMETHODCALLTYPE
idataobject_getdata (LPDATAOBJECT This,
		     LPFORMATETC  pFormatEtc,
		     LPSTGMEDIUM  pMedium)
{
  data_object *ctx = (data_object *) This;
  GdkAtom target;
  HRESULT hr;
  GdkEvent e;

  GDK_NOTE (DND, g_print ("idataobject_getdata %p %s ",
			  This, _gdk_win32_cf_to_string (pFormatEtc->cfFormat)));

  /* Check whether we can provide requested format */
  hr = query (This, pFormatEtc);
  if (hr != S_OK)
    return hr;

  /* Append a GDK_SELECTION_GET event and then hope the app sets the
   * property associated with the _gdk_ole2_dnd atom
   */

  active_pFormatEtc = pFormatEtc;
  active_pMedium = pMedium;

  target = GDK_TARGET_STRING;

  e.type = GDK_SELECTION_REQUEST;
  e.selection.window = ctx->context->source_window;
  e.selection.send_event = FALSE; /* ??? */
  /* FIXME: Should really both selection and property be _gdk_ole2_dnd? */
  e.selection.selection = _gdk_ole2_dnd;
  /* FIXME: Target? */
  e.selection.target = _utf8_string;
  e.selection.property = _gdk_ole2_dnd;
  e.selection.time = GDK_CURRENT_TIME;

  g_object_ref (e.selection.window);

  GDK_NOTE (EVENTS, _gdk_win32_print_event (&e));
  gdk_event_put (&e);
  process_pending_events ();

  active_pFormatEtc = NULL;
  active_pMedium = NULL;

  if (pMedium->hGlobal == NULL) {
    return E_UNEXPECTED;
  }

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
idataobject_getdatahere (LPDATAOBJECT This,
			 LPFORMATETC  pFormatEtc,
			 LPSTGMEDIUM  pMedium)
{
  GDK_NOTE (DND, g_print ("idataobject_getdatahere %p %s E_UNEXPECTED\n",
			  This, _gdk_win32_cf_to_string (pFormatEtc->cfFormat)));

  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
idataobject_querygetdata (LPDATAOBJECT This,
			  LPFORMATETC  pFormatEtc)
{
  HRESULT hr;

  hr = query (This, pFormatEtc);

#define CASE(x) case x: g_print (#x)
  GDK_NOTE (DND, {
      g_print ("idataobject_querygetdata %p %s \n",
	       This, _gdk_win32_cf_to_string (pFormatEtc->cfFormat));
      switch (hr)
	{
	CASE (DV_E_FORMATETC);
	CASE (DV_E_LINDEX);
	CASE (DV_E_TYMED);
	CASE (DV_E_DVASPECT);
	CASE (S_OK);
	default: g_print ("%#lx", hr);
	}
    });

  return hr;
}

static HRESULT STDMETHODCALLTYPE
idataobject_getcanonicalformatetc (LPDATAOBJECT This,
				   LPFORMATETC  pFormatEtcIn,
				   LPFORMATETC  pFormatEtcOut)
{
  GDK_NOTE (DND, g_print ("idataobject_getcanonicalformatetc %p E_UNEXPECTED\n", This));

  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
idataobject_setdata (LPDATAOBJECT This,
		     LPFORMATETC  pFormatEtc,
		     LPSTGMEDIUM  pMedium,
		     BOOL         fRelease)
{
  GDK_NOTE (DND, g_print ("idataobject_setdata %p %s E_UNEXPECTED\n",
			  This, _gdk_win32_cf_to_string (pFormatEtc->cfFormat)));

  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
idataobject_enumformatetc (LPDATAOBJECT     This,
			   DWORD            dwDirection,
			   LPENUMFORMATETC *ppEnumFormatEtc)
{
  GDK_NOTE (DND, g_print ("idataobject_enumformatetc %p ", This));

  if (dwDirection != DATADIR_GET)
    {
      GDK_NOTE (DND, g_print ("E_NOTIMPL\n"));
      return E_NOTIMPL;
    }

  *ppEnumFormatEtc = &enum_formats_new ()->ief;

  GDK_NOTE (DND, g_print ("%p S_OK\n", *ppEnumFormatEtc));

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
idataobject_dadvise (LPDATAOBJECT This,
		     LPFORMATETC  pFormatetc,
		     DWORD        advf,
		     LPADVISESINK pAdvSink,
		     DWORD       *pdwConnection)
{
  GDK_NOTE (DND, g_print ("idataobject_dadvise %p E_NOTIMPL\n", This));

  return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
idataobject_dunadvise (LPDATAOBJECT This,
		       DWORD         dwConnection)
{
  GDK_NOTE (DND, g_print ("idataobject_dunadvise %p E_NOTIMPL\n", This));

  return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
idataobject_enumdadvise (LPDATAOBJECT    This,
			 LPENUMSTATDATA *ppenumAdvise)
{
  GDK_NOTE (DND, g_print ("idataobject_enumdadvise %p OLE_E_ADVISENOTSUPPORTED\n", This));

  return OLE_E_ADVISENOTSUPPORTED;
}

static ULONG STDMETHODCALLTYPE
ienumformatetc_addref (LPENUMFORMATETC This)
{
  enum_formats *en = (enum_formats *) This;
  int ref_count = ++en->ref_count;

  GDK_NOTE (DND, g_print ("ienumformatetc_addref %p %d\n", This, ref_count));

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_queryinterface (LPENUMFORMATETC This,
			       REFIID          riid,
			       LPVOID         *ppvObject)
{
  GDK_NOTE (DND, {
      g_print ("ienumformatetc_queryinterface %p", This);
      PRINT_GUID (riid);
    });

  *ppvObject = NULL;

  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      GDK_NOTE (DND, g_print ("...IUnknown S_OK\n"));
      ienumformatetc_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IEnumFORMATETC))
    {
      GDK_NOTE (DND, g_print ("...IEnumFORMATETC S_OK\n"));
      ienumformatetc_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else
    {
      GDK_NOTE (DND, g_print ("...E_NOINTERFACE\n"));
      return E_NOINTERFACE;
    }
}

static ULONG STDMETHODCALLTYPE
ienumformatetc_release (LPENUMFORMATETC This)
{
  enum_formats *en = (enum_formats *) This;
  int ref_count = --en->ref_count;

  GDK_NOTE (DND, g_print ("ienumformatetc_release %p %d\n", This, ref_count));

  if (ref_count == 0)
    g_free (This);

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_next (LPENUMFORMATETC This,
		     ULONG	     celt,
		     LPFORMATETC     elts,
		     ULONG	    *nelt)
{
  enum_formats *en = (enum_formats *) This;
  int i, n;

  GDK_NOTE (DND, g_print ("ienumformatetc_next %p %d %ld ", This, en->ix, celt));

  n = 0;
  for (i = 0; i < celt; i++)
    {
      if (en->ix >= nformats)
	break;
      elts[i] = formats[en->ix++];
      n++;
    }

  if (nelt != NULL)
    *nelt = n;

  GDK_NOTE (DND, g_print ("%s\n", (n == celt) ? "S_OK" : "S_FALSE"));

  if (n == celt)
    return S_OK;
  else
    return S_FALSE;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_skip (LPENUMFORMATETC This,
		     ULONG	     celt)
{
  enum_formats *en = (enum_formats *) This;

  GDK_NOTE (DND, g_print ("ienumformatetc_skip %p %d %ld S_OK\n", This, en->ix, celt));

  en->ix += celt;

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_reset (LPENUMFORMATETC This)
{
  enum_formats *en = (enum_formats *) This;

  GDK_NOTE (DND, g_print ("ienumformatetc_reset %p S_OK\n", This));

  en->ix = 0;

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_clone (LPENUMFORMATETC  This,
		      LPENUMFORMATETC *ppEnumFormatEtc)
{
  enum_formats *en = (enum_formats *) This;
  enum_formats *new;

  GDK_NOTE (DND, g_print ("ienumformatetc_clone %p S_OK\n", This));

  new = enum_formats_new ();

  new->ix = en->ix;

  *ppEnumFormatEtc = &new->ief;

  return S_OK;
}

static IDropTargetVtbl idt_vtbl = {
  idroptarget_queryinterface,
  idroptarget_addref,
  idroptarget_release,
  idroptarget_dragenter,
  idroptarget_dragover,
  idroptarget_dragleave,
  idroptarget_drop
};

static IDropSourceVtbl ids_vtbl = {
  idropsource_queryinterface,
  idropsource_addref,
  idropsource_release,
  idropsource_querycontinuedrag,
  idropsource_givefeedback
};

static IDataObjectVtbl ido_vtbl = {
  idataobject_queryinterface,
  idataobject_addref,
  idataobject_release,
  idataobject_getdata,
  idataobject_getdatahere,
  idataobject_querygetdata,
  idataobject_getcanonicalformatetc,
  idataobject_setdata,
  idataobject_enumformatetc,
  idataobject_dadvise,
  idataobject_dunadvise,
  idataobject_enumdadvise
};

static IEnumFORMATETCVtbl ief_vtbl = {
  ienumformatetc_queryinterface,
  ienumformatetc_addref,
  ienumformatetc_release,
  ienumformatetc_next,
  ienumformatetc_skip,
  ienumformatetc_reset,
  ienumformatetc_clone
};


static target_drag_context *
target_context_new (GdkWindow *window)
{
  target_drag_context *result;
  GdkDragContextPrivateWin32 *private;

  result = g_new0 (target_drag_context, 1);

  result->idt.lpVtbl = &idt_vtbl;

  result->context = gdk_drag_context_new ();
  result->context->protocol = GDK_DRAG_PROTO_OLE2;
  result->context->is_source = FALSE;

  result->context->source_window = NULL;

  result->context->dest_window = window;
  g_object_ref (window);

  /* FIXME: result->context->targets? */

  result->context->actions = GDK_ACTION_DEFAULT | GDK_ACTION_COPY | GDK_ACTION_MOVE;
  result->context->suggested_action = GDK_ACTION_MOVE;
  result->context->action = GDK_ACTION_MOVE;

  private = result->context->windowing_data;
  private->iface = (IUnknown *) &result->idt;
  idroptarget_addref (&result->idt);

  GDK_NOTE (DND, g_print ("target_context_new: %p\n", result));

  return result;
}

static source_drag_context *
source_context_new (GdkWindow *window,
		    GList     *targets)
{
  source_drag_context *result;
  GdkDragContextPrivateWin32 *private;

  result = g_new0 (source_drag_context, 1);

  result->ids.lpVtbl = &ids_vtbl;

  result->context = gdk_drag_context_new ();
  result->context->protocol = GDK_DRAG_PROTO_OLE2;
  result->context->is_source = TRUE;

  result->context->source_window = window;
  g_object_ref (window);

  result->context->dest_window = NULL;
  result->context->targets = g_list_copy (targets);

  private = result->context->windowing_data;
  private->iface = (IUnknown *) &result->ids;
  idropsource_addref (&result->ids);

  GDK_NOTE (DND, g_print ("source_context_new: %p\n", result));

  return result;
}

static data_object *
data_object_new (GdkDragContext *context)
{
  data_object *result;

  result = g_new0 (data_object, 1);

  result->ido.lpVtbl = &ido_vtbl;
  result->ref_count = 1;
  result->context = context;

  GDK_NOTE (DND, g_print ("data_object_new: %p\n", result));

  return result;
}

static enum_formats *
enum_formats_new (void)
{
  enum_formats *result;

  result = g_new0 (enum_formats, 1);

  result->ief.lpVtbl = &ief_vtbl;
  result->ref_count = 1;
  result->ix = 0;

  return result;
}

void
_gdk_win32_ole2_dnd_property_change (GdkAtom       type,
				     gint          format,
				     const guchar *data,
				     gint          nelements)
{
  if (use_ole2_dnd)
    {
      HGLOBAL hdata = NULL;

      if (active_pFormatEtc == NULL || active_pMedium == NULL)
	return;

      /* Set up the data buffer for wide character text request */
      if (active_pFormatEtc->cfFormat == CF_UNICODETEXT)
	{
	  gunichar2 *wdata;
	  glong wlen;

	  wdata = g_utf8_to_utf16 ((const char *) data, -1, NULL, &wlen, NULL);
	  hdata = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, (wlen + 1) * 2);
	  if (hdata)
	    {
	      wchar_t *ptr = (wchar_t *) GlobalLock(hdata);
	      memcpy (ptr, wdata, (wlen + 1) * 2);
	      GlobalUnlock(hdata);
	    }
	  g_free (wdata);
	}
      else
	g_warning ("Only text handled for now");

      /* Pack up data */
      active_pMedium->tymed = TYMED_HGLOBAL;
      active_pMedium->hGlobal = hdata;
      active_pMedium->pUnkForRelease = 0;
    }
}

/* From MS Knowledge Base article Q130698 */

static gboolean
resolve_link (HWND     hWnd,
	      wchar_t *link,
	      gchar  **lpszPath)
{
  WIN32_FILE_ATTRIBUTE_DATA wfad;
  HRESULT hr;
  IShellLinkW *pslW = NULL;
  IPersistFile *ppf = NULL;

  /* Check if the file is empty first because IShellLink::Resolve for
   * some reason succeeds with an empty file and returns an empty
   * "link target". (#524151)
   */
    if (!GetFileAttributesExW (link, GetFileExInfoStandard, &wfad) ||
	(wfad.nFileSizeHigh == 0 && wfad.nFileSizeLow == 0))
      return FALSE;

  /* Assume failure to start with: */
  *lpszPath = 0;

  /* Call CoCreateInstance to obtain the IShellLink interface
   * pointer. This call fails if CoInitialize is not called, so it is
   * assumed that CoInitialize has been called.
   */

  hr = CoCreateInstance (&CLSID_ShellLink,
			 NULL,
			 CLSCTX_INPROC_SERVER,
			 &IID_IShellLinkW,
			 (LPVOID *)&pslW);

  if (SUCCEEDED (hr))
   {
     /* The IShellLink interface supports the IPersistFile
      * interface. Get an interface pointer to it.
      */
     hr = pslW->lpVtbl->QueryInterface (pslW,
					&IID_IPersistFile,
					(LPVOID *) &ppf);
   }

  if (SUCCEEDED (hr))
    {
      /* Load the file. */
      hr = ppf->lpVtbl->Load (ppf, link, STGM_READ);
    }

  if (SUCCEEDED (hr))
    {
      /* Resolve the link by calling the Resolve()
       * interface function.
       */
      hr = pslW->lpVtbl->Resolve (pslW, hWnd, SLR_ANY_MATCH | SLR_NO_UI);
    }

  if (SUCCEEDED (hr))
    {
      wchar_t wtarget[MAX_PATH];

      hr = pslW->lpVtbl->GetPath (pslW, wtarget, MAX_PATH, NULL, 0);
      if (SUCCEEDED (hr))
	*lpszPath = g_utf16_to_utf8 (wtarget, -1, NULL, NULL, NULL);
    }

  if (ppf)
    ppf->lpVtbl->Release (ppf);

  if (pslW)
    pslW->lpVtbl->Release (pslW);

  return SUCCEEDED (hr);
}

#if 0

/* Check for filenames like C:\Users\tml\AppData\Local\Temp\d5qtkvvs.bmp */
static gboolean
filename_looks_tempish (const char *filename)
{
  char *dirname;
  char *p;
  const char *q;
  gboolean retval = FALSE;

  dirname = g_path_get_dirname (filename);

  p = dirname;
  q = g_get_tmp_dir ();

  while (*p && *q &&
	 ((G_IS_DIR_SEPARATOR (*p) && G_IS_DIR_SEPARATOR (*q)) ||
	  g_ascii_tolower (*p) == g_ascii_tolower (*q)))
    p++, q++;

  if (!*p && !*q)
    retval = TRUE;

  g_free (dirname);

  return retval;
}

static gboolean
close_it (gpointer data)
{
  close (GPOINTER_TO_INT (data));

  return FALSE;
}

#endif

static GdkFilterReturn
gdk_dropfiles_filter (GdkXEvent *xev,
		      GdkEvent  *event,
		      gpointer   data)
{
  GdkDragContext *context;
  GString *result;
  MSG *msg = (MSG *) xev;
  HANDLE hdrop;
  POINT pt;
  gint nfiles, i;
  gchar *fileName, *linkedFile;

  if (msg->message == WM_DROPFILES)
    {
      GDK_NOTE (DND, g_print ("WM_DROPFILES: %p\n", msg->hwnd));

      context = gdk_drag_context_new ();
      context->protocol = GDK_DRAG_PROTO_WIN32_DROPFILES;
      context->is_source = FALSE;

      context->source_window = _gdk_root;
      g_object_ref (context->source_window);

      context->dest_window = event->any.window;
      g_object_ref (context->dest_window);

      /* WM_DROPFILES drops are always file names */
      context->targets =
	g_list_append (NULL, _text_uri_list);
      context->actions = GDK_ACTION_COPY;
      context->suggested_action = GDK_ACTION_COPY;
      current_dest_drag = context;

      event->dnd.type = GDK_DROP_START;
      event->dnd.context = current_dest_drag;

      hdrop = (HANDLE) msg->wParam;
      DragQueryPoint (hdrop, &pt);
      ClientToScreen (msg->hwnd, &pt);

      event->dnd.x_root = pt.x + _gdk_offset_x;
      event->dnd.y_root = pt.y + _gdk_offset_y;
      event->dnd.time = _gdk_win32_get_next_tick (msg->time);

      nfiles = DragQueryFile (hdrop, 0xFFFFFFFF, NULL, 0);

      result = g_string_new (NULL);
      for (i = 0; i < nfiles; i++)
	{
	  gchar *uri;
	  wchar_t wfn[MAX_PATH];

	  DragQueryFileW (hdrop, i, wfn, MAX_PATH);
	  fileName = g_utf16_to_utf8 (wfn, -1, NULL, NULL, NULL);

	  /* Resolve shortcuts */
	  if (resolve_link (msg->hwnd, wfn, &linkedFile))
	    {
	      uri = g_filename_to_uri (linkedFile, NULL, NULL);
	      if (uri != NULL)
		{
		  g_string_append (result, uri);
		  GDK_NOTE (DND, g_print ("... %s link to %s: %s\n",
					  fileName, linkedFile, uri));
		  g_free (uri);
		}
	      g_free (fileName);
	      fileName = linkedFile;
	    }
	  else
	    {
	      uri = g_filename_to_uri (fileName, NULL, NULL);
	      if (uri != NULL)
		{
		  g_string_append (result, uri);
		  GDK_NOTE (DND, g_print ("... %s: %s\n", fileName, uri));
		  g_free (uri);
		}
	    }

#if 0
	  /* Awful hack to recognize temp files corresponding to
	   * images dragged from Firefox... Open the file right here
	   * so that it is less likely that Firefox manages to delete
	   * it before the GTK+-using app (typically GIMP) has opened
	   * it.
	   *
	   * Not compiled in for now, because it means images dragged
	   * from Firefox would stay around in the temp folder which
	   * is not what Firefox intended. I don't feel comfortable
	   * with that, both from a geenral sanity point of view, and
	   * from a privacy point of view. It's better to wait for
	   * Firefox to fix the problem, for instance by deleting the
	   * temp file after a longer delay, or to wait until we
	   * implement the OLE2_DND...
	   */
	  if (filename_looks_tempish (fileName))
	    {
	      int fd = g_open (fileName, _O_RDONLY|_O_BINARY, 0);
	      if (fd == -1)
		{
		  GDK_NOTE (DND, g_print ("Could not open %s, maybe an image dragged from Firefox that it already deleted\n", fileName));
		}
	      else
		{
		  GDK_NOTE (DND, g_print ("Opened %s as %d so that Firefox won't delete it\n", fileName, fd));
		  g_timeout_add_seconds (1, close_it, GINT_TO_POINTER (fd));
		}
	    }
#endif

	  g_free (fileName);
	  g_string_append (result, "\015\012");
	}
      _gdk_dropfiles_store (result->str);
      g_string_free (result, FALSE);

      DragFinish (hdrop);

      return GDK_FILTER_TRANSLATE;
    }
  else
    return GDK_FILTER_CONTINUE;
}

static void
add_format (GArray *fmts,
	    CLIPFORMAT cf)
{
  FORMATETC fmt;

  fmt.cfFormat = cf;
  fmt.ptd = NULL;
  fmt.dwAspect = DVASPECT_CONTENT;
  fmt.lindex = -1;
  fmt.tymed = TYMED_HGLOBAL;

  g_array_append_val (fmts, fmt);
}


void
_gdk_dnd_init (void)
{
  if (getenv ("GDK_WIN32_USE_EXPERIMENTAL_OLE2_DND"))
    use_ole2_dnd = TRUE;

  if (use_ole2_dnd)
    {
      HRESULT hr;
      GArray *fmts;

      hr = OleInitialize (NULL);

      if (! SUCCEEDED (hr))
	g_error ("OleInitialize failed");

      fmts = g_array_new (FALSE, FALSE, sizeof (FORMATETC));

      /* The most important presumably */
      add_format (fmts, CF_UNICODETEXT);

      /* Used for GTK+ internal DND, I think was the intent? Anyway, code below assumes
       * this is at index 1.
       */
      add_format (fmts, CF_GDIOBJFIRST);

      add_format (fmts, CF_HDROP);

      add_format (fmts, _cf_png);
      add_format (fmts, CF_DIB);

      add_format (fmts, _cf_url);
      add_format (fmts, _cf_html_format);
      add_format (fmts, _cf_text_html);

      nformats = fmts->len;
      formats = (FORMATETC*) g_array_free (fmts, FALSE);

      target_ctx_for_window = g_hash_table_new (g_direct_hash, g_direct_equal);
    }
}

void
_gdk_win32_dnd_exit (void)
{
  if (use_ole2_dnd)
    {
      OleUninitialize ();
    }
}

/* Source side */

static void
local_send_leave (GdkDragContext *context,
		  guint32         time)
{
  GdkEvent tmp_event;

  GDK_NOTE (DND, g_print ("local_send_leave: context=%p current_dest_drag=%p\n",
			  context,
			  current_dest_drag));

  if ((current_dest_drag != NULL) &&
      (current_dest_drag->protocol == GDK_DRAG_PROTO_LOCAL) &&
      (current_dest_drag->source_window == context->source_window))
    {
      tmp_event.dnd.type = GDK_DRAG_LEAVE;
      tmp_event.dnd.window = context->dest_window;
      /* Pass ownership of context to the event */
      tmp_event.dnd.send_event = FALSE;
      tmp_event.dnd.context = current_dest_drag;
      tmp_event.dnd.time = GDK_CURRENT_TIME; /* FIXME? */

      current_dest_drag = NULL;

      GDK_NOTE (EVENTS, _gdk_win32_print_event (&tmp_event));
      gdk_event_put (&tmp_event);
    }
}

static void
local_send_enter (GdkDragContext *context,
		  guint32         time)
{
  GdkEvent tmp_event;
  GdkDragContextPrivateWin32 *private;
  GdkDragContext *new_context;

  GDK_NOTE (DND, g_print ("local_send_enter: context=%p current_dest_drag=%p\n",
			  context,
			  current_dest_drag));

  private = PRIVATE_DATA (context);

  if (current_dest_drag != NULL)
    {
      g_object_unref (G_OBJECT (current_dest_drag));
      current_dest_drag = NULL;
    }

  new_context = gdk_drag_context_new ();
  new_context->protocol = GDK_DRAG_PROTO_LOCAL;
  new_context->is_source = FALSE;

  new_context->source_window = context->source_window;
  g_object_ref (new_context->source_window);

  new_context->dest_window = context->dest_window;
  g_object_ref (new_context->dest_window);

  new_context->targets = g_list_copy (context->targets);

  gdk_window_set_events (new_context->source_window,
			 gdk_window_get_events (new_context->source_window) |
			 GDK_PROPERTY_CHANGE_MASK);
  new_context->actions = context->actions;

  tmp_event.type = GDK_DRAG_ENTER;
  tmp_event.dnd.window = context->dest_window;
  tmp_event.dnd.send_event = FALSE;
  tmp_event.dnd.context = new_context;
  tmp_event.dnd.time = GDK_CURRENT_TIME; /* FIXME? */

  current_dest_drag = new_context;

  GDK_NOTE (EVENTS, _gdk_win32_print_event (&tmp_event));
  gdk_event_put (&tmp_event);
}

static void
local_send_motion (GdkDragContext *context,
		   gint            x_root,
		   gint            y_root,
		   GdkDragAction   action,
		   guint32         time)
{
  GdkEvent tmp_event;

  GDK_NOTE (DND, g_print ("local_send_motion: context=%p (%d,%d) current_dest_drag=%p\n",
			  context, x_root, y_root,
			  current_dest_drag));

  if ((current_dest_drag != NULL) &&
      (current_dest_drag->protocol == GDK_DRAG_PROTO_LOCAL) &&
      (current_dest_drag->source_window == context->source_window))
    {
      tmp_event.type = GDK_DRAG_MOTION;
      tmp_event.dnd.window = current_dest_drag->dest_window;
      tmp_event.dnd.send_event = FALSE;
      tmp_event.dnd.context = current_dest_drag;
      tmp_event.dnd.time = time;

      current_dest_drag->suggested_action = action;

      tmp_event.dnd.x_root = x_root;
      tmp_event.dnd.y_root = y_root;

      PRIVATE_DATA (current_dest_drag)->last_pt.x = x_root - _gdk_offset_x;
      PRIVATE_DATA (current_dest_drag)->last_pt.y = y_root - _gdk_offset_y;

      PRIVATE_DATA (context)->drag_status = GDK_DRAG_STATUS_MOTION_WAIT;

      GDK_NOTE (EVENTS, _gdk_win32_print_event (&tmp_event));
      gdk_event_put (&tmp_event);
    }
}

static void
local_send_drop (GdkDragContext *context,
		 guint32         time)
{
  GdkEvent tmp_event;

  GDK_NOTE (DND, g_print ("local_send_drop: context=%p current_dest_drag=%p\n",
			  context,
			  current_dest_drag));

  if ((current_dest_drag != NULL) &&
      (current_dest_drag->protocol == GDK_DRAG_PROTO_LOCAL) &&
      (current_dest_drag->source_window == context->source_window))
    {
      GdkDragContextPrivateWin32 *private;
      private = PRIVATE_DATA (current_dest_drag);

      /* Pass ownership of context to the event */
      tmp_event.type = GDK_DROP_START;
      tmp_event.dnd.window = current_dest_drag->dest_window;
      tmp_event.dnd.send_event = FALSE;
      tmp_event.dnd.context = current_dest_drag;
      tmp_event.dnd.time = GDK_CURRENT_TIME;

      tmp_event.dnd.x_root = private->last_pt.x + _gdk_offset_x;
      tmp_event.dnd.y_root = private->last_pt.y + _gdk_offset_y;

      current_dest_drag = NULL;

      GDK_NOTE (EVENTS, _gdk_win32_print_event (&tmp_event));
      gdk_event_put (&tmp_event);
    }

}

static void
gdk_drag_do_leave (GdkDragContext *context,
		   guint32         time)
{
  if (context->dest_window)
    {
      GDK_NOTE (DND, g_print ("gdk_drag_do_leave\n"));

      if (!use_ole2_dnd)
	{
	  if (context->protocol == GDK_DRAG_PROTO_LOCAL)
	    local_send_leave (context, time);
	}

      g_object_unref (context->dest_window);
      context->dest_window = NULL;
    }
}

GdkDragContext *
gdk_drag_begin (GdkWindow *window,
		GList     *targets)
{
  if (!use_ole2_dnd)
    {
      GdkDragContext *new_context;

      g_return_val_if_fail (window != NULL, NULL);

      new_context = gdk_drag_context_new ();

      new_context->is_source = TRUE;

      new_context->source_window = window;
      g_object_ref (window);

      new_context->targets = g_list_copy (targets);
      new_context->actions = 0;

      return new_context;
    }
  else
    {
      source_drag_context *ctx;

      g_return_val_if_fail (window != NULL, NULL);

      GDK_NOTE (DND, g_print ("gdk_drag_begin\n"));

      ctx = source_context_new (window, targets);

      _dnd_source_state = GDK_WIN32_DND_PENDING;

      pending_src_context = ctx;
      g_object_ref (ctx->context);

      return ctx->context;
    }
}

void
_gdk_win32_dnd_do_dragdrop (void)
{
  if (use_ole2_dnd)
    {
      GdkDragContext* drag_ctx;
      GdkDragContextPrivateWin32 *private;
      BYTE kbd_state[256];
      data_object *dobj;
      HRESULT hr;
      DWORD dwEffect;
#if 0
      HGLOBAL global;
      STGMEDIUM medium;
#endif

      if (pending_src_context == NULL)
	return;

      drag_ctx = pending_src_context->context;
      private = PRIVATE_DATA (drag_ctx);

      dobj = data_object_new (drag_ctx);

      API_CALL (GetCursorPos, (&private->last_pt));
      API_CALL (ScreenToClient, (GDK_WINDOW_HWND (drag_ctx->source_window), &private->last_pt));
      private->last_key_state = 0;
      API_CALL (GetKeyboardState, (kbd_state));

      if (kbd_state[VK_CONTROL])
	private->last_key_state |= MK_CONTROL;
      if (kbd_state[VK_SHIFT])
	private->last_key_state |= MK_SHIFT;
      if (kbd_state[VK_LBUTTON])
	private->last_key_state |= MK_LBUTTON;
      if (kbd_state[VK_MBUTTON])
	private->last_key_state |= MK_MBUTTON;
      if (kbd_state[VK_RBUTTON])
	private->last_key_state |= MK_RBUTTON;

#if 0
      global = GlobalAlloc (GMEM_FIXED, sizeof (ctx));

      memcpy (&global, ctx, sizeof (ctx));

      medium.tymed = TYMED_HGLOBAL;
      medium.hGlobal = global;
      medium.pUnkForRelease = NULL;

      /* FIXME I wish I remember what I was thinking of here, i.e. what
       * the formats[1] signifies, i.e. the CF_GDIOBJFIRST FORMATETC?
       */
      dobj->ido.lpVtbl->SetData (&dobj->ido, &formats[1], &medium, TRUE);
#endif

      /* Start dragging with mainloop inside the OLE2 API. Exits only when done */

      GDK_NOTE (DND, g_print ("Calling DoDragDrop\n"));

      _gdk_win32_begin_modal_call ();
      hr = DoDragDrop (&dobj->ido, &pending_src_context->ids,
		       DROPEFFECT_COPY | DROPEFFECT_MOVE,
		       &dwEffect);
      _gdk_win32_end_modal_call ();

      GDK_NOTE (DND, g_print ("DoDragDrop returned %s\n",
			      (hr == DRAGDROP_S_DROP ? "DRAGDROP_S_DROP" :
			       (hr == DRAGDROP_S_CANCEL ? "DRAGDROP_S_CANCEL" :
				(hr == E_UNEXPECTED ? "E_UNEXPECTED" :
				 g_strdup_printf ("%#.8lx", hr))))));

      /* Delete dnd selection after successful move */
      if (hr == DRAGDROP_S_DROP && dwEffect == DROPEFFECT_MOVE)
	{
	  GdkEvent tmp_event;

	  tmp_event.type = GDK_SELECTION_REQUEST;
	  tmp_event.selection.window = drag_ctx->source_window;
	  tmp_event.selection.send_event = FALSE;
	  tmp_event.selection.selection = _gdk_ole2_dnd;
	  tmp_event.selection.target = _delete;
	  tmp_event.selection.property = _gdk_ole2_dnd; /* ??? */
	  tmp_event.selection.time = GDK_CURRENT_TIME; /* ??? */
	  g_object_ref (tmp_event.selection.window);

	  GDK_NOTE (EVENTS, _gdk_win32_print_event (&tmp_event));
	  gdk_event_put (&tmp_event);
	}

#if 0
      // Send a GDK_DROP_FINISHED to the source window
      GetCursorPos (&pt);
      ptl.x = pt.x;
      ptl.y = pt.y;
      if ( pending_src_context != NULL && pending_src_context->context != NULL
	   && pending_src_context->context->source_window != NULL )
	push_dnd_event (GDK_DROP_FINISHED, pending_src_context->context, ptl, FALSE);
#endif

      dobj->ido.lpVtbl->Release (&dobj->ido);
      if (pending_src_context != NULL)
	{
	  pending_src_context->ids.lpVtbl->Release (&pending_src_context->ids);
	  pending_src_context = NULL;
	}
    }
}

GdkNativeWindow
gdk_drag_get_protocol_for_display (GdkDisplay      *display,
				   GdkNativeWindow  xid,
				   GdkDragProtocol *protocol)
{
  GdkWindow *window;

  window = gdk_window_lookup (xid);
  if (window &&
      gdk_window_get_window_type (window) != GDK_WINDOW_FOREIGN)
    {
      if (g_object_get_data (G_OBJECT (window), "gdk-dnd-registered") != NULL)
	{
	  if (use_ole2_dnd)
	    *protocol = GDK_DRAG_PROTO_OLE2;
	  else
	    *protocol = GDK_DRAG_PROTO_LOCAL;

	  return xid;
	}
    }

  return 0;
}

typedef struct {
  gint x;
  gint y;
  HWND ignore;
  HWND result;
} find_window_enum_arg;

static BOOL CALLBACK
find_window_enum_proc (HWND   hwnd,
                       LPARAM lparam)
{
  RECT rect;
  POINT tl, br;
  find_window_enum_arg *a = (find_window_enum_arg *) lparam;

  if (hwnd == a->ignore)
    return TRUE;

  if (!IsWindowVisible (hwnd))
    return TRUE;

  tl.x = tl.y = 0;
  ClientToScreen (hwnd, &tl);
  GetClientRect (hwnd, &rect);
  br.x = rect.right;
  br.y = rect.bottom;
  ClientToScreen (hwnd, &br);

  if (a->x >= tl.x && a->y >= tl.y && a->x < br.x && a->y < br.y)
    {
      a->result = hwnd;
      return FALSE;
    }
  else
    return TRUE;
}

void
gdk_drag_find_window_for_screen (GdkDragContext  *context,
				 GdkWindow       *drag_window,
				 GdkScreen       *screen,
				 gint             x_root,
				 gint             y_root,
				 GdkWindow      **dest_window,
				 GdkDragProtocol *protocol)
{
  GdkWindow *dw;
  find_window_enum_arg a;

  a.x = x_root - _gdk_offset_x;
  a.y = y_root - _gdk_offset_y;
  a.ignore = drag_window ? GDK_WINDOW_HWND (drag_window) : NULL;
  a.result = NULL;

  EnumWindows (find_window_enum_proc, (LPARAM) &a);

  if (a.result == NULL)
    *dest_window = NULL;
  else
    {
      dw = gdk_win32_handle_table_lookup (a.result);
      if (dw)
        {
          *dest_window = gdk_window_get_toplevel (dw);
          g_object_ref (*dest_window);
        }
      else
        *dest_window = gdk_window_foreign_new_for_display (_gdk_display, a.result);

      if (use_ole2_dnd)
        *protocol = GDK_DRAG_PROTO_OLE2;
      else if (context->source_window)
        *protocol = GDK_DRAG_PROTO_LOCAL;
      else
        *protocol = GDK_DRAG_PROTO_WIN32_DROPFILES;
    }

  GDK_NOTE (DND,
	    g_print ("gdk_drag_find_window: %p %+d%+d: %p: %p %s\n",
		     (drag_window ? GDK_WINDOW_HWND (drag_window) : NULL),
		     x_root, y_root,
		     a.result,
		     (*dest_window ? GDK_WINDOW_HWND (*dest_window) : NULL),
		     _gdk_win32_drag_protocol_to_string (*protocol)));
}

gboolean
gdk_drag_motion (GdkDragContext *context,
		 GdkWindow      *dest_window,
		 GdkDragProtocol protocol,
		 gint            x_root,
		 gint            y_root,
		 GdkDragAction   suggested_action,
		 GdkDragAction   possible_actions,
		 guint32         time)
{
  GdkDragContextPrivateWin32 *private;

  g_return_val_if_fail (context != NULL, FALSE);

  context->actions = possible_actions;

  GDK_NOTE (DND, g_print ("gdk_drag_motion: %s suggested=%s, possible=%s\n"
			  " context=%p:{actions=%s,suggested=%s,action=%s}\n",
			  _gdk_win32_drag_protocol_to_string (protocol),
			  _gdk_win32_drag_action_to_string (suggested_action),
			  _gdk_win32_drag_action_to_string (possible_actions),
			  context,
			  _gdk_win32_drag_action_to_string (context->actions),
			  _gdk_win32_drag_action_to_string (context->suggested_action),
			  _gdk_win32_drag_action_to_string (context->action)));

  private = PRIVATE_DATA (context);

  if (!use_ole2_dnd)
    {
      if (context->dest_window == dest_window)
	{
	  GdkDragContext *dest_context;

	  dest_context = gdk_drag_context_find (FALSE,
						context->source_window,
						dest_window);

	  if (dest_context)
	    dest_context->actions = context->actions;

	  context->suggested_action = suggested_action;
	}
      else
	{
	  GdkEvent tmp_event;

	  /* Send a leave to the last destination */
	  gdk_drag_do_leave (context, time);
	  private->drag_status = GDK_DRAG_STATUS_DRAG;

	  /* Check if new destination accepts drags, and which protocol */
	  if (dest_window)
	    {
	      context->dest_window = dest_window;
	      g_object_ref (context->dest_window);
	      context->protocol = protocol;

	      switch (protocol)
		{
		case GDK_DRAG_PROTO_LOCAL:
		  local_send_enter (context, time);
		  break;

		default:
		  break;
		}
	      context->suggested_action = suggested_action;
	    }
	  else
	    {
	      context->dest_window = NULL;
	      context->action = 0;
	    }

	  /* Push a status event, to let the client know that
	   * the drag changed
	   */
	  tmp_event.type = GDK_DRAG_STATUS;
	  tmp_event.dnd.window = context->source_window;
	  /* We use this to signal a synthetic status. Perhaps
	   * we should use an extra field...
	   */
	  tmp_event.dnd.send_event = TRUE;

	  tmp_event.dnd.context = context;
	  tmp_event.dnd.time = time;

	  GDK_NOTE (EVENTS, _gdk_win32_print_event (&tmp_event));
	  gdk_event_put (&tmp_event);
	}

      /* Send a drag-motion event */

      private->last_pt.x = x_root - _gdk_offset_x;
      private->last_pt.y = y_root - _gdk_offset_y;

      if (context->dest_window)
	{
	  if (private->drag_status == GDK_DRAG_STATUS_DRAG)
	    {
	      switch (context->protocol)
		{
		case GDK_DRAG_PROTO_LOCAL:
		  local_send_motion (context, x_root, y_root, suggested_action, time);
		  break;

		case GDK_DRAG_PROTO_NONE:
		  g_warning ("GDK_DRAG_PROTO_NONE is not valid in gdk_drag_motion()");
		  break;

		default:
		  break;
		}
	    }
	  else
	    {
	      GDK_NOTE (DND, g_print (" returning TRUE\n"
				      " context=%p:{actions=%s,suggested=%s,action=%s}\n",
				      context,
				      _gdk_win32_drag_action_to_string (context->actions),
				      _gdk_win32_drag_action_to_string (context->suggested_action),
				      _gdk_win32_drag_action_to_string (context->action)));
	      return TRUE;
	    }
	}
    }

  GDK_NOTE (DND, g_print (" returning FALSE\n"
			  " context=%p:{actions=%s,suggested=%s,action=%s}\n",
			  context,
			  _gdk_win32_drag_action_to_string (context->actions),
			  _gdk_win32_drag_action_to_string (context->suggested_action),
			  _gdk_win32_drag_action_to_string (context->action)));
  return FALSE;
}

void
gdk_drag_drop (GdkDragContext *context,
	       guint32         time)
{
  g_return_if_fail (context != NULL);

  GDK_NOTE (DND, g_print ("gdk_drag_drop\n"));

  if (!use_ole2_dnd)
    {
      if (context->dest_window &&
	  context->protocol == GDK_DRAG_PROTO_LOCAL)
	local_send_drop (context, time);
    }
  else
    {
      _dnd_source_state = GDK_WIN32_DND_DROPPED;
    }
}

void
gdk_drag_abort (GdkDragContext *context,
		guint32         time)
{
  g_return_if_fail (context != NULL);

  GDK_NOTE (DND, g_print ("gdk_drag_abort\n"));

  if (use_ole2_dnd)
    _dnd_source_state = GDK_WIN32_DND_NONE;
}

/* Destination side */

void
gdk_drag_status (GdkDragContext *context,
		 GdkDragAction   action,
		 guint32         time)
{
  GdkDragContextPrivateWin32 *private;
  GdkDragContext *src_context;
  GdkEvent tmp_event;

  g_return_if_fail (context != NULL);

  private = PRIVATE_DATA (context);

  GDK_NOTE (DND, g_print ("gdk_drag_status: %s\n"
			  " context=%p:{actions=%s,suggested=%s,action=%s}\n",
			  _gdk_win32_drag_action_to_string (action),
			  context,
			  _gdk_win32_drag_action_to_string (context->actions),
			  _gdk_win32_drag_action_to_string (context->suggested_action),
			  _gdk_win32_drag_action_to_string (context->action)));

  context->action = action;

  if (!use_ole2_dnd)
    {
      src_context = gdk_drag_context_find (TRUE,
					   context->source_window,
					   context->dest_window);

      if (src_context)
	{
	  GdkDragContextPrivateWin32 *private = PRIVATE_DATA (src_context);

	  if (private->drag_status == GDK_DRAG_STATUS_MOTION_WAIT)
	    private->drag_status = GDK_DRAG_STATUS_DRAG;

	  tmp_event.type = GDK_DRAG_STATUS;
	  tmp_event.dnd.window = context->source_window;
	  tmp_event.dnd.send_event = FALSE;
	  tmp_event.dnd.context = src_context;
	  tmp_event.dnd.time = GDK_CURRENT_TIME; /* FIXME? */

	  if (action == GDK_ACTION_DEFAULT)
	    action = 0;

	  src_context->action = action;

	  GDK_NOTE (EVENTS, _gdk_win32_print_event (&tmp_event));
	  gdk_event_put (&tmp_event);
	}
    }
}

void
gdk_drop_reply (GdkDragContext *context,
		gboolean        ok,
		guint32         time)
{
  g_return_if_fail (context != NULL);

  GDK_NOTE (DND, g_print ("gdk_drop_reply\n"));

  if (!use_ole2_dnd)
    if (context->dest_window)
      {
	if (context->protocol == GDK_DRAG_PROTO_WIN32_DROPFILES)
	  _gdk_dropfiles_store (NULL);
      }
}

void
gdk_drop_finish (GdkDragContext *context,
		 gboolean        success,
		 guint32         time)
{
  GdkDragContextPrivateWin32 *private;
  GdkDragContext *src_context;
  GdkEvent tmp_event;

  g_return_if_fail (context != NULL);

  GDK_NOTE (DND, g_print ("gdk_drop_finish\n"));

  private = PRIVATE_DATA (context);

  if (!use_ole2_dnd)
    {
      src_context = gdk_drag_context_find (TRUE,
					   context->source_window,
					   context->dest_window);
      if (src_context)
	{
	  tmp_event.type = GDK_DROP_FINISHED;
	  tmp_event.dnd.window = src_context->source_window;
	  tmp_event.dnd.send_event = FALSE;
	  tmp_event.dnd.context = src_context;

	  GDK_NOTE (EVENTS, _gdk_win32_print_event (&tmp_event));
	  gdk_event_put (&tmp_event);
	}
    }
  else
    {
      gdk_drag_do_leave (context, time);

      if (success)
	_dnd_target_state = GDK_WIN32_DND_DROPPED;
      else
	_dnd_target_state = GDK_WIN32_DND_FAILED;
    }
}

#if 0

static GdkFilterReturn
gdk_destroy_filter (GdkXEvent *xev,
		    GdkEvent  *event,
		    gpointer   data)
{
  MSG *msg = (MSG *) xev;

  if (msg->message == WM_DESTROY)
    {
      IDropTarget *idtp = (IDropTarget *) data;

      GDK_NOTE (DND, g_print ("gdk_destroy_filter: WM_DESTROY: %p\n", msg->hwnd));
#if 0
      idtp->lpVtbl->Release (idtp);
#endif
      RevokeDragDrop (msg->hwnd);
      CoLockObjectExternal ((IUnknown*) idtp, FALSE, TRUE);
    }
  return GDK_FILTER_CONTINUE;
}

#endif

void
gdk_window_register_dnd (GdkWindow *window)
{
  target_drag_context *ctx;
  HRESULT hr;

  g_return_if_fail (window != NULL);

  if (gdk_window_get_window_type (window) == GDK_WINDOW_OFFSCREEN)
    return;

  if (g_object_get_data (G_OBJECT (window), "gdk-dnd-registered") != NULL)
    return;
  else
    g_object_set_data (G_OBJECT (window), "gdk-dnd-registered", GINT_TO_POINTER (TRUE));

  GDK_NOTE (DND, g_print ("gdk_window_register_dnd: %p\n", GDK_WINDOW_HWND (window)));

  if (!use_ole2_dnd)
    {
      /* We always claim to accept dropped files, but in fact we might not,
       * of course. This function is called in such a way that it cannot know
       * whether the window (widget) in question actually accepts files
       * (in gtk, data of type text/uri-list) or not.
       */
      gdk_window_add_filter (window, gdk_dropfiles_filter, NULL);
      DragAcceptFiles (GDK_WINDOW_HWND (window), TRUE);
    }
  else
    {
      /* Return if window is already setup for DND. */
      if (g_hash_table_lookup (target_ctx_for_window, GDK_WINDOW_HWND (window)) != NULL)
	return;

      /* Register for OLE2 d&d : similarly, claim to accept all supported
       * data types because we cannot know from here what the window
       * actually accepts.
       */
      /* FIXME: This of course won't work with user-extensible data types! */
      ctx = target_context_new (window);

      hr = CoLockObjectExternal ((IUnknown *) &ctx->idt, TRUE, FALSE);
      if (!SUCCEEDED (hr))
	OTHER_API_FAILED ("CoLockObjectExternal");
      else
	{
	  hr = RegisterDragDrop (GDK_WINDOW_HWND (window), &ctx->idt);
	  if (hr == DRAGDROP_E_ALREADYREGISTERED)
	    {
	      g_print ("DRAGDROP_E_ALREADYREGISTERED\n");
	      CoLockObjectExternal ((IUnknown *) &ctx->idt, FALSE, FALSE);
	    }
	  else if (!SUCCEEDED (hr))
	    OTHER_API_FAILED ("RegisterDragDrop");
	  else
	    {
	      gdk_window_ref (window);
	      g_hash_table_insert (target_ctx_for_window, GDK_WINDOW_HWND (window), ctx);
	    }
	}
    }
}

GdkAtom
gdk_drag_get_selection (GdkDragContext *context)
{
  switch (context->protocol)
    {
    case GDK_DRAG_PROTO_LOCAL:
      return _local_dnd;
    case GDK_DRAG_PROTO_WIN32_DROPFILES:
      return _gdk_win32_dropfiles;
    case GDK_DRAG_PROTO_OLE2:
      return _gdk_ole2_dnd;
    default:
      return GDK_NONE;
    }
}

gboolean
gdk_drag_drop_succeeded (GdkDragContext *context)
{
  GdkDragContextPrivateWin32 *private;

  g_return_val_if_fail (context != NULL, FALSE);

  private = PRIVATE_DATA (context);

  /* FIXME: Can we set drop_failed when the drop has failed? */
  return !private->drop_failed;
}
