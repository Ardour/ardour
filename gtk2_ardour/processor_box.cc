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
#include <gtkmm2ext/window_title.h>

#include "ardour/amp.h"
#include "ardour/ardour.h"
#include "ardour/audio_diskstream.h"
#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
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
bool ProcessorBox::get_colors = true;
Gdk::Color* ProcessorBox::active_processor_color;
Gdk::Color* ProcessorBox::inactive_processor_color;

ProcessorBox::ProcessorBox (Placement pcmnt, Session& sess, PluginSelector &plugsel,
			    RouteRedirectSelection & rsel, bool owner_is_mixer)
	: _session(sess)
	, _owner_is_mixer (owner_is_mixer)
	, _placement(pcmnt)
	, _plugin_selector(plugsel)
	, _rr_selection(rsel)
{
	if (get_colors) {
		active_processor_color = new Gdk::Color;
		inactive_processor_color = new Gdk::Color;
		set_color (*active_processor_color, rgba_from_style (
				"ProcessorSelector", 0xff, 0, 0, 0, "fg", Gtk::STATE_ACTIVE, false ));
		set_color (*inactive_processor_color, rgba_from_style (
				"ProcessorSelector", 0xff, 0, 0, 0, "fg", Gtk::STATE_NORMAL, false ));
		get_colors = false;
	}

	_width = Wide;
	processor_menu = 0;
	send_action_menu = 0;
	processor_drag_in_progress = false;
	no_processor_redisplay = false;
	ignore_delete = false;

	model = ListStore::create(columns);

	RefPtr<TreeSelection> selection = processor_display.get_selection();
	selection->set_mode (Gtk::SELECTION_MULTIPLE);
	selection->signal_changed().connect (mem_fun (*this, &ProcessorBox::selection_changed));

	processor_display.set_model (model);
	processor_display.append_column (X_("notshown"), columns.text);
	processor_display.set_name ("ProcessorSelector");
	processor_display.set_headers_visible (false);
	processor_display.set_reorderable (true);
	processor_display.set_size_request (-1, 40);
	processor_display.get_column(0)->set_sizing(TREE_VIEW_COLUMN_FIXED);
	processor_display.get_column(0)->set_fixed_width(48);
	processor_display.add_object_drag (columns.processor.index(), "processors");
	processor_display.signal_drop.connect (mem_fun (*this, &ProcessorBox::object_drop));

	TreeViewColumn* name_col = processor_display.get_column(0);
	CellRendererText* renderer = dynamic_cast<CellRendererText*>(
			processor_display.get_column_cell_renderer (0));
	name_col->add_attribute(renderer->property_foreground_gdk(), columns.color);

	processor_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	model->signal_row_deleted().connect (mem_fun (*this, &ProcessorBox::row_deleted));

	processor_scroller.add (processor_display);
	processor_eventbox.add (processor_scroller);

	processor_scroller.set_size_request (-1, 40);

	pack_start (processor_eventbox, true, true);

	processor_eventbox.signal_enter_notify_event().connect (bind (
			sigc::ptr_fun (ProcessorBox::enter_box),
			this));

	processor_display.signal_button_press_event().connect (
			mem_fun(*this, &ProcessorBox::processor_button_press_event), false);
	processor_display.signal_button_release_event().connect (
			mem_fun(*this, &ProcessorBox::processor_button_release_event));
}

ProcessorBox::~ProcessorBox ()
{
}

