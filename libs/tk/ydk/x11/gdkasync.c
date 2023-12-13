/* GTK - The GIMP Toolkit
 * gdkasync.c: Utility functions using the Xlib asynchronous interfaces
 * Copyright (C) 2003, Red Hat, Inc.
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
/* Portions of code in this file are based on code from Xlib
 */
/*
Copyright 1986, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/
#include "config.h"
#ifdef NEED_XIPROTO_H_FOR_XREPLY
#include <X11/extensions/XIproto.h>
#endif
#include <X11/Xlibint.h>
#include "gdkasync.h"
#include "gdkx.h"
#include "gdkalias.h"

typedef struct _ChildInfoChildState ChildInfoChildState;
typedef struct _ChildInfoState ChildInfoState;
typedef struct _ListChildrenState ListChildrenState;
typedef struct _SendEventState SendEventState;
typedef struct _SetInputFocusState SetInputFocusState;
typedef struct _RoundtripState RoundtripState;

typedef enum {
  CHILD_INFO_GET_PROPERTY,
  CHILD_INFO_GET_WA,
  CHILD_INFO_GET_GEOMETRY
} ChildInfoReq;

struct _ChildInfoChildState
{
  gulong seq[3];
};

struct _ChildInfoState
{
  gboolean get_wm_state;
  Window *children;
  guint nchildren;
  GdkChildInfoX11 *child_info;
  ChildInfoChildState *child_states;

  guint current_child;
  guint n_children_found;
  gint current_request;
  gboolean have_error;
  gboolean child_has_error;
};

struct _ListChildrenState
{
  Display *dpy;
  gulong get_property_req;
  gboolean have_error;
  gboolean has_wm_state;
};

struct _SendEventState
{
  Display *dpy;
  Window window;
  _XAsyncHandler async;
  gulong send_event_req;
  gulong get_input_focus_req;
  gboolean have_error;
  GdkSendXEventCallback callback;
  gpointer data;
};

struct _SetInputFocusState
{
  Display *dpy;
  _XAsyncHandler async;
  gulong set_input_focus_req;
  gulong get_input_focus_req;
};

struct _RoundtripState
{
  Display *dpy;
  _XAsyncHandler async;
  gulong get_input_focus_req;
  GdkDisplay *display;
  GdkRoundTripCallback callback;
  gpointer data;
};

static gboolean
callback_idle (gpointer data)
{
  SendEventState *state = (SendEventState *)data;  
  
  state->callback (state->window, !state->have_error, state->data);

  g_free (state);

  return FALSE;
}

static Bool
send_event_handler (Display *dpy,
		    xReply  *rep,
		    char    *buf,
		    int      len,
		    XPointer data)
{
  SendEventState *state = (SendEventState *)data;  

  if (dpy->last_request_read == state->send_event_req)
    {
      if (rep->generic.type == X_Error &&
	  rep->error.errorCode == BadWindow)
	{
	  state->have_error = TRUE;
	  return True;
	}
    }
  else if (dpy->last_request_read == state->get_input_focus_req)
    {
      xGetInputFocusReply replbuf;
      xGetInputFocusReply *repl;
      
      if (rep->generic.type != X_Error)
	{
	  /* Actually does nothing, since there are no additional bytes
	   * to read, but maintain good form.
	   */
	  repl = (xGetInputFocusReply *)
	    _XGetAsyncReply(dpy, (char *)&replbuf, rep, buf, len,
			    (sizeof(xGetInputFocusReply) - sizeof(xReply)) >> 2,
			    True);
	}

      if (state->callback)
        gdk_threads_add_idle (callback_idle, state);

      DeqAsyncHandler(state->dpy, &state->async);

      return (rep->generic.type != X_Error);
    }

  return False;
}

