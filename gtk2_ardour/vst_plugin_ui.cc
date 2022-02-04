/*
 * Copyright (C) 2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#include <gtkmm.h>
#include "ardour/auditioner.h"
#include "ardour/session.h"
#include "ardour/vst_plugin.h"
#include "ardour/vst_types.h"
#include "ardour/plugin_insert.h"
#include "vst_plugin_ui.h"

#ifdef PLATFORM_WINDOWS
#include <gdk/gdkwin32.h>
#elif defined __APPLE__
// TODO
#else
#include <gdk/gdkx.h>
#endif

VSTPluginUI::VSTPluginUI (boost::shared_ptr<ARDOUR::PluginInsert> insert, boost::shared_ptr<ARDOUR::VSTPlugin> plugin)
	: PlugUIBase (insert)
	, _vst (plugin)
{
	Gtk::HBox* box = manage (new Gtk::HBox);
	box->set_spacing (6);
	box->set_border_width (6);

	bool for_auditioner =false;
	if (insert->session().the_auditioner()) {
		for_auditioner = insert->session().the_auditioner()->the_instrument() == insert;
	}
	if (!for_auditioner) {
		add_common_widgets (box);
	}

	pack_start (*box, false, false);
	box->signal_size_allocate().connect (sigc::mem_fun (*this, &VSTPluginUI::top_box_allocated));
#ifdef GDK_WINDOWING_X11
	pack_start (_socket, true, true);
	_socket.set_border_width (0);
#endif
}

VSTPluginUI::~VSTPluginUI ()
{

}

void
VSTPluginUI::preset_selected (ARDOUR::Plugin::PresetRecord preset)
{
#ifdef GDK_WINDOWING_X11
	_socket.grab_focus ();
#endif
	PlugUIBase::preset_selected (preset);
}

int
VSTPluginUI::get_preferred_height ()
{
	return _vst->state()->height + _vst->state()->voffset;
}

int
VSTPluginUI::get_preferred_width ()
{
	return _vst->state()->width + _vst->state()->hoffset;
}

int
VSTPluginUI::package (Gtk::Window& win)
{
#ifdef GDK_WINDOWING_X11
	/* Forward configure events to plugin window */
	win.signal_configure_event().connect (sigc::mem_fun (*this, &VSTPluginUI::configure_handler), false);

	/* This assumes that the window's owner understands the XEmbed protocol */
	_socket.add_id (get_XID ());
	_socket.set_size_request(
			_vst->state()->width + _vst->state()->hoffset,
			_vst->state()->height + _vst->state()->voffset);
#endif

	return 0;
}

bool
VSTPluginUI::on_window_show(const std::string& title)
{
	_vst->state()->gui_shown = 1;
	return PlugUIBase::on_window_show(title);
}

void
VSTPluginUI::on_window_hide()
{
	_vst->state()->gui_shown = 0;
	PlugUIBase::on_window_hide();
}


bool
VSTPluginUI::configure_handler (GdkEventConfigure*)
{
#ifdef GDK_WINDOWING_X11
	XEvent event;
	gint x, y;
	GdkWindow* w;
	Window xw = _vst->state()->linux_plugin_ui_window;

	if ((w = _socket.gobj()->plug_window) == 0) {
		return false;
	}

	event.xconfigure.type = ConfigureNotify;
	event.xconfigure.event = GDK_WINDOW_XWINDOW (w);
	event.xconfigure.window = GDK_WINDOW_XWINDOW (w);

	/* The ICCCM says that synthetic events should have root relative
	 * coordinates. We still aren't really ICCCM compliant, since
	 * we don't send events when the real toplevel is moved.
	 */
	gdk_error_trap_push ();
	gdk_window_get_origin (w, &x, &y);
	gdk_error_trap_pop ();

	event.xconfigure.x = x;
	event.xconfigure.y = y;
	event.xconfigure.width = GTK_WIDGET (_socket.gobj())->allocation.width;
	event.xconfigure.height = GTK_WIDGET (_socket.gobj())->allocation.height;

	event.xconfigure.border_width = 0;
	event.xconfigure.above = None;
	event.xconfigure.override_redirect = False;

	gdk_error_trap_push ();
	XSendEvent (GDK_WINDOW_XDISPLAY (w), GDK_WINDOW_XWINDOW (w), False, StructureNotifyMask, &event);
	/* if the plugin does adds itself to the parent,
	 * but ardour re-parents it, we have a pointer to
	 * the socket's child and need to resize the
	 * child window (e.g. JUCE, u-he)
	 */
	if (xw) {
		XMoveResizeWindow (GDK_WINDOW_XDISPLAY (w), xw,
				0, 0, _vst->state()->width, _vst->state()->height);
		XMapRaised (GDK_WINDOW_XDISPLAY (w), xw);
		XFlush (GDK_WINDOW_XDISPLAY (w));
	}
	gdk_error_trap_pop ();
#endif

	return false;
}

