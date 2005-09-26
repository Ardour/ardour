/*
    Copyright (C) 2002 Paul Davis

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
#include <stdint.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <sigc++/bind.h>

#include "connection_editor.h"

#include <ardour/session.h>
#include <ardour/session_connection.h>
#include <ardour/audioengine.h>
#include <ardour/connection.h>

#include "utils.h"
#include "keyboard.h"
#include "prompter.h"

#include "i18n.h"

#include <inttypes.h>

using namespace std;
using namespace ARDOUR;
using namespace Gtk;
using namespace sigc;

ConnectionEditor::ConnectionEditor ()
	: ArdourDialog ("connection editor"),
	  input_connection_display (1),
	  output_connection_display (1),
	  input_frame (_("Input Connections")),
	  output_frame (_("Output Connections")),
	  new_input_connection_button (_("New Input")),
	  new_output_connection_button (_("New Output")),
	  delete_connection_button (_("Delete")),
	  clear_button (_("Clear")),
	  add_port_button (_("Add Port")),
	  ok_button (_("Close")),
	  cancel_button (_("Cancel")),
	  rescan_button (_("Rescan"))
	  
{
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);

	session = 0;
	selected_port = -1;
	current_connection = 0;
	push_at_front = false;

	set_name ("ConnectionEditorWindow");

	ok_button.set_name ("ConnectionEditorButton");
	cancel_button.set_name ("ConnectionEditorButton");
	rescan_button.set_name ("ConnectionEditorButton");
	new_input_connection_button.set_name ("ConnectionEditorButton");
	new_output_connection_button.set_name ("ConnectionEditorButton");
	clear_button.set_name ("ConnectionEditorButton");
	
	button_frame.set_name ("ConnectionEditorFrame");
	input_frame.set_name ("ConnectionEditorFrame");
	output_frame.set_name ("ConnectionEditorFrame");

	button_box.set_spacing (15);
	button_box.set_border_width (5);
	Gtkmm2ext::set_size_request_to_display_given_text (ok_button, _("OK"), 40, 15);
 	button_box.pack_end (ok_button, false, false);
 	// button_box.pack_end (cancel_button, false, false);
	cancel_button.hide();
	button_frame.add (button_box);

	ok_button.signal_clicked().connect (mem_fun(*this, &ConnectionEditor::accept));
	cancel_button.signal_clicked().connect (mem_fun(*this, &ConnectionEditor::cancel));
	cancel_button.signal_clicked().connect (mem_fun(*this, &ConnectionEditor::rescan));

	notebook.set_name ("ConnectionEditorNotebook");
	notebook.set_size_request (-1, 125);

	clear_button.set_name ("ConnectionEditorButton");
	add_port_button.set_name ("ConnectionEditorButton");
	Gtkmm2ext::set_size_request_to_display_given_text (add_port_button, _("Add Port"), 35, 15);

	selector_frame.set_name ("ConnectionEditorFrame");
	port_frame.set_name ("ConnectionEditorFrame");

	selector_frame.set_label (_("Available Ports"));
	
	selector_button_box.set_spacing (5);
	selector_button_box.set_border_width (5);
	Gtkmm2ext::set_size_request_to_display_given_text (rescan_button, _("Rescan"), 35, 15);
	selector_button_box.pack_start (rescan_button, false, false);

	selector_box.set_spacing (5);
	selector_box.set_border_width (5);
	selector_box.pack_start (notebook);
	selector_box.pack_start (selector_button_box);

	selector_frame.add (selector_box);

	port_box.set_spacing (5);
	port_box.set_border_width (3);

	port_button_box.set_spacing (5);
	port_button_box.set_border_width (2);

	port_button_box.pack_start (add_port_button, false, false);
	port_and_button_box.set_border_width (5);
	port_and_button_box.pack_start (port_button_box, false, false);
	port_and_button_box.pack_start (port_box);

	port_frame.add (port_and_button_box);

	port_and_selector_box.set_spacing (5);
	port_and_selector_box.pack_start (port_frame);
	port_and_selector_box.pack_start (selector_frame);

	right_vbox.set_spacing (5);
	right_vbox.set_border_width (5);
	right_vbox.pack_start (port_and_selector_box);

	input_connection_display.set_shadow_type (Gtk::SHADOW_IN);
	input_connection_display.set_selection_mode (GTK_SELECTION_SINGLE);
	input_connection_display.set_size_request (80, -1);
	input_connection_display.set_name ("ConnectionEditorConnectionList");
	input_connection_display.select_row.connect (bind (mem_fun(*this, &ConnectionEditor::connection_selected), true));

	output_connection_display.set_shadow_type (Gtk::SHADOW_IN);
	output_connection_display.set_selection_mode (GTK_SELECTION_SINGLE);
	output_connection_display.set_size_request (80, -1);
	output_connection_display.set_name ("ConnectionEditorConnectionList");
	output_connection_display.select_row.connect (bind (mem_fun(*this, &ConnectionEditor::connection_selected), false));

	input_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	output_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	input_scroller.add_with_viewport (input_connection_display);
	output_scroller.add_with_viewport (output_connection_display);

	input_box.set_border_width (5);
	input_box.set_spacing (5);
	input_box.pack_start (input_scroller);
	input_box.pack_start (new_input_connection_button, false, false);
	input_frame.add (input_box);

	output_box.set_border_width (5);
	output_box.set_spacing (5);
	output_box.pack_start (output_scroller);
	output_box.pack_start (new_output_connection_button, false, false);
	output_frame.add (output_box);

	connection_box.set_spacing (5);
	connection_box.pack_start (input_frame);
	connection_box.pack_start (output_frame);

	left_vbox.set_spacing (5);
	left_vbox.pack_start (connection_box);

	main_hbox.set_border_width (10);
	main_hbox.set_spacing (5);
	main_hbox.pack_start (left_vbox);
	main_hbox.pack_start (right_vbox);

	main_vbox.set_border_width (10);
	main_vbox.set_spacing (5);
	main_vbox.pack_start (main_hbox);
	main_vbox.pack_start (button_frame, false, false);

	set_title (_("ardour: connections"));
	add (main_vbox);

	delete_event.connect (bind (ptr_fun (just_hide_it), reinterpret_cast<Window *> (this)));

	clear_button.signal_clicked().connect (mem_fun(*this, &ConnectionEditor::clear));
	add_port_button.signal_clicked().connect (mem_fun(*this, &ConnectionEditor::add_port));
	new_input_connection_button.signal_clicked().connect (bind (mem_fun(*this, &ConnectionEditor::new_connection), true));
	new_output_connection_button.signal_clicked().connect (bind (mem_fun(*this, &ConnectionEditor::new_connection), false));
	delete_connection_button.signal_clicked().connect (mem_fun(*this, &ConnectionEditor::delete_connection));
}

ConnectionEditor::~ConnectionEditor()
{
}

void
ConnectionEditor::set_session (Session *s)
{
	if (s != session) {

		ArdourDialog::set_session (s);
		
		if (session) {
			session->ConnectionAdded.connect (mem_fun(*this, &ConnectionEditor::proxy_add_connection_and_select));
			session->ConnectionRemoved.connect (mem_fun(*this, &ConnectionEditor::proxy_remove_connection));
		} else {
			hide ();
		}
	}
}

void
ConnectionEditor::rescan ()
{
	refill_connection_display ();
	display_ports ();
}

void
ConnectionEditor::cancel ()
{
	hide ();
}

void
ConnectionEditor::accept ()
{
	hide ();
}

void
ConnectionEditor::clear ()
{
	if (current_connection) {
		current_connection->clear ();
	}
}

gint
ConnectionEditor::on_map (GdkEventAny *ev)
{
	refill_connection_display ();
	return Window::on_map (ev);
}

void
ConnectionEditor::add_connection (ARDOUR::Connection *connection)
{
	using namespace CList_Helpers;
	const char *rowtext[1];

	rowtext[0] = connection->name().c_str();

	if (dynamic_cast<InputConnection *> (connection)) {
		if (push_at_front) {
			input_connection_display.rows().push_front (rowtext);
			input_connection_display.rows().front().set_data (connection);
		} else {
			input_connection_display.rows().push_back (rowtext);
			input_connection_display.rows().back().set_data (connection);
		}
	} else {
		if (push_at_front) {
			output_connection_display.rows().push_front (rowtext);
			output_connection_display.rows().front().set_data (connection);
		} else {
			output_connection_display.rows().push_back (rowtext);
			output_connection_display.rows().back().set_data (connection);
		}
	}
}

void
ConnectionEditor::remove_connection (ARDOUR::Connection *connection)
{
	using namespace Gtk::CList_Helpers;
	RowList::iterator i;
	RowList* rlist;

	if (dynamic_cast<InputConnection *> (connection)) {
		rlist = &input_connection_display.rows();
	} else {
		rlist = &output_connection_display.rows();
	}

	if ((i = rlist->find_data (connection)) != rlist->end()) {
		rlist->erase (i);
	}
}

void
ConnectionEditor::proxy_add_connection_and_select (ARDOUR::Connection *connection)
{
	Gtkmm2ext::UI::instance()->call_slot (bind (mem_fun(*this, &ConnectionEditor::add_connection_and_select), connection));
}

void
ConnectionEditor::proxy_remove_connection (ARDOUR::Connection *connection)
{
	Gtkmm2ext::UI::instance()->call_slot (bind (mem_fun(*this, &ConnectionEditor::remove_connection), connection));
}

void
ConnectionEditor::add_connection_and_select (ARDOUR::Connection *connection)
{
	add_connection (connection);

	if (dynamic_cast<InputConnection *> (connection)) {
		input_connection_display.rows().front().select ();
	} else {
		output_connection_display.rows().front().select ();
	}
}

void
ConnectionEditor::refill_connection_display ()
{
	input_connection_display.clear();
	output_connection_display.clear();

	current_connection = 0;
	
	if (session) {
		session->foreach_connection (this, &ConnectionEditor::add_connection);
	}
}
	
void
ConnectionEditor::connection_selected (gint row, gint col, GdkEvent *ev, bool input)
{
	ARDOUR::Connection *old_current = current_connection;


	if (input) {
		output_connection_display.unselect_all ();
		current_connection = reinterpret_cast<ARDOUR::Connection*> (input_connection_display.row(row).get_data());
	} else {
		input_connection_display.unselect_all ();
		current_connection = reinterpret_cast<ARDOUR::Connection*> (output_connection_display.row(row).get_data());
	}

	if (old_current != current_connection) {
		config_connection.disconnect ();
		connect_connection.disconnect ();
	}

	if (current_connection) {
		config_connection = current_connection->ConfigurationChanged.connect 
			(bind (mem_fun(*this, &ConnectionEditor::configuration_changed), input));
		connect_connection = current_connection->ConnectionsChanged.connect 
			(bind (mem_fun(*this, &ConnectionEditor::connections_changed), input));
	}

	display_connection_state (input);
	display_ports ();
}

void
ConnectionEditor::configuration_changed (bool for_input)
{
	display_connection_state (for_input);
}

void
ConnectionEditor::connections_changed (int which_port, bool for_input)
{
	display_connection_state (for_input);
}

void
ConnectionEditor::display_ports ()
{
	if (session == 0 || current_connection == 0) {
		return;
	}
	
	using namespace Notebook_Helpers;
	using namespace CList_Helpers;

	typedef map<string,vector<pair<string,string> > > PortMap;
	PortMap portmap;
	const char **ports;
	PageList& pages = notebook.pages();
	gint current_page;
	vector<string> rowdata;
	bool for_input;

	current_page = notebook.get_current_page_num ();
	pages.clear ();

	/* get relevant current JACK ports */

	for_input = (dynamic_cast<InputConnection *> (current_connection) != 0);

	ports = session->engine().get_ports ("", JACK_DEFAULT_AUDIO_TYPE, for_input?JackPortIsOutput:JackPortIsInput);

	if (ports == 0) {
		return;
	}

	/* find all the client names and group their ports into a list-by-client */
	
	for (int n = 0; ports[n]; ++n) {

		pair<string,vector<pair<string,string> > > newpair;
		pair<string,string> strpair;
		pair<PortMap::iterator,bool> result;

		string str = ports[n];
		string::size_type pos;
		string portname;

		pos = str.find (':');

		newpair.first = str.substr (0, pos); 
		portname = str.substr (pos+1);

		result = portmap.insert (newpair);

		strpair.first = portname;
		strpair.second = str;

		result.first->second.push_back (strpair);
	}

	PortMap::iterator i;

	for (i = portmap.begin(); i != portmap.end(); ++i) {
		
		Box *client_box = manage (new VBox);
		Gtk::CList *client_port_display = manage (new Gtk::CList (1));
		ScrolledWindow *scroller = manage (new ScrolledWindow);

		scroller->add_with_viewport (*client_port_display);
		scroller->set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

		client_box->pack_start (*scroller);

		client_port_display->set_selection_mode (GTK_SELECTION_BROWSE);
		client_port_display->set_name ("ConnectionEditorList");

		for (vector<pair<string,string> >::iterator s = i->second.begin(); s != i->second.end(); ++s) {
			
			rowdata.clear ();
			rowdata.push_back (s->first);
			client_port_display->rows().push_back (rowdata);
			client_port_display->rows().back().set_data (g_strdup (s->second.c_str()), free);
		}

		client_port_display->columns_autosize ();
		client_port_display->select_row.connect (bind (mem_fun(*this, &ConnectionEditor::port_selection_handler), client_port_display));
		
		Label *tab_label = manage (new Label);

		tab_label->set_name ("ConnectionEditorNotebookTab");
		tab_label->set_text ((*i).first);

		pages.push_back (TabElem (*client_box, *tab_label));
	}

	notebook.set_page (current_page);
	notebook.show.connect (bind (mem_fun (notebook, &Notebook::set_page), current_page));
	selector_box.show_all ();
}	

