/*
    Copyright (C) 2002-2003 Paul Davis 

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

#include <sigc++/bind.h>

#include <gtkmm/messagedialog.h>

#include <glibmm/thread.h>

#include <ardour/io.h>
#include <ardour/route.h>
#include <ardour/audioengine.h>
#include <ardour/port.h>
#include <ardour/insert.h>
#include <ardour/session.h>
#include <ardour/audio_diskstream.h>

#include <gtkmm2ext/doi.h>
#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>

#include "utils.h"
#include "io_selector.h"
#include "keyboard.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Glib;
using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;

IOSelectorWindow::IOSelectorWindow (Session& sess, boost::shared_ptr<IO> ior, bool input, bool can_cancel)
	: ArdourDialog ("i/o selector"),
	  _selector (sess, ior, input),
	  ok_button (can_cancel ? _("OK"): _("Close")),
	  cancel_button (_("Cancel")),
	  rescan_button (_("Rescan"))

{
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);
	set_name ("IOSelectorWindow");

	string title;
	if (input) {
		title = string_compose(_("%1 input"), ior->name());
	} else {
		title = string_compose(_("%1 output"), ior->name());
	}

	ok_button.set_name ("IOSelectorButton");
	cancel_button.set_name ("IOSelectorButton");
	rescan_button.set_name ("IOSelectorButton");

	button_box.set_spacing (5);
	button_box.set_border_width (5);
	button_box.set_homogeneous (true);
	button_box.pack_start (rescan_button);

	if (can_cancel) {
		button_box.pack_start (cancel_button);
	} else {
		cancel_button.hide();
	}
		
	button_box.pack_start (ok_button);

	get_vbox()->pack_start (_selector);
	get_vbox()->pack_start (button_box, false, false);

	ok_button.signal_clicked().connect (mem_fun(*this, &IOSelectorWindow::accept));
	cancel_button.signal_clicked().connect (mem_fun(*this, &IOSelectorWindow::cancel));
	rescan_button.signal_clicked().connect (mem_fun(*this, &IOSelectorWindow::rescan));

	set_title (title);
	set_position (WIN_POS_MOUSE);

	signal_delete_event().connect (bind (sigc::ptr_fun (just_hide_it), reinterpret_cast<Window *> (this)));
}

IOSelectorWindow::~IOSelectorWindow()
{
}

void
IOSelectorWindow::rescan ()
{
	_selector.redisplay ();
}

void
IOSelectorWindow::cancel ()
{
	_selector.Finished(IOSelector::Cancelled);
	hide ();
}

void
IOSelectorWindow::accept ()
{
	_selector.Finished(IOSelector::Accepted);
	hide ();
}

void
IOSelectorWindow::on_map ()
{
	_selector.redisplay ();
	Window::on_map ();
}

/*************************
  The IO Selector "widget"
 *************************/  

