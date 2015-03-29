/*
    Copyright (C) 2000-2004 Paul Davis

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

#ifdef COMPILER_MSVC
#define rintf(x) round((x) + 0.5)
#endif

#include <cmath>
#include <iostream>
#include <set>

#include <sigc++/bind.h>

#include "pbd/convert.h"

#include <glibmm/miscutils.h>

#include <gtkmm/messagedialog.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/choice.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/doi.h>
#include <gtkmm2ext/rgb_macros.h>

#include "ardour/amp.h"
#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/internal_return.h"
#include "ardour/internal_send.h"
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

#include "actions.h"
#include "ardour_dialog.h"
#include "ardour_ui.h"
#include "gui_thread.h"
#include "io_selector.h"
#include "keyboard.h"
#include "mixer_ui.h"
#include "mixer_strip.h"
#include "plugin_selector.h"
#include "plugin_ui.h"
#include "port_insert_ui.h"
#include "processor_box.h"
#include "public_editor.h"
#include "return_ui.h"
#include "route_processor_selection.h"
#include "send_ui.h"
#include "timers.h"

#include "i18n.h"

#ifdef AUDIOUNIT_SUPPORT
class AUPluginUI;
#endif

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;

ProcessorBox* ProcessorBox::_current_processor_box = 0;
RefPtr<Action> ProcessorBox::paste_action;
RefPtr<Action> ProcessorBox::cut_action;
RefPtr<Action> ProcessorBox::rename_action;
RefPtr<Action> ProcessorBox::edit_action;
RefPtr<Action> ProcessorBox::edit_generic_action;

static const uint32_t audio_port_color = 0x4A8A0EFF; // Green
static const uint32_t midi_port_color = 0x960909FF; //Red

ProcessorEntry::ProcessorEntry (ProcessorBox* parent, boost::shared_ptr<Processor> p, Width w)
	: _button (ArdourButton::led_default_elements)
	, _position (PreFader)
	, _position_num(0)
	, _selectable(true)
	, _parent (parent)
	, _processor (p)
	, _width (w)
	, _input_icon(true)
	, _output_icon(false)
{
	_vbox.show ();
	
	_button.set_distinct_led_click (true);
	_button.set_fallthrough_to_parent(true);
	_button.set_led_left (true);
	_button.signal_led_clicked.connect (sigc::mem_fun (*this, &ProcessorEntry::led_clicked));
	_button.set_text (name (_width));

	if (_processor) {

		_vbox.pack_start (_routing_icon);
		_vbox.pack_start (_input_icon);
		_vbox.pack_start (_button, true, true);
		_vbox.pack_end (_output_icon);

		_button.set_active (_processor->active());

		_routing_icon.set_no_show_all(true);
		_input_icon.set_no_show_all(true);

		_button.show ();
		_routing_icon.set_visible(false);
		_input_icon.hide();
		_output_icon.show();

		_processor->ActiveChanged.connect (active_connection, invalidator (*this), boost::bind (&ProcessorEntry::processor_active_changed, this), gui_context());
		_processor->PropertyChanged.connect (name_connection, invalidator (*this), boost::bind (&ProcessorEntry::processor_property_changed, this, _1), gui_context());
		_processor->ConfigurationChanged.connect (config_connection, invalidator (*this), boost::bind (&ProcessorEntry::processor_configuration_changed, this, _1, _2), gui_context());

		set<Evoral::Parameter> p = _processor->what_can_be_automated ();
		for (set<Evoral::Parameter>::iterator i = p.begin(); i != p.end(); ++i) {
			
			std::string label = _processor->describe_parameter (*i);

			if (boost::dynamic_pointer_cast<Send> (_processor)) {
				label = _("Send");
			} else if (boost::dynamic_pointer_cast<Return> (_processor)) {
				label = _("Return");
			}

			Control* c = new Control (_processor->automation_control (*i), label);
			
			_controls.push_back (c);

			if (boost::dynamic_pointer_cast<Amp> (_processor) == 0) {
				/* Add non-Amp controls to the processor box */
				_vbox.pack_start (c->box);
			}
		}

		_input_icon.set_ports(_processor->input_streams());
		_output_icon.set_ports(_processor->output_streams());

		_routing_icon.set_sources(_processor->input_streams());
		_routing_icon.set_sinks(_processor->output_streams());

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

void
ProcessorEntry::set_position (Position p, uint32_t num)
{
	_position = p;
	_position_num = num;

	if (_position_num == 0 || _routing_icon.get_visible()) {
		_input_icon.show();
	} else {
		_input_icon.hide();
	}

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
	switch (_position) {
	case PreFader:
		_button.set_name ("processor prefader");
		break;

	case Fader:
		_button.set_name ("processor fader");
		break;

	case PostFader:
		_button.set_name ("processor postfader");
		break;
	}
}


