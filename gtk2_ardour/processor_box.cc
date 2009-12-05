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
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/doi.h>

#include "ardour/amp.h"
#include "ardour/ardour.h"
#include "ardour/audio_diskstream.h"
#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/internal_send.h"
#include "ardour/ladspa_plugin.h"
#include "ardour/meter.h"
#include "ardour/plugin_insert.h"
#include "ardour/port_insert.h"
#include "ardour/profile.h"
#include "ardour/return.h"
#include "ardour/route.h"
#include "ardour/send.h"
#include "ardour/session.h"

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
#include "processor_box.h"
#include "public_editor.h"
#include "return_ui.h"
#include "route_processor_selection.h"
#include "send_ui.h"
#include "utils.h"

#include "i18n.h"

#ifdef HAVE_AUDIOUNITS
class AUPluginUI;
#endif

using namespace std;
using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;

ProcessorBox* ProcessorBox::_current_processor_box = 0;
RefPtr<Action> ProcessorBox::paste_action;
Glib::RefPtr<Gdk::Pixbuf> SendProcessorEntry::_slider;

ProcessorEntry::ProcessorEntry (boost::shared_ptr<Processor> p, Width w)
	: _processor (p)
	, _width (w)
{
	_hbox.pack_start (_active, false, false);
	_event_box.add (_name);
	_hbox.pack_start (_event_box, true, true);
	_vbox.pack_start (_hbox);

	_name.set_alignment (0, 0.5);
	_name.set_text (name ());
	_name.set_padding (2, 2);
	
	_active.set_active (_processor->active ());
	_active.signal_toggled().connect (mem_fun (*this, &ProcessorEntry::active_toggled));
	
	_processor->ActiveChanged.connect (mem_fun (*this, &ProcessorEntry::processor_active_changed));
	_processor->NameChanged.connect (mem_fun (*this, &ProcessorEntry::processor_name_changed));
}

EventBox&
ProcessorEntry::action_widget ()
{
	return _event_box;
}

Gtk::Widget&
ProcessorEntry::widget ()
{
	return _vbox;
}

string
ProcessorEntry::drag_text () const
{
	return name ();
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
ProcessorEntry::active_toggled ()
{
	if (_active.get_active ()) {
		if (!_processor->active ()) {
			_processor->activate ();
		}
	} else {
		if (_processor->active ()) {
			_processor->deactivate ();
		}
	}
}

void
ProcessorEntry::processor_active_changed ()
{
	if (_active.get_active () != _processor->active ()) {
		_active.set_active (_processor->active ());
	}
}

void
ProcessorEntry::processor_name_changed ()
{
	_name.set_text (name ());
}

string
ProcessorEntry::name () const
{
	boost::shared_ptr<Send> send;
	string name_display;
	
	if (!_processor->active()) {
		name_display = " (";
	}
	
	if ((send = boost::dynamic_pointer_cast<Send> (_processor)) != 0 &&
	    !boost::dynamic_pointer_cast<InternalSend>(_processor)) {
		
		name_display += '>';
		
		/* grab the send name out of its overall name */
		
		string::size_type lbracket, rbracket;
		lbracket = send->name().find ('[');
		rbracket = send->name().find (']');
		
		switch (_width) {
		case Wide:
			name_display += send->name().substr (lbracket+1, lbracket-rbracket-1);
			break;
		case Narrow:
			name_display += PBD::short_version (send->name().substr (lbracket+1, lbracket-rbracket-1), 4);
			break;
		}
		
	} else {
		
		switch (_width) {
		case Wide:
			name_display += _processor->display_name();
			break;
		case Narrow:
			name_display += PBD::short_version (_processor->display_name(), 5);
			break;
		}
		
	}
	
	if (!_processor->active()) {
		name_display += ')';
	}
	
	return name_display;
}

SendProcessorEntry::SendProcessorEntry (boost::shared_ptr<Send> s, Width w)
	: ProcessorEntry (s, w),
	  _send (s),
	  _adjustment (0, 0, 1, 0.01, 0.1),
	  _fader (_slider, &_adjustment, 0, false),
	  _ignore_gain_change (false)
{
	_fader.set_controllable (_send->amp()->gain_control ());
	_vbox.pack_start (_fader);

	_adjustment.signal_value_changed().connect (mem_fun (*this, &SendProcessorEntry::gain_adjusted));
	_send->amp()->gain_control()->Changed.connect (mem_fun (*this, &SendProcessorEntry::show_gain));
	show_gain ();
}

void
SendProcessorEntry::setup_slider_pix ()
{
	_slider = ::get_icon ("fader_belt_h_thin");
	assert (_slider);
}

void
SendProcessorEntry::show_gain ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &SendProcessorEntry::show_gain));
	
	float const value = gain_to_slider_position (_send->amp()->gain ());

	if (_adjustment.get_value() != value) {
		_ignore_gain_change = true;
		_adjustment.set_value (value);
		_ignore_gain_change = false;
	}
}