IOSelector::IOSelector (Session& sess, boost::shared_ptr<IO> ior, bool input)
	: session (sess),
	  io (ior),
	  for_input (input),
	  port_frame (for_input? _("Inputs") : _("Outputs")),
	  add_port_button (for_input? _("Add Input") : _("Add Output")),
	  remove_port_button (for_input? _("Remove Input") : _("Remove Output")),
	  clear_connections_button (_("Disconnect All"))
{
	selected_port = 0;

	notebook.set_name ("IOSelectorNotebook");
	notebook.set_size_request (-1, 125);

	clear_connections_button.set_name ("IOSelectorButton");
	add_port_button.set_name ("IOSelectorButton");
	remove_port_button.set_name ("IOSelectorButton");

	selector_frame.set_name ("IOSelectorFrame");
	port_frame.set_name ("IOSelectorFrame");

	selector_frame.set_label (_("Available connections"));
	
	selector_button_box.set_spacing (5);
	selector_button_box.set_border_width (5);

	selector_box.set_spacing (5);
	selector_box.set_border_width (5);
	selector_box.pack_start (notebook);
	selector_box.pack_start (selector_button_box, false, false);

	selector_frame.add (selector_box);

	port_box.set_spacing (5);
	port_box.set_border_width (5);

	port_display_scroller.set_name ("IOSelectorNotebook");
	port_display_scroller.set_border_width (0);
	port_display_scroller.set_size_request (-1, 170);
	port_display_scroller.add (port_box);
	port_display_scroller.set_policy (POLICY_NEVER,
					  POLICY_AUTOMATIC);

	port_button_box.set_spacing (5);
	port_button_box.set_border_width (5);

	port_button_box.pack_start (add_port_button, false, false);
	port_button_box.pack_start (remove_port_button, false, false);
	port_button_box.pack_start (clear_connections_button, false, false);

	port_and_button_box.set_border_width (5);
	port_and_button_box.pack_start (port_button_box, false, false);
	port_and_button_box.pack_start (port_display_scroller);

	port_frame.add (port_and_button_box);

	port_and_selector_box.set_spacing (5);
	port_and_selector_box.pack_start (port_frame);
	port_and_selector_box.pack_start (selector_frame);

	set_spacing (5);
	set_border_width (5);
	pack_start (port_and_selector_box);

	rescan();
	display_ports ();

	clear_connections_button.signal_clicked().connect (mem_fun(*this, &IOSelector::clear_connections));

	add_port_button.signal_clicked().connect (mem_fun(*this, &IOSelector::add_port));
	remove_port_button.signal_clicked().connect (mem_fun(*this, &IOSelector::remove_port));

	if (for_input) {
		io->input_changed.connect (mem_fun(*this, &IOSelector::ports_changed));
	} else {
		io->output_changed.connect (mem_fun(*this, &IOSelector::ports_changed));
	}

	io->name_changed.connect (mem_fun(*this, &IOSelector::name_changed));
}

IOSelector::~IOSelector ()
{
}

void 
IOSelector::set_button_sensitivity ()
{
	if (for_input) {

		if (io->input_maximum() < 0 || io->input_maximum() > (int) io->n_inputs()) {
			add_port_button.set_sensitive (true);
		} else {
			add_port_button.set_sensitive (false);
		}

	} else {

		if (io->output_maximum() < 0 || io->output_maximum() > (int) io->n_outputs()) {
			add_port_button.set_sensitive (true);
		} else {
			add_port_button.set_sensitive (false);
		}
			
	}

	if (for_input) {
		if (io->n_inputs() && (io->input_minimum() < 0 || io->input_minimum() < (int) io->n_inputs())) {
			remove_port_button.set_sensitive (true);
		} else {
			remove_port_button.set_sensitive (false);
		}
			
	} else {
		if (io->n_outputs() && (io->output_minimum() < 0 || io->output_minimum() < (int) io->n_outputs())) {
			remove_port_button.set_sensitive (true);
		} else {
			remove_port_button.set_sensitive (false);
		}
	}
}


void
IOSelector::name_changed (void* src)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &IOSelector::name_changed), src));
	
	display_ports ();
}

void
IOSelector::clear_connections ()
{
	if (for_input) {
		io->disconnect_inputs (this);
	} else {
		io->disconnect_outputs (this);
	}
}

