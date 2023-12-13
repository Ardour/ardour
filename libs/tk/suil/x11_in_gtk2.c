/*
  Copyright 2011-2020 David Robillard <http://drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "suil_internal.h"

#include "lv2/core/lv2.h"
#include "lv2/options/options.h"
#include "lv2/ui/ui.h"
#include "lv2/urid/urid.h"
#include "suil/suil.h"

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <glib-object.h>
#include <glib.h>
#include <gobject/gclosure.h>
#include <gtk/gtk.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	bool is_set;
	int  width;
	int  height;
} SuilX11SizeHints;

typedef struct {
	GtkSocket                   socket;
	GtkPlug*                    plug;
	SuilWrapper*                wrapper;
	SuilInstance*               instance;
	const LV2UI_Idle_Interface* idle_iface;
	guint                       idle_id;
	guint                       idle_ms;
	SuilX11SizeHints            max_size;
	SuilX11SizeHints            custom_size;
	SuilX11SizeHints            base_size;
	SuilX11SizeHints            min_size;
	bool                        query_wm;
} SuilX11Wrapper;

typedef struct {
	GtkSocketClass parent_class;
} SuilX11WrapperClass;

GType suil_x11_wrapper_get_type(void);  // Accessor for SUIL_TYPE_X11_WRAPPER

#define SUIL_TYPE_X11_WRAPPER (suil_x11_wrapper_get_type())
#define SUIL_X11_WRAPPER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SUIL_TYPE_X11_WRAPPER, SuilX11Wrapper))

G_DEFINE_TYPE(SuilX11Wrapper, suil_x11_wrapper, GTK_TYPE_SOCKET)

/**
   Check if 'swallowed' subwindow is known to the X server.

   Gdk/GTK can mark the window as realized, mapped and visible even though
   there is no window-ID on the X server for it yet.  Then,
   suil_x11_on_size_allocate() will cause a "BadWinow" X error.
*/
static bool
x_window_is_valid(SuilX11Wrapper* socket)
{
	GdkWindow* window     = gtk_widget_get_window(GTK_WIDGET(socket->plug));
	Window     root       = 0;
	Window     parent     = 0;
	Window*    children   = NULL;
	unsigned   childcount = 0;

	XQueryTree(GDK_WINDOW_XDISPLAY(window),
	           GDK_WINDOW_XID(window),
	           &root, &parent, &children, &childcount);
	for (unsigned i = 0; i < childcount; ++i) {
		if (children[i] == (Window)socket->instance->ui_widget) {
			XFree(children);
			return true;
		}
	}
	if (children) {
		XFree(children);
	}
	return false;
}

static Window
get_parent_window(Display* display, Window child)
{
	Window   root     = 0;
	Window   parent   = 0;
	Window*  children = NULL;
	unsigned count    = 0;

	if (child) {
		if (XQueryTree(display, child, &root, &parent, &children, &count)) {
			if (children) {
				XFree(children);
			}
		}
	}

	return (parent == root) ? 0 : parent;
}

static gboolean
on_plug_removed(GtkSocket* sock, gpointer data)
{
	(void)data;

	SuilX11Wrapper* const self = SUIL_X11_WRAPPER(sock);

	if (self->idle_id) {
		g_source_remove(self->idle_id);
		self->idle_id = 0;
	}

	if (self->instance->handle) {
		self->instance->descriptor->cleanup(self->instance->handle);
		self->instance->handle = NULL;
	}

	self->plug = NULL;
	return TRUE;
}

static void
suil_x11_wrapper_finalize(GObject* gobject)
{
	SuilX11Wrapper* const self = SUIL_X11_WRAPPER(gobject);

	self->wrapper->impl = NULL;

	G_OBJECT_CLASS(suil_x11_wrapper_parent_class)->finalize(gobject);
}