static void
client_message_to_wire (XClientMessageEvent *ev,
			xEvent              *event)
{
  int i;
  event->u.clientMessage.window = ev->window;
  event->u.u.type = ev->type;
  event->u.u.detail = ev->format;
  switch (ev->format)
    {
    case 8:	
      event->u.clientMessage.u.b.type   = ev->message_type;
      for (i = 0; i < 20; i++)
	event->u.clientMessage.u.b.bytes[i] = ev->data.b[i];
      break;
    case 16:
      event->u.clientMessage.u.s.type   = ev->message_type;
      event->u.clientMessage.u.s.shorts0   = ev->data.s[0];
      event->u.clientMessage.u.s.shorts1   = ev->data.s[1];
      event->u.clientMessage.u.s.shorts2   = ev->data.s[2];
      event->u.clientMessage.u.s.shorts3   = ev->data.s[3];
      event->u.clientMessage.u.s.shorts4   = ev->data.s[4];
      event->u.clientMessage.u.s.shorts5   = ev->data.s[5];
      event->u.clientMessage.u.s.shorts6   = ev->data.s[6];
      event->u.clientMessage.u.s.shorts7   = ev->data.s[7];
      event->u.clientMessage.u.s.shorts8   = ev->data.s[8];
      event->u.clientMessage.u.s.shorts9   = ev->data.s[9];
      break;
    case 32:
      event->u.clientMessage.u.l.type   = ev->message_type;
      event->u.clientMessage.u.l.longs0   = ev->data.l[0];
      event->u.clientMessage.u.l.longs1   = ev->data.l[1];
      event->u.clientMessage.u.l.longs2   = ev->data.l[2];
      event->u.clientMessage.u.l.longs3   = ev->data.l[3];
      event->u.clientMessage.u.l.longs4   = ev->data.l[4];
      break;
    default:
      /* client passing bogus data, let server complain */
      break;
    }
}

void
_gdk_x11_send_client_message_async (GdkDisplay           *display, 
				    Window                window, 
				    gboolean              propagate,
				    glong                 event_mask,
				    XClientMessageEvent  *event_send,
				    GdkSendXEventCallback callback,
				    gpointer              data)
{
  Display *dpy;
  SendEventState *state;
  
  dpy = GDK_DISPLAY_XDISPLAY (display);

  state = g_new (SendEventState, 1);

  state->dpy = dpy;
  state->window = window;
  state->callback = callback;
  state->data = data;
  state->have_error = FALSE;
  
  LockDisplay(dpy);

  state->async.next = dpy->async_handlers;
  state->async.handler = send_event_handler;
  state->async.data = (XPointer) state;
  dpy->async_handlers = &state->async;

  {
    register xSendEventReq *req;
    xEvent ev;
    
    client_message_to_wire (event_send, &ev);
      
    GetReq(SendEvent, req);
    req->destination = window;
    req->propagate = propagate;
    req->eventMask = event_mask;
    /* gross, matches Xproto.h */
#ifdef WORD64			
    memcpy ((char *) req->eventdata, (char *) &ev, SIZEOF(xEvent));
#else    
    memcpy ((char *) &req->event, (char *) &ev, SIZEOF(xEvent));
#endif
    
    state->send_event_req = dpy->request;
  }

  /*
   * XSync (dpy, 0)
   */
  {
    xReq *req;
    
    GetEmptyReq(GetInputFocus, req);
    state->get_input_focus_req = dpy->request;
  }
  
  UnlockDisplay(dpy);
  SyncHandle();
}

static Bool
set_input_focus_handler (Display *dpy,
			 xReply  *rep,
			 char    *buf,
			 int      len,
			 XPointer data)
{
  SetInputFocusState *state = (SetInputFocusState *)data;  

  if (dpy->last_request_read == state->set_input_focus_req)
    {
      if (rep->generic.type == X_Error &&
	  rep->error.errorCode == BadMatch)
	{
	  /* Consume BadMatch errors, since we have no control
	   * over them.
	   */
	  return True;
	}
    }
  
  if (dpy->last_request_read == state->get_input_focus_req)
    {
      xGetInputFocusReply replbuf;
      xGetInputFocusReply *repl;
      
      if (rep->generic.type != X_Error)
	{
	  /* Actually does nothing, since there are no additional bytes
	   * to read, but maintain good form.
	   */
	  repl = (xGetInputFocusReply *)
	    _XGetAsyncReply(dpy, (char *)&replbuf, rep, buf, len,
			    (sizeof(xGetInputFocusReply) - sizeof(xReply)) >> 2,
			    True);
	}

      DeqAsyncHandler(state->dpy, &state->async);

      g_free (state);
      
      return (rep->generic.type != X_Error);
    }

  return False;
}