boost::shared_ptr<Processor>
ProcessorEntry::processor () const
{
	return _processor;
}

void
ProcessorEntry::set_enum_width (Width w)
{
	_width = w;
	_button.set_text (name (_width));
}

void
ProcessorEntry::led_clicked()
{
	if (_processor) {
		if (_button.get_active ()) {
			_processor->deactivate ();
		} else {
			_processor->activate ();
		}
	}
}

void
ProcessorEntry::processor_active_changed ()
{
	if (_processor) {
		_button.set_active (_processor->active());
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
	_input_icon.set_ports(in);
	_output_icon.set_ports(out);
	_routing_icon.set_sources(in);
	_routing_icon.set_sinks(out);
	_input_icon.queue_draw();
	_output_icon.queue_draw();
	_routing_icon.queue_draw();
}

void
ProcessorEntry::setup_tooltip ()
{
	if (_processor) {
		boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (_processor);
		if (pi) {
			std::string postfix = "";
			uint32_t replicated;
			if ((replicated = pi->get_count()) > 1) {
				postfix = string_compose(_("\nThis mono plugin has been replicated %1 times."), replicated);
			}
			if (pi->plugin()->has_editor()) {
				ARDOUR_UI::instance()->set_tip (_button,
						string_compose (_("<b>%1</b>\nDouble-click to show GUI.\nAlt+double-click to show generic GUI.%2"), name (Wide), postfix));
			} else {
				ARDOUR_UI::instance()->set_tip (_button,
						string_compose (_("<b>%1</b>\nDouble-click to show generic GUI.%2"), name (Wide), postfix));
			}
			return;
		}
	}
	ARDOUR_UI::instance()->set_tip (_button, string_compose ("<b>%1</b>", name (Wide)));
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

		/* grab the send name out of its overall name */

		string::size_type lbracket, rbracket;
		lbracket = send->name().find ('[');
		rbracket = send->name().find (']');

		switch (w) {
		case Wide:
			name_display += send->name().substr (lbracket+1, lbracket-rbracket-1);
			break;
		case Narrow:
			name_display += PBD::short_version (send->name().substr (lbracket+1, lbracket-rbracket-1), 4);
			break;
		}

	} else {
		boost::shared_ptr<ARDOUR::PluginInsert> pi;
		uint32_t replicated;
		if ((pi = boost::dynamic_pointer_cast<ARDOUR::PluginInsert> (_processor)) != 0
				&& (replicated = pi->get_count()) > 1)
		{
			name_display += string_compose(_("(%1x1) "), replicated);
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
}

void
ProcessorEntry::set_control_state (XMLNode const * node)
{
	for (list<Control*>::const_iterator i = _controls.begin(); i != _controls.end(); ++i) {
		(*i)->set_state (node);
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


Menu *
ProcessorEntry::build_controls_menu ()
{
	using namespace Menu_Helpers;
	Menu* menu = manage (new Menu);
	MenuList& items = menu->items ();

	items.push_back (
		MenuElem (_("Show All Controls"), sigc::mem_fun (*this, &ProcessorEntry::show_all_controls))
		);
		
	items.push_back (
		MenuElem (_("Hide All Controls"), sigc::mem_fun (*this, &ProcessorEntry::hide_all_controls))
		);

	if (!_controls.empty ()) {
		items.push_back (SeparatorElem ());
	}
	
	for (list<Control*>::iterator i = _controls.begin(); i != _controls.end(); ++i) {
		items.push_back (CheckMenuElem ((*i)->name ()));
		Gtk::CheckMenuItem* c = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
		c->set_active ((*i)->visible ());
		c->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &ProcessorEntry::toggle_control_visibility), *i));
	}

	return menu;
}

void
ProcessorEntry::toggle_control_visibility (Control* c)
{
	c->set_visible (!c->visible ());
	_parent->update_gui_object_state (this);
}

Menu *
ProcessorEntry::build_send_options_menu ()
{
	using namespace Menu_Helpers;
	Menu* menu = manage (new Menu);
	MenuList& items = menu->items ();

	boost::shared_ptr<Send> send = boost::dynamic_pointer_cast<Send> (_processor);
	if (send) {

		items.push_back (CheckMenuElem (_("Link panner controls")));
		Gtk::CheckMenuItem* c = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
		c->set_active (send->panner_shell()->is_linked_to_route());
		c->signal_toggled().connect (sigc::mem_fun (*this, &ProcessorEntry::toggle_panner_link));

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

ProcessorEntry::Control::Control (boost::shared_ptr<AutomationControl> c, string const & n)
	: _control (c)
	, _adjustment (gain_to_slider_position_with_max (1.0, Config->get_max_gain()), 0, 1, 0.01, 0.1)
	, _slider (&_adjustment, boost::shared_ptr<PBD::Controllable>(), 0, 13)
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
		_button.signal_led_clicked.connect (sigc::mem_fun (*this, &Control::button_clicked));
		c->Changed.connect (_connection, MISSING_INVALIDATOR, boost::bind (&Control::control_changed, this), gui_context ());

	} else {
		
		_slider.set_name ("ProcessorControlSlider");
		_slider.set_text (_name);

		box.add (_slider);
		_slider.show ();

		const ARDOUR::ParameterDescriptor& desc = c->desc();
		double const lo = c->internal_to_interface(desc.lower);
		double const up = c->internal_to_interface(desc.upper);
		double const normal = c->internal_to_interface(desc.normal);
		double smallstep = desc.smallstep;
		double largestep = desc.largestep;

		if (smallstep == 0.0) {
			smallstep = up / 1000.;
		} else {
			smallstep = c->internal_to_interface(desc.lower + smallstep);
		}

		if (largestep == 0.0) {
			largestep = up / 40.;
		} else {
			largestep = c->internal_to_interface(desc.lower + largestep);
		}

		_adjustment.set_lower (lo);
		_adjustment.set_upper (up);
		_adjustment.set_step_increment (smallstep);
		_adjustment.set_page_increment (largestep);
		_slider.set_default_value (normal);
		
		_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &Control::slider_adjusted));
		c->Changed.connect (_connection, MISSING_INVALIDATOR, boost::bind (&Control::control_changed, this), gui_context ());
	}

	Timers::rapid_connect (sigc::mem_fun (*this, &Control::control_changed));
	
	control_changed ();
	set_tooltip ();

	/* We're providing our own PersistentTooltip */
	set_no_tooltip_whatsoever (_slider);
}

