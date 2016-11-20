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
#ifdef MACVST_SUPPORT
#include "ardour/mac_vst_plugin.h"
#include "vst_plugin_ui.h"
#endif
#ifdef LV2_SUPPORT
#include "ardour/lv2_plugin.h"
#include "lv2_plugin_ui.h"
#endif

#include "ardour_window.h"
#include "ardour_ui.h"
#include "prompter.h"
#include "plugin_ui.h"
#include "utils.h"
#include "gui_thread.h"
#include "public_editor.h"
#include "processor_box.h"
#include "keyboard.h"
#include "latency_gui.h"
#include "plugin_eq_gui.h"
#include "new_plugin_preset_dialog.h"
#include "tooltips.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;

PluginUIWindow::PluginUIWindow (
	boost::shared_ptr<PluginInsert> insert,
	bool                            scrollable,
	bool                            editor)
	: ArdourWindow (string())
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

		case ARDOUR::MacVST:
			have_gui = create_mac_vst_editor (insert);
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

	set_name ("PluginEditor");
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK|Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);

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
#ifndef NDEBUG
	cerr << "PluginWindow deleted for " << this << endl;
#endif
	delete _pluginui;
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
#if defined (HAVE_AUDIOUNITS) && defined(__APPLE__)
                if (pre_deactivate_x >= 0) {
                        move (pre_deactivate_x, pre_deactivate_y);
                }
#endif

		if (_pluginui->on_window_show (_title)) {
			Window::on_show ();
		}
	}
}

void
PluginUIWindow::on_hide ()
{
#if defined (HAVE_AUDIOUNITS) && defined(__APPLE__)
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
		WindowsVSTPluginUI* vpu = new WindowsVSTPluginUI (insert, vp, GTK_WIDGET(this->gobj()));

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
#ifdef MACVST_SUPPORT
PluginUIWindow::create_mac_vst_editor (boost::shared_ptr<PluginInsert> insert)
#else
PluginUIWindow::create_mac_vst_editor (boost::shared_ptr<PluginInsert>)
#endif
{
#ifndef MACVST_SUPPORT
	return false;
#else
	boost::shared_ptr<MacVSTPlugin> mvst;
	if ((mvst = boost::dynamic_pointer_cast<MacVSTPlugin> (insert->plugin())) == 0) {
		error << string_compose (_("unknown type of editor-supplying plugin (note: no MacVST support in this version of %1)"), PROGRAM_NAME)
		      << endmsg;
		throw failed_constructor ();
	}
	VSTPluginUI* vpu = create_mac_vst_gui (insert);
	_pluginui = vpu;
	_pluginui->KeyboardFocused.connect (sigc::mem_fun (*this, &PluginUIWindow::keyboard_focused));
	add (*vpu);
	vpu->package (*this);

	Application::instance()->ActivationChanged.connect (mem_fun (*this, &PluginUIWindow::app_activated));

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
#ifdef __APPLE__
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
		_pluginui->KeyboardFocused.connect (sigc::mem_fun (*this, &PluginUIWindow::keyboard_focused));
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
			_pluginui->grab_focus();
			if (_pluginui->non_gtk_gui()) {
				_pluginui->forward_key_event (event);
			} else {
					return relay_key_press (event, this);
			}
		}
		return true;
	}
	/* for us to be getting key press events, there really
	   MUST be a _pluginui, but just to be safe, check ...
	*/

	if (_pluginui) {
		_pluginui->grab_focus();
		if (_pluginui->non_gtk_gui()) {
			/* pass main window as the window for the event
			   to be handled in, not this one, because there are
			   no widgets in this window that we want to have
			   key focus.
			*/
			return relay_key_press (event, &ARDOUR_UI::instance()->main_window());
		} else {
			return relay_key_press (event, this);
		}
	}

	return false;
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
}

