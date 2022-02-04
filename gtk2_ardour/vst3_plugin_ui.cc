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

#include <glibmm/main.h>

#include "ardour/auditioner.h"
#include "ardour/session.h"
#include "ardour/plugin_insert.h"
#include "ardour/vst3_plugin.h"

#include "gtkmm2ext/gui_thread.h"

#include "timers.h"
#include "ui_config.h"
#include "vst3_plugin_ui.h"

using namespace ARDOUR;
using namespace Steinberg;

#ifdef PLATFORM_WINDOWS
DEF_CLASS_IID (Presonus::IPlugInViewScaling)
#endif

VST3PluginUI::VST3PluginUI (boost::shared_ptr<PluginInsert> pi, boost::shared_ptr<VST3Plugin> vst3)
	: PlugUIBase (pi)
	, _pi (pi)
	, _vst3 (vst3)
	, _req_width (0)
	, _req_height (0)
	, _resize_in_progress (false)
	, _view_realized (false)
{
	_ardour_buttons_box.set_spacing (6);
	_ardour_buttons_box.set_border_width (6);

	bool for_auditioner =false;
	if (insert->session().the_auditioner()) {
		for_auditioner = insert->session().the_auditioner()->the_instrument() == insert;
	}
	if (!for_auditioner) {
		add_common_widgets (&_ardour_buttons_box);
	}

	_vst3->OnResizeView.connect (_resize_connection, invalidator (*this), boost::bind (&VST3PluginUI::resize_callback, this, _1, _2), gui_context());
	//pi->plugin()->PresetLoaded.connect (*this, invalidator (*this), boost::bind (&VST3PluginUI::queue_port_update, this), gui_context ());

	pack_start (_ardour_buttons_box, false, false);
	_ardour_buttons_box.show_all ();
}

VST3PluginUI::~VST3PluginUI ()
{
}

gint
VST3PluginUI::get_preferred_height ()
{
	IPlugView* view = _vst3->view ();
	ViewRect rect;
	if (view && view->getSize (&rect) == kResultOk){
		return rect.bottom - rect.top;
	}
	return 0;
}

gint
VST3PluginUI::get_preferred_width ()
{
	IPlugView* view = _vst3->view ();
	ViewRect rect;
	if (view && view->getSize (&rect) == kResultOk){
		return rect.right - rect.left;
	}
	return 0;
}

bool
VST3PluginUI::resizable ()
{
	IPlugView* view = _vst3->view ();
	return view && view->canResize () == kResultTrue;
}

bool
VST3PluginUI::non_gtk_gui() const
{
	/* return true to enable forward_key_event */
	return false;
}

int
VST3PluginUI::package (Gtk::Window& win)
{
	win.signal_map_event().connect (sigc::mem_fun(*this, &VST3PluginUI::start_updating));
	win.signal_unmap_event().connect (sigc::mem_fun(*this, &VST3PluginUI::stop_updating));

	IPlugView* view = _vst3->view ();
	FUnknownPtr<Presonus::IPlugInViewScaling> vs (view);
	if (vs) {
		vs->setContentScaleFactor (UIConfiguration::instance().get_ui_scale ());
	}

	return 0;
}

bool
VST3PluginUI::start_updating (GdkEventAny*)
{
	_update_connection.disconnect();
	_update_connection = Timers::super_rapid_connect (sigc::mem_fun(*this, &VST3PluginUI::parameter_update));
	return false;
}

bool
VST3PluginUI::stop_updating (GdkEventAny*)
{
	_update_connection.disconnect();
	return false;
}

void
VST3PluginUI::parameter_update ()
{
	// XXX replicated plugins, too ?!
	_vst3->update_contoller_param ();
}

void
VST3PluginUI::forward_key_event (GdkEventKey* ev)
{
	/* NB VST3NSViewPluginUI overrides this */
#if 0 // -> non_gtk_gui () -> true
	// TODO: map key-events
	IPlugView* view = _vst3->view ();
	switch (gdk_key->type) {
		case GDK_KEY_PRESS:
			/* onKeyDown (char16 key, int16 keyCode, int16 modifiers)
			 * key: unicode code of key
			 * keyCode: virtual keycode for non ascii keys - see VirtualKeyCodes in keycodes.h
			 * modifiers	: any combination of modifiers - see KeyModifier in keycodes.h
			 */
			view->onKeyDown (ev->keyval, ev->hardware_keycode, ev->state);
			break;
		case GDK_KEY_RELEASE:
			//view->onKeyUp (key, keyCode, modifiers);
			break;
			break;
		default:
			return;
	}
#endif
}

bool
VST3PluginUI::forward_scroll_event (GdkEventScroll* ev)
{
	IPlugView* view = _vst3->view ();
	switch (ev->direction) {
		case GDK_SCROLL_UP:
		case GDK_SCROLL_LEFT:
			return view->onWheel (-1) == kResultTrue;
			break;
		case GDK_SCROLL_DOWN:
		case GDK_SCROLL_RIGHT:
			return view->onWheel (-1) == kResultTrue;
			break;
	}
	return false;
}
