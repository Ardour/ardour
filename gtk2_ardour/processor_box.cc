/*
 * Copyright (C) 2007-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2008-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2015 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2015 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2014-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2017 Johannes Mueller <github@johannes-mueller.org>
 * Copyright (C) 2018 Len Ovens <len@ovenwerks.net>
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

#include <cmath>
#include <iostream>
#include <set>

#include <sigc++/bind.h>

#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>

#include <gtkmm/messagedialog.h>

#include "pbd/unwind.h"

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/menu_elems.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/doi.h"
#include "gtkmm2ext/rgb_macros.h"

#include "widgets/choice.h"
#include "widgets/prompter.h"
#include "widgets/tooltips.h"

#include "ardour/amp.h"
#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/internal_return.h"
#include "ardour/internal_send.h"
#include "ardour/luaproc.h"
#include "ardour/luascripting.h"
#include "ardour/meter.h"
#include "ardour/panner_shell.h"
#include "ardour/plugin_insert.h"
#include "ardour/pannable.h"
#include "ardour/port_insert.h"
#include "ardour/profile.h"
#include "ardour/return.h"
#include "ardour/route.h"
#include "ardour/send.h"
#include "ardour/session.h"
#include "ardour/types.h"
#include "ardour/value_as_string.h"

#include "LuaBridge/LuaBridge.h"

#include "actions.h"
#include "ardour_dialog.h"
#include "ardour_ui.h"
#include "gui_thread.h"
#include "io_selector.h"
#include "keyboard.h"
#include "luainstance.h"
#include "mixer_ui.h"
#include "mixer_strip.h"
#include "plugin_pin_dialog.h"
#include "plugin_selector.h"
#include "plugin_ui.h"
#include "port_insert_ui.h"
#include "processor_box.h"
#include "processor_selection.h"
#include "public_editor.h"
#include "return_ui.h"
#include "script_selector.h"
#include "send_ui.h"
#include "timers.h"
#include "new_plugin_preset_dialog.h"

#include "pbd/i18n.h"

#ifdef AUDIOUNIT_SUPPORT
class AUPluginUI;
#endif

#ifndef NDEBUG
bool ProcessorBox::show_all_processors = false;
#endif

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace ArdourWidgets;

ProcessorBox*  ProcessorBox::_current_processor_box = 0;
bool           ProcessorBox::_ignore_disk_io_change = false;

RefPtr<Action> ProcessorBox::paste_action;
RefPtr<Action> ProcessorBox::cut_action;
RefPtr<Action> ProcessorBox::copy_action;
RefPtr<Action> ProcessorBox::rename_action;
RefPtr<Action> ProcessorBox::delete_action;
RefPtr<Action> ProcessorBox::backspace_action;
RefPtr<Action> ProcessorBox::manage_pins_action;
RefPtr<Action> ProcessorBox::disk_io_action;
RefPtr<Action> ProcessorBox::edit_action;
RefPtr<Action> ProcessorBox::edit_generic_action;
RefPtr<ActionGroup> ProcessorBox::processor_box_actions;
Gtkmm2ext::Bindings* ProcessorBox::bindings = 0;


// TODO consolidate with PluginPinDialog::set_color
static void set_routing_color (cairo_t* cr, bool midi)
{
	static const uint32_t audio_port_color = 0x4A8A0EFF; // Green
	static const uint32_t midi_port_color = 0x960909FF; //Red

	if (midi) {
		cairo_set_source_rgb (cr,
				UINT_RGBA_R_FLT(midi_port_color),
				UINT_RGBA_G_FLT(midi_port_color),
				UINT_RGBA_B_FLT(midi_port_color));
	} else {
		cairo_set_source_rgb (cr,
				UINT_RGBA_R_FLT(audio_port_color),
				UINT_RGBA_G_FLT(audio_port_color),
				UINT_RGBA_B_FLT(audio_port_color));
	}
}

ProcessorEntry::ProcessorEntry (ProcessorBox* parent, boost::shared_ptr<Processor> p, Width w)
	: _button (ArdourButton::led_default_elements)
	, _position (PreFader)
	, _position_num(0)
	, _parent (parent)
	, _selectable(true)
	, _unknown_processor(false)
	, _processor (p)
	, _width (w)
	, input_icon(true)
	, output_icon(false)
	, routing_icon(true)
	, output_routing_icon(false)
	, _plugin_display(0)
{
	_vbox.show ();

	_button.set_distinct_led_click (true);
	_button.set_fallthrough_to_parent(true);
	_button.set_led_left (true);
	_button.signal_led_clicked.connect (sigc::mem_fun (*this, &ProcessorEntry::led_clicked));
	_button.set_text (name (_width));

	if (boost::dynamic_pointer_cast<PeakMeter> (_processor)) {
		_button.set_elements(ArdourButton::Element(_button.elements() & ~ArdourButton::Indicator));
	}
	if (boost::dynamic_pointer_cast<UnknownProcessor> (_processor)) {
		_button.set_elements(ArdourButton::Element(_button.elements() & ~ArdourButton::Indicator));
		_unknown_processor = true;
	}
	{
		boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (_processor);
		if (pi && pi->plugin()) {
			_plugin_preset_pointer = PluginPresetPtr (new PluginPreset (pi->plugin()->get_info()));
		}
	}
	if (_processor) {

		_vbox.pack_start (routing_icon);
		_vbox.pack_start (input_icon);
		_vbox.pack_start (_button, true, true);

		boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (_processor);
		if (pi && pi->plugin() && pi->plugin()->has_inline_display()) {
			if (pi->plugin()->get_info()->type != ARDOUR::Lua) {
				_plugin_display = new PluginInlineDisplay (*this, pi->plugin(),
						std::max (60.f, rintf(112.f * UIConfiguration::instance().get_ui_scale())));
			} else {
				assert (boost::dynamic_pointer_cast<LuaProc>(pi->plugin()));
				_plugin_display = new LuaPluginDisplay (*this, boost::dynamic_pointer_cast<LuaProc>(pi->plugin()),
						std::max (60.f, rintf(112.f * UIConfiguration::instance().get_ui_scale())));
			}
			_vbox.pack_start (*_plugin_display);
			_plugin_display->set_no_show_all (true);
			if (UIConfiguration::instance().get_show_inline_display_by_default ()) {
				_plugin_display->show ();
			}
		}
		_vbox.pack_end (output_routing_icon);
		_vbox.pack_end (output_icon);

		_button.set_active (_processor->enabled ());

		input_icon.set_no_show_all(true);
		routing_icon.set_no_show_all(true);
		output_icon.set_no_show_all(true);
		output_routing_icon.set_no_show_all(true);

		_button.show ();
		input_icon.hide();
		output_icon.show();
		routing_icon.hide();
		output_routing_icon.hide();

		_processor->ActiveChanged.connect (active_connection, invalidator (*this), boost::bind (&ProcessorEntry::processor_active_changed, this), gui_context());
		_processor->PropertyChanged.connect (name_connection, invalidator (*this), boost::bind (&ProcessorEntry::processor_property_changed, this, _1), gui_context());
		_processor->ConfigurationChanged.connect (config_connection, invalidator (*this), boost::bind (&ProcessorEntry::processor_configuration_changed, this, _1, _2), gui_context());

		const uint32_t limit_inline_controls = UIConfiguration::instance().get_max_inline_controls ();

		set<Evoral::Parameter> p = _processor->what_can_be_automated ();
		for (set<Evoral::Parameter>::iterator i = p.begin(); i != p.end(); ++i) {

			std::string label = _processor->describe_parameter (*i);

			if (label == X_("hidden")) {
				continue;
			}

			if (boost::dynamic_pointer_cast<Send> (_processor)) {
				label = _("Send");
			} else if (boost::dynamic_pointer_cast<Return> (_processor)) {
				label = _("Return");
			}

			Control* c = new Control (_processor->automation_control (*i), label);

			_controls.push_back (c);

			if (boost::dynamic_pointer_cast<Amp> (_processor) == 0) {
				/* Add non-Amp (Fader & Trim) controls to the processor box */
				_vbox.pack_start (c->box);
			}

			if (limit_inline_controls > 0 && _controls.size() >= limit_inline_controls) {
				break;
			}
		}

		setup_tooltip ();
		setup_visuals ();
	} else {
		_vbox.set_size_request (-1, _button.size_request().height);
	}
}

ProcessorEntry::~ProcessorEntry ()
{
	for (list<Control*>::iterator i = _controls.begin(); i != _controls.end(); ++i) {
		delete *i;
	}
	delete _plugin_display;
}

EventBox&
ProcessorEntry::action_widget ()
{
	return _button;
}

Gtk::Widget&
ProcessorEntry::widget ()
{
	return _vbox;
}

string
ProcessorEntry::drag_text () const
{
	return name (Wide);
}

bool
ProcessorEntry::can_copy_state (Gtkmm2ext::DnDVBoxChild* o) const
{
	ProcessorEntry *other = dynamic_cast<ProcessorEntry*> (o);
	if (!other) {
		return false;
	}
	boost::shared_ptr<ARDOUR::Processor> otherproc = other->processor();
	boost::shared_ptr<PluginInsert> my_pi = boost::dynamic_pointer_cast<PluginInsert> (_processor);
	boost::shared_ptr<PluginInsert> ot_pi = boost::dynamic_pointer_cast<PluginInsert> (otherproc);
	if (boost::dynamic_pointer_cast<UnknownProcessor> (_processor)) {
		return false;
	}
	if (boost::dynamic_pointer_cast<UnknownProcessor> (otherproc)) {
		return false;
	}
	if (!my_pi || !ot_pi) {
		return false;
	}
	if (my_pi->type() != ot_pi->type()) {
		return false;
	}
	boost::shared_ptr<Plugin> my_p = my_pi->plugin();
	boost::shared_ptr<Plugin> ot_p = ot_pi->plugin();
	if (!my_p || !ot_p) {
		return false;
	}
	if (my_p->unique_id() != ot_p->unique_id()) {
		return false;
	}
	return true;
}

bool
ProcessorEntry::drag_data_get (Glib::RefPtr<Gdk::DragContext> const, Gtk::SelectionData &data)
{
	if (data.get_target() == "PluginPresetPtr") {
		boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (_processor);

		if (!_plugin_preset_pointer || !pi) {
			data.set (data.get_target(), 8, NULL, 0);
			return true;
		}

		boost::shared_ptr<ARDOUR::Plugin> plugin = pi->plugin();
		assert (plugin);

		PluginManager& manager (PluginManager::instance());
		bool fav = manager.get_status (_plugin_preset_pointer->_pip) == PluginManager::Favorite;

		NewPluginPresetDialog d (plugin,
				string_compose(_("New Favorite Preset for \"%1\""),_plugin_preset_pointer->_pip->name), !fav);

		_plugin_preset_pointer->_preset.valid = false;

		switch (d.run ()) {
			default:
			case Gtk::RESPONSE_CANCEL:
				data.set (data.get_target(), 8, NULL, 0);
				return true;
				break;

			case Gtk::RESPONSE_NO:
				break;

			case Gtk::RESPONSE_ACCEPT:
				if (d.name().empty()) {
					break;
				}

				if (d.replace ()) {
					plugin->remove_preset (d.name ());
				}

				Plugin::PresetRecord const r = plugin->save_preset (d.name());

				if (!r.uri.empty ()) {
					_plugin_preset_pointer->_preset.uri   = r.uri;
					_plugin_preset_pointer->_preset.label = r.label;
					_plugin_preset_pointer->_preset.user  = r.user;
					_plugin_preset_pointer->_preset.valid = r.valid;
				}
		}
		data.set (data.get_target(), 8, (const guchar *) &_plugin_preset_pointer, sizeof (PluginPresetPtr));
		return true;
	}
	return false;
}

void
ProcessorEntry::set_position (Position p, uint32_t num)
{
	_position = p;
	_position_num = num;
	setup_visuals ();
}

void
ProcessorEntry::set_visual_state (Gtkmm2ext::VisualState s, bool yn)
{
	if (yn) {
		_button.set_visual_state (Gtkmm2ext::VisualState (_button.visual_state() | s));
	} else {
		_button.set_visual_state (Gtkmm2ext::VisualState (_button.visual_state() & ~s));
	}
}

void
ProcessorEntry::setup_visuals ()
{
	if (_unknown_processor) {
		_button.set_name ("processor stub");
		return;
	}
	boost::shared_ptr<Send> send;
	if ((send = boost::dynamic_pointer_cast<Send> (_processor))) {
		if (send->remove_on_disconnect ()) {
			_button.set_name ("processor sidechain");
			return;
		}
	}

	boost::shared_ptr<InternalSend> aux;
	if ((aux = boost::dynamic_pointer_cast<InternalSend> (_processor))) {
		if (aux->allow_feedback ()) {
			_button.set_name ("processor auxfeedback");
			return;
		}
	}

	switch (_position) {
	case PreFader:
		if (_plugin_display) { _plugin_display->set_name ("processor prefader"); }
		_button.set_name ("processor prefader");
		break;

	case Fader:
		_button.set_name ("processor fader");
		break;

	case PostFader:
		if (_plugin_display) { _plugin_display->set_name ("processor postfader"); }
		_button.set_name ("processor postfader");
		break;
	}
}


boost::shared_ptr<Processor>
ProcessorEntry::processor () const
{
	if (!_processor) {
		return boost::shared_ptr<Processor>();
	}
	return _processor;
}

void
ProcessorEntry::set_enum_width (Width w)
{
	_width = w;
	_button.set_text (name (_width));
}

void
ProcessorEntry::led_clicked(GdkEventButton *ev)
{
	bool ctrl_shift_pressed = false;
	Keyboard::ModifierMask ctrl_shift_mask = Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier);

	if (Keyboard::modifier_state_equals (ev->state, ctrl_shift_mask)) {
		ctrl_shift_pressed = true;
	}

	if (_processor) {
		if (_button.get_active ()) {
			if (ctrl_shift_pressed) {
				_parent->all_visible_processors_active(false);

				if (_position == Fader) {
					_processor->enable (false);
				}
			}
			else {
				_processor->enable (false);
			}

		} else {
			if (ctrl_shift_pressed) {
				_parent->all_visible_processors_active(true);

				if (_position == Fader) {
					_processor->enable (true);
				}
			}
			else {
				_processor->enable (true);
			}
		}
	}
}

void
ProcessorEntry::processor_active_changed ()
{
	if (_processor) {
		_button.set_active (_processor->enabled ());
	}
}

void
ProcessorEntry::processor_property_changed (const PropertyChange& what_changed)
{
	if (what_changed.contains (ARDOUR::Properties::name)) {
		_button.set_text (name (_width));
		setup_tooltip ();
	}
}

void
ProcessorEntry::processor_configuration_changed (const ChanCount in, const ChanCount out)
{
	_parent->setup_routing_feeds ();
	input_icon.queue_draw();
	output_icon.queue_draw();
	routing_icon.queue_draw();
	output_routing_icon.queue_draw();
}

void
ProcessorEntry::setup_tooltip ()
{
	if (_processor) {
		boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (_processor);
		if (pi) {
			std::string postfix = "";
			uint32_t replicated;

			if (pi->plugin()->has_inline_display()) {
				postfix += string_compose(_("\n%1+double-click to toggle inline-display"), Keyboard::tertiary_modifier_name ());
			}

			if ((replicated = pi->get_count()) > 1) {
				postfix += string_compose(_("\nThis plugin has been replicated %1 times."), replicated);
			}

			if (pi->plugin()->has_editor()) {
				set_tooltip (_button,
						string_compose (_("<b>%1</b>\nDouble-click to show GUI.\n%2+double-click to show generic GUI.%3"), name (Wide), Keyboard::secondary_modifier_name (), postfix));
			} else {
				set_tooltip (_button,
						string_compose (_("<b>%1</b>\nDouble-click to show generic GUI.%2"), name (Wide), postfix));
			}
			return;
		}
		if(boost::dynamic_pointer_cast<UnknownProcessor> (_processor)) {
			ARDOUR_UI::instance()->set_tip (_button,
					string_compose (_("<b>%1</b>\nThe Plugin is not available on this system\nand has been replaced by a stub."), name (Wide)));
			return;
		}
		boost::shared_ptr<Send> send;
		if ((send = boost::dynamic_pointer_cast<Send> (_processor)) != 0 &&
				!boost::dynamic_pointer_cast<InternalSend>(_processor)) {
			if (send->remove_on_disconnect ()) {
				set_tooltip (_button, string_compose ("<b>&gt; %1</b>\nThis (sidechain) send will be removed when disconnected.", _processor->name()));
			} else {
				set_tooltip (_button, string_compose ("<b>&gt; %1</b>", _processor->name()));
			}
			return;
		}
	}
	set_tooltip (_button, string_compose ("<b>%1</b>", name (Wide)));
}