PlugUIBase::PlugUIBase (boost::shared_ptr<PluginInsert> pi)
	: insert (pi)
	, plugin (insert->plugin())
	, add_button (_("Add"))
	, save_button (_("Save"))
	, delete_button (_("Delete"))
	, reset_button (_("Reset"))
	, bypass_button (ArdourButton::led_default_elements)
	, pin_management_button (_("Pinout"))
	, description_expander (_("Description"))
	, plugin_analysis_expander (_("Plugin analysis"))
	, latency_gui (0)
	, latency_dialog (0)
	, eqgui (0)
{
	_preset_modified.set_size_request (16, -1);
	_preset_combo.set_text("(default)");
	set_tooltip (_preset_combo, _("Presets (if any) for this plugin\n(Both factory and user-created)"));
	set_tooltip (add_button, _("Save a new preset"));
	set_tooltip (save_button, _("Save the current preset"));
	set_tooltip (delete_button, _("Delete the current preset"));
	set_tooltip (reset_button, _("Reset parameters to default (if no parameters are in automation play mode)"));
	set_tooltip (pin_management_button, _("Show Plugin Pin Management Dialog"));
	set_tooltip (bypass_button, _("Disable signal processing by the plugin"));
	_no_load_preset = 0;

	update_preset_list ();
	update_preset ();

	add_button.set_name ("generic button");
	add_button.signal_clicked.connect (sigc::mem_fun (*this, &PlugUIBase::add_plugin_setting));

	save_button.set_name ("generic button");
	save_button.signal_clicked.connect(sigc::mem_fun(*this, &PlugUIBase::save_plugin_setting));

	delete_button.set_name ("generic button");
	delete_button.signal_clicked.connect (sigc::mem_fun (*this, &PlugUIBase::delete_plugin_setting));

	reset_button.set_name ("generic button");
	reset_button.signal_clicked.connect (sigc::mem_fun (*this, &PlugUIBase::reset_plugin_parameters));

	pin_management_button.set_name ("generic button");
	pin_management_button.signal_clicked.connect (sigc::mem_fun (*this, &PlugUIBase::manage_pins));

	insert->ActiveChanged.connect (active_connection, invalidator (*this), boost::bind (&PlugUIBase::processor_active_changed, this,  boost::weak_ptr<Processor>(insert)), gui_context());

	bypass_button.set_name ("plugin bypass button");
	bypass_button.set_text (_("Bypass"));
	bypass_button.set_active (!pi->enabled ());
	bypass_button.signal_button_release_event().connect (sigc::mem_fun(*this, &PlugUIBase::bypass_button_release), false);
	focus_button.add_events (Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK);

	focus_button.signal_button_release_event().connect (sigc::mem_fun(*this, &PlugUIBase::focus_toggled));
	focus_button.add_events (Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK);

	/* these images are not managed, so that we can remove them at will */

	focus_out_image = new Image (get_icon (X_("computer_keyboard")));
	focus_in_image = new Image (get_icon (X_("computer_keyboard_active")));

	focus_button.add (*focus_out_image);

	set_tooltip (focus_button, string_compose (_("Click to allow the plugin to receive keyboard events that %1 would normally use as a shortcut"), PROGRAM_NAME));
	set_tooltip (bypass_button, _("Click to enable/disable this plugin"));

	description_expander.property_expanded().signal_changed().connect( sigc::mem_fun(*this, &PlugUIBase::toggle_description));
	description_expander.set_expanded(false);

	plugin_analysis_expander.property_expanded().signal_changed().connect( sigc::mem_fun(*this, &PlugUIBase::toggle_plugin_analysis));
	plugin_analysis_expander.set_expanded(false);

	insert->DropReferences.connect (death_connection, invalidator (*this), boost::bind (&PlugUIBase::plugin_going_away, this), gui_context());

	plugin->PresetAdded.connect (*this, invalidator (*this), boost::bind (&PlugUIBase::preset_added_or_removed, this), gui_context ());
	plugin->PresetRemoved.connect (*this, invalidator (*this), boost::bind (&PlugUIBase::preset_added_or_removed, this), gui_context ());
	plugin->PresetLoaded.connect (*this, invalidator (*this), boost::bind (&PlugUIBase::update_preset, this), gui_context ());
	plugin->PresetDirty.connect (*this, invalidator (*this), boost::bind (&PlugUIBase::update_preset_modified, this), gui_context ());

	insert->AutomationStateChanged.connect (*this, invalidator (*this), boost::bind (&PlugUIBase::automation_state_changed, this), gui_context());

	automation_state_changed();
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

	latency_button.set_text (t);
}

void
PlugUIBase::latency_button_clicked ()
{
	if (!latency_gui) {
		latency_gui = new LatencyGUI (*(insert.get()), insert->session().frame_rate(), insert->session().get_block_size());
		latency_dialog = new ArdourWindow (_("Edit Latency"));
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
		bypass_button.set_active (!p->enabled ());
	}
}

void
PlugUIBase::preset_selected (Plugin::PresetRecord preset)
{
	if (_no_load_preset) {
		return;
	}
	if (!preset.label.empty()) {
		insert->load_preset (preset);
	} else {
		// blank selected = no preset
		plugin->clear_preset();
	}
}

#ifdef NO_PLUGIN_STATE
static bool seen_saving_message = false;

static void show_no_plugin_message()
{
	info << string_compose (_("Plugin presets are not supported in this build of %1. Consider paying for a full version"),
			PROGRAM_NAME)
	     << endmsg;
	info << _("To get full access to updates without this limitation\n"
	          "consider becoming a subscriber for a low cost every month.")
	     << endmsg;
	info << X_("https://community.ardour.org/s/subscribe")
	     << endmsg;
	ARDOUR_UI::instance()->popup_error(_("Plugin presets are not supported in this build, see the Log window for more information."));
}
#endif

