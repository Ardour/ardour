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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <climits>
#include <cerrno>
#include <cmath>
#include <string>

#include "pbd/stl_delete.h"
#include "pbd/xml++.h"
#include "pbd/failed_constructor.h"

#include <gtkmm/widget.h>
#include <gtkmm/box.h>
#include <gtkmm2ext/click_box.h>
#include <gtkmm2ext/fastmeter.h>
#include <gtkmm2ext/barcontroller.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/doi.h>
#include <gtkmm2ext/slider_controller.h>
#include <gtkmm2ext/application.h>

#include "midi++/manager.h"

#include "ardour/session.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/ladspa_plugin.h"
#ifdef WINDOWS_VST_SUPPORT
#include "ardour/windows_vst_plugin.h"
#include "windows_vst_plugin_ui.h"
#endif
#ifdef LXVST_SUPPORT
#include "ardour/lxvst_plugin.h"
#include "lxvst_plugin_ui.h"
#endif
#ifdef LV2_SUPPORT
#include "ardour/lv2_plugin.h"
#include "lv2_plugin_ui.h"
#endif

#include <lrdf.h>

#include "ardour_window.h"
#include "ardour_ui.h"
#include "prompter.h"
#include "plugin_ui.h"
#include "utils.h"
#include "gui_thread.h"
#include "public_editor.h"
#include "keyboard.h"
#include "latency_gui.h"
#include "plugin_eq_gui.h"
#include "new_plugin_preset_dialog.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;

PluginUIWindow::PluginUIWindow (
	Gtk::Window*                    win,
	boost::shared_ptr<PluginInsert> insert,
	bool                            scrollable,
	bool                            editor)
	: parent (win)
	, was_visible (false)
	, _keyboard_focused (false)
#ifdef AUDIOUNIT_SUPPORT
        , pre_deactivate_x (-1)
        , pre_deactivate_y (-1)
#endif

{
	bool have_gui = false;

	Label* label = manage (new Label());
	label->set_markup ("<b>THIS IS THE PLUGIN UI</b>");

	if (editor && insert->plugin()->has_editor()) {
		switch (insert->type()) {
		case ARDOUR::Windows_VST:
			have_gui = create_windows_vst_editor (insert);
			break;
			
		case ARDOUR::LXVST:
			have_gui = create_lxvst_editor (insert);
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
#ifndef WINDOWS_VST_SUPPORT
			error << string_compose (_("unknown type of editor-supplying plugin (note: no VST support in this version of %1)"), PROGRAM_NAME)
			      << endmsg;
#else
			error << _("unknown type of editor-supplying plugin")
			      << endmsg;
#endif
			throw failed_constructor ();
		}

	}

	if (!have_gui) {
		GenericPluginUI* pu = new GenericPluginUI (insert, scrollable);

		_pluginui = pu;
		_pluginui->KeyboardFocused.connect (sigc::mem_fun (*this, &PluginUIWindow::keyboard_focused));
		add (*pu);
		set_wmclass (X_("ardour_plugin_editor"), PROGRAM_NAME);

		signal_map_event().connect (sigc::mem_fun (*pu, &GenericPluginUI::start_updating));
		signal_unmap_event().connect (sigc::mem_fun (*pu, &GenericPluginUI::stop_updating));
	}

	// set_position (Gtk::WIN_POS_MOUSE);
	set_name ("PluginEditor");
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK|Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);

	signal_delete_event().connect (sigc::bind (sigc::ptr_fun (just_hide_it), reinterpret_cast<Window*> (this)), false);
	insert->DropReferences.connect (death_connection, invalidator (*this), boost::bind (&PluginUIWindow::plugin_going_away, this), gui_context());

	gint h = _pluginui->get_preferred_height ();
	gint w = _pluginui->get_preferred_width ();

	if (scrollable) {
		if (h > 600) h = 600;
	}

	set_default_size (w, h);
	set_resizable (_pluginui->resizable());
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
#ifdef __APPLE__
	set_keep_above (true);