void
SendProcessorEntry::gain_adjusted ()
{
	if (_ignore_gain_change) {
		return;
	}

	_send->amp()->set_gain (slider_position_to_gain (_adjustment.get_value()), this);
}

void
SendProcessorEntry::set_pixel_width (int p)
{
	_fader.set_fader_length (p);
}

ProcessorBox::ProcessorBox (ARDOUR::Session& sess, sigc::slot<PluginSelector*> get_plugin_selector,
			RouteRedirectSelection& rsel, MixerStrip* parent, bool owner_is_mixer)
	: _session(sess)
	, _parent_strip (parent)
	, _owner_is_mixer (owner_is_mixer)
	, ab_direction (true)
	, _get_plugin_selector (get_plugin_selector)
	, _placement(PreFader)
	, _rr_selection(rsel)
{
	_width = Wide;
	processor_menu = 0;
	send_action_menu = 0;
	no_processor_redisplay = false;

	processor_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	processor_scroller.add (processor_display);
	pack_start (processor_scroller, true, true);

	processor_display.set_flags (CAN_FOCUS);
	processor_display.set_name ("ProcessorSelector");
	processor_display.set_size_request (48, -1);
	processor_display.set_data ("processorbox", this);

	processor_display.signal_enter_notify_event().connect (mem_fun(*this, &ProcessorBox::enter_notify), false);
	processor_display.signal_leave_notify_event().connect (mem_fun(*this, &ProcessorBox::leave_notify), false);

	processor_display.signal_key_press_event().connect (mem_fun(*this, &ProcessorBox::processor_key_press_event));
	processor_display.signal_key_release_event().connect (mem_fun(*this, &ProcessorBox::processor_key_release_event));

	processor_display.ButtonPress.connect (mem_fun (*this, &ProcessorBox::processor_button_press_event));
	processor_display.ButtonRelease.connect (mem_fun (*this, &ProcessorBox::processor_button_release_event));

	processor_display.Reordered.connect (mem_fun (*this, &ProcessorBox::reordered));
	processor_display.DropFromAnotherBox.connect (mem_fun (*this, &ProcessorBox::object_drop));
	processor_display.SelectionChanged.connect (mem_fun (*this, &ProcessorBox::selection_changed));
}

ProcessorBox::~ProcessorBox ()
{
}

void
ProcessorBox::set_route (boost::shared_ptr<Route> r)
{
	if (_route == r) {
		return;
	}
	
	connections.clear ();

	/* new route: any existing block on processor redisplay must be meaningless */
	no_processor_redisplay = false;
	_route = r;

	connections.push_back (_route->processors_changed.connect (mem_fun (*this, &ProcessorBox::route_processors_changed)));
	connections.push_back (_route->GoingAway.connect (
			mem_fun (*this, &ProcessorBox::route_going_away)));
	connections.push_back (_route->NameChanged.connect (
			mem_fun(*this, &ProcessorBox::route_name_changed)));

	redisplay_processors ();
}

void
ProcessorBox::route_going_away ()
{
	/* don't keep updating display as processors are deleted */
	no_processor_redisplay = true;
}

