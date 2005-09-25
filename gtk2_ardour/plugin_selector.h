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

#include <gtk--.h>
#include <gtk--/ctree.h>
#include <gtkmmext/selector.h>

#include <ardour_dialog.h>

namespace ARDOUR {
	class Session;
	class PluginManager;
	class Plugin;
}

class PluginSelector : public ArdourDialog 
{
  public:
	PluginSelector (ARDOUR::PluginManager *);
	SigC::Signal1<void,ARDOUR::Plugin *> PluginCreated;

	void set_session (ARDOUR::Session*);

  private:
	ARDOUR::Session* session;
	Gtk::Notebook notebook;

	// page 1
	Gtkmmext::Selector ladspa_display;
	void column_clicked (int column, GtkCList* clist);

#ifdef VST_SUPPORT
	// page 2
	Gtkmmext::Selector vst_display;
	static void _vst_refiller (Gtk::CList &, void *);
	void vst_refiller (Gtk::CList &);
#endif	
	Gtkmmext::Selector o_selector;

	ARDOUR::PluginInfo* i_selected_plug;

	// We need an integer for the output side because
	// the name isn't promised to be unique.
	gint o_selected_plug;

	ARDOUR::PluginManager *manager;
	list<ARDOUR::PluginInfo*> added_plugins;

	static void _input_refiller (Gtk::CList &, void *);
	static void _output_refiller (Gtk::CList &, void *);

	void input_refiller (Gtk::CList &);
	void output_refiller (Gtk::CList &);
	void i_plugin_selected (Gtkmmext::Selector *selector,
			      Gtkmmext::SelectionResult *res);
	void i_plugin_chosen (Gtkmmext::Selector *selector,
			    Gtkmmext::SelectionResult *res);
	void o_plugin_selected (Gtkmmext::Selector *selector,
			      Gtkmmext::SelectionResult *res);
	void o_plugin_chosen (Gtkmmext::Selector *selector,
			    Gtkmmext::SelectionResult *res);
	
	void btn_add_clicked();
	void btn_remove_clicked();
	void btn_ok_clicked();
	void btn_update_clicked();
	void btn_apply_clicked();
	void btn_cancel_clicked();
	void use_plugin (ARDOUR::PluginInfo*);
	gint wm_close(GdkEventAny* ev);
};

#endif // __ardour_plugin_selector_h__
