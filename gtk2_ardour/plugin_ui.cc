/*
    Copyright (C) 2000 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <climits>
#include <cerrno>
#include <cmath>
#include <string>

#include <pbd/stl_delete.h>
#include <pbd/xml++.h>
#include <pbd/failed_constructor.h>

#include <gtkmm/widget.h>
#include <gtkmm2ext/click_box.h>
#include <gtkmm2ext/fastmeter.h>
#include <gtkmm2ext/barcontroller.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/doi.h>
#include <gtkmm2ext/slider_controller.h>
#include <gtkmm2ext/application.h>

#include <midi++/manager.h>

#include <ardour/plugin.h>
#include <ardour/insert.h>
#include <ardour/ladspa_plugin.h>
#ifdef VST_SUPPORT
#include <ardour/vst_plugin.h>
#endif
#ifdef HAVE_SUIL
#include <ardour/lv2_plugin.h>
#include "lv2_plugin_ui.h"
#endif

#include <lrdf.h>

#include "ardour_ui.h"
#include "prompter.h"
#include "plugin_ui.h"
#include "utils.h"
#include "gui_thread.h"
#include "public_editor.h"
#include "keyboard.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace sigc;

PluginUIWindow::PluginUIWindow (Gtk::Window* win, boost::shared_ptr<PluginInsert> insert, bool scrollable)
	: parent (win)
        , was_visible (false)
        , _keyboard_focused (false)
#ifdef HAVE_AUDIOUNITS
	, pre_deactivate_x (-1)                                                                                                          
	, pre_deactivate_y (-1)                                                                                                          
#endif                  
{
	bool have_gui = false;

	if (insert->plugin()->has_editor()) {
		switch (insert->type()) {
		case ARDOUR::VST:
			have_gui = create_vst_editor (insert);
			break;

		case ARDOUR::AudioUnit:
			have_gui = create_audiounit_editor (insert);
			break;
			
		case ARDOUR::LADSPA:
			error << _("Eh? LADSPA plugins don't have editors!") << endmsg;
			break;

		case ARDOUR::LV2:
			have_gui = create_lv2_editor (insert);
			break;

		default:
#ifndef VST_SUPPORT
			error << _("unknown type of editor-supplying plugin (note: no VST support in this version of ardour)")
			      << endmsg;
#else
			error << _("unknown type of editor-supplying plugin")
			      << endmsg;
#endif
			throw failed_constructor ();
		}

	} 

	if (!have_gui) {

		GenericPluginUI*  pu  = new GenericPluginUI (insert, scrollable);
		
		_pluginui = pu;
		_pluginui->KeyboardFocused.connect (sigc::mem_fun (*this, &PluginUIWindow::keyboard_focused));
		add (*pu);
		
		set_wmclass (X_("ardour_plugin_editor"), PROGRAM_NAME);

		signal_map_event().connect (mem_fun (*pu, &GenericPluginUI::start_updating));
		signal_unmap_event().connect (mem_fun (*pu, &GenericPluginUI::stop_updating));
	}

	// set_position (Gtk::WIN_POS_MOUSE);
	set_name ("PluginEditor");
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK|Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);

	signal_delete_event().connect (bind (sigc::ptr_fun (just_hide_it), reinterpret_cast<Window*> (this)), false);
	death_connection = insert->GoingAway.connect (mem_fun(*this, &PluginUIWindow::plugin_going_away));

	gint h = _pluginui->get_preferred_height ();
	gint w = _pluginui->get_preferred_width ();

	if (scrollable) {
		if (h > 600) h = 600;
		if (w > 600) w = 600;

		if (w < 0) {
			w = 450;
		}
	}

	set_default_size (w, h); 
}

PluginUIWindow::~PluginUIWindow ()
{
	delete _pluginui;
}

void
PluginUIWindow::set_parent (Gtk::Window* win)
{
	parent = win;
}

void
PluginUIWindow::on_map ()
{
	Window::on_map ();
	set_keep_above (true);
}

bool
PluginUIWindow::on_enter_notify_event (GdkEventCrossing *ev)
{
	Keyboard::the_keyboard().enter_window (ev, this);
	return false;
}

bool
PluginUIWindow::on_leave_notify_event (GdkEventCrossing *ev)
{
	Keyboard::the_keyboard().leave_window (ev, this);
	return false;
}

bool
PluginUIWindow::on_focus_in_event (GdkEventFocus *ev)
{
	Window::on_focus_in_event (ev);
	//Keyboard::the_keyboard().magic_widget_grab_focus ();
	return false;
}

bool
PluginUIWindow::on_focus_out_event (GdkEventFocus *ev)
{
	Window::on_focus_out_event (ev);
	//Keyboard::the_keyboard().magic_widget_drop_focus ();
	return false;
}

void
PluginUIWindow::on_show ()
{
	set_role("plugin_ui");
	
	if (_pluginui) {
		_pluginui->update_presets ();
	}

	if (_pluginui) {
#if defined (HAVE_AUDIOUNITS) && defined(GTKOSX)
		if (pre_deactivate_x >= 0) {                                                                             
			move (pre_deactivate_x, pre_deactivate_y);                                                       
		}                                                      
#endif
		if (_pluginui->on_window_show (_title)) {
			Window::on_show ();
		}
	} 

	if (parent) {
		// set_transient_for (*parent);
	}
}

void
PluginUIWindow::on_hide ()
{
#if defined (HAVE_AUDIOUNITS) && defined(GTKOSX)
	get_position (pre_deactivate_x, pre_deactivate_y);                                                               
#endif

	Window::on_hide ();

	if (_pluginui) {
		_pluginui->on_window_hide ();
	}
}

void
PluginUIWindow::set_title(const std::string& title)
{
	//cout << "PluginUIWindow::set_title(\"" << title << "\"" << endl;
	Gtk::Window::set_title(title);
	_title = title;
}

bool
PluginUIWindow::create_vst_editor(boost::shared_ptr<PluginInsert> insert)
{
#ifndef VST_SUPPORT
	return false;
#else

	boost::shared_ptr<VSTPlugin> vp;

	if ((vp = boost::dynamic_pointer_cast<VSTPlugin> (insert->plugin())) == 0) {
		error << _("unknown type of editor-supplying plugin (note: no VST support in this version of ardour)")
			      << endmsg;
		throw failed_constructor ();
	} else {
		VSTPluginUI* vpu = new VSTPluginUI (insert, vp);
	
		_pluginui = vpu;
		_pluginui->KeyboardFocused.connect (sigc::mem_fun (*this, &PluginUIWindow::keyboard_focused));
		add (*vpu);
		vpu->package (*this);
	}
	return true;
#endif
}

bool
PluginUIWindow::create_audiounit_editor (boost::shared_ptr<PluginInsert> insert)
{
#if !defined(HAVE_AUDIOUNITS) || !defined(GTKOSX)
	return false;
#else
	VBox* box;
	_pluginui = create_au_gui (insert, &box);
	_pluginui->KeyboardFocused.connect (sigc::mem_fun (*this, &PluginUIWindow::keyboard_focused));
	add (*box);

	Application::instance()->ActivationChanged.connect (mem_fun (*this, &PluginUIWindow::app_activated));

	return true;
#endif
}

void
PluginUIWindow::app_activated (bool yn)
{
#if defined (HAVE_AUDIOUNITS) && defined(GTKOSX)
	if (_pluginui) {
		if (yn) {
			if (was_visible) {
				_pluginui->activate ();
                                if (pre_deactivate_x >= 0) {                                                                             
                                        move (pre_deactivate_x, pre_deactivate_y);                                                       
                                }                                                      
				// present ();
				show ();
				was_visible = true;
			}
		} else {
			was_visible = is_visible();
                        get_position (pre_deactivate_x, pre_deactivate_y);                                                               
			hide ();
			_pluginui->deactivate ();
		}
	} 
#endif
}

bool
PluginUIWindow::create_lv2_editor(boost::shared_ptr<PluginInsert> insert)
{
#ifndef HAVE_SUIL
	return false;
#else

	boost::shared_ptr<LV2Plugin> vp;
	
	if ((vp = boost::dynamic_pointer_cast<LV2Plugin> (insert->plugin())) == 0) {
		error << _("create_lv2_editor called on non-LV2 plugin") << endmsg;
		throw failed_constructor ();
	} else {
		LV2PluginUI* lpu = new LV2PluginUI (insert, vp);
		_pluginui = lpu;
		add (*lpu);
		lpu->package (*this);
	}

	return true;
#endif
}

void
PluginUIWindow::keyboard_focused (bool yn)
{
	_keyboard_focused = yn;
}

bool
PluginUIWindow::on_key_press_event (GdkEventKey* event)
{
	if (_keyboard_focused) {
		if (_pluginui) {
                        if (_pluginui->non_gtk_gui()) {
                                _pluginui->forward_key_event (event); 
                        } else {
                                return relay_key_press (event, this);
                        }
		}
		return true;
	} else {
                if (_pluginui->non_gtk_gui()) {
                        /* pass editor window as the window for the event
                           to be handled in, not this one, because there are
                           no widgets in this window that we want to have
                           key focus.
                        */
                        return relay_key_press (event, &PublicEditor::instance());
                } else {
                        return relay_key_press (event, this);
                }
	}
}