void
ProcessorBox::set_route (boost::shared_ptr<Route> r)
{
	connections.clear ();

	_route = r;

	connections.push_back (_route->processors_changed.connect (
			mem_fun(*this, &ProcessorBox::redisplay_processors)));
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
ProcessorBox::object_drop (const list<boost::shared_ptr<Processor> >& procs)
{
	for (std::list<boost::shared_ptr<Processor> >::const_iterator i = procs.begin();
			i != procs.end(); ++i) {
		XMLNode& state = (*i)->get_state ();
		XMLNodeList nlist;
		nlist.push_back (&state);
		paste_processor_state (nlist);
		delete &state;
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
		plugin_menu_item->set_submenu (_plugin_selector.plugin_menu());
	}

	paste_action->set_sensitive (!_rr_selection.processors.empty());

	processor_menu->popup (1, arg);
}

void
ProcessorBox::processor_drag_begin (GdkDragContext *context)
{
	processor_drag_in_progress = true;
}

void
ProcessorBox::processor_drag_end (GdkDragContext *context)
{
	processor_drag_in_progress = false;
}

bool
ProcessorBox::processor_button_press_event (GdkEventButton *ev)
{
	TreeIter iter;
	TreeModel::Path path;
	TreeViewColumn* column;
	int cellx;
	int celly;
	boost::shared_ptr<Processor> processor;
	int ret = false;
	bool selected = false;

	if (processor_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		if ((iter = model->get_iter (path))) {
			processor = (*iter)[columns.processor];
			selected = processor_display.get_selection()->is_selected (iter);
		}

	}

	if (processor && (Keyboard::is_edit_event (ev) || (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS))) {

		if (_session.engine().connected()) {
			/* XXX giving an error message here is hard, because we may be in the midst of a button press */
			edit_processor (processor);
		}
		ret = true;

	} else if (processor && ev->button == 1 && selected) {

		// this is purely informational but necessary
		ProcessorSelected (processor); // emit

	} else if (!processor && ev->button == 1 && ev->type == GDK_2BUTTON_PRESS) {

		choose_plugin ();
		_plugin_selector.show_manager ();
	}


	return ret;
}

bool
ProcessorBox::processor_button_release_event (GdkEventButton *ev)
{
	TreeIter iter;
	TreeModel::Path path;
	TreeViewColumn* column;
	int cellx;
	int celly;
	boost::shared_ptr<Processor> processor;
	int ret = false;


	if (processor_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		if ((iter = model->get_iter (path))) {
			processor = (*iter)[columns.processor];
		}
	}

	if (processor && Keyboard::is_delete_event (ev)) {

		Glib::signal_idle().connect (bind (
				mem_fun(*this, &ProcessorBox::idle_delete_processor),
				boost::weak_ptr<Processor>(processor)));
		ret = true;

	} else if (Keyboard::is_context_menu_event (ev)) {

		show_processor_menu(ev->time);
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

	return ret;
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
	bool sensitive = (processor_display.get_selection()->count_selected_rows()) ? true : false;
	ActionManager::set_sensitive (ActionManager::plugin_selection_sensitive_actions, sensitive);
}

void
ProcessorBox::select_all_processors ()
{
	processor_display.get_selection()->select_all();
}

void
ProcessorBox::deselect_all_processors ()
{
	processor_display.get_selection()->unselect_all();
}

void
ProcessorBox::choose_plugin ()
{
	_plugin_selector.set_interested_object (*this);
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

		assign_default_sort_key (processor);

		if (_route->add_processor (processor, &err_streams)) {
			weird_plugin_dialog (**p, err_streams, _route);
			// XXX SHAREDPTR delete plugin here .. do we even need to care?
		} else {

			if (Profile->get_sae()) {
				processor->activate ();
			}
			processor->ActiveChanged.connect (bind (
					mem_fun (*this, &ProcessorBox::show_processor_active),
					boost::weak_ptr<Processor>(processor)));
		}
	}
}

void
ProcessorBox::weird_plugin_dialog (Plugin& p, Route::ProcessorStreams streams, boost::shared_ptr<IO> io)
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
	boost::shared_ptr<Processor> processor (new PortInsert (_session));
	processor->ActiveChanged.connect (bind (
			mem_fun(*this, &ProcessorBox::show_processor_active),
			boost::weak_ptr<Processor>(processor)));

	assign_default_sort_key (processor);
	_route->add_processor (processor);
}