string
ProcessorEntry::name (Width w) const
{
	boost::shared_ptr<Send> send;
	string name_display;

	if (!_processor) {
		return string();
	}

	if ((send = boost::dynamic_pointer_cast<Send> (_processor)) != 0 &&
	    !boost::dynamic_pointer_cast<InternalSend>(_processor)) {

		name_display += '>';
		std::string send_name;
		bool pretty_ok = true;

		if (send->remove_on_disconnect ()) {
			// assume it's a sidechain, find pretty name of connected port(s)
			PortSet& ps (send->output ()->ports ());
			for (PortSet::iterator i = ps.begin (); i != ps.end () && pretty_ok; ++i) {
				vector<string> connections;
				if (i->get_connections (connections)) {
					vector<string>::const_iterator ci;
					for (ci = connections.begin(); ci != connections.end(); ++ci) {
						std::string pn = AudioEngine::instance()->get_pretty_name_by_name (*ci);
						if (pn.empty ()) {
							continue;
						}
						if (send_name.empty ()) {
							send_name = pn;
						} else if (send_name != pn) {
							// pretty names don't match
							pretty_ok = false;
							break;
						}
					}
				}
			}
		}

		if (!pretty_ok) {
			send_name = "";
		}

		/* grab the send name out of its overall name */
		if (send_name.empty()) {
			send_name = send->name();
			string::size_type lbracket, rbracket;
			lbracket = send_name.find ('[');
			rbracket = send_name.find (']');
			send_name = send_name.substr (lbracket+1, lbracket-rbracket-1);
		}

		switch (w) {
		case Wide:
			name_display += send_name;
			break;
		case Narrow:
			name_display += PBD::short_version (send_name, 5);
			break;
		}

	} else {
		boost::shared_ptr<ARDOUR::PluginInsert> pi;
		if ((pi = boost::dynamic_pointer_cast<ARDOUR::PluginInsert> (_processor)) != 0 && pi->get_count() > 1) {
			switch (w) {
				case Wide:
					name_display += "* ";
					break;
				case Narrow:
					name_display += "*";
					break;
			}
		}

		switch (w) {
		case Wide:
			name_display += _processor->display_name();
			break;
		case Narrow:
			name_display += PBD::short_version (_processor->display_name(), 5);
			break;
		}
	}

	return name_display;
}

void
ProcessorEntry::show_all_controls ()
{
	for (list<Control*>::iterator i = _controls.begin(); i != _controls.end(); ++i) {
		(*i)->set_visible (true);
	}

	_parent->update_gui_object_state (this);
}

void
ProcessorEntry::hide_all_controls ()
{
	for (list<Control*>::iterator i = _controls.begin(); i != _controls.end(); ++i) {
		(*i)->set_visible (false);
	}

	_parent->update_gui_object_state (this);
}

void
ProcessorEntry::add_control_state (XMLNode* node) const
{
	for (list<Control*>::const_iterator i = _controls.begin(); i != _controls.end(); ++i) {
		(*i)->add_state (node);
	}

	if (_plugin_display) {
		XMLNode* c = new XMLNode (X_("Object"));
		c->set_property (X_("id"), X_("InlineDisplay"));
		c->set_property (X_("visible"), _plugin_display->is_visible ());
		node->add_child_nocopy (*c);
	}
}

void
ProcessorEntry::set_control_state (XMLNode const * node)
{
	for (list<Control*>::const_iterator i = _controls.begin(); i != _controls.end(); ++i) {
		(*i)->set_state (node);
	}

	if (_plugin_display) {
		XMLNode* n = GUIObjectState::get_node (node, X_("InlineDisplay"));
		if (!n) return;

		bool visible;
		if (n->get_property (X_("visible"), visible)) {
			if (visible) {
				_plugin_display->show ();
			} else {
				_plugin_display->hide ();
			}
		}
	}
}

string
ProcessorEntry::state_id () const
{
	return string_compose ("processor %1", _processor->id().to_s());
}

void
ProcessorEntry::hide_things ()
{
	for (list<Control*>::iterator i = _controls.begin(); i != _controls.end(); ++i) {
		(*i)->hide_things ();
	}
}


Menu*
ProcessorEntry::build_controls_menu ()
{
	using namespace Menu_Helpers;

	if (!_plugin_display && _controls.empty ()) {
		return NULL;
	}

	Menu* menu = manage (new Menu);
	MenuList& items = menu->items ();

	if (_plugin_display) {
		items.push_back (CheckMenuElem (_("Inline Display")));
		Gtk::CheckMenuItem* c = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
		c->set_active (_plugin_display->is_visible ());
		c->signal_toggled().connect (sigc::mem_fun (*this, &ProcessorEntry::toggle_inline_display_visibility));
	}

	if (_controls.empty ()) {
		return menu;
	} else {
		items.push_back (SeparatorElem ());
	}

	items.push_back (
		MenuElem (_("Show All Controls"), sigc::mem_fun (*this, &ProcessorEntry::show_all_controls))
		);

	items.push_back (
		MenuElem (_("Hide All Controls"), sigc::mem_fun (*this, &ProcessorEntry::hide_all_controls))
		);

	items.push_back (SeparatorElem ());

	for (list<Control*>::iterator i = _controls.begin(); i != _controls.end(); ++i) {
		items.push_back (CheckMenuElemNoMnemonic ((*i)->name ()));
		Gtk::CheckMenuItem* c = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
		c->set_active ((*i)->visible ());
		c->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &ProcessorEntry::toggle_control_visibility), *i));
	}

	return menu;
}

void
ProcessorEntry::toggle_inline_display_visibility ()
{
	if (_plugin_display->is_visible ()) {
		_plugin_display->hide();
	} else {
		_plugin_display->show();
	}
	_parent->update_gui_object_state (this);
}

void
ProcessorEntry::toggle_control_visibility (Control* c)
{
	c->set_visible (!c->visible ());
	_parent->update_gui_object_state (this);
}

Menu*
ProcessorEntry::build_send_options_menu ()
{
	using namespace Menu_Helpers;
	Menu* menu = manage (new Menu);
	MenuList& items = menu->items ();

	if (!ARDOUR::Profile->get_mixbus()) {
		boost::shared_ptr<Send> send = boost::dynamic_pointer_cast<Send> (_processor);
		if (send) {
			items.push_back (CheckMenuElem (_("Link panner controls")));
			Gtk::CheckMenuItem* c = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
			c->set_active (send->panner_shell()->is_linked_to_route());
			c->signal_toggled().connect (sigc::mem_fun (*this, &ProcessorEntry::toggle_panner_link));
		}
	}

	boost::shared_ptr<InternalSend> aux = boost::dynamic_pointer_cast<InternalSend> (_processor);
	if (aux) {
		items.push_back (CheckMenuElem (_("Allow Feedback Loop")));
		Gtk::CheckMenuItem* c = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
		c->set_active (aux->allow_feedback());
		c->signal_toggled().connect (sigc::mem_fun (*this, &ProcessorEntry::toggle_allow_feedback));
	}
	return menu;
}

void
ProcessorEntry::toggle_panner_link ()
{
	boost::shared_ptr<Send> send = boost::dynamic_pointer_cast<Send> (_processor);
	if (send) {
		send->panner_shell()->set_linked_to_route(!send->panner_shell()->is_linked_to_route());
	}
}

void
ProcessorEntry::toggle_allow_feedback ()
{
	boost::shared_ptr<InternalSend> aux = boost::dynamic_pointer_cast<InternalSend> (_processor);
	if (aux) {
		aux->set_allow_feedback (!aux->allow_feedback ());
	}
}

ProcessorEntry::Control::Control (boost::shared_ptr<AutomationControl> c, string const & n)
	: _control (c)
	, _adjustment (gain_to_slider_position_with_max (1.0, Config->get_max_gain()), 0, 1, 0.01, 0.1)
	, _slider (&_adjustment, boost::shared_ptr<PBD::Controllable>(), 0, max(13.f, rintf(13.f * UIConfiguration::instance().get_ui_scale())))
	, _slider_persistant_tooltip (&_slider)
	, _button (ArdourButton::led_default_elements)
	, _ignore_ui_adjustment (false)
	, _visible (false)
	, _name (n)
{
	_slider.set_controllable (c);
	box.set_padding(0, 0, 4, 4);

	if (c->toggled()) {
		_button.set_text (_name);
		_button.set_led_left (true);
		_button.set_name ("processor control button");
		box.add (_button);
		_button.show ();

		_button.signal_clicked.connect (sigc::mem_fun (*this, &Control::button_clicked));
		_button.signal_led_clicked.connect (sigc::mem_fun (*this, &Control::button_clicked_event));
		c->Changed.connect (_connections, invalidator (*this), boost::bind (&Control::control_changed, this), gui_context ());
		if (c->alist ()) {
			c->alist()->automation_state_changed.connect (_connections, invalidator (*this), boost::bind (&Control::control_automation_state_changed, this), gui_context());
			control_automation_state_changed ();
		}

	} else {

		_slider.set_name ("ProcessorControlSlider");
		_slider.set_text (_name);

		box.add (_slider);
		_slider.show ();

		const ARDOUR::ParameterDescriptor& desc = c->desc();
		double const lo        = c->internal_to_interface (desc.lower);
		double const up        = c->internal_to_interface (desc.upper);
		double const normal    = c->internal_to_interface (desc.normal);
		double const smallstep = c->internal_to_interface (desc.lower + desc.smallstep);
		double const largestep = c->internal_to_interface (desc.lower + desc.largestep);

		_adjustment.set_lower (lo);
		_adjustment.set_upper (up);
		_adjustment.set_step_increment (smallstep);
		_adjustment.set_page_increment (largestep);
		_slider.set_default_value (normal);

		_slider.StartGesture.connect(sigc::mem_fun(*this, &Control::start_touch));
		_slider.StopGesture.connect(sigc::mem_fun(*this, &Control::end_touch));

		_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &Control::slider_adjusted));
		c->Changed.connect (_connections, invalidator (*this), boost::bind (&Control::control_changed, this), gui_context ());
		if (c->alist ()) {
			c->alist()->automation_state_changed.connect (_connections, invalidator (*this), boost::bind (&Control::control_automation_state_changed, this), gui_context());
			control_automation_state_changed ();
		}
	}

	control_changed ();
	set_tooltip ();

	/* We're providing our own PersistentTooltip */
	set_no_tooltip_whatsoever (_slider);
}

ProcessorEntry::Control::~Control ()
{
}

void
ProcessorEntry::Control::set_tooltip ()
{
	boost::shared_ptr<AutomationControl> c = _control.lock ();

	if (!c) {
		return;
	}
	std::string tt = _name + ": " + ARDOUR::value_as_string (c->desc(), c->get_value ());
	string sm = Gtkmm2ext::markup_escape_text (tt);
	_slider_persistant_tooltip.set_tip (sm);
	ArdourWidgets::set_tooltip (_button, Gtkmm2ext::markup_escape_text (sm));
}

void
ProcessorEntry::Control::slider_adjusted ()
{
	if (_ignore_ui_adjustment) {
		return;
	}

	boost::shared_ptr<AutomationControl> c = _control.lock ();

	if (!c) {
		return;
	}

	c->set_value ( c->interface_to_internal(_adjustment.get_value ()) , Controllable::NoGroup);
	set_tooltip ();
}

void
ProcessorEntry::Control::start_touch ()
{
	boost::shared_ptr<AutomationControl> c = _control.lock ();
	if (!c) {
		return;
	}
	c->start_touch (c->session().transport_sample());
}

void
ProcessorEntry::Control::end_touch ()
{
	boost::shared_ptr<AutomationControl> c = _control.lock ();
	if (!c) {
		return;
	}
	c->stop_touch (c->session().transport_sample());
}

void
ProcessorEntry::Control::button_clicked ()
{
	boost::shared_ptr<AutomationControl> c = _control.lock ();

	if (!c) {
		return;
	}

	bool const n = _button.get_active ();

	c->set_value (n ? 0 : 1, Controllable::NoGroup);
	_button.set_active (!n);
	set_tooltip ();
}

void
ProcessorEntry::Control::button_clicked_event (GdkEventButton *ev)
{
	(void) ev;

	button_clicked ();
}

void
ProcessorEntry::Control::control_automation_state_changed ()
{
	boost::shared_ptr<AutomationControl> c = _control.lock ();
	if (!c) {
		return;
	}
	bool x = c->alist()->automation_state() & Play;
	if (c->toggled ()) {
		_button.set_sensitive (!x);
	} else {
		_slider.set_sensitive (!x);
	}
}

void
ProcessorEntry::Control::control_changed ()
{
	boost::shared_ptr<AutomationControl> c = _control.lock ();
	if (!c) {
		return;
	}

	_ignore_ui_adjustment = true;

	if (c->toggled ()) {
		_button.set_active (c->get_value() > 0.5);
	} else {
		// Note: the _slider watches the controllable by itself
		const double nval = c->internal_to_interface (c->get_value ());
		if (_adjustment.get_value() != nval) {
			_adjustment.set_value (nval);
			set_tooltip ();
		}
	}

	_ignore_ui_adjustment = false;
}

void
ProcessorEntry::Control::add_state (XMLNode* node) const
{
	XMLNode* c = new XMLNode (X_("Object"));
	c->set_property (X_("id"), state_id ());
	c->set_property (X_("visible"), _visible);
	node->add_child_nocopy (*c);
}

void
ProcessorEntry::Control::set_state (XMLNode const * node)
{
	XMLNode* n = GUIObjectState::get_node (node, state_id ());
	if (n) {
		bool visible;
		if (n->get_property (X_("visible"), visible)) {
			set_visible (visible);
		}
	} else {
		boost::shared_ptr<AutomationControl> c = _control.lock ();
		set_visible (c && (c->flags () & Controllable::InlineControl));
	}
}

void
ProcessorEntry::Control::set_visible (bool v)
{
	if (v) {
		box.show ();
	} else {
		box.hide ();
	}

	_visible = v;
}

/** Called when the Editor might have re-shown things that
    we want hidden.
*/
void
ProcessorEntry::Control::hide_things ()
{
	if (!_visible) {
		box.hide ();
	}
}

string
ProcessorEntry::Control::state_id () const
{
	boost::shared_ptr<AutomationControl> c = _control.lock ();
	assert (c);

	return string_compose (X_("control %1"), c->id().to_s ());
}

PluginInsertProcessorEntry::PluginInsertProcessorEntry (ProcessorBox* b, boost::shared_ptr<ARDOUR::PluginInsert> p, Width w)
	: ProcessorEntry (b, p, w)
	, _plugin_insert (p)
{
	p->PluginIoReConfigure.connect (
		_iomap_connection, invalidator (*this), boost::bind (&PluginInsertProcessorEntry::iomap_changed, this), gui_context()
		);
	p->PluginMapChanged.connect (
		_iomap_connection, invalidator (*this), boost::bind (&PluginInsertProcessorEntry::iomap_changed, this), gui_context()
		);
	p->PluginConfigChanged.connect (
		_iomap_connection, invalidator (*this), boost::bind (&PluginInsertProcessorEntry::iomap_changed, this), gui_context()
		);
}

void
PluginInsertProcessorEntry::iomap_changed ()
{
	_parent->setup_routing_feeds ();
	routing_icon.queue_draw();
	output_routing_icon.queue_draw();
}

void
PluginInsertProcessorEntry::hide_things ()
{
	ProcessorEntry::hide_things ();
}

ProcessorEntry::PortIcon::PortIcon(bool input) {
	_input = input;
	_ports = ARDOUR::ChanCount(ARDOUR::DataType::AUDIO, 1);
	set_size_request (-1, std::max (2.f, rintf(2.f * UIConfiguration::instance().get_ui_scale())));
}

bool
ProcessorEntry::PortIcon::on_expose_event (GdkEventExpose* ev)
{
	cairo_t* cr = gdk_cairo_create (get_window()->gobj());

	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	Gtk::Allocation a = get_allocation();
	double const width = a.get_width();
	double const height = a.get_height();

	Gdk::Color const bg = get_style()->get_bg (STATE_NORMAL);
	cairo_set_source_rgb (cr, bg.get_red_p (), bg.get_green_p (), bg.get_blue_p ());

	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	const double dx = rint(max(2., 2. * UIConfiguration::instance().get_ui_scale()));
	for (uint32_t i = 0; i < _ports.n_total(); ++i) {
		set_routing_color (cr, i < _ports.n_midi());
		const double x = ProcessorEntry::RoutingIcon::pin_x_pos (i, width, _ports.n_total(), 0 , false);
		cairo_rectangle (cr, x - .5 - dx * .5, 0, 1 + dx, height);
		cairo_fill(cr);
	}

	cairo_destroy(cr);
	return true;
}

ProcessorEntry::RoutingIcon::RoutingIcon (bool input)
	: _fed_by (false)
	, _input (input)
{
	set_terminal (false);
}

void
ProcessorEntry::RoutingIcon::set_terminal (bool b) {
	_terminal = b;
	int h = std::max (8.f, rintf(8.f * sqrt (UIConfiguration::instance().get_ui_scale())));
	if (_terminal) {
		h += std::max (4.f, rintf(4.f * sqrt (UIConfiguration::instance().get_ui_scale())));
	}
	set_size_request (-1, h);
}

