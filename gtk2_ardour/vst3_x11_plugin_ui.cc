/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <boost/unordered_map.hpp>

#include <glibmm/main.h>
#include <glibmm/threads.h>

#include <gtkmm/socket.h>

#include "pbd/unwind.h"

#include "ardour/plugin_insert.h"
#include "ardour/vst3_plugin.h"

#include "gtkmm2ext/gui_thread.h"

#include "vst3_x11_plugin_ui.h"

#include <gdk/gdkx.h> /* must come later than glibmm/object.h */

using namespace ARDOUR;
using namespace Steinberg;

class VST3X11Runloop : public Linux::IRunLoop
{
private:
	struct EventHandler
	{
		EventHandler (Linux::IEventHandler* handler = 0, GIOChannel* gio_channel = 0, guint source_id = 0)
			: _handler (handler)
			, _gio_channel (gio_channel)
			, _source_id (source_id)
		{}

		bool operator== (EventHandler const& other) {
			return other._handler == _handler && other._gio_channel == _gio_channel && other._source_id == _source_id;
		}
		Linux::IEventHandler* _handler;
		GIOChannel*           _gio_channel;
		guint                 _source_id;
	};

	boost::unordered_map<FileDescriptor, EventHandler> _event_handlers;
	boost::unordered_map<guint, Linux::ITimerHandler*> _timer_handlers;

	static gboolean event (GIOChannel* source, GIOCondition condition, gpointer data)
	{
		Linux::IEventHandler* handler = reinterpret_cast<Linux::IEventHandler*> (data);
		handler->onFDIsSet (g_io_channel_unix_get_fd (source));
		if (condition & ~G_IO_IN) {
			/* remove on error */
			return false;
		} else {
			return true;
		}
	}

	static gboolean timeout (gpointer data)
	{
		Linux::ITimerHandler* handler = reinterpret_cast<Linux::ITimerHandler*> (data);
		handler->onTimer ();
		return true;
	}

public:
	~VST3X11Runloop ()
	{
		clear ();
	}

	void clear () {
		Glib::Threads::Mutex::Lock lm (_lock);
		for (boost::unordered_map<FileDescriptor, EventHandler>::const_iterator it = _event_handlers.begin (); it != _event_handlers.end (); ++it) {
			g_source_remove (it->second._source_id);
			g_io_channel_unref (it->second._gio_channel);
		}
		for (boost::unordered_map<guint, Linux::ITimerHandler*>::const_iterator it = _timer_handlers.begin (); it != _timer_handlers.end (); ++it) {
			g_source_remove (it->first);
		}
		_event_handlers.clear ();
		_timer_handlers.clear ();
	}

	/* VST3 IRunLoop interface */
	tresult registerEventHandler (Linux::IEventHandler* handler, FileDescriptor fd) SMTG_OVERRIDE
	{
		if (!handler || _event_handlers.find(fd) != _event_handlers.end()) {
			return kInvalidArgument;
		}

		Glib::Threads::Mutex::Lock lm (_lock);
		GIOChannel* gio_channel = g_io_channel_unix_new (fd);
		guint id = g_io_add_watch (gio_channel, (GIOCondition) (G_IO_IN /*| G_IO_OUT*/ | G_IO_ERR | G_IO_HUP), event, handler);
		_event_handlers[fd] = EventHandler (handler, gio_channel, id);
		return kResultTrue;
	}

	tresult unregisterEventHandler (Linux::IEventHandler* handler) SMTG_OVERRIDE
	{
		if (!handler) {
			return kInvalidArgument;
		}

		tresult rv = false;
		Glib::Threads::Mutex::Lock lm (_lock);
		for (boost::unordered_map<FileDescriptor, EventHandler>::const_iterator it = _event_handlers.begin (); it != _event_handlers.end ();) {
			if (it->second._handler == handler) {
				g_source_remove (it->second._source_id);
				g_io_channel_unref (it->second._gio_channel);
				it = _event_handlers.erase (it);
				rv = kResultTrue;
			} else {
				++it;
			}
		}
		return rv;
	}

	tresult registerTimer (Linux::ITimerHandler* handler, TimerInterval milliseconds) SMTG_OVERRIDE
	{
		if (!handler || milliseconds == 0) {
			return kInvalidArgument;
		}
		Glib::Threads::Mutex::Lock lm (_lock);
		guint id = g_timeout_add_full (G_PRIORITY_HIGH_IDLE, milliseconds, timeout, handler, NULL);
		_timer_handlers[id] = handler;
		return kResultTrue;

	}

	tresult unregisterTimer (Linux::ITimerHandler* handler) SMTG_OVERRIDE
	{
		if (!handler) {
			return kInvalidArgument;
		}

		tresult rv = false;
		Glib::Threads::Mutex::Lock lm (_lock);
		for (boost::unordered_map<guint, Linux::ITimerHandler*>::const_iterator it = _timer_handlers.begin (); it != _timer_handlers.end ();) {
			if (it->second == handler) {
				g_source_remove (it->first);
				it = _timer_handlers.erase (it);
				rv = kResultTrue;
			} else {
				++it;
			}
		}
		return rv;
	}