void
ProcessorBox::choose_send ()
{
	boost::shared_ptr<Send> send (new Send (_session));

	/* make an educated guess at the initial number of outputs for the send */
	ChanCount outs = (_session.master_out())
			? _session.master_out()->n_outputs()
			: _route->n_outputs();

	/* XXX need processor lock on route */
	try {
		send->io()->ensure_io (ChanCount::ZERO, outs, false, this);
	} catch (AudioEngine::PortRegistrationFailure& err) {
		error << string_compose (_("Cannot set up new send: %1"), err.what()) << endmsg;
		return;
	}

	/* let the user adjust the IO setup before creation */
	IOSelectorWindow *ios = new IOSelectorWindow (_session, send->io(), false, true);
	ios->show_all ();

	/* keep a reference to the send so it doesn't get deleted while
	   the IOSelectorWindow is doing its stuff */
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
		assign_default_sort_key (processor);
		_route->add_processor (processor);
		if (Profile->get_sae()) {
			processor->activate ();
		}
		break;
	}

	delete_when_idle (ios);
}

void
ProcessorBox::choose_return ()
{
	boost::shared_ptr<Return> retrn (new Return (_session));

	/* assume user just wants a single audio input (sidechain) by default */
	ChanCount ins(DataType::AUDIO, 1);

	/* XXX need processor lock on route */
	try {
		retrn->io()->ensure_io (ins, ChanCount::ZERO, false, this);
	} catch (AudioEngine::PortRegistrationFailure& err) {
		error << string_compose (_("Cannot set up new return: %1"), err.what()) << endmsg;
		return;
	}

	/* let the user adjust the IO setup before creation */
	IOSelectorWindow *ios = new IOSelectorWindow (_session, retrn->io(), true, true);
	ios->show_all ();

	/* keep a reference to the send so it doesn't get deleted while
	   the IOSelectorWindow is doing its stuff */
	_processor_being_created = retrn;

	ios->selector().Finished.connect (bind (
			mem_fun(*this, &ProcessorBox::return_io_finished),
			boost::weak_ptr<Processor>(retrn), ios));
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
		assign_default_sort_key (processor);
		_route->add_processor (processor);
		if (Profile->get_sae()) {
			processor->activate ();
		}
		break;
	}

	delete_when_idle (ios);
}

void
ProcessorBox::redisplay_processors ()
{
	ENSURE_GUI_THREAD (mem_fun(*this, &ProcessorBox::redisplay_processors));

	if (no_processor_redisplay) {
		return;
	}

	ignore_delete = true;
	model->clear ();
	ignore_delete = false;

	processor_active_connections.clear ();
	processor_name_connections.clear ();

	_route->foreach_processor (_placement, mem_fun (*this, &ProcessorBox::add_processor_to_display));

	switch (_placement) {
	case PreFader:
		build_processor_tooltip (processor_eventbox, _("Pre-fader inserts, sends & plugins:"));
		break;
	case PostFader:
		build_processor_tooltip (processor_eventbox, _("Post-fader inserts, sends & plugins:"));
		break;
	}
}

void
ProcessorBox::add_processor_to_display (boost::weak_ptr<Processor> p)
{
	boost::shared_ptr<Processor> processor (p.lock ());
	if (!processor) {
		return;
	}

	if (processor == _route->amp()) {
		return;
	}

	Gtk::TreeModel::Row row = *(model->append());
	row[columns.text] = processor_name (processor);
	row[columns.processor] = processor;

	show_processor_active (processor);

	processor_active_connections.push_back (processor->ActiveChanged.connect (bind (
			mem_fun(*this, &ProcessorBox::show_processor_active),
			boost::weak_ptr<Processor>(processor))));
	processor_name_connections.push_back (processor->NameChanged.connect (bind (
			mem_fun(*this, &ProcessorBox::show_processor_name),
			boost::weak_ptr<Processor>(processor))));
}

string
ProcessorBox::processor_name (boost::weak_ptr<Processor> weak_processor)
{
	boost::shared_ptr<Processor> processor (weak_processor.lock());

	if (!processor) {
		return string();
	}

	boost::shared_ptr<Send> send;
	string name_display;

	if (!processor->active()) {
		name_display = " (";
	}

	if ((send = boost::dynamic_pointer_cast<Send> (processor)) != 0) {

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
			name_display += processor->name();
			break;
		case Narrow:
			name_display += PBD::short_version (processor->name(), 5);
			break;
		}

	}

	if (!processor->active()) {
		name_display += ')';
	}

	return name_display;
}