static void
suil_x11_wrapper_realize(GtkWidget* w)
{
	SuilX11Wrapper* const wrap   = SUIL_X11_WRAPPER(w);
	GtkSocket* const      socket = GTK_SOCKET(w);

	if (GTK_WIDGET_CLASS(suil_x11_wrapper_parent_class)->realize) {
		GTK_WIDGET_CLASS(suil_x11_wrapper_parent_class)->realize(w);
	}

	gtk_socket_add_id(socket, gtk_plug_get_id(wrap->plug));

	gtk_widget_set_sensitive(GTK_WIDGET(wrap->plug), TRUE);
	gtk_widget_set_can_focus(GTK_WIDGET(wrap->plug), TRUE);
	gtk_widget_grab_focus(GTK_WIDGET(wrap->plug));

	// Setup drag/drop proxy from parent/grandparent window
	GdkWindow* gwindow         = gtk_widget_get_window(GTK_WIDGET(wrap->plug));
	Window     xwindow         = GDK_WINDOW_XID(gwindow);
	Atom       xdnd_proxy_atom = gdk_x11_get_xatom_by_name("XdndProxy");
	Window     plugin          = (Window)wrap->instance->ui_widget;

	while (xwindow) {
		XChangeProperty(GDK_WINDOW_XDISPLAY(gwindow),
		                xwindow,
		                xdnd_proxy_atom,
		                XA_WINDOW,
		                32,
		                PropModeReplace,
		                (unsigned char*)&plugin,
		                1);

		xwindow = get_parent_window(GDK_WINDOW_XDISPLAY(gwindow), xwindow);
	}
}

static void
suil_x11_wrapper_show(GtkWidget* w)
{
	SuilX11Wrapper* const wrap = SUIL_X11_WRAPPER(w);

	if (GTK_WIDGET_CLASS(suil_x11_wrapper_parent_class)->show) {
		GTK_WIDGET_CLASS(suil_x11_wrapper_parent_class)->show(w);
	}

	gtk_widget_show(GTK_WIDGET(wrap->plug));
}

static gboolean
forward_key_event(SuilX11Wrapper* socket,
                  GdkEvent*       gdk_event)
{
	GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(socket->plug));
	GdkScreen* screen = gdk_visual_get_screen(gdk_window_get_visual(window));

	Window target_window = 0;
	if (gdk_event->any.window == window) {
		// Event sent up to the plug window, forward it up to the parent
		GtkWidget* widget = GTK_WIDGET(socket->instance->host_widget);
		GdkWindow* parent = gtk_widget_get_parent_window(widget);
		if (parent) {
			target_window = GDK_WINDOW_XID(parent);
		} else {
			return FALSE;  // Wrapper is a top-level window, do nothing
		}
	} else {
		// Event sent anywhere else, send to the plugin
		target_window = (Window)socket->instance->ui_widget;
	}

	XKeyEvent xev;
	memset(&xev, 0, sizeof(xev));
	xev.type      = (gdk_event->type == GDK_KEY_PRESS) ? KeyPress : KeyRelease;
	xev.root      = GDK_WINDOW_XID(gdk_screen_get_root_window(screen));
	xev.window    = target_window;
	xev.subwindow = None;
	xev.time      = gdk_event->key.time;
	xev.state     = gdk_event->key.state;
	xev.keycode   = gdk_event->key.hardware_keycode;

	XSendEvent(GDK_WINDOW_XDISPLAY(window),
	           target_window,
	           False,
	           NoEventMask,
	           (XEvent*)&xev);

	return (gdk_event->any.window != window);
}

static gboolean
idle_size_request(gpointer user_data)
{
	GtkWidget* w = GTK_WIDGET(user_data);
	gtk_widget_queue_resize(w);
	return FALSE;
}

