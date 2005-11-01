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

#include <map>
#include <vector>
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

	input_connection_model = ListStore::create (connection_columns);
	output_connection_model = ListStore::create (connection_columns);
	
	input_connection_display.set_model (input_connection_model);
	output_connection_display.set_model (output_connection_model);

	input_connection_display.append_column (_("Connections"), connection_columns.name);
	output_connection_display.append_column (_("Connections"), connection_columns.name);

	input_connection_display.get_selection()->set_mode(Gtk::SELECTION_SINGLE);
	input_connection_display.set_size_request (80, -1);
	input_connection_display.set_name ("ConnectionEditorConnectionList");

	output_connection_display.get_selection()->set_mode(Gtk::SELECTION_SINGLE);
	output_connection_display.set_size_request (80, -1);
	output_connection_display.set_name ("ConnectionEditorConnectionList");

	input_connection_display.get_selection()->signal_changed().connect (bind (mem_fun(*this, &ConnectionEditor::selection_changed), &input_connection_display));
	output_connection_display.get_selection()->signal_changed().connect (bind (mem_fun(*this, &ConnectionEditor::selection_changed), &output_connection_display));


	input_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	output_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	input_scroller.add (input_connection_display);
	output_scroller.add (output_connection_display);

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

	// GTK2FIX
	// signal_delete_event.connect (bind (ptr_fun (just_hide_it), reinterpret_cast<Window *> (this)));
	
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

void
ConnectionEditor::on_map ()
{
	refill_connection_display ();
	Window::on_map ();
}

void
ConnectionEditor::add_connection (ARDOUR::Connection *connection)
{
	TreeModel::Row row;

	if (dynamic_cast<InputConnection *> (connection)) {

		if (push_at_front) {
			row = *(input_connection_model->prepend());
		} else {
			row = *(input_connection_model->append());
		}

	} else {

		if (push_at_front) {
			row = *(output_connection_model->prepend());
		} else {
			row = *(output_connection_model->append());
		}
	}

	row[connection_columns.connection] = connection;
	row[connection_columns.name] = connection->name();
}

void
ConnectionEditor::remove_connection (ARDOUR::Connection *connection)
{
	TreeModel::iterator i;
	Glib::RefPtr<TreeModel> model = input_connection_model;

	if (dynamic_cast<InputConnection *> (connection) == 0) {
		model = output_connection_model;
	}

	TreeModel::Children rows = model->children();

	for (i = rows.begin(); i != rows.end(); ++i) {
		if ((*i)[connection_columns.connection] == connection) {
			// model->erase (i);
			break;
		}
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

	// GTK2FIX
	// if (dynamic_cast<InputConnection *> (connection)) {
	// input_connection_display.rows().front().select ();
        // } else {
	//	output_connection_display.rows().front().select ();
	//}
}

void
ConnectionEditor::refill_connection_display ()
{
	input_connection_display.set_model (Glib::RefPtr<TreeModel>(0));
	output_connection_display.set_model (Glib::RefPtr<TreeModel>(0));

	input_connection_model.clear();
	output_connection_model.clear();

	current_connection = 0;
	
	if (session) {
		session->foreach_connection (this, &ConnectionEditor::add_connection);
	}

	input_connection_display.set_model (input_connection_model);
	output_connection_display.set_model (output_connection_model);

}
	
void
ConnectionEditor::selection_changed (TreeView* view)
{
	ARDOUR::Connection *old_current = current_connection;

	TreeIter iter;
	TreeModel::Path path;
	Glib::RefPtr<TreeView::Selection> selection = view->get_selection();
	Glib::RefPtr<TreeModel> model = view->get_model();
	bool input = (view == &input_connection_display);

	iter = model->get_iter (path);
	
	current_connection = (*iter)[connection_columns.connection];

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

	typedef std::map<std::string,std::vector<std::pair<std::string,std::string> > > PortMap;
	PortMap portmap;
	const char **ports;
	PageList& pages = notebook.pages();
	gint current_page;
	vector<string> rowdata;
	bool for_input;

	current_page = notebook.get_current_page ();
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
		Gtk::CTreeView *display = manage (new Gtk::TreeView);
		RefPtr<TreeModel> model = TreeModel::create (columns);
		ScrolledWindow *scroller = manage (new ScrolledWindow);

		display->set_selection_mode (GTK_SELECTION_SINGLE);
		display->set_name ("ConnectionEditorList");

		for (vector<pair<string,string> >::iterator s = i->second.begin(); s != i->second.end(); ++s) {
			
			Row row = model->append ();

			row[displayed_name] = s->first;
			row[full_name] = s->second;
		}

		display->get_selection()->signal_changed().connect (bind (mem_fun(*this, &ConnectionEditor::port_selection_handler), display));
		
		Label *tab_label = manage (new Label);

		tab_label->set_name ("ConnectionEditorNotebookTab");
		tab_label->set_text ((*i).first);

		display->set_model (model);

		scroller->add (*client_port_display);
		scroller->set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

		client_box->pack_start (*scroller);

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
			
		tview = manage (new TreeView());
		Glib::RefPtr<ListStore> port_model = ListStore::create (*port_display_columns);
		
		tview->set_model (port_model);
		tview->append_column (_(buf), port_display_columns->name);
		tview->set_selection()->set_mode (Gtk::SELECTION_SINGLE);
		tview->set_data ("port", (gpointer) ((intptr_t) n));
		tview->set_headers_visible (true);
		tview->set_name ("ConnectionEditorPortList");
		tview->signal_button_press_event().connect (bind (mem_fun(*this, &ConnectionEditor::port_column_click), clist));

		scroller = manage (new ScrolledWindow);
		
		scroller->add (*tview);
		scroller->set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

		port_displays.insert (port_displays.end(), scroller);
		port_box.pack_start (*scroller);

		scroller->set_size_request (-1, 75);

		/* now fill the clist with the current connections */

		const ARDOUR::Connection::PortList& connections = current_connection->port_connections (n);
	
		for (ARDOUR::Connection::PortList::const_iterator i = connections.begin(); i != connections.end(); ++i) {

			TreeModel::Row row = *(model->append());

			row[port_connection_columns.name] = (*i)->name();
		}
	}

	port_box.show_all ();
}