void
ProcessorEntry::Control::set_tooltip ()
{
	boost::shared_ptr<AutomationControl> c = _control.lock ();

	if (!c) {
		return;
	}
	
	stringstream s;
	s << _name << ": ";
	if (c->toggled ()) {
		s << (c->get_value() > 0.5 ? _("on") : _("off"));
	} else {
		s << setprecision(2) << fixed;
		s << c->internal_to_user (c->get_value ());
	}

	string sm = Glib::Markup::escape_text (s.str());
	
	_slider_persistant_tooltip.set_tip (sm);
	ARDOUR_UI::instance()->set_tip (_button, sm);
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

	c->set_value ( c->interface_to_internal(_adjustment.get_value ()) );
	set_tooltip ();
}

void
ProcessorEntry::Control::button_clicked ()
{
	boost::shared_ptr<AutomationControl> c = _control.lock ();

	if (!c) {
		return;
	}

	bool const n = _button.get_active ();

	c->set_value (n ? 0 : 1);
	_button.set_active (!n);
	set_tooltip ();
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

		_adjustment.set_value (c->internal_to_interface(c->get_value ()));
		set_tooltip ();
	}
	
	_ignore_ui_adjustment = false;
}

void
ProcessorEntry::Control::add_state (XMLNode* node) const
{
	XMLNode* c = new XMLNode (X_("Object"));
	c->add_property (X_("id"), state_id ());
	c->add_property (X_("visible"), _visible);
	node->add_child_nocopy (*c);
}

void
ProcessorEntry::Control::set_state (XMLNode const * node)
{
	XMLNode* n = GUIObjectState::get_node (node, state_id ());
	if (n) {
		XMLProperty* p = n->property (X_("visible"));
		set_visible (p && string_is_affirmative (p->value ()));
	} else {
		set_visible (false);
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
		_splitting_connection, invalidator (*this), boost::bind (&PluginInsertProcessorEntry::plugin_insert_splitting_changed, this), gui_context()
		);

	plugin_insert_splitting_changed ();
}

void
PluginInsertProcessorEntry::plugin_insert_splitting_changed ()
{
	_output_icon.set_ports(_plugin_insert->output_streams());
	_routing_icon.set_splitting(_plugin_insert->splitting ());

	ChanCount sources = _plugin_insert->input_streams();
	ChanCount sinks = _plugin_insert->natural_input_streams();

	/* replicated instances */
	if (!_plugin_insert->splitting () && _plugin_insert->get_count() > 1) {
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			sinks.set(*t, sinks.get(*t) * _plugin_insert->get_count());
		}
	}
	/* MIDI bypass */
	if (_plugin_insert->natural_output_streams().n_midi() == 0 &&
			_plugin_insert->output_streams().n_midi() == 1) {
		sinks.set(DataType::MIDI, 1);
		sources.set(DataType::MIDI, 1);
	}

	_input_icon.set_ports(sinks);
	_routing_icon.set_sinks(sinks);
	_routing_icon.set_sources(sources);

	if (_plugin_insert->splitting () ||
			_plugin_insert->input_streams().n_audio() < _plugin_insert->natural_input_streams().n_audio()
		 )
	{
		_routing_icon.set_size_request (-1, 7);
		_routing_icon.set_visible(true);
		_input_icon.show();
	} else {
		_routing_icon.set_visible(false);
		if (_position_num != 0) {
			_input_icon.hide();
		}
	}

	_input_icon.queue_draw();
	_output_icon.queue_draw();
	_routing_icon.queue_draw();
}

