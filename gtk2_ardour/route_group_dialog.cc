/*
    Copyright (C) 2009 Paul Davis

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

#include <gtkmm/table.h>
#include <gtkmm/stock.h>
#include <gtkmm/messagedialog.h>
#include "ardour/route_group.h"
#include "ardour/session.h"
#include "route_group_dialog.h"
#include "group_tabs.h"
#include "i18n.h"
#include <iostream>

using namespace Gtk;
using namespace ARDOUR;
using namespace std;
using namespace PBD;

RouteGroupDialog::RouteGroupDialog (RouteGroup* g, bool creating_new)
	: ArdourDialog (_("Track/bus Group"))
	, _group (g)
	, _initial_name (g->name ())
	, _active (_("Active"))
	, _gain (_("Gain"))
	, _relative (_("Relative"))
	, _mute (_("Muting"))
	, _solo (_("Soloing"))
	, _rec_enable (_("Record enable"))
	, _select (_("Selection"))
	, _route_active (_("Active state"))
	, _share_color (_("Color"))
	, _share_monitoring (_("Monitoring"))
{
	set_modal (true);
	set_skip_taskbar_hint (true);
	set_resizable (true);
	set_position (Gtk::WIN_POS_MOUSE);
	set_name (N_("RouteGroupDialog"));

	VBox* main_vbox = manage (new VBox);
	Gtk::Label* l;

	get_vbox()->set_spacing (4);

	main_vbox->set_spacing (18);
	main_vbox->set_border_width (5);

	HBox* hbox = manage (new HBox);
	hbox->set_spacing (6);
	l = manage (new Label (_("Name:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false ));

	hbox->pack_start (*l, false, true);
	hbox->pack_start (_name, true, true);

	VBox* top_vbox = manage (new VBox);
	top_vbox->set_spacing (4);

	top_vbox->pack_start (*hbox, false, true);
	top_vbox->pack_start (_active);

	l = manage (new Label (_("Color"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	hbox = manage (new HBox);
	hbox->set_spacing (12);
	hbox->pack_start (*l, false, false);
	hbox->pack_start (_color, false, false);
	top_vbox->pack_start (*hbox, false, false);
	
	main_vbox->pack_start (*top_vbox, false, false);

	_name.set_text (_group->name ());
	_active.set_active (_group->is_active ());
	_color.set_color (GroupTabs::group_color (_group));

	VBox* options_box = manage (new VBox);
	options_box->set_spacing (6);
	
	l = manage (new Label (_("<b>Sharing</b>"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false ));
	l->set_use_markup ();
	options_box->pack_start (*l, false, true);

	_name.signal_activate ().connect (sigc::bind (sigc::mem_fun (*this, &Dialog::response), RESPONSE_OK));

	_gain.set_active (_group->is_gain());
	_relative.set_active (_group->is_relative());
	_mute.set_active (_group->is_mute());
	_solo.set_active (_group->is_solo());
	_rec_enable.set_active (_group->is_recenable());
	_select.set_active (_group->is_select());
	_route_active.set_active (_group->is_route_active());
	_share_color.set_active (_group->is_color());
	_share_monitoring.set_active (_group->is_monitoring());

	_name.signal_changed().connect (sigc::mem_fun (*this, &RouteGroupDialog::update));
	_active.signal_toggled().connect (sigc::mem_fun (*this, &RouteGroupDialog::update));
	_color.signal_color_set().connect (sigc::mem_fun (*this, &RouteGroupDialog::update));
	_gain.signal_toggled().connect (sigc::mem_fun (*this, &RouteGroupDialog::update));
 	_relative.signal_toggled().connect (sigc::mem_fun (*this, &RouteGroupDialog::update));
 	_mute.signal_toggled().connect (sigc::mem_fun (*this, &RouteGroupDialog::update));
 	_solo.signal_toggled().connect (sigc::mem_fun (*this, &RouteGroupDialog::update));
 	_rec_enable.signal_toggled().connect (sigc::mem_fun (*this, &RouteGroupDialog::update));
 	_select.signal_toggled().connect (sigc::mem_fun (*this, &RouteGroupDialog::update));
 	_route_active.signal_toggled().connect (sigc::mem_fun (*this, &RouteGroupDialog::update));
	_share_color.signal_toggled().connect (sigc::mem_fun (*this, &RouteGroupDialog::update));
	_share_monitoring.signal_toggled().connect (sigc::mem_fun (*this, &RouteGroupDialog::update));
	
	gain_toggled ();

	Table* table = manage (new Table (11, 4, false));
	table->set_row_spacings	(6);

	l = manage (new Label ("", Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	l->set_padding (8, 0);
	table->attach (*l, 0, 1, 0, 8, Gtk::FILL, Gtk::FILL, 0, 0);

	table->attach (_gain, 1, 3, 1, 2, Gtk::FILL, Gtk::FILL, 0, 0);

	l = manage (new Label ("", Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	l->set_padding (0, 0);
	table->attach (*l, 1, 2, 2, 3, Gtk::FILL, Gtk::FILL, 0, 0);
	table->attach (_relative, 2, 3, 2, 3, Gtk::FILL, Gtk::FILL, 0, 0);

	table->attach (_mute, 1, 3, 3, 4, Gtk::FILL, Gtk::FILL, 0, 0);
	table->attach (_solo, 1, 3, 4, 5, Gtk::FILL, Gtk::FILL, 0, 0);
	table->attach (_rec_enable, 1, 3, 5, 6, Gtk::FILL, Gtk::FILL, 0, 0);
	table->attach (_select, 1, 3, 6, 7, Gtk::FILL, Gtk::FILL, 0, 0);
	table->attach (_route_active, 1, 3, 7, 8, Gtk::FILL, Gtk::FILL, 0, 0);
	table->attach (_share_color, 1, 3, 8, 9, Gtk::FILL, Gtk::FILL, 0, 0);
	table->attach (_share_monitoring, 1, 3, 9, 10, Gtk::FILL, Gtk::FILL, 0, 0);

	options_box->pack_start (*table, false, true);
	main_vbox->pack_start (*options_box, false, true);

	get_vbox()->pack_start (*main_vbox, false, false);

	_gain.signal_toggled().connect(sigc::mem_fun (*this, &RouteGroupDialog::gain_toggled));

	if (creating_new) {
		add_button (Stock::CANCEL, RESPONSE_CANCEL);
		add_button (Stock::NEW, RESPONSE_OK);
		set_default_response (RESPONSE_OK);
	} else {
		add_button (Stock::CLOSE, RESPONSE_CLOSE);
		set_default_response (RESPONSE_CLOSE);
	}

	show_all_children ();
}

/** @return true if the route group edit was cancelled, otherwise false */
bool
RouteGroupDialog::do_run ()
{
	while (1) {
		int const r = run ();
		if (r == Gtk::RESPONSE_CANCEL) {
			/* cancel, so just bail now */
			return Gtk::RESPONSE_CANCEL;
		}

		if (unique_name ()) {
			/* not cancelled and the name is ok, so all is well */
			return false;
		}

		_group->set_name (_initial_name);
		MessageDialog msg (
			_("A route group of this name already exists.  Please use a different name."),
			false,
			Gtk::MESSAGE_ERROR,
			Gtk::BUTTONS_OK,
			true
			);

		msg.run ();
	}

	/* NOTREACHED */
	return false;
}

