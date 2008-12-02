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
#include <ardour/insert.h>
#include <ardour/ladspa_plugin.h>
#include <ardour/connection.h>
#include <ardour/session_connection.h>
#include <ardour/profile.h>

#include "ardour_ui.h"
#include "ardour_dialog.h"
#include "public_editor.h"
#include "redirect_box.h"
#include "keyboard.h"
#include "plugin_selector.h"
#include "route_redirect_selection.h"
#include "mixer_ui.h"
#include "actions.h"
#include "plugin_ui.h"
#include "send_ui.h"
#include "io_selector.h"
#include "utils.h"
#include "gui_thread.h"

#include "i18n.h"

#ifdef HAVE_AUDIOUNITS
class AUPluginUI;
#endif

using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;

RedirectBox* RedirectBox::_current_redirect_box = 0;
RefPtr<Action> RedirectBox::paste_action;
bool RedirectBox::get_colors = true;
Gdk::Color* RedirectBox::active_redirect_color;
Gdk::Color* RedirectBox::inactive_redirect_color;

RedirectBox::RedirectBox (Placement pcmnt, Session& sess, PluginSelector &plugsel, 
			  RouteRedirectSelection& rsel, bool owner_is_mixer)
	: _session(sess), 
	  _owner_is_mixer (owner_is_mixer), 
	  _placement(pcmnt), 
	  _plugin_selector(plugsel),
	  _rr_selection(rsel)
{
	if (get_colors) {
		active_redirect_color = new Gdk::Color;
		inactive_redirect_color = new Gdk::Color;
		set_color (*active_redirect_color, rgba_from_style ("RedirectSelector", 0xff, 0, 0, 0, "fg", Gtk::STATE_ACTIVE, false ));
		set_color (*inactive_redirect_color, rgba_from_style ("RedirectSelector", 0xff, 0, 0, 0, "fg", Gtk::STATE_NORMAL, false ));
		get_colors = false;
	}

	_width = Wide;
	redirect_menu = 0;
	send_action_menu = 0;
	redirect_drag_in_progress = false;
	no_redirect_redisplay = false;
	ignore_delete = false;

	model = ListStore::create(columns);

	RefPtr<TreeSelection> selection = redirect_display.get_selection();
	selection->set_mode (Gtk::SELECTION_MULTIPLE);
	selection->signal_changed().connect (mem_fun (*this, &RedirectBox::selection_changed));

	redirect_display.set_model (model);
	redirect_display.append_column (X_("notshown"), columns.text);
	redirect_display.set_name ("RedirectSelector");
	redirect_display.set_headers_visible (false);
	redirect_display.set_reorderable (true);
	redirect_display.set_size_request (-1, 40);
	redirect_display.get_column(0)->set_sizing(TREE_VIEW_COLUMN_FIXED);
	redirect_display.get_column(0)->set_fixed_width(48);
	redirect_display.add_object_drag (columns.redirect.index(), "redirects");
	redirect_display.signal_drop.connect (mem_fun (*this, &RedirectBox::object_drop));

	TreeViewColumn* name_col = redirect_display.get_column(0);
	CellRendererText* renderer = dynamic_cast<CellRendererText*>(redirect_display.get_column_cell_renderer (0));
	name_col->add_attribute(renderer->property_foreground_gdk(), columns.color);

	redirect_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	
	model->signal_row_deleted().connect (mem_fun (*this, &RedirectBox::row_deleted));

	redirect_scroller.add (redirect_display);
	redirect_eventbox.add (redirect_scroller);
	
	redirect_scroller.set_size_request (-1, 40);

	pack_start (redirect_eventbox, true, true);

	redirect_eventbox.signal_enter_notify_event().connect (bind (sigc::ptr_fun (RedirectBox::enter_box), this));

	redirect_display.signal_button_press_event().connect (mem_fun(*this, &RedirectBox::redirect_button_press_event), false);
	redirect_display.signal_button_release_event().connect (mem_fun(*this, &RedirectBox::redirect_button_release_event));
}

RedirectBox::~RedirectBox ()
{
}

void
RedirectBox::set_route (boost::shared_ptr<Route> r)
{
	connections.clear ();

	_route = r;

	connections.push_back (_route->redirects_changed.connect (mem_fun(*this, &RedirectBox::redisplay_redirects)));
	connections.push_back (_route->GoingAway.connect (mem_fun (*this, &RedirectBox::route_going_away)));

	redisplay_redirects (0);
}

void
RedirectBox::route_going_away ()
{
	/* don't keep updating display as redirects are deleted */
	no_redirect_redisplay = true;
}

void
RedirectBox::object_drop (const list<boost::shared_ptr<Redirect> >& redirects)
{
	paste_redirect_list (redirects);
}