void
PluginInsertProcessorEntry::hide_things ()
{
	ProcessorEntry::hide_things ();
	plugin_insert_splitting_changed ();
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

	if (_ports.n_total() > 1) {
		for (uint32_t i = 0; i < _ports.n_total(); ++i) {
			if (i < _ports.n_midi()) {
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
			const float x = rintf(width * (.2f + .6f * i / (_ports.n_total() - 1.f)));
			cairo_rectangle (cr, x-1, 0, 3, height);
			cairo_fill(cr);
		}
	} else if (_ports.n_total() == 1) {
		if (_ports.n_midi() == 1) {
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
		const float x = rintf(width * .5);
		cairo_rectangle (cr, x-1, 0, 3, height);
		cairo_fill(cr);
		cairo_stroke(cr);
	}

	cairo_destroy(cr);
	return true;
}

bool
ProcessorEntry::RoutingIcon::on_expose_event (GdkEventExpose* ev)
{
	cairo_t* cr = gdk_cairo_create (get_window()->gobj());

	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	cairo_set_line_width (cr, 1.0);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

	Gtk::Allocation a = get_allocation();
	double const width = a.get_width();
	double const height = a.get_height();

	Gdk::Color const bg = get_style()->get_bg (STATE_NORMAL);
	cairo_set_source_rgb (cr, bg.get_red_p (), bg.get_green_p (), bg.get_blue_p ());

	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	Gdk::Color const fg = get_style()->get_fg (STATE_NORMAL);
	cairo_set_source_rgb (cr, fg.get_red_p (), fg.get_green_p (), fg.get_blue_p ());

	const uint32_t sources = _sources.n_total();
	const uint32_t sinks = _sinks.n_total();

	/* MIDI */
	const uint32_t midi_sources = _sources.n_midi();
	const uint32_t midi_sinks = _sinks.n_midi();

	cairo_set_source_rgb (cr,
			UINT_RGBA_R_FLT(midi_port_color),
			UINT_RGBA_G_FLT(midi_port_color),
			UINT_RGBA_B_FLT(midi_port_color));
	if (midi_sources > 0 && midi_sinks > 0 && sinks > 1 && sources > 1) {
		for (uint32_t i = 0 ; i < midi_sources; ++i) {
			const float si_x  = rintf(width * (.2f + .6f * i  / (sinks - 1.f))) + .5f;
			const float si_x0 = rintf(width * (.2f + .6f * i / (sources - 1.f))) + .5f;
			cairo_move_to (cr, si_x, height);
			cairo_curve_to (cr, si_x, 0, si_x0, height, si_x0, 0);
			cairo_stroke (cr);
		}
	} else if (midi_sources == 1 && midi_sinks == 1 && sinks == 1 && sources == 1) {
		const float si_x = rintf(width * .5f) + .5f;
		cairo_move_to (cr, si_x, height);
		cairo_line_to (cr, si_x, 0);
		cairo_stroke (cr);
	} else if (midi_sources == 1 && midi_sinks == 1) {
		/* unusual cases -- removed synth, midi-track w/audio plugins */
		const float si_x  = rintf(width * (sinks   > 1 ? .2f : .5f)) + .5f;
		const float si_x0 = rintf(width * (sources > 1 ? .2f : .5f)) + .5f;
		cairo_move_to (cr, si_x, height);
		cairo_curve_to (cr, si_x, 0, si_x0, height, si_x0, 0);
		cairo_stroke (cr);
	}

	/* AUDIO */
	const uint32_t audio_sources = _sources.n_audio();
	const uint32_t audio_sinks = _sinks.n_audio();
	cairo_set_source_rgb (cr,
			UINT_RGBA_R_FLT(audio_port_color),
			UINT_RGBA_G_FLT(audio_port_color),
			UINT_RGBA_B_FLT(audio_port_color));

	if (_splitting) {
		assert(audio_sources < 2);
		assert(audio_sinks > 1);
		/* assume there is only ever one MIDI port */
		const float si_x0 = rintf(width * (midi_sources > 0 ? .8f : .5f)) + .5f;
		for (uint32_t i = midi_sinks; i < sinks; ++i) {
			const float si_x = rintf(width * (.2f + .6f * i / (sinks - 1.f))) + .5f;
			cairo_move_to (cr, si_x, height);
			cairo_curve_to (cr, si_x, 0, si_x0, height, si_x0, 0);
			cairo_stroke (cr);
		}
	} else if (audio_sources > 1) {
		for (uint32_t i = 0 ; i < audio_sources; ++i) {
			const float si_x = rintf(width * (.2f + .6f * (i + midi_sinks) / (sinks - 1.f))) + .5f;
			const float si_x0 = rintf(width * (.2f + .6f * (i + midi_sources) / (sources - 1.f))) + .5f;
			cairo_move_to (cr, si_x, height);
			cairo_curve_to (cr, si_x, 0, si_x0, height, si_x0, 0);
			cairo_stroke (cr);
		}
	} else if (audio_sources == 1 && audio_sinks == 1) {
		const float si_x = rintf(width * .5f) + .5f;
		cairo_move_to (cr, si_x, height);
		cairo_line_to (cr, si_x, 0);
		cairo_stroke (cr);
	}
	cairo_destroy(cr);
	return true;
}

ProcessorBox::ProcessorBox (ARDOUR::Session* sess, boost::function<PluginSelector*()> get_plugin_selector,
			    RouteProcessorSelection& rsel, MixerStrip* parent, bool owner_is_mixer)
	: _parent_strip (parent)
	, _owner_is_mixer (owner_is_mixer)
	, ab_direction (true)
	, _get_plugin_selector (get_plugin_selector)
	, _placement (-1)
	, _visible_prefader_processors (0)
	, _rr_selection(rsel)
	, _redisplay_pending (false)

{
	set_session (sess);

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

	processor_scroller.show ();
	processor_display.show ();

	if (parent) {
		parent->DeliveryChanged.connect (
			_mixer_strip_connections, invalidator (*this), boost::bind (&ProcessorBox::mixer_strip_delivery_changed, this, _1), gui_context ()
			);
	}

	ARDOUR_UI::instance()->set_tip (processor_display, _("Right-click to add/remove/edit\nplugins,inserts,sends and more"));
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
	/* don't keep updating display as processors are deleted */
	no_processor_redisplay = true;
	_route.reset ();
}

void
ProcessorBox::object_drop(DnDVBox<ProcessorEntry>* source, ProcessorEntry* position, Glib::RefPtr<Gdk::DragContext> const & context)
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

	list<ProcessorEntry*> children = source->selection ();
	list<boost::shared_ptr<Processor> > procs;
	for (list<ProcessorEntry*>::const_iterator i = children.begin(); i != children.end(); ++i) {
		if ((*i)->processor ()) {
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

	using namespace Menu_Helpers;
	Menu* menu = manage (new Menu);
	MenuList& items = menu->items();

	for (RouteList::iterator r = rl->begin(); r != rl->end(); ++r) {
		if (!_route->internal_send_for (*r) && *r != _route) {
			items.push_back (MenuElem ((*r)->name(), sigc::bind (sigc::ptr_fun (ProcessorBox::rb_choose_aux), boost::weak_ptr<Route>(*r))));
		}
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
			/* stupid gtkmm: we need to pass a null reference here */
			gtk_menu_item_set_submenu (aux_menu_item->gobj(), 0);
			aux_menu_item->set_sensitive (false);
		}
	}

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
				gtk_menu_item_set_submenu (controls_menu_item->gobj(), 0);
				controls_menu_item->set_sensitive (false);
			}
		} else {
			controls_menu_item->set_sensitive (false);
		}
	}

	Gtk::MenuItem* send_menu_item = dynamic_cast<Gtk::MenuItem*>(ActionManager::get_widget("/ProcessorMenu/send_options"));
	if (send_menu_item) {
		if (single_selection) {
			Menu* m = single_selection->build_send_options_menu ();
			if (m && !m->items().empty()) {
				send_menu_item->set_submenu (*m);
				send_menu_item->set_sensitive (true);
			} else {
				gtk_menu_item_set_submenu (send_menu_item->gobj(), 0);
				send_menu_item->set_sensitive (false);
			}
		} else {
			send_menu_item->set_sensitive (false);
		}
	}

	/* Sensitise actions as approprioate */

        cut_action->set_sensitive (can_cut());
	paste_action->set_sensitive (!_rr_selection.processors.empty());

	const bool sensitive = !processor_display.selection().empty();
	ActionManager::set_sensitive (ActionManager::plugin_selection_sensitive_actions, sensitive);
	edit_action->set_sensitive (one_processor_can_be_edited ());
	edit_generic_action->set_sensitive (one_processor_can_be_edited ());

	boost::shared_ptr<PluginInsert> pi;
	if (single_selection) {
		pi = boost::dynamic_pointer_cast<PluginInsert> (single_selection->processor ());
	}

	/* allow editing with an Ardour-generated UI for plugin inserts with editors */
	edit_action->set_sensitive (pi && pi->plugin()->has_editor ());

	/* disallow rename for multiple selections, for plugin inserts and for the fader */
	rename_action->set_sensitive (single_selection && !pi && !boost::dynamic_pointer_cast<Amp> (single_selection->processor ()));

	processor_menu->popup (1, arg);

	/* Add a placeholder gap to the processor list to indicate where a processor would be
	   inserted were one chosen from the menu.
	*/
	int x, y;
	processor_display.get_pointer (x, y);
	_placement = processor_display.add_placeholder (y);

	if (_visible_prefader_processors == 0 && _placement > 0) {
		--_placement;
	}
}