void
ProcessorEntry::RoutingIcon::set (
		const ARDOUR::ChanCount& in,
		const ARDOUR::ChanCount& out,
		const ARDOUR::ChanCount& sinks,
		const ARDOUR::ChanCount& sources,
		const ARDOUR::ChanMapping& in_map,
		const ARDOUR::ChanMapping& out_map,
		const ARDOUR::ChanMapping& thru_map)
{
	_in       = in;
	_out      = out;
	_sources  = sources;
	_sinks    = sinks;
	_in_map   = in_map;
	_out_map  = out_map;
	_thru_map = thru_map;
}

bool
ProcessorEntry::RoutingIcon::in_identity () const {
	if (_thru_map.n_total () > 0) {
		return false;
	}
	if (!_in_map.is_monotonic () || !_in_map.is_identity ()) {
		return false;
	}
	if (_in_map.n_total () != _sinks.n_total () || _in.n_total () != _sinks.n_total ()) {
		return false;
	}
	return true;
}

bool
ProcessorEntry::RoutingIcon::out_identity () const {
	if (_thru_map.n_total () > 0) {
		// TODO skip if trhu is not connected to any of next's inputs
		return false;
	}
	if (!_out_map.is_monotonic () || !_out_map.is_identity ()) {
		return false;
	}
	if (_out_map.n_total () != _sources.n_total () || _out.n_total () != _sources.n_total ()) {
		return false;
	}
	return true;
}

bool
ProcessorEntry::RoutingIcon::can_coalesce () const {
	if (_thru_map.n_total () > 0) {
		return false;
	}
	if (_fed_by && _f_out != _f_sources) {
		return false;
	}
	if (_fed_by && !_f_out_map.is_identity () && !_in_map.is_identity ()) {
		return false;
	}
	if (_input && _sinks == _in && (!_fed_by || _f_out == _in)) {
		return true;
	}
	return false;
}

void
ProcessorEntry::RoutingIcon::set_fed_by (
				const ARDOUR::ChanCount& out,
				const ARDOUR::ChanCount& sources,
				const ARDOUR::ChanMapping& out_map,
				const ARDOUR::ChanMapping& thru_map)
{
	_f_out      = out;
	_f_sources  = sources;
	_f_out_map  = out_map;
	_f_thru_map = thru_map;
	_fed_by     = true;
}

void
ProcessorEntry::RoutingIcon::set_feeding (
				const ARDOUR::ChanCount& in,
				const ARDOUR::ChanCount& sinks,
				const ARDOUR::ChanMapping& in_map,
				const ARDOUR::ChanMapping& thru_map)
{
	_i_in       = in;
	_i_sinks    = sinks;
	_i_in_map   = in_map;
	_i_thru_map = thru_map;
	_feeding    = true;
}

double
ProcessorEntry::RoutingIcon::pin_x_pos (uint32_t i, double width, uint32_t n_total, uint32_t n_midi, bool midi)
{
	if (!midi) { i += n_midi; }
	if (n_total == 1) {
		assert (i == 0);
		return rint (width * .5) +.5;
	}
	return rint (width * (.15 + .7 * i / (n_total - 1))) + .5;
}

void
ProcessorEntry::RoutingIcon::draw_gnd (cairo_t* cr, double x0, double y0, double height, bool midi)
{
	const double dx = 1 + rint (max(2., 2. * UIConfiguration::instance().get_ui_scale()));
	const double y1 = rint (height * .66) + .5;

	cairo_save (cr);
	cairo_translate (cr, x0, y0);
	cairo_move_to (cr, 0, height);
	cairo_line_to (cr, 0, y1);
	cairo_move_to (cr, 0 - dx, y1);
	cairo_line_to (cr, 0 + dx, y1);

	set_routing_color (cr, midi);
	cairo_set_line_width (cr, 1.0);
	cairo_stroke (cr);
	cairo_restore (cr);
}

void
ProcessorEntry::RoutingIcon::draw_sidechain (cairo_t* cr, double x0, double y0, double height, bool midi)
{
	const double dx = 1 + rint (max(2., 2. * UIConfiguration::instance().get_ui_scale()));
	const double y1 = rint (height * .5) - .5;

	cairo_save (cr);
	cairo_translate (cr, x0, y0);
	cairo_move_to (cr, 0 - dx, height);
	cairo_line_to (cr, 0, y1);
	cairo_line_to (cr, 0 + dx, height);
	cairo_close_path (cr);

	set_routing_color (cr, midi);
	cairo_fill (cr);
	cairo_restore (cr);
}

void
ProcessorEntry::RoutingIcon::draw_thru_src (cairo_t* cr, double x0, double y0, double height, bool midi)
{
	const double rad = 1;
	const double y1 = height - rad - 1.5;

	cairo_arc (cr, x0, y0 + y1, rad, 0, 2. * M_PI);
	cairo_move_to (cr, x0, y0 + height - 1.5);
	cairo_line_to (cr, x0, y0 + height);
	set_routing_color (cr, midi);
	cairo_set_line_width  (cr, 1.0);
	cairo_stroke (cr);
}

void
ProcessorEntry::RoutingIcon::draw_thru_sink (cairo_t* cr, double x0, double y0, double height, bool midi)
{
	const double rad = 1;
	const double y1 = rad + 1;

	cairo_arc (cr, x0, y0 + y1, rad, 0, 2. * M_PI);
	cairo_move_to (cr, x0, y0);
	cairo_line_to (cr, x0, y0 + 1);
	set_routing_color (cr, midi);
	cairo_set_line_width  (cr, 1.0);
	cairo_stroke (cr);
}

void
ProcessorEntry::RoutingIcon::draw_connection (cairo_t* cr, double x0, double x1, double y0, double y1, bool midi, bool dashed)
{
	double bz = abs (y1 - y0);

	cairo_move_to (cr, x0, y0);
	cairo_curve_to (cr, x0, y0 + bz, x1, y1 - bz, x1, y1);
	cairo_set_line_width  (cr, 1.0);
	cairo_set_line_cap  (cr,  CAIRO_LINE_CAP_ROUND);
	cairo_set_source_rgb (cr, 1, 0, 0);
	if (dashed) {
		const double dashes[] = { 2, 3 };
		cairo_set_dash (cr, dashes, 2, 0);
	}
	set_routing_color (cr, midi);
	cairo_stroke (cr);
	if (dashed) {
		cairo_set_dash (cr, 0, 0, 0);
	}
}

bool
ProcessorEntry::RoutingIcon::on_expose_event (GdkEventExpose* ev)
{
	cairo_t* cr = gdk_cairo_create (get_window()->gobj());

	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

	Gtk::Allocation a = get_allocation();
	double const width = a.get_width();
	double const height = a.get_height();

	Gdk::Color const bg = get_style()->get_bg (STATE_NORMAL);
	cairo_set_source_rgb (cr, bg.get_red_p (), bg.get_green_p (), bg.get_blue_p ());

	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	if (_input) {
		if (can_coalesce ()) {
			expose_coalesced_input_map (cr, width, height);
		} else {
			expose_input_map (cr, width, height);
		}
	} else {
		expose_output_map (cr, width, height);
	}

	cairo_destroy(cr);
	return true;
}

void
ProcessorEntry::RoutingIcon::expose_coalesced_input_map (cairo_t* cr, const double width, const double height)
{
	const uint32_t pc_in = _sinks.n_total();
	const uint32_t pc_in_midi = _sinks.n_midi();

	for (uint32_t i = 0; i < pc_in; ++i) {
		const bool is_midi = i < pc_in_midi;
		bool valid_in;
		uint32_t pn = is_midi ? i : i - pc_in_midi;
		DataType dt = is_midi ? DataType::MIDI : DataType::AUDIO;
		uint32_t idx = _in_map.get (dt, pn, &valid_in);
		if (!valid_in) {
			double x = pin_x_pos (i, width, pc_in, 0, is_midi);
			draw_gnd (cr, x, 0, height, is_midi);
			continue;
		}
		if (idx >= _in.get (dt)) {
			// side-chain, probably
			double x = pin_x_pos (i, width, pc_in, 0, is_midi);
			draw_sidechain (cr, x, 0, height, is_midi);
			continue;
		}
		double c_x0;
		double c_x1 = pin_x_pos (i, width, pc_in, 0, false);

		if (_fed_by) {
			bool valid_src;
			uint32_t src = _f_out_map.get_src (dt, idx, &valid_src);
			if (!valid_src) {
				double x = pin_x_pos (i, width, pc_in, 0, false);
				bool valid_thru;
				_f_thru_map.get (dt, idx, &valid_thru);
				if (valid_thru) {
					draw_thru_src (cr, x, 0, height, is_midi);
				} else {
					draw_gnd (cr, x, 0, height, is_midi);
				}
				continue;
			}
			c_x0 = pin_x_pos (src, width, _f_sources.n_total(), _f_sources.n_midi(), is_midi);
		} else {
			c_x0 = pin_x_pos (idx, width, _in.n_total(), _in.n_midi(), is_midi);
		}
		draw_connection (cr, c_x0, c_x1, 0, height, is_midi);
	}
}

void
ProcessorEntry::RoutingIcon::expose_input_map (cairo_t* cr, const double width, const double height)
{
	const uint32_t n_in = _in.n_total();
	const uint32_t n_in_midi = _in.n_midi();
	const uint32_t pc_in = _sinks.n_total();
	const uint32_t pc_in_midi = _sinks.n_midi();

	// draw inputs to this
	for (uint32_t i = 0; i < pc_in; ++i) {
		const bool is_midi = i < pc_in_midi;
		bool valid_in;
		uint32_t pn = is_midi ? i : i - pc_in_midi;
		DataType dt = is_midi ? DataType::MIDI : DataType::AUDIO;
		uint32_t idx = _in_map.get (dt, pn, &valid_in);
		// check if it's fed
		bool valid_src = true;
		if (valid_in && idx < _in.get (dt) && _fed_by) {
			bool valid_out;
			bool valid_thru;
			_f_out_map.get_src (dt, idx, &valid_out);
			_f_thru_map.get (dt, idx, &valid_thru);
			if (!valid_out && !valid_thru) {
				valid_src = false;
			}
		}
		if (!valid_in || !valid_src) {
			double x = pin_x_pos (i, width, pc_in, 0, is_midi);
			draw_gnd (cr, x, 0, height, is_midi);
			continue;
		}
		if (idx >= _in.get (dt)) {
			// side-chain, probably
			double x = pin_x_pos (i, width, pc_in, 0, is_midi);
			draw_sidechain (cr, x, 0, height, is_midi);
			continue;
		}
		double c_x1 = pin_x_pos (i, width, pc_in, 0, false);
		double c_x0 = pin_x_pos (idx, width, n_in, n_in_midi, is_midi);
		draw_connection (cr, c_x0, c_x1, 0, height, is_midi);
	}

	// draw reverse thru
	for (uint32_t i = 0; i < n_in; ++i) {
		const bool is_midi = i < n_in_midi;
		bool valid_thru;
		uint32_t pn = is_midi ? i : i - n_in_midi;
		DataType dt = is_midi ? DataType::MIDI : DataType::AUDIO;
		_thru_map.get_src (dt, pn, &valid_thru);
		if (!valid_thru) {
			continue;
		}
		double x = pin_x_pos (i, width, n_in, 0, is_midi);
		draw_thru_sink (cr, x, 0, height, is_midi);
	}
}

void
ProcessorEntry::RoutingIcon::expose_output_map (cairo_t* cr, const double width, const double height)
{
	int dh = std::max (4.f, rintf(4.f * UIConfiguration::instance().get_ui_scale()));
	double ht = _terminal ? height - dh : height;

	// draw outputs of this
	const uint32_t pc_out = _sources.n_total();
	const uint32_t pc_out_midi = _sources.n_midi();
	const uint32_t n_out = _out.n_total();
	const uint32_t n_out_midi = _out.n_midi();

	for (uint32_t i = 0; i < pc_out; ++i) {
		const bool is_midi = i < pc_out_midi;
		bool valid_out;
		uint32_t pn = is_midi ? i : i - pc_out_midi;
		DataType dt = is_midi ? DataType::MIDI : DataType::AUDIO;
		uint32_t idx = _out_map.get (dt, pn, &valid_out);
		if (!valid_out) {
			continue;
		}
		// skip connections that are not used in the next's input
		if (_feeding) {
			bool valid_thru, valid_sink;
			_i_in_map.get_src (dt, idx, &valid_sink);
			_i_thru_map.get_src (dt, idx, &valid_thru);
			if (!valid_thru && !valid_sink) {
				if (!is_midi || i != 0) { // special case midi-bypass
					continue;
				}
			}
		}
		double c_x0 = pin_x_pos (i, width, pc_out, 0, false);
		double c_x1 = pin_x_pos (idx, width, n_out, n_out_midi, is_midi);
		draw_connection (cr, c_x0, c_x1, 0, ht, is_midi);
	}

	for (uint32_t i = 0; i < n_out; ++i) {
		const bool is_midi = i < n_out_midi;
		uint32_t pn = is_midi ? i : i - n_out_midi;
		DataType dt = is_midi ? DataType::MIDI : DataType::AUDIO;
		double x = pin_x_pos (i, width, n_out, 0, is_midi);

		if (!_terminal) {
			bool valid_thru_f = false;
			// skip connections that are not used in the next's input
			if (_feeding) {
				bool valid_sink;
				_i_in_map.get_src (dt, pn, &valid_sink);
				_i_thru_map.get_src (dt, pn, &valid_thru_f);
				if (!valid_thru_f && !valid_sink) {
					if (!is_midi || i != 0) { // special case midi-bypass
						continue;
					}
				}
			}

			bool valid_src;
			_out_map.get_src (dt, pn, &valid_src);
			if (!valid_src) {
				bool valid_thru;
				uint32_t idx = _thru_map.get (dt, pn, &valid_thru);
				if (valid_thru) {
					if (idx >= _in.get (dt)) {
						draw_sidechain (cr, x, 0, height, is_midi);
					} else {
						draw_thru_src (cr, x, 0, height, is_midi);
					}
				} else if (valid_thru_f){
					// gnd is part of input, unless it's a thru input
					// (also only true if !coalesced into one small display)
					draw_gnd (cr, x, 0, height, is_midi);
				}
			}
		} else {
			// terminal node, add arrows
			bool valid_src;
			_out_map.get_src (dt, pn, &valid_src);
			if (!valid_src) {
				bool valid_thru;
				uint32_t idx = _thru_map.get (dt, pn, &valid_thru);
				if (valid_thru) {
					if (idx >= _in.get (dt)) {
						draw_sidechain (cr, x, 0, height - dh, is_midi);
					} else {
						draw_thru_src (cr, x, 0, height - dh, is_midi);
					}
				} else {
					draw_gnd (cr, x, 0, height - dh, is_midi);
				}
			}

			set_routing_color (cr, is_midi);
			cairo_set_line_width (cr, 1.0);
			cairo_move_to (cr, x, height - dh);
			cairo_line_to (cr, x, height - 2);
			cairo_stroke (cr);

			const double ar = dh - 1;
			cairo_move_to (cr, x - ar, height - ar);
			cairo_line_to (cr, x     , height - .5);
			cairo_line_to (cr, x + ar, height - ar);
			cairo_line_to (cr, x     , height - ar * .5);
			cairo_close_path (cr);
			cairo_fill_preserve (cr);
			cairo_stroke (cr);
		}
	}
}

ProcessorEntry::PluginInlineDisplay::PluginInlineDisplay (ProcessorEntry& e, boost::shared_ptr<ARDOUR::Plugin> p, uint32_t max_height)
	: PluginDisplay (p, max_height)
	, _entry (e)
	, _scroll (false)
	, _given_max_height (max_height)
{
	std::string postfix = string_compose(_("\n%1+double-click to toggle inline-display"), Keyboard::tertiary_modifier_name ());

	if (_plug->has_editor()) {
		set_tooltip (*this,
				string_compose (_("<b>%1</b>\nDouble-click to show GUI.\n%2+double-click to show generic GUI.%3"), e.name (Wide), Keyboard::primary_modifier_name (), postfix));
	} else {
		set_tooltip (*this,
				string_compose (_("<b>%1</b>\nDouble-click to show generic GUI.%2"), e.name (Wide), postfix));
	}
}


bool
ProcessorEntry::PluginInlineDisplay::on_button_press_event (GdkEventButton *ev)
{
	assert (_entry.processor ());

	boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (_entry.processor());
	// duplicated code :(
	// consider some tweaks to pass this up to the DnDVBox somehow:
	// select processor, then call (private)
	//_entry._parent->processor_button_press_event (ev, &_entry);
	if (pi && pi->plugin() && pi->plugin()->has_inline_display()
			&& Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)
			&& ev->button == 1
			&& ev->type == GDK_2BUTTON_PRESS) {
		_entry.toggle_inline_display_visibility ();
		return true;
	}
	else if (Keyboard::is_edit_event (ev) || (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS)) {
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {
			_entry._parent->generic_edit_processor (_entry.processor ());
		} else {
			_entry._parent->edit_processor (_entry.processor ());
		}
		return true;
	}
	return false;
}