void
ProcessorBox::object_drop(DnDVBox<ProcessorEntry>* source, ProcessorEntry* position, Glib::RefPtr<Gdk::DragContext> const & context)
{
	boost::shared_ptr<Processor> p;
	if (position) {
		p = position->processor ();
	}

	list<ProcessorEntry*> children = source->selection ();
	list<boost::shared_ptr<Processor> > procs;
	for (list<ProcessorEntry*>::const_iterator i = children.begin(); i != children.end(); ++i) {
		procs.push_back ((*i)->processor ());
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
			cerr << "source was another processor box, delete the selected items\n";
			other->delete_dragged_processors (procs);
		}
	}
}

void
ProcessorBox::update()
{
	redisplay_processors ();
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

	redisplay_processors ();
}

void
ProcessorBox::remove_processor_gui (boost::shared_ptr<Processor> processor)
{
	boost::shared_ptr<Send> send;
	boost::shared_ptr<Return> retrn;
	boost::shared_ptr<PortInsert> port_insert;

	if ((port_insert = boost::dynamic_pointer_cast<PortInsert> (processor)) != 0) {
		PortInsertUI *io_selector = reinterpret_cast<PortInsertUI *> (port_insert->get_gui());
		port_insert->set_gui (0);
		delete io_selector;
	} else if ((send = boost::dynamic_pointer_cast<Send> (processor)) != 0) {
		SendUIWindow *sui = reinterpret_cast<SendUIWindow*> (send->get_gui());
		send->set_gui (0);
		delete sui;
	} else if ((retrn = boost::dynamic_pointer_cast<Return> (processor)) != 0) {
		ReturnUIWindow *rui = reinterpret_cast<ReturnUIWindow*> (retrn->get_gui());
		retrn->set_gui (0);
		delete rui;
	}
}

void
ProcessorBox::build_send_action_menu ()
{
	using namespace Menu_Helpers;

	send_action_menu = new Menu;
	send_action_menu->set_name ("ArdourContextMenu");
	MenuList& items = send_action_menu->items();

	items.push_back (MenuElem (_("New send"), mem_fun(*this, &ProcessorBox::new_send)));
	items.push_back (MenuElem (_("Show send controls"), mem_fun(*this, &ProcessorBox::show_send_controls)));
}

Gtk::Menu*
ProcessorBox::build_possible_aux_menu ()
{
	boost::shared_ptr<RouteList> rl = _session.get_routes_with_internal_returns();

	if (rl->empty()) {
		return 0;
	}

	using namespace Menu_Helpers;
	Menu* menu = manage (new Menu);
	MenuList& items = menu->items();

	for (RouteList::iterator r = rl->begin(); r != rl->end(); ++r) {
		if (!_route->internal_send_for (*r) && *r != _route) {
			items.push_back (MenuElem ((*r)->name(), bind (sigc::ptr_fun (ProcessorBox::rb_choose_aux), boost::weak_ptr<Route>(*r))));
		}
	}

	return menu;
}

void
ProcessorBox::show_send_controls ()
{
}

void
ProcessorBox::new_send ()
{
}

void
ProcessorBox::show_processor_menu (gint arg)
{
	if (processor_menu == 0) {
		processor_menu = build_processor_menu ();
	}

	Gtk::MenuItem* plugin_menu_item = dynamic_cast<Gtk::MenuItem*>(ActionManager::get_widget("/processormenu/newplugin"));

	if (plugin_menu_item) {
		plugin_menu_item->set_submenu (*_get_plugin_selector()->plugin_menu());
	}

	Gtk::MenuItem* aux_menu_item = dynamic_cast<Gtk::MenuItem*>(ActionManager::get_widget("/processormenu/newaux"));

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

	paste_action->set_sensitive (!_rr_selection.processors.empty());

	processor_menu->popup (1, arg);
}

bool
ProcessorBox::enter_notify (GdkEventCrossing*)
{
	_current_processor_box = this;
	Keyboard::magic_widget_grab_focus ();
	processor_display.grab_focus ();

	return false;
}

bool
ProcessorBox::leave_notify (GdkEventCrossing* ev)
{
	switch (ev->detail) {
	case GDK_NOTIFY_INFERIOR:
		break;
	default:
		Keyboard::magic_widget_drop_focus ();
	}

	return false;
}