#endif // __APPLE__
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
		_pluginui->update_preset_list ();
		_pluginui->update_preset ();
	}

	if (_pluginui) {
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
	Window::on_hide ();

	if (_pluginui) {
		_pluginui->on_window_hide ();
	}
}

void
PluginUIWindow::set_title(const std::string& title)
{
	Gtk::Window::set_title(title);
	_title = title;
}

bool
#ifdef WINDOWS_VST_SUPPORT
PluginUIWindow::create_windows_vst_editor(boost::shared_ptr<PluginInsert> insert)
#else
PluginUIWindow::create_windows_vst_editor(boost::shared_ptr<PluginInsert>)
#endif
{
#ifndef WINDOWS_VST_SUPPORT
	return false;
#else

	boost::shared_ptr<WindowsVSTPlugin> vp;

	if ((vp = boost::dynamic_pointer_cast<WindowsVSTPlugin> (insert->plugin())) == 0) {
		error << string_compose (_("unknown type of editor-supplying plugin (note: no VST support in this version of %1)"), PROGRAM_NAME)
		      << endmsg;
		throw failed_constructor ();
	} else {
		WindowsVSTPluginUI* vpu = new WindowsVSTPluginUI (insert, vp);

		_pluginui = vpu;
		_pluginui->KeyboardFocused.connect (sigc::mem_fun (*this, &PluginUIWindow::keyboard_focused));
		add (*vpu);
		vpu->package (*this);
	}

	return true;
#endif
}

bool
#ifdef LXVST_SUPPORT
PluginUIWindow::create_lxvst_editor(boost::shared_ptr<PluginInsert> insert)
#else
PluginUIWindow::create_lxvst_editor(boost::shared_ptr<PluginInsert>)
#endif
{
#ifndef LXVST_SUPPORT
	return false;
#else

	boost::shared_ptr<LXVSTPlugin> lxvp;

	if ((lxvp = boost::dynamic_pointer_cast<LXVSTPlugin> (insert->plugin())) == 0) {
		error << string_compose (_("unknown type of editor-supplying plugin (note: no linuxVST support in this version of %1)"), PROGRAM_NAME)
		      << endmsg;
		throw failed_constructor ();
	} else {
		LXVSTPluginUI* lxvpu = new LXVSTPluginUI (insert, lxvp);

		_pluginui = lxvpu;
		_pluginui->KeyboardFocused.connect (sigc::mem_fun (*this, &PluginUIWindow::keyboard_focused));
		add (*lxvpu);
		lxvpu->package (*this);
	}

	return true;
#endif
}