/// Read XSizeHints and store the values for later use
static void
query_wm_hints(SuilX11Wrapper* wrap)
{
	GdkWindow* window   = gtk_widget_get_window(GTK_WIDGET(wrap->plug));
	XSizeHints hints    = {0};
	long       supplied = 0;

	XGetWMNormalHints(GDK_WINDOW_XDISPLAY(window),
	                  (Window)wrap->instance->ui_widget,
	                  &hints,
	                  &supplied);

	if (hints.flags & PMaxSize) {
		wrap->max_size.width  = hints.max_width;
		wrap->max_size.height = hints.max_height;
		wrap->max_size.is_set = true;
	}
	if (hints.flags & PBaseSize) {
		wrap->base_size.width  = hints.base_width;
		wrap->base_size.height = hints.base_height;
		wrap->base_size.is_set = true;
	}
	if (hints.flags & PMinSize) {
		wrap->min_size.width  = hints.min_width;
		wrap->min_size.height = hints.min_height;
		wrap->min_size.is_set = true;
	}

	wrap->query_wm = false;
}

static void
forward_size_request(SuilX11Wrapper* socket,
                     GtkAllocation*  allocation)
{
	GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(socket->plug));
	if (x_window_is_valid(socket)) {
		// Calculate allocation size constrained to X11 limits for widget
		int        width  = allocation->width;
		int        height = allocation->height;

		if (socket->query_wm) {
			query_wm_hints(socket);
		}

		if (socket->max_size.is_set) {
			width  = MIN(width, socket->max_size.width);
			height = MIN(height, socket->max_size.height);
		}
		if (socket->min_size.is_set) {
			width  = MAX(width, socket->min_size.width);
			height = MAX(height, socket->min_size.height);
		}

		// Resize widget window
		XResizeWindow(GDK_WINDOW_XDISPLAY(window),
		              (Window)socket->instance->ui_widget,
		              (unsigned)width, (unsigned)height);

		// Get actual widget geometry
		Window       root    = 0;
		int          wx      = 0;
		int          wy      = 0;
		unsigned int ww      = 0;
		unsigned int wh      = 0;
		unsigned int ignored = 0;
		XGetGeometry(GDK_WINDOW_XDISPLAY(window),
		             (Window)socket->instance->ui_widget,
		             &root,
		             &wx, &wy, &ww, &wh,
		             &ignored, &ignored);

		// Center widget in allocation
		wx = (allocation->width  - (int)ww) / 2;
		wy = (allocation->height - (int)wh) / 2;
		XMoveWindow(GDK_WINDOW_XDISPLAY(window),
		            (Window)socket->instance->ui_widget,
		            wx, wy);
	} else {
		/* Child has not been realized, so unable to resize now.
		   Queue an idle resize. */
		g_idle_add(idle_size_request, socket->plug);
	}
}

static gboolean
suil_x11_wrapper_key_event(GtkWidget*   widget,
                           GdkEventKey* event)
{
	SuilX11Wrapper* const self = SUIL_X11_WRAPPER(widget);

	if (self->plug) {
		return forward_key_event(self, (GdkEvent*)event);
	}

	return FALSE;
}

static void
suil_x11_on_size_request(GtkWidget*      widget,
                         GtkRequisition* requisition)
{
	SuilX11Wrapper* const self = SUIL_X11_WRAPPER(widget);

	if (self->custom_size.is_set) {
		requisition->width  = self->custom_size.width;
		requisition->height = self->custom_size.height;
	} else if (self->base_size.is_set) {
		requisition->width  = self->base_size.width;
		requisition->height = self->base_size.height;
	} else if (self->min_size.is_set) {
		requisition->width  = self->min_size.width;
		requisition->height = self->min_size.height;
	}
}

static void
suil_x11_on_size_allocate(GtkWidget*     widget,
                          GtkAllocation* a)
{
	SuilX11Wrapper* const self = SUIL_X11_WRAPPER(widget);

	if (self->plug
	    && GTK_WIDGET_REALIZED(widget)
	    && GTK_WIDGET_MAPPED(widget)
	    && GTK_WIDGET_VISIBLE(widget)) {
		forward_size_request(self, a);
	}
}

