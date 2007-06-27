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

#include <sigc++/bind.h>

#include <pbd/convert.h>

#include <glibmm/miscutils.h>

#include <gtkmm/messagedialog.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/choice.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/doi.h>
#include <gtkmm2ext/window_title.h>

#include <ardour/ardour.h>
#include <ardour/session.h>
#include <ardour/audioengine.h>
#include <ardour/route.h>
#include <ardour/audio_track.h>
#include <ardour/audio_diskstream.h>
#include <ardour/send.h>
#include <ardour/plugin_insert.h>
#include <ardour/port_insert.h>
#include <ardour/ladspa_plugin.h>

#include "ardour_ui.h"
#include "ardour_dialog.h"
#include "public_editor.h"
#include "processor_box.h"
#include "keyboard.h"
#include "plugin_selector.h"
#include "route_processor_selection.h"
#include "mixer_ui.h"
#include "actions.h"
#include "plugin_ui.h"
#include "send_ui.h"
#include "io_selector.h"
#include "utils.h"
#include "gui_thread.h"

#include "i18n.h"

#ifdef HAVE_AUDIOUNIT
#include "au_pluginui.h"
#endif

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

ProcessorBox::ProcessorBox (Placement pcmnt, Session& sess, boost::shared_ptr<Route> rt, PluginSelector &plugsel, 
			  RouteRedirectSelection & rsel, bool owner_is_mixer)
	: _route(rt), 
	  _session(sess), 
	  _owner_is_mixer (owner_is_mixer), 
	  _placement(pcmnt), 
	  _plugin_selector(plugsel),
	  _rr_selection(rsel)
{
	if (get_colors) {
		active_processor_color = new Gdk::Color;
		inactive_processor_color = new Gdk::Color;
		set_color (*active_processor_color, rgba_from_style ("RedirectSelector", 0xff, 0, 0, 0, "fg", Gtk::STATE_ACTIVE, false ));
		set_color (*inactive_processor_color, rgba_from_style ("RedirectSelector", 0xff, 0, 0, 0, "fg", Gtk::STATE_NORMAL, false ));
		get_colors = false;
	}

	_width = Wide;
	processor_menu = 0;
	send_action_menu = 0;
	processor_drag_in_progress = false;
	no_processor_redisplay = false;
	ignore_delete = false;
	ab_direction = true;

	model = ListStore::create(columns);

	RefPtr<TreeSelection> selection = processor_display.get_selection();
	selection->set_mode (Gtk::SELECTION_MULTIPLE);
	selection->signal_changed().connect (mem_fun (*this, &ProcessorBox::selection_changed));

	processor_display.set_model (model);
	processor_display.append_column (X_("notshown"), columns.text);
	processor_display.set_name ("RedirectSelector");
	processor_display.set_headers_visible (false);
	processor_display.set_reorderable (true);
	processor_display.set_size_request (-1, 40);
	processor_display.get_column(0)->set_sizing(TREE_VIEW_COLUMN_FIXED);
	processor_display.get_column(0)->set_fixed_width(48);
	processor_display.add_object_drag (columns.processor.index(), "redirects");
	processor_display.signal_object_drop.connect (mem_fun (*this, &ProcessorBox::object_drop));

	TreeViewColumn* name_col = processor_display.get_column(0);
	CellRendererText* renderer = dynamic_cast<CellRendererText*>(processor_display.get_column_cell_renderer (0));
	name_col->add_attribute(renderer->property_foreground_gdk(), columns.color);

	processor_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	
	model->signal_row_deleted().connect (mem_fun (*this, &ProcessorBox::row_deleted));

	processor_scroller.add (processor_display);
	processor_eventbox.add (processor_scroller);
	
	processor_scroller.set_size_request (-1, 40);

	pack_start (processor_eventbox, true, true);

	_route->processors_changed.connect (mem_fun(*this, &ProcessorBox::redisplay_processors));
	_route->GoingAway.connect (mem_fun (*this, &ProcessorBox::route_going_away));

	processor_eventbox.signal_enter_notify_event().connect (bind (sigc::ptr_fun (ProcessorBox::enter_box), this));

	processor_display.signal_button_press_event().connect (mem_fun(*this, &ProcessorBox::processor_button_press_event), false);
	processor_display.signal_button_release_event().connect (mem_fun(*this, &ProcessorBox::processor_button_release_event));

	/* start off as a passthru strip. we'll correct this, if necessary,
	   in update_diskstream_display().
	*/

	/* now force an update of all the various elements */

	redisplay_processors ();
}

