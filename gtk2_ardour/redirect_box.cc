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

    $Id$
*/

#include <cmath>
#include <glib.h>

#include <sigc++/bind.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/choice.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/doi.h>

#include <ardour/ardour.h>
#include <ardour/session.h>
#include <ardour/audioengine.h>
#include <ardour/route.h>
#include <ardour/audio_track.h>
#include <ardour/diskstream.h>
#include <ardour/send.h>
#include <ardour/insert.h>
#include <ardour/ladspa_plugin.h>
#include <ardour/connection.h>
#include <ardour/session_connection.h>

#include "ardour_ui.h"
#include "ardour_dialog.h"
#include "ardour_message.h"
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

using namespace sigc;
using namespace ARDOUR;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;

RedirectBox* RedirectBox::_current_redirect_box = 0;


RedirectBox::RedirectBox (Placement pcmnt, Session& sess, Route& rt, PluginSelector &plugsel, 
			  RouteRedirectSelection & rsel, bool owner_is_mixer)
	: _route(rt), 
	  _session(sess), 
	  _owner_is_mixer (owner_is_mixer), 
	  _placement(pcmnt), 
	  _plugin_selector(plugsel),
	  _rr_selection(rsel)
	  //redirect_display (1)
{
	_width = Wide;
	redirect_menu = 0;
	send_action_menu = 0;
	redirect_drag_in_progress = false;
	
	model = ListStore::create(columns);

	RefPtr<TreeSelection> selection = redirect_display.get_selection();
	selection->set_mode (Gtk::SELECTION_MULTIPLE);
	selection->signal_changed().connect (mem_fun (*this, &RedirectBox::selection_changed));

	redirect_display.set_model (model);
	redirect_display.append_column (NULL, columns.text);
	redirect_display.set_name ("MixerRedirectSelector");
	redirect_display.set_headers_visible (false);
	redirect_display.set_reorderable (true);
	redirect_display.set_size_request (-1, 48);
	redirect_display.add_object_drag (columns.redirect.index(), "redirects");
	redirect_display.signal_object_drop.connect (mem_fun (*this, &RedirectBox::object_drop));

	// Does this adequately replace the drag start/stop signal handlers?
	model->signal_rows_reordered().connect (mem_fun (*this, &RedirectBox::redirects_reordered));
	redirect_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	redirect_scroller.add (redirect_display);
	redirect_eventbox.add (redirect_scroller);
	pack_start (redirect_eventbox, true, true);

	redirect_scroller.show ();
	redirect_display.show ();
	redirect_eventbox.show ();
	show_all ();

	_route.redirects_changed.connect (mem_fun(*this, &RedirectBox::redirects_changed));

	redirect_eventbox.signal_enter_notify_event().connect (bind (sigc::ptr_fun (RedirectBox::enter_box), this));
	redirect_eventbox.signal_leave_notify_event().connect (bind (sigc::ptr_fun (RedirectBox::leave_box), this));

	redirect_display.signal_button_press_event().connect (mem_fun(*this, &RedirectBox::redirect_button));
	redirect_display.signal_button_release_event().connect (mem_fun(*this, &RedirectBox::redirect_button));

	//redirect_display.signal_button_release_event().connect_after (ptr_fun (do_not_propagate));
	set_stuff_from_route ();

	/* start off as a passthru strip. we'll correct this, if necessary,
	   in update_diskstream_display().
	*/

	//set_name ("AudioTrackStripBase");

	/* now force an update of all the various elements */

	redirects_changed (0);

	//add_events (Gdk::BUTTON_RELEASE_MASK);
}

RedirectBox::~RedirectBox ()
{
//	GoingAway(); /* EMIT_SIGNAL */

}

void
RedirectBox::object_drop (string type, uint32_t cnt, void** ptr)
{
	if (type != "redirects") {
		return;
	}
}

void
RedirectBox::set_stuff_from_route ()
{
}

void
RedirectBox::set_title (const std::string & title)
{
	redirect_display.get_column(0)->set_title (title);
}

void
RedirectBox::set_title_shown (bool flag)
{
}


void
RedirectBox::update()
{
	redirects_changed(0);
}


void
RedirectBox::set_width (Width w)
{
	if (_width == w) {
		return;
	}
	_width = w;

	redirects_changed(0);
}