void
RedirectBox::update()
{
	redisplay_redirects (0);
}

void
RedirectBox::set_width (Width w)
{
	if (_width == w) {
		return;
	}
	_width = w;
	if (w == -1) {
		abort ();
	}
	redisplay_redirects (0);
}

void
RedirectBox::remove_redirect_gui (boost::shared_ptr<Redirect> redirect)
{
	boost::shared_ptr<Insert> insert;
	boost::shared_ptr<Send> send;
	boost::shared_ptr<PortInsert> port_insert;

	if ((insert = boost::dynamic_pointer_cast<Insert> (redirect)) != 0) {

		if ((port_insert = boost::dynamic_pointer_cast<PortInsert> (insert)) != 0) {
			PortInsertUI *io_selector = reinterpret_cast<PortInsertUI *> (port_insert->get_gui());
			port_insert->set_gui (0);
			delete io_selector;
		} 

	} else if ((send = boost::dynamic_pointer_cast<Send> (insert)) != 0) {
		SendUIWindow *sui = reinterpret_cast<SendUIWindow*> (send->get_gui());
		send->set_gui (0);
		delete sui;
	}
}

void 
RedirectBox::build_send_action_menu ()

{
	using namespace Menu_Helpers;

	send_action_menu = new Menu;
	send_action_menu->set_name ("ArdourContextMenu");
	MenuList& items = send_action_menu->items();

	items.push_back (MenuElem (_("New send"), mem_fun(*this, &RedirectBox::new_send)));
	items.push_back (MenuElem (_("Show send controls"), mem_fun(*this, &RedirectBox::show_send_controls)));
}

void
RedirectBox::show_send_controls ()

{
}

void
RedirectBox::new_send ()

{
}

void
RedirectBox::show_redirect_menu (gint arg)
{
	if (redirect_menu == 0) {
		redirect_menu = build_redirect_menu ();
	}

	Gtk::MenuItem* plugin_menu_item = dynamic_cast<Gtk::MenuItem*>(ActionManager::get_widget("/redirectmenu/newplugin"));

	if (plugin_menu_item) {
		plugin_menu_item->set_submenu (_plugin_selector.plugin_menu());
	}

	paste_action->set_sensitive (!_rr_selection.redirects.empty());

	redirect_menu->popup (1, arg);
}

void
RedirectBox::redirect_drag_begin (GdkDragContext *context)
{
	redirect_drag_in_progress = true;
}

void
RedirectBox::redirect_drag_end (GdkDragContext *context)
{
	redirect_drag_in_progress = false;
}

bool
RedirectBox::redirect_button_press_event (GdkEventButton *ev)
{
	TreeIter iter;
	TreeModel::Path path;
	TreeViewColumn* column;
	int cellx;
	int celly;
	boost::shared_ptr<Redirect> redirect;
	int ret = false;
	bool selected = false;

	if (redirect_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		if ((iter = model->get_iter (path))) {
			redirect = (*iter)[columns.redirect];
			selected = redirect_display.get_selection()->is_selected (iter);
		}
		
	}

	if (redirect && (Keyboard::is_edit_event (ev) || (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS))) {
		
		if (_session.engine().connected()) {
			/* XXX giving an error message here is hard, because we may be in the midst of a button press */
			edit_redirect (redirect);
		}
		ret = true;
		
	} else if (redirect && ev->button == 1 && selected) {

		// this is purely informational but necessary
		RedirectSelected (redirect); // emit

	} else if (!redirect && ev->button == 1 && ev->type == GDK_2BUTTON_PRESS) {

		choose_plugin ();
		_plugin_selector.show_manager ();

	}
	
	return ret;
}

bool
RedirectBox::redirect_button_release_event (GdkEventButton *ev)
{
	TreeIter iter;
	TreeModel::Path path;
	TreeViewColumn* column;
	int cellx;
	int celly;
	boost::shared_ptr<Redirect> redirect;
	int ret = false;


	if (redirect_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		if ((iter = model->get_iter (path))) {
			redirect = (*iter)[columns.redirect];
		}
	}

	if (redirect && Keyboard::is_delete_event (ev)) {
		
		Glib::signal_idle().connect (bind (mem_fun(*this, &RedirectBox::idle_delete_redirect), boost::weak_ptr<Redirect>(redirect)));
		ret = true;
		
	} else if (Keyboard::is_context_menu_event (ev)) {

		show_redirect_menu(ev->time);
		ret = true;

	} else if (redirect && Keyboard::is_button2_event (ev) && (Keyboard::no_modifier_keys_pressed (ev) && ((ev->state & Gdk::BUTTON2_MASK) == Gdk::BUTTON2_MASK))) {
		
		/* button2-click with no modifiers */

		redirect->set_active (!redirect->active(), this);
		ret = true;

	} 

	return ret;
}