void
ProcessorBox::build_processor_tooltip (EventBox& box, string start)
{
	string tip(start);

	Gtk::TreeModel::Children children = model->children();
	for(Gtk::TreeModel::Children::iterator iter = children.begin(); iter != children.end(); ++iter) {
  		Gtk::TreeModel::Row row = *iter;
		tip += '\n';
  		tip += row[columns.text];
	}
	ARDOUR_UI::instance()->tooltips().set_tip (box, tip);
}

void
ProcessorBox::show_processor_name (boost::weak_ptr<Processor> processor)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &ProcessorBox::show_processor_name), processor));
	show_processor_active (processor);
}

void
ProcessorBox::show_processor_active (boost::weak_ptr<Processor> weak_processor)
{
	boost::shared_ptr<Processor> processor (weak_processor.lock());

	if (!processor) {
		return;
	}

	ENSURE_GUI_THREAD(bind (mem_fun(*this, &ProcessorBox::show_processor_active), weak_processor));

	Gtk::TreeModel::Children children = model->children();
	Gtk::TreeModel::Children::iterator iter = children.begin();

	while (iter != children.end()) {

		boost::shared_ptr<Processor> r = (*iter)[columns.processor];

		if (r == processor) {
			(*iter)[columns.text] = processor_name (r);

			if (processor->active()) {
				(*iter)[columns.color] = *active_processor_color;
			} else {
				(*iter)[columns.color] = *inactive_processor_color;
			}
			break;
		}

		iter++;
	}
}

void
ProcessorBox::row_deleted (const Gtk::TreeModel::Path& path)
{
	if (!ignore_delete) {
		compute_processor_sort_keys ();
	}
}

