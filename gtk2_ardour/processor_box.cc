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

#include "ardour/amp.h"
#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/internal_return.h"
#include "ardour/internal_send.h"
#include "ardour/plugin_insert.h"
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
#include "utils.h"

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

ProcessorEntry::ProcessorEntry (ProcessorBox* parent, boost::shared_ptr<Processor> p, Width w)
	: _button (ArdourButton::led_default_elements)
	, _position (PreFader)
	, _parent (parent)
	, _processor (p)
	, _width (w)
	, _visual_state (Gtk::STATE_NORMAL)
{
	_vbox.show ();
	
	_button.set_diameter (3);
	_button.set_distinct_led_click (true);
	_button.set_led_left (true);
	_button.signal_led_clicked.connect (sigc::mem_fun (*this, &ProcessorEntry::led_clicked));
	_button.set_text (name (_width));

	if (_processor) {

		_vbox.pack_start (_button, true, true);

		_button.set_active (_processor->active());
		_button.show ();
		
		_processor->ActiveChanged.connect (active_connection, invalidator (*this), boost::bind (&ProcessorEntry::processor_active_changed, this), gui_context());
		_processor->PropertyChanged.connect (name_connection, invalidator (*this), boost::bind (&ProcessorEntry::processor_property_changed, this, _1), gui_context());

		set<Evoral::Parameter> p = _processor->what_can_be_automated ();
		for (set<Evoral::Parameter>::iterator i = p.begin(); i != p.end(); ++i) {
			
			Control* c = new Control (_processor->automation_control (*i), _processor->describe_parameter (*i));
			
			_controls.push_back (c);

			if (boost::dynamic_pointer_cast<Amp> (_processor) == 0) {
				/* Add non-Amp controls to the processor box */
				_vbox.pack_start (c->box);
			}

			if (boost::dynamic_pointer_cast<Send> (_processor)) {
				/* Don't label send faders */
				c->hide_label ();
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
ProcessorEntry::set_position (Position p)
{
	_position = p;
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
ProcessorEntry::setup_tooltip ()
{
	ARDOUR_UI::instance()->set_tip (_button, name (Wide));
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
		CheckMenuItem* c = dynamic_cast<CheckMenuItem*> (&items.back ());
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

ProcessorEntry::Control::Control (boost::shared_ptr<AutomationControl> c, string const & n)
	: _control (c)
	, _adjustment (gain_to_slider_position_with_max (1.0, Config->get_max_gain()), 0, 1, 0.01, 0.1)
	, _slider (&_adjustment, 0, 13, false)
	, _slider_persistant_tooltip (&_slider)
	, _button (ArdourButton::Element (ArdourButton::Text | ArdourButton::Indicator))
	, _ignore_ui_adjustment (false)
	, _visible (false)
	, _name (n)
{
	_slider.set_controllable (c);

	if (c->toggled()) {
		_button.set_text (_name);
		_button.set_led_left (true);
		_button.set_name ("processor control button");
		box.pack_start (_button);
		_button.show ();

		_button.signal_clicked.connect (sigc::mem_fun (*this, &Control::button_clicked));
		_button.signal_led_clicked.connect (sigc::mem_fun (*this, &Control::button_clicked));
		c->Changed.connect (_connection, MISSING_INVALIDATOR, boost::bind (&Control::control_changed, this), gui_context ());

	} else {
		
		_slider.set_name ("PluginSlider");
		_slider.set_text (_name);

		box.pack_start (_slider);
		_slider.show ();

		double const lo = c->internal_to_interface (c->lower ());
		double const up = c->internal_to_interface (c->upper ());
		
		_adjustment.set_lower (lo);
		_adjustment.set_upper (up);
		_adjustment.set_step_increment ((up - lo) / 100);
		_adjustment.set_page_increment ((up - lo) / 10);
		_slider.set_default_value (c->internal_to_interface (c->normal ()));
		
		_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &Control::slider_adjusted));
		c->Changed.connect (_connection, MISSING_INVALIDATOR, boost::bind (&Control::control_changed, this), gui_context ());
	}

	ARDOUR_UI::RapidScreenUpdate.connect (sigc::mem_fun (*this, &Control::control_changed));
	
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
	
	ARDOUR_UI::instance()->set_tip (_label, sm);
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

	c->set_value (c->interface_to_internal (_adjustment.get_value ()));
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

		_adjustment.set_value (c->internal_to_interface (c->get_value ()));
		
		stringstream s;
		s.precision (1);
		s.setf (ios::fixed, ios::floatfield);
		s << c->internal_to_user (c->get_value ());
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

void
ProcessorEntry::Control::hide_label ()
{
	_label.hide ();
}

string
ProcessorEntry::Control::state_id () const
{
	boost::shared_ptr<AutomationControl> c = _control.lock ();
	assert (c);

	return string_compose (X_("control %1"), c->id().to_s ());
}

BlankProcessorEntry::BlankProcessorEntry (ProcessorBox* b, Width w)
	: ProcessorEntry (b, boost::shared_ptr<Processor>(), w)
{
}

PluginInsertProcessorEntry::PluginInsertProcessorEntry (ProcessorBox* b, boost::shared_ptr<ARDOUR::PluginInsert> p, Width w)
	: ProcessorEntry (b, p, w)
	, _plugin_insert (p)
{
	p->SplittingChanged.connect (
		_splitting_connection, invalidator (*this), boost::bind (&PluginInsertProcessorEntry::plugin_insert_splitting_changed, this), gui_context()
		);

	_splitting_icon.set_size_request (-1, 12);

	_vbox.pack_start (_splitting_icon);
	_vbox.reorder_child (_splitting_icon, 0);

	plugin_insert_splitting_changed ();
}

void
PluginInsertProcessorEntry::plugin_insert_splitting_changed ()
{
	if (_plugin_insert->splitting ()) {
		_splitting_icon.show ();
	} else {
		_splitting_icon.hide ();
	}
}

void
PluginInsertProcessorEntry::hide_things ()
{
	ProcessorEntry::hide_things ();
	plugin_insert_splitting_changed ();
}

void
PluginInsertProcessorEntry::setup_visuals ()
{
	switch (_position) {
	case PreFader:
		_splitting_icon.set_name ("ProcessorPreFader");
		break;

	case Fader:
		_splitting_icon.set_name ("ProcessorFader");
		break;

	case PostFader:
		_splitting_icon.set_name ("ProcessorPostFader");
		break;
	}

	ProcessorEntry::setup_visuals ();
}

bool
PluginInsertProcessorEntry::SplittingIcon::on_expose_event (GdkEventExpose* ev)
{
	cairo_t* cr = gdk_cairo_create (get_window()->gobj());

	cairo_set_line_width (cr, 1);

	double const width = ev->area.width;
	double const height = ev->area.height;

	Gdk::Color const bg = get_style()->get_bg (STATE_NORMAL);
	cairo_set_source_rgb (cr, bg.get_red_p (), bg.get_green_p (), bg.get_blue_p ());

	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	Gdk::Color const fg = get_style()->get_fg (STATE_NORMAL);
	cairo_set_source_rgb (cr, fg.get_red_p (), fg.get_green_p (), fg.get_blue_p ());

	cairo_move_to (cr, width * 0.3, height);
	cairo_line_to (cr, width * 0.3, height * 0.5);
	cairo_line_to (cr, width * 0.7, height * 0.5);
	cairo_line_to (cr, width * 0.7, height);
	cairo_move_to (cr, width * 0.5, height * 0.5);
	cairo_line_to (cr, width * 0.5, 0);
	cairo_stroke (cr);

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
	processor_display.set_spacing (2);

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
			while (dynamic_cast<BlankProcessorEntry*> (*i)) {
				assert (i != c.end ());
				++i;
			}

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
		}
	}

	/* Sensitise actions as approprioate */

        cut_action->set_sensitive (can_cut());
	paste_action->set_sensitive (!_rr_selection.processors.empty());

	const bool sensitive = !processor_display.selection().empty();
	ActionManager::set_sensitive (ActionManager::plugin_selection_sensitive_actions, sensitive);
	edit_action->set_sensitive (one_processor_can_be_edited ());

	boost::shared_ptr<PluginInsert> pi;
	if (single_selection) {
		pi = boost::dynamic_pointer_cast<PluginInsert> (single_selection->processor ());
	}

	/* allow editing with an Ardour-generated UI for plugin inserts with editors */
	edit_generic_action->set_sensitive (pi && pi->plugin()->has_editor ());

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

void
ProcessorBox::processor_operation (ProcessorOperation op) 
{
	ProcSelection targets;

	get_selected_processors (targets);

	if (targets.empty()) {

		int x, y;
		processor_display.get_pointer (x, y);

		pair<ProcessorEntry *, double> const pointer = processor_display.get_child_at_position (y);

		if (pointer.first && pointer.first->processor()) {
			targets.push_back (pointer.first->processor ());
		}
	}

	switch (op) {
	case ProcessorsSelectAll:
		processor_display.select_all ();
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
			if (Config->get_use_plugin_own_gui ()) {
				toggle_edit_processor (processor);
			} else {
				toggle_edit_generic_processor (processor);
			}
		}
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

	} else if (Keyboard::is_context_menu_event (ev)) {

		show_processor_menu (ev->time);

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
	dialog.set_position (Gtk::WIN_POS_MOUSE);
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
	boost::shared_ptr<Send> send (new Send (*_session, _route->pannable(), _route->mute_master()));

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

	if (_visible_prefader_processors == 0) { // fader only
		BlankProcessorEntry* bpe = new BlankProcessorEntry (this, _width);
		processor_display.add_child (bpe);
	}

	_route->foreach_processor (sigc::mem_fun (*this, &ProcessorBox::add_processor_to_display));

	for (list<ProcessorWindowProxy*>::iterator i = _processor_window_proxies.begin(); i != _processor_window_proxies.end(); ++i) {
		(*i)->marked = false;
	}

	_route->foreach_processor (sigc::mem_fun (*this, &ProcessorBox::maybe_add_processor_to_ui_list));

	/* trim dead wood from the processor window proxy list */

	list<ProcessorWindowProxy*>::iterator i = _processor_window_proxies.begin();
	while (i != _processor_window_proxies.end()) {
		list<ProcessorWindowProxy*>::iterator j = i;
		++j;

		if (!(*i)->marked) {
			ARDOUR_UI::instance()->remove_window_proxy (*i);
			delete *i;
			_processor_window_proxies.erase (i);
		}

		i = j;
	}

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

	list<ProcessorWindowProxy*>::iterator i = _processor_window_proxies.begin ();
	while (i != _processor_window_proxies.end()) {

		boost::shared_ptr<Processor> t = (*i)->processor().lock ();

		if (p == t) {
			/* this processor is already on the list; done */
			(*i)->marked = true;
			return;
		}

		++i;
	}

	/* not on the list; add it */

	string loc;
	if (_parent_strip) {
		if (_parent_strip->mixer_owned()) {
			loc = X_("M");
		} else {
			loc = X_("R");
		}
	} else {
		loc = X_("P");
	}

	ProcessorWindowProxy* wp = new ProcessorWindowProxy (
		string_compose ("%1-%2-%3", loc, _route->id(), p->id()),
		_session->extra_xml (X_("UI")),
		this,
		w);

	wp->marked = true;

        /* if the processor already has an existing UI,
           note that so that we don't recreate it
        */

        void* existing_ui = p->get_ui ();

        if (existing_ui) {
                wp->set (static_cast<Gtk::Window*>(existing_ui));
        }

	_processor_window_proxies.push_back (wp);
	ARDOUR_UI::instance()->add_window_proxy (wp);
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

	/* Set up this entry's state from the GUIObjectState */
	XMLNode* proc = entry_gui_object_state (e);
	if (proc) {
		e->set_control_state (proc);
	}
	
	if (boost::dynamic_pointer_cast<Send> (processor)) {
		/* Always show send controls */
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

	for (list<ProcessorEntry*>::iterator i = children.begin(); i != children.end(); ++i) {
		if (boost::dynamic_pointer_cast<Amp>((*i)->processor())) {
			pre_fader = false;
			(*i)->set_position (ProcessorEntry::Fader);
		} else {
			if (pre_fader) {
				(*i)->set_position (ProcessorEntry::PreFader);
			} else {
				(*i)->set_position (ProcessorEntry::PostFader);
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
	dialog.set_position (Gtk::WIN_POS_MOUSE);
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

				XMLNode n (**niter);
                                InternalSend* s = new InternalSend (*_session, _route->pannable(), _route->mute_master(),
								    boost::shared_ptr<Route>(), Delivery::Aux); 

				IOProcessor::prepare_for_reset (n, s->name());

                                if (s->set_state (n, Stateful::loading_state_version)) {
                                        delete s;
                                        return;
                                }

				p.reset (s);

			} else if (type->value() == "send") {

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
		(boost::dynamic_pointer_cast<Send> (processor) && !boost::dynamic_pointer_cast<InternalSend> (processor))||
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

void
ProcessorBox::toggle_edit_processor (boost::shared_ptr<Processor> processor)
{
	boost::shared_ptr<Send> send;
	boost::shared_ptr<InternalSend> internal_send;
	boost::shared_ptr<Return> retrn;
	boost::shared_ptr<PluginInsert> plugin_insert;
	boost::shared_ptr<PortInsert> port_insert;
	Window* gidget = 0;

	if (boost::dynamic_pointer_cast<AudioTrack>(_route) != 0) {

		if (boost::dynamic_pointer_cast<AudioTrack> (_route)->freeze_state() == AudioTrack::Frozen) {
			return;
		}
	}

	if (boost::dynamic_pointer_cast<Amp> (processor)) {

		if (_parent_strip) {
			_parent_strip->revert_to_default_display ();
		}

	} else if ((send = boost::dynamic_pointer_cast<Send> (processor)) != 0) {

		if (!_session->engine().connected()) {
			return;
		}

		if (boost::dynamic_pointer_cast<InternalSend> (processor) == 0) {
			SendUIWindow* w = new SendUIWindow (send, _session);
			w->show ();
		} else {
			/* assign internal send to main fader */
			if (_parent_strip) {
				if (_parent_strip->current_delivery() == send) {
					_parent_strip->revert_to_default_display ();
				} else {
					_parent_strip->show_send(send);
				}
			} 
		}

	} else if ((retrn = boost::dynamic_pointer_cast<Return> (processor)) != 0) {

		if (boost::dynamic_pointer_cast<InternalReturn> (retrn)) {
			/* no GUI for these */
			return;
		}

		if (!_session->engine().connected()) {
			return;
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

			plugin_ui = new PluginUIWindow (plugin_insert);
			plugin_ui->set_title (generate_processor_title (plugin_insert));
			set_processor_ui (plugin_insert, plugin_ui);

		} else {
			plugin_ui = dynamic_cast<PluginUIWindow *> (w);
		}

		gidget = plugin_ui;

	} else if ((port_insert = boost::dynamic_pointer_cast<PortInsert> (processor)) != 0) {

		if (!_session->engine().connected()) {
			MessageDialog msg ( _("Not connected to JACK - no I/O changes are possible"));
			msg.run ();
			return;
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

	if (gidget) {
		if (gidget->is_visible()) {
			gidget->hide ();
		} else {
			gidget->show_all ();
			gidget->present ();
		}
	}
}

/** Toggle a generic (Ardour-generated) plugin UI */
void
ProcessorBox::toggle_edit_generic_processor (boost::shared_ptr<Processor> processor)
{
	boost::shared_ptr<PluginInsert> plugin_insert
		= boost::dynamic_pointer_cast<PluginInsert>(processor);
	if (!plugin_insert) {
		return;
	}

	PluginUIWindow* plugin_ui = new PluginUIWindow (plugin_insert, true, false);
	plugin_ui->set_title(generate_processor_title (plugin_insert));

	if (plugin_ui->is_visible()) {
		plugin_ui->hide();
	} else {
		plugin_ui->show_all();
		plugin_ui->present();
	}
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
	ActionManager::jack_sensitive_actions.push_back (act);
	act = ActionManager::register_action (popup_act_grp, X_("newsend"), _("New External Send ..."),
			sigc::ptr_fun (ProcessorBox::rb_choose_send));
	ActionManager::jack_sensitive_actions.push_back (act);

	ActionManager::register_action (popup_act_grp, X_("newaux"), _("New Aux Send ..."));

	ActionManager::register_action (popup_act_grp, X_("controls"), _("Controls"));

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
		popup_act_grp, X_("edit-generic"), _("Edit with basic controls..."),
		sigc::ptr_fun (ProcessorBox::rb_edit_generic));

	ActionManager::add_action_group (popup_act_grp);
}

void
ProcessorBox::rb_edit_generic ()
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->for_selected_processors (&ProcessorBox::toggle_edit_generic_processor);
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

	_current_processor_box->for_selected_processors (&ProcessorBox::toggle_edit_processor);
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

	return string_compose(_("%1: %2 (by %3)"), _route->name(), pi->name(), maker);
}

/** @param p Processor.
 *  @return the UI window for \a p.
 */
Window *
ProcessorBox::get_processor_ui (boost::shared_ptr<Processor> p) const
{
	list<ProcessorWindowProxy*>::const_iterator i = _processor_window_proxies.begin ();
	while (i != _processor_window_proxies.end()) {
		boost::shared_ptr<Processor> t = (*i)->processor().lock ();
		if (t && t == p) {
			return (*i)->get ();
		}

		++i;
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
 	list<ProcessorWindowProxy*>::iterator i = _processor_window_proxies.begin ();

	p->set_ui (w);

	while (i != _processor_window_proxies.end()) {
		boost::shared_ptr<Processor> t = (*i)->processor().lock ();
		if (t && t == p) {
			(*i)->set (w);
			return;
		}

		++i;
	}

	/* we shouldn't get here, because the ProcessorUIList should always contain
	   an entry for each processor.
	*/
	assert (false);
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

ProcessorWindowProxy::ProcessorWindowProxy (
	string const & name,
	XMLNode const * node,
	ProcessorBox* box,
	boost::weak_ptr<Processor> processor
	)
	: WindowProxy<Gtk::Window> (name, node)
	, marked (false)
	, _processor_box (box)
	, _processor (processor)
{

}


void
ProcessorWindowProxy::show ()
{
	boost::shared_ptr<Processor> p = _processor.lock ();
	if (!p) {
		return;
	}

	_processor_box->toggle_edit_processor (p);
}

