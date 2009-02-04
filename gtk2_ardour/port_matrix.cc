/*
    Copyright (C) 2002-2009 Paul Davis 

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

#include <iostream>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/label.h>
#include <gtkmm/menu.h>
#include <gtkmm/menushell.h>
#include <gtkmm/menu_elems.h>
#include "ardour/bundle.h"
#include "ardour/types.h"
#include "ardour/session.h"
#include "ardour/route.h"
#include "port_matrix.h"
#include "port_matrix_body.h"
#include "i18n.h"

/** PortMatrix constructor.
 *  @param session Our session.
 *  @param type Port type that we are handling.
 */
PortMatrix::PortMatrix (ARDOUR::Session& session, ARDOUR::DataType type)
	: _session (session),
	  _type (type),
	  _column_visibility_box_added (false),
	  _row_visibility_box_added (false),
	  _menu (0),
	  _setup_once (false),
	  _arrangement (TOP_TO_RIGHT),
	  _row_index (0),
	  _column_index (1)
{
	_body = new PortMatrixBody (this);
	
	_ports[0].set_type (type);
	_ports[1].set_type (type);

	_row_visibility_box.pack_start (_row_visibility_label, Gtk::PACK_SHRINK);
	_column_visibility_box.pack_start (_column_visibility_label, Gtk::PACK_SHRINK);

	_hscroll.signal_value_changed().connect (sigc::mem_fun (*this, &PortMatrix::hscroll_changed));
	_vscroll.signal_value_changed().connect (sigc::mem_fun (*this, &PortMatrix::vscroll_changed));

	/* watch for routes being added or removed */
	_session.RouteAdded.connect (sigc::hide (sigc::mem_fun (*this, &PortMatrix::routes_changed)));
	
	reconnect_to_routes ();

	show_all ();
}

PortMatrix::~PortMatrix ()
{
	delete _body;
	
	for (std::vector<Gtk::CheckButton*>::iterator i = _column_visibility_buttons.begin(); i != _column_visibility_buttons.end(); ++i) {
		delete *i;
	}

	for (std::vector<Gtk::CheckButton*>::iterator i = _row_visibility_buttons.begin(); i != _row_visibility_buttons.end(); ++i) {
		delete *i;
	}

	delete _menu;
}

/** Disconnect from and reconnect to routes' signals that we need to watch for things that affect the matrix */
void
PortMatrix::reconnect_to_routes ()
{
	for (std::vector<sigc::connection>::iterator i = _route_connections.begin(); i != _route_connections.end(); ++i) {
		i->disconnect ();
	}

	boost::shared_ptr<ARDOUR::RouteList> routes = _session.get_routes ();
	for (ARDOUR::RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
		_route_connections.push_back (
			(*i)->processors_changed.connect (sigc::mem_fun (*this, &PortMatrix::setup))
			);
	}
}

/** A route has been added to or removed from the session */
void
PortMatrix::routes_changed ()
{
	reconnect_to_routes ();
	setup ();
}

