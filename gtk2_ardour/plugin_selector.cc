/*
    Copyright (C) 2000-2006 Paul Davis 

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
using namespace PBD;
using namespace Gtk;

PluginSelector::PluginSelector (PluginManager *mgr)
	: ArdourDialog (_("ardour: plugins"), true, false)
{
	set_position (Gtk::WIN_POS_MOUSE);
	set_name ("PluginSelectorWindow");
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);

	manager = mgr;
	session = 0;
	
	current_selection = ARDOUR::LADSPA;

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

#ifdef HAVE_COREAUDIO
	aumodel = ListStore::create(aucols);
	au_display.set_model (aumodel);
	au_display.append_column (_("Available plugins"), aucols.name);
	au_display.append_column (_("# Inputs"), aucols.ins);
	au_display.append_column (_("# Outputs"), aucols.outs);
	au_display.set_headers_visible (true);
	au_display.set_reorderable (false);
	auscroller.set_border_width(10);
	auscroller.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	auscroller.add(au_display);

	for (int i = 0; i <=2; i++) {
		Gtk::TreeView::Column* column = au_display.get_column(i);
		column->set_sort_column(i);
	}
#endif

	ascroller.set_border_width(10);
	ascroller.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	ascroller.add(added_list);
	btn_add = manage(new Gtk::Button(Stock::ADD));
	ARDOUR_UI::instance()->tooltips().set_tip(*btn_add, _("Add a plugin to the effect list"));
	btn_add->set_sensitive (false);
	btn_remove = manage(new Gtk::Button(Stock::REMOVE));
	btn_remove->set_sensitive (false);
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
	set_response_sensitive (RESPONSE_APPLY, false);
	get_vbox()->pack_start (*table);

	// Notebook tab order must be the same in here as in set_correct_focus()
	using namespace Gtk::Notebook_Helpers;
	notebook.pages().push_back (TabElem (lscroller, _("LADSPA")));

#ifdef VST_SUPPORT
	if (Config->get_use_vst()) {
		notebook.pages().push_back (TabElem (vscroller, _("VST")));
	}
#endif

#ifdef HAVE_COREAUDIO
	notebook.pages().push_back (TabElem (auscroller, _("AudioUnit")));
#endif

	table->set_name("PluginSelectorTable");
	ladspa_display.set_name("PluginSelectorDisplay");
	//ladspa_display.set_name("PluginSelectorList");
	added_list.set_name("PluginSelectorList");

	ladspa_display.signal_button_press_event().connect_notify (mem_fun(*this, &PluginSelector::row_clicked));
	ladspa_display.get_selection()->signal_changed().connect (mem_fun(*this, &PluginSelector::ladspa_display_selection_changed));
	ladspa_display.grab_focus();
	
#ifdef VST_SUPPORT
	if (Config->get_use_vst()) {
		vst_display.signal_button_press_event().connect_notify (mem_fun(*this, &PluginSelector::row_clicked));
		vst_display.get_selection()->signal_changed().connect (mem_fun(*this, &PluginSelector::vst_display_selection_changed));
	}
#endif

#ifdef HAVE_COREAUDIO
	au_display.signal_button_press_event().connect_notify (mem_fun(*this, &PluginSelector::row_clicked));
	au_display.get_selection()->signal_changed().connect (mem_fun(*this, &PluginSelector::au_display_selection_changed));
#endif

	btn_update->signal_clicked().connect (mem_fun(*this, &PluginSelector::btn_update_clicked));
	btn_add->signal_clicked().connect(mem_fun(*this, &PluginSelector::btn_add_clicked));
	btn_remove->signal_clicked().connect(mem_fun(*this, &PluginSelector::btn_remove_clicked));
	added_list.get_selection()->signal_changed().connect (mem_fun(*this, &PluginSelector::added_list_selection_changed));

	input_refiller ();
	
#ifdef VST_SUPPORT
	vst_refiller ();
#endif

#ifdef HAVE_COREAUDIO
	au_refiller ();
#endif

	signal_show().connect (mem_fun (*this, &PluginSelector::set_correct_focus));
}

/**
 * Makes sure keyboard focus is always in the plugin list
 * of the selected notebook tab.
 **/