Menu *
RedirectBox::build_redirect_menu ()
{
	redirect_menu = dynamic_cast<Gtk::Menu*>(ActionManager::get_widget("/redirectmenu") );
	redirect_menu->set_name ("ArdourContextMenu");

	show_all_children();

	return redirect_menu;
}

void
RedirectBox::selection_changed ()
{
	bool sensitive = (redirect_display.get_selection()->count_selected_rows()) ? true : false;
	ActionManager::set_sensitive (ActionManager::plugin_selection_sensitive_actions, sensitive);
}

void
RedirectBox::select_all_redirects ()
{
	redirect_display.get_selection()->select_all();
}

void
RedirectBox::deselect_all_redirects ()
{
	redirect_display.get_selection()->unselect_all();
}

void
RedirectBox::choose_plugin ()
{
	_plugin_selector.set_interested_object (*this);
}

void
RedirectBox::use_plugins (const SelectedPlugins& plugins)
{
	for (SelectedPlugins::const_iterator p = plugins.begin(); p != plugins.end(); ++p) {

		boost::shared_ptr<Redirect> redirect (new PluginInsert (_session, *p, _placement));

		uint32_t err_streams;

		if (Config->get_new_plugins_active()) {
			redirect->set_active (true, this);
		}
		
		if (_route->add_redirect (redirect, this, &err_streams)) {
			weird_plugin_dialog (**p, err_streams, _route);
		} else {
			
			if (Profile->get_sae()) {
				redirect->set_active (true, 0);
			}
			redirect->active_changed.connect (bind (mem_fun (*this, &RedirectBox::show_redirect_active_r), boost::weak_ptr<Redirect>(redirect)));
		}
	}
}

void
RedirectBox::weird_plugin_dialog (Plugin& p, uint32_t streams, boost::shared_ptr<IO> io)
{
	ArdourDialog dialog (_("ardour: weird plugin dialog"));
	Label label;

	/* i hate this kind of code */

	if (streams > (unsigned)p.get_info()->n_inputs) {
		label.set_text (string_compose (_(
"You attempted to add a plugin (%1).\n"
"The plugin has %2 inputs\n"
"but at the insertion point, there are\n"
"%3 active signal streams.\n"
"\n"
"This makes no sense - you are throwing away\n"
"part of the signal."),
					 p.name(),
					 p.get_info()->n_inputs,
					 streams));
	} else if (streams < (unsigned)p.get_info()->n_inputs) {
		label.set_text (string_compose (_(
"You attempted to add a plugin (%1).\n"
"The plugin has %2 inputs\n"
"but at the insertion point there are\n"
"only %3 active signal streams.\n"
"\n"
"This makes no sense - unless the plugin supports\n"
"side-chain inputs. A future version of Ardour will\n"
"support this type of configuration."),
					 p.name(),
					 p.get_info()->n_inputs,
					 streams));
	} else {
		label.set_text (string_compose (_(
"You attempted to add a plugin (%1).\n"
"\n"
"The I/O configuration doesn't make sense:\n"
"\n" 
"The plugin has %2 inputs and %3 outputs.\n"
"The track/bus has %4 inputs and %5 outputs.\n"
"The insertion point, has %6 active signals.\n"
"\n"
"Ardour does not understand what to do in such situations.\n"),
					 p.name(),
					 p.get_info()->n_inputs,
					 p.get_info()->n_outputs,
					 io->n_inputs(),
					 io->n_outputs(),
					 streams));
	}

	dialog.set_border_width (PublicEditor::window_border_width);

	label.set_alignment (0.5, 0.5);
	dialog.get_vbox()->pack_start (label);
	dialog.add_button (Stock::OK, RESPONSE_ACCEPT);

	dialog.set_name (X_("PluginIODialog"));
	dialog.set_position (Gtk::WIN_POS_MOUSE);
	dialog.set_modal (true);
	dialog.show_all ();

	dialog.run ();
}

void
RedirectBox::choose_insert ()
{
	boost::shared_ptr<Redirect> redirect (new PortInsert (_session, _placement));
	redirect->active_changed.connect (bind (mem_fun(*this, &RedirectBox::show_redirect_active_r), boost::weak_ptr<Redirect>(redirect)));
	_route->add_redirect (redirect, this);
}

void
RedirectBox::choose_send ()
{
	boost::shared_ptr<Send> send (new Send (_session, _placement));

	/* XXX need redirect lock on route */

	try {
		send->ensure_io (0, _route->max_redirect_outs(), false, this);
	} catch (AudioEngine::PortRegistrationFailure& err) {
		error << string_compose (_("Cannot set up new send: %1"), err.what()) << endmsg;
		return;
	}
	
	IOSelectorWindow *ios = new IOSelectorWindow (_session, send, false, true);
	
	ios->show_all ();

	boost::shared_ptr<Redirect> r = boost::static_pointer_cast<Redirect>(send);

	ios->selector().Finished.connect (bind (mem_fun(*this, &RedirectBox::send_io_finished), boost::weak_ptr<Redirect>(r), ios));
}

