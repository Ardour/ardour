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

#include <cstdio>
#include <lrdf.h>

#include <gtkmm/table.h>
#include <gtkmm/button.h>
#include <gtkmm/notebook.h>

#include <ardour/plugin_manager.h>
#include <ardour/plugin.h>
#include <ardour/configuration.h>

#include "ardour_ui.h"
#include "plugin_selector.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;

PluginSelector::PluginSelector (PluginManager *mgr)
	: ArdourDialog ("plugin selector")
{
	set_position (Gtk::WIN_POS_MOUSE);
	set_name ("PluginSelectorWindow");
	set_title (_("ardour: plugins"));
	set_modal(true);
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);

	manager = mgr;
	session = 0;
	o_selected_plug = -1;
	i_selected_plug = 0;

	lmodel = Gtk::ListStore::create(lcols);
	ladspa_display.set_model (lmodel);
	ladspa_display.append_column (_("Available LADSPA plugins"), lcols.name);
	ladspa_display.append_column (_("Type"), lcols.type);
	ladspa_display.append_column (_("# Inputs"),lcols.ins);
	ladspa_display.append_column (_("# Outputs"), lcols.outs);
	ladspa_display.set_headers_visible (true);
	ladspa_display.set_reorderable (false);

	amodel = Gtk::ListStore::create(acols);
	added_list.set_model (amodel);
	added_list.append_column (_("To be added"), acols.text);
	added_list.set_headers_visible (true);
	added_list.set_reorderable (false);

	for (int i = 0; i <=3; i++) {
		Gtk::TreeView::Column* column = ladspa_display.get_column(i);
		column->set_sort_column(i);
	}

#ifdef VST_SUPPORT
	vmodel = ListStore::create(vcols);
	vst_display.set_model (vmodel);
	vst_display.append_column (_("Available plugins"), vcols.name);
	vst_display.append_column (_("# Inputs"), vcols.ins);
	vst_display.append_column (_("# Outputs"), vcols.outs);
	vst_display.set_headers_visible (true);
	vst_display.set_reorderable (false);

	for (int i = 0; i <=2; i++) {
		column = vst_display.get_column(i);
		column->set_sort_column(i);
	}
#endif

	Gtk::Button *btn_add = manage(new Gtk::Button(_("Add")));
	ARDOUR_UI::instance()->tooltips().set_tip(*btn_add, _("Add a plugin to the effect list"));
	Gtk::Button *btn_remove = manage(new Gtk::Button(_("Remove")));
	ARDOUR_UI::instance()->tooltips().set_tip(*btn_remove, _("Remove a plugin from the effect list"));
	Gtk::Button *btn_ok = manage(new Gtk::Button(_("OK")));
	Gtk::Button *btn_cancel = manage(new Gtk::Button(_("Cancel")));

	Gtk::Button *btn_update = manage(new Gtk::Button(_("Update")));
	ARDOUR_UI::instance()->tooltips().set_tip(*btn_update, _("Update available plugins"));

	btn_ok->set_name("PluginSelectorButton");
	btn_cancel->set_name("PluginSelectorButton");
	btn_add->set_name("PluginSelectorButton");
	btn_remove->set_name("PluginSelectorButton");
	
	Gtk::Table* table = manage(new Gtk::Table(7, 10));
	table->set_size_request(750, 500);
	table->attach(notebook, 0, 7, 0, 5);

	table->attach(*btn_add, 1, 2, 5, 6, Gtk::FILL, Gtk::FILL, 5, 5);
	table->attach(*btn_remove, 3, 4, 5, 6, Gtk::FILL, Gtk::FILL, 5, 5);
	table->attach(*btn_update, 5, 6, 5, 6, Gtk::FILL, Gtk::FILL, 5, 5);

	table->attach(added_list, 0, 7, 7, 9);
	table->attach(*btn_ok, 1, 3, 9, 10, Gtk::FILL, Gtk::FILL, 5, 5);
	table->attach(*btn_cancel, 3, 4, 9, 10, Gtk::FILL, Gtk::FILL, 5, 5);
	add (*table);

	using namespace Gtk::Notebook_Helpers;
	notebook.pages().push_back (TabElem (ladspa_display, _("LADSPA")));