void
RedirectBox::remove_redirect_gui (Redirect *redirect)
{
	Insert *insert = 0;
	Send *send = 0;
	PortInsert *port_insert = 0;

	if ((insert = dynamic_cast<Insert *> (redirect)) != 0) {

		if ((port_insert = dynamic_cast<PortInsert *> (insert)) != 0) {
			PortInsertUI *io_selector = reinterpret_cast<PortInsertUI *> (port_insert->get_gui());
			port_insert->set_gui (0);
			delete io_selector;
		} 

	} else if ((send = dynamic_cast<Send *> (insert)) != 0) {
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

	redirect_menu->popup (1, 0);
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

gint
RedirectBox::redirect_button (GdkEventButton *ev)
{
	Redirect *redirect;
	TreeModel::Row row = *(redirect_display.get_selection()->get_selected());
	redirect = row[columns.redirect];

	switch (ev->type) {
	case GDK_BUTTON_PRESS:
		if (ev->button == 3) {
			show_redirect_menu (0); // Handle the context-click menu here as well
			return TRUE;
		}
		else
			return FALSE;

	case GDK_2BUTTON_PRESS:
		if (ev->state != 0) {
			return FALSE;
		}
		/* might be edit event, see below */
		break;

	case GDK_BUTTON_RELEASE:
		break;

	default:
		/* shouldn't be here, but gcc complains */
		return FALSE;
	}

	if (redirect && Keyboard::is_delete_event (ev)) {
		
		Glib::signal_idle().connect (bind (mem_fun(*this, &RedirectBox::idle_delete_redirect), redirect));
		return TRUE;

	} else if (redirect && (Keyboard::is_edit_event (ev) || ev->type == GDK_2BUTTON_PRESS)) {
		
		if (_session.engine().connected()) {
			/* XXX giving an error message here is hard, because we may be in the midst of a button press */
			edit_redirect (redirect);
		}
		return TRUE;

	} else if (Keyboard::is_context_menu_event (ev)) {
		show_redirect_menu(0);
		return TRUE; //stop_signal (*clist, "button-release-event");

	} else {
		switch (ev->button) {
		case 1:
			return FALSE;
			break;

		case 2:
			if (redirect) {
				redirect->set_active (!redirect->active(), this);
			}
			break;

		case 3:
			break;

		default:
			return FALSE;
		}
	}

	return TRUE;
}

Menu *
RedirectBox::build_redirect_menu ()
{
	redirect_menu = dynamic_cast<Gtk::Menu*>(ActionManager::get_widget("/redirectmenu") );
	redirect_menu->signal_map_event().connect (mem_fun(*this, &RedirectBox::redirect_menu_map_handler));
	redirect_menu->set_name ("ArdourContextMenu");

	show_all_children();

	return redirect_menu;
}

void
RedirectBox::selection_changed ()
{
	bool sensitive = (redirect_display.get_selection()->count_selected_rows()) ? true : false;

	for (vector<Glib::RefPtr<Gtk::Action> >::iterator i = ActionManager::plugin_selection_sensitive_actions.begin(); i != ActionManager::plugin_selection_sensitive_actions.end(); ++i) {
		(*i)->set_sensitive (sensitive);
	}
}

gint
RedirectBox::redirect_menu_map_handler (GdkEventAny *ev)
{
	// GTK2FIX
	// popup_act_grp->get_action("paste")->set_sensitive (!_rr_selection.redirects.empty());
	return FALSE;
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
	sigc::connection newplug_connection = _plugin_selector.PluginCreated.connect (mem_fun(*this,&RedirectBox::insert_plugin_chosen));
	_plugin_selector.run ();
	newplug_connection.disconnect();
}

void
RedirectBox::insert_plugin_chosen (Plugin *plugin)
{
	if (plugin) {

		Redirect *redirect = new PluginInsert (_session, *plugin, _placement);
		
		redirect->active_changed.connect (mem_fun(*this, &RedirectBox::show_redirect_active));

		uint32_t err_streams;

		if (_route.add_redirect (redirect, this, &err_streams)) {
			wierd_plugin_dialog (*plugin, err_streams, _route);
			delete redirect;
		}
	}
}

void
RedirectBox::wierd_plugin_dialog (Plugin& p, uint32_t streams, IO& io)
{
	ArdourDialog dialog ("wierd plugin dialog");
	Label label;

	/* i hate this kind of code */

	if (streams > p.get_info().n_inputs) {
		label.set_text (string_compose (_(
"You attempted to add a plugin (%1).\n"
"The plugin has %2 inputs\n"
"but at the insertion point, there are\n"
"%3 active signal streams.\n"
"\n"
"This makes no sense - you are throwing away\n"
"part of the signal."),
					 p.name(),
					 p.get_info().n_inputs,
					 streams));
	} else if (streams < p.get_info().n_inputs) {
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
					 p.get_info().n_inputs,
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
					 p.get_info().n_inputs,
					 p.get_info().n_outputs,
					 io.n_inputs(),
					 io.n_outputs(),
					 streams));
	}

	dialog.get_vbox()->pack_start (label);
	dialog.add_button (Stock::OK, RESPONSE_ACCEPT);

	dialog.set_name (X_("PluginIODialog"));
	dialog.set_position (Gtk::WIN_POS_MOUSE);
	dialog.set_modal (true);
	dialog.show_all ();

	// GTK2FIX
	//dialog.realize();
	//dialog.get_window()->set_decorations (Gdk::WMDecoration (GDK_DECOR_BORDER|GDK_DECOR_RESIZEH));

	dialog.run ();
}