void
IOSelector::rescan ()
{
	using namespace Notebook_Helpers;

	typedef std::map<string,vector<pair<string,string> > > PortMap;
	PortMap portmap;
	const char **ports;
	PageList& pages = notebook.pages();
	gint current_page;
	vector<string> rowdata;

	page_selection_connection.disconnect ();

	current_page = notebook.get_current_page ();

	pages.clear ();

	/* get relevant current JACK ports */

	ports = session.engine().get_ports ("", JACK_DEFAULT_AUDIO_TYPE, for_input ? JackPortIsOutput : JackPortIsInput);

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
		TreeView *display = manage (new TreeView);
		RefPtr<ListStore> model = ListStore::create (port_display_columns);
		ScrolledWindow *scroller = manage (new ScrolledWindow);

		display->set_model (model);
		display->append_column (X_("notvisible"), port_display_columns.displayed_name);
		display->set_headers_visible (false);
		display->get_selection()->set_mode (SELECTION_SINGLE);
		display->set_name ("IOSelectorList");

		for (vector<pair<string,string> >::iterator s = i->second.begin(); s != i->second.end(); ++s) {
			
			TreeModel::Row row = *(model->append ());

			row[port_display_columns.displayed_name] = s->first;
			row[port_display_columns.full_name] = s->second;
		}

		display->signal_button_release_event().connect (bind (mem_fun(*this, &IOSelector::port_selection_changed), display));
		Label *tab_label = manage (new Label);

		tab_label->set_name ("IOSelectorNotebookTab");
		tab_label->set_text ((*i).first);

		scroller->add (*display);
		scroller->set_policy (POLICY_AUTOMATIC, POLICY_AUTOMATIC);

		client_box->pack_start (*scroller);

		pages.push_back (TabElem (*client_box, *tab_label));
	}

	notebook.set_current_page (current_page);
	page_selection_connection = notebook.signal_show().connect (bind (mem_fun (notebook, &Notebook::set_current_page), current_page));
	selector_box.show_all ();
}	

void
IOSelector::display_ports ()
{
	TreeView *firsttview = 0;
	TreeView *selected_port_tview = 0;
	{
		Glib::Mutex::Lock lm  (port_display_lock);
		Port *port;
		uint32_t limit;
		
		if (for_input) {
			limit = io->n_inputs();
		} else {
			limit = io->n_outputs();
		}
		
		for (slist<TreeView *>::iterator i = port_displays.begin(); i != port_displays.end(); ) {
			
			slist<TreeView *>::iterator tmp;
			
			tmp = i;
			++tmp;
			
			port_box.remove (**i);
			delete *i;
			port_displays.erase (i);
			
			i = tmp;
		} 
		
		for (uint32_t n = 0; n < limit; ++n) {
			
			TreeView* tview;
			//ScrolledWindow *scroller;
			string really_short_name;
			
			if (for_input) {
				port = io->input (n);
			} else {
				port = io->output (n);
			}
			
			/* we know there is '/' because we put it there */
			
			really_short_name = port->short_name();
			really_short_name = really_short_name.substr (really_short_name.find ('/') + 1);
			
			tview = manage (new TreeView());
			RefPtr<ListStore> port_model = ListStore::create (port_display_columns);
			
			if (!firsttview) {
				firsttview = tview;
			}
			
			tview->set_model (port_model);
			tview->append_column (really_short_name, port_display_columns.displayed_name);
			tview->get_selection()->set_mode (SELECTION_SINGLE);
			tview->set_data ("port", port);
			tview->set_headers_visible (true);
			tview->set_name ("IOSelectorPortList");
			
			port_box.pack_start (*tview);
			port_displays.insert (port_displays.end(), tview);
			
			/* now fill the clist with the current connections */
			
			
			const char **connections = port->get_connections ();
			
			if (connections) {
				for (uint32_t c = 0; connections[c]; ++c) {
					TreeModel::Row row = *(port_model->append());
					row[port_display_columns.displayed_name] = connections[c];
					row[port_display_columns.full_name] = connections[c];
				}
			}
			
			if (for_input) {
				
				if (io->input_maximum() == 1) {
					selected_port = port;
					selected_port_tview = tview;
				} else {
					if (port == selected_port) {
						selected_port_tview = tview;
					}
				}
				
			} else {
				
				if (io->output_maximum() == 1) {
					selected_port = port;
					selected_port_tview = tview;
				} else {
					if (port == selected_port) {
						selected_port_tview = tview;
					}
				}
			}
			
			TreeViewColumn* col = tview->get_column (0);
			
			col->set_clickable (true);
			
			/* handle button events on the column header ... */
			col->signal_clicked().connect (bind (mem_fun(*this, &IOSelector::select_treeview), tview));

			/* ... and within the treeview itself */
			tview->signal_button_release_event().connect (bind (mem_fun(*this, &IOSelector::connection_button_release), tview));
		}
		
		port_box.show_all ();
	}
	
	if (!selected_port_tview) {
		selected_port_tview = firsttview;
	}

	if (selected_port_tview) {
		select_treeview (selected_port_tview);
	}
}