bool
ProcessorBox::processor_key_press_event (GdkEventKey *)
{
	/* do real stuff on key release */
	return false;
}

bool
ProcessorBox::processor_key_release_event (GdkEventKey *ev)
{
	bool ret = false;
	ProcSelection targets;

	get_selected_processors (targets);

	if (targets.empty()) {

		int x, y;
		processor_display.get_pointer (x, y);

		pair<ProcessorEntry *, int> const pointer = processor_display.get_child_at_position (x, y);

		if (pointer.first) {
			targets.push_back (pointer.first->processor ());
		}
	}


	switch (ev->keyval) {
	case GDK_a:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
			processor_display.select_all ();
			ret = true;
		} 
		break;

	case GDK_c:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
			copy_processors (targets);
			ret = true;
		}
		break;

	case GDK_x:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
			cut_processors (targets);
			ret = true;
		}
		break;

	case GDK_v:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
			if (targets.empty()) {
				paste_processors ();
			} else {
				paste_processors (targets.front());
			}
			ret = true;
		}
		break;

	case GDK_Up:
		break;

	case GDK_Down:
		break;

	case GDK_Delete:
	case GDK_BackSpace:
		delete_processors (targets);
		ret = true;
		break;

	case GDK_Return:
		for (ProcSelection::iterator i = targets.begin(); i != targets.end(); ++i) {
			if ((*i)->active()) {
				(*i)->deactivate ();
			} else {
				(*i)->activate ();
			}
		}
		ret = true;
		break;

	case GDK_slash:
		ab_plugins ();
		ret = true;
		break;

	default:
		break;
	}

	return ret;
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

		if (_session.engine().connected()) {
			/* XXX giving an error message here is hard, because we may be in the midst of a button press */
			edit_processor (processor);
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
	
	int ret = false;

	if (processor && Keyboard::is_delete_event (ev)) {

		Glib::signal_idle().connect (bind (
				mem_fun(*this, &ProcessorBox::idle_delete_processor),
				boost::weak_ptr<Processor>(processor)));
		ret = true;

	} else if (Keyboard::is_context_menu_event (ev)) {

		/* figure out if we are above or below the fader/amp processor,
		   and set the next insert position appropriately.
		*/

		if (processor) {
			if (_route->processor_is_prefader (processor)) {
				_placement = PreFader;
			} else {
				_placement = PostFader;
			}
		} else {
			_placement = PostFader;
		}

		show_processor_menu (ev->time);
		ret = true;

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
		ret = true;

	}

	return false;
}

Menu *
ProcessorBox::build_processor_menu ()
{
	processor_menu = dynamic_cast<Gtk::Menu*>(ActionManager::get_widget("/processormenu") );
	processor_menu->set_name ("ArdourContextMenu");

	show_all_children();

	return processor_menu;
}