void
ConnectionEditor::display_connection_state (bool for_input)
{
	LockMonitor lm (port_display_lock, __LINE__, __FILE__);
	uint32_t limit;

	if (session == 0 || current_connection == 0) {
		return;
	}

	string frame_label = _("Connection \"");
	frame_label += current_connection->name();
	frame_label += _("\"");
	port_frame.set_label (frame_label);

	for (slist<ScrolledWindow *>::iterator i = port_displays.begin(); i != port_displays.end(); ) {
		
		slist<ScrolledWindow *>::iterator tmp;

		tmp = i;
		tmp++;

		port_box.remove (**i);
		delete *i;
		port_displays.erase (i);

		i = tmp;
	} 
	
	limit = current_connection->nports();

	for (uint32_t n = 0; n < limit; ++n) {

		CList *clist;
		ScrolledWindow *scroller;

		const gchar *title[1];
		char buf[32];
		string really_short_name;

		if (for_input) {
			snprintf(buf, sizeof(buf)-1, _("in %d"), n+1);
		} else {
			snprintf(buf, sizeof(buf)-1, _("out %d"), n+1);
		}
			
		title[0] = buf;
		clist = manage (new CList (1, title));
		scroller = new ScrolledWindow;
		
		scroller->add_with_viewport (*clist);
		scroller->set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

		port_displays.insert (port_displays.end(), scroller);
		port_box.pack_start (*scroller);

		clist->set_data ("port", (gpointer) ((intptr_t) n));

		clist->set_name ("ConnectionEditorPortList");
		clist->click_column.connect (bind (mem_fun(*this, &ConnectionEditor::port_column_click), clist));
		clist->set_selection_mode (GTK_SELECTION_SINGLE);
		clist->set_shadow_type (Gtk::SHADOW_IN);

		scroller->set_size_request (-1, 75);

		/* now fill the clist with the current connections */

		const ARDOUR::Connection::PortList& connections = current_connection->port_connections (n);
	
		for (ARDOUR::Connection::PortList::const_iterator i = connections.begin(); i != connections.end(); ++i) {
			const gchar *data[1];
			
			data[0] = (*i).c_str();
			clist->rows().push_back (data);
		}

		clist->columns_autosize ();
		clist->signal_button_release_event().connect (bind (mem_fun(*this, &ConnectionEditor::port_button_event), clist));
	}

	port_box.show_all ();
}