bool
IOSelector::port_selection_changed (GdkEventButton *ev, TreeView* treeview)
{
	TreeModel::iterator i = treeview->get_selection()->get_selected();
	int status;

	if (!i) {
		return 0;
	}

	if (selected_port == 0) {
		return 0;
	}

	ustring other_port_name = (*i)[port_display_columns.full_name];
	
	if (for_input) {
		if ((status = io->connect_input (selected_port, other_port_name, this)) == 0) {
			Port *p = session.engine().get_port_by_name (other_port_name);
			p->enable_metering();
		}
	} else {
		status = io->connect_output (selected_port, other_port_name, this);
	}

	if (status == 0) {
		select_next_treeview ();
	}
	
	treeview->get_selection()->unselect_all();
	return 0;
}

void
IOSelector::ports_changed (IOChange change, void *src)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &IOSelector::ports_changed), change, src));
	
	display_ports ();
}

void
IOSelector::add_port ()
{
	/* add a new port, then hide the button if we're up to the maximum allowed */

	if (for_input) {

		try {
			io->add_input_port ("", this);
		}

		catch (AudioEngine::PortRegistrationFailure& err) {
			MessageDialog msg (0,  _("There are no more JACK ports available."));
			msg.run ();
		}

	} else {

		try {
			io->add_output_port ("", this);
		}

		catch (AudioEngine::PortRegistrationFailure& err) {
			MessageDialog msg (0, _("There are no more JACK ports available."));
			msg.run ();
		}
	}
		
	set_button_sensitivity ();
}

void
IOSelector::remove_port ()
{
	uint32_t nports;

	// always remove last port
	
	if (for_input) {
		if ((nports = io->n_inputs()) > 0) {
			io->remove_input_port (io->input(nports-1), this);
		}

	} else {
		if ((nports = io->n_outputs()) > 0) {
			io->remove_output_port (io->output(nports-1), this);
		}
	}
	
	set_button_sensitivity ();
}

gint
IOSelector::connection_button_release (GdkEventButton *ev, TreeView *treeview)
{
	/* this handles button release on a port name row: i.e. a connection
	   between the named port and the port represented by the treeview.
	*/

	Gtk::TreeModel::iterator iter;
	TreeModel::Path path;
	TreeViewColumn* column;
	int cellx;
	int celly;

	/* only handle button1 events here */

	if (ev->button != 1) {
		return false;
	}
	
	if (!treeview->get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		return false;
	}

	if ((iter = treeview->get_model()->get_iter (path.to_string()))) {

		/* path is valid */
		ustring connected_port_name = (*iter)[port_display_columns.full_name];
		Port *port = reinterpret_cast<Port *> (treeview->get_data (_("port")));
		
		if (for_input) {
			Port *p = session.engine().get_port_by_name (connected_port_name);
			p->disable_metering();
			io->disconnect_input (port, connected_port_name, this);
		} else {
			io->disconnect_output (port, connected_port_name, this);
		}
	}

	return true;
}

void
IOSelector::select_next_treeview ()
{
	slist<TreeView*>::iterator next;

	if (port_displays.empty() || port_displays.size() == 1) {
		return;
	}

	for (slist<TreeView *>::iterator i = port_displays.begin(); i != port_displays.end(); ++i) {

		if ((*i)->get_name() == "IOSelectorPortListSelected") {

			++i;

			if (i == port_displays.end()) {
				select_treeview (port_displays.front());
			} else {
				select_treeview (*i);
			}
			
			break;
		}
	}
}