void
PlugUIBase::add_plugin_setting ()
{
#ifndef NO_PLUGIN_STATE
	NewPluginPresetDialog d (plugin, _("New Preset"));

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
#else
	if (!seen_saving_message) {
		seen_saving_message = true;
		show_no_plugin_message();
	}
#endif
}

void
PlugUIBase::save_plugin_setting ()
{
#ifndef NO_PLUGIN_STATE
	string const name = _preset_combo.get_text ();
	plugin->remove_preset (name);
	Plugin::PresetRecord const r = plugin->save_preset (name);
	if (!r.uri.empty ()) {
		plugin->load_preset (r);
	}
#else
	if (!seen_saving_message) {
		seen_saving_message = true;
		show_no_plugin_message();
	}
#endif
}

void
PlugUIBase::delete_plugin_setting ()
{
#ifndef NO_PLUGIN_STATE
	plugin->remove_preset (_preset_combo.get_text ());
#else
	if (!seen_saving_message) {
		seen_saving_message = true;
		show_no_plugin_message();
	}
#endif
}

void
PlugUIBase::automation_state_changed ()
{
	reset_button.set_sensitive (insert->can_reset_all_parameters());
}

void
PlugUIBase::reset_plugin_parameters ()
{
	insert->reset_parameters_to_default ();
}

void
PlugUIBase::manage_pins ()
{
	PluginPinWindowProxy* proxy = insert->pinmgr_proxy ();
	if (proxy) {
		proxy->get (true);
		proxy->present ();
		proxy->get ()->raise();
	}
}

bool
PlugUIBase::bypass_button_release (GdkEventButton*)
{
	bool view_says_bypassed = (bypass_button.active_state() != 0);

	if (view_says_bypassed != insert->enabled ()) {
		insert->enable (view_says_bypassed);
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
		set_tooltip (focus_button, string_compose (_("Click to allow the plugin to receive keyboard events that %1 would normally use as a shortcut"), PROGRAM_NAME));
		KeyboardFocused (false);
	} else {
		Keyboard::the_keyboard().magic_widget_grab_focus();
		focus_button.remove ();
		focus_button.add (*focus_in_image);
		focus_in_image->show ();
		set_tooltip (focus_button, string_compose (_("Click to allow normal use of %1 keyboard shortcuts"), PROGRAM_NAME));
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
		const int child_height = description_expander.get_child ()->get_height ();

		description_expander.remove();

		Gtk::Window *toplevel = (Gtk::Window*) description_expander.get_ancestor (GTK_TYPE_WINDOW);

		if (toplevel) {
			Gtk::Requisition wr;
			toplevel->get_size (wr.width, wr.height);
			wr.height -= child_height;
			toplevel->resize (wr.width, wr.height);
		}

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

		plugin_analysis_expander.add (*eqgui);
		plugin_analysis_expander.show_all ();
		eqgui->start_listening ();
	}

	if (!plugin_analysis_expander.get_expanded()) {
		// Hide & remove from expander
		const int child_height = plugin_analysis_expander.get_child ()->get_height ();

		eqgui->hide ();
		eqgui->stop_listening ();
		plugin_analysis_expander.remove();

		Gtk::Window *toplevel = (Gtk::Window*) plugin_analysis_expander.get_ancestor (GTK_TYPE_WINDOW);

		if (toplevel) {
			Gtk::Requisition wr;
			toplevel->get_size (wr.width, wr.height);
			wr.height -= child_height;
			toplevel->resize (wr.width, wr.height);
		}
	}
}

void
PlugUIBase::update_preset_list ()
{
	using namespace Menu_Helpers;

	vector<ARDOUR::Plugin::PresetRecord> presets = plugin->get_presets();

	++_no_load_preset;

	// Add a menu entry for each preset
	_preset_combo.clear_items();
	for (vector<ARDOUR::Plugin::PresetRecord>::const_iterator i = presets.begin(); i != presets.end(); ++i) {
		_preset_combo.AddMenuElem(
			MenuElem(i->label, sigc::bind(sigc::mem_fun(*this, &PlugUIBase::preset_selected), *i)));
	}

	// Add an empty entry for un-setting current preset (see preset_selected)
	Plugin::PresetRecord no_preset;
	_preset_combo.AddMenuElem(
		MenuElem("", sigc::bind(sigc::mem_fun(*this, &PlugUIBase::preset_selected), no_preset)));

	--_no_load_preset;
}

void
PlugUIBase::update_preset ()
{
	Plugin::PresetRecord p = plugin->last_preset();

	++_no_load_preset;
	if (p.uri.empty()) {
		_preset_combo.set_text (_("(none)"));
	} else {
		_preset_combo.set_text (p.label);
	}
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
PlugUIBase::preset_added_or_removed ()
{
	/* Update both the list and the currently-displayed preset */
	update_preset_list ();
	update_preset ();
}