void
_gdk_x11_set_input_focus_safe (GdkDisplay             *display,
			       Window                  window,
			       int                     revert_to,
			       Time                    time)
{
  Display *dpy;
  SetInputFocusState *state;
  
  dpy = GDK_DISPLAY_XDISPLAY (display);

  state = g_new (SetInputFocusState, 1);

  state->dpy = dpy;
  
  LockDisplay(dpy);

  state->async.next = dpy->async_handlers;
  state->async.handler = set_input_focus_handler;
  state->async.data = (XPointer) state;
  dpy->async_handlers = &state->async;

  {
    xSetInputFocusReq *req;
    
    GetReq(SetInputFocus, req);
    req->focus = window;
    req->revertTo = revert_to;
    req->time = time;
    state->set_input_focus_req = dpy->request;
  }

  /*
   * XSync (dpy, 0)
   */
  {
    xReq *req;
    
    GetEmptyReq(GetInputFocus, req);
    state->get_input_focus_req = dpy->request;
  }
  
  UnlockDisplay(dpy);
  SyncHandle();
}

static Bool
list_children_handler (Display *dpy,
		       xReply  *rep,
		       char    *buf,
		       int      len,
		       XPointer data)
{
  ListChildrenState *state = (ListChildrenState *)data;

  if (dpy->last_request_read != state->get_property_req)
    return False;
  
  if (rep->generic.type == X_Error)
    {
      state->have_error = TRUE;
      return False;
    }
  else
    {
      xGetPropertyReply replbuf;
      xGetPropertyReply *repl;
	    
      repl = (xGetPropertyReply *)
	_XGetAsyncReply(dpy, (char *)&replbuf, rep, buf, len,
			(sizeof(xGetPropertyReply) - sizeof(xReply)) >> 2,
			True);

      state->has_wm_state = repl->propertyType != None;
      /* Since we called GetProperty with longLength of 0, we don't
       * have to worry about consuming the property data that would
       * normally follow after the reply
       */

      return True;
    }
}

static gboolean
list_children_and_wm_state (Display      *dpy,
			    Window        w,
			    Atom          wm_state_atom,
			    gboolean     *has_wm_state,
			    Window      **children,
			    unsigned int *nchildren)
{
  ListChildrenState state;
  _XAsyncHandler async;
  long nbytes;
  xQueryTreeReply rep;
  register xResourceReq *req;
  xGetPropertyReq *prop_req;

  LockDisplay(dpy);

  *children = NULL;
  *nchildren = 0;
  *has_wm_state = FALSE;
  
  state.have_error = FALSE;
  state.has_wm_state = FALSE;

  if (wm_state_atom)
    {
      async.next = dpy->async_handlers;
      async.handler = list_children_handler;
      async.data = (XPointer) &state;
      dpy->async_handlers = &async;

      GetReq (GetProperty, prop_req);
      prop_req->window = w;
      prop_req->property = wm_state_atom;
      prop_req->type = AnyPropertyType;
      prop_req->delete = False;
      prop_req->longOffset = 0;
      prop_req->longLength = 0;
      
      state.get_property_req = dpy->request;
    }
  
  GetResReq(QueryTree, w, req);
  if (!_XReply(dpy, (xReply *)&rep, 0, xFalse))
    {
      state.have_error = TRUE;
      goto out;
    }

  if (rep.nChildren != 0)
    {
      nbytes = rep.nChildren << 2;
      if (state.have_error)
	{
	  _XEatData(dpy, (unsigned long) nbytes);
	  goto out;
	}
      *children = g_new (Window, rep.nChildren);
      _XRead32 (dpy, (long *) *children, nbytes);
    }

  *nchildren = rep.nChildren;
  *has_wm_state = state.has_wm_state;

 out:
  if (wm_state_atom)
    DeqAsyncHandler(dpy, &async);
  UnlockDisplay(dpy);
  SyncHandle();
  
  return !state.have_error;
}

static void
handle_get_wa_reply (Display                   *dpy,
		     ChildInfoState            *state,
		     xGetWindowAttributesReply *repl)
{
  GdkChildInfoX11 *child = &state->child_info[state->n_children_found];
  child->is_mapped = repl->mapState != IsUnmapped;
  child->window_class = repl->class;
}

static void
handle_get_geometry_reply (Display           *dpy,
			   ChildInfoState    *state,
			   xGetGeometryReply *repl)
{
  GdkChildInfoX11 *child = &state->child_info[state->n_children_found];
  
  child->x = cvtINT16toInt (repl->x);
  child->y = cvtINT16toInt (repl->y);
  child->width = repl->width;
  child->height = repl->height;
}