void
IOSelector::select_treeview (TreeView* tview)
{
	/* Gack. TreeView's don't respond visually to a change
	   in their state, so rename them to force a style
	   switch.
	*/

	Glib::Mutex::Lock lm  (port_display_lock);
 	Port* port = reinterpret_cast<Port *> (tview->get_data (_("port")));

	selected_port = port;

	tview->set_name ("IOSelectorPortListSelected");
	tview->queue_draw ();

	/* ugly hack to force the column header button to change as well */

	TreeViewColumn* col = tview->get_column (0);
	GtkTreeViewColumn* ccol = col->gobj();
	
	if (ccol->button) {
		gtk_widget_set_name (ccol->button, "IOSelectorPortListSelected");	
		gtk_widget_queue_draw (ccol->button);
	}

	for (slist<TreeView*>::iterator i = port_displays.begin(); i != port_displays.end(); ++i) {
		if (*i == tview) {
			continue;
		}
		
		col = (*i)->get_column (0);
		ccol = col->gobj();
		
		if (ccol->button) {
			gtk_widget_set_name (ccol->button, "IOSelectorPortList");
			gtk_widget_queue_draw (ccol->button);
		}
		
		(*i)->set_name ("IOSelectorPortList");
		(*i)->queue_draw ();
	}

	selector_box.show_all ();
}

void
IOSelector::redisplay ()
{
	display_ports ();

	if (for_input) {
		if (io->input_maximum() != 0) {
			rescan ();
		}
	} else {
		if (io->output_maximum() != 0) {
			rescan();
		}
	}
}

PortInsertUI::PortInsertUI (Session& sess, boost::shared_ptr<PortInsert> pi)
	: input_selector (sess, pi, true),
	  output_selector (sess, pi, false)
{
	hbox.pack_start (output_selector, true, true);
	hbox.pack_start (input_selector, true, true);


	pack_start (hbox);
}

void
PortInsertUI::redisplay()
{

	input_selector.redisplay();
	output_selector.redisplay();
}

void
PortInsertUI::finished(IOSelector::Result r)
{
	input_selector.Finished (r);
	output_selector.Finished (r);
}


PortInsertWindow::PortInsertWindow (Session& sess, boost::shared_ptr<PortInsert> pi, bool can_cancel)
	: ArdourDialog ("port insert dialog"),
	  _portinsertui (sess, pi),
	  ok_button (can_cancel ? _("OK"): _("Close")),
	  cancel_button (_("Cancel")),
	  rescan_button (_("Rescan"))
{

	set_name ("IOSelectorWindow");
	string title = _("ardour: ");
	title += pi->name();
	set_title (title);
	
	ok_button.set_name ("IOSelectorButton");
	cancel_button.set_name ("IOSelectorButton");
	rescan_button.set_name ("IOSelectorButton");

	button_box.set_spacing (5);
	button_box.set_border_width (5);
	button_box.set_homogeneous (true);
	button_box.pack_start (rescan_button);
	if (can_cancel) {
		button_box.pack_start (cancel_button);
	}
	else {
		cancel_button.hide();
	}
	button_box.pack_start (ok_button);

	get_vbox()->pack_start (_portinsertui);
	get_vbox()->pack_start (button_box, false, false);

	ok_button.signal_clicked().connect (mem_fun(*this, &PortInsertWindow::accept));
	cancel_button.signal_clicked().connect (mem_fun(*this, &PortInsertWindow::cancel));
	rescan_button.signal_clicked().connect (mem_fun(*this, &PortInsertWindow::rescan));

	signal_delete_event().connect (bind (sigc::ptr_fun (just_hide_it), reinterpret_cast<Window *> (this)));	
	pi->GoingAway.connect (mem_fun(*this, &PortInsertWindow::plugin_going_away));
}

void
PortInsertWindow::plugin_going_away ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &PortInsertWindow::plugin_going_away));
	
	delete_when_idle (this);
}

void
PortInsertWindow::on_map ()
{
	_portinsertui.redisplay ();
	Window::on_map ();
}


void
PortInsertWindow::rescan ()
{
	_portinsertui.redisplay();
}

void
PortInsertWindow::cancel ()
{
	_portinsertui.finished(IOSelector::Cancelled);
	hide ();
}

void
PortInsertWindow::accept ()
{
	_portinsertui.finished(IOSelector::Accepted);
	hide ();
}