void
RouteGroupDialog::update ()
{
	PropertyList plist;

	plist.add (Properties::gain, _gain.get_active());
	plist.add (Properties::recenable, _rec_enable.get_active());
	plist.add (Properties::mute, _mute.get_active());
	plist.add (Properties::solo, _solo.get_active ());
	plist.add (Properties::select, _select.get_active());
	plist.add (Properties::route_active, _route_active.get_active());
	plist.add (Properties::relative, _relative.get_active());
	plist.add (Properties::active, _active.get_active());
	plist.add (Properties::name, string (_name.get_text()));
	plist.add (Properties::color, _share_color.get_active());
	plist.add (Properties::monitoring, _share_monitoring.get_active());

	_group->apply_changes (plist);
	
	GroupTabs::set_group_color (_group, _color.get_color ());
}

void
RouteGroupDialog::gain_toggled ()
{
	_relative.set_sensitive (_gain.get_active ());
}

/** @return true if the current group's name is unique accross the session */
bool
RouteGroupDialog::unique_name () const
{
	list<RouteGroup*> route_groups = _group->session().route_groups ();
	list<RouteGroup*>::iterator i = route_groups.begin ();
	while (i != route_groups.end() && ((*i)->name() != _name.get_text() || *i == _group)) {
		++i;
	}

	return (i == route_groups.end ());
}