/** Set up everything that changes about the matrix */
void
PortMatrix::setup ()
{
	select_arrangement ();
	_body->setup ();
	setup_scrollbars ();
	queue_draw ();

	if (_setup_once) {

		/* we've set up before, so we need to clean up before re-setting-up */
		/* XXX: we ought to be able to do this by just getting a list of children
		   from each container widget, but I couldn't make that work */

               
		for (std::vector<Gtk::CheckButton*>::iterator i = _column_visibility_buttons.begin(); i != _column_visibility_buttons.end(); ++i) {
			_column_visibility_box.remove (**i);
			delete *i;
		}

		_column_visibility_buttons.clear ();

		for (std::vector<Gtk::CheckButton*>::iterator i = _row_visibility_buttons.begin(); i != _row_visibility_buttons.end(); ++i) {
			_row_visibility_box.remove (**i);
			delete *i;
		}

		_row_visibility_buttons.clear ();
		
		_scroller_table.remove (_vscroll);
		_scroller_table.remove (*_body);
		_scroller_table.remove (_hscroll);

		_main_hbox.remove (_scroller_table);
		if (_row_visibility_box_added) {
			_main_hbox.remove (_row_visibility_box);
		}
		
		if (_column_visibility_box_added) {
			remove (_column_visibility_box);
		}
		remove (_main_hbox);
	}

	if (_column_index == 0) {
		_column_visibility_label.set_text (_("Show Outputs"));
		_row_visibility_label.set_text (_("Show Inputs"));
	} else {
		_column_visibility_label.set_text (_("Show Inputs"));
		_row_visibility_label.set_text (_("Show Outputs"));
	}

	for (PortGroupList::List::const_iterator i = columns()->begin(); i != columns()->end(); ++i) {
		Gtk::CheckButton* b = new Gtk::CheckButton ((*i)->name);
		b->set_active ((*i)->visible());
		boost::weak_ptr<PortGroup> w (*i);
		b->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &PortMatrix::visibility_toggled), w, b));
		_column_visibility_buttons.push_back (b);
		_column_visibility_box.pack_start (*b, Gtk::PACK_SHRINK);
	}

	for (PortGroupList::List::const_iterator i = rows()->begin(); i != rows()->end(); ++i) {
		Gtk::CheckButton* b = new Gtk::CheckButton ((*i)->name);
		b->set_active ((*i)->visible());
		boost::weak_ptr<PortGroup> w (*i);
		b->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &PortMatrix::visibility_toggled), w, b));
		_row_visibility_buttons.push_back (b);
		_row_visibility_box.pack_start (*b, Gtk::PACK_SHRINK);
	}

	if (_arrangement == TOP_TO_RIGHT) {
	  
		_scroller_table.attach (_hscroll, 0, 1, 0, 1, Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK);
		_scroller_table.attach (*_body, 0, 1, 1, 2);
		_scroller_table.attach (_vscroll, 1, 2, 1, 2, Gtk::SHRINK);

		_main_hbox.pack_start (_scroller_table);

		if (rows()->size() > 1) {
			_main_hbox.pack_start (_row_visibility_box, Gtk::PACK_SHRINK);
			_row_visibility_box_added = true;
		} else {
			_row_visibility_box_added = false;
		}

		if (columns()->size() > 1) {
			pack_start (_column_visibility_box, Gtk::PACK_SHRINK);
			_column_visibility_box_added = true;
		} else {
			_column_visibility_box_added = false;
		}
		
		pack_start (_main_hbox);
		
	} else {
		_scroller_table.attach (_vscroll, 0, 1, 0, 1, Gtk::SHRINK);
		_scroller_table.attach (*_body, 1, 2, 0, 1);
		_scroller_table.attach (_hscroll, 1, 2, 1, 2, Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK);

		if (rows()->size() > 1) {
			_main_hbox.pack_start (_row_visibility_box, Gtk::PACK_SHRINK);
			_row_visibility_box_added = true;
		} else {
			_row_visibility_box_added = false;
		}
		
		_main_hbox.pack_start (_scroller_table);

		pack_start (_main_hbox);
		
		if (columns()->size() > 1) {
			pack_start (_column_visibility_box, Gtk::PACK_SHRINK);
			_column_visibility_box_added = true;
		} else {
			_column_visibility_box_added = false;
		}
	}

	_setup_once = true;

	show_all ();
}

void
PortMatrix::set_type (ARDOUR::DataType t)
{
	_type = t;
	_ports[0].set_type (_type);
	_ports[1].set_type (_type);
	setup ();
}

void
PortMatrix::hscroll_changed ()
{
	_body->set_xoffset (_hscroll.get_adjustment()->get_value());
}

void
PortMatrix::vscroll_changed ()
{
	_body->set_yoffset (_vscroll.get_adjustment()->get_value());
}

void
PortMatrix::setup_scrollbars ()
{
	Gtk::Adjustment* a = _hscroll.get_adjustment ();
	a->set_lower (0);
	a->set_upper (_body->full_scroll_width());
	a->set_page_size (_body->alloc_scroll_width());
	a->set_step_increment (32);
	a->set_page_increment (128);

	a = _vscroll.get_adjustment ();
	a->set_lower (0);
	a->set_upper (_body->full_scroll_height());
	a->set_page_size (_body->alloc_scroll_height());
	a->set_step_increment (32);
	a->set_page_increment (128);
}