void
RedirectBox::send_io_finished (IOSelector::Result r, boost::weak_ptr<Redirect> weak_redirect, IOSelectorWindow* ios)
{
	boost::shared_ptr<Redirect> redirect (weak_redirect.lock());

	if (!redirect) {
		return;
	}

	switch (r) {
	case IOSelector::Cancelled:
		// redirect will go away when all shared_ptrs to it vanish
		break;

	case IOSelector::Accepted:
		_route->add_redirect (redirect, this);
		if (Profile->get_sae()) {
			redirect->set_active (true, 0);
		}
		break;
	}

	delete_when_idle (ios);
}

void
RedirectBox::redisplay_redirects (void *src)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &RedirectBox::redisplay_redirects), src));

	if (no_redirect_redisplay) {
		return;
	}
	
	ignore_delete = true;
	model->clear ();
	ignore_delete = false;

	redirect_active_connections.clear ();
	redirect_name_connections.clear ();

	void (RedirectBox::*pmf)(boost::shared_ptr<Redirect>) = &RedirectBox::add_redirect_to_display;
	_route->foreach_redirect (this, pmf);

	switch (_placement) {
	case PreFader:
		build_redirect_tooltip(redirect_eventbox, _("Pre-fader inserts, sends & plugins:"));
		break;
	case PostFader:
		build_redirect_tooltip(redirect_eventbox, _("Post-fader inserts, sends & plugins:"));
		break;
	}
}

void
RedirectBox::add_redirect_to_display (boost::shared_ptr<Redirect> redirect)
{
	if (redirect->placement() != _placement) {
		return;
	}
	
	Gtk::TreeModel::Row row = *(model->append());

	row[columns.text] = redirect_name (redirect);
	row[columns.redirect] = redirect;

	show_redirect_active (redirect);

	redirect_active_connections.push_back (redirect->active_changed.connect (bind (mem_fun(*this, &RedirectBox::show_redirect_active_r), boost::weak_ptr<Redirect>(redirect))));
	redirect_name_connections.push_back (redirect->name_changed.connect (bind (mem_fun(*this, &RedirectBox::show_redirect_name), boost::weak_ptr<Redirect>(redirect))));
}

string
RedirectBox::redirect_name (boost::weak_ptr<Redirect> weak_redirect)
{
	boost::shared_ptr<Redirect> redirect (weak_redirect.lock());

	if (!redirect) {
		return string();
	}

	boost::shared_ptr<Send> send;
	string name_display;

	if (!redirect->active()) {
		name_display = " (";
	}

	if ((send = boost::dynamic_pointer_cast<Send> (redirect)) != 0) {

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
			name_display += redirect->name();
			break;
		case Narrow:
			name_display += PBD::short_version (redirect->name(), 5);
			break;
		}

	}

	if (!redirect->active()) {
		name_display += ')';
	}

	return name_display;
}

void
RedirectBox::build_redirect_tooltip (EventBox& box, string start)
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
RedirectBox::show_redirect_name (void* src, boost::weak_ptr<Redirect> redirect)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &RedirectBox::show_redirect_name), src, redirect));
	show_redirect_active (redirect);
}

void
RedirectBox::show_redirect_active_r (Redirect* r, void *src, boost::weak_ptr<Redirect> weak_redirect)
{
	show_redirect_active (weak_redirect);
}

void
RedirectBox::show_redirect_active (boost::weak_ptr<Redirect> weak_redirect)
{
	boost::shared_ptr<Redirect> redirect (weak_redirect.lock());
	
	if (!redirect) {
		return;
	}

	ENSURE_GUI_THREAD(bind (mem_fun(*this, &RedirectBox::show_redirect_active), weak_redirect));
	
	Gtk::TreeModel::Children children = model->children();
	Gtk::TreeModel::Children::iterator iter = children.begin();

	while (iter != children.end()) {

		boost::shared_ptr<Redirect> r = (*iter)[columns.redirect];

		if (r == redirect) {
			
			(*iter)[columns.text] = redirect_name (r);
			
			if (redirect->active()) {
				(*iter)[columns.color] = *active_redirect_color;
			} else {
				(*iter)[columns.color] = *inactive_redirect_color;
			}
			break;
		}

		iter++;
	}
}

void
RedirectBox::row_deleted (const Gtk::TreeModel::Path& path)
{
	if (!ignore_delete) {
		compute_redirect_sort_keys ();
	}
}