bool
ProcessorBox::enter_notify (GdkEventCrossing*)
{
	_current_processor_box = this;
	return false;
}

bool
ProcessorBox::leave_notify (GdkEventCrossing*)
{
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

	if ( (op == ProcessorsDelete) && targets.empty() )
		return false;  //nothing to delete.  return false so the editor-mixer, because the user was probably intending to delete something in the editor
	
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
			if ((*i)->active()) {
				(*i)->deactivate ();
			} else {
				(*i)->activate ();
			}
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

	if (processor && (Keyboard::is_edit_event (ev) || (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS))) {

		if (_session->engine().connected()) {
			/* XXX giving an error message here is hard, because we may be in the midst of a button press */

			if (!one_processor_can_be_edited ()) return true;

			if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {
				generic_edit_processor (processor);
			} else {
				edit_processor (processor);
			}
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
#ifndef GTKOSX
		   && (Keyboard::no_modifier_keys_pressed (ev) && ((ev->state & Gdk::BUTTON2_MASK) == Gdk::BUTTON2_MASK))
#endif
		) {

		/* button2-click with no/appropriate modifiers */

		if (processor->active()) {
			processor->deactivate ();
		} else {
			processor->activate ();
		}
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
		} else {

			if (Profile->get_sae()) {
				processor->activate ();
			}
		}
	}

	return false;
}