void
ProcessorEntry::PluginInlineDisplay::on_size_request (Requisition* req)
{
	req->width = 56;
	req->height = _cur_height;
}


void
ProcessorEntry::PluginInlineDisplay::update_height_alloc (uint32_t inline_height)
{
	/* work-around scroll-bar + aspect ratio
	 * show inline-view -> height changes -> scrollbar gets added
	 * -> width changes -> inline-view, fixed aspect ratio -> height changes
	 * -> scroll bar is removed [-> width changes ; repeat ]
	 */
	uint32_t shm = std::min (_max_height, inline_height);
	bool sc = false;
	Gtk::Container* pr = get_parent();
	for (uint32_t i = 0; i < 4 && pr; ++i) {
		// VBox, EventBox, ViewPort, ScrolledWindow
		pr = pr->get_parent();
	}
	Gtk::ScrolledWindow* sw = dynamic_cast<Gtk::ScrolledWindow*> (pr);
	if (sw) {
		const Gtk::VScrollbar* vsb = sw->get_vscrollbar();
		sc = vsb && vsb->is_visible();
	}

	if (shm != _cur_height) {
		queue_resize ();
		if (!_scroll && sc) {
			_max_height = shm;
		} else {
			_max_height = _given_max_height;
		}
		_cur_height = shm;
	}

	_scroll = sc;
}

void
ProcessorEntry::PluginInlineDisplay::display_frame (cairo_t* cr, double w, double h)
{
	Gtkmm2ext::rounded_rectangle (cr, .5, -1.5, w - 1, h + 1, 7);
}

ProcessorEntry::LuaPluginDisplay::LuaPluginDisplay (ProcessorEntry& e, boost::shared_ptr<ARDOUR::LuaProc> p, uint32_t max_height)
	: PluginInlineDisplay (e, p, max_height)
	, _luaproc (p)
	, _lua_render_inline (0)
{
	p->setup_lua_inline_gui (&lua_gui);

	lua_State* LG = lua_gui.getState ();
	LuaInstance::bind_cairo (LG);
	luabridge::LuaRef lua_render = luabridge::getGlobal (LG, "render_inline");
	assert (lua_render.isFunction ());
	_lua_render_inline = new luabridge::LuaRef (lua_render);
}

ProcessorEntry::LuaPluginDisplay::~LuaPluginDisplay ()
{
	delete (_lua_render_inline);
}

uint32_t
ProcessorEntry::LuaPluginDisplay::render_inline (cairo_t *cr, uint32_t width)
{
	Cairo::Context ctx (cr);
	try {
		luabridge::LuaRef rv = (*_lua_render_inline)((Cairo::Context *)&ctx, width, _max_height);
		lua_gui.collect_garbage_step ();
		if (rv.isTable ()) {
			uint32_t h = rv[2];
			return h;
		}
	} catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
		cerr << "LuaException:" << e.what () << endl;
#endif
	} catch (...) { }
	return 0;
}


static std::list<Gtk::TargetEntry> drop_targets()
{
	std::list<Gtk::TargetEntry> tmp;
	tmp.push_back (Gtk::TargetEntry ("processor")); // from processor-box to processor-box
	tmp.push_back (Gtk::TargetEntry ("PluginInfoPtr")); // from plugin-manager
	tmp.push_back (Gtk::TargetEntry ("PluginFavoritePtr")); // from sidebar
	return tmp;
}

static std::list<Gtk::TargetEntry> drag_targets()
{
	std::list<Gtk::TargetEntry> tmp;
	tmp.push_back (Gtk::TargetEntry ("PluginPresetPtr")); // to sidebar (optional preset)
	tmp.push_back (Gtk::TargetEntry ("processor")); // to processor-box (copy)
	return tmp;
}

static std::list<Gtk::TargetEntry> drag_targets_noplugin()
{
	std::list<Gtk::TargetEntry> tmp;
	tmp.push_back (Gtk::TargetEntry ("processor")); // to processor box (sends, faders re-order)
	return tmp;
}

ProcessorBox::ProcessorBox (ARDOUR::Session* sess, boost::function<PluginSelector*()> get_plugin_selector,
			    ProcessorSelection& psel, MixerStrip* parent, bool owner_is_mixer)
	: _parent_strip (parent)
	, _owner_is_mixer (owner_is_mixer)
	, ab_direction (true)
	, _get_plugin_selector (get_plugin_selector)
	, _placement (-1)
	, _p_selection(psel)
	, processor_display (drop_targets())
	, _redisplay_pending (false)
{
	set_session (sess);

	/* ProcessorBox actions and bindings created statically by call to
	 * ProcessorBox::register_actions(), made by ARDOUR_UI so that actions
	 * are available for context menus.
	 */

	processor_display.set_data ("ardour-bindings", bindings);

	_width = Wide;
	processor_menu = 0;
	no_processor_redisplay = false;

	processor_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	processor_scroller.add (processor_display);
	pack_start (processor_scroller, true, true);

	processor_display.set_flags (CAN_FOCUS);
	processor_display.set_name ("ProcessorList");
	processor_display.set_data ("processorbox", this);
	processor_display.set_size_request (48, -1);
	processor_display.set_spacing (0);

	processor_display.signal_enter_notify_event().connect (sigc::mem_fun(*this, &ProcessorBox::enter_notify), false);
	processor_display.signal_leave_notify_event().connect (sigc::mem_fun(*this, &ProcessorBox::leave_notify), false);

	processor_display.ButtonPress.connect (sigc::mem_fun (*this, &ProcessorBox::processor_button_press_event));
	processor_display.ButtonRelease.connect (sigc::mem_fun (*this, &ProcessorBox::processor_button_release_event));

	processor_display.Reordered.connect (sigc::mem_fun (*this, &ProcessorBox::reordered));
	processor_display.DropFromAnotherBox.connect (sigc::mem_fun (*this, &ProcessorBox::object_drop));
	processor_display.DropFromExternal.connect (sigc::mem_fun (*this, &ProcessorBox::plugin_drop));

	processor_scroller.show ();
	processor_display.show ();

	if (parent) {
		parent->DeliveryChanged.connect (
			_mixer_strip_connections, invalidator (*this), boost::bind (&ProcessorBox::mixer_strip_delivery_changed, this, _1), gui_context ()
			);
	}

	set_tooltip (processor_display, _("Right-click to add/remove/edit\nplugins,inserts,sends and more"));
}

ProcessorBox::~ProcessorBox ()
{
	/* it may appear as if we should delete processor_menu but that is a
	 * pointer to a widget owned by the UI Manager, and has potentially
	 * be returned to many other ProcessorBoxes. GTK doesn't really make
	 * clear the ownership of this widget, which is a menu and thus is
	 * never packed into any container other than an implict GtkWindow.
	 *
	 * For now, until or if we ever get clarification over the ownership
	 * story just let it continue to exist. At worst, its a small memory leak.
	 */
}

void
ProcessorBox::set_route (boost::shared_ptr<Route> r)
{
	if (_route == r) {
		return;
	}

	_route_connections.drop_connections();

	/* new route: any existing block on processor redisplay must be meaningless */
	no_processor_redisplay = false;
	_route = r;

	_route->processors_changed.connect (
		_route_connections, invalidator (*this), boost::bind (&ProcessorBox::route_processors_changed, this, _1), gui_context()
		);

	_route->DropReferences.connect (
		_route_connections, invalidator (*this), boost::bind (&ProcessorBox::route_going_away, this), gui_context()
		);

	_route->PropertyChanged.connect (
		_route_connections, invalidator (*this), boost::bind (&ProcessorBox::route_property_changed, this, _1), gui_context()
		);

	redisplay_processors ();
}

void
ProcessorBox::route_going_away ()
{
	no_processor_redisplay = true;
	processor_display.clear ();
	_route.reset ();
}

boost::shared_ptr<Processor>
ProcessorBox::find_drop_position (ProcessorEntry* position)
{
	boost::shared_ptr<Processor> p;
	if (position) {
		p = position->processor ();
		if (!p) {
			/* dropped on the blank entry (which will be before the
				 fader), so use the first non-blank child as our
				 `dropped on' processor */
			list<ProcessorEntry*> c = processor_display.children ();
			list<ProcessorEntry*>::iterator i = c.begin ();

			assert (i != c.end ());
			p = (*i)->processor ();
			assert (p);
		}
	}
	return p;
}

void
ProcessorBox::_drop_plugin_preset (Gtk::SelectionData const &data, Route::ProcessorList &pl)
{
		const void * d = data.get_data();
		const Gtkmm2ext::DnDTreeView<ARDOUR::PluginPresetPtr>* tv = reinterpret_cast<const Gtkmm2ext::DnDTreeView<ARDOUR::PluginPresetPtr>*>(d);

		PluginPresetList nfos;
		TreeView* source;
		tv->get_object_drag_data (nfos, &source);

		for (list<PluginPresetPtr>::const_iterator i = nfos.begin(); i != nfos.end(); ++i) {
			PluginPresetPtr ppp = (*i);
			PluginInfoPtr pip = ppp->_pip;
			PluginPtr p = pip->load (*_session);
			if (!p) {
				continue;
			}

			if (ppp->_preset.valid) {
				p->load_preset (ppp->_preset);
			}

			boost::shared_ptr<Processor> processor (new PluginInsert (*_session, p));
			if (Config->get_new_plugins_active ()) {
				processor->enable (true);
			}
			pl.push_back (processor);
		}
}

void
ProcessorBox::_drop_plugin (Gtk::SelectionData const &data, Route::ProcessorList &pl)
{
		const void * d = data.get_data();
		const Gtkmm2ext::DnDTreeView<ARDOUR::PluginInfoPtr>* tv = reinterpret_cast<const Gtkmm2ext::DnDTreeView<ARDOUR::PluginInfoPtr>*>(d);
		PluginInfoList nfos;

		TreeView* source;
		tv->get_object_drag_data (nfos, &source);

		for (list<PluginInfoPtr>::const_iterator i = nfos.begin(); i != nfos.end(); ++i) {
			PluginPtr p = (*i)->load (*_session);
			if (!p) {
				continue;
			}
			boost::shared_ptr<Processor> processor (new PluginInsert (*_session, p));
			if (Config->get_new_plugins_active ()) {
				processor->enable (true);
			}
			pl.push_back (processor);
		}
}

void
ProcessorBox::plugin_drop (Gtk::SelectionData const &data, ProcessorEntry* position, Glib::RefPtr<Gdk::DragContext> const & context)
{
	if (!_session) {
		return;
	}

	boost::shared_ptr<Processor> p = find_drop_position (position);
	Route::ProcessorList pl;

	if (data.get_target() == "PluginInfoPtr") {
		_drop_plugin (data, pl);
	}
	else if (data.get_target() == "PluginFavoritePtr") {
		_drop_plugin_preset (data, pl);
	}
	else {
		return;
	}

	Route::ProcessorStreams err;
	if (_route->add_processors (pl, p, &err)) {
		string msg = _(
				"Processor Drag/Drop failed. Probably because\n\
the I/O configuration of the plugins could\n\
not match the configuration of this track.");
		MessageDialog am (msg);
		am.run ();
	}
}

void
ProcessorBox::object_drop (DnDVBox<ProcessorEntry>* source, ProcessorEntry* position, Glib::RefPtr<Gdk::DragContext> const & context)
{
	if (Gdk::ACTION_LINK == context->get_selected_action()) {
		list<ProcessorEntry*> children = source->selection ();
		assert (children.size() == 1);
		ProcessorEntry* other = *children.begin();
		assert (other->can_copy_state (position));
		boost::shared_ptr<ARDOUR::Processor> otherproc = other->processor();
		boost::shared_ptr<ARDOUR::Processor> proc = position->processor();
		boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (proc);
		assert (otherproc && proc && pi);

		PBD::ID id = pi->id();
		XMLNode& state = otherproc->get_state ();
		/* strip side-chain state (processor inside processor must be a side-chain)
		 * otherwise we'll end up with duplicate ports-names.
		 * (this needs a better solution which retains connections)
		 */
		state.remove_nodes_and_delete ("Processor");
		/* Controllable and automation IDs should not be copied */
		PBD::Stateful::ForceIDRegeneration force_ids;
		proc->set_state (state, Stateful::loading_state_version);
		/* but retain the processor's ID (LV2 state save) */
		boost::dynamic_pointer_cast<PluginInsert>(proc)->update_id (id);
		return;
	}

	boost::shared_ptr<Processor> p = find_drop_position (position);

	list<ProcessorEntry*> children = source->selection (true);
	list<boost::shared_ptr<Processor> > procs;
	for (list<ProcessorEntry*>::const_iterator i = children.begin(); i != children.end(); ++i) {
		if ((*i)->processor ()) {
			if (boost::dynamic_pointer_cast<UnknownProcessor> ((*i)->processor())) {
				continue;
			}
			if (boost::dynamic_pointer_cast<PortInsert> ((*i)->processor())) {
				continue;
			}
			procs.push_back ((*i)->processor ());
		}
	}

	for (list<boost::shared_ptr<Processor> >::const_iterator i = procs.begin(); i != procs.end(); ++i) {
		XMLNode& state = (*i)->get_state ();
		XMLNodeList nlist;
		nlist.push_back (&state);
		paste_processor_state (nlist, p);
		delete &state;
	}

	/* since the dndvbox doesn't take care of this properly, we have to delete the originals
	   ourselves.
	*/

	if ((context->get_suggested_action() == Gdk::ACTION_MOVE) && source) {
		ProcessorBox* other = reinterpret_cast<ProcessorBox*> (source->get_data ("processorbox"));
		if (other) {
			other->delete_dragged_processors (procs);
		}
	}
}

void
ProcessorBox::set_width (Width w)
{
	if (_width == w) {
		return;
	}

	_width = w;

	list<ProcessorEntry*> children = processor_display.children ();
	for (list<ProcessorEntry*>::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->set_enum_width (w);
	}

	queue_resize ();
}

Gtk::Menu*
ProcessorBox::build_possible_aux_menu ()
{
	boost::shared_ptr<RouteList> rl = _session->get_routes_with_internal_returns();

	if (rl->empty()) {
		/* No aux sends if there are no busses */
		return 0;
	}

	if (_route->is_monitor () || _route->is_foldbackbus ()) {
		return 0;
	}

	using namespace Menu_Helpers;
	Menu* menu = manage (new Menu);
	MenuList& items = menu->items();

	for (RouteList::iterator r = rl->begin(); r != rl->end(); ++r) {
		if ((*r)->is_master() || (*r)->is_monitor () || *r == _route) {
			/* don't allow sending to master or monitor or to self */
			continue;
		}
		if ((*r)->is_foldbackbus ()) {
			continue;
		}
		if (_route->internal_send_for (*r)) {
			/* aux-send to target already exists */
			continue;
		}
		items.push_back (MenuElemNoMnemonic ((*r)->name(), sigc::bind (sigc::ptr_fun (ProcessorBox::rb_choose_aux), boost::weak_ptr<Route>(*r))));
	}

	return menu;
}

Gtk::Menu*
ProcessorBox::build_possible_listener_menu ()
{
	boost::shared_ptr<RouteList> rl = _session->get_routes_with_internal_returns();

	if (rl->empty()) {
		/* No aux sends if there are no busses */
		return 0;
	}

	if (_route->is_monitor () || _route->is_foldbackbus ()) {
		return 0;
	}

	using namespace Menu_Helpers;
	Menu* menu = manage (new Menu);
	MenuList& items = menu->items();

	for (RouteList::iterator r = rl->begin(); r != rl->end(); ++r) {
		if ((*r)->is_master() || (*r)->is_monitor () || *r == _route) {
			/* don't allow sending to master or monitor or to self */
			continue;
		}
		if (!(*r)->is_foldbackbus ()) {
			continue;
		}
		if (_route->internal_send_for (*r)) {
			/* aux-send to target already exists */
			continue;
		}
		items.push_back (MenuElemNoMnemonic ((*r)->name(), sigc::bind (sigc::ptr_fun (ProcessorBox::rb_choose_aux), boost::weak_ptr<Route>(*r))));
	}

	return menu;
}

Gtk::Menu*
ProcessorBox::build_possible_remove_listener_menu ()
{
	boost::shared_ptr<RouteList> rl = _session->get_routes_with_internal_returns();

	if (rl->empty()) {
		/* No aux sends if there are no busses */
		return 0;
	}

	if (_route->is_monitor () || _route->is_foldbackbus ()) {
		return 0;
	}

	using namespace Menu_Helpers;
	Menu* menu = manage (new Menu);
	MenuList& items = menu->items();

	for (RouteList::iterator r = rl->begin(); r != rl->end(); ++r) {
		if ((*r)->is_master() || (*r)->is_monitor () || *r == _route) {
			/* don't allow sending to master or monitor or to self */
			continue;
		}
		if (!(*r)->is_foldbackbus ()) {
			continue;
		}
		if (!_route->internal_send_for (*r)) {
			/* aux-send to target already exists */
			continue;
		}
		items.push_back (MenuElemNoMnemonic ((*r)->name(), sigc::bind (sigc::ptr_fun (ProcessorBox::rb_remove_aux), boost::weak_ptr<Route>(*r))));
	}

	return menu;
}