void
RedirectBox::choose_insert ()
{
	Redirect *redirect = new PortInsert (_session, _placement);
	redirect->active_changed.connect (mem_fun(*this, &RedirectBox::show_redirect_active));
	_route.add_redirect (redirect, this);
}

void
RedirectBox::choose_send ()
{
	Send *send = new Send (_session, _placement);

	/* XXX need redirect lock on route */

	send->ensure_io (0, _route.max_redirect_outs(), false, this);
	
	IOSelectorWindow *ios = new IOSelectorWindow (_session, *send, false, true);
	
	ios->show_all ();
	ios->selector().Finished.connect (bind (mem_fun(*this, &RedirectBox::send_io_finished), static_cast<Redirect*>(send), ios));
}

void
RedirectBox::send_io_finished (IOSelector::Result r, Redirect* redirect, IOSelectorWindow* ios)
{
	switch (r) {
	case IOSelector::Cancelled:
		delete redirect;
		break;

	case IOSelector::Accepted:
		_route.add_redirect (redirect, this);
		break;
	}

	delete_when_idle (ios);
}

void
RedirectBox::redirects_changed (void *src)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &RedirectBox::redirects_changed), src));
	
	//redirect_display.freeze ();
	model->clear ();
	redirect_active_connections.clear ();
	redirect_name_connections.clear ();

	_route.foreach_redirect (this, &RedirectBox::add_redirect_to_display);

	switch (_placement) {
	case PreFader:
		build_redirect_tooltip(redirect_eventbox, _("Pre-fader inserts, sends & plugins:"));
		break;
	case PostFader:
		build_redirect_tooltip(redirect_eventbox, _("Post-fader inserts, sends & plugins:"));
		break;
	}
	//redirect_display.thaw ();
}

void
RedirectBox::add_redirect_to_display (Redirect *redirect)
{
	if (redirect->placement() != _placement) {
		return;
	}
	
	Gtk::TreeModel::Row row = *(model->append());
	row[columns.text] = redirect_name (*redirect);
	row[columns.redirect] = redirect;
	
	show_redirect_active (redirect, this);

	redirect_active_connections.push_back (redirect->active_changed.connect (mem_fun(*this, &RedirectBox::show_redirect_active)));
	redirect_name_connections.push_back (redirect->name_changed.connect (bind (mem_fun(*this, &RedirectBox::show_redirect_name), redirect)));
}

string
RedirectBox::redirect_name (Redirect& redirect)
{
	Send *send;
	string name_display;

	if (!redirect.active()) {
		name_display = " (";
	}

	if ((send = dynamic_cast<Send *> (&redirect)) != 0) {

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
			name_display += short_version (send->name().substr (lbracket+1, lbracket-rbracket-1), 4);
			break;
		}

	} else {

		switch (_width) {
		case Wide:
			name_display += redirect.name();
			break;
		case Narrow:
			name_display += short_version (redirect.name(), 5);
			break;
		}

	}

	if (!redirect.active()) {
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
RedirectBox::show_redirect_name (void* src, Redirect *redirect)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &RedirectBox::show_redirect_name), src, redirect));
	
	show_redirect_active (redirect, src);
}