void
ProcessorBox::weird_plugin_dialog (Plugin& p, Route::ProcessorStreams streams)
{
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
	boost::shared_ptr<Pannable> sendpan(new Pannable (*_session));
	boost::shared_ptr<Send> send (new Send (*_session, sendpan, _route->mute_master()));

	/* make an educated guess at the initial number of outputs for the send */
	ChanCount outs = (_session->master_out())
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
		if (Profile->get_sae()) {
			processor->activate ();
		}
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
		if (Profile->get_sae()) {
			processor->activate ();
		}
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

	_session->add_internal_send (target, _placement, _route);
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
	bool     fader_seen;

	if (no_processor_redisplay) {
		return;
	}

	processor_display.clear ();

	_visible_prefader_processors = 0;
	fader_seen = false;

	_route->foreach_processor (sigc::bind (sigc::mem_fun (*this, &ProcessorBox::help_count_visible_prefader_processors), 
					       &_visible_prefader_processors, &fader_seen));

	_route->foreach_processor (sigc::mem_fun (*this, &ProcessorBox::add_processor_to_display));
	_route->foreach_processor (sigc::mem_fun (*this, &ProcessorBox::maybe_add_processor_to_ui_list));
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

	/* not on the list; add it */

	string loc;
#if 0 // is this still needed? Why?
	if (_parent_strip) {
		if (_parent_strip->mixer_owned()) {
			loc = X_("M");
		} else {
			loc = X_("R");
		}
	} else {
		loc = X_("P");
	}
#else
	loc = X_("P");
#endif

	ProcessorWindowProxy* wp = new ProcessorWindowProxy (
		string_compose ("%1-%2-%3", loc, _route->id(), p->id()),
		this,
		w);

	const XMLNode* ui_xml = _session->extra_xml (X_("UI"));

	if (ui_xml) {
		wp->set_state (*ui_xml);
	}
	
        void* existing_ui = p->get_ui ();

        if (existing_ui) {
                wp->use_window (*(reinterpret_cast<Gtk::Window*>(existing_ui)));
        }

	p->set_window_proxy (wp);
	WM::Manager::instance().register_window (wp);
}

void
ProcessorBox::help_count_visible_prefader_processors (boost::weak_ptr<Processor> p, uint32_t* cnt, bool* amp_seen)
{
	boost::shared_ptr<Processor> processor (p.lock ());

	if (processor && processor->display_to_user()) {

		if (boost::dynamic_pointer_cast<Amp>(processor)) {
			*amp_seen = true;
		} else {
			if (!*amp_seen) {
				(*cnt)++;
			}
		}
	}
}