bool
PluginUIWindow::on_key_release_event (GdkEventKey* event)
{
	if (_keyboard_focused) {
		if (_pluginui) {
                        if (_pluginui->non_gtk_gui()) {
                                _pluginui->forward_key_event (event);
                        } 
                        return true;
		}
		return false;
	} else {
		return true;
	}
}

void
PluginUIWindow::plugin_going_away ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &PluginUIWindow::plugin_going_away));

	if (_pluginui) {
		_pluginui->stop_updating(0);
	}
	
	death_connection.disconnect ();
	delete_when_idle (this);
}

PlugUIBase::PlugUIBase (boost::shared_ptr<PluginInsert> pi)
	: insert (pi),
	  plugin (insert->plugin()),
	  save_button(_("Save")),
	  bypass_button (_("Bypass"))
{
        //preset_combo.set_use_arrows_always(true);
	preset_combo.set_size_request (100, -1);
	update_presets ();

	preset_combo.signal_changed().connect(mem_fun(*this, &PlugUIBase::setting_selected));
	no_load_preset = false;

	save_button.set_name ("PluginSaveButton");
	save_button.signal_clicked().connect(mem_fun(*this, &PlugUIBase::save_plugin_setting));

	insert->active_changed.connect (mem_fun(*this, &PlugUIBase::redirect_active_changed));
	bypass_button.set_active (!pi->active());

	bypass_button.set_name ("PluginBypassButton");
	bypass_button.signal_toggled().connect (mem_fun(*this, &PlugUIBase::bypass_toggled));
	focus_button.add_events (Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK);

	focus_button.signal_button_release_event().connect (mem_fun(*this, &PlugUIBase::focus_toggled));
	focus_button.add_events (Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK);

	/* these images are not managed, so that we can remove them at will */

	focus_out_image = new Image (get_icon (X_("computer_keyboard")));
	focus_in_image = new Image (get_icon (X_("computer_keyboard_active")));
	
	focus_button.add (*focus_out_image);

	ARDOUR_UI::instance()->set_tip (&focus_button, string_compose (_("Click to allow the plugin to receive keyboard events that %1 would normally use as a shortcut"), PROGRAM_NAME).c_str(), "");
	ARDOUR_UI::instance()->set_tip (&bypass_button, _("Click to enable/disable this plugin"), "");

	insert->GoingAway.connect (mem_fun (*this, &PlugUIBase::plugin_going_away));
}