ProcessorBox::~ProcessorBox ()
{
}

void
ProcessorBox::route_going_away ()
{
	/* don't keep updating display as processors are deleted */
	no_processor_redisplay = true;
}

void
ProcessorBox::object_drop (string type, uint32_t cnt, const boost::shared_ptr<Processor>* ptr)
{
	if (type != "redirects" || cnt == 0 || !ptr) {
		return;
	}

	/* do something with the dropped processors */

	list<boost::shared_ptr<Processor> > processors;
	
	for (uint32_t n = 0; n < cnt; ++n) {
		processors.push_back (ptr[n]);
	}
	
	paste_processor_list (processors);
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
	boost::shared_ptr<PortInsert> port_processor;

	if ((port_processor = boost::dynamic_pointer_cast<PortInsert> (processor)) != 0) {
			PortInsertUI *io_selector = reinterpret_cast<PortInsertUI *> (port_processor->get_gui());
			port_processor->set_gui (0);
			delete io_selector;
	} else if ((send = boost::dynamic_pointer_cast<Send> (processor)) != 0) {
		SendUIWindow *sui = reinterpret_cast<SendUIWindow*> (send->get_gui());
		send->set_gui (0);
		delete sui;
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
		InsertSelected (processor); // emit
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
		
		Glib::signal_idle().connect (bind (mem_fun(*this, &ProcessorBox::idle_delete_processor), boost::weak_ptr<Processor>(processor)));
		ret = true;
		
	} else if (Keyboard::is_context_menu_event (ev)) {

		show_processor_menu(ev->time);
		ret = true;

	} else if (processor && (ev->button == 2) && (ev->state == Gdk::BUTTON2_MASK)) {
		
		processor->set_active (!processor->active());
		ret = true;

	} 

	return ret;
}

Menu *
ProcessorBox::build_processor_menu ()
{
	processor_menu = dynamic_cast<Gtk::Menu*>(ActionManager::get_widget("/redirectmenu") );
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
	sigc::connection newplug_connection = _plugin_selector.PluginCreated.connect (mem_fun(*this,&ProcessorBox::processor_plugin_chosen));
	_plugin_selector.show_all();
	_plugin_selector.run ();
	newplug_connection.disconnect();
}

void
ProcessorBox::processor_plugin_chosen (boost::shared_ptr<Plugin> plugin)
{
	if (plugin) {

		boost::shared_ptr<Processor> processor (new PluginInsert (_session, plugin, _placement));
		
		processor->ActiveChanged.connect (bind (mem_fun (*this, &ProcessorBox::show_processor_active), boost::weak_ptr<Processor>(processor)));

		Route::ProcessorStreams err;

		if (_route->add_processor (processor, &err)) {
			weird_plugin_dialog (*plugin, err, _route);
			// XXX SHAREDPTR delete plugin here .. do we even need to care? 
		}
	}
}