static void
suil_x11_on_map_event(GtkWidget* widget, GdkEvent* event)
{
	(void)event;

	SuilX11Wrapper* const self = SUIL_X11_WRAPPER(widget);

	/* Reset the size request to the minimum sizes.  This is called after the
	   initial size negotiation, where Gtk called suil_x11_on_size_request() to
	   get the size request, which might be bigger than the minimum size.
	   However, the Gtk2 size model has no proper way to handle minimum and
	   default sizes, so hack around this by setting the size request
	   properties (which really mean minimum size) back to the minimum after
	   the widget is mapped.  This makes it possible for the initial mapping to
	   use the default size, but still allow the user to resize the widget
	   smaller, down to the minimum size. */

	if ((self->custom_size.is_set || self->base_size.is_set) &&
	    self->min_size.is_set) {
		g_object_set(G_OBJECT(GTK_WIDGET(self)),
		             "width-request", self->min_size.width,
		             "height-request", self->min_size.height,
		             NULL);
	}
}

static void
suil_x11_wrapper_class_init(SuilX11WrapperClass* klass)
{
	GObjectClass* const   gobject_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass* const widget_class  = GTK_WIDGET_CLASS(klass);

	gobject_class->finalize         = suil_x11_wrapper_finalize;
	widget_class->realize           = suil_x11_wrapper_realize;
	widget_class->show              = suil_x11_wrapper_show;
	widget_class->key_press_event   = suil_x11_wrapper_key_event;
	widget_class->key_release_event = suil_x11_wrapper_key_event;
}

static void
suil_x11_wrapper_init(SuilX11Wrapper* self)
{
	self->plug        = GTK_PLUG(gtk_plug_new(0));
	self->wrapper     = NULL;
	self->instance    = NULL;
	self->idle_iface  = NULL;
	self->idle_ms     = 1000 / 30; // 30 Hz default
	self->max_size    = (SuilX11SizeHints){false, 0, 0};
	self->custom_size = (SuilX11SizeHints){false, 0, 0};
	self->base_size   = (SuilX11SizeHints){false, 0, 0};
	self->min_size    = (SuilX11SizeHints){false, 0, 0};
	self->query_wm    = true;
}

static int
wrapper_resize(LV2UI_Feature_Handle handle, int width, int height)
{
	SuilX11Wrapper* const wrap = SUIL_X11_WRAPPER(handle);

	wrap->custom_size.width  = width;
	wrap->custom_size.height = height;
	wrap->custom_size.is_set = width > 0 && height > 0;

	// Assume the plugin has also updated min/max size constraints
	wrap->query_wm = true;

	gtk_widget_queue_resize(GTK_WIDGET(handle));
	return 0;
}

static gboolean
suil_x11_wrapper_idle(void* data)
{
	SuilX11Wrapper* const wrap = SUIL_X11_WRAPPER(data);

	wrap->idle_iface->idle(wrap->instance->handle);

	return TRUE;  // Continue calling
}

