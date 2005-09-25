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

#include <gtk--/table.h>
#include <gtk--/button.h>
#include <gtk--/notebook.h>
#include <gtk--/ctree.h>

#include <ardour/plugin_manager.h>
#include <ardour/plugin.h>
#include <ardour/configuration.h>

#include "ardour_ui.h"
#include "plugin_selector.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;

static const gchar *i_titles[] = {
	N_("Available LADSPA plugins"), 
	N_("Type"),
	N_("# Inputs"), 
	N_("# Outputs"),
	0
};

#ifdef VST_SUPPORT
static const gchar *vst_titles[] = {
	N_("Available VST plugins"), 
	N_("# Inputs"), 
	N_("# Outputs"),
	0
};
#endif

static const gchar *o_titles[] = {
	N_("To be added"),
	0
};

PluginSelector::PluginSelector (PluginManager *mgr)
	: ArdourDialog ("plugin selector"),
	  ladspa_display (_input_refiller, this, internationalize (i_titles), false, true),
#ifdef VST_SUPPORT
	 vst_display (_vst_refiller, this, internationalize (vst_titles), false, true),
#endif
	  o_selector (_output_refiller, this, internationalize (o_titles), false, true)
{
	set_position (GTK_WIN_POS_MOUSE);
	set_name ("PluginSelectorWindow");
	set_title (_("ardour: plugins"));
	set_modal(true);
	add_events (GDK_KEY_PRESS_MASK|GDK_KEY_RELEASE_MASK);

	manager = mgr;
	session = 0;
	o_selected_plug = -1;
	i_selected_plug = 0;

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
	table->set_usize(750, 500);
	table->attach(notebook, 0, 7, 0, 5);

	table->attach(*btn_add, 1, 2, 5, 6, GTK_FILL, 0, 5, 5);
	table->attach(*btn_remove, 3, 4, 5, 6, GTK_FILL, 0, 5, 5);
	table->attach(*btn_update, 5, 6, 5, 6, GTK_FILL, 0, 5, 5);

	table->attach(o_selector, 0, 7, 7, 9);
	table->attach(*btn_ok, 1, 3, 9, 10, GTK_FILL, 0, 5, 5);
	table->attach(*btn_cancel, 3, 4, 9, 10, GTK_FILL, 0, 5, 5);
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
	ladspa_display.clist().set_name("PluginSelectorList");
	o_selector.clist().set_name("PluginSelectorList");
	
	ladspa_display.clist().column_titles_active();
	ladspa_display.clist().column(0).set_auto_resize (false);
	ladspa_display.clist().column(0).set_width(470);

	ladspa_display.clist().column(1).set_auto_resize (true);
	o_selector.clist().column(0).set_auto_resize (true);

	ladspa_display.selection_made.connect (slot(*this, &PluginSelector::i_plugin_selected));
	ladspa_display.choice_made.connect(slot(*this, &PluginSelector::i_plugin_chosen));
	ladspa_display.clist().click_column.connect(bind (slot(*this, &PluginSelector::column_clicked), ladspa_display.clist().gtkobj()));
#ifdef VST_SUPPORT
	if (Config->get_use_vst()) {
		vst_display.selection_made.connect (slot(*this, &PluginSelector::i_plugin_selected));
		vst_display.choice_made.connect(slot(*this, &PluginSelector::i_plugin_chosen));
		vst_display.clist().click_column.connect(bind (slot(*this, &PluginSelector::column_clicked), vst_display.clist().gtkobj()));
	}
#endif
	o_selector.selection_made.connect(slot(*this, &PluginSelector::o_plugin_selected));
	o_selector.choice_made.connect(slot(*this,&PluginSelector::o_plugin_chosen));
	btn_update->clicked.connect (slot(*this, &PluginSelector::btn_update_clicked));
	btn_add->clicked.connect(slot(*this, &PluginSelector::btn_add_clicked));
	btn_remove->clicked.connect(slot(*this, &PluginSelector::btn_remove_clicked));
	btn_ok->clicked.connect(slot(*this, &PluginSelector::btn_ok_clicked));
	btn_cancel->clicked.connect(slot(*this,&PluginSelector::btn_cancel_clicked));
	delete_event.connect (slot (*this, &PluginSelector::wm_close));

}

void
PluginSelector::set_session (Session* s)
{
	ENSURE_GUI_THREAD(bind (slot (*this, &PluginSelector::set_session), s));
	
	session = s;

	if (session) {
		session->going_away.connect (bind (slot (*this, &PluginSelector::set_session), static_cast<Session*> (0)));
	}
}

void
PluginSelector::_input_refiller (Gtk::CList &list, void *arg)
{
	((PluginSelector *) arg)->input_refiller (list);
}

void
PluginSelector::_output_refiller (Gtk::CList &list, void *arg)
{
	((PluginSelector *) arg)->output_refiller (list);
}

int compare(const void *left, const void *right)
{
  return strcmp(*((char**)left), *((char**)right));
}