void
PluginSelector::set_correct_focus()
{
	int cp = notebook.get_current_page();

	if (cp == 0) {
		ladspa_display.grab_focus();
		return;
	}

#ifdef VST_SUPPORT
	if (Config->get_use_vst()) {
		cp--;
	
		if (cp == 0) {
			vst_display.grab_focus();
			return;
		}
	}
#endif

#ifdef HAVE_COREAUDIO
	cp--;

	if (cp == 0) {
		au_display.grab_focus();
		return;
	}
#endif
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
		session->GoingAway.connect (bind (mem_fun(*this, &PluginSelector::set_session), static_cast<Session*> (0)));
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
	PluginInfoList &plugs = manager->ladspa_plugin_info ();
	PluginInfoList::iterator i;
	char ibuf[16], obuf[16];
	lmodel->clear();

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
	PluginInfoList &plugs = manager->vst_plugin_info ();
	PluginInfoList::iterator i;
	char ibuf[16], obuf[16];
	vmodel->clear();
	
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

void
PluginSelector::vst_display_selection_changed()
{
	if (vst_display.get_selection()->count_selected_rows() != 0) {
		btn_add->set_sensitive (true);
	} else {
		btn_add->set_sensitive (false);
	}

	current_selection = ARDOUR::VST;
}

#endif //VST_SUPPORT

#ifdef HAVE_COREAUDIO

void
PluginSelector::_au_refiller (void *arg)
{
	((PluginSelector *) arg)->au_refiller ();
}

void
PluginSelector::au_refiller ()
{
	guint row;
	PluginInfoList plugs (AUPluginInfo::discover ());
	PluginInfoList::iterator i;
	char ibuf[16], obuf[16];
	aumodel->clear();
	
	// Insert into GTK list
	for (row = 0, i=plugs.begin(); i != plugs.end(); ++i, ++row) {

		snprintf (ibuf, sizeof(ibuf)-1, "%d", (*i)->n_inputs);
		snprintf (obuf, sizeof(obuf)-1, "%d", (*i)->n_outputs);		
		
		Gtk::TreeModel::Row newrow = *(aumodel->append());
		newrow[aucols.name] = (*i)->name.c_str();
		newrow[aucols.ins] = ibuf;
		newrow[aucols.outs] = obuf;
		newrow[aucols.plugin] = *i;
	}
	aumodel->set_sort_column (0, Gtk::SORT_ASCENDING);
}

void
PluginSelector::au_display_selection_changed()
{
	if (au_display.get_selection()->count_selected_rows() != 0) {
		btn_add->set_sensitive (true);
	} else {
		btn_add->set_sensitive (false);
	}
	
	current_selection = ARDOUR::AudioUnit;
}

#endif //HAVE_COREAUDIO

void
PluginSelector::use_plugin (PluginInfoPtr pi)
{
	if (session == 0) {
		return;
	}

	PluginPtr plugin = pi->load (*session);

	if (plugin) {
		PluginCreated (plugin);
	}
}

void
PluginSelector::btn_add_clicked()
{
	std::string name;
	PluginInfoPtr pi;
	Gtk::TreeModel::Row newrow = *(amodel->append());
	
	Gtk::TreeModel::Row row;

	switch (current_selection) {
		case ARDOUR::LADSPA:
			row = *(ladspa_display.get_selection()->get_selected());
			name = row[lcols.name];
			pi = row[lcols.plugin];
			break;
		case ARDOUR::VST:
#ifdef VST_SUPPORT
			row = *(vst_display.get_selection()->get_selected());
			name = row[vcols.name];
			pi = row[vcols.plugin];
#endif
			break;
		case ARDOUR::AudioUnit:
#ifdef HAVE_COREAUDIO
			row = *(au_display.get_selection()->get_selected());
			name = row[aucols.name];
			pi = row[aucols.plugin];
#endif
			break;
		default:
			error << "Programming error.  Unknown plugin selected." << endmsg;
			return;
	}

	newrow[acols.text] = name;
	newrow[acols.plugin] = pi;

	if (!amodel->children().empty()) {
		set_response_sensitive (RESPONSE_APPLY, true);
	}
}

void
PluginSelector::btn_remove_clicked()
{
	Gtk::TreeModel::iterator iter = added_list.get_selection()->get_selected();
	
	amodel->erase(iter);
	if (amodel->children().empty()) {
		set_response_sensitive (RESPONSE_APPLY, false);
	}
}

void
PluginSelector::btn_update_clicked()
{
	manager->refresh ();
	input_refiller ();
#ifdef VST_SUPPORT
	vst_refiller ();
#endif	
#ifdef HAVE_COREAUDIO
	au_refiller ();
#endif
}

void
PluginSelector::ladspa_display_selection_changed()
{
	if (ladspa_display.get_selection()->count_selected_rows() != 0) {
		btn_add->set_sensitive (true);
	} else {
		btn_add->set_sensitive (false);
	}
	
	current_selection = ARDOUR::LADSPA;
}

void
PluginSelector::added_list_selection_changed()
{
  if (added_list.get_selection()->count_selected_rows() != 0) {
    btn_remove->set_sensitive (true);
  } else {
    btn_remove->set_sensitive (false);
  }
}

int
PluginSelector::run ()
{
	ResponseType r;
	TreeModel::Children::iterator i;

	r = (ResponseType) Dialog::run ();

	switch (r) {
	case RESPONSE_APPLY:
		for (i = amodel->children().begin(); i != amodel->children().end(); ++i) {
			use_plugin ((*i)[acols.plugin]);
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
	amodel->clear();
}