void
ConnectionEditor::port_selection_changed (TreeView* tview)
{
	Glib::RefPtr<TreeView::Selection> sel = tview->get_selection();
	TreeModel::iterator iter = sel->get_selected();

	if (!current_connection) {
		return;
	}

	if (iter) {
		TreeModel::Row row = *iter;
		string other_port_name = row[port_display_columns.full_name];

	
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
ConnectionEditor::connection_port_button_press_event (GdkEventButton* ev, TreeView* tview)
{
	LockMonitor lm (port_display_lock, __LINE__, __FILE__);

	int which_port = reinterpret_cast<intptr_t> (treeview->get_data ("port"));

	if (which_port != selected_port) {
		
		selected_port = which_port;
		display_ports ();

		tview->set_name ("ConnectionEditorPortListSelected");

		for (slist<ScrolledWindow *>::iterator i = port_displays.begin(); i != port_displays.end(); ++i) {

			Widget *child = (*i)->get_child();

			if (static_cast<TreeView *> (child) != tview) {
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

void
ConnectionEditor::connection_selection_changed (TreeView* tview);
{
	Glib::RefPtr<TreeView::Selection> sel = tview->get_selection();
	TreeModel::iterator iter = sel->get_selected();

	if (iter) {
		TreeModel::Row row = *iter;
		current_connection = row[XXXX_display_columns.connection];
	} else {
		current_connection = 0;
	}
}

void
ConnectionEditor::new_connection (bool for_input)
{
	string name;

	if (session == 0) {
		return;
	}

	ArdourPrompter prompter (true);
	prompter.set_prompt (_("Name for new connection:"));
	prompter.done.connect (Gtk::Main::quit.slot());

	switch (prompter.run()) {
	case GTK_RESPONSE_ACCEPT:
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
		break;

	default:
		break;
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
ConnectionEditor::port_button_event (GdkEventButton *ev, Treeview* treeview)
{
	int row, col;
	TreeIter iter;
	TreeModel::Path path;
	TreeViewColumn* column;
	int cellx;
	int celly;

	if (current_connection == 0) {
		return false;
	}

	if (!(Keyboard::is_delete_event (ev))) {
		return false;
	}

	if (!treeview->get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		return false;
	}

	if ((iter = treeview->get_model()->get_iter (path))) {
		/* path is valid */
		
		string port_name = (*iter)[columns.full_name];
		int which_port = (intptr_t) treeview->get_data ("port");	

		current_connection->remove_connection (which_port, port_name);
	}

	return true;
}