bool
#ifdef AUDIOUNIT_SUPPORT
PluginUIWindow::create_audiounit_editor (boost::shared_ptr<PluginInsert> insert)
#else
PluginUIWindow::create_audiounit_editor (boost::shared_ptr<PluginInsert>)
#endif
{
#ifndef AUDIOUNIT_SUPPORT
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
#ifdef GTKOSX
PluginUIWindow::app_activated (bool yn)
#else
PluginUIWindow::app_activated (bool)
#endif
{
#ifdef AUDIOUNIT_SUPPORT
	if (_pluginui) {
		if (yn) {
			if (was_visible) {
				_pluginui->activate ();
                                if (pre_deactivate_x >= 0) {
                                        move (pre_deactivate_x, pre_deactivate_y);
                                }
				present ();
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
#ifdef HAVE_SUIL
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
#else
	return false;
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
		/* for us to be getting key press events, there really
		   MUST be a _pluginui, but just to be safe, check ...
		*/

		if (_pluginui) {
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
		} else {
			return false;
		}
	}
}

bool
PluginUIWindow::on_key_release_event (GdkEventKey *event)
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
	ENSURE_GUI_THREAD (*this, &PluginUIWindow::plugin_going_away)

	if (_pluginui) {
		_pluginui->stop_updating(0);
	}

	death_connection.disconnect ();

	delete_when_idle (this);
}

PlugUIBase::PlugUIBase (boost::shared_ptr<PluginInsert> pi)
	: insert (pi)
	, plugin (insert->plugin())
	, add_button (_("Add"))
	, save_button (_("Save"))
	, delete_button (_("Delete"))
	, bypass_button (ArdourButton::led_default_elements)
	, description_expander (_("Description"))
	, plugin_analysis_expander (_("Plugin analysis"))
	, latency_gui (0)
	, latency_dialog (0)
	, eqgui (0)
{
	_preset_modified.set_size_request (16, -1);
	_preset_combo.signal_changed().connect(sigc::mem_fun(*this, &PlugUIBase::preset_selected));
	ARDOUR_UI::instance()->set_tip (_preset_combo, _("Presets (if any) for this plugin\n(Both factory and user-created)"));
	ARDOUR_UI::instance()->set_tip (add_button, _("Save a new preset"));
	ARDOUR_UI::instance()->set_tip (save_button, _("Save the current preset"));
	ARDOUR_UI::instance()->set_tip (delete_button, _("Delete the current preset"));
	ARDOUR_UI::instance()->set_tip (bypass_button, _("Disable signal processing by the plugin"));
	_no_load_preset = 0;

	update_preset_list ();
	update_preset ();

	add_button.set_name ("PluginAddButton");
	add_button.signal_clicked().connect (sigc::mem_fun (*this, &PlugUIBase::add_plugin_setting));

	save_button.set_name ("PluginSaveButton");
	save_button.signal_clicked().connect(sigc::mem_fun(*this, &PlugUIBase::save_plugin_setting));

	delete_button.set_name ("PluginDeleteButton");
	delete_button.signal_clicked().connect (sigc::mem_fun (*this, &PlugUIBase::delete_plugin_setting));

	insert->ActiveChanged.connect (active_connection, invalidator (*this), boost::bind (&PlugUIBase::processor_active_changed, this,  boost::weak_ptr<Processor>(insert)), gui_context());

	bypass_button.set_name ("plugin bypass button");
	bypass_button.set_text (_("Bypass"));
	bypass_button.set_active (!pi->active());
	bypass_button.signal_button_release_event().connect (sigc::mem_fun(*this, &PlugUIBase::bypass_button_release));
	focus_button.add_events (Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK);

	focus_button.signal_button_release_event().connect (sigc::mem_fun(*this, &PlugUIBase::focus_toggled));
	focus_button.add_events (Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK);

	/* these images are not managed, so that we can remove them at will */

	focus_out_image = new Image (get_icon (X_("computer_keyboard")));
	focus_in_image = new Image (get_icon (X_("computer_keyboard_active")));

	focus_button.add (*focus_out_image);

	ARDOUR_UI::instance()->set_tip (focus_button, string_compose (_("Click to allow the plugin to receive keyboard events that %1 would normally use as a shortcut"), PROGRAM_NAME));
	ARDOUR_UI::instance()->set_tip (bypass_button, _("Click to enable/disable this plugin"));

	description_expander.property_expanded().signal_changed().connect( sigc::mem_fun(*this, &PlugUIBase::toggle_description));
	description_expander.set_expanded(false);

	plugin_analysis_expander.property_expanded().signal_changed().connect( sigc::mem_fun(*this, &PlugUIBase::toggle_plugin_analysis));
	plugin_analysis_expander.set_expanded(false);

	insert->DropReferences.connect (death_connection, invalidator (*this), boost::bind (&PlugUIBase::plugin_going_away, this), gui_context());

	plugin->PresetAdded.connect (*this, invalidator (*this), boost::bind (&PlugUIBase::preset_added_or_removed, this), gui_context ());
	plugin->PresetRemoved.connect (*this, invalidator (*this), boost::bind (&PlugUIBase::preset_added_or_removed, this), gui_context ());
	plugin->PresetLoaded.connect (*this, invalidator (*this), boost::bind (&PlugUIBase::update_preset, this), gui_context ());
	plugin->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&PlugUIBase::parameter_changed, this, _1, _2), gui_context ());
}

