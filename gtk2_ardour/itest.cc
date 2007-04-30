/*
    Copyright (C) 2000-2007 Paul Davis 

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

#include <map>
#include <vector>
#include <string>
#include <iostream>

#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/box.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm2ext/dndtreeview.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treestore.h>
#include <gtkmm/treepath.h>
#include <gtkmm/button.h>
#include <gtkmm/window.h>
#include <jack/jack.h>

using namespace std;
using namespace sigc;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Glib;

struct ModelColumns : public TreeModel::ColumnRecord {
    ModelColumns() { 
	    add (used);
	    add (text);
	    add (port);
    }
    TreeModelColumn<bool>         used;
    TreeModelColumn<ustring>      text;
    TreeModelColumn<jack_port_t*> port;
};

jack_client_t* jack;

void
fill_it (RefPtr<TreeStore> model, TreeView* display, ModelColumns* columns)
{
	RefPtr<TreeModel> old = display->get_model();
	display->set_model (RefPtr<TreeStore>(0));

	model->clear ();
	
	const char ** ports;
	typedef map<string,vector<pair<string,string> > > PortMap;
	PortMap portmap;
	PortMap::iterator i;
	
	ports = jack_get_ports (jack, "", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput);

	if (ports == 0) {
		goto out;
	}

	/* find all the client names and group their ports into a list-by-client */
	
	for (int n = 0; ports[n]; ++n) {

		pair<string,vector<pair<string,string> > > newpair;
		pair<string,string> strpair;
		std::pair<PortMap::iterator,bool> result;

		string str = ports[n];
		string::size_type pos;
		string portname;

		pos = str.find (':');

		newpair.first = str.substr (0, pos); 
		portname = str.substr (pos+1);

		/* this may or may not succeed at actually inserting. 
		   we don't care, however: we just want an iterator
		   that gives us either the inserted element or
		   the existing one with the same name.
		*/

		result = portmap.insert (newpair);

		strpair.first = portname;
		strpair.second = str;

		result.first->second.push_back (strpair);
	}


	for (i = portmap.begin(); i != portmap.end(); ++i) {

		/* i->first is a client name, i->second is a PortMap of all of its ports */

		TreeModel::Row parent = *(model->append());

		parent[columns->used] = false;
		parent[columns->text] = i->first;
		parent[columns->port] = 0;

		for (vector<pair<string,string> >::iterator s = i->second.begin(); s != i->second.end(); ++s) {

			/* s->first is a port name */
			
			TreeModel::Row row = *(model->append (parent.children()));

			row[columns->used] = ((random()%2) == 1);
			row[columns->text] = s->first;
			row[columns->port] = (jack_port_t*) random();
		}
	}

  out:
	display->set_model (old);
}

void
selection_changed (RefPtr<TreeModel> model, TreeView* display, ModelColumns* columns)
{
//	TreeSelection::ListHandle_Path selection = display->get_selection()->get_selected_rows ();
//
//	for (TreeSelection::ListHandle_Path::iterator x = selection.begin(); x != selection.end(); ++x) {
//		cerr << "selected: " << (*(model->get_iter (*x)))[columns->text] << endl;
//	}
}

bool
selection_filter (const RefPtr<TreeModel>& model, const TreeModel::Path& path, bool yn, ModelColumns* columns)
{
	return (*(model->get_iter (path)))[columns->port] != 0;
}

void
object_drop (string type, uint32_t cnt, void** ptr)
{
	cerr << "Got an object drop of " << cnt << " pointer(s) of type " << type << endl;
}

int
main (int argc, char* argv[])
{
	Main app (&argc, &argv);
	Window win;
	VBox   vpacker;
	HBox   hpacker;
	Button rescan ("rescan");
	ScrolledWindow scrollerA;
	ScrolledWindow scrollerB;
	DnDTreeView displayA;
	DnDTreeView displayB;
	ModelColumns columns;

	if ((jack = jack_client_new ("itest")) == NULL) {
		return -1;
	}

	RefPtr<TreeStore> modelA = TreeStore::create (columns);
	RefPtr<TreeStore> modelB = TreeStore::create (columns);
	
	displayA.set_model (modelA);
	displayA.append_column ("Use", columns.used);
	displayA.append_column ("Source/Port", columns.text);
	displayA.set_reorderable (true);
	displayA.add_object_drag (columns.port.index(), "ports");
	displayA.signal_object_drop.connect (ptr_fun (object_drop));
	
	displayA.get_selection()->set_mode (SELECTION_MULTIPLE);
	displayA.get_selection()->set_select_function (bind (ptr_fun (selection_filter), &columns));
	displayA.get_selection()->signal_changed().connect (bind (ptr_fun (selection_changed), modelA, &displayA, &columns));

	displayB.set_model (modelB);
	displayB.append_column ("Use", columns.used);
	displayB.append_column ("Source/Port", columns.text);
	displayB.set_reorderable (true);
	displayB.add_object_drag (columns.port.index(), "ports");
	displayB.signal_object_drop.connect (ptr_fun (object_drop));

	displayB.get_selection()->set_mode (SELECTION_MULTIPLE);
	displayB.get_selection()->set_select_function (bind (ptr_fun (selection_filter), &columns));
	displayB.get_selection()->signal_changed().connect (bind (ptr_fun (selection_changed), modelB, &displayB, &columns));

	scrollerA.add (displayA);
	scrollerB.add (displayB);

	hpacker.pack_start (scrollerA);
	hpacker.pack_start (scrollerB);

	vpacker.pack_start (hpacker);
	vpacker.pack_start (rescan, false, false);
	
	win.add (vpacker);
	win.set_size_request (500, 400);
	win.show_all ();
	
	rescan.signal_clicked().connect (bind (ptr_fun (fill_it), modelA, &displayA, &columns));
	rescan.signal_clicked().connect (bind (ptr_fun (fill_it), modelB, &displayB, &columns));
	
	fill_it (modelA, &displayA, &columns);
	fill_it (modelB, &displayB, &columns);

	displayA.expand_all();
	displayB.expand_all();

	app.run ();

	jack_client_close (jack);
}