static int
wrapper_wrap(SuilWrapper*  wrapper,
             SuilInstance* instance)
{
	SuilX11Wrapper* const wrap = SUIL_X11_WRAPPER(wrapper->impl);

	instance->host_widget = GTK_WIDGET(wrap);
	wrap->wrapper         = wrapper;
	wrap->instance        = instance;

	GdkWindow*  window   = gtk_widget_get_window(GTK_WIDGET(wrap->plug));
	GdkDisplay* display  = gdk_window_get_display(window);
	Display*    xdisplay = GDK_WINDOW_XDISPLAY(window);
	Window      xwindow  = (Window)instance->ui_widget;

	gdk_display_sync(display);
	if (x_window_is_valid(wrap)) {
		XWindowAttributes attrs;
		XGetWindowAttributes(xdisplay, xwindow, &attrs);

		query_wm_hints(wrap);

		if (!wrap->base_size.is_set) {
			// Fall back to using initial size as base size
			wrap->base_size.is_set = true;
			wrap->base_size.width  = attrs.width;
			wrap->base_size.height = attrs.height;
		}
	}

	const LV2UI_Idle_Interface* idle_iface = NULL;
	if (instance->descriptor->extension_data) {
		idle_iface = (const LV2UI_Idle_Interface*)
			instance->descriptor->extension_data(LV2_UI__idleInterface);
	}
	if (idle_iface) {
		wrap->idle_iface = idle_iface;
		wrap->idle_id    = g_timeout_add(
			wrap->idle_ms, suil_x11_wrapper_idle, wrap);
	}

	g_signal_connect(G_OBJECT(wrap),
	                 "plug-removed",
	                 G_CALLBACK(on_plug_removed),
	                 NULL);

	g_signal_connect(G_OBJECT(wrap),
	                 "size-request",
	                 G_CALLBACK(suil_x11_on_size_request),
	                 NULL);

	g_signal_connect(G_OBJECT(wrap),
	                 "size-allocate",
	                 G_CALLBACK(suil_x11_on_size_allocate),
	                 NULL);

	g_signal_connect(G_OBJECT(wrap),
	                 "map-event",
	                 G_CALLBACK(suil_x11_on_map_event),
	                 NULL);

	return 0;
}

static void
wrapper_free(SuilWrapper* wrapper)
{
	if (wrapper->impl) {
		SuilX11Wrapper* const wrap = SUIL_X11_WRAPPER(wrapper->impl);
		gtk_object_destroy(GTK_OBJECT(wrap));
	}
}

SUIL_LIB_EXPORT
SuilWrapper*
suil_wrapper_new(SuilHost*      host,
                 const char*    host_type_uri,
                 const char*    ui_type_uri,
                 LV2_Feature*** features,
                 unsigned       n_features)
{
	(void)host;
	(void)host_type_uri;
	(void)ui_type_uri;

	SuilWrapper* wrapper = (SuilWrapper*)calloc(1, sizeof(SuilWrapper));
	wrapper->wrap = wrapper_wrap;
	wrapper->free = wrapper_free;

	SuilX11Wrapper* const wrap = SUIL_X11_WRAPPER(
		g_object_new(SUIL_TYPE_X11_WRAPPER, NULL));

	wrapper->impl             = wrap;
	wrapper->resize.handle    = wrap;
	wrapper->resize.ui_resize = wrapper_resize;

	gtk_widget_set_sensitive(GTK_WIDGET(wrap), TRUE);
	gtk_widget_set_can_focus(GTK_WIDGET(wrap), TRUE);

	const intptr_t parent_id = (intptr_t)gtk_plug_get_id(wrap->plug);
	suil_add_feature(features, &n_features, LV2_UI__parent, (void*)parent_id);
	suil_add_feature(features, &n_features, LV2_UI__resize, &wrapper->resize);
	suil_add_feature(features, &n_features, LV2_UI__idleInterface, NULL);

	// Scan for URID map and options
	LV2_URID_Map*       map     = NULL;
	LV2_Options_Option* options = NULL;
	for (LV2_Feature** f = *features; *f && (!map || !options); ++f) {
		if (!strcmp((*f)->URI, LV2_OPTIONS__options)) {
			options = (LV2_Options_Option*)(*f)->data;
		} else if (!strcmp((*f)->URI, LV2_URID__map)) {
			map = (LV2_URID_Map*)(*f)->data;
		}
	}

	if (map && options) {
		// Set UI update rate if given
		LV2_URID ui_updateRate = map->map(map->handle, LV2_UI__updateRate);
		for (LV2_Options_Option* o = options; o->key; ++o) {
			if (o->key == ui_updateRate) {
				wrap->idle_ms = (guint)(1000.0f / *(const float*)o->value);
				break;
			}
		}
	}

	return wrapper;
}