void
RedirectBox::compute_redirect_sort_keys ()
{
	uint32_t sort_key = 0;
	Gtk::TreeModel::Children children = model->children();

	for (Gtk::TreeModel::Children::iterator iter = children.begin(); iter != children.end(); ++iter) {
		boost::shared_ptr<Redirect> r = (*iter)[columns.redirect];
		r->set_sort_key (sort_key);
		sort_key++;
	}

	if (_route->sort_redirects ()) {

		redisplay_redirects (0);

		/* now tell them about the problem */

		ArdourDialog dialog (_("ardour: weird plugin dialog"));
		Label label;

		label.set_text (_("\
You cannot reorder this set of redirects\n\
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
RedirectBox::rename_redirects ()
{
	vector<boost::shared_ptr<Redirect> > to_be_renamed;
	
	get_selected_redirects (to_be_renamed);

	if (to_be_renamed.empty()) {
		return;
	}

	for (vector<boost::shared_ptr<Redirect> >::iterator i = to_be_renamed.begin(); i != to_be_renamed.end(); ++i) {
		rename_redirect (*i);
	}
}

void
RedirectBox::cut_redirects ()
{
	vector<boost::shared_ptr<Redirect> > to_be_removed;
	
	get_selected_redirects (to_be_removed);

	if (to_be_removed.empty()) {
		return;
	}

	/* this essentially transfers ownership of the redirect
	   of the redirect from the route to the mixer
	   selection.
	*/
	
	_rr_selection.set (to_be_removed);

	no_redirect_redisplay = true;
	for (vector<boost::shared_ptr<Redirect> >::iterator i = to_be_removed.begin(); i != to_be_removed.end(); ++i) {
		// Do not cut inserts or sends
		if (boost::dynamic_pointer_cast<PluginInsert>((*i)) != 0) {
			void* gui = (*i)->get_gui ();
		
			if (gui) {
				static_cast<Gtk::Widget*>(gui)->hide ();
			}
		
			if (_route->remove_redirect (*i, this)) {
				/* removal failed */
				_rr_selection.remove (*i);
			}
		} else {
			_rr_selection.remove (*i);
		}

	}
	no_redirect_redisplay = false;
	redisplay_redirects (this);
}

void
RedirectBox::copy_redirects ()
{
	vector<boost::shared_ptr<Redirect> > to_be_copied;
	vector<boost::shared_ptr<Redirect> > copies;

	get_selected_redirects (to_be_copied);

	if (to_be_copied.empty()) {
		return;
	}

	for (vector<boost::shared_ptr<Redirect> >::iterator i = to_be_copied.begin(); i != to_be_copied.end(); ++i) {
		// Do not copy inserts 
		if ((boost::dynamic_pointer_cast<PluginInsert>((*i)) != 0) ||
		    (boost::dynamic_pointer_cast<Send>((*i)) != 0)) {
			copies.push_back (Redirect::clone (*i));
		}
  	}

	_rr_selection.set (copies);

}

void
RedirectBox::delete_redirects ()
{
	vector<boost::shared_ptr<Redirect> > to_be_deleted;
	
	get_selected_redirects (to_be_deleted);

	if (to_be_deleted.empty()) {
		return;
	}

	for (vector<boost::shared_ptr<Redirect> >::iterator i = to_be_deleted.begin(); i != to_be_deleted.end(); ++i) {
		
		void* gui = (*i)->get_gui ();
		
		if (gui) {
			static_cast<Gtk::Widget*>(gui)->hide ();
		}

		_route->remove_redirect( *i, this);
	}

	no_redirect_redisplay = false;
	redisplay_redirects (this);
}

gint
RedirectBox::idle_delete_redirect (boost::weak_ptr<Redirect> weak_redirect)
{
	boost::shared_ptr<Redirect> redirect (weak_redirect.lock());

	if (!redirect) {
		return false;
	}

	/* NOT copied to _mixer.selection() */

	no_redirect_redisplay = true;

	void* gui = redirect->get_gui ();
	
	if (gui) {
		static_cast<Gtk::Widget*>(gui)->hide ();
	}
	
	_route->remove_redirect (redirect, this);
	no_redirect_redisplay = false;
	redisplay_redirects (this);

	return false;
}

void
RedirectBox::rename_redirect (boost::shared_ptr<Redirect> redirect)
{
	ArdourPrompter name_prompter (true);
	string result;
	name_prompter.set_prompt (_("rename redirect"));
	name_prompter.set_initial_text (redirect->name());
	name_prompter.add_button (_("Rename"), Gtk::RESPONSE_ACCEPT);
	name_prompter.set_response_sensitive (Gtk::RESPONSE_ACCEPT, false);
	name_prompter.show_all ();

	switch (name_prompter.run ()) {

	case Gtk::RESPONSE_ACCEPT:
        name_prompter.get_result (result);
        if (result.length()) {
			redirect->set_name (result, this);
		}	
		break;
	}

	return;
}

void
RedirectBox::cut_redirect (boost::shared_ptr<Redirect> redirect)
{
	/* this essentially transfers ownership of the redirect
	   of the redirect from the route to the mixer
	   selection.
	*/

	_rr_selection.add (redirect);
	
	void* gui = redirect->get_gui ();

	if (gui) {
		static_cast<Gtk::Widget*>(gui)->hide ();
	}
	
	no_redirect_redisplay = true;
	if (_route->remove_redirect (redirect, this)) {
		_rr_selection.remove (redirect);
	}
	no_redirect_redisplay = false;
	redisplay_redirects (this);
}

void
RedirectBox::copy_redirect (boost::shared_ptr<Redirect> redirect)
{
	boost::shared_ptr<Redirect> copy = Redirect::clone (redirect);
	_rr_selection.add (copy);
}

void
RedirectBox::paste_redirects ()
{
	if (_rr_selection.redirects.empty()) {
		return;
	}

	paste_redirect_list (_rr_selection.redirects);
}

void
RedirectBox::paste_redirect_list (const list<boost::shared_ptr<Redirect> >& redirects)
{
	list<boost::shared_ptr<Redirect> > copies;

	for (list<boost::shared_ptr<Redirect> >::const_iterator i = redirects.begin(); i != redirects.end(); ++i) {

		boost::shared_ptr<Redirect> copy = Redirect::clone (*i);

		copy->set_placement (_placement, this);
		copies.push_back (copy);
	}

	if (_route->add_redirects (copies, this)) {

		string msg = _(
			"Copying the set of redirects on the clipboard failed,\n\
probably because the I/O configuration of the plugins\n\
could not match the configuration of this track.");
		MessageDialog am (msg);
		am.run ();
	}
}

void
RedirectBox::activate_redirect (boost::shared_ptr<Redirect> r)
{
	r->set_active (true, 0);
}

void
RedirectBox::deactivate_redirect (boost::shared_ptr<Redirect> r)
{
	r->set_active (false, 0);
}

void
RedirectBox::get_selected_redirects (vector<boost::shared_ptr<Redirect> >& redirects)
{
    vector<Gtk::TreeModel::Path> pathlist = redirect_display.get_selection()->get_selected_rows();
 
    for (vector<Gtk::TreeModel::Path>::iterator iter = pathlist.begin(); iter != pathlist.end(); ++iter) {
	    redirects.push_back ((*(model->get_iter(*iter)))[columns.redirect]);
    }
}

void
RedirectBox::for_selected_redirects (void (RedirectBox::*pmf)(boost::shared_ptr<Redirect>))
{
    vector<Gtk::TreeModel::Path> pathlist = redirect_display.get_selection()->get_selected_rows();

	for (vector<Gtk::TreeModel::Path>::iterator iter = pathlist.begin(); iter != pathlist.end(); ++iter) {
		boost::shared_ptr<Redirect> redirect = (*(model->get_iter(*iter)))[columns.redirect];
		(this->*pmf)(redirect);
	}
}

void
RedirectBox::clone_redirects ()
{
	RouteSelection& routes (_rr_selection.routes);

	if (!routes.empty()) {
		if (_route->copy_redirects (*routes.front(), _placement)) {
			string msg = _(
"Copying the set of redirects on the clipboard failed,\n\
probably because the I/O configuration of the plugins\n\
could not match the configuration of this track.");
			MessageDialog am (msg);
			am.run ();
		}
	}
}

void
RedirectBox::all_redirects_active (bool state)
{
	_route->all_redirects_active (_placement, state);
}

void
RedirectBox::clear_redirects ()
{
	string prompt;
	vector<string> choices;

	if (boost::dynamic_pointer_cast<AudioTrack>(_route) != 0) {
		if (_placement == PreFader) {
			prompt = _("Do you really want to remove all pre-fader redirects from this track?\n"
				   "(this cannot be undone)");
		} else {
			prompt = _("Do you really want to remove all post-fader redirects from this track?\n"
				   "(this cannot be undone)");
		}
	} else {
		if (_placement == PreFader) {
			prompt = _("Do you really want to remove all pre-fader redirects from this bus?\n"
				   "(this cannot be undone)");
		} else {
			prompt = _("Do you really want to remove all post-fader redirects from this bus?\n"
				   "(this cannot be undone)");
		}
	}

	choices.push_back (_("Cancel"));
	choices.push_back (_("Yes, remove them all"));

	Gtkmm2ext::Choice prompter (prompt, choices);

	if (prompter.run () == 1) {
		_route->clear_redirects (_placement, this);
	}
}

void
RedirectBox::edit_redirect (boost::shared_ptr<Redirect> redirect)
{
	boost::shared_ptr<Insert> insert;

	if (boost::dynamic_pointer_cast<AudioTrack>(_route) != 0) {

		if (boost::dynamic_pointer_cast<AudioTrack> (_route)->freeze_state() == AudioTrack::Frozen) {
			return;
		}
	}
	
	if ((insert = boost::dynamic_pointer_cast<Insert> (redirect)) == 0) {
		
		/* it's a send */
		
		if (!_session.engine().connected()) {
			return;
		}

		boost::shared_ptr<Send> send = boost::dynamic_pointer_cast<Send> (redirect);
		
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
		
	} else {
		
		/* it's an insert */

		boost::shared_ptr<PluginInsert> plugin_insert;
		boost::shared_ptr<PortInsert> port_insert;
		
		if ((plugin_insert = boost::dynamic_pointer_cast<PluginInsert> (insert)) != 0) {
			
			PluginUIWindow *plugin_ui;

			/* these are both allowed to be null */
			
			Container* toplevel = get_toplevel();
			Window* win = dynamic_cast<Gtk::Window*>(toplevel);
			
			if (plugin_insert->get_gui() == 0) {

				plugin_ui = new PluginUIWindow (win, plugin_insert);
				
				WindowTitle title(Glib::get_application_name());
				title += generate_redirect_title (plugin_insert);
				plugin_ui->set_title (title.get_string());
				
				plugin_insert->set_gui (plugin_ui);
				
				// change window title when route name is changed
				_route->name_changed.connect (bind (mem_fun(*this, &RedirectBox::route_name_changed), plugin_ui, boost::weak_ptr<PluginInsert> (plugin_insert)));
				
			} else {
				plugin_ui = reinterpret_cast<PluginUIWindow *> (plugin_insert->get_gui());
				plugin_ui->set_parent (win);
			}
			
			if (plugin_ui->is_visible()) {
				plugin_ui->get_window()->raise ();
			} else {
				plugin_ui->show_all ();
				plugin_ui->present ();
			}

		} else if ((port_insert = boost::dynamic_pointer_cast<PortInsert> (insert)) != 0) {
			
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
			
			if (io_selector->is_visible()) {
				io_selector->get_window()->raise ();
			} else {
				io_selector->show_all ();
				io_selector->present ();
			}
		}
	}
}

bool
RedirectBox::enter_box (GdkEventCrossing *ev, RedirectBox* rb)
{
	switch (ev->detail) {
	case GDK_NOTIFY_INFERIOR:
		break;

	case GDK_NOTIFY_VIRTUAL:
		/* fallthru */

	default:
		_current_redirect_box = rb;
	}

	return false;
}

void
RedirectBox::register_actions ()
{
	Glib::RefPtr<Gtk::ActionGroup> popup_act_grp = Gtk::ActionGroup::create(X_("redirectmenu"));
	Glib::RefPtr<Action> act;

	/* new stuff */
	ActionManager::register_action (popup_act_grp, X_("newplugin"), _("New Plugin"),  sigc::ptr_fun (RedirectBox::rb_choose_plugin));

	act = ActionManager::register_action (popup_act_grp, X_("newinsert"), _("New Insert"),  sigc::ptr_fun (RedirectBox::rb_choose_insert));
	ActionManager::jack_sensitive_actions.push_back (act);
	act = ActionManager::register_action (popup_act_grp, X_("newsend"), _("New Send ..."),  sigc::ptr_fun (RedirectBox::rb_choose_send));
	ActionManager::jack_sensitive_actions.push_back (act);

	ActionManager::register_action (popup_act_grp, X_("clear"), _("Clear"),  sigc::ptr_fun (RedirectBox::rb_clear));

	/* standard editing stuff */
	act = ActionManager::register_action (popup_act_grp, X_("cut"), _("Cut"),  sigc::ptr_fun (RedirectBox::rb_cut));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);
	act = ActionManager::register_action (popup_act_grp, X_("copy"), _("Copy"),  sigc::ptr_fun (RedirectBox::rb_copy));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);

	act = ActionManager::register_action (popup_act_grp, X_("delete"), _("Delete"),  sigc::ptr_fun (RedirectBox::rb_delete));
	ActionManager::plugin_selection_sensitive_actions.push_back(act); // ??

	paste_action = ActionManager::register_action (popup_act_grp, X_("paste"), _("Paste"),  sigc::ptr_fun (RedirectBox::rb_paste));
	act = ActionManager::register_action (popup_act_grp, X_("rename"), _("Rename"),  sigc::ptr_fun (RedirectBox::rb_rename));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);
	ActionManager::register_action (popup_act_grp, X_("selectall"), _("Select All"),  sigc::ptr_fun (RedirectBox::rb_select_all));
	ActionManager::register_action (popup_act_grp, X_("deselectall"), _("Deselect All"),  sigc::ptr_fun (RedirectBox::rb_deselect_all));
		
	/* activation */
	act = ActionManager::register_action (popup_act_grp, X_("activate"), _("Activate"),  sigc::ptr_fun (RedirectBox::rb_activate));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);
	act = ActionManager::register_action (popup_act_grp, X_("deactivate"), _("Deactivate"),  sigc::ptr_fun (RedirectBox::rb_deactivate));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);
	ActionManager::register_action (popup_act_grp, X_("activate_all"), _("Activate all"),  sigc::ptr_fun (RedirectBox::rb_activate_all));
	ActionManager::register_action (popup_act_grp, X_("deactivate_all"), _("Deactivate all"),  sigc::ptr_fun (RedirectBox::rb_deactivate_all));

	/* show editors */
	act = ActionManager::register_action (popup_act_grp, X_("edit"), _("Edit"),  sigc::ptr_fun (RedirectBox::rb_edit));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);

	ActionManager::add_action_group (popup_act_grp);


}

