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

#include "plugin_ui.h"
#include "send_ui.h"
#include "io_selector.h"
#include "utils.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace Gtk;
using namespace Gtkmm2ext;



RedirectBox::RedirectBox (Placement pcmnt, Session& sess, Route& rt, PluginSelector &plugsel, RouteRedirectSelection & rsel, bool owner_is_mixer)
	: _route(rt), 
	  _session(sess), 
	  _owner_is_mixer (owner_is_mixer), 
	  _placement(pcmnt), 
	  _plugin_selector(plugsel), 
	  _rr_selection(rsel), 
	  redirect_display (1)
{
	_width = Wide;
	redirect_menu = 0;
	send_action_menu = 0;
	redirect_drag_in_progress = false;
	
	redirect_display.set_name ("MixerRedirectSelector");
	redirect_display.column_titles_active ();
	redirect_display.set_reorderable (true);
	redirect_display.set_button_actions (0, (GTK_BUTTON_SELECTS|GTK_BUTTON_DRAGS));
	redirect_display.set_button_actions (1, 0);
	redirect_display.set_button_actions (2, 0);
	redirect_display.set_button_actions (3, 0);
	redirect_display.drag_begin.connect (mem_fun(*this, &RedirectBox::redirect_drag_begin));
	redirect_display.drag_end.connect (mem_fun(*this, &RedirectBox::redirect_drag_end));
	redirect_display.set_size_request (-1, 48);
	redirect_display.set_selection_mode (GTK_SELECTION_MULTIPLE);
	redirect_display.set_shadow_type (Gtk::SHADOW_IN);
	redirect_display.row_move.connect (mem_fun(*this, &RedirectBox::redirects_reordered));

	redirect_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	redirect_scroller.add (redirect_display);
	redirect_eventbox.add (redirect_scroller);
	pack_start (redirect_eventbox, true, true);

	redirect_scroller.show ();
	redirect_display.show ();
	redirect_eventbox.show ();
	show_all ();

	_route.redirects_changed.connect (mem_fun(*this, &RedirectBox::redirects_changed));

	redirect_display.button_press_event.connect (mem_fun(*this, &RedirectBox::redirect_button));
	redirect_display.button_release_event.connect (mem_fun(*this, &RedirectBox::redirect_button));

	redirect_display.button_release_event.connect_after (ptr_fun (do_not_propagate));
	_plugin_selector.hide.connect(mem_fun(*this,&RedirectBox::disconnect_newplug));

	redirect_display.click_column.connect (mem_fun(*this, &RedirectBox::show_redirect_menu));
	
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
RedirectBox::set_stuff_from_route ()
{
}

void
RedirectBox::set_title (const std::string & title)
{
	redirect_display.column(0).set_title (title);
}

void
RedirectBox::set_title_shown (bool flag)
{
	if (flag) {
		redirect_display.column_titles_show();
	} else {
		redirect_display.column_titles_hide();
	}
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
		redirect_menu = build_redirect_menu (redirect_display);
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
	gint row=-1, col=-1;
	Redirect *redirect;
	CList *clist = &redirect_display;

	if (clist->get_selection_info ((int)ev->x, (int)ev->y, &row, &col) != 1) {
		redirect = 0;
	} else {
		redirect = reinterpret_cast<Redirect *> (clist->row (row).get_data ());
	}

	switch (ev->type) {
	case GDK_BUTTON_PRESS:
		return FALSE;

	case GDK_2BUTTON_PRESS:
		if (ev->state != 0) {
			return FALSE;
		}
		/* might be edit event, see below */
		break;

	case GDK_BUTTON_RELEASE:
		if (redirect_drag_in_progress) {
			// drag-n-drop reordering 
			return stop_signal (*clist, "button-release-event");
		}
		/* continue on */
		break;

	default:
		/* shouldn't be here, but gcc complains */
		return FALSE;
	}

	if (redirect && Keyboard::is_delete_event (ev)) {
		
		Gtk::Main::idle.connect (bind (mem_fun(*this, &RedirectBox::idle_delete_redirect), redirect));
		return TRUE;

	} else if (redirect && (Keyboard::is_edit_event (ev) || ev->type == GDK_2BUTTON_PRESS)) {
		
		if (_session.engine().connected()) {
			/* XXX giving an error message here is hard, because we may be in the midst of a button press */
			edit_redirect (redirect);
		}
		return TRUE;

	} else if (Keyboard::is_context_menu_event (ev)) {
		show_redirect_menu(0);
		return stop_signal (*clist, "button-release-event");

	} else {
		switch (ev->button) {
		case 1:
			if (redirect) {
				using namespace CList_Helpers;
				SelectionList& sel (redirect_display.selection());
				bool selecting = true;
				
				for (SelectionIterator i = sel.begin(); i != sel.end(); ++i) {
					if ((*i).get_row_num() == row) {
						// clicked row is not selected yet, so it is
						// becoming selected now
						selecting = false;
						break;
					}
				}

				if (selecting) {
					RedirectSelected (redirect); // emit
				}
				else {
					RedirectUnselected (redirect); // emit
				}
			}
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
RedirectBox::build_redirect_menu (CList& clist)
{
	using namespace Menu_Helpers;
	Menu * menu = new Menu;
	menu->set_name ("ArdourContextMenu");
	MenuList& items = menu->items();
	menu->set_name ("ArdourContextMenu");
	
	/* new stuff */
	
	items.push_back (MenuElem (_("New Plugin ..."), mem_fun(*this, &RedirectBox::choose_plugin)));
	items.push_back (MenuElem (_("New Insert"), mem_fun(*this, &RedirectBox::choose_insert)));
	items.push_back (MenuElem (_("New Send ..."), mem_fun(*this, &RedirectBox::choose_send)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Clear"), mem_fun(*this, &RedirectBox::clear_redirects)));
	items.push_back (SeparatorElem());

	/* standard editing stuff */

	items.push_back (MenuElem (_("Cut"), mem_fun(*this, &RedirectBox::cut_redirects)));
	selection_dependent_items.push_back (items.back());
	items.push_back (MenuElem (_("Copy"), mem_fun(*this, &RedirectBox::copy_redirects)));
	selection_dependent_items.push_back (items.back());
	items.push_back (MenuElem (_("Paste"), mem_fun(*this, &RedirectBox::paste_redirects)));
	redirect_paste_item = items.back();
	
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Rename"), mem_fun(*this, &RedirectBox::rename_redirects)));

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Select all"), mem_fun(*this, &RedirectBox::select_all_redirects)));
	items.push_back (MenuElem (_("Deselect all"), mem_fun(*this, &RedirectBox::deselect_all_redirects)));

#if LATER
	Menu *select_sub_menu = manage (new Menu);
	MenuList& sitems = select_sub_menu->items();
	select_sub_menu->set_name ("ArdourContextMenu");
	
	sitems.push_back (MenuElem (_("Plugins")));
	sitems.push_back (MenuElem (_("Inserts")));
	sitems.push_back (MenuElem (_("Sends")));
	sitems.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Select all ..."), *select_sub_menu));
#endif	
	/* activation */
						     
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Activate"), bind (mem_fun(*this, &RedirectBox::for_selected_redirects),
							&RedirectBox::activate_redirect)));
	selection_dependent_items.push_back (items.back());
	items.push_back (MenuElem (_("Deactivate"), bind (mem_fun(*this, &RedirectBox::for_selected_redirects),
							   &RedirectBox::deactivate_redirect)));
	selection_dependent_items.push_back (items.back());
	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Activate All"), bind (mem_fun(*this, &RedirectBox::all_redirects_active), true)));
	items.push_back (MenuElem (_("Deactivate All"), bind (mem_fun(*this, &RedirectBox::all_redirects_active), false)));

	/* show editors */

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Edit"), bind (mem_fun(*this, &RedirectBox::for_selected_redirects),
						    &RedirectBox::edit_redirect)));
	selection_dependent_items.push_back (items.back());

	menu->map_event.connect (mem_fun(*this, &RedirectBox::redirect_menu_map_handler));

	return menu;
}