PlugUIBase::~PlugUIBase ()
{
}

void
PlugUIBase::plugin_going_away ()
{
	/* drop references to the plugin/insert */
	insert.reset ();
	plugin.reset ();
}

void
PlugUIBase::redirect_active_changed (Redirect* r, void* src)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &PlugUIBase::redirect_active_changed), r, src));
	bypass_button.set_active (!r->active());
}

void
PlugUIBase::setting_selected()
{
	if (no_load_preset) {
		return;
	}
	
	if (preset_combo.get_active_text().length() > 0) {
		if (!plugin->load_preset(preset_combo.get_active_text())) {
			warning << string_compose(_("Plugin preset %1 not found"), preset_combo.get_active_text()) << endmsg;
		}
	}
}

void
PlugUIBase::save_plugin_setting ()
{
	ArdourPrompter prompter (true);
	prompter.set_prompt(_("Name of New Preset:"));
	prompter.add_button (Gtk::Stock::ADD, Gtk::RESPONSE_ACCEPT);
	prompter.set_response_sensitive (Gtk::RESPONSE_ACCEPT, false);
	prompter.set_type_hint (Gdk::WINDOW_TYPE_HINT_UTILITY);

	prompter.show_all();
	prompter.present ();

	switch (prompter.run ()) {
	case Gtk::RESPONSE_ACCEPT:

		string name;

		prompter.get_result(name);

		if (name.length()) {
			if (plugin->save_preset(name)) {

				/* a rather inefficient way to add the newly saved preset
				   to the list.
				*/

				no_load_preset = true;
				set_popdown_strings (preset_combo, plugin->get_presets());
				preset_combo.set_active_text (name);
				no_load_preset = false;
			}
		}
		break;
	}
}

void
PlugUIBase::bypass_toggled ()
{
	bool x;

	if ((x = bypass_button.get_active()) == insert->active()) {
		insert->set_active (!x, this);
	}
}

bool
PlugUIBase::focus_toggled (GdkEventButton* ev)
{
	if (Keyboard::the_keyboard().some_magic_widget_has_focus()) {
		Keyboard::the_keyboard().magic_widget_drop_focus();
		focus_button.remove ();
		focus_button.add (*focus_out_image);
		focus_out_image->show ();
		ARDOUR_UI::instance()->set_tip (&focus_button, string_compose (_("Click to allow the plugin to receive keyboard events that %1 would normally use as a shortcut"), PROGRAM_NAME).c_str(), "");
		KeyboardFocused (false);
	} else {
		Keyboard::the_keyboard().magic_widget_grab_focus();
		focus_button.remove ();
		focus_button.add (*focus_in_image);
		focus_in_image->show ();
		ARDOUR_UI::instance()->set_tip (&focus_button, string_compose (_("Click to allow normal use of %1 keyboard shortcuts"), PROGRAM_NAME).c_str(), "");
		KeyboardFocused (true);
	}

	return true;
}

void
PlugUIBase::update_presets ()
{
	vector<string> presets = plugin->get_presets();
	no_load_preset = true;

	set_popdown_strings (preset_combo, plugin->get_presets());

	string current_preset = plugin->current_preset();

	if (!current_preset.empty()) {
		for (vector<string>::iterator p = presets.begin(); p != presets.end(); ++p) {
			if (*p == current_preset) {
				preset_combo.set_active_text (current_preset);
			}
		}
	}

	no_load_preset = false;
}