PlugUIBase::~PlugUIBase()
{
	delete eqgui;
	delete latency_gui;
}

void
PlugUIBase::plugin_going_away ()
{
	/* drop references to the plugin/insert */
	insert.reset ();
	plugin.reset ();
}

void
PlugUIBase::set_latency_label ()
{
	framecnt_t const l = insert->effective_latency ();
	framecnt_t const sr = insert->session().frame_rate ();

	string t;

	if (l < sr / 1000) {
		t = string_compose (P_("latency (%1 sample)", "latency (%1 samples)", l), l);
	} else {
		t = string_compose (_("latency (%1 ms)"), (float) l / ((float) sr / 1000.0f));
	}

	latency_label.set_text (t);
}

void
PlugUIBase::latency_button_clicked ()
{
	if (!latency_gui) {
		latency_gui = new LatencyGUI (*(insert.get()), insert->session().frame_rate(), insert->session().get_block_size());
		latency_dialog = new ArdourWindow (_("Edit Latency"));
		latency_dialog->set_position (WIN_POS_MOUSE);
		/* use both keep-above and transient for to try cover as many
		   different WM's as possible.
		*/
		latency_dialog->set_keep_above (true);
		Window* win = dynamic_cast<Window*> (bypass_button.get_toplevel ());
		if (win) {
			latency_dialog->set_transient_for (*win);
		}
		latency_dialog->add (*latency_gui);
		latency_dialog->signal_hide().connect (sigc::mem_fun (*this, &PlugUIBase::set_latency_label));
	}

	latency_dialog->show_all ();
}

void
PlugUIBase::processor_active_changed (boost::weak_ptr<Processor> weak_p)
{
	ENSURE_GUI_THREAD (*this, &PlugUIBase::processor_active_changed, weak_p);
	boost::shared_ptr<Processor> p (weak_p.lock());

	if (p) {
		bypass_button.set_active (!p->active());
	}
}

void
PlugUIBase::preset_selected ()
{
	if (_no_load_preset) {
		return;
	}

	if (_preset_combo.get_active_text().length() > 0) {
		const Plugin::PresetRecord* pr = plugin->preset_by_label (_preset_combo.get_active_text());
		if (pr) {
			plugin->load_preset (*pr);
		} else {
			warning << string_compose(_("Plugin preset %1 not found"),
						  _preset_combo.get_active_text()) << endmsg;
		}
	} else {
		// blank selected = no preset
		plugin->clear_preset();
	}
}

void
PlugUIBase::add_plugin_setting ()
{
	NewPluginPresetDialog d (plugin);

	switch (d.run ()) {
	case Gtk::RESPONSE_ACCEPT:
		if (d.name().empty()) {
			break;
		}

		if (d.replace ()) {
			plugin->remove_preset (d.name ());
		}

		Plugin::PresetRecord const r = plugin->save_preset (d.name());
		if (!r.uri.empty ()) {
			plugin->load_preset (r);
		}
		break;
	}
}

void
PlugUIBase::save_plugin_setting ()
{
	string const name = _preset_combo.get_active_text ();
	plugin->remove_preset (name);
	Plugin::PresetRecord const r = plugin->save_preset (name);
	if (!r.uri.empty ()) {
		plugin->load_preset (r);
	}
}

void
PlugUIBase::delete_plugin_setting ()
{
	plugin->remove_preset (_preset_combo.get_active_text ());
}

bool
PlugUIBase::bypass_button_release (GdkEventButton*)
{
	bool view_says_bypassed = (bypass_button.active_state() != 0);
	
	if (view_says_bypassed != insert->active()) {
		if (view_says_bypassed) {
			insert->activate ();
		} else {
			insert->deactivate ();
		}
	}

	return false;
}