void
ConnectionEditor::port_selection_handler (gint row, gint col, GdkEvent *ev, Gtk::CList *clist)
{
	using namespace CList_Helpers;

	string other_port_name = (char *) clist->rows()[row].get_data();
	
	if (current_connection && selected_port >= 0) {
		current_connection->add_connection (selected_port, other_port_name);
	}

}

void
ConnectionEditor::add_port ()
{
	if (current_connection) {
		current_connection->add_port ();
	}
}

void
ConnectionEditor::port_column_click (gint col, CList *clist)
{
	/* Gack. CList's don't respond visually to a change
	   in their state, so rename them to force a style
	   switch.
	*/

	LockMonitor lm (port_display_lock, __LINE__, __FILE__);

	int which_port = reinterpret_cast<intptr_t> (clist->get_data ("port"));

	if (which_port != selected_port) {
		
		selected_port = which_port;
		display_ports ();

		clist->set_name ("ConnectionEditorPortListSelected");

		for (slist<ScrolledWindow *>::iterator i = port_displays.begin(); i != port_displays.end(); ++i) {

			Widget *child = (*i)->get_child();

			if (static_cast<CList *> (child) != clist) {
				child->set_name ("ConnectionEditorPortList");
				child->queue_draw ();
			}
		}
		
	} else {
		
		selected_port = -1;
		clist->set_name ("ConnectionEditorPortList");
		clist->queue_draw();
	}
}