void
RedirectBox::show_redirect_active (Redirect *redirect, void *src)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &RedirectBox::show_redirect_active), redirect, src));

	Gtk::TreeModel::Children children = model->children();
	Gtk::TreeModel::Children::iterator iter = children.begin();

	while( iter != children.end())
	{
		if ((*iter)[columns.redirect] == redirect)
			break;
		iter++;
	}

	(*iter)[columns.text] = redirect_name (*redirect);

	if (redirect->active()) {
		redirect_display.get_selection()->select (iter);
	} else {
		redirect_display.get_selection()->unselect (iter);
	}
}

void
RedirectBox::redirects_reordered (const TreeModel::Path& path,const TreeModel::iterator& iter ,int* hmm)
{
	/* this is called before the reorder has been done, so just queue
	   something for idle time.
	*/

	Glib::signal_idle().connect (mem_fun(*this, &RedirectBox::compute_redirect_sort_keys));
}

gint
RedirectBox::compute_redirect_sort_keys ()
{
	uint32_t sort_key = 0;
	Gtk::TreeModel::Children children = model->children();

	for (Gtk::TreeModel::Children::iterator iter = children.begin(); iter != children.end(); ++iter) {
		Redirect *redirect = (*iter)[columns.redirect];
		redirect->set_sort_key (sort_key, this);
		sort_key++;
	}

	if (_route.sort_redirects ()) {

		redirects_changed (0);

		/* now tell them about the problem */

		ArdourDialog dialog ("wierd plugin dialog");
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

		// GTK2FIX
		//dialog.realize();
		//dialog.get_window()->set_decorations (Gdk::WMDecoration (GDK_DECOR_BORDER|GDK_DECOR_RESIZEH));
		
		dialog.run ();
	}

	return FALSE;
}

void
RedirectBox::rename_redirects ()
{
	vector<Redirect*> to_be_renamed;
	
	get_selected_redirects (to_be_renamed);

	if (to_be_renamed.empty()) {
		return;
	}

	for (vector<Redirect*>::iterator i = to_be_renamed.begin(); i != to_be_renamed.end(); ++i) {
		rename_redirect (*i);
	}
}

void
RedirectBox::cut_redirects ()
{
	vector<Redirect*> to_be_removed;
	
	get_selected_redirects (to_be_removed);

	if (to_be_removed.empty()) {
		return;
	}

	/* this essentially transfers ownership of the redirect
	   of the redirect from the route to the mixer
	   selection.
	*/
	
	_rr_selection.set (to_be_removed);

	for (vector<Redirect*>::iterator i = to_be_removed.begin(); i != to_be_removed.end(); ++i) {
		
		void* gui = (*i)->get_gui ();
		
		if (gui) {
			static_cast<Gtk::Widget*>(gui)->hide ();
		}
		
		if (_route.remove_redirect (*i, this)) {
			/* removal failed */
			_rr_selection.remove (*i);
		}

	}
}

void
RedirectBox::copy_redirects ()
{
	vector<Redirect*> to_be_copied;
	vector<Redirect*> copies;

	get_selected_redirects (to_be_copied);

	if (to_be_copied.empty()) {
		return;
	}

	for (vector<Redirect*>::iterator i = to_be_copied.begin(); i != to_be_copied.end(); ++i) {
		copies.push_back (Redirect::clone (**i));
  	}

	_rr_selection.set (copies);
}

gint
RedirectBox::idle_delete_redirect (Redirect *redirect)
{
	/* NOT copied to _mixer.selection() */

	if (_route.remove_redirect (redirect, this)) {
		/* removal failed */
		return FALSE;
	}

	delete redirect;
	return FALSE;
}

void
RedirectBox::rename_redirect (Redirect* redirect)
{
	ArdourDialog dialog (_("ardour: rename redirect"), true);
	Entry  entry;
	VBox   vbox;
	HBox   hbox;
	Button ok_button (_("OK"));
	Button cancel_button (_("Cancel"));

	dialog.set_name ("RedirectRenameWindow");
	dialog.set_size_request (300, -1);
	dialog.set_position (Gtk::WIN_POS_MOUSE);

	dialog.add_action_widget (entry, RESPONSE_ACCEPT);
	dialog.add_button (Stock::OK, RESPONSE_ACCEPT);
	dialog.add_button (Stock::CANCEL, RESPONSE_CANCEL);
	
	entry.set_name ("RedirectNameDisplay");
	entry.set_text (redirect->name());
	entry.select_region (0, -1);
	entry.grab_focus ();

	switch (dialog.run ()) {
	case RESPONSE_ACCEPT:
		redirect->set_name (entry.get_text(), this);
		break;
	default:
		break;
	}
}