void
ProcessorBox::compute_processor_sort_keys ()
{
	uint32_t sort_key;
	Gtk::TreeModel::Children children = model->children();

	if (_placement == PreFader) {
		sort_key = 0;
	} else {
		sort_key = _route->fader_sort_key() + 1;
	}

	for (Gtk::TreeModel::Children::iterator iter = children.begin(); iter != children.end(); ++iter) {
		boost::shared_ptr<Processor> r = (*iter)[columns.processor];
		r->set_sort_key (sort_key);
		sort_key++;
	}

	if (_route->sort_processors ()) {

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
	XMLNode* node = new XMLNode (X_("cut"));

	get_selected_processors (to_be_removed);

	if (to_be_removed.empty()) {
		return;
	}

	no_processor_redisplay = true;
	for (ProcSelection::iterator i = to_be_removed.begin(); i != to_be_removed.end(); ++i) {
		// Do not cut inserts
		if (boost::dynamic_pointer_cast<PluginInsert>((*i)) != 0 ||
		    (boost::dynamic_pointer_cast<Send>((*i)) != 0)) {

			void* gui = (*i)->get_gui ();

			if (gui) {
				static_cast<Gtk::Widget*>(gui)->hide ();
			}

			XMLNode& child ((*i)->get_state());

			if (_route->remove_processor (*i) == 0) {
				/* success */
				node->add_child_nocopy (child);
			} else {
				delete &child;
			}
		}
	}

	_rr_selection.set (node);

	no_processor_redisplay = false;
	redisplay_processors ();
}

void
ProcessorBox::copy_processors ()
{
	ProcSelection to_be_copied;
	XMLNode* node = new XMLNode (X_("copy"));

	get_selected_processors (to_be_copied);

	if (to_be_copied.empty()) {
		return;
	}

	for (ProcSelection::iterator i = to_be_copied.begin(); i != to_be_copied.end(); ++i) {
		// Do not copy inserts
		if (boost::dynamic_pointer_cast<PluginInsert>((*i)) != 0 ||
		    (boost::dynamic_pointer_cast<Send>((*i)) != 0)) {
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

	if (to_be_deleted.empty()) {
		return;
	}

	for (ProcSelection::iterator i = to_be_deleted.begin(); i != to_be_deleted.end(); ++i) {

		void* gui = (*i)->get_gui ();

		if (gui) {
			static_cast<Gtk::Widget*>(gui)->hide ();
		}

		_route->remove_processor(*i);
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
	name_prompter.set_prompt (_("rename processor"));
	name_prompter.set_initial_text (processor->name());
	name_prompter.add_button (_("Rename"), Gtk::RESPONSE_ACCEPT);
	name_prompter.set_response_sensitive (Gtk::RESPONSE_ACCEPT, false);
	name_prompter.show_all ();

	switch (name_prompter.run ()) {

	case Gtk::RESPONSE_ACCEPT:
        name_prompter.get_result (result);
        if (result.length()) {
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

	cerr << "paste from node called " << _rr_selection.processors.get_node().name() << endl;

	paste_processor_state (_rr_selection.processors.get_node().children());
}

void
ProcessorBox::paste_processor_state (const XMLNodeList& nlist)
{
	XMLNodeConstIterator niter;
	list<boost::shared_ptr<Processor> > copies;

	cerr << "Pasting processor selection containing " << nlist.size() << endl;

	if (nlist.empty()) {
		return;
	}

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		cerr << "try using " << (*niter)->name() << endl;
		XMLProperty const * type = (*niter)->property ("type");
		assert (type);

		boost::shared_ptr<Processor> p;
		try {
			if (type->value() == "send") {
				XMLNode n (**niter);
				Send::make_unique (n, _session);
				p.reset (new Send (_session, n));

			} else if (type->value() == "meter") {
				p = _route->shared_peak_meter();

			} else if (type->value() == "main-outs") {
				/* do not copy-n-paste main outs */
				continue;

			} else if (type->value() == "amp") {
				/* do not copy-n-paste amp */
				continue;

			} else if (type->value() == "listen") {
				p.reset (new Delivery (_session, **niter));
				
			} else {
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

	assign_default_sort_key (copies.front());

	if (_route->add_processors (copies, 0, copies.front()->sort_key())) {

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
    vector<Gtk::TreeModel::Path> pathlist = processor_display.get_selection()->get_selected_rows();

    for (vector<Gtk::TreeModel::Path>::iterator iter = pathlist.begin(); iter != pathlist.end(); ++iter) {
	    processors.push_back ((*(model->get_iter(*iter)))[columns.processor]);
    }
}

void
ProcessorBox::for_selected_processors (void (ProcessorBox::*method)(boost::shared_ptr<Processor>))
{
    vector<Gtk::TreeModel::Path> pathlist = processor_display.get_selection()->get_selected_rows();

	for (vector<Gtk::TreeModel::Path>::iterator iter = pathlist.begin(); iter != pathlist.end(); ++iter) {
		boost::shared_ptr<Processor> processor = (*(model->get_iter(*iter)))[columns.processor];
		(this->*method)(processor);
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

	if (boost::dynamic_pointer_cast<AudioTrack>(_route) != 0) {
		if (_placement == PreFader) {
			prompt = _("Do you really want to remove all pre-fader processors from this track?\n"
				   "(this cannot be undone)");
		} else {
			prompt = _("Do you really want to remove all post-fader processors from this track?\n"
				   "(this cannot be undone)");
		}
	} else {
		if (_placement == PreFader) {
			prompt = _("Do you really want to remove all pre-fader processors from this bus?\n"
				   "(this cannot be undone)");
		} else {
			prompt = _("Do you really want to remove all post-fader processors from this bus?\n"
				   "(this cannot be undone)");
		}
	}

	choices.push_back (_("Cancel"));
	choices.push_back (_("Yes, remove them all"));

	Gtkmm2ext::Choice prompter (prompt, choices);

	if (prompter.run () == 1) {
		_route->clear_processors (_placement);
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

		boost::shared_ptr<Send> send = boost::dynamic_pointer_cast<Send> (processor);

		SendUIWindow *send_ui;

		if (send->get_gui() == 0) {

			send_ui = new SendUIWindow (send, _session);

			WindowTitle title(Glib::get_application_name());
			title += send->name();
			send_ui->set_title (title.get_string());

			send->set_gui (send_ui);

		} else {
			send_ui = reinterpret_cast<SendUIWindow *> (send->get_gui());
		}

		gidget = send_ui;
	
	} else if ((retrn = boost::dynamic_pointer_cast<Return> (processor)) != 0) {

		if (!_session.engine().connected()) {
			return;
		}

		boost::shared_ptr<Return> retrn = boost::dynamic_pointer_cast<Return> (processor);

		ReturnUIWindow *return_ui;

		if (retrn->get_gui() == 0) {

			return_ui = new ReturnUIWindow (retrn, _session);

			WindowTitle title(Glib::get_application_name());
			title += retrn->name();
			return_ui->set_title (title.get_string());

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

			WindowTitle title(Glib::get_application_name());
			title += generate_processor_title (plugin_insert);
			plugin_ui->set_title (title.get_string());

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
			io_selector = new PortInsertWindow (_session, port_insert);
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

bool
ProcessorBox::enter_box (GdkEventCrossing *ev, ProcessorBox* rb)
{
	switch (ev->detail) {
	case GDK_NOTIFY_INFERIOR:
		break;

	case GDK_NOTIFY_VIRTUAL:
		/* fallthru */

	default:
		_current_processor_box = rb;
	}

	return false;
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
	act = ActionManager::register_action (popup_act_grp, X_("newreturn"), _("New Return ..."),
			sigc::ptr_fun (ProcessorBox::rb_choose_return));
	ActionManager::jack_sensitive_actions.push_back (act);

	ActionManager::register_action (popup_act_grp, X_("clear"), _("Clear"),
			sigc::ptr_fun (ProcessorBox::rb_clear));

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

	/* activation */
	act = ActionManager::register_action (popup_act_grp, X_("activate"), _("Activate"),
			sigc::ptr_fun (ProcessorBox::rb_activate));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);
	act = ActionManager::register_action (popup_act_grp, X_("deactivate"), _("Deactivate"),
			sigc::ptr_fun (ProcessorBox::rb_deactivate));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);
	ActionManager::register_action (popup_act_grp, X_("activate_all"), _("Activate all"),
			sigc::ptr_fun (ProcessorBox::rb_activate_all));
	ActionManager::register_action (popup_act_grp, X_("deactivate_all"), _("Deactivate all"),
			sigc::ptr_fun (ProcessorBox::rb_deactivate_all));

	/* show editors */
	act = ActionManager::register_action (popup_act_grp, X_("edit"), _("Edit"),
			sigc::ptr_fun (ProcessorBox::rb_edit));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);

	ActionManager::add_action_group (popup_act_grp);
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
ProcessorBox::rb_choose_return ()
{
	if (_current_processor_box == 0) {
		return;
	}
	_current_processor_box->choose_return ();
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
ProcessorBox::rb_activate ()
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->for_selected_processors (&ProcessorBox::activate_processor);
}

void
ProcessorBox::rb_deactivate ()
{
	if (_current_processor_box == 0) {
		return;
	}
	_current_processor_box->for_selected_processors (&ProcessorBox::deactivate_processor);
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

	Gtk::TreeModel::Children children = model->children();

	for (Gtk::TreeModel::Children::iterator iter = children.begin(); iter != children.end(); ++iter) {
  		Gtk::TreeModel::Row row = *iter;

  		processor= row[columns.processor];

		void* gui = processor->get_gui();

		if (!gui) {
			continue;
		}

		/* rename editor windows for sends and plugins */

		WindowTitle title (Glib::get_application_name());

		if ((send = boost::dynamic_pointer_cast<Send> (processor)) != 0) {
			title += send->name();
			static_cast<Window*>(gui)->set_title (title.get_string());
		} else if ((plugin_insert = boost::dynamic_pointer_cast<PluginInsert> (processor)) != 0) {
			title += generate_processor_title (plugin_insert);
			static_cast<Window*>(gui)->set_title (title.get_string());
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
ProcessorBox::assign_default_sort_key (boost::shared_ptr<Processor> p)
{
	p->set_sort_key (_placement == PreFader ? 0 : 9999);
	cerr << "default sort key for "
	     << _placement << " = " << p->sort_key()
	     << endl;
}
	