bool
PlugUIBase::focus_toggled (GdkEventButton*)
{
	if (Keyboard::the_keyboard().some_magic_widget_has_focus()) {
		Keyboard::the_keyboard().magic_widget_drop_focus();
		focus_button.remove ();
		focus_button.add (*focus_out_image);
		focus_out_image->show ();
		ARDOUR_UI::instance()->set_tip (focus_button, string_compose (_("Click to allow the plugin to receive keyboard events that %1 would normally use as a shortcut"), PROGRAM_NAME));
		KeyboardFocused (false);
	} else {
		Keyboard::the_keyboard().magic_widget_grab_focus();
		focus_button.remove ();
		focus_button.add (*focus_in_image);
		focus_in_image->show ();
		ARDOUR_UI::instance()->set_tip (focus_button, string_compose (_("Click to allow normal use of %1 keyboard shortcuts"), PROGRAM_NAME));
		KeyboardFocused (true);
	}

	return true;
}

void
PlugUIBase::toggle_description()
{
	if (description_expander.get_expanded() &&
	    !description_expander.get_child()) {
		const std::string text = plugin->get_docs();
		if (text.empty()) {
			return;
		}

		Gtk::Label* label = manage(new Gtk::Label(text));
		label->set_line_wrap(true);
		label->set_line_wrap_mode(Pango::WRAP_WORD);
		description_expander.add(*label);
		description_expander.show_all();
	}
	
	if (!description_expander.get_expanded()) {
		description_expander.remove();
	}
}


void
PlugUIBase::toggle_plugin_analysis()
{
	if (plugin_analysis_expander.get_expanded() &&
	    !plugin_analysis_expander.get_child()) {
		// Create the GUI
		if (eqgui == 0) {
			eqgui = new PluginEqGui (insert);
		}

		Gtk::Window *toplevel = (Gtk::Window*) plugin_analysis_expander.get_ancestor (GTK_TYPE_WINDOW);

		if (toplevel) {
			toplevel->get_size (pre_eq_size.width, pre_eq_size.height);
		}

		plugin_analysis_expander.add (*eqgui);
		plugin_analysis_expander.show_all ();
		eqgui->start_listening ();
	}

	if (!plugin_analysis_expander.get_expanded()) {
		// Hide & remove from expander

		eqgui->hide ();
		eqgui->stop_listening ();
		plugin_analysis_expander.remove();

		Gtk::Window *toplevel = (Gtk::Window*) plugin_analysis_expander.get_ancestor (GTK_TYPE_WINDOW);

		if (toplevel) {
			toplevel->resize (pre_eq_size.width, pre_eq_size.height);
		}
	}
}

void
PlugUIBase::update_preset_list ()
{
	vector<string> preset_labels;
	vector<ARDOUR::Plugin::PresetRecord> presets = plugin->get_presets();

	++_no_load_preset;

	for (vector<ARDOUR::Plugin::PresetRecord>::const_iterator i = presets.begin(); i != presets.end(); ++i) {
		preset_labels.push_back (i->label);
	}

	preset_labels.push_back("");

	set_popdown_strings (_preset_combo, preset_labels);

	--_no_load_preset;
}

void
PlugUIBase::update_preset ()
{
	Plugin::PresetRecord p = plugin->last_preset();

	++_no_load_preset;
	_preset_combo.set_active_text (p.label);
	--_no_load_preset;

	save_button.set_sensitive (!p.uri.empty() && p.user);
	delete_button.set_sensitive (!p.uri.empty() && p.user);

	update_preset_modified ();
}

void
PlugUIBase::update_preset_modified ()
{

	if (plugin->last_preset().uri.empty()) {
		_preset_modified.set_text ("");
		return;
	}

	bool const c = plugin->parameter_changed_since_last_preset ();
	if (_preset_modified.get_text().empty() == c) {
		_preset_modified.set_text (c ? "*" : "");
	}
}

void
PlugUIBase::parameter_changed (uint32_t, float)
{
	update_preset_modified ();
}

void
PlugUIBase::preset_added_or_removed ()
{
	/* Update both the list and the currently-displayed preset */
	update_preset_list ();
	update_preset ();
}