void
ProcessorBox::show_processor_menu (int arg)
{
	if (processor_menu == 0) {
		processor_menu = build_processor_menu ();
		processor_menu->signal_unmap().connect (sigc::mem_fun (*this, &ProcessorBox::processor_menu_unmapped));
	}

	/* Sort out the plugin submenu */

	Gtk::MenuItem* plugin_menu_item = dynamic_cast<Gtk::MenuItem*>(ActionManager::get_widget("/ProcessorMenu/newplugin"));

	if (plugin_menu_item) {
		plugin_menu_item->set_submenu (*_get_plugin_selector()->plugin_menu());
	}

	/* And the aux submenu */

	Gtk::MenuItem* aux_menu_item = dynamic_cast<Gtk::MenuItem*>(ActionManager::get_widget("/ProcessorMenu/newaux"));

	if (aux_menu_item) {
		Menu* m = build_possible_aux_menu();
		if (m && !m->items().empty()) {
			aux_menu_item->set_submenu (*m);
			aux_menu_item->set_sensitive (true);
		} else {
			delete m;
			/* stupid gtkmm: we need to pass a null reference here */
			gtk_menu_item_set_submenu (aux_menu_item->gobj(), 0);
			aux_menu_item->set_sensitive (false);
		}
	}

	Gtk::MenuItem* listen_menu_item = dynamic_cast<Gtk::MenuItem*>(ActionManager::get_widget("/ProcessorMenu/newlisten"));

	if (listen_menu_item) {
		Menu* m = build_possible_listener_menu();
		if (m && !m->items().empty()) {
			listen_menu_item->set_submenu (*m);
			listen_menu_item->set_sensitive (true);
		} else {
			delete m;
			/* stupid gtkmm: we need to pass a null reference here */
			gtk_menu_item_set_submenu (listen_menu_item->gobj(), 0);
			listen_menu_item->set_sensitive (false);
		}
	}

	Gtk::MenuItem* remove_listen_menu_item = dynamic_cast<Gtk::MenuItem*>(ActionManager::get_widget("/ProcessorMenu/removelisten"));

	if (remove_listen_menu_item) {
		Menu* m = build_possible_remove_listener_menu();
		if (m && !m->items().empty()) {
			remove_listen_menu_item->set_submenu (*m);
			remove_listen_menu_item->set_sensitive (true);
		} else {
			delete m;
			/* stupid gtkmm: we need to pass a null reference here */
			gtk_menu_item_set_submenu (remove_listen_menu_item->gobj(), 0);
			remove_listen_menu_item->set_sensitive (false);
		}
	}

	ActionManager::get_action (X_("ProcessorMenu"), "newinsert")->set_sensitive (!_route->is_monitor () && !_route->is_foldbackbus ());
	ActionManager::get_action (X_("ProcessorMenu"), "newsend")->set_sensitive (!_route->is_monitor () && !_route->is_foldbackbus ());

	ProcessorEntry* single_selection = 0;
	if (processor_display.selection().size() == 1) {
		single_selection = processor_display.selection().front();
	}

	/* And the controls submenu */

	Gtk::MenuItem* controls_menu_item = dynamic_cast<Gtk::MenuItem*>(ActionManager::get_widget("/ProcessorMenu/controls"));

	if (controls_menu_item) {
		if (single_selection) {
			Menu* m = single_selection->build_controls_menu ();
			if (m && !m->items().empty()) {
				controls_menu_item->set_submenu (*m);
				controls_menu_item->set_sensitive (true);
			} else {
				delete m;
				gtk_menu_item_set_submenu (controls_menu_item->gobj(), 0);
				controls_menu_item->set_sensitive (false);
			}
		} else {
			controls_menu_item->set_sensitive (false);
		}
	}


	Gtk::MenuItem* send_menu_item = dynamic_cast<Gtk::MenuItem*>(ActionManager::get_widget("/ProcessorMenu/send_options"));
	if (send_menu_item) {
		if (single_selection && !_route->is_monitor()) {
			Menu* m = single_selection->build_send_options_menu ();
			if (m && !m->items().empty()) {
				send_menu_item->set_submenu (*m);
				send_menu_item->set_sensitive (true);
			} else {
				delete m;
				gtk_menu_item_set_submenu (send_menu_item->gobj(), 0);
				send_menu_item->set_sensitive (false);
			}
		} else {
			send_menu_item->set_sensitive (false);
		}
	}

	/* Sensitise actions as approprioate */

	const bool sensitive = !processor_display.selection().empty() && ! stub_processor_selected ();

	paste_action->set_sensitive (!_p_selection.processors.empty());
	cut_action->set_sensitive (sensitive && can_cut ());
	copy_action->set_sensitive (sensitive);
	delete_action->set_sensitive (sensitive || stub_processor_selected ());
	backspace_action->set_sensitive (sensitive || stub_processor_selected ());

	edit_action->set_sensitive (one_processor_can_be_edited ());
	edit_generic_action->set_sensitive (one_processor_can_be_edited ());

	boost::shared_ptr<PluginInsert> pi;
	if (single_selection) {
		pi = boost::dynamic_pointer_cast<PluginInsert> (single_selection->processor ());
	}

	manage_pins_action->set_sensitive (pi != 0);
	if (boost::dynamic_pointer_cast<Track>(_route)) {
		disk_io_action->set_sensitive (true);
		PBD::Unwinder<bool> uw (_ignore_disk_io_change, true);
		ActionManager::get_toggle_action (X_("ProcessorMenu"), "disk-io-prefader")->set_active (_route->disk_io_point () == DiskIOPreFader);
		ActionManager::get_toggle_action (X_("ProcessorMenu"), "disk-io-postfader")->set_active (_route->disk_io_point () == DiskIOPostFader);
		ActionManager::get_toggle_action (X_("ProcessorMenu"), "disk-io-custom")->set_active (_route->disk_io_point () == DiskIOCustom);
	} else {
		disk_io_action->set_sensitive (false);
	}

	/* allow editing with an Ardour-generated UI for plugin inserts with editors */
	edit_action->set_sensitive (pi && pi->plugin()->has_editor ());

	/* disallow rename for multiple selections, for plugin inserts and for the fader */
	rename_action->set_sensitive (single_selection
			&& !pi
			&& !boost::dynamic_pointer_cast<Amp> (single_selection->processor ())
			&& !boost::dynamic_pointer_cast<UnknownProcessor> (single_selection->processor ()));

	processor_menu->popup (1, arg);

	/* Add a placeholder gap to the processor list to indicate where a processor would be
	   inserted were one chosen from the menu.
	*/
	int x, y;
	processor_display.get_pointer (x, y);
	_placement = processor_display.add_placeholder (y);
}

bool
ProcessorBox::enter_notify (GdkEventCrossing*)
{
	processor_display.grab_focus ();
	_current_processor_box = this;
	return false;
}

bool
ProcessorBox::leave_notify (GdkEventCrossing* ev)
{
	if (ev->detail == GDK_NOTIFY_INFERIOR) {
		return false;
	}

	Widget* top = get_toplevel();

	if (top->is_toplevel()) {
		Window* win = dynamic_cast<Window*> (top);
		gtk_window_set_focus (win->gobj(), 0);
	}

	return false;
}

bool
ProcessorBox::processor_operation (ProcessorOperation op)
{
	ProcSelection targets;

	get_selected_processors (targets);

/*	if (targets.empty()) {

		int x, y;
		processor_display.get_pointer (x, y);

		pair<ProcessorEntry *, double> const pointer = processor_display.get_child_at_position (y);

		if (pointer.first && pointer.first->processor()) {
			targets.push_back (pointer.first->processor ());
		}
	}
*/

	if ((op == ProcessorsDelete) && targets.empty()) {
		return false;  //nothing to delete.  return false so the editor-mixer, because the user was probably intending to delete something in the editor
	}

	switch (op) {
	case ProcessorsSelectAll:
		processor_display.select_all ();
		break;

	case ProcessorsSelectNone:
		processor_display.select_none ();
		break;

	case ProcessorsCopy:
		copy_processors (targets);
		break;

	case ProcessorsCut:
		cut_processors (targets);
		break;

	case ProcessorsPaste:
		// some processors are not selectable (e.g fader, meter), target is empty.
		if (targets.empty() && _placement >= 0) {
			assert (_route);
			boost::shared_ptr<Processor> proc = _route->before_processor_for_index (_placement);
			if (proc) {
				targets.push_back (proc);
			}
		}
		if (targets.empty()) {
			paste_processors ();
		} else {
			paste_processors (targets.front());
		}
		break;

	case ProcessorsDelete:
		delete_processors (targets);
		break;

	case ProcessorsToggleActive:
		for (ProcSelection::iterator i = targets.begin(); i != targets.end(); ++i) {
			if (!(*i)->display_to_user ()) {
				assert (0); // these should not be selectable to begin with.
				continue;
			}
			if (!boost::dynamic_pointer_cast<PluginInsert> (*i)) {
				continue;
			}
#ifdef MIXBUS
			if (boost::dynamic_pointer_cast<PluginInsert> (*i)->is_channelstrip()) {
				continue;
			}
#endif
			(*i)->enable (!(*i)->enabled ());
		}
		break;

	case ProcessorsAB:
		ab_plugins ();
		break;

	default:
		break;
	}

	return true;
}

ProcessorWindowProxy*
ProcessorBox::find_window_proxy (boost::shared_ptr<Processor> processor) const
{
	return  processor->window_proxy();
}


bool
ProcessorBox::processor_button_press_event (GdkEventButton *ev, ProcessorEntry* child)
{
	boost::shared_ptr<Processor> processor;
	if (child) {
		processor = child->processor ();
	}

	int ret = false;
	bool selected = processor_display.selected (child);

	boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (processor);
	if (pi && pi->plugin() && pi->plugin()->has_inline_display()
			&& Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)
			&& ev->button == 1
			&& ev->type == GDK_2BUTTON_PRESS) {
		child->toggle_inline_display_visibility ();
		return true;
	}

	if (processor && (Keyboard::is_edit_event (ev) || (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS))) {

		if (!one_processor_can_be_edited ()) {
			return true;
		}
		if (!ARDOUR_UI_UTILS::engine_is_running ()) {
			return true;
		}

		if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {
			generic_edit_processor (processor);
		} else {
			edit_processor (processor);
		}

		ret = true;

	} else if (Keyboard::is_context_menu_event (ev)) {

		show_processor_menu (ev->time);

		ret = true;

	} else if (processor && ev->button == 1 && selected) {

		// this is purely informational but necessary for route params UI
		ProcessorSelected (processor); // emit

	} else if (!processor && ev->button == 1 && ev->type == GDK_2BUTTON_PRESS) {

		choose_plugin ();
		_get_plugin_selector()->show_manager ();
	}

	return ret;
}

bool
ProcessorBox::processor_button_release_event (GdkEventButton *ev, ProcessorEntry* child)
{
	boost::shared_ptr<Processor> processor;
	if (child) {
		processor = child->processor ();
	}

	if (processor && Keyboard::is_delete_event (ev)) {

		Glib::signal_idle().connect (sigc::bind (
				sigc::mem_fun(*this, &ProcessorBox::idle_delete_processor),
				boost::weak_ptr<Processor>(processor)));

	} else if (processor && Keyboard::is_button2_event (ev)
#ifndef __APPLE__
		   && (Keyboard::no_modifier_keys_pressed (ev) && ((ev->state & Gdk::BUTTON2_MASK) == Gdk::BUTTON2_MASK))
#endif
		) {

		/* button2-click with no/appropriate modifiers */
		processor->enable (!processor->enabled ());
	}

	return false;
}

Menu *
ProcessorBox::build_processor_menu ()
{
	processor_menu = dynamic_cast<Gtk::Menu*>(ActionManager::get_widget("/ProcessorMenu") );
	processor_menu->set_name ("ArdourContextMenu");
	return processor_menu;
}

void
ProcessorBox::select_all_processors ()
{
	processor_display.select_all ();
}

void
ProcessorBox::deselect_all_processors ()
{
	processor_display.select_none ();
}

void
ProcessorBox::choose_plugin ()
{
	_get_plugin_selector()->set_interested_object (*this);
}

/** @return true if an error occurred, otherwise false */
bool
ProcessorBox::use_plugins (const SelectedPlugins& plugins)
{
	for (SelectedPlugins::const_iterator p = plugins.begin(); p != plugins.end(); ++p) {

		boost::shared_ptr<Processor> processor (new PluginInsert (*_session, *p));

		Route::ProcessorStreams err_streams;

		if (_route->add_processor_by_index (processor, _placement, &err_streams, Config->get_new_plugins_active ())) {
			weird_plugin_dialog (**p, err_streams);
			return true;
			// XXX SHAREDPTR delete plugin here .. do we even need to care?
		} else if (plugins.size() == 1 && UIConfiguration::instance().get_open_gui_after_adding_plugin()) {
			if (processor->what_can_be_automated ().size () == 0) {
				; /* plugin without controls, don't show ui */
			}
			else if (boost::dynamic_pointer_cast<PluginInsert>(processor)->plugin()->has_inline_display() && UIConfiguration::instance().get_prefer_inline_over_gui()) {
				; /* only show inline display */
			}
			else if (processor_can_be_edited (processor)) {
				if (!ARDOUR_UI_UTILS::engine_is_running()) {
					return true;
				} else if ((*p)->has_editor ()) {
					edit_processor (processor);
				} else if (boost::dynamic_pointer_cast<PluginInsert>(processor)->plugin()->parameter_count() > 0) {
					generic_edit_processor (processor);
				}
			}
		}
		/* add next processor below the currently added.
		 * Note: placement < 0: add the bottom */
		if (_placement >= 0) {
			++_placement;
		}
	}

	return false;
}

void
ProcessorBox::weird_plugin_dialog (Plugin& p, Route::ProcessorStreams streams)
{
	/* XXX this needs to be re-worked!
	 *
	 * With new pin-management "streams" is no longer correct.
	 * p.get_info () is also incorrect for variable i/o plugins (always -1,-1).
	 *
	 * Since pin-management was added, this dialog will only show in a very rare
	 * condition (non-replicated variable i/o configuration failed).
	 *
	 * TODO: simplify the message after the string-freeze is lifted.
	 */
	ArdourDialog dialog (_("Plugin Incompatibility"));
	Label label;

	string text = string_compose(_("You attempted to add the plugin \"%1\" in slot %2.\n"),
			p.name(), streams.index);

	bool has_midi  = streams.count.n_midi() > 0 || p.get_info()->n_inputs.n_midi() > 0;
	bool has_audio = streams.count.n_audio() > 0 || p.get_info()->n_inputs.n_audio() > 0;

	text += _("\nThis plugin has:\n");
	if (has_midi) {
		uint32_t const n = p.get_info()->n_inputs.n_midi ();
		text += string_compose (ngettext ("\t%1 MIDI input\n", "\t%1 MIDI inputs\n", n), n);
	}
	if (has_audio) {
		uint32_t const n = p.get_info()->n_inputs.n_audio ();
		text += string_compose (ngettext ("\t%1 audio input\n", "\t%1 audio inputs\n", n), n);
	}

	text += _("\nbut at the insertion point, there are:\n");
	if (has_midi) {
		uint32_t const n = streams.count.n_midi ();
		text += string_compose (ngettext ("\t%1 MIDI channel\n", "\t%1 MIDI channels\n", n), n);
	}
	if (has_audio) {
		uint32_t const n = streams.count.n_audio ();
		text += string_compose (ngettext ("\t%1 audio channel\n", "\t%1 audio channels\n", n), n);
	}

	text += string_compose (_("\n%1 is unable to insert this plugin here.\n"), PROGRAM_NAME);
	label.set_text(text);

	dialog.get_vbox()->pack_start (label);
	dialog.add_button (Stock::OK, RESPONSE_ACCEPT);

	dialog.set_name (X_("PluginIODialog"));
	dialog.set_modal (true);
	dialog.show_all ();

	dialog.run ();
}

void
ProcessorBox::choose_insert ()
{
	boost::shared_ptr<Processor> processor (new PortInsert (*_session, _route->pannable(), _route->mute_master()));
	_route->add_processor_by_index (processor, _placement);
}