gint
ConnectionEditor::connection_click (GdkEventButton *ev, CList *clist)
{
	gint row, col;

	if (clist->get_selection_info ((int)ev->x, (int)ev->y, &row, &col) == 0) {
		return FALSE;
	}

	current_connection = reinterpret_cast<ARDOUR::Connection *> (clist->row(row).get_data ());

	return TRUE;
}

void
ConnectionEditor::new_connection (bool for_input)
{
	if (session == 0) {
		return;
	}

	ArdourPrompter prompter (true);
	prompter.set_prompt (_("Name for new connection:"));
	prompter.done.connect (Gtk::Main::quit.slot());
	prompter.show_all();

	Gtk::Main::run();

	if (prompter.status == Gtkmm2ext::Prompter::entered) {
		string name;
		prompter.get_result (name);

		push_at_front = true;

		if (name.length()) {
			if (for_input) {
				session->add_connection (new ARDOUR::InputConnection (name));
			} else {
				session->add_connection (new ARDOUR::OutputConnection (name));
			}
		}
		push_at_front = false;
	}
}

void
ConnectionEditor::delete_connection ()
{
	if (session && current_connection) {
		session->remove_connection (current_connection);
		current_connection = 0;
	}
}

gint
ConnectionEditor::port_button_event (GdkEventButton *ev, CList *clist)
{
	int row, col;

	if (current_connection == 0) {
		return FALSE;
	}

	if (clist->get_selection_info ((int) ev->x, (int) ev->y, &row, &col) == 0) {
		return FALSE;
	}

	if (!(Keyboard::is_delete_event (ev))) {
		return FALSE;
	}

	string port_name = clist->cell (row, col).get_text ();
	int which_port = (intptr_t) clist->get_data ("port");
	
	current_connection->remove_connection (which_port, port_name);

	return TRUE;
}
	
	