#ifdef VST_SUPPORT
	if (Config->get_use_vst()) {
		notebook.pages().push_back (TabElem (vst_display, _("VST")));
	}
#endif

	table->set_name("PluginSelectorTable");
	//ladspa_display.set_name("PluginSelectorDisplay");
	ladspa_display.set_name("PluginSelectorList");
	added_list.set_name("PluginSelectorList");
	
	//ladspa_display.clist().column(0).set_auto_resize (false);
	//ladspa_display.clist().column(0).set_width(470);

	//ladspa_display.clist().column(1).set_auto_resize (true);
	//o_selector.clist().column(0).set_auto_resize (true);

	ladspa_display.signal_button_press_event().connect_notify (mem_fun(*this, &PluginSelector::row_clicked));
#ifdef VST_SUPPORT
	if (Config->get_use_vst()) {
		vst_display.signal_button_press_event().connect_notify (mem_fun(*this, &PluginSelector::row_clicked));
	}
#endif
	
	btn_update->signal_clicked().connect (mem_fun(*this, &PluginSelector::btn_update_clicked));
	btn_add->signal_clicked().connect(mem_fun(*this, &PluginSelector::btn_add_clicked));
	btn_remove->signal_clicked().connect(mem_fun(*this, &PluginSelector::btn_remove_clicked));
	btn_ok->signal_clicked().connect(mem_fun(*this, &PluginSelector::btn_ok_clicked));
	btn_cancel->signal_clicked().connect(mem_fun(*this,&PluginSelector::btn_cancel_clicked));
	signal_delete_event().connect (mem_fun(*this, &PluginSelector::wm_close));

}

void
PluginSelector::row_clicked(GdkEventButton* event)
{
	if (event->type == GDK_2BUTTON_PRESS)
		btn_add_clicked();
}

void
PluginSelector::set_session (Session* s)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &PluginSelector::set_session), s));
	
	session = s;

	if (session) {
		session->going_away.connect (bind (mem_fun(*this, &PluginSelector::set_session), static_cast<Session*> (0)));
	}
}

void
PluginSelector::_input_refiller (void *arg)
{
	((PluginSelector *) arg)->input_refiller ();
}

/*
void
PluginSelector::_output_refiller (void *arg)
{
	((PluginSelector *) arg)->output_refiller ();
}
*/

int compare(const void *left, const void *right)
{
  return strcmp(*((char**)left), *((char**)right));
}

void
PluginSelector::input_refiller ()
{
	//const gchar *rowdata[4];
	guint row;
	list<PluginInfo *> &plugs = manager->ladspa_plugin_info ();
	list<PluginInfo *>::iterator i;
	char ibuf[16], obuf[16];
	
	// Insert into GTK list
	for (row = 0, i=plugs.begin(); i != plugs.end(); ++i, ++row) {
		//rowdata[0] = (*i)->name.c_str();
		//rowdata[1] = (*i)->category.c_str();

		snprintf (ibuf, sizeof(ibuf)-1, "%d", (*i)->n_inputs);
		snprintf (obuf, sizeof(obuf)-1, "%d", (*i)->n_outputs);		
		
		Gtk::TreeModel::Row newrow = *(lmodel->append());
		newrow[lcols.name] = (*i)->name.c_str();
		newrow[lcols.type] = (*i)->category.c_str();
		newrow[lcols.ins] = ibuf;
		newrow[lcols.outs] = obuf;
		newrow[lcols.plugin] = *i;
		//clist.insert_row (row, rowdata);
		//clist.rows().back().set_data (*i);
	}

 	//clist.set_sort_column (0);
 	//clist.sort ();
}