/* Caller must not hold process lock */
void
ProcessorBox::choose_send ()
{
	boost::shared_ptr<Send> send (new Send (*_session, _route->pannable (), _route->mute_master()));

	/* make an educated guess at the initial number of outputs for the send */
	ChanCount outs = (_route->n_outputs().n_audio() && _session->master_out())
			? _session->master_out()->n_outputs()
			: _route->n_outputs();

	/* XXX need processor lock on route */
	try {
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock());
		send->output()->ensure_io (outs, false, this);
	} catch (AudioEngine::PortRegistrationFailure& err) {
		error << string_compose (_("Cannot set up new send: %1"), err.what()) << endmsg;
		return;
	}

	/* let the user adjust the IO setup before creation.

	   Note: this dialog is NOT modal - we just leave it to run and it will
	   return when its Finished signal is emitted - typically when the window
	   is closed.
	 */

	IOSelectorWindow *ios = new IOSelectorWindow (_session, send->output(), true);
	ios->show ();

	/* keep a reference to the send so it doesn't get deleted while
	   the IOSelectorWindow is doing its stuff
	*/
	_processor_being_created = send;

	ios->selector().Finished.connect (sigc::bind (
			sigc::mem_fun(*this, &ProcessorBox::send_io_finished),
			boost::weak_ptr<Processor>(send), ios));

}

void
ProcessorBox::send_io_finished (IOSelector::Result r, boost::weak_ptr<Processor> weak_processor, IOSelectorWindow* ios)
{
	boost::shared_ptr<Processor> processor (weak_processor.lock());

	/* drop our temporary reference to the new send */
	_processor_being_created.reset ();

	if (!processor) {
		return;
	}

	switch (r) {
	case IOSelector::Cancelled:
		// processor will go away when all shared_ptrs to it vanish
		break;

	case IOSelector::Accepted:
		_route->add_processor_by_index (processor, _placement);
		break;
	}

	delete_when_idle (ios);
}

void
ProcessorBox::return_io_finished (IOSelector::Result r, boost::weak_ptr<Processor> weak_processor, IOSelectorWindow* ios)
{
	boost::shared_ptr<Processor> processor (weak_processor.lock());

	/* drop our temporary reference to the new return */
	_processor_being_created.reset ();

	if (!processor) {
		return;
	}

	switch (r) {
	case IOSelector::Cancelled:
		// processor will go away when all shared_ptrs to it vanish
		break;

	case IOSelector::Accepted:
		_route->add_processor_by_index (processor, _placement);
		break;
	}

	delete_when_idle (ios);
}

void
ProcessorBox::choose_aux (boost::weak_ptr<Route> wr)
{
	if (!_route) {
		return;
	}

	boost::shared_ptr<Route> target = wr.lock();

	if (!target) {
		return;
	}

	if (target->is_foldbackbus ()) {
		_route->add_foldback_send (target);
	} else {
		_session->add_internal_send (target, _placement, _route);
	}
}

void
ProcessorBox::remove_aux (boost::weak_ptr<Route> wr)
{
	if (!_route) {
		return;
	}

	boost::shared_ptr<Route> target = wr.lock();

	if (!target) {
		return;
	}
	boost::shared_ptr<Send>  send = _route->internal_send_for (target);
	boost::shared_ptr<Processor> proc = boost::dynamic_pointer_cast<Processor> (send);
	_route->remove_processor (proc);

}

void
ProcessorBox::route_processors_changed (RouteProcessorChange c)
{
	if (c.type == RouteProcessorChange::MeterPointChange && c.meter_visibly_changed == false) {
		/* the meter has moved, but it was and still is invisible to the user, so nothing to do */
		return;
	}

	redisplay_processors ();
}

void
ProcessorBox::redisplay_processors ()
{
	ENSURE_GUI_THREAD (*this, &ProcessorBox::redisplay_processors);

	if (no_processor_redisplay) {
		return;
	}

	processor_display.clear ();

	_route->foreach_processor (sigc::mem_fun (*this, &ProcessorBox::add_processor_to_display));
	_route->foreach_processor (sigc::mem_fun (*this, &ProcessorBox::maybe_add_processor_to_ui_list));
	_route->foreach_processor (sigc::mem_fun (*this, &ProcessorBox::maybe_add_processor_pin_mgr));

	setup_entry_positions ();
}

/** Add a ProcessorWindowProxy for a processor to our list, if that processor does
 *  not already have one.
 */
void
ProcessorBox::maybe_add_processor_to_ui_list (boost::weak_ptr<Processor> w)
{
	boost::shared_ptr<Processor> p = w.lock ();
	if (!p) {
		return;
	}
	if (p->window_proxy()) {
		return;
	}

	/* see also ProcessorBox::get_editor_window */
	bool have_ui = false;

	if (boost::dynamic_pointer_cast<PluginInsert> (p)) {
		have_ui = true;
	}
	else if (boost::dynamic_pointer_cast<PortInsert> (p)) {
		have_ui = true;
	}
	else if (boost::dynamic_pointer_cast<Send> (p)) {
		if (!boost::dynamic_pointer_cast<InternalSend> (p)) {
			have_ui = true;
		}
	}
	else if (boost::dynamic_pointer_cast<Return> (p)) {
		if (!boost::dynamic_pointer_cast<InternalReturn> (p)) {
			have_ui = true;
		}
	}

	if (!have_ui) {
		return;
	}

	ProcessorWindowProxy* wp = new ProcessorWindowProxy (
			string_compose ("P-%1-%2", _route->id(), p->id()),
			this,
			w);

	const XMLNode* ui_xml = _session->extra_xml (X_("UI"));

	if (ui_xml) {
		wp->set_state (*ui_xml, 0);
	}

	p->set_window_proxy (wp);
	WM::Manager::instance().register_window (wp);
}

void
ProcessorBox::maybe_add_processor_pin_mgr (boost::weak_ptr<Processor> w)
{
	boost::shared_ptr<Processor> p = w.lock ();
	if (!p || p->pinmgr_proxy ()) {
		return;
	}
	if (!boost::dynamic_pointer_cast<PluginInsert> (p)) {
		return;
	}

	PluginPinWindowProxy* wp = new PluginPinWindowProxy (
			string_compose ("PM-%1-%2", _route->id(), p->id()), w);
	wp->set_session (_session);

	const XMLNode* ui_xml = _session->extra_xml (X_("UI"));
	if (ui_xml) {
		wp->set_state (*ui_xml, 0);
	}

	p->set_pingmgr_proxy (wp);
	WM::Manager::instance().register_window (wp);
}

void
ProcessorBox::add_processor_to_display (boost::weak_ptr<Processor> p)
{
	boost::shared_ptr<Processor> processor (p.lock ());

	if (!processor || ( !processor->display_to_user()
#ifndef NDEBUG
	                    && !show_all_processors
#endif
	                  )
	   ) {
		return;
	}

	boost::shared_ptr<PluginInsert> plugin_insert = boost::dynamic_pointer_cast<PluginInsert> (processor);

	ProcessorEntry* e = 0;
	if (plugin_insert) {
		e = new PluginInsertProcessorEntry (this, plugin_insert, _width);
	} else {
		e = new ProcessorEntry (this, processor, _width);
	}

	boost::shared_ptr<Send> send = boost::dynamic_pointer_cast<Send> (processor);
	boost::shared_ptr<PortInsert> ext = boost::dynamic_pointer_cast<PortInsert> (processor);
	boost::shared_ptr<UnknownProcessor> stub = boost::dynamic_pointer_cast<UnknownProcessor> (processor);

	//faders and meters are not deletable, copy/paste-able, so they shouldn't be selectable
	if (!send && !plugin_insert && !ext && !stub) {
		e->set_selectable(false);
	}

	/* Set up this entry's state from the GUIObjectState */
	XMLNode* proc = entry_gui_object_state (e);
	if (proc) {
		e->set_control_state (proc);
		update_gui_object_state (e); /* save updated state (InlineControl) */
	}

	if (plugin_insert
#ifdef MIXBUS
			&& !plugin_insert->plugin(0)->is_channelstrip()
#endif
		 )
	{
		processor_display.add_child (e, drag_targets());
	} else {
		processor_display.add_child (e, drag_targets_noplugin());
	}
}

void
ProcessorBox::reordered ()
{
	compute_processor_sort_keys ();
	setup_entry_positions ();
}

void
ProcessorBox::setup_routing_feeds ()
{
	list<ProcessorEntry*> children = processor_display.children ();
	/* first set the i/o maps for every processor */
	list<ProcessorEntry*>::iterator prev = children.begin();

	for (list<ProcessorEntry*>::iterator i = children.begin(); i != children.end(); ++i) {
		boost::shared_ptr<ARDOUR::Processor> p = (*i)->processor();
		boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (p);

		list<ProcessorEntry*>::iterator next = i;
		next++;

		if (pi) {
			ChanCount sinks = pi->natural_input_streams ();
			ChanCount sources = pi->natural_output_streams ();
			uint32_t count = pi->get_count ();
			ChanCount in, out;
			pi->configured_io (in, out);

			ChanCount midi_thru;
			ChanCount midi_bypass;
			ChanMapping input_map (pi->input_map ());
			if (pi->has_midi_thru ()) {
				 midi_thru.set(DataType::MIDI, 1);
				 input_map.set (DataType::MIDI, 0, 0);
			}
			if (pi->has_midi_bypass ()) {
				 midi_bypass.set(DataType::MIDI, 1);
			}

			(*i)->input_icon.set_ports (sinks * count + midi_thru);
			(*i)->output_icon.set_ports (sources * count + midi_bypass);

			(*i)->routing_icon.set (
					in, out,
					sinks * count + midi_thru,
					sources * count + midi_bypass,
					input_map,
					pi->output_map (),
					pi->thru_map ());

			if (next != children.end()) {
				(*next)->routing_icon.set_fed_by (out, sources * count + midi_bypass,
						pi->output_map (), pi->thru_map ());
			}

			if (prev != i) {
				(*prev)->routing_icon.set_feeding (in, sinks * count + midi_thru,
						pi->input_map (), pi->thru_map ());
			}

		} else {
			(*i)->input_icon.set_ports (p->input_streams());
			(*i)->output_icon.set_ports (p->output_streams());
			ChanMapping inmap (p->input_streams ());
			ChanMapping outmap (p->output_streams ());
			ChanMapping thrumap;
			(*i)->routing_icon.set (
					p->input_streams(),
					p->output_streams(),
					p->input_streams(),
					p->output_streams(),
					inmap, outmap, thrumap);

			if (next != children.end()) {
				(*next)->routing_icon.set_fed_by (
						p->output_streams(),
						p->output_streams(),
						outmap, thrumap);
			}
			if (prev != i) {
				(*prev)->routing_icon.set_feeding (
						p->input_streams(),
						p->output_streams(),
						inmap, thrumap);
			}
		}

		if (i == children.begin()) {
			(*i)->routing_icon.unset_fed_by ();
		}
		prev = i;
		(*i)->input_icon.hide();
	}

	/* now set which icons need to be displayed */
	for (list<ProcessorEntry*>::iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->output_routing_icon.copy_state ((*i)->routing_icon);

		if ((*i)->routing_icon.in_identity ()) {
			(*i)->routing_icon.hide();
			if (i == children.begin()) {
				(*i)->input_icon.show();
			}
		} else {
			(*i)->routing_icon.show();
			(*i)->routing_icon.queue_draw();
			(*i)->input_icon.show();
		}

		list<ProcessorEntry*>::iterator next = i;
		if (++next == children.end()) {
			// last processor in the chain
			(*i)->output_routing_icon.set_terminal(true);
			(*i)->output_routing_icon.unset_feeding ();
			if ((*i)->routing_icon.out_identity ()) {
				(*i)->output_routing_icon.hide();
			} else {
				(*i)->output_routing_icon.show();
				(*i)->output_routing_icon.queue_draw();
			}
		} else {
			(*i)->output_routing_icon.set_terminal(false);
			if (   !(*i)->routing_icon.out_identity ()
					&& !(*next)->routing_icon.in_identity ()
					&&  (*next)->routing_icon.can_coalesce ()) {
				(*i)->output_routing_icon.hide();
			} else if (!(*i)->routing_icon.out_identity ()) {
				(*i)->output_routing_icon.show();
				(*i)->output_routing_icon.queue_draw();
				(*next)->input_icon.show();
			} else {
				(*i)->output_routing_icon.hide();
			}
		}
	}
}

void
ProcessorBox::setup_entry_positions ()
{
	list<ProcessorEntry*> children = processor_display.children ();
	bool pre_fader = true;

	uint32_t num = 0;
	for (list<ProcessorEntry*>::iterator i = children.begin(); i != children.end(); ++i) {
		if (boost::dynamic_pointer_cast<Amp>((*i)->processor()) &&
		    boost::dynamic_pointer_cast<Amp>((*i)->processor())->gain_control()->parameter().type() == GainAutomation) {
			pre_fader = false;
			(*i)->set_position (ProcessorEntry::Fader, num++);
		} else {
			if (pre_fader) {
				(*i)->set_position (ProcessorEntry::PreFader, num++);
			} else {
				(*i)->set_position (ProcessorEntry::PostFader, num++);
			}
		}
	}
	setup_routing_feeds ();
}

void
ProcessorBox::compute_processor_sort_keys ()
{
	list<ProcessorEntry*> children = processor_display.children ();
	Route::ProcessorList our_processors;

	for (list<ProcessorEntry*>::iterator i = children.begin(); i != children.end(); ++i) {
		if ((*i)->processor()) {
			our_processors.push_back ((*i)->processor ());
		}
	}

	if (_route->reorder_processors (our_processors)) {
		/* Reorder failed, so report this to the user.  As far as I can see this must be done
		   in an idle handler: it seems that the redisplay_processors() that happens below destroys
		   widgets that were involved in the drag-and-drop on the processor list, which causes problems
		   when the drag is torn down after this handler function is finished.
		*/
		Glib::signal_idle().connect_once (sigc::mem_fun (*this, &ProcessorBox::report_failed_reorder));
	}
}

void
ProcessorBox::report_failed_reorder ()
{
	/* reorder failed, so redisplay */

	redisplay_processors ();

	/* now tell them about the problem */

	ArdourDialog dialog (_("Plugin Incompatibility"));
	Label label;

	label.set_text (_("\
You cannot reorder these plugins/sends/inserts\n\
in that way because the inputs and\n\
outputs will not work correctly."));

	dialog.get_vbox()->set_border_width (12);
	dialog.get_vbox()->pack_start (label);
	dialog.add_button (Stock::OK, RESPONSE_ACCEPT);

	dialog.set_name (X_("PluginIODialog"));
	dialog.set_modal (true);
	dialog.show_all ();

	dialog.run ();
}

void
ProcessorBox::rename_processors ()
{
	ProcSelection to_be_renamed;

	get_selected_processors (to_be_renamed);

	if (to_be_renamed.empty()) {
		return;
	}

	for (ProcSelection::iterator i = to_be_renamed.begin(); i != to_be_renamed.end(); ++i) {
		rename_processor (*i);
	}
}

bool
ProcessorBox::can_cut () const
{
	vector<boost::shared_ptr<Processor> > sel;

	get_selected_processors (sel);

	/* cut_processors () does not cut inserts */

	for (vector<boost::shared_ptr<Processor> >::const_iterator i = sel.begin (); i != sel.end (); ++i) {

		if (boost::dynamic_pointer_cast<PluginInsert>((*i)) != 0 ||
		    (boost::dynamic_pointer_cast<Send>((*i)) != 0) ||
		    (boost::dynamic_pointer_cast<Return>((*i)) != 0)) {
			return true;
		}
	}

	return false;
}

bool
ProcessorBox::stub_processor_selected () const
{
	vector<boost::shared_ptr<Processor> > sel;

	get_selected_processors (sel);

	for (vector<boost::shared_ptr<Processor> >::const_iterator i = sel.begin (); i != sel.end (); ++i) {
		if (boost::dynamic_pointer_cast<UnknownProcessor>((*i)) != 0) {
			return true;
		}
	}

	return false;
}

void
ProcessorBox::cut_processors (const ProcSelection& to_be_removed)
{
	if (to_be_removed.empty()) {
		return;
	}

	XMLNode* node = new XMLNode (X_("cut"));
	Route::ProcessorList to_cut;

	no_processor_redisplay = true;
	for (ProcSelection::const_iterator i = to_be_removed.begin(); i != to_be_removed.end(); ++i) {
		// Cut only plugins, sends and returns
		if (boost::dynamic_pointer_cast<PluginInsert>((*i)) != 0 ||
		    (boost::dynamic_pointer_cast<Send>((*i)) != 0) ||
		    (boost::dynamic_pointer_cast<Return>((*i)) != 0)) {

			Window* w = get_processor_ui (*i);

			if (w) {
				w->hide ();
			}

			XMLNode& child ((*i)->get_state());
			node->add_child_nocopy (child);
			to_cut.push_back (*i);
		}
	}

	if (_route->remove_processors (to_cut) != 0) {
		delete node;
		no_processor_redisplay = false;
		return;
	}

	_p_selection.set (node);

	no_processor_redisplay = false;
	redisplay_processors ();
}

void
ProcessorBox::copy_processors (const ProcSelection& to_be_copied)
{
	if (to_be_copied.empty()) {
		return;
	}

	XMLNode* node = new XMLNode (X_("copy"));

	for (ProcSelection::const_iterator i = to_be_copied.begin(); i != to_be_copied.end(); ++i) {
		// Copy only plugins, sends, returns
		if (boost::dynamic_pointer_cast<PluginInsert>((*i)) != 0 ||
		    (boost::dynamic_pointer_cast<Send>((*i)) != 0) ||
		    (boost::dynamic_pointer_cast<Return>((*i)) != 0)) {
			node->add_child_nocopy ((*i)->get_state());
		}
	}

	_p_selection.set (node);
}