void
ProcessorBox::weird_plugin_dialog (Plugin& p, Route::ProcessorStreams streams, boost::shared_ptr<IO> io)
{
	ArdourDialog dialog (_("ardour: weird plugin dialog"));
	Label label;

	/* i hate this kind of code */

	if (streams.count > p.get_info()->n_inputs) {
		label.set_text (string_compose (_(
"You attempted to add a plugin (%1).\n"
"The plugin has %2 inputs\n"
"but at the processorion point, there are\n"
"%3 active signal streams.\n"
"\n"
"This makes no sense - you are throwing away\n"
"part of the signal."),
					 p.name(),
					 p.get_info()->n_inputs.n_total(),
					 streams.count.n_total()));
	} else if (streams.count < p.get_info()->n_inputs) {
		label.set_text (string_compose (_(
"You attempted to add a plugin (%1).\n"
"The plugin has %2 inputs\n"
"but at the processorion point there are\n"
"only %3 active signal streams.\n"
"\n"
"This makes no sense - unless the plugin supports\n"
"side-chain inputs. A future version of Ardour will\n"
"support this type of configuration."),
					 p.name(),
					 p.get_info()->n_inputs.n_total(),
					 streams.count.n_total()));
	} else {
		label.set_text (string_compose (_(
"You attempted to add a plugin (%1).\n"
"\n"
"The I/O configuration doesn't make sense:\n"
"\n" 
"The plugin has %2 inputs and %3 outputs.\n"
"The track/bus has %4 inputs and %5 outputs.\n"
"The processorion point, has %6 active signals.\n"
"\n"
"Ardour does not understand what to do in such situations.\n"),
					 p.name(),
					 p.get_info()->n_inputs.n_total(),
					 p.get_info()->n_outputs.n_total(),
					 io->n_inputs().n_total(),
					 io->n_outputs().n_total(),
					 streams.count.n_total()));
	}

	dialog.get_vbox()->pack_start (label);
	dialog.add_button (Stock::OK, RESPONSE_ACCEPT);

	dialog.set_name (X_("PluginIODialog"));
	dialog.set_position (Gtk::WIN_POS_MOUSE);
	dialog.set_modal (true);
	dialog.show_all ();

	dialog.run ();
}

void
ProcessorBox::choose_processor ()
{
	boost::shared_ptr<Processor> processor (new PortInsert (_session, _placement));
	processor->ActiveChanged.connect (bind (mem_fun(*this, &ProcessorBox::show_processor_active), boost::weak_ptr<Processor>(processor)));
	_route->add_processor (processor);
}

void
ProcessorBox::choose_send ()
{
	boost::shared_ptr<Send> send (new Send (_session, _placement));
	//send->set_default_type(_route->default_type());

	/* XXX need redirect lock on route */

	// This will be set properly in route->add_processor
	send->configure_io (_route->max_processor_outs(), _route->max_processor_outs());
	
	IOSelectorWindow *ios = new IOSelectorWindow (_session, send->io(), false, true);
	
	ios->show_all ();

	ios->selector().Finished.connect (bind (mem_fun(*this, &ProcessorBox::send_io_finished), send, ios));
}

void
ProcessorBox::send_io_finished (IOSelector::Result r, boost::shared_ptr<Send> send, IOSelectorWindow* ios)
{
	if (!send) {
		return;
	}

	switch (r) {
	case IOSelector::Cancelled:
		// send will go away when all shared_ptrs to it vanish
		break;

	case IOSelector::Accepted:
		_route->add_processor (send);
		break;
	}

	delete_when_idle (ios);
}

void
ProcessorBox::redisplay_processors ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &ProcessorBox::redisplay_processors));

	if (no_processor_redisplay) {
		return;
	}
	
	ignore_delete = true;
	model->clear ();
	ignore_delete = false;

	processor_active_connections.clear ();
	processor_name_connections.clear ();

	void (ProcessorBox::*pmf)(boost::shared_ptr<Processor>) = &ProcessorBox::add_processor_to_display;
	_route->foreach_processor (this, pmf);

	switch (_placement) {
	case PreFader:
		build_processor_tooltip(processor_eventbox, _("Pre-fader processors, sends & plugins:"));
		break;
	case PostFader:
		build_processor_tooltip(processor_eventbox, _("Post-fader processors, sends & plugins:"));
		break;
	}
}

