/*
    Copyright (C) 2000 Paul Davis 

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

#ifndef __ardour_plugin_selector_h__
#define __ardour_plugin_selector_h__

#include <gtkmm/dialog.h>
#include <gtkmm/notebook.h>
#include <gtkmm/treeview.h>
#include <gtkmm2ext/selector.h>

#include <ardour/plugin.h>

namespace ARDOUR {
	class Session;
	class PluginManager;
}

class PluginSelector : public ArdourDialog
{
  public:
	PluginSelector (ARDOUR::PluginManager *);
	sigc::signal<void,boost::shared_ptr<ARDOUR::Plugin> > PluginCreated;

	int run (); // XXX should we try not to overload the non-virtual Gtk::Dialog::run() ?

	void set_session (ARDOUR::Session*);

  private:
	ARDOUR::Session* session;
	Gtk::Notebook notebook;
	Gtk::ScrolledWindow lscroller;  // ladspa
	Gtk::ScrolledWindow vscroller;  // vst
	Gtk::ScrolledWindow auscroller; // AudioUnit
	Gtk::ScrolledWindow ascroller;  // Added plugins
	
	ARDOUR::PluginType current_selection;

	// page 1
	struct LadspaColumns : public Gtk::TreeModel::ColumnRecord {
		LadspaColumns () {
			add (name);
		    add (type);
			add (ins);
			add (outs);
			add (plugin);
		}
	    Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> type;
		Gtk::TreeModelColumn<std::string> ins;
		Gtk::TreeModelColumn<std::string> outs;
	    Gtk::TreeModelColumn<ARDOUR::PluginInfoPtr> plugin;
	};
	LadspaColumns lcols;
	Glib::RefPtr<Gtk::ListStore> lmodel;
	Glib::RefPtr<Gtk::TreeSelection> lselection;
	Gtk::TreeView ladspa_display;
	Gtk::Button* btn_add;
	Gtk::Button* btn_remove;

	struct AddedColumns : public Gtk::TreeModel::ColumnRecord {
		AddedColumns () {
			add (text);
			add (plugin);
		}
		Gtk::TreeModelColumn<std::string> text;
		Gtk::TreeModelColumn<ARDOUR::PluginInfoPtr> plugin;
	};
	AddedColumns acols;
	Glib::RefPtr<Gtk::ListStore> amodel;
	Glib::RefPtr<Gtk::TreeSelection> aselection;
	Gtk::TreeView added_list;

#ifdef VST_SUPPORT
	// page 2
	struct VstColumns : public Gtk::TreeModel::ColumnRecord {
		VstColumns () {
			add (name);
			add (ins);
			add (outs);
			add (plugin);
		}
	    Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> ins;
		Gtk::TreeModelColumn<std::string> outs;
	    Gtk::TreeModelColumn<ARDOUR::PluginInfoPtr> plugin;
	};
	VstColumns vcols;
	Glib::RefPtr<Gtk::ListStore> vmodel;
	Glib::RefPtr<Gtk::TreeSelection> vselection;
	Gtk::TreeView vst_display;
	static void _vst_refiller (void *);
	void vst_refiller ();
	void vst_display_selection_changed();
#endif // VST_SUPPORT

#ifdef HAVE_COREAUDIO
	// page 3
	struct AUColumns : public Gtk::TreeModel::ColumnRecord {
		AUColumns () {
			add (name);
			add (ins);
			add (outs);
			add (plugin);
		}
		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> ins;
		Gtk::TreeModelColumn<std::string> outs;
		Gtk::TreeModelColumn<ARDOUR::PluginInfoPtr> plugin;
	};
	AUColumns aucols;
	Glib::RefPtr<Gtk::ListStore> aumodel;
	Glib::RefPtr<Gtk::TreeSelection> auselection;
	Gtk::TreeView au_display;
	static void _au_refiller (void *);
	void au_refiller ();
	void au_display_selection_changed();
#endif //HAVE_COREAUDIO

	ARDOUR::PluginManager *manager;

	static void _input_refiller (void *);
	
	void input_refiller ();
	void row_clicked(GdkEventButton *);
	void btn_add_clicked();
	void btn_remove_clicked();
	void btn_update_clicked();
	void added_list_selection_changed();
	void ladspa_display_selection_changed();
	void btn_apply_clicked();
	void use_plugin (ARDOUR::PluginInfoPtr);
	void cleanup ();
};

#endif // __ardour_plugin_selector_h__