void
ProcessorBox::delete_processors (const ProcSelection& targets)
{
	if (targets.empty()) {
		return;
	}

	no_processor_redisplay = true;

	for (ProcSelection::const_iterator i = targets.begin(); i != targets.end(); ++i) {

		Window* w = get_processor_ui (*i);

		if (w) {
			w->hide ();
		}

		_route->remove_processor(*i);
	}

	no_processor_redisplay = false;
	redisplay_processors ();
}

void
ProcessorBox::delete_dragged_processors (const list<boost::shared_ptr<Processor> >& procs)
{
	list<boost::shared_ptr<Processor> >::const_iterator x;

	no_processor_redisplay = true;
	for (x = procs.begin(); x != procs.end(); ++x) {

		Window* w = get_processor_ui (*x);

		if (w) {
			w->hide ();
		}

		_route->remove_processor(*x);
	}

	no_processor_redisplay = false;
	redisplay_processors ();
}

gint
ProcessorBox::idle_delete_processor (boost::weak_ptr<Processor> weak_processor)
{
	boost::shared_ptr<Processor> processor (weak_processor.lock());

	if (!processor) {
		return false;
	}

	/* NOT copied to _mixer.selection() */

	no_processor_redisplay = true;
	_route->remove_processor (processor);
	no_processor_redisplay = false;
	redisplay_processors ();

	return false;
}

void
ProcessorBox::rename_processor (boost::shared_ptr<Processor> processor)
{
	Prompter name_prompter (true);
	string result;
	name_prompter.set_title (_("Rename Processor"));
	name_prompter.set_prompt (_("New name:"));
	name_prompter.set_initial_text (processor->name());
	name_prompter.add_button (_("Rename"), Gtk::RESPONSE_ACCEPT);
	name_prompter.set_response_sensitive (Gtk::RESPONSE_ACCEPT, false);
	name_prompter.show_all ();

	switch (name_prompter.run ()) {

	case Gtk::RESPONSE_ACCEPT:
		name_prompter.get_result (result);
		if (result.length() && result != processor->name ()) {

			int tries = 0;
			string test = result;

			while (tries < 100) {
				if (_session->io_name_is_legal (test)) {
					result = test;
					break;
				}
				tries++;

				test = string_compose ("%1-%2", result, tries);
			}

			if (tries < 100) {
				processor->set_name (result);
			} else {
				/* unlikely! */
				ARDOUR_UI::instance()->popup_error
				       (string_compose (_("At least 100 IO objects exist with a name like %1 - name not changed"), result));
			}
		}
		break;
	}

	return;
}

void
ProcessorBox::paste_processors ()
{
	if (_p_selection.processors.empty()) {
		return;
	}

	paste_processor_state (_p_selection.processors.get_node().children(), boost::shared_ptr<Processor>());
}

void
ProcessorBox::paste_processors (boost::shared_ptr<Processor> before)
{

	if (_p_selection.processors.empty()) {
		return;
	}

	paste_processor_state (_p_selection.processors.get_node().children(), before);
}

void
ProcessorBox::paste_processor_state (const XMLNodeList& nlist, boost::shared_ptr<Processor> p)
{
	XMLNodeConstIterator niter;
	list<boost::shared_ptr<Processor> > copies;

	if (nlist.empty()) {
		return;
	}

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		XMLProperty const * type = (*niter)->property ("type");
		XMLProperty const * role = (*niter)->property ("role");
		assert (type);

		boost::shared_ptr<Processor> p;
		try {
			if (type->value() == "meter" ||
			    type->value() == "main-outs" ||
			    type->value() == "amp" ||
			    type->value() == "intreturn") {
				/* do not paste meter, main outs, amp or internal returns */
				continue;

			} else if (type->value() == "intsend") {

				/* aux sends are OK, but those used for
				 * other purposes, are not.
				 */

				assert (role);

				if (role->value() != "Aux") {
					continue;
				}

				boost::shared_ptr<Pannable> sendpan(new Pannable (*_session));
				XMLNode n (**niter);
				InternalSend* s = new InternalSend (*_session, sendpan, _route->mute_master(),
						_route, boost::shared_ptr<Route>(), Delivery::Aux);

				IOProcessor::prepare_for_reset (n, s->name());

				if (s->set_state (n, Stateful::loading_state_version)) {
					delete s;
					return;
				}

				p.reset (s);

			} else if (type->value() == "send") {

				boost::shared_ptr<Pannable> sendpan(new Pannable (*_session));
				XMLNode n (**niter);

				Send* s = new Send (*_session, _route->pannable(), _route->mute_master());

				IOProcessor::prepare_for_reset (n, s->name());

				if (s->set_state (n, Stateful::loading_state_version)) {
					delete s;
					return;
				}

				p.reset (s);

			} else if (type->value() == "return") {

				XMLNode n (**niter);
				Return* r = new Return (*_session);

				IOProcessor::prepare_for_reset (n, r->name());

				if (r->set_state (n, Stateful::loading_state_version)) {
					delete r;
					return;
				}

				p.reset (r);

			} else if (type->value() == "port") {

				XMLNode n (**niter);
				PortInsert* pi = new PortInsert (*_session, _route->pannable (), _route->mute_master ());

				IOProcessor::prepare_for_reset (n, pi->name());

				if (pi->set_state (n, Stateful::loading_state_version)) {
					return;
				}

				p.reset (pi);
			} else {
				/* XXX its a bit limiting to assume that everything else
				   is a plugin.
				*/
				p.reset (new PluginInsert (*_session));
				/* we can't use RAII Stateful::ForceIDRegeneration
				 * because that'd void copying the state and wrongly bump
				 * the state-version counter.
				 * we need to load the state (incl external files) first and
				 * only then update the ID)
				 */
				PBD::ID id = p->id();
				/* strip side-chain state (processor inside processor must be a side-chain)
				 * otherwise we'll end up with duplicate ports-names.
				 * (this needs a better solution which retains connections)
				 */
				XMLNode state (**niter);
				state.remove_nodes_and_delete ("Processor");

				/* Controllable and automation IDs should not be copied */
				PBD::Stateful::ForceIDRegeneration force_ids;
				p->set_state (state, Stateful::current_state_version);
				/* but retain the processor's ID (LV2 state save) */
				boost::dynamic_pointer_cast<PluginInsert>(p)->update_id (id);
			}

			copies.push_back (p);
		}

		catch (...) {
			error << _("plugin insert constructor failed") << endmsg;
		}
	}

	if (copies.empty()) {
		return;
	}

	if (_route->add_processors (copies, p)) {

		string msg = _(
			"Copying the set of processors on the clipboard failed,\n\
probably because the I/O configuration of the plugins\n\
could not match the configuration of this track.");
		MessageDialog am (msg);
		am.run ();
	}
}

void
ProcessorBox::get_selected_processors (ProcSelection& processors) const
{
	const list<ProcessorEntry*> selection = processor_display.selection (true);
	for (list<ProcessorEntry*>::const_iterator i = selection.begin(); i != selection.end(); ++i) {
		processors.push_back ((*i)->processor ());
	}
}

void
ProcessorBox::for_selected_processors (void (ProcessorBox::*method)(boost::shared_ptr<Processor>))
{
	list<ProcessorEntry*> selection = processor_display.selection ();
	for (list<ProcessorEntry*>::iterator i = selection.begin(); i != selection.end(); ++i) {
		(this->*method) ((*i)->processor ());
	}
}

void
ProcessorBox::all_visible_processors_active (bool state)
{
	_route->all_visible_processors_active (state);
}

void
ProcessorBox::ab_plugins ()
{
	_route->ab_plugins (ab_direction);
	ab_direction = !ab_direction;
}

void
ProcessorBox::set_disk_io_position (DiskIOPoint diop)
{
	boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> (_route);
	if (t) {
		t->set_disk_io_point (diop);
	}
}

void
ProcessorBox::clear_processors ()
{
	string prompt;
	vector<string> choices;

	prompt = string_compose (_("Do you really want to remove all processors from %1?\n"
				   "(this cannot be undone)"), _route->name());

	choices.push_back (_("Cancel"));
	choices.push_back (_("Yes, remove them all"));

	ArdourWidgets::Choice prompter (_("Remove processors"), prompt, choices);

	if (prompter.run () == 1) {
		_route->clear_processors (PreFader);
		_route->clear_processors (PostFader);
	}
}

void
ProcessorBox::clear_processors (Placement p)
{
	string prompt;
	vector<string> choices;

	if (p == PreFader) {
		prompt = string_compose (_("Do you really want to remove all pre-fader processors from %1?\n"
					   "(this cannot be undone)"), _route->name());
	} else {
		prompt = string_compose (_("Do you really want to remove all post-fader processors from %1?\n"
					   "(this cannot be undone)"), _route->name());
	}

	choices.push_back (_("Cancel"));
	choices.push_back (_("Yes, remove them all"));

	ArdourWidgets::Choice prompter (_("Remove processors"), prompt, choices);

	if (prompter.run () == 1) {
		_route->clear_processors (p);
	}
}

bool
ProcessorBox::processor_can_be_edited (boost::shared_ptr<Processor> processor)
{
	boost::shared_ptr<AudioTrack> at = boost::dynamic_pointer_cast<AudioTrack> (_route);
	if (at && at->freeze_state() == AudioTrack::Frozen) {
		return false;
	}

	if (
		boost::dynamic_pointer_cast<Send> (processor) ||
		boost::dynamic_pointer_cast<Return> (processor) ||
		boost::dynamic_pointer_cast<PluginInsert> (processor) ||
		boost::dynamic_pointer_cast<PortInsert> (processor)
		) {
		return true;
	}

	return false;
}

bool
ProcessorBox::one_processor_can_be_edited ()
{
	list<ProcessorEntry*> selection = processor_display.selection ();
	list<ProcessorEntry*>::iterator i = selection.begin();
	while (i != selection.end() && processor_can_be_edited ((*i)->processor()) == false) {
		++i;
	}

	return (i != selection.end());
}

Gtk::Window*
ProcessorBox::get_editor_window (boost::shared_ptr<Processor> processor, bool use_custom)
{
	boost::shared_ptr<Send> send;
	boost::shared_ptr<InternalSend> internal_send;
	boost::shared_ptr<Return> retrn;
	boost::shared_ptr<PluginInsert> plugin_insert;
	boost::shared_ptr<PortInsert> port_insert;
	Window* gidget = 0;

	/* This method may or may not return a Window, but if it does not it
	 * will modify the parent mixer strip appearance layout to allow
	 * "editing" the @param processor that was passed in.
	 *
	 * So for example, if the processor is an Amp (gain), the parent strip
	 * will be forced back into a model where the fader controls the main gain.
	 * If the processor is a send, then we map the send controls onto the
	 * strip.
	 *
	 * Plugins and others will return a window for control.
	 */

	if (boost::dynamic_pointer_cast<AudioTrack>(_route) != 0) {

		if (boost::dynamic_pointer_cast<AudioTrack> (_route)->freeze_state() == AudioTrack::Frozen) {
			return 0;
		}
	}

	if (boost::dynamic_pointer_cast<Amp> (processor) && boost::dynamic_pointer_cast<Amp> (processor)->gain_control()->parameter().type() == GainAutomation) {

		if (_parent_strip) {
			_parent_strip->revert_to_default_display ();
		}

	} else if ((send = boost::dynamic_pointer_cast<Send> (processor)) != 0) {

		if (!ARDOUR_UI_UTILS::engine_is_running ()) {
			return 0;
		}

		if (boost::dynamic_pointer_cast<InternalSend> (processor) == 0) {

			gidget = new SendUIWindow (send, _session);
		}

	} else if ((retrn = boost::dynamic_pointer_cast<Return> (processor)) != 0) {

		if (boost::dynamic_pointer_cast<InternalReturn> (retrn)) {
			/* no GUI for these */
			return 0;
		}

		if (!ARDOUR_UI_UTILS::engine_is_running ()) {
			return 0;
		}

		boost::shared_ptr<Return> retrn = boost::dynamic_pointer_cast<Return> (processor);

		ReturnUIWindow *return_ui;
		Window* w = get_processor_ui (retrn);

		if (w == 0) {

			return_ui = new ReturnUIWindow (retrn, _session);
			return_ui->set_title (retrn->name ());
			set_processor_ui (send, return_ui);

		} else {
			return_ui = dynamic_cast<ReturnUIWindow *> (w);
		}

		gidget = return_ui;

	} else if ((plugin_insert = boost::dynamic_pointer_cast<PluginInsert> (processor)) != 0) {

		PluginUIWindow *plugin_ui;

		/* these are both allowed to be null */

		Window* w = get_processor_ui (plugin_insert);

		if (w == 0) {
			plugin_ui = new PluginUIWindow (plugin_insert, false, use_custom);
			plugin_ui->set_title (generate_processor_title (plugin_insert));
			set_processor_ui (plugin_insert, plugin_ui);

		} else {
			plugin_ui = dynamic_cast<PluginUIWindow *> (w);
		}

		gidget = plugin_ui;

	} else if ((port_insert = boost::dynamic_pointer_cast<PortInsert> (processor)) != 0) {

		if (!ARDOUR_UI_UTILS::engine_is_running ()) {
			return 0;
		}

		PortInsertWindow *io_selector;

		Window* w = get_processor_ui (port_insert);

		if (w == 0) {
			io_selector = new PortInsertWindow (_session, port_insert);
			set_processor_ui (port_insert, io_selector);

		} else {
			io_selector = dynamic_cast<PortInsertWindow *> (w);
		}

		gidget = io_selector;
	}

	return gidget;
}

Gtk::Window*
ProcessorBox::get_generic_editor_window (boost::shared_ptr<Processor> processor)
{
	boost::shared_ptr<PluginInsert> plugin_insert
		= boost::dynamic_pointer_cast<PluginInsert>(processor);

	if (!plugin_insert) {
		return 0;
	}

	PluginUIWindow* win = new PluginUIWindow (plugin_insert, true, false);
	win->set_title (generate_processor_title (plugin_insert));

	return win;
}