static void
handle_get_property_reply (Display           *dpy,
			   ChildInfoState    *state,
			   xGetPropertyReply *repl)
{
  GdkChildInfoX11 *child = &state->child_info[state->n_children_found];
  child->has_wm_state = repl->propertyType != None;

  /* Since we called GetProperty with longLength of 0, we don't
   * have to worry about consuming the property data that would
   * normally follow after the reply
   */
}

static void
next_child (ChildInfoState *state)
{
  if (state->current_request == CHILD_INFO_GET_GEOMETRY)
    {
      if (!state->have_error && !state->child_has_error)
	{
	  state->child_info[state->n_children_found].window = state->children[state->current_child];
	  state->n_children_found++;
	}
      state->current_child++;
      if (state->get_wm_state)
	state->current_request = CHILD_INFO_GET_PROPERTY;
      else
	state->current_request = CHILD_INFO_GET_WA;
      state->child_has_error = FALSE;
      state->have_error = FALSE;
    }
  else
    state->current_request++;
}

static Bool
get_child_info_handler (Display *dpy,
			xReply  *rep,
			char    *buf,
			int      len,
			XPointer data)
{
  Bool result = True;
  
  ChildInfoState *state = (ChildInfoState *)data;

  if (dpy->last_request_read != state->child_states[state->current_child].seq[state->current_request])
    return False;
  
  if (rep->generic.type == X_Error)
    {
      state->child_has_error = TRUE;
      if (rep->error.errorCode != BadDrawable ||
	  rep->error.errorCode != BadWindow)
	{
	  state->have_error = TRUE;
	  result = False;
	}
    }
  else
    {
      switch (state->current_request)
	{
	case CHILD_INFO_GET_PROPERTY:
	  {
	    xGetPropertyReply replbuf;
	    xGetPropertyReply *repl;
	    
	    repl = (xGetPropertyReply *)
	      _XGetAsyncReply(dpy, (char *)&replbuf, rep, buf, len,
			      (sizeof(xGetPropertyReply) - sizeof(xReply)) >> 2,
			      True);
	    
	    handle_get_property_reply (dpy, state, repl);
	  }
	  break;
	case CHILD_INFO_GET_WA:
	  {
	    xGetWindowAttributesReply replbuf;
	    xGetWindowAttributesReply *repl;
	    
	    repl = (xGetWindowAttributesReply *)
	      _XGetAsyncReply(dpy, (char *)&replbuf, rep, buf, len,
			      (sizeof(xGetWindowAttributesReply) - sizeof(xReply)) >> 2,
			      True);
	    
	    handle_get_wa_reply (dpy, state, repl);
	  }
	  break;
	case CHILD_INFO_GET_GEOMETRY:
	  {
	    xGetGeometryReply replbuf;
	    xGetGeometryReply *repl;
	    
	    repl = (xGetGeometryReply *)
	      _XGetAsyncReply(dpy, (char *)&replbuf, rep, buf, len,
			      (sizeof(xGetGeometryReply) - sizeof(xReply)) >> 2,
			      True);
	    
	    handle_get_geometry_reply (dpy, state, repl);
	  }
	  break;
	}
    }

  next_child (state);

  return result;
}