void
RedirectBox::rb_choose_plugin ()
{
	if (_current_redirect_box == 0) {
		return;
	}
	_current_redirect_box->choose_plugin ();
}

void
RedirectBox::rb_choose_insert ()
{
	if (_current_redirect_box == 0) {
		return;
	}
	_current_redirect_box->choose_insert ();
}

void
RedirectBox::rb_choose_send ()
{
	if (_current_redirect_box == 0) {
		return;
	}
	_current_redirect_box->choose_send ();
}

void
RedirectBox::rb_clear ()
{
	if (_current_redirect_box == 0) {
		return;
	}

	_current_redirect_box->clear_redirects ();
}

void
RedirectBox::rb_cut ()
{
	if (_current_redirect_box == 0) {
		return;
	}

	_current_redirect_box->cut_redirects ();
}

void
RedirectBox::rb_delete ()
{
	if (_current_redirect_box == 0) {
		return;
	}

	_current_redirect_box->delete_redirects ();
}

void
RedirectBox::rb_copy ()
{
	if (_current_redirect_box == 0) {
		return;
	}
	_current_redirect_box->copy_redirects ();
}

void
RedirectBox::rb_paste ()
{
	if (_current_redirect_box == 0) {
		return;
	}

	_current_redirect_box->paste_redirects ();
}