void
ProcessorBox::selection_changed ()
{
	bool sensitive = (processor_display.selection().empty()) ? false : true;
	ActionManager::set_sensitive (ActionManager::plugin_selection_sensitive_actions, sensitive);
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

void
ProcessorBox::use_plugins (const SelectedPlugins& plugins)
{
	for (SelectedPlugins::const_iterator p = plugins.begin(); p != plugins.end(); ++p) {

		boost::shared_ptr<Processor> processor (new PluginInsert (_session, *p));

		Route::ProcessorStreams err_streams;

		if (Config->get_new_plugins_active()) {
			processor->activate ();
		}

		if (_route->add_processor (processor, _placement, &err_streams)) {
			weird_plugin_dialog (**p, err_streams);
			// XXX SHAREDPTR delete plugin here .. do we even need to care?
		} else {

			if (Profile->get_sae()) {
				processor->activate ();
			}
		}
	}
}

void
ProcessorBox::weird_plugin_dialog (Plugin& p, Route::ProcessorStreams streams)
{
	ArdourDialog dialog (_("ardour: weird plugin dialog"));
	Label label;

	string text = string_compose(_("You attempted to add the plugin \"%1\" at index %2.\n"),
			p.name(), streams.index);

	bool has_midi  = streams.count.n_midi() > 0 || p.get_info()->n_inputs.n_midi() > 0;
	bool has_audio = streams.count.n_audio() > 0 || p.get_info()->n_inputs.n_audio() > 0;

	text += _("\nThis plugin has:\n");
	if (has_midi) {
		text += string_compose("\t%1 ", p.get_info()->n_inputs.n_midi()) + _("MIDI input(s)\n");
	}
	if (has_audio) {
		text += string_compose("\t%1 ", p.get_info()->n_inputs.n_audio()) + _("audio input(s)\n");
	}

	text += _("\nBut at the insertion point, there are:\n");
	if (has_midi) {
		text += string_compose("\t%1 ", streams.count.n_midi()) + _("MIDI channel(s)\n");
	}
	if (has_audio) {
		text += string_compose("\t%1 ", streams.count.n_audio()) + _("audio channel(s)\n");
	}

	text += _("\nArdour is unable to insert this plugin here.\n");
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
	boost::shared_ptr<Processor> processor (new PortInsert (_session, _route->mute_master()));
	_route->add_processor (processor, _placement);
}

void
ProcessorBox::choose_send ()
{
	boost::shared_ptr<Send> send (new Send (_session, _route->mute_master()));

	/* make an educated guess at the initial number of outputs for the send */
	ChanCount outs = (_session.master_out())
			? _session.master_out()->n_outputs()
			: _route->n_outputs();

	/* XXX need processor lock on route */
	try {
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

	IOSelectorWindow *ios = new IOSelectorWindow (&_session, send->output(), true);
	ios->show_all ();

	/* keep a reference to the send so it doesn't get deleted while
	   the IOSelectorWindow is doing its stuff
	*/
	_processor_being_created = send;

	ios->selector().Finished.connect (bind (
			mem_fun(*this, &ProcessorBox::send_io_finished),
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
		_route->add_processor (processor, _placement);
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
		_route->add_processor (processor, _placement);
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

	boost::shared_ptr<RouteList> rlist (new RouteList);
	rlist->push_back (_route);

	_session.add_internal_sends (target, PreFader, rlist);
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
	ENSURE_GUI_THREAD (mem_fun(*this, &ProcessorBox::redisplay_processors));

	if (no_processor_redisplay) {
		return;
	}

	processor_display.clear ();

	_route->foreach_processor (mem_fun (*this, &ProcessorBox::add_processor_to_display));

	build_processor_tooltip (processor_eventbox, _("Inserts, sends & plugins:"));
}

void
ProcessorBox::add_processor_to_display (boost::weak_ptr<Processor> p)
{
	boost::shared_ptr<Processor> processor (p.lock ());

	if (!processor || !processor->display_to_user()) {
		return;
	}

	boost::shared_ptr<Send> send = boost::dynamic_pointer_cast<Send> (processor);
	ProcessorEntry* e = 0;
	if (send) {
		e = new SendProcessorEntry (send, _width);
	} else {
		e = new ProcessorEntry (processor, _width);
	}
	e->set_pixel_width (get_allocation().get_width());
	processor_display.add_child (e);
}


void
ProcessorBox::build_processor_tooltip (EventBox& box, string start)
{
	string tip(start);

	list<ProcessorEntry*> children = processor_display.children ();
	for (list<ProcessorEntry*>::iterator i = children.begin(); i != children.end(); ++i) {
		tip += '\n';
  		tip += (*i)->processor()->name();
	}
	
	ARDOUR_UI::instance()->tooltips().set_tip (box, tip);
}

void
ProcessorBox::reordered ()
{
	compute_processor_sort_keys ();
}

void
ProcessorBox::compute_processor_sort_keys ()
{
	list<ProcessorEntry*> children = processor_display.children ();
	Route::ProcessorList our_processors;

	for (list<ProcessorEntry*>::iterator iter = children.begin(); iter != children.end(); ++iter) {
		our_processors.push_back ((*iter)->processor ());
	}

	if (_route->reorder_processors (our_processors)) {

		/* reorder failed, so redisplay */

		redisplay_processors ();

		/* now tell them about the problem */

		ArdourDialog dialog (_("ardour: weird plugin dialog"));
		Label label;

		label.set_text (_("\
You cannot reorder this set of processors\n\
in that way because the inputs and\n\
outputs do not work correctly."));

		dialog.get_vbox()->pack_start (label);
		dialog.add_button (Stock::OK, RESPONSE_ACCEPT);

		dialog.set_name (X_("PluginIODialog"));
		dialog.set_position (Gtk::WIN_POS_MOUSE);
		dialog.set_modal (true);
		dialog.show_all ();

		dialog.run ();
	}
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

void
ProcessorBox::cut_processors ()
{
	ProcSelection to_be_removed;

	get_selected_processors (to_be_removed);
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

			void* gui = (*i)->get_gui ();

			if (gui) {
				static_cast<Gtk::Widget*>(gui)->hide ();
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
ProcessorBox::copy_processors ()
{
	ProcSelection to_be_copied;
	get_selected_processors (to_be_copied);
	copy_processors (to_be_copied);
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
ProcessorBox::delete_processors ()
{
	ProcSelection to_be_deleted;
	get_selected_processors (to_be_deleted);
	delete_processors (to_be_deleted);
}

void
ProcessorBox::delete_processors (const ProcSelection& targets)
{
	if (targets.empty()) {
		return;
	}

	no_processor_redisplay = true;

	for (ProcSelection::const_iterator i = targets.begin(); i != targets.end(); ++i) {

		void* gui = (*i)->get_gui ();

		if (gui) {
			static_cast<Gtk::Widget*>(gui)->hide ();
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

		void* gui = (*x)->get_gui ();

		if (gui) {
			static_cast<Gtk::Widget*>(gui)->hide ();
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
			if (_session.route_by_name (result)) {
				ARDOUR_UI::instance()->popup_error (_("A track already exists with that name."));
				return;
			}
			processor->set_name (result);
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
		assert (type);

		boost::shared_ptr<Processor> p;
		try {
			if (type->value() == "meter" ||
			    type->value() == "main-outs" ||
			    type->value() == "amp" ||
			    type->value() == "intsend" || type->value() == "intreturn") {
				/* do not paste meter, main outs, amp or internal send/returns */
				continue;

			} else if (type->value() == "send") {

				XMLNode n (**niter);
				Send::make_unique (n, _session);
				p.reset (new Send (_session, _route->mute_master(), n));

			} else if (type->value() == "return") {

				XMLNode n (**niter);
				Return::make_unique (n, _session);
				p.reset (new Return (_session, **niter));

			} else {
				/* XXX its a bit limiting to assume that everything else
				   is a plugin.
				*/
				p.reset (new PluginInsert (_session, **niter));
			}

			copies.push_back (p);
		}

		catch (...) {
			cerr << "plugin insert constructor failed\n";
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
ProcessorBox::activate_processor (boost::shared_ptr<Processor> r)
{
	r->activate ();
}

void
ProcessorBox::deactivate_processor (boost::shared_ptr<Processor> r)
{
	r->deactivate ();
}

void
ProcessorBox::get_selected_processors (ProcSelection& processors)
{
	list<ProcessorEntry*> selection = processor_display.selection ();
	for (list<ProcessorEntry*>::iterator i = selection.begin(); i != selection.end(); ++i) {
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
ProcessorBox::all_processors_active (bool state)
{
	_route->all_processors_active (_placement, state);
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

	Gtkmm2ext::Choice prompter (prompt, choices);

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

	Gtkmm2ext::Choice prompter (prompt, choices);

	if (prompter.run () == 1) {
		_route->clear_processors (p);
	}
}

void
ProcessorBox::edit_processor (boost::shared_ptr<Processor> processor)
{
	boost::shared_ptr<Send> send;
	boost::shared_ptr<Return> retrn;
	boost::shared_ptr<PluginInsert> plugin_insert;
	boost::shared_ptr<PortInsert> port_insert;
	Window* gidget = 0;

	if (boost::dynamic_pointer_cast<AudioTrack>(_route) != 0) {

		if (boost::dynamic_pointer_cast<AudioTrack> (_route)->freeze_state() == AudioTrack::Frozen) {
			return;
		}
	}

	if ((send = boost::dynamic_pointer_cast<Send> (processor)) != 0) {

		if (!_session.engine().connected()) {
			return;
		}

#ifdef OLD_SEND_EDITING
		SendUIWindow *send_ui;

		if (send->get_gui() == 0) {
			send_ui = new SendUIWindow (send, _session);
			send_ui->set_title (send->name());
			send->set_gui (send_ui);

		} else {
			send_ui = reinterpret_cast<SendUIWindow *> (send->get_gui());
		}

		gidget = send_ui;
#else
		if (_parent_strip) {
			_parent_strip->show_send (send);
		}
#endif

	} else if ((retrn = boost::dynamic_pointer_cast<Return> (processor)) != 0) {

		if (!_session.engine().connected()) {
			return;
		}

		boost::shared_ptr<Return> retrn = boost::dynamic_pointer_cast<Return> (processor);

		ReturnUIWindow *return_ui;

		if (retrn->get_gui() == 0) {

			return_ui = new ReturnUIWindow (retrn, _session);
			return_ui->set_title (retrn->name ());
			send->set_gui (return_ui);

		} else {
			return_ui = reinterpret_cast<ReturnUIWindow *> (retrn->get_gui());
		}

		gidget = return_ui;

	} else if ((plugin_insert = boost::dynamic_pointer_cast<PluginInsert> (processor)) != 0) {

		PluginUIWindow *plugin_ui;

		/* these are both allowed to be null */

		Container* toplevel = get_toplevel();
		Window* win = dynamic_cast<Gtk::Window*>(toplevel);

		if (plugin_insert->get_gui() == 0) {

			plugin_ui = new PluginUIWindow (win, plugin_insert);
			plugin_ui->set_title (generate_processor_title (plugin_insert));
			plugin_insert->set_gui (plugin_ui);

		} else {
			plugin_ui = reinterpret_cast<PluginUIWindow *> (plugin_insert->get_gui());
			plugin_ui->set_parent (win);
		}

		gidget = plugin_ui;

	} else if ((port_insert = boost::dynamic_pointer_cast<PortInsert> (processor)) != 0) {

		if (!_session.engine().connected()) {
			MessageDialog msg ( _("Not connected to JACK - no I/O changes are possible"));
			msg.run ();
			return;
		}

		PortInsertWindow *io_selector;

		if (port_insert->get_gui() == 0) {
			io_selector = new PortInsertWindow (&_session, port_insert);
			port_insert->set_gui (io_selector);

		} else {
			io_selector = reinterpret_cast<PortInsertWindow *> (port_insert->get_gui());
		}

		gidget = io_selector;
	}

	if (gidget) {
		if (gidget->is_visible()) {
			gidget->get_window()->raise ();
		} else {
			gidget->show_all ();
			gidget->present ();
		}
	}
}

void
ProcessorBox::register_actions ()
{
	Glib::RefPtr<Gtk::ActionGroup> popup_act_grp = Gtk::ActionGroup::create(X_("processormenu"));
	Glib::RefPtr<Action> act;

	/* new stuff */
	ActionManager::register_action (popup_act_grp, X_("newplugin"), _("New Plugin"),
			sigc::ptr_fun (ProcessorBox::rb_choose_plugin));

	act = ActionManager::register_action (popup_act_grp, X_("newinsert"), _("New Insert"),
			sigc::ptr_fun (ProcessorBox::rb_choose_insert));
	ActionManager::jack_sensitive_actions.push_back (act);
	act = ActionManager::register_action (popup_act_grp, X_("newsend"), _("New Send ..."),
			sigc::ptr_fun (ProcessorBox::rb_choose_send));
	ActionManager::jack_sensitive_actions.push_back (act);

	ActionManager::register_action (popup_act_grp, X_("newaux"), _("New Aux Send ..."));

	ActionManager::register_action (popup_act_grp, X_("clear"), _("Clear (all)"),
			sigc::ptr_fun (ProcessorBox::rb_clear));
	ActionManager::register_action (popup_act_grp, X_("clear_pre"), _("Clear (pre-fader)"),
			sigc::ptr_fun (ProcessorBox::rb_clear_pre));
	ActionManager::register_action (popup_act_grp, X_("clear_post"), _("Clear (post-fader)"),
			sigc::ptr_fun (ProcessorBox::rb_clear_post));

	/* standard editing stuff */
	act = ActionManager::register_action (popup_act_grp, X_("cut"), _("Cut"),
			sigc::ptr_fun (ProcessorBox::rb_cut));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);
	act = ActionManager::register_action (popup_act_grp, X_("copy"), _("Copy"),
			sigc::ptr_fun (ProcessorBox::rb_copy));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);

	act = ActionManager::register_action (popup_act_grp, X_("delete"), _("Delete"),
			sigc::ptr_fun (ProcessorBox::rb_delete));
	ActionManager::plugin_selection_sensitive_actions.push_back(act); // ??

	paste_action = ActionManager::register_action (popup_act_grp, X_("paste"), _("Paste"),
			sigc::ptr_fun (ProcessorBox::rb_paste));
	act = ActionManager::register_action (popup_act_grp, X_("rename"), _("Rename"),
			sigc::ptr_fun (ProcessorBox::rb_rename));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);
	ActionManager::register_action (popup_act_grp, X_("selectall"), _("Select All"),
			sigc::ptr_fun (ProcessorBox::rb_select_all));
	ActionManager::register_action (popup_act_grp, X_("deselectall"), _("Deselect All"),
			sigc::ptr_fun (ProcessorBox::rb_deselect_all));

	/* activation etc. */

	ActionManager::register_action (popup_act_grp, X_("activate_all"), _("Activate all"),
					sigc::ptr_fun (ProcessorBox::rb_activate_all));
	ActionManager::register_action (popup_act_grp, X_("deactivate_all"), _("Deactivate all"),
					sigc::ptr_fun (ProcessorBox::rb_deactivate_all));
	ActionManager::register_action (popup_act_grp, X_("ab_plugins"), _("A/B Plugins"),
					sigc::ptr_fun (ProcessorBox::rb_ab_plugins));

	/* show editors */
	act = ActionManager::register_action (popup_act_grp, X_("edit"), _("Edit"),
					      sigc::ptr_fun (ProcessorBox::rb_edit));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);

	ActionManager::add_action_group (popup_act_grp);
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

	_current_processor_box->cut_processors ();
}

void
ProcessorBox::rb_delete ()
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->delete_processors ();
}

void
ProcessorBox::rb_copy ()
{
	if (_current_processor_box == 0) {
		return;
	}
	_current_processor_box->copy_processors ();
}

void
ProcessorBox::rb_paste ()
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->paste_processors ();
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

	_current_processor_box->select_all_processors ();
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

	_current_processor_box->all_processors_active (true);
}

void
ProcessorBox::rb_deactivate_all ()
{
	if (_current_processor_box == 0) {
		return;
	}
	_current_processor_box->all_processors_active (false);
}

void
ProcessorBox::rb_edit ()
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->for_selected_processors (&ProcessorBox::edit_processor);
}

void
ProcessorBox::route_name_changed ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &ProcessorBox::route_name_changed));

	boost::shared_ptr<Processor> processor;
	boost::shared_ptr<PluginInsert> plugin_insert;
	boost::shared_ptr<Send> send;

	list<ProcessorEntry*> children = processor_display.children();

	for (list<ProcessorEntry*>::iterator iter = children.begin(); iter != children.end(); ++iter) {

  		processor = (*iter)->processor ();

		void* gui = processor->get_gui();

		if (!gui) {
			continue;
		}

		/* rename editor windows for sends and plugins */

		if ((send = boost::dynamic_pointer_cast<Send> (processor)) != 0) {
			static_cast<Window*>(gui)->set_title (send->name ());
		} else if ((plugin_insert = boost::dynamic_pointer_cast<PluginInsert> (processor)) != 0) {
			static_cast<Window*>(gui)->set_title (generate_processor_title (plugin_insert));
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

void
ProcessorBox::on_size_allocate (Allocation& a)
{
	HBox::on_size_allocate (a);

	list<ProcessorEntry*> children = processor_display.children ();
	for (list<ProcessorEntry*>::const_iterator i = children.begin(); i != children.end(); ++i) {
		(*i)->set_pixel_width (a.get_width ());
	}
}