bool
VSTPluginUI::dispatch_effeditkey (GdkEventKey* gdk_key)
{
	int effopcode;
	switch (gdk_key->type) {
		case GDK_KEY_PRESS:
			effopcode = 59; // effEditKeyDown
			break;
		case GDK_KEY_RELEASE:
			effopcode = 60; // effEditKeyUp
			break;
		default:
			return false;
	}

	/* see https://github.com/DISTRHO/DPF/blob/master/distrho/src/DistrhoPluginVST.cpp
	 * and https://github.com/steinbergmedia/vstgui/blob/develop/vstgui/lib/vstkeycode.h#L19
	 */
	int special_key = 0;
	int ascii_key = 0;

	switch (gdk_key->keyval) {
		case GDK_BackSpace:
			special_key = 1;
			break;
		case GDK_Tab:
		case GDK_KP_Tab:
			special_key = 2;
			break;
		case GDK_Return:
			special_key = 4;
			break;
		case GDK_KP_Enter:
			special_key = 19;
			break;
		case GDK_Escape:
			special_key = 6;
			break;
		case GDK_KP_Space:
			special_key = 7;
			break;

		case GDK_End:
		case GDK_KP_End:
			special_key = 9;
			break;
		case GDK_Home:
		case GDK_KP_Home:
			special_key = 10;
			break;
		case GDK_Left:
			special_key = 11;
			break;
		case GDK_Up:
			special_key = 12;
			break;
		case GDK_Right:
			special_key = 13;
			break;
		case GDK_Down:
			special_key = 14;
			break;
		case GDK_Page_Up:
		case GDK_KP_Page_Up:
			special_key = 15;
			break;
		case GDK_Page_Down:
			/* fallthrough */
		case GDK_KP_Page_Down:
			special_key = 16;
			break;
		case GDK_Insert:
			special_key = 21;
			break;
		case GDK_Delete:
		case GDK_KP_Delete:
			special_key = 22;
			break;

		case GDK_Shift_L:
		case GDK_Shift_R:
			special_key = 54;
			break;
		case GDK_Control_L:
		case GDK_Control_R:
			special_key = 55;
			break;
		case GDK_Alt_L:
		case GDK_Alt_R:
			special_key = 56;
			break;

		case GDK_F1:  special_key = 40; break;
		case GDK_F2:  special_key = 41; break;
		case GDK_F3:  special_key = 42; break;
		case GDK_F4:  special_key = 43; break;
		case GDK_F5:  special_key = 44; break;
		case GDK_F6:  special_key = 45; break;
		case GDK_F7:  special_key = 46; break;
		case GDK_F8:  special_key = 47; break;
		case GDK_F9:  special_key = 48; break;
		case GDK_F10: special_key = 49; break;
		case GDK_F11: special_key = 50; break;
		case GDK_F12: special_key = 51; break;

		default:
			ascii_key = gdk_key->keyval;
			break;
	}

	if (special_key > 0 || ascii_key > 0) {
		VSTState* vstfx = _vst->state();
		/* expect non-zero return if key was handled */
		return 0 != vstfx->plugin->dispatcher (vstfx->plugin, effopcode, (int)ascii_key, (intptr_t)special_key, NULL, 0);
	}
	return false;
}