/** Disassociate all of our ports from each other */
void
PortMatrix::disassociate_all ()
{
	ARDOUR::BundleList a = _ports[0].bundles ();
	ARDOUR::BundleList b = _ports[1].bundles ();
	
	for (ARDOUR::BundleList::iterator i = a.begin(); i != a.end(); ++i) {
		for (uint32_t j = 0; j < (*i)->nchannels(); ++j) {
			for (ARDOUR::BundleList::iterator k = b.begin(); k != b.end(); ++k) {
				for (uint32_t l = 0; l < (*k)->nchannels(); ++l) {
						
					ARDOUR::BundleChannel c[2] = {
						ARDOUR::BundleChannel (*i, j),
						ARDOUR::BundleChannel (*k, l)
							};
					
					set_state (c, false);

				}
			}
		}
	}

	_body->rebuild_and_draw_grid ();
}

/* Decide how to arrange the components of the matrix */
void
PortMatrix::select_arrangement ()
{
	uint32_t const N[2] = {
		_ports[0].total_visible_ports (),
		_ports[1].total_visible_ports ()
	};

	/* The list with the most ports goes on left or right, so that the most port
	   names are printed horizontally and hence more readable.  However we also
	   maintain notional `signal flow' vaguely from left to right.  Subclasses
	   should choose where to put ports based on signal flowing from _ports[0]
	   to _ports[1] */
	
	if (N[0] > N[1]) {

		_row_index = 0;
		_column_index = 1;
		_arrangement = LEFT_TO_BOTTOM;

	} else {

		_row_index = 1;
		_column_index = 0;
		_arrangement = TOP_TO_RIGHT;
	}
}

/** @return columns list */
PortGroupList const *
PortMatrix::columns () const
{
	return &_ports[_column_index];
}

/* @return rows list */
PortGroupList const *
PortMatrix::rows () const
{
	return &_ports[_row_index];
}

/** A group visibility checkbutton has been toggled.
 * @param w Group.
 * @param b Button.
 */
void
PortMatrix::visibility_toggled (boost::weak_ptr<PortGroup> w, Gtk::CheckButton* b)
{
	boost::shared_ptr<PortGroup> g = w.lock ();
	if (!g) {
		return;
	}
	
	g->set_visible (b->get_active());
	_body->setup ();
	setup_scrollbars ();
	queue_draw ();
}

void
PortMatrix::popup_channel_context_menu (int dim, uint32_t N, uint32_t t)
{
	delete _menu;

	_menu = new Gtk::Menu;
	_menu->set_name ("ArdourContextMenu");
	
	Gtk::Menu_Helpers::MenuList& items = _menu->items ();

	ARDOUR::BundleChannel bc;

	ARDOUR::BundleList const r = _ports[dim].bundles();
	for (ARDOUR::BundleList::const_iterator i = r.begin(); i != r.end(); ++i) {
		if (N < (*i)->nchannels ()) {
			bc = ARDOUR::BundleChannel (*i, N);
			break;
		} else {
			N -= (*i)->nchannels ();
		}
	}

	if (bc.bundle) {
		char buf [64];

		if (can_rename_channels (dim)) {
			snprintf (buf, sizeof (buf), _("Rename '%s'..."), bc.bundle->channel_name (bc.channel).c_str());
			boost::weak_ptr<ARDOUR::Bundle> w (bc.bundle);
			items.push_back (
				Gtk::Menu_Helpers::MenuElem (
					buf,
					sigc::bind (sigc::mem_fun (*this, &PortMatrix::rename_channel_proxy), w, bc.channel)
					)
				);
		}

		if (can_remove_channels (dim)) {
			snprintf (buf, sizeof (buf), _("Remove '%s'"), bc.bundle->channel_name (bc.channel).c_str());
			boost::weak_ptr<ARDOUR::Bundle> w (bc.bundle);
			items.push_back (
				Gtk::Menu_Helpers::MenuElem (
					buf,
					sigc::bind (sigc::mem_fun (*this, &PortMatrix::remove_channel_proxy), w, bc.channel)
					)
				);
		}
			
		_menu->popup (1, t);
	}
	
}


void
PortMatrix::remove_channel_proxy (boost::weak_ptr<ARDOUR::Bundle> b, uint32_t c)
{
	boost::shared_ptr<ARDOUR::Bundle> sb = b.lock ();
	if (!sb) {
		return;
	}

	remove_channel (ARDOUR::BundleChannel (sb, c));

}

void
PortMatrix::rename_channel_proxy (boost::weak_ptr<ARDOUR::Bundle> b, uint32_t c)
{
	boost::shared_ptr<ARDOUR::Bundle> sb = b.lock ();
	if (!sb) {
		return;
	}

	rename_channel (ARDOUR::BundleChannel (sb, c));
}