gboolean
_gdk_x11_get_window_child_info (GdkDisplay       *display,
				Window            window,
				gboolean          get_wm_state,
				gboolean         *win_has_wm_state,
				GdkChildInfoX11 **children,
				guint            *nchildren)
{
  Display *dpy;
  _XAsyncHandler async;
  ChildInfoState state;
  Atom wm_state_atom;
  gboolean has_wm_state;
  Bool result;
  guint i;

  *children = NULL;
  *nchildren = 0;
  
  dpy = GDK_DISPLAY_XDISPLAY (display);
  if (get_wm_state)
    wm_state_atom = gdk_x11_get_xatom_by_name_for_display (display, "WM_STATE");
  else
    wm_state_atom = None;

  state.children = NULL;
  state.nchildren = 0;

  gdk_error_trap_push ();
  result = list_children_and_wm_state (dpy, window,
				       win_has_wm_state ? wm_state_atom : None,
				       &has_wm_state,
				       &state.children, &state.nchildren);
  gdk_error_trap_pop ();
  if (!result)
    {
      g_free (state.children);
      return FALSE;
    }

  if (has_wm_state)
    {
      if (win_has_wm_state)
	*win_has_wm_state = TRUE;
      g_free (state.children);
      return TRUE;
    }
  else
    {
      if (win_has_wm_state)
	*win_has_wm_state = FALSE;
    }

  state.get_wm_state = get_wm_state;
  state.child_info = g_new (GdkChildInfoX11, state.nchildren);
  state.child_states = g_new (ChildInfoChildState, state.nchildren);
  state.current_child = 0;
  state.n_children_found = 0;
  if (get_wm_state)
    state.current_request = CHILD_INFO_GET_PROPERTY;
  else
    state.current_request = CHILD_INFO_GET_WA;
  state.have_error = FALSE;
  state.child_has_error = FALSE;

  LockDisplay(dpy);

  async.next = dpy->async_handlers;
  async.handler = get_child_info_handler;
  async.data = (XPointer) &state;
  dpy->async_handlers = &async;
  
  for (i = 0; i < state.nchildren; i++)
    {
      xResourceReq *resource_req;
      xGetPropertyReq *prop_req;
      Window window = state.children[i];
      
      if (get_wm_state)
	{
	  GetReq (GetProperty, prop_req);
	  prop_req->window = window;
	  prop_req->property = wm_state_atom;
	  prop_req->type = AnyPropertyType;
	  prop_req->delete = False;
	  prop_req->longOffset = 0;
	  prop_req->longLength = 0;

	  state.child_states[i].seq[CHILD_INFO_GET_PROPERTY] = dpy->request;
	}
      
      GetResReq(GetWindowAttributes, window, resource_req);
      state.child_states[i].seq[CHILD_INFO_GET_WA] = dpy->request;
      
      GetResReq(GetGeometry, window, resource_req);
      state.child_states[i].seq[CHILD_INFO_GET_GEOMETRY] = dpy->request;
    }

  if (i != 0)
    {
      /* Wait for the last reply
       */
      xGetGeometryReply rep;

      /* On error, our async handler will get called
       */
      if (_XReply (dpy, (xReply *)&rep, 0, xTrue))
	handle_get_geometry_reply (dpy, &state, &rep);

      next_child (&state);
    }

  if (!state.have_error)
    {
      *children = state.child_info;
      *nchildren = state.n_children_found;
    }
  else
    {
      g_free (state.child_info);
    }

  g_free (state.children);
  g_free (state.child_states);
  
  DeqAsyncHandler(dpy, &async);
  UnlockDisplay(dpy);
  SyncHandle();

  return !state.have_error;
}

static gboolean
roundtrip_callback_idle (gpointer data)
{
  RoundtripState *state = (RoundtripState *)data;  
  
  state->callback (state->display, state->data, state->get_input_focus_req);

  g_free (state);

  return FALSE;
}

static Bool
roundtrip_handler (Display *dpy,
		   xReply  *rep,
		   char    *buf,
		   int      len,
		   XPointer data)
{
  RoundtripState *state = (RoundtripState *)data;  
  
  if (dpy->last_request_read == state->get_input_focus_req)
    {
      xGetInputFocusReply replbuf;
      xGetInputFocusReply *repl;
      
      if (rep->generic.type != X_Error)
	{
	  /* Actually does nothing, since there are no additional bytes
	   * to read, but maintain good form.
	   */
	  repl = (xGetInputFocusReply *)
	    _XGetAsyncReply(dpy, (char *)&replbuf, rep, buf, len,
			    (sizeof(xGetInputFocusReply) - sizeof(xReply)) >> 2,
			    True);
	}

      
      if (state->callback)
        gdk_threads_add_idle (roundtrip_callback_idle, state);

      DeqAsyncHandler(state->dpy, &state->async);

      return (rep->generic.type != X_Error);
    }

  return False;
}

void
_gdk_x11_roundtrip_async (GdkDisplay           *display, 
			  GdkRoundTripCallback callback,
			  gpointer              data)
{
  Display *dpy;
  RoundtripState *state;
  
  dpy = GDK_DISPLAY_XDISPLAY (display);

  state = g_new (RoundtripState, 1);

  state->display = display;
  state->dpy = dpy;
  state->callback = callback;
  state->data = data;
  
  LockDisplay(dpy);

  state->async.next = dpy->async_handlers;
  state->async.handler = roundtrip_handler;
  state->async.data = (XPointer) state;
  dpy->async_handlers = &state->async;

  /*
   * XSync (dpy, 0)
   */
  {
    xReq *req;
    
    GetEmptyReq(GetInputFocus, req);
    state->get_input_focus_req = dpy->request;
  }
  
  UnlockDisplay(dpy);
  SyncHandle();
}

#define __GDK_ASYNC_C__
#include "gdkaliasdef.c"