void
ProcessorBox::register_actions ()
{
	/* We need to use a static object as the owner, since these actions
	   need to be considered ownable by all ProcessorBox objects
	*/

	load_bindings ();

	processor_box_actions = ActionManager::create_action_group (bindings, X_("ProcessorMenu"));

	Glib::RefPtr<Action> act;

	/* new stuff */
	ActionManager::register_action (processor_box_actions, X_("newplugin"), _("New Plugin"),
			sigc::ptr_fun (ProcessorBox::rb_choose_plugin));

	act = ActionManager::register_action (processor_box_actions, X_("newinsert"), _("New Insert"),
			sigc::ptr_fun (ProcessorBox::rb_choose_insert));
	ActionManager::engine_sensitive_actions.push_back (act);
	act = ActionManager::register_action (processor_box_actions, X_("newsend"), _("New External Send ..."),
			sigc::ptr_fun (ProcessorBox::rb_choose_send));
	ActionManager::engine_sensitive_actions.push_back (act);

	ActionManager::register_action (processor_box_actions, X_("newaux"), _("New Aux Send ..."));
	ActionManager::register_action (processor_box_actions, X_("newlisten"), _("New Monitor Send ..."));
	ActionManager::register_action (processor_box_actions, X_("removelisten"), _("Remove Monitor Send ..."));

	ActionManager::register_action (processor_box_actions, X_("controls"), _("Controls"));
	ActionManager::register_action (processor_box_actions, X_("send_options"), _("Send Options"));

	ActionManager::register_action (processor_box_actions, X_("clear"), _("Clear (all)"),
			sigc::ptr_fun (ProcessorBox::rb_clear));
	ActionManager::register_action (processor_box_actions, X_("clear_pre"), _("Clear (pre-fader)"),
			sigc::ptr_fun (ProcessorBox::rb_clear_pre));
	ActionManager::register_action (processor_box_actions, X_("clear_post"), _("Clear (post-fader)"),
			sigc::ptr_fun (ProcessorBox::rb_clear_post));

	/* standard editing stuff */

	cut_action = ActionManager::register_action (processor_box_actions, X_("cut"), _("Cut"),
	                                                    sigc::ptr_fun (ProcessorBox::rb_cut));
	copy_action = ActionManager::register_action (processor_box_actions, X_("copy"), _("Copy"),
	                                                     sigc::ptr_fun (ProcessorBox::rb_copy));
	delete_action = ActionManager::register_action (processor_box_actions, X_("delete"), _("Delete"),
	                                                       sigc::ptr_fun (ProcessorBox::rb_delete));
	backspace_action = ActionManager::register_action (processor_box_actions, X_("backspace"), _("Delete"),
	                                                       sigc::ptr_fun (ProcessorBox::rb_delete));

	ActionManager::plugin_selection_sensitive_actions.push_back (cut_action);
	ActionManager::plugin_selection_sensitive_actions.push_back (copy_action);
	ActionManager::plugin_selection_sensitive_actions.push_back (delete_action);
	ActionManager::plugin_selection_sensitive_actions.push_back (backspace_action);

	paste_action = ActionManager::register_action (processor_box_actions, X_("paste"), _("Paste"),
			sigc::ptr_fun (ProcessorBox::rb_paste));
	rename_action = ActionManager::register_action (processor_box_actions, X_("rename"), _("Rename"),
			sigc::ptr_fun (ProcessorBox::rb_rename));
	ActionManager::register_action (processor_box_actions, X_("selectall"), _("Select All"),
			sigc::ptr_fun (ProcessorBox::rb_select_all));
	ActionManager::register_action (processor_box_actions, X_("deselectall"), _("Deselect All"),
			sigc::ptr_fun (ProcessorBox::rb_deselect_all));

	/* activation etc. */

	ActionManager::register_action (processor_box_actions, X_("activate_all"), _("Activate All"),
			sigc::ptr_fun (ProcessorBox::rb_activate_all));
	ActionManager::register_action (processor_box_actions, X_("deactivate_all"), _("Deactivate All"),
			sigc::ptr_fun (ProcessorBox::rb_deactivate_all));
	ActionManager::register_action (processor_box_actions, X_("ab_plugins"), _("A/B Plugins"),
			sigc::ptr_fun (ProcessorBox::rb_ab_plugins));

	manage_pins_action = ActionManager::register_action (
		processor_box_actions, X_("manage-pins"), _("Pin Connections..."),
		sigc::ptr_fun (ProcessorBox::rb_manage_pins));

	/* Disk IO stuff */
	disk_io_action = ActionManager::register_action (processor_box_actions, X_("disk-io-menu"), _("Disk I/O ..."));
	ActionManager::register_toggle_action (processor_box_actions, X_("disk-io-prefader"), _("Pre-Fader"), sigc::bind (sigc::ptr_fun (ProcessorBox::rb_set_disk_io_position), DiskIOPreFader));
	ActionManager::register_toggle_action (processor_box_actions, X_("disk-io-postfader"), _("Post-Fader"), sigc::bind (sigc::ptr_fun (ProcessorBox::rb_set_disk_io_position), DiskIOPostFader));
	ActionManager::register_toggle_action (processor_box_actions, X_("disk-io-custom"), _("Custom"), sigc::bind (sigc::ptr_fun (ProcessorBox::rb_set_disk_io_position), DiskIOCustom));

	/* show editors */
	edit_action = ActionManager::register_action (
		processor_box_actions, X_("edit"), _("Edit..."),
		sigc::ptr_fun (ProcessorBox::rb_edit));

	edit_generic_action = ActionManager::register_action (
		processor_box_actions, X_("edit-generic"), _("Edit with generic controls..."),
		sigc::ptr_fun (ProcessorBox::rb_edit_generic));

}

void
ProcessorBox::rb_edit_generic ()
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->for_selected_processors (&ProcessorBox::generic_edit_processor);
}

void
ProcessorBox::rb_ab_plugins ()
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->ab_plugins ();
}

void
ProcessorBox::rb_set_disk_io_position (DiskIOPoint diop)
{
	if (_current_processor_box == 0) {
		return;
	}
	if (_ignore_disk_io_change) {
		return;
	}

	_current_processor_box->set_disk_io_position (diop);
}

void
ProcessorBox::rb_manage_pins ()
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->for_selected_processors (&ProcessorBox::manage_pins);
}
void
ProcessorBox::rb_choose_plugin ()
{
	if (_current_processor_box == 0) {
		return;
	}
	_current_processor_box->choose_plugin ();
}

void
ProcessorBox::rb_choose_insert ()
{
	if (_current_processor_box == 0) {
		return;
	}
	_current_processor_box->choose_insert ();
}

void
ProcessorBox::rb_choose_send ()
{
	if (_current_processor_box == 0) {
		return;
	}
	_current_processor_box->choose_send ();
}

void
ProcessorBox::rb_choose_aux (boost::weak_ptr<Route> wr)
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->choose_aux (wr);
}

void
ProcessorBox::rb_remove_aux (boost::weak_ptr<Route> wr)
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->remove_aux (wr);
}

void
ProcessorBox::rb_clear ()
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->clear_processors ();
}


void
ProcessorBox::rb_clear_pre ()
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->clear_processors (PreFader);
}


void
ProcessorBox::rb_clear_post ()
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->clear_processors (PostFader);
}

void
ProcessorBox::rb_cut ()
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->processor_operation (ProcessorsCut);
}

void
ProcessorBox::rb_delete ()
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->processor_operation (ProcessorsDelete);
}

void
ProcessorBox::rb_copy ()
{
	if (_current_processor_box == 0) {
		return;
	}
	_current_processor_box->processor_operation (ProcessorsCopy);
}

void
ProcessorBox::rb_paste ()
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->processor_operation (ProcessorsPaste);
}

void
ProcessorBox::rb_rename ()
{
	if (_current_processor_box == 0) {
		return;
	}
	_current_processor_box->rename_processors ();
}

void
ProcessorBox::rb_select_all ()
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->processor_operation (ProcessorsSelectAll);
}

void
ProcessorBox::rb_deselect_all ()
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->deselect_all_processors ();
}

void
ProcessorBox::rb_activate_all ()
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->all_visible_processors_active (true);
}

void
ProcessorBox::rb_deactivate_all ()
{
	if (_current_processor_box == 0) {
		return;
	}
	_current_processor_box->all_visible_processors_active (false);
}

void
ProcessorBox::rb_edit ()
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->for_selected_processors (&ProcessorBox::edit_processor);
}

bool
ProcessorBox::edit_aux_send (boost::shared_ptr<Processor> processor)
{
	if (boost::dynamic_pointer_cast<InternalSend> (processor) == 0) {
		return false;
	}
	if (ARDOUR::Profile->get_mixbus()) {
		/* don't allow editing sends, ignore switch to send-edit */
		return true;
	}

	if (_parent_strip) {
		boost::shared_ptr<Send> send = boost::dynamic_pointer_cast<Send> (processor);
		if (_parent_strip->current_delivery() == send) {
			_parent_strip->revert_to_default_display ();
		} else {
			_parent_strip->show_send(send);
		}
	}
	return true;
}

void
ProcessorBox::edit_processor (boost::shared_ptr<Processor> processor)
{
	if (!processor) {
		return;
	}
	if (edit_aux_send (processor)) {
		return;
	}
	if (!ARDOUR_UI_UTILS::engine_is_running ()) {
		return;
	}

	ProcessorWindowProxy* proxy = find_window_proxy (processor);

	if (proxy) {
		proxy->set_custom_ui_mode (true);
		proxy->show_the_right_window ();
	}
}

void
ProcessorBox::generic_edit_processor (boost::shared_ptr<Processor> processor)
{
	if (!processor) {
		return;
	}
	if (edit_aux_send (processor)) {
		return;
	}
	if (!ARDOUR_UI_UTILS::engine_is_running ()) {
		return;
	}

	ProcessorWindowProxy* proxy = find_window_proxy (processor);

	if (proxy) {
		proxy->set_custom_ui_mode (false);
		proxy->show_the_right_window ();
	}
}

void
ProcessorBox::manage_pins (boost::shared_ptr<Processor> processor)
{
	if (!processor) {
		return;
	}
	PluginPinWindowProxy* proxy = processor->pinmgr_proxy ();
	if (proxy) {
		proxy->get (true);
		proxy->present();
	}
}


void
ProcessorBox::route_property_changed (const PropertyChange& what_changed)
{
	if (!what_changed.contains (ARDOUR::Properties::name)) {
		return;
	}

	ENSURE_GUI_THREAD (*this, &ProcessorBox::route_property_changed, what_changed);

	boost::shared_ptr<Processor> processor;
	boost::shared_ptr<PluginInsert> plugin_insert;
	boost::shared_ptr<Send> send;

	list<ProcessorEntry*> children = processor_display.children();

	for (list<ProcessorEntry*>::iterator iter = children.begin(); iter != children.end(); ++iter) {

		processor = (*iter)->processor ();

		if (!processor) {
			continue;
		}

		Window* w = get_processor_ui (processor);

		if (!w) {
			continue;
		}

		/* rename editor windows for sends and plugins */

		if ((send = boost::dynamic_pointer_cast<Send> (processor)) != 0) {
			w->set_title (send->name ());
		} else if ((plugin_insert = boost::dynamic_pointer_cast<PluginInsert> (processor)) != 0) {
			w->set_title (generate_processor_title (plugin_insert));
		}
	}
}

string
ProcessorBox::generate_processor_title (boost::shared_ptr<PluginInsert> pi)
{
	string maker = pi->plugin()->maker() ? pi->plugin()->maker() : "";
	string::size_type email_pos;

	if ((email_pos = maker.find_first_of ('<')) != string::npos) {
		maker = maker.substr (0, email_pos - 1);
	}

	if (maker.length() > 32) {
		maker = maker.substr (0, 32);
		maker += " ...";
	}

	SessionObject* owner = pi->owner();

	if (owner) {
		return string_compose(_("%1: %2 (by %3)"), owner->name(), pi->name(), maker);
	} else {
		return string_compose(_("%1 (by %2)"), pi->name(), maker);
	}
}

/** @param p Processor.
 *  @return the UI window for \a p.
 */
Window *
ProcessorBox::get_processor_ui (boost::shared_ptr<Processor> p) const
{
	ProcessorWindowProxy* wp = p->window_proxy();
	if (wp) {
		return wp->get ();
	}
	return 0;
}

/** Make a note of the UI window that a processor is using.
 *  @param p Processor.
 *  @param w UI window.
 */
void
ProcessorBox::set_processor_ui (boost::shared_ptr<Processor> p, Gtk::Window* w)
{
	assert (p->window_proxy());
	p->window_proxy()->use_window (*w);
}

void
ProcessorBox::mixer_strip_delivery_changed (boost::weak_ptr<Delivery> w)
{
	boost::shared_ptr<Delivery> d = w.lock ();
	if (!d) {
		return;
	}

	list<ProcessorEntry*> children = processor_display.children ();
	list<ProcessorEntry*>::const_iterator i = children.begin();
	while (i != children.end() && (*i)->processor() != d) {
		++i;
	}

	if (i == children.end()) {
		processor_display.set_active (0);
	} else {
		processor_display.set_active (*i);
	}
}

/** Called to repair the damage of Editor::show_window doing a show_all() */
void
ProcessorBox::hide_things ()
{
	list<ProcessorEntry*> c = processor_display.children ();
	for (list<ProcessorEntry*>::iterator i = c.begin(); i != c.end(); ++i) {
		(*i)->hide_things ();
	}
}

void
ProcessorBox::processor_menu_unmapped ()
{
	processor_display.remove_placeholder ();
	/* make all possibly-desensitized actions sensitive again so that
	   they be activated by other means (e.g. bindings)
	*/
	ActionManager::set_sensitive (ActionManager::plugin_selection_sensitive_actions, true);
}

XMLNode *
ProcessorBox::entry_gui_object_state (ProcessorEntry* entry)
{
	if (!_parent_strip) {
		return 0;
	}

	GUIObjectState& st = _parent_strip->gui_object_state ();

	XMLNode* strip = st.get_or_add_node (_parent_strip->state_id ());
	assert (strip);
	return st.get_or_add_node (strip, entry->state_id());
}

void
ProcessorBox::update_gui_object_state (ProcessorEntry* entry)
{
	XMLNode* proc = entry_gui_object_state (entry);
	if (!proc) {
		return;
	}

	/* XXX: this is a bit inefficient; we just remove all child nodes and re-add them */
	proc->remove_nodes_and_delete (X_("Object"));
	entry->add_control_state (proc);
}

bool
ProcessorBox::is_editor_mixer_strip() const
{
	return _parent_strip && !_parent_strip->mixer_owned();
}

ProcessorWindowProxy::ProcessorWindowProxy (string const & name, ProcessorBox* box, boost::weak_ptr<Processor> processor)
	: WM::ProxyBase (name, string())
	, _processor_box (box)
	, _processor (processor)
	, is_custom (true)
	, want_custom (true)
{
	boost::shared_ptr<Processor> p = _processor.lock ();
	if (!p) {
		return;
	}
	p->DropReferences.connect (going_away_connection, MISSING_INVALIDATOR, boost::bind (&ProcessorWindowProxy::processor_going_away, this), gui_context());

	p->ToggleUI.connect (gui_connections, invalidator (*this), boost::bind (&ProcessorWindowProxy::show_the_right_window, this, false), gui_context());
	p->ShowUI.connect (gui_connections, invalidator (*this), boost::bind (&ProcessorWindowProxy::show_the_right_window, this, true), gui_context());
	p->HideUI.connect (gui_connections, invalidator (*this), boost::bind (&ProcessorWindowProxy::hide, this), gui_context());
}

ProcessorWindowProxy::~ProcessorWindowProxy()
{
	/* processor window proxies do not own the windows they create with
	 * ::get(), so set _window to null before the normal WindowProxy method
	 * deletes it.
	 */
	_window = 0;
}

void
ProcessorWindowProxy::processor_going_away ()
{
	gui_connections.drop_connections ();
	delete _window;
	_window = 0;
	WM::Manager::instance().remove (this);
	/* should be no real reason to do this, since the object that would
	   send DropReferences is about to be deleted, but lets do it anyway.
	*/
	going_away_connection.disconnect();
	delete this;
}

ARDOUR::SessionHandlePtr*
ProcessorWindowProxy::session_handle()
{
	/* we don't care */
	return 0;
}

XMLNode&
ProcessorWindowProxy::get_state ()
{
	XMLNode *node;
	node = &ProxyBase::get_state();
	node->set_property (X_("custom-ui"), is_custom);
	return *node;
}

int
ProcessorWindowProxy::set_state (const XMLNode& node, int /*version*/)
{
	XMLNodeList children = node.children ();
	XMLNodeList::const_iterator i = children.begin ();
	while (i != children.end()) {
		std::string name;
		if ((*i)->name() == X_("Window") && (*i)->get_property (X_("name"), name) && name == _name) {
			break;
		}
		++i;
	}

	if (i != children.end()) {
		(*i)->get_property (X_("custom-ui"), want_custom);
	}

	return ProxyBase::set_state (node, 0);
}

Gtk::Window*
ProcessorWindowProxy::get (bool create)
{
	boost::shared_ptr<Processor> p = _processor.lock ();

	if (!p) {
		return 0;
	}
	if (_window && (is_custom != want_custom)) {
		/* drop existing window - wrong type */
		set_state_mask (Gtkmm2ext::WindowProxy::StateMask (state_mask () & ~WindowProxy::Size));
		drop_window ();
	}

	if (!_window) {
		if (!create) {
			return 0;
		}

		is_custom = want_custom;
		_window = _processor_box->get_editor_window (p, is_custom);

		if (_window) {
			setup ();
			_window->show_all ();
		}
	}
	return _window;
}

void
ProcessorWindowProxy::show_the_right_window (bool show_not_toggle)
{
	if (_window && (is_custom != want_custom)) {
		/* drop existing window - wrong type */
		set_state_mask (Gtkmm2ext::WindowProxy::StateMask (state_mask () & ~WindowProxy::Size));
		drop_window ();
	}
	if (_window && fully_visible () && show_not_toggle) {
		return;
	}
	toggle ();
}


PluginPinWindowProxy::PluginPinWindowProxy(std::string const &name, boost::weak_ptr<ARDOUR::Processor> processor)
	: WM::ProxyBase (name, string())
	, _processor (processor)
{
	boost::shared_ptr<Processor> p = _processor.lock ();
	if (!p) {
		return;
	}
	p->DropReferences.connect (going_away_connection, MISSING_INVALIDATOR, boost::bind (&PluginPinWindowProxy::processor_going_away, this), gui_context());
}

PluginPinWindowProxy::~PluginPinWindowProxy()
{
	_window = 0;
}

ARDOUR::SessionHandlePtr*
PluginPinWindowProxy::session_handle ()
{
	ArdourWindow* aw = dynamic_cast<ArdourWindow*> (_window);
	if (aw) { return aw; }
	return 0;
}

Gtk::Window*
PluginPinWindowProxy::get (bool create)
{
	boost::shared_ptr<Processor> p = _processor.lock ();
	boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (p);
	if (!p || !pi) {
		return 0;
	}

	if (!_window) {
		if (!create) {
			return 0;
		}
		_window = new PluginPinDialog (pi);
		ArdourWindow* aw = dynamic_cast<ArdourWindow*> (_window);
		if (aw) {
			aw->set_session (_session);
		}
		_window->show_all ();
	}
	return _window;
}

void
PluginPinWindowProxy::processor_going_away ()
{
	delete _window;
	_window = 0;
	WM::Manager::instance().remove (this);
	going_away_connection.disconnect();
	delete this;
}

void
ProcessorBox::load_bindings ()
{
	bindings = Bindings::get_bindings (X_("Processor Box"));
}