void
RedirectBox::cut_redirect (Redirect *redirect)
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
	
	if (_route.remove_redirect (redirect, this)) {
		_rr_selection.remove (redirect);
	}
}

void
RedirectBox::copy_redirect (Redirect *redirect)
{
	Redirect* copy = Redirect::clone (*redirect);
	_rr_selection.add (copy);
}

void
RedirectBox::paste_redirects ()
{
	if (_rr_selection.redirects.empty()) {
		return;
	}

	RedirectSelection& sel (_rr_selection.redirects);
	list<Redirect*> others;

	for (list<Redirect*>::iterator i = sel.begin(); i != sel.end(); ++i) {

		Redirect* copy = Redirect::clone (**i);

		copy->set_placement (_placement, this);
		others.push_back (copy);
	}

	if (_route.add_redirects (others, this)) {
		for (list<Redirect*>::iterator i = others.begin(); i != others.end(); ++i) {
			delete *i;
		}

		string msg = _(
			"Copying the set of redirects on the clipboard failed,\n\
probably because the I/O configuration of the plugins\n\
could not match the configuration of this track.");
		ArdourMessage am (0, X_("bad redirect copy dialog"), msg);
	}
}

void
RedirectBox::activate_redirect (Redirect *r)
{
	r->set_active (true, 0);
}

void
RedirectBox::deactivate_redirect (Redirect *r)
{
	r->set_active (false, 0);
}

void
RedirectBox::get_selected_redirects (vector<Redirect*>& redirects)
{
    vector<Gtk::TreeModel::Path> pathlist = redirect_display.get_selection()->get_selected_rows();
 
	for (vector<Gtk::TreeModel::Path>::iterator iter = pathlist.begin(); iter != pathlist.end(); ++iter)
		redirects.push_back ((*(model->get_iter(*iter)))[columns.redirect]);
}

void
RedirectBox::for_selected_redirects (void (RedirectBox::*pmf)(Redirect*))
{
    vector<Gtk::TreeModel::Path> pathlist = redirect_display.get_selection()->get_selected_rows();

	for (vector<Gtk::TreeModel::Path>::iterator iter = pathlist.begin(); iter != pathlist.end(); ++iter) {
		Redirect* redirect = (*(model->get_iter(*iter)))[columns.redirect];
		(this->*pmf)(redirect);
	}
}

void
RedirectBox::clone_redirects ()
{
	RouteSelection& routes (_rr_selection.routes);

	if (!routes.empty()) {
		if (_route.copy_redirects (*routes.front(), _placement)) {
			string msg = _(
"Copying the set of redirects on the clipboard failed,\n\
probably because the I/O configuration of the plugins\n\
could not match the configuration of this track.");
			ArdourMessage am (0, X_("bad redirect copy dialog"), msg);
		}
	}
}

void
RedirectBox::all_redirects_active (bool state)
{
	_route.all_redirects_active (state);
}

void
RedirectBox::clear_redirects()
{
	string prompt;
	vector<string> choices;

	if (dynamic_cast<AudioTrack*>(&_route) != 0) {
		prompt = _("Do you really want to remove all redirects from this track?\n"
			   "(this cannot be undone)");
	} else {
		prompt = _("Do you really want to remove all redirects from this bus?\n"
			   "(this cannot be undone)");
	}

	choices.push_back (_("Yes, remove them all"));
	choices.push_back (_("Cancel"));

	Gtkmm2ext::Choice prompter (prompt, choices);

	prompter.chosen.connect(sigc::ptr_fun(Gtk::Main::quit));
	prompter.show_all ();

	Gtk::Main::run ();

	if (prompter.get_choice() == 0) {
		_route.clear_redirects (this);
	}
}


