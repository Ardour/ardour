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
#include <gtkmm/stock.h>
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
using namespace Gtk;

PluginSelector::PluginSelector (PluginManager *mgr)
	: ArdourDialog (_("ardour: plugins"), true, false)
{
	set_position (Gtk::WIN_POS_MOUSE);
	set_name ("PluginSelectorWindow");
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);

	manager = mgr;
	session = 0;
	o_selected_plug = -1;
	i_selected_plug = 0;

	lmodel = Gtk::ListStore::create(lcols);
	ladspa_display.set_model (lmodel);
	ladspa_display.append_column (_("Available LADSPA Plugins"), lcols.name);
	ladspa_display.append_column (_("Type"), lcols.type);
	ladspa_display.append_column (_("# Inputs"),lcols.ins);
	ladspa_display.append_column (_("# Outputs"), lcols.outs);
	ladspa_display.set_headers_visible (true);
	ladspa_display.set_reorderable (false);
	lscroller.set_border_width(10);
	lscroller.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	lscroller.add(ladspa_display);

	amodel = Gtk::ListStore::create(acols);
	added_list.set_model (amodel);
	added_list.append_column (_("Plugins to be Connected to Insert"), acols.text);
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
	vscroller.set_border_width(10);
	vscroller.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	vscroller.add(vst_display);

	for (int i = 0; i <=2; i++) {
		Gtk::TreeView::Column* column = vst_display.get_column(i);
		column->set_sort_column(i);
	}
#endif
	ascroller.set_border_width(10);
	ascroller.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	ascroller.add(added_list);
	Gtk::Button *btn_add = manage(new Gtk::Button(Stock::ADD));
	ARDOUR_UI::instance()->tooltips().set_tip(*btn_add, _("Add a plugin to the effect list"));
	Gtk::Button *btn_remove = manage(new Gtk::Button(Stock::REMOVE));
	ARDOUR_UI::instance()->tooltips().set_tip(*btn_remove, _("Remove a plugin from the effect list"));
	Gtk::Button *btn_update = manage(new Gtk::Button(Stock::REFRESH));
	ARDOUR_UI::instance()->tooltips().set_tip(*btn_update, _("Update available plugins"));

	btn_add->set_name("PluginSelectorButton");
	btn_remove->set_name("PluginSelectorButton");

	Gtk::Table* table = manage(new Gtk::Table(7, 10));
	table->set_size_request(750, 500);
	table->attach(notebook, 0, 7, 0, 5);

	table->attach(*btn_add, 1, 2, 5, 6, Gtk::FILL, Gtk::FILL, 5, 5);
	table->attach(*btn_remove, 3, 4, 5, 6, Gtk::FILL, Gtk::FILL, 5, 5);
	table->attach(*btn_update, 5, 6, 5, 6, Gtk::FILL, Gtk::FILL, 5, 5);

	table->attach(ascroller, 0, 7, 7, 9);

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::CONNECT, RESPONSE_APPLY);
	set_default_response (RESPONSE_APPLY);

	get_vbox()->pack_start (*table);

	using namespace Gtk::Notebook_Helpers;
	notebook.pages().push_back (TabElem (lscroller, _("LADSPA")));
#ifdef VST_SUPPORT
	if (Config->get_use_vst()) {
		notebook.pages().push_back (TabElem (vscroller, _("VST")));
	}
#endif

	table->set_name("PluginSelectorTable");
	ladspa_display.set_name("PluginSelectorDisplay");
	//ladspa_display.set_name("PluginSelectorList");
	added_list.set_name("PluginSelectorList");

	ladspa_display.signal_button_press_event().connect_notify (mem_fun(*this, &PluginSelector::row_clicked));
#ifdef VST_SUPPORT
	if (Config->get_use_vst()) {
		vst_display.signal_button_press_event().connect_notify (mem_fun(*this, &PluginSelector::row_clicked));
	}
#endif
	
	btn_update->signal_clicked().connect (mem_fun(*this, &PluginSelector::btn_update_clicked));
	btn_add->signal_clicked().connect(mem_fun(*this, &PluginSelector::btn_add_clicked));
	btn_remove->signal_clicked().connect(mem_fun(*this, &PluginSelector::btn_remove_clicked));

	input_refiller ();
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

int compare(const void *left, const void *right)
{
  return strcmp(*((char**)left), *((char**)right));
}

void
PluginSelector::input_refiller ()
{
	guint row;
	list<PluginInfo *> &plugs = manager->ladspa_plugin_info ();
	list<PluginInfo *>::iterator i;
	char ibuf[16], obuf[16];
	lmodel->clear();
#ifdef VST_SUPPORT
	vmodel->clear();
#endif
	// Insert into GTK list
	for (row = 0, i=plugs.begin(); i != plugs.end(); ++i, ++row) {
		snprintf (ibuf, sizeof(ibuf)-1, "%d", (*i)->n_inputs);
		snprintf (obuf, sizeof(obuf)-1, "%d", (*i)->n_outputs);		
		
		Gtk::TreeModel::Row newrow = *(lmodel->append());
		newrow[lcols.name] = (*i)->name.c_str();
		newrow[lcols.type] = (*i)->category.c_str();
		newrow[lcols.ins] = ibuf;
		newrow[lcols.outs] = obuf;
		newrow[lcols.plugin] = *i;
	}

	lmodel->set_sort_column (0, Gtk::SORT_ASCENDING);
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

		snprintf (ibuf, sizeof(ibuf)-1, "%d", (*i)->n_inputs);
		snprintf (obuf, sizeof(obuf)-1, "%d", (*i)->n_outputs);		
		
		Gtk::TreeModel::Row newrow = *(vmodel->append());
		newrow[vcols.name] = (*i)->name.c_str();
		newrow[vcols.ins] = ibuf;
		newrow[vcols.outs] = obuf;
		newrow[vcols.plugin] = *i;
	}
	vmodel->set_sort_column (0, Gtk::SORT_ASCENDING);
}
#endif

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
	list<PluginInfo*>::iterator i;
	Gtk::TreeModel::iterator iter = added_list.get_selection()->get_selected();
	for (i = added_plugins.begin(); (*i) != (*iter)[acols.plugin]; ++i);

	added_plugins.erase(i);	
	amodel->erase(iter);
}

void
PluginSelector::btn_update_clicked()
{
	manager->refresh ();
	input_refiller ();
}

int
PluginSelector::run ()
{
	ResponseType r;
	list<PluginInfo*>::iterator i;

	r = (ResponseType) Dialog::run ();

	switch (r) {
	case RESPONSE_APPLY:
		for (i = added_plugins.begin(); i != added_plugins.end(); ++i){
			use_plugin (*i);
		}
		break;

	default:
		break;
	}

	cleanup ();

	return (int) r;
}

void
PluginSelector::cleanup ()
{
	hide();
	added_plugins.clear();
	amodel->clear();
}