void
ProcessorBox::add_processor_to_display (boost::shared_ptr<Processor> processor)
{
	if (processor->placement() != _placement) {
		return;
	}
	
	Gtk::TreeModel::Row row = *(model->append());
	row[columns.text] = processor_name (processor);
	row[columns.processor] = processor;

	show_processor_active (processor);

	processor_active_connections.push_back (processor->ActiveChanged.connect (bind (mem_fun(*this, &ProcessorBox::show_processor_active), boost::weak_ptr<Processor>(processor))));
	processor_name_connections.push_back (processor->NameChanged.connect (bind (mem_fun(*this, &ProcessorBox::show_processor_name), boost::weak_ptr<Processor>(processor))));
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

		/* don't use the column text, since it may be narrowed */

		boost::shared_ptr<Processor> i = row[columns.processor];
  		tip += i->name();
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
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &ProcessorBox::show_processor_active), weak_processor));
	
	boost::shared_ptr<Processor> processor (weak_processor.lock());
	
	if (!processor) {
		return;
	}

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
	uint32_t sort_key = 0;
	Gtk::TreeModel::Children children = model->children();

	for (Gtk::TreeModel::Children::iterator iter = children.begin(); iter != children.end(); ++iter) {
		boost::shared_ptr<Processor> i = (*iter)[columns.processor];
		i->set_sort_key (sort_key);
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
	vector<boost::shared_ptr<Processor> > to_be_renamed;
	
	get_selected_processors (to_be_renamed);

	if (to_be_renamed.empty()) {
		return;
	}

	for (vector<boost::shared_ptr<Processor> >::iterator i = to_be_renamed.begin(); i != to_be_renamed.end(); ++i) {
		rename_processor (*i);
	}
}

void
ProcessorBox::cut_processors ()
{
	vector<boost::shared_ptr<Processor> > to_be_removed;
	
	get_selected_processors (to_be_removed);

	if (to_be_removed.empty()) {
		return;
	}

	/* this essentially transfers ownership of the processor
	   of the processor from the route to the mixer
	   selection.
	*/
	
	_rr_selection.set (to_be_removed);

	no_processor_redisplay = true;
	for (vector<boost::shared_ptr<Processor> >::iterator i = to_be_removed.begin(); i != to_be_removed.end(); ++i) {
		// Do not cut processors or sends
		if (boost::dynamic_pointer_cast<PluginInsert>((*i)) != 0) {
			void* gui = (*i)->get_gui ();
		
			if (gui) {
				static_cast<Gtk::Widget*>(gui)->hide ();
			}
		
			if (_route->remove_processor (*i)) {
				/* removal failed */
				_rr_selection.remove (*i);
			}
		} else {
			_rr_selection.remove (*i);
		}

	}
	no_processor_redisplay = false;
	redisplay_processors ();
}

void
ProcessorBox::copy_processors ()
{
	vector<boost::shared_ptr<Processor> > to_be_copied;
	vector<boost::shared_ptr<Processor> > copies;

	get_selected_processors (to_be_copied);

	if (to_be_copied.empty()) {
		return;
	}

	for (vector<boost::shared_ptr<Processor> >::iterator i = to_be_copied.begin(); i != to_be_copied.end(); ++i) {
		// Do not copy processors or sends
		if (boost::dynamic_pointer_cast<PluginInsert>((*i)) != 0) {
			copies.push_back (Processor::clone (*i));
		}
  	}

	_rr_selection.set (copies);

}