void
RedirectBox::rb_rename ()
{
	if (_current_redirect_box == 0) {
		return;
	}
	_current_redirect_box->rename_redirects ();
}

void
RedirectBox::rb_select_all ()
{
	if (_current_redirect_box == 0) {
		return;
	}

	_current_redirect_box->select_all_redirects ();
}

void
RedirectBox::rb_deselect_all ()
{
	if (_current_redirect_box == 0) {
		return;
	}

	_current_redirect_box->deselect_all_redirects ();
}

void
RedirectBox::rb_activate ()
{
	if (_current_redirect_box == 0) {
		return;
	}

	_current_redirect_box->for_selected_redirects (&RedirectBox::activate_redirect);
}

void
RedirectBox::rb_deactivate ()
{
	if (_current_redirect_box == 0) {
		return;
	}
	_current_redirect_box->for_selected_redirects (&RedirectBox::deactivate_redirect);
}

void
RedirectBox::rb_activate_all ()
{
	if (_current_redirect_box == 0) {
		return;
	}

	_current_redirect_box->all_redirects_active (true);
}

void
RedirectBox::rb_deactivate_all ()
{
	if (_current_redirect_box == 0) {
		return;
	}
	_current_redirect_box->all_redirects_active (false);
}

void
RedirectBox::rb_edit ()
{
	if (_current_redirect_box == 0) {
		return;
	}

	_current_redirect_box->for_selected_redirects (&RedirectBox::edit_redirect);
}

void
RedirectBox::route_name_changed (void* src, PluginUIWindow* plugin_ui, boost::weak_ptr<PluginInsert> wpi)
{
	ENSURE_GUI_THREAD(bind (mem_fun (*this, &RedirectBox::route_name_changed), src, plugin_ui, wpi));
	boost::shared_ptr<PluginInsert> pi (wpi.lock());
	

	if (pi) {
		WindowTitle title(Glib::get_application_name());
		title += generate_redirect_title (pi);
		plugin_ui->set_title (title.get_string());
	}
}

string 
RedirectBox::generate_redirect_title (boost::shared_ptr<PluginInsert> pi)
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