void
PluginSelector::input_refiller (Gtk::CList &clist)
{
	const gchar *rowdata[4];
	guint row;
	list<PluginInfo *> &plugs = manager->ladspa_plugin_info ();
	list<PluginInfo *>::iterator i;
	char ibuf[16], obuf[16];
	
	// Insert into GTK list
	for (row = 0, i=plugs.begin(); i != plugs.end(); ++i, ++row) {
		rowdata[0] = (*i)->name.c_str();
		rowdata[1] = (*i)->category.c_str();

		snprintf (ibuf, sizeof(ibuf)-1, "%d", (*i)->n_inputs);
		snprintf (obuf, sizeof(obuf)-1, "%d", (*i)->n_outputs);		
		rowdata[2] = ibuf;
		rowdata[3] = obuf;
		
		clist.insert_row (row, rowdata);
		clist.rows().back().set_data (*i);
	}

 	clist.set_sort_column (0);
 	clist.sort ();
}

#ifdef VST_SUPPORT

void
PluginSelector::_vst_refiller (Gtk::CList &list, void *arg)
{
	((PluginSelector *) arg)->vst_refiller (list);
}

void
PluginSelector::vst_refiller (Gtk::CList &clist)
{
	const gchar *rowdata[3];
	guint row;
	list<PluginInfo *> &plugs = manager->vst_plugin_info ();
	list<PluginInfo *>::iterator i;
	char ibuf[16], obuf[16];
	
	if (!Config->get_use_vst()) {
		return;
	}

	// Insert into GTK list

	for (row = 0, i = plugs.begin(); i != plugs.end(); ++i, ++row) {
		rowdata[0] = (*i)->name.c_str();

		snprintf (ibuf, sizeof(ibuf)-1, "%d", (*i)->n_inputs);
		snprintf (obuf, sizeof(obuf)-1, "%d", (*i)->n_outputs);		
		rowdata[1] = ibuf;
		rowdata[2] = obuf;
		
		clist.insert_row (row, rowdata);
		clist.rows().back().set_data (*i);
	}

 	clist.set_sort_column (0);
 	clist.sort ();
}
#endif

void
PluginSelector::output_refiller (Gtk::CList &clist)
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

void
PluginSelector::i_plugin_chosen (Gtkmmext::Selector *selector,
				 Gtkmmext::SelectionResult *res)
{
	if (res) {
		// get text for name column (0)
		i_selected_plug = static_cast<PluginInfo*> (selector->clist().row(res->row).get_data());
		//i_selected_plug = *res->text;
	} else {
		i_selected_plug = 0;
	}
}

void
PluginSelector::i_plugin_selected (Gtkmmext::Selector *selector,
				   Gtkmmext::SelectionResult *res)
{
	if (res) {
		added_plugins.push_back (static_cast<PluginInfo*> (selector->clist().row(res->row).get_data()));
		//added_plugins.push_back(*(res->text));
		o_selector.rescan();
	}
}

void
PluginSelector::o_plugin_chosen (Gtkmmext::Selector *selector,
			      Gtkmmext::SelectionResult *res)
{
	if (res && res->text) {
		o_selected_plug = res->row;
	} else {
		o_selected_plug = -1;
	}

}

void
PluginSelector::o_plugin_selected (Gtkmmext::Selector *selector,
				Gtkmmext::SelectionResult *res)
{
	if(res && res->text){
		gint row = 0;
		list<PluginInfo*>::iterator i = added_plugins.begin();
		while (row < res->row){
			i++;
			row++;
		}
		added_plugins.erase(i);
		o_selector.rescan();
		o_selected_plug = -1;
	}
}

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
	if (i_selected_plug) {
		added_plugins.push_back (i_selected_plug);
		o_selector.rescan();
	}
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
		o_selector.rescan();
		o_selected_plug = -1;
	}
}

// Adds a plugin, and closes the window.
void 
PluginSelector::btn_ok_clicked()
{
	using namespace Gtk::CList_Helpers;

	list<PluginInfo*>::iterator i;

	for (i = added_plugins.begin(); i != added_plugins.end(); ++i){
		use_plugin (*i);
	}

	hide();
	added_plugins.clear();
	o_selector.rescan();
	i_selected_plug = 0;
	o_selected_plug = -1;

	SelectionList s_list = ladspa_display.clist().selection();
	SelectionList::iterator s = s_list.begin();
	if (s != s_list.end()) {
		(*s).unselect();
	}

#ifdef VST_SUPPORT
	SelectionList v_list = vst_display.clist().selection();
	SelectionList::iterator v = v_list.begin();
	if (v != v_list.end()) {
		(*v).unselect();
	}
#endif
}

void
PluginSelector::btn_cancel_clicked()
{
	hide();
	added_plugins.clear();
	o_selector.rescan();
	i_selected_plug = 0;
	o_selected_plug = -1;
}

void
PluginSelector::btn_update_clicked()
{
	manager->refresh ();
	ladspa_display.rescan ();
}

gint
PluginSelector::wm_close(GdkEventAny* ev)
{
	btn_cancel_clicked();
	return TRUE;
}

void
PluginSelector::column_clicked (int column, GtkCList* clist)
{
	gtk_clist_set_sort_column (clist, column);
	gtk_clist_sort (clist);
}