void
ProcessorBox::delete_processors ()
{
	vector<boost::shared_ptr<Processor> > to_be_deleted;
	
	get_selected_processors (to_be_deleted);

	if (to_be_deleted.empty()) {
		return;
	}

	for (vector<boost::shared_ptr<Processor> >::iterator i = to_be_deleted.begin(); i != to_be_deleted.end(); ++i) {
		
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
ProcessorBox::cut_processor (boost::shared_ptr<Processor> processor)
{
	/* this essentially transfers ownership of the processor
	   of the processor from the route to the mixer
	   selection.
	*/

	_rr_selection.add (processor);
	
	void* gui = processor->get_gui ();

	if (gui) {
		static_cast<Gtk::Widget*>(gui)->hide ();
	}
	
	no_processor_redisplay = true;
	if (_route->remove_processor (processor)) {
		_rr_selection.remove (processor);
	}
	no_processor_redisplay = false;
	redisplay_processors ();
}

void
ProcessorBox::copy_processor (boost::shared_ptr<Processor> processor)
{
	boost::shared_ptr<Processor> copy = Processor::clone (processor);
	_rr_selection.add (copy);
}

void
ProcessorBox::paste_processors ()
{
	if (_rr_selection.processors.empty()) {
		return;
	}

	paste_processor_list (_rr_selection.processors);
}

void
ProcessorBox::paste_processor_list (list<boost::shared_ptr<Processor> >& processors)
{
	list<boost::shared_ptr<Processor> > copies;

	for (list<boost::shared_ptr<Processor> >::iterator i = processors.begin(); i != processors.end(); ++i) {

		boost::shared_ptr<Processor> copy = Processor::clone (*i);

		copy->set_placement (_placement);
		copies.push_back (copy);
	}

	if (_route->add_processors (copies)) {

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
	r->set_active (true);
}

void
ProcessorBox::deactivate_processor (boost::shared_ptr<Processor> r)
{
	r->set_active (false);
}

void
ProcessorBox::get_selected_processors (vector<boost::shared_ptr<Processor> >& processors)
{
    vector<Gtk::TreeModel::Path> pathlist = processor_display.get_selection()->get_selected_rows();
 
    for (vector<Gtk::TreeModel::Path>::iterator iter = pathlist.begin(); iter != pathlist.end(); ++iter) {
	    processors.push_back ((*(model->get_iter(*iter)))[columns.processor]);
    }
}

void
ProcessorBox::for_selected_processors (void (ProcessorBox::*pmf)(boost::shared_ptr<Processor>))
{
    vector<Gtk::TreeModel::Path> pathlist = processor_display.get_selection()->get_selected_rows();

	for (vector<Gtk::TreeModel::Path>::iterator iter = pathlist.begin(); iter != pathlist.end(); ++iter) {
		boost::shared_ptr<Processor> processor = (*(model->get_iter(*iter)))[columns.processor];
		(this->*pmf)(processor);
	}
}

void
ProcessorBox::clone_processors ()
{
	RouteSelection& routes (_rr_selection.routes);

	if (!routes.empty()) {
		if (_route->copy_processors (*routes.front(), _placement)) {
			string msg = _(
"Copying the set of processors on the clipboard failed,\n\
probably because the I/O configuration of the plugins\n\
could not match the configuration of this track.");
			MessageDialog am (msg);
			am.run ();
		}
	}
}

void
ProcessorBox::all_processors_active (bool state)
{
	_route->all_processors_active (_placement, state);
}

void
ProcessorBox::all_plugins_active (bool state)
{
	if (state) {
		// XXX not implemented
	} else {
		_route->disable_plugins (_placement);
	}
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
	boost::shared_ptr<PluginInsert> plugin_processor;
	boost::shared_ptr<PortInsert> port_processor;

	if (boost::dynamic_pointer_cast<AudioTrack>(_route) != 0) {

		if (boost::dynamic_pointer_cast<AudioTrack> (_route)->freeze_state() == AudioTrack::Frozen) {
			return;
		}
	}
	
	if ((send = boost::dynamic_pointer_cast<Send> (send)) != 0) {
		
		if (!_session.engine().connected()) {
			return;
		}

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
		
		if (send_ui->is_visible()) {
			send_ui->get_window()->raise ();
		} else {
			send_ui->show_all ();
			send_ui->present ();
		}
		
	} else if ((plugin_processor = boost::dynamic_pointer_cast<PluginInsert> (processor)) != 0) {
			
			ARDOUR::PluginType type = plugin_processor->type();

			if (type == ARDOUR::LADSPA || type == ARDOUR::VST) {
				PluginUIWindow *plugin_ui;
			
				if (plugin_processor->get_gui() == 0) {
								
					plugin_ui = new PluginUIWindow (plugin_processor);

					if (_owner_is_mixer) {
						ARDOUR_UI::instance()->the_mixer()->ensure_float (*plugin_ui);
					} else {
						ARDOUR_UI::instance()->the_editor().ensure_float (*plugin_ui);
					}

					WindowTitle title(Glib::get_application_name());
					title += generate_processor_title (plugin_processor);
					plugin_ui->set_title (title.get_string());

					plugin_processor->set_gui (plugin_ui);
					
					// change window title when route name is changed
					_route->NameChanged.connect (bind (mem_fun(*this, &ProcessorBox::route_name_changed), plugin_ui, boost::weak_ptr<PluginInsert> (plugin_processor)));
					
				
				} else {
					plugin_ui = reinterpret_cast<PluginUIWindow *> (plugin_processor->get_gui());
				}
			
				if (plugin_ui->is_visible()) {
					plugin_ui->get_window()->raise ();
				} else {
					plugin_ui->show_all ();
					plugin_ui->present ();
				}
#ifdef HAVE_AUDIOUNIT
			} else if (type == ARDOUR::AudioUnit) {
				AUPluginUI* plugin_ui;
				if (plugin_processor->get_gui() == 0) {
					plugin_ui = new AUPluginUI (plugin_processor);
				} else {
					plugin_ui = reinterpret_cast<AUPluginUI*> (plugin_processor->get_gui());
				}
				
				if (plugin_ui->is_visible()) {
					plugin_ui->get_window()->raise ();
				} else {
					plugin_ui->show_all ();
					plugin_ui->present ();
				}
#endif				
			} else {
				warning << "Unsupported plugin sent to ProcessorBox::edit_processor()" << endmsg;
				return;
			}

	} else if ((port_processor = boost::dynamic_pointer_cast<PortInsert> (processor)) != 0) {

		if (!_session.engine().connected()) {
			MessageDialog msg ( _("Not connected to JACK - no I/O changes are possible"));
			msg.run ();
			return;
		}

		PortInsertWindow *io_selector;

		if (port_processor->get_gui() == 0) {
			io_selector = new PortInsertWindow (_session, port_processor);
			port_processor->set_gui (io_selector);

		} else {
			io_selector = reinterpret_cast<PortInsertWindow *> (port_processor->get_gui());
		}

		if (io_selector->is_visible()) {
			io_selector->get_window()->raise ();
		} else {
			io_selector->show_all ();
			io_selector->present ();
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
	Glib::RefPtr<Gtk::ActionGroup> popup_act_grp = Gtk::ActionGroup::create(X_("redirectmenu"));
	Glib::RefPtr<Action> act;

	/* new stuff */
	ActionManager::register_action (popup_act_grp, X_("newplugin"), _("New Plugin ..."),  sigc::ptr_fun (ProcessorBox::rb_choose_plugin));

	act = ActionManager::register_action (popup_act_grp, X_("newprocessor"), _("New Insert"),  sigc::ptr_fun (ProcessorBox::rb_choose_processor));
	ActionManager::jack_sensitive_actions.push_back (act);
	act = ActionManager::register_action (popup_act_grp, X_("newsend"), _("New Send ..."),  sigc::ptr_fun (ProcessorBox::rb_choose_send));
	ActionManager::jack_sensitive_actions.push_back (act);

	ActionManager::register_action (popup_act_grp, X_("clear"), _("Clear"),  sigc::ptr_fun (ProcessorBox::rb_clear));

	/* standard editing stuff */
	act = ActionManager::register_action (popup_act_grp, X_("cut"), _("Cut"),  sigc::ptr_fun (ProcessorBox::rb_cut));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);
	act = ActionManager::register_action (popup_act_grp, X_("copy"), _("Copy"),  sigc::ptr_fun (ProcessorBox::rb_copy));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);

	act = ActionManager::register_action (popup_act_grp, X_("delete"), _("Delete"),  sigc::ptr_fun (ProcessorBox::rb_delete));
	ActionManager::plugin_selection_sensitive_actions.push_back(act); // ??

	paste_action = ActionManager::register_action (popup_act_grp, X_("paste"), _("Paste"),  sigc::ptr_fun (ProcessorBox::rb_paste));
	act = ActionManager::register_action (popup_act_grp, X_("rename"), _("Rename"),  sigc::ptr_fun (ProcessorBox::rb_rename));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);
	ActionManager::register_action (popup_act_grp, X_("selectall"), _("Select All"),  sigc::ptr_fun (ProcessorBox::rb_select_all));
	ActionManager::register_action (popup_act_grp, X_("deselectall"), _("Deselect All"),  sigc::ptr_fun (ProcessorBox::rb_deselect_all));
		
	/* activation */
	act = ActionManager::register_action (popup_act_grp, X_("activate"), _("Activate"),  sigc::ptr_fun (ProcessorBox::rb_activate));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);
	act = ActionManager::register_action (popup_act_grp, X_("deactivate"), _("Deactivate"),  sigc::ptr_fun (ProcessorBox::rb_deactivate));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);
	ActionManager::register_action (popup_act_grp, X_("activate_all"), _("Activate all"),  sigc::ptr_fun (ProcessorBox::rb_activate_all));
	ActionManager::register_action (popup_act_grp, X_("deactivate_all"), _("Deactivate all"),  sigc::ptr_fun (ProcessorBox::rb_deactivate_all));

	ActionManager::register_action (popup_act_grp, X_("a_b_plugins"), _("A/B plugins"),  sigc::ptr_fun (ProcessorBox::rb_ab_plugins));
	ActionManager::register_action (popup_act_grp, X_("deactivate_plugins"), _("Deactivate plugins"),  sigc::ptr_fun (ProcessorBox::rb_deactivate_plugins));

	/* show editors */
	act = ActionManager::register_action (popup_act_grp, X_("edit"), _("Edit"),  sigc::ptr_fun (ProcessorBox::rb_edit));
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
ProcessorBox::rb_choose_processor ()
{
	if (_current_processor_box == 0) {
		return;
	}
	_current_processor_box->choose_processor ();
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
ProcessorBox::rb_deactivate_plugins ()
{
	if (_current_processor_box == 0) {
		return;
	}
	_current_processor_box->all_plugins_active (false);
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
ProcessorBox::rb_edit ()
{
	if (_current_processor_box == 0) {
		return;
	}

	_current_processor_box->for_selected_processors (&ProcessorBox::edit_processor);
}

void
ProcessorBox::route_name_changed (PluginUIWindow* plugin_ui, boost::weak_ptr<PluginInsert> wpi)
{
	ENSURE_GUI_THREAD(bind (mem_fun (*this, &ProcessorBox::route_name_changed), plugin_ui, wpi));
	boost::shared_ptr<PluginInsert> pi (wpi.lock());
	

	if (pi) {
		WindowTitle title(Glib::get_application_name());
		title += generate_processor_title (pi);
		plugin_ui->set_title (title.get_string());
	}
}

string 
ProcessorBox::generate_processor_title (boost::shared_ptr<PluginInsert> pi)
{
	string maker = pi->plugin()->maker();
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