	uint32 PLUGIN_API addRef () SMTG_OVERRIDE { return 1; }
	uint32 PLUGIN_API release () SMTG_OVERRIDE { return 1; }
	tresult queryInterface (const TUID, void**) SMTG_OVERRIDE { return kNoInterface; }

private:
	Glib::Threads::Mutex _lock;
};

VST3X11Runloop static_runloop;

VST3X11PluginUI::VST3X11PluginUI (boost::shared_ptr<PluginInsert> pi, boost::shared_ptr<VST3Plugin> vst3)
	: VST3PluginUI (pi, vst3)
	//, _runloop (new VST3X11Runloop)
{
	_vst3->set_runloop (&static_runloop);

	pack_start (_gui_widget, true, true);

	_gui_widget.signal_realize().connect (mem_fun (this, &VST3X11PluginUI::view_realized));
	_gui_widget.signal_size_request ().connect (mem_fun (this, &VST3X11PluginUI::view_size_request));
	_gui_widget.signal_size_allocate ().connect (mem_fun (this, &VST3X11PluginUI::view_size_allocate));
	_gui_widget.signal_scroll_event ().connect (sigc::mem_fun (*this, &VST3X11PluginUI::forward_scroll_event), false);

#if 0
	_gui_widget.add_events (Gdk::POINTER_MOTION_HINT_MASK | Gdk::SCROLL_MASK | Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK | Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK|Gdk::SCROLL_MASK);
#endif

	_gui_widget.show ();
}

VST3X11PluginUI::~VST3X11PluginUI ()
{
	assert (_view_realized);
	_vst3->close_view ();
}

void
VST3X11PluginUI::view_realized ()
{
	IPlugView* view = _vst3->view ();
	if (!view) {
		return;
	}
	Window window = _gui_widget.get_id ();
	if (kResultOk != view->attached (reinterpret_cast<void*> (window), Steinberg::kPlatformTypeX11EmbedWindowID)) {
		assert (0);
	}
	_view_realized = true;
#if 0
	_gui_widget.set_sensitive (true);
	_gui_widget.set_can_focus (true);
	_gui_widget.grab_focus ();
#endif

	ViewRect rect;
	if (view->getSize (&rect) == kResultOk) {
		_req_width  = rect.right - rect.left;
		_req_height = rect.bottom - rect.top;
	}
	_gui_widget.queue_resize ();
}

void
VST3X11PluginUI::view_size_request (GtkRequisition* requisition)
{
	requisition->width  = _req_width;
	requisition->height = _req_height;
}

void
VST3X11PluginUI::view_size_allocate (Gtk::Allocation& allocation)
{
	IPlugView* view = _vst3->view ();
	if (!view || !_view_realized) {
		return;
	}
	PBD::Unwinder<bool> uw (_resize_in_progress, true);

	ViewRect rect;
	if (view->getSize (&rect) == kResultOk
	    && ! (rect.right - rect.left == allocation.get_width () && rect.bottom - rect.top ==  allocation.get_height ()))
	{
		rect.right = rect.left + allocation.get_width ();
		rect.bottom = rect.top + allocation.get_height ();
#if 0
		if (view->checkSizeConstraint (&rect) != kResultTrue) {
			view->getSize (&rect);
		}
		allocation.set_width (rect.right - rect.left);
		allocation.set_height (rect.bottom - rect.top);
#endif
		if (view->canResize() == kResultTrue) {
			view->onSize (&rect);
		}
	}
}

void
VST3X11PluginUI::resize_callback (int width, int height)
{
	// printf ("VST3X11PluginUI::resize_callback %d x %d\n", width, height);
	IPlugView* view = _vst3->view ();
	if (!view || _resize_in_progress) {
		return;
	}
	if (view->canResize() == kResultTrue) {
		gint xx, yy;
		if (gtk_widget_translate_coordinates (
		    GTK_WIDGET(_gui_widget.gobj()),
		    GTK_WIDGET(get_toplevel()->gobj()),
		    0, 0, &xx, &yy))
		{
			get_window()->resize (width + xx, height + yy);
		}
	} else {
		_req_width  = width;
		_req_height = height;
		_gui_widget.queue_resize ();
	}
}

bool
VST3X11PluginUI::on_window_show (const std::string& /*title*/)
{
	IPlugView* view = _vst3->view ();
	if (!view) {
		return false;
	}
	gtk_widget_realize (GTK_WIDGET(_gui_widget.gobj()));
	_gui_widget.show_all ();
	_gui_widget.queue_resize ();
	return true;
}

void
VST3X11PluginUI::on_window_hide ()
{
	_gui_widget.hide ();
}

void
VST3X11PluginUI::grab_focus ()
{
#if 1
	IPlugView* view = _vst3->view ();
	if (view) {
		view->onFocus (true);
	}
#endif
}