void
RedirectBox::edit_redirect (Redirect* redirect)
{
	Insert *insert;

	if (dynamic_cast<AudioTrack*>(&_route) != 0) {

		if (dynamic_cast<AudioTrack*> (&_route)->freeze_state() == AudioTrack::Frozen) {
			return;
		}
	}
	
	if ((insert = dynamic_cast<Insert *> (redirect)) == 0) {
		
		/* its a send */
		
		if (!_session.engine().connected()) {
			return;
		}

		Send *send = dynamic_cast<Send*> (redirect);
		
		SendUIWindow *send_ui;
		
		if (send->get_gui() == 0) {
			
			string title;
			title = string_compose(_("ardour: %1"), send->name());	
			
			send_ui = new SendUIWindow (*send, _session);
			send_ui->set_title (title);
			send->set_gui (send_ui);
			
		} else {
			send_ui = reinterpret_cast<SendUIWindow *> (send->get_gui());
		}
		
		if (send_ui->is_visible()) {
			send_ui->get_window()->raise ();
		} else {
			send_ui->show_all ();
		}
		
	} else {
		
		/* its an insert */
		
		PluginInsert *plugin_insert;
		PortInsert *port_insert;
		
		if ((plugin_insert = dynamic_cast<PluginInsert *> (insert)) != 0) {
			
			PluginUIWindow *plugin_ui;
			
			if (plugin_insert->get_gui() == 0) {
				
				string title;
				string maker = plugin_insert->plugin().maker();
				string::size_type email_pos;
				
				if ((email_pos = maker.find_first_of ('<')) != string::npos) {
					maker = maker.substr (0, email_pos - 1);
				}
				
				if (maker.length() > 32) {
					maker = maker.substr (0, 32);
					maker += " ...";
				}

				title = string_compose(_("ardour: %1: %2 (by %3)"), _route.name(), plugin_insert->name(), maker);	
				
				plugin_ui = new PluginUIWindow (_session.engine(), *plugin_insert);
				if (_owner_is_mixer) {
					ARDOUR_UI::instance()->the_mixer()->ensure_float (*plugin_ui);
				} else {
					ARDOUR_UI::instance()->the_editor().ensure_float (*plugin_ui);
				}
				plugin_ui->set_title (title);
				plugin_insert->set_gui (plugin_ui);
				
			} else {
				plugin_ui = reinterpret_cast<PluginUIWindow *> (plugin_insert->get_gui());
			}
			
			if (plugin_ui->is_visible()) {
				plugin_ui->get_window()->raise ();
			} else {
				plugin_ui->show_all ();
			}
			
		} else if ((port_insert = dynamic_cast<PortInsert *> (insert)) != 0) {
			
			if (!_session.engine().connected()) {
				ArdourMessage msg (NULL, "nojackdialog", _("Not connected to JACK - no I/O changes are possible"));
				return;
			}

			PortInsertWindow *io_selector;

			if (port_insert->get_gui() == 0) {
				io_selector = new PortInsertWindow (_session, *port_insert);
				port_insert->set_gui (io_selector);
				
			} else {
				io_selector = reinterpret_cast<PortInsertWindow *> (port_insert->get_gui());
			}
			
			if (io_selector->is_visible()) {
				io_selector->get_window()->raise ();
			} else {
				io_selector->show_all ();
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

bool
RedirectBox::leave_box (GdkEventCrossing *ev, RedirectBox* rb)
{
	switch (ev->detail) {
	case GDK_NOTIFY_INFERIOR:
		break;

	case GDK_NOTIFY_VIRTUAL:
		/* fallthru */
	default:
		_current_redirect_box = 0;
	}

	return false;
}

void
RedirectBox::register_actions ()
{
	Glib::RefPtr<Gtk::ActionGroup> popup_act_grp = Gtk::ActionGroup::create(X_("redirectmenu"));
	Glib::RefPtr<Action> act;

	/* new stuff */
	ActionManager::register_action (popup_act_grp, X_("newplugin"), _("New Plugin ..."),  sigc::ptr_fun (RedirectBox::rb_choose_plugin));
	ActionManager::register_action (popup_act_grp, X_("newinsert"), _("New Insert"),  sigc::ptr_fun (RedirectBox::rb_choose_insert));
	ActionManager::register_action (popup_act_grp, X_("newsend"), _("New Send ..."),  sigc::ptr_fun (RedirectBox::rb_choose_send));
	ActionManager::register_action (popup_act_grp, X_("clear"), _("Clear"),  sigc::ptr_fun (RedirectBox::rb_clear));

	/* standard editing stuff */
	act = ActionManager::register_action (popup_act_grp, X_("cut"), _("Cut"),  sigc::ptr_fun (RedirectBox::rb_cut));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);
	act = ActionManager::register_action (popup_act_grp, X_("copy"), _("Copy"),  sigc::ptr_fun (RedirectBox::rb_copy));
	ActionManager::plugin_selection_sensitive_actions.push_back(act);
	ActionManager::ActionManager::register_action (popup_act_grp, X_("paste"), _("Paste"),  sigc::ptr_fun (RedirectBox::rb_paste));
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