gint
RedirectBox::redirect_menu_map_handler (GdkEventAny *ev)
{
	using namespace Menu_Helpers;
	using namespace CList_Helpers;

	Gtk::CList* clist = &redirect_display;

	bool sensitive = !clist->selection().empty();

	for (vector<MenuItem*>::iterator i = selection_dependent_items.begin(); i != selection_dependent_items.end(); ++i) {
		(*i)->set_sensitive (sensitive);
	}

	redirect_paste_item->set_sensitive (!_rr_selection.redirects.empty());
	return FALSE;
}

void
RedirectBox::select_all_redirects ()
{
	redirect_display.selection().all();
}

void
RedirectBox::deselect_all_redirects ()
{
	redirect_display.selection().clear ();
}

void
RedirectBox::choose_plugin ()
{
	show_plugin_selector();
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
	Button button (_("OK"));
	VBox vpacker;
	HBox button_box;

	/* i hate this kind of code */

	if (streams > p.get_info().n_inputs) {
		label.set_text (compose (_(
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
		label.set_text (compose (_(
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
		label.set_text (compose (_(
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

	button_box.pack_start (button, false, true);

	vpacker.set_spacing (12);
	vpacker.set_border_width (12);
	vpacker.pack_start (label);
	vpacker.pack_start (button_box);

	button.signal_clicked().connect (bind (mem_fun (dialog, &ArdourDialog::stop), 0));

	dialog.add (vpacker);
	dialog.set_name (X_("PluginIODialog"));
	dialog.set_position (Gtk::WIN_POS_MOUSE);
	dialog.set_modal (true);
	dialog.show_all ();

	dialog.realize();
	dialog.get_window().set_decorations (GdkWMDecoration (GDK_DECOR_BORDER|GDK_DECOR_RESIZEH));

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
RedirectBox::disconnect_newplug ()
{
    newplug_connection.disconnect();
}
void
RedirectBox::show_plugin_selector ()
{
	newplug_connection = _plugin_selector.PluginCreated.connect (mem_fun(*this,&RedirectBox::insert_plugin_chosen));
	_plugin_selector.show_all ();
}

void
RedirectBox::redirects_changed (void *src)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &RedirectBox::redirects_changed), src));
	
	redirect_display.freeze ();
	redirect_display.clear ();
	redirect_active_connections.clear ();
	redirect_name_connections.clear ();

	_route.foreach_redirect (this, &RedirectBox::add_redirect_to_display);

	switch (_placement) {
	case PreFader:
		build_redirect_tooltip(redirect_display, redirect_eventbox, _("Pre-fader inserts, sends & plugins:"));
		break;
	case PostFader:
		build_redirect_tooltip(redirect_display, redirect_eventbox, _("Post-fader inserts, sends & plugins:"));
		break;
	}
	redirect_display.thaw ();
}

void
RedirectBox::add_redirect_to_display (Redirect *redirect)
{
	const gchar *rowdata[1];
	gint row;
	CList *clist = 0;

	if (redirect->placement() != _placement) {
		return;
	}
	
	clist = &redirect_display;

	string rname = redirect_name (*redirect);
	rowdata[0] = rname.c_str();
	clist->rows().push_back (rowdata);
	row = clist->rows().size() - 1;
	clist->row (row).set_data (redirect);

	show_redirect_active (redirect, this);

	redirect_active_connections.push_back
		(redirect->active_changed.connect (mem_fun(*this, &RedirectBox::show_redirect_active)));
	redirect_name_connections.push_back
		(redirect->name_changed.connect (bind (mem_fun(*this, &RedirectBox::show_redirect_name), redirect)));
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
RedirectBox::build_redirect_tooltip (CList& clist, EventBox& box, string start)
{
	CList_Helpers::RowIterator ri;
	string tip(start);

	for (ri = clist.rows().begin(); ri != clist.rows().end(); ++ri) {
		tip += '\n';
		tip += clist.cell(ri->get_row_num(), 0).get_text();
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

	CList_Helpers::RowIterator ri;
	CList *clist;

	if ((ri = redirect_display.rows().find_data (redirect)) == redirect_display.rows().end()) {
		return;
	}

	clist = &redirect_display;
		
	clist->cell(ri->get_row_num(), 0).set_text (redirect_name (*redirect));

	if (redirect->active()) {
		// ri->select ();
	} else {
		// ri->unselect ();
	}
}

void
RedirectBox::redirects_reordered (gint src, gint dst)
{
	/* this is called before the reorder has been done, so just queue
	   something for idle time.
	*/

	Gtk::Main::idle.connect (mem_fun(*this, &RedirectBox::compute_redirect_sort_keys));
}

gint
RedirectBox::compute_redirect_sort_keys ()
{
	CList_Helpers::RowList::iterator i;
	uint32_t sort_key;

	sort_key = 0;

	for (i = redirect_display.rows().begin(); i != redirect_display.rows().end(); ++i) {
		Redirect *redirect = reinterpret_cast<Redirect*> (i->get_data());
		redirect->set_sort_key (sort_key, this);
		sort_key++;
	}

	if (_route.sort_redirects ()) {

		redirects_changed (0);

		/* now tell them about the problem */

		ArdourDialog dialog ("wierd plugin dialog");
		Label label;
		Button button (_("OK"));
		VBox vpacker;
		HBox button_box;

		label.set_text (_("\
You cannot reorder this set of redirects\n\
in that way because the inputs and\n\
outputs do not work correctly."));

		button_box.pack_start (button, false, true);
		
		vpacker.set_spacing (12);
		vpacker.set_border_width (12);
		vpacker.pack_start (label);
		vpacker.pack_start (button_box);
		
		button.signal_clicked().connect (bind (mem_fun (dialog, &ArdourDialog::stop), 0));
		
		dialog.add (vpacker);
		dialog.set_name (X_("PluginIODialog"));
		dialog.set_position (Gtk::WIN_POS_MOUSE);
		dialog.set_modal (true);
		dialog.show_all ();

		dialog.realize();
		dialog.get_window().set_decorations (GdkWMDecoration (GDK_DECOR_BORDER|GDK_DECOR_RESIZEH));
		
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
	ArdourDialog dialog ("rename redirect dialog");
	Entry  entry;
	VBox   vbox;
	HBox   hbox;
	Button ok_button (_("OK"));
	Button cancel_button (_("Cancel"));

	dialog.set_title (_("ardour: rename redirect"));
	dialog.set_name ("RedirectRenameWindow");
	dialog.set_size_request (300, -1);
	dialog.set_position (Gtk::WIN_POS_MOUSE);
	dialog.set_modal (true);

	vbox.set_border_width (12);
	vbox.set_spacing (12);
	vbox.pack_start (entry, false, false);
	vbox.pack_start (hbox, false, false);
	hbox.pack_start (ok_button);
	hbox.pack_start (cancel_button);
	
	dialog.add (vbox);

	entry.set_name ("RedirectNameDisplay");
	entry.set_text (redirect->name());
	entry.select_region (0, -1);
	entry.grab_focus ();

	ok_button.set_name ("EditorGTKButton");
	cancel_button.set_name ("EditorGTKButton");

	entry.activate.connect (bind (mem_fun (dialog, &ArdourDialog::stop), 1));
	cancel_button.signal_clicked().connect (bind (mem_fun (dialog, &ArdourDialog::stop), -1));
	ok_button.signal_clicked().connect (bind (mem_fun (dialog, &ArdourDialog::stop), 1));

	/* recurse */
	
	dialog.set_keyboard_input (true);
	dialog.run ();

	if (dialog.run_status() == 1) {
		redirect->set_name (entry.get_text(), this);
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
	using namespace CList_Helpers;
	SelectionList& sel (redirect_display.selection());

	for (SelectionIterator i = sel.begin(); i != sel.end(); ++i) {
		Redirect* redirect = reinterpret_cast<Redirect *> ((*i).get_data ());
		redirects.push_back (redirect);
	}
}

void
RedirectBox::for_selected_redirects (void (RedirectBox::*pmf)(Redirect*))
{
	using namespace CList_Helpers;
	SelectionList& sel (redirect_display.selection());

	for (SelectionIterator i = sel.begin(); i != sel.end(); ++i) {
		Redirect* redirect = reinterpret_cast<Redirect *> ((*i).get_data ());
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

	prompter.chosen.connect (Gtk::Main::quit.slot());
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
			title = compose(_("ardour: %1"), send->name());	
			
			send_ui = new SendUIWindow (*send, _session);
			send_ui->set_title (title);
			send->set_gui (send_ui);
			
		} else {
			send_ui = reinterpret_cast<SendUIWindow *> (send->get_gui());
		}
		
		if (send_ui->is_visible()) {
			send_ui->get_window().raise ();
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

				title = compose(_("ardour: %1: %2 (by %3)"), _route.name(), plugin_insert->name(), maker);	
				
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
				plugin_ui->get_window().raise ();
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
				io_selector->get_window().raise ();
			} else {
				io_selector->show_all ();
			}
		}
	}
}