void
ProcessorBox::add_processor_to_display (boost::weak_ptr<Processor> p)
{
	boost::shared_ptr<Processor> processor (p.lock ());

	if (!processor || !processor->display_to_user()) {
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
	
	//faders and meters are not deletable, copy/paste-able, so they shouldn't be selectable
	if (!send && !plugin_insert && !ext)
		e->set_selectable(false);

	bool mark_send_visible = false;
	if (send && _parent_strip) {
		/* show controls of new sends by default */
		GUIObjectState& st = _parent_strip->gui_object_state ();
		XMLNode* strip = st.get_or_add_node (_parent_strip->state_id ());
		assert (strip);
		/* check if state exists, if not it must be a new send */
		if (!st.get_node(strip, e->state_id())) {
			mark_send_visible = true;
		}
	}

	/* Set up this entry's state from the GUIObjectState */
	XMLNode* proc = entry_gui_object_state (e);
	if (proc) {
		e->set_control_state (proc);
	}

	if (mark_send_visible) {
		e->show_all_controls ();
	}

	processor_display.add_child (e);
}

void
ProcessorBox::reordered ()
{
	compute_processor_sort_keys ();
	setup_entry_positions ();
}

void
ProcessorBox::setup_entry_positions ()
{
	list<ProcessorEntry*> children = processor_display.children ();
	bool pre_fader = true;

	uint32_t num = 0;
	for (list<ProcessorEntry*>::iterator i = children.begin(); i != children.end(); ++i) {
		if (boost::dynamic_pointer_cast<Amp>((*i)->processor())) {
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

	_rr_selection.set (node);

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

	_rr_selection.set (node);
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
	ArdourPrompter name_prompter (true);
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
		if (result.length()) {

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
	if (_rr_selection.processors.empty()) {
		return;
	}

	paste_processor_state (_rr_selection.processors.get_node().children(), boost::shared_ptr<Processor>());
}

void
ProcessorBox::paste_processors (boost::shared_ptr<Processor> before)
{

	if (_rr_selection.processors.empty()) {
		return;
	}

	paste_processor_state (_rr_selection.processors.get_node().children(), before);
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
                                p->set_state (**niter, Stateful::current_state_version);
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
	const list<ProcessorEntry*> selection = processor_display.selection ();
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
ProcessorBox::clear_processors ()
{
	string prompt;
	vector<string> choices;

	prompt = string_compose (_("Do you really want to remove all processors from %1?\n"
				   "(this cannot be undone)"), _route->name());

	choices.push_back (_("Cancel"));
	choices.push_back (_("Yes, remove them all"));

	Gtkmm2ext::Choice prompter (_("Remove processors"), prompt, choices);

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

	Gtkmm2ext::Choice prompter (_("Remove processors"), prompt, choices);

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

	if (boost::dynamic_pointer_cast<Amp> (processor)) {

		if (_parent_strip) {
			_parent_strip->revert_to_default_display ();
		}

	} else if ((send = boost::dynamic_pointer_cast<Send> (processor)) != 0) {

		if (!_session->engine().connected()) {
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

		if (!_session->engine().connected()) {
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

		if (!_session->engine().connected()) {
			MessageDialog msg ( _("Not connected to audio engine - no I/O changes are possible"));
			msg.run ();
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
	Glib::RefPtr<Gtk::ActionGroup> popup_act_grp = Gtk::ActionGroup::create(X_("ProcessorMenu"));
	Glib::RefPtr<Action> act;

	/* new stuff */
	ActionManager::register_action (popup_act_grp, X_("newplugin"), _("New Plugin"),
			sigc::ptr_fun (ProcessorBox::rb_choose_plugin));

	act = ActionManager::register_action (popup_act_grp, X_("newinsert"), _("New Insert"),
			sigc::ptr_fun (ProcessorBox::rb_choose_insert));
	ActionManager::engine_sensitive_actions.push_back (act);
	act = ActionManager::register_action (popup_act_grp, X_("newsend"), _("New External Send ..."),
			sigc::ptr_fun (ProcessorBox::rb_choose_send));
	ActionManager::engine_sensitive_actions.push_back (act);

	ActionManager::register_action (popup_act_grp, X_("newaux"), _("New Aux Send ..."));

	ActionManager::register_action (popup_act_grp, X_("controls"), _("Controls"));
	ActionManager::register_action (popup_act_grp, X_("send_options"), _("Send Options"));

	ActionManager::register_action (popup_act_grp, X_("clear"), _("Clear (all)"),
			sigc::ptr_fun (ProcessorBox::rb_clear));
	ActionManager::register_action (popup_act_grp, X_("clear_pre"), _("Clear (pre-fader)"),
			sigc::ptr_fun (ProcessorBox::rb_clear_pre));
	ActionManager::register_action (popup_act_grp, X_("clear_post"), _("Clear (post-fader)"),
			sigc::ptr_fun (ProcessorBox::rb_clear_post));

	/* standard editing stuff */
	cut_action = ActionManager::register_action (popup_act_grp, X_("cut"), _("Cut"),
                                                     sigc::ptr_fun (ProcessorBox::rb_cut));
	ActionManager::plugin_selection_sensitive_actions.push_back(cut_action);
	act = ActionManager::register_action (popup_act_grp, X_("copy"), _("Copy"),
			sigc::ptr_fun (ProcessorBox::rb_copy));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);

	act = ActionManager::register_action (popup_act_grp, X_("delete"), _("Delete"),
			sigc::ptr_fun (ProcessorBox::rb_delete));
	ActionManager::plugin_selection_sensitive_actions.push_back(act); // ??

	paste_action = ActionManager::register_action (popup_act_grp, X_("paste"), _("Paste"),
			sigc::ptr_fun (ProcessorBox::rb_paste));
	rename_action = ActionManager::register_action (popup_act_grp, X_("rename"), _("Rename"),
			sigc::ptr_fun (ProcessorBox::rb_rename));
	ActionManager::register_action (popup_act_grp, X_("selectall"), _("Select All"),
			sigc::ptr_fun (ProcessorBox::rb_select_all));
	ActionManager::register_action (popup_act_grp, X_("deselectall"), _("Deselect All"),
			sigc::ptr_fun (ProcessorBox::rb_deselect_all));

	/* activation etc. */

	ActionManager::register_action (popup_act_grp, X_("activate_all"), _("Activate All"),
			sigc::ptr_fun (ProcessorBox::rb_activate_all));
	ActionManager::register_action (popup_act_grp, X_("deactivate_all"), _("Deactivate All"),
			sigc::ptr_fun (ProcessorBox::rb_deactivate_all));
	ActionManager::register_action (popup_act_grp, X_("ab_plugins"), _("A/B Plugins"),
			sigc::ptr_fun (ProcessorBox::rb_ab_plugins));

	/* show editors */
	edit_action = ActionManager::register_action (
		popup_act_grp, X_("edit"), _("Edit..."),
		sigc::ptr_fun (ProcessorBox::rb_edit));

	edit_generic_action = ActionManager::register_action (
		popup_act_grp, X_("edit-generic"), _("Edit with generic controls..."),
		sigc::ptr_fun (ProcessorBox::rb_edit_generic));

	ActionManager::add_action_group (popup_act_grp);
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

	ProcessorWindowProxy* proxy = find_window_proxy (processor);

	if (proxy) {
		proxy->set_custom_ui_mode (true);
		proxy->toggle ();
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

	ProcessorWindowProxy* proxy = find_window_proxy (processor);

	if (proxy) {
		proxy->set_custom_ui_mode (false);
		proxy->toggle ();
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
	p->set_ui (w);
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
	, is_custom (false)
	, want_custom (false)
{
	boost::shared_ptr<Processor> p = _processor.lock ();
	if (!p) {
		return;
	}
	p->DropReferences.connect (going_away_connection, MISSING_INVALIDATOR, boost::bind (&ProcessorWindowProxy::processor_going_away, this), gui_context());
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
	delete _window;
	_window = 0;
	WM::Manager::instance().remove (this);
	/* should be no real reason to do this, since the object that would
	   send DropReferences is about to be deleted, but lets do it anyway.
	*/
	going_away_connection.disconnect();
}

ARDOUR::SessionHandlePtr*
ProcessorWindowProxy::session_handle() 
{
	/* we don't care */
	return 0;
}

XMLNode&
ProcessorWindowProxy::get_state () const
{
	XMLNode *node;
	node = &ProxyBase::get_state();
	node->add_property (X_("custom-ui"), is_custom? X_("yes") : X_("no"));
	return *node;
}

void
ProcessorWindowProxy::set_state (const XMLNode& node)
{
	XMLNodeList children = node.children ();
	XMLNodeList::const_iterator i = children.begin ();
	while (i != children.end()) {
		XMLProperty* prop = (*i)->property (X_("name"));
		if ((*i)->name() == X_("Window") && prop && prop->value() == _name) {
			break;
		}
		++i;
	}

	if (i != children.end()) {
		XMLProperty* prop;
		if ((prop = (*i)->property (X_("custom-ui"))) != 0) {
			want_custom = PBD::string_is_affirmative (prop->value ());
		}
	}

	ProxyBase::set_state(node);
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
		}
	}

	return _window;
}

void
ProcessorWindowProxy::toggle ()
{
	if (_window && (is_custom != want_custom)) {
		/* drop existing window - wrong type */
		drop_window ();
	}
	is_custom = want_custom;

	WM::ProxyBase::toggle ();
}