#ifdef VST_SUPPORT

void
PluginSelector::_vst_refiller (void *arg)
{
	((PluginSelector *) arg)->vst_refiller ();
}

void
PluginSelector::vst_refiller ()
{
	guint row;
	list<PluginInfo *> &plugs = manager->vst_plugin_info ();
	list<PluginInfo *>::iterator i;
	char ibuf[16], obuf[16];
	
	// Insert into GTK list
	for (row = 0, i=plugs.begin(); i != plugs.end(); ++i, ++row) {
		//rowdata[0] = (*i)->name.c_str();
		//rowdata[1] = (*i)->category.c_str();

		snprintf (ibuf, sizeof(ibuf)-1, "%d", (*i)->n_inputs);
		snprintf (obuf, sizeof(obuf)-1, "%d", (*i)->n_outputs);		
		
		Gtk::TreeModel::Row newrow = *(vmodel->append());
		newrow[vcols.name] = (*i)->name.c_str();
		newrow[vcols.ins] = ibuf;
		newrow[vcols.outs] = obuf;
		newrow[vcols.plugin] = i;
	}

 	//clist.set_sort_column (0);
 	//clist.sort ();
}
#endif

/*
void
PluginSelector::output_refiller ()
{
	const gchar *rowdata[2];
	guint row;
	list<PluginInfo*>::iterator i;
	
	// Insert into GTK list

	for (row = 0, i = added_plugins.begin(); i != added_plugins.end(); ++i, ++row){
		rowdata[0] = (*i)->name.c_str();
		clist.insert_row (row, rowdata);
		clist.rows().back().set_data (*i);
	}
}
*/

void
PluginSelector::use_plugin (PluginInfo* pi)
{
	list<PluginInfo *>::iterator i;

	if (pi == 0 || session == 0) {
		return;
	}

	Plugin *plugin = manager->load (*session, pi);

	if (plugin) {
		PluginCreated (plugin);
	}
}

void
PluginSelector::btn_add_clicked()
{
	bool vst = notebook.get_current_page(); // 0 = LADSPA, 1 = VST
	std::string name;
	ARDOUR::PluginInfo *pi;
	Gtk::TreeModel::Row newrow = *(amodel->append());
	
	if (vst) {
#ifdef VST_SUPPORT
		Gtk::TreeModel::Row row = *(vst_display.get_selection()->get_selected());
		name = row[vcols.name];
		pi = row[vcols.plugin];
		added_plugins.push_back (row[vcols.plugin]);
#endif
	} else {
		Gtk::TreeModel::Row row = *(ladspa_display.get_selection()->get_selected());
		name = row[lcols.name];
		pi = row[lcols.plugin];
		added_plugins.push_back (row[lcols.plugin]);
	}
	newrow[acols.text] = name;
	newrow[acols.plugin] = pi;
}

void
PluginSelector::btn_remove_clicked()
{
	if (o_selected_plug > -1){
		gint row = 0;
		list<PluginInfo*>::iterator i = added_plugins.begin();
		while(row < o_selected_plug){
			i++;
			row++;
		}
		added_plugins.erase(i);
		//o_selector.rescan();
		o_selected_plug = -1;
	}
}

// Adds a plugin, and closes the window.
void 
PluginSelector::btn_ok_clicked()
{
	list<PluginInfo*>::iterator i;

	for (i = added_plugins.begin(); i != added_plugins.end(); ++i){
		use_plugin (*i);
	}

	hide();
	added_plugins.clear();
}

void
PluginSelector::btn_cancel_clicked()
{
	hide();
	added_plugins.clear();
}

void
PluginSelector::btn_update_clicked()
{
	manager->refresh ();
	input_refiller ();
}

gint
PluginSelector::wm_close(GdkEventAny* ev)
{
	btn_cancel_clicked();
	return TRUE;
}
