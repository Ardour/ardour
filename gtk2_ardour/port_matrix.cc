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
#include <gtkmm/window.h>
#include "ardour/bundle.h"
#include "ardour/types.h"
#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/audioengine.h"
#include "port_matrix.h"
#include "port_matrix_body.h"
#include "port_matrix_component.h"
#include "i18n.h"

using namespace std;
using namespace sigc;
using namespace Gtk;
using namespace ARDOUR;

/** PortMatrix constructor.
 *  @param session Our session.
 *  @param type Port type that we are handling.
 */
PortMatrix::PortMatrix (Window* parent, Session& session, DataType type)
	: Table (2, 2),
	  _session (session),
	  _parent (parent),
	  _type (type),
	  _menu (0),
	  _arrangement (TOP_TO_RIGHT),
	  _row_index (0),
	  _column_index (1),
	  _min_height_divisor (1),
	  _show_only_bundles (false),
	  _inhibit_toggle_show_only_bundles (false)
{
	_body = new PortMatrixBody (this);

	for (int i = 0; i < 2; ++i) {
		_ports[i].set_type (type);
		
		/* watch for the content of _ports[] changing */
		_ports[i].Changed.connect (mem_fun (*this, &PortMatrix::setup));
	}

	_hscroll.signal_value_changed().connect (mem_fun (*this, &PortMatrix::hscroll_changed));
	_vscroll.signal_value_changed().connect (mem_fun (*this, &PortMatrix::vscroll_changed));

	/* watch for routes being added or removed */
	_session.RouteAdded.connect (sigc::hide (mem_fun (*this, &PortMatrix::routes_changed)));

	/* and also bundles */
	_session.BundleAdded.connect (sigc::hide (mem_fun (*this, &PortMatrix::setup_global_ports)));

	/* and also ports */
	_session.engine().PortRegisteredOrUnregistered.connect (mem_fun (*this, &PortMatrix::setup_all_ports));
	
	reconnect_to_routes ();

	attach (*_body, 0, 1, 0, 1);
	attach (_vscroll, 1, 2, 0, 1, SHRINK);
	attach (_hscroll, 0, 1, 1, 2, FILL | EXPAND, SHRINK);
	
	show_all ();
}

PortMatrix::~PortMatrix ()
{
	delete _body;
	delete _menu;
}

/** Disconnect from and reconnect to routes' signals that we need to watch for things that affect the matrix */
void
PortMatrix::reconnect_to_routes ()
{
	for (vector<connection>::iterator i = _route_connections.begin(); i != _route_connections.end(); ++i) {
		i->disconnect ();
	}
	_route_connections.clear ();

	boost::shared_ptr<RouteList> routes = _session.get_routes ();
	for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
		_route_connections.push_back (
			(*i)->processors_changed.connect (mem_fun (*this, &PortMatrix::setup_global_ports))
			);
	}
}

/** A route has been added to or removed from the session */
void
PortMatrix::routes_changed ()
{
	reconnect_to_routes ();
	setup_global_ports ();
}

/** Set up everything that depends on the content of _ports[] */
void
PortMatrix::setup ()
{
	if ((get_flags () & Gtk::REALIZED) == 0) {
		select_arrangement ();
	}

	_body->setup ();
	setup_scrollbars ();
	queue_draw ();

	show_all ();
}

void
PortMatrix::set_type (DataType t)
{
	_type = t;
	_ports[0].set_type (_type);
	_ports[1].set_type (_type);
	
	setup_all_ports ();
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
	Adjustment* a = _hscroll.get_adjustment ();
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
	PortGroup::BundleList a = _ports[0].bundles ();
	PortGroup::BundleList b = _ports[1].bundles ();

	for (PortGroup::BundleList::iterator i = a.begin(); i != a.end(); ++i) {
		for (uint32_t j = 0; j < i->bundle->nchannels(); ++j) {
			for (PortGroup::BundleList::iterator k = b.begin(); k != b.end(); ++k) {
				for (uint32_t l = 0; l < k->bundle->nchannels(); ++l) {
						
					BundleChannel c[2] = {
						BundleChannel (i->bundle, j),
						BundleChannel (k->bundle, l)
							};

					if (get_state (c) == PortMatrixNode::ASSOCIATED) {
						set_state (c, false);
					}

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
		_ports[0].total_visible_channels (),
		_ports[1].total_visible_channels ()
	};

	/* The list with the most channels goes on left or right, so that the most channel
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

void
PortMatrix::popup_menu (
	pair<boost::shared_ptr<PortGroup>, BundleChannel> column,
	pair<boost::shared_ptr<PortGroup>, BundleChannel> row,
	uint32_t t
	)
{
	using namespace Menu_Helpers;
	
	delete _menu;

	_menu = new Menu;
	_menu->set_name ("ArdourContextMenu");
	
	MenuList& items = _menu->items ();

	boost::shared_ptr<PortGroup> pg[2];
	pg[_column_index] = column.first;
	pg[_row_index] = row.first;

	BundleChannel bc[2];
	bc[_column_index] = column.second;
	bc[_row_index] = row.second;

	char buf [64];
	bool need_separator = false;

	for (int dim = 0; dim < 2; ++dim) {

		if (bc[dim].bundle) {

			Menu* m = manage (new Menu);
			MenuList& sub = m->items ();

			boost::weak_ptr<Bundle> w (bc[dim].bundle);

			if (can_add_channel (bc[dim].bundle)) {
				snprintf (buf, sizeof (buf), _("Add %s"), channel_noun().c_str());
				sub.push_back (MenuElem (buf, bind (mem_fun (*this, &PortMatrix::add_channel_proxy), w)));
			}
			
			
			if (can_rename_channels (bc[dim].bundle)) {
				snprintf (buf, sizeof (buf), _("Rename '%s'..."), bc[dim].bundle->channel_name (bc[dim].channel).c_str());
				sub.push_back (
					MenuElem (
						buf,
						bind (mem_fun (*this, &PortMatrix::rename_channel_proxy), w, bc[dim].channel)
						)
					);
			}

			sub.push_back (SeparatorElem ());
			
			if (can_remove_channels (bc[dim].bundle)) {
				snprintf (buf, sizeof (buf), _("Remove '%s'"), bc[dim].bundle->channel_name (bc[dim].channel).c_str());
				sub.push_back (
					MenuElem (
						buf,
						bind (mem_fun (*this, &PortMatrix::remove_channel_proxy), w, bc[dim].channel)
						)
					);
			}			

			if (_show_only_bundles) {
				snprintf (buf, sizeof (buf), _("%s all"), disassociation_verb().c_str());
			} else {
				snprintf (
					buf, sizeof (buf), _("%s all from '%s'"),
					disassociation_verb().c_str(),
					bc[dim].bundle->channel_name (bc[dim].channel).c_str()
					);
			}
			
			sub.push_back (
				MenuElem (buf, bind (mem_fun (*this, &PortMatrix::disassociate_all_on_channel), w, bc[dim].channel, dim))
				);

			items.push_back (MenuElem (bc[dim].bundle->name().c_str(), *m));
			need_separator = true;
		}

	}

	if (need_separator) {
		items.push_back (SeparatorElem ());
	}

	need_separator = false;
	
	for (int dim = 0; dim < 2; ++dim) {

		if (pg[dim]) {

			boost::weak_ptr<PortGroup> wp (pg[dim]);
			
			if (pg[dim]->visible()) {
				if (dim == 0) {
					if (pg[dim]->name.empty()) {
						snprintf (buf, sizeof (buf), _("Hide sources"));
					} else {
						snprintf (buf, sizeof (buf), _("Hide '%s' sources"), pg[dim]->name.c_str());
					}
				} else {
					if (pg[dim]->name.empty()) {
						snprintf (buf, sizeof (buf), _("Hide destinations"));
					} else {
						snprintf (buf, sizeof (buf), _("Hide '%s' destinations"), pg[dim]->name.c_str());
					}
				}

				items.push_back (MenuElem (buf, bind (mem_fun (*this, &PortMatrix::hide_group), wp)));
			} else {
				if (dim == 0) {
					if (pg[dim]->name.empty()) {
						snprintf (buf, sizeof (buf), _("Show sources"));
					} else {
						snprintf (buf, sizeof (buf), _("Show '%s' sources"), pg[dim]->name.c_str());
					}
				} else {
					if (pg[dim]->name.empty()) {
						snprintf (buf, sizeof (buf), _("Show destinations"));
					} else {
						snprintf (buf, sizeof (buf), _("Show '%s' destinations"), pg[dim]->name.c_str());
					}
				}
				items.push_back (MenuElem (buf, bind (mem_fun (*this, &PortMatrix::show_group), wp)));
			}

			need_separator = true;
		}
	}

	if (need_separator) {
		items.push_back (SeparatorElem ());
	}

	items.push_back (MenuElem (_("Rescan"), mem_fun (*this, &PortMatrix::setup_all_ports)));
	items.push_back (CheckMenuElem (_("Show individual ports"), mem_fun (*this, &PortMatrix::toggle_show_only_bundles)));
	CheckMenuItem* i = dynamic_cast<CheckMenuItem*> (&items.back());
	_inhibit_toggle_show_only_bundles = true;
	i->set_active (!_show_only_bundles);
	_inhibit_toggle_show_only_bundles = false;
	
	_menu->popup (1, t);
}

void
PortMatrix::remove_channel_proxy (boost::weak_ptr<Bundle> b, uint32_t c)
{
	boost::shared_ptr<Bundle> sb = b.lock ();
	if (!sb) {
		return;
	}

	remove_channel (BundleChannel (sb, c));

}

void
PortMatrix::rename_channel_proxy (boost::weak_ptr<Bundle> b, uint32_t c)
{
	boost::shared_ptr<Bundle> sb = b.lock ();
	if (!sb) {
		return;
	}

	rename_channel (BundleChannel (sb, c));
}

void
PortMatrix::disassociate_all_on_channel (boost::weak_ptr<Bundle> bundle, uint32_t channel, int dim)
{
	boost::shared_ptr<Bundle> sb = bundle.lock ();
	if (!sb) {
		return;
	}

	PortGroup::BundleList a = _ports[1-dim].bundles ();

	for (PortGroup::BundleList::iterator i = a.begin(); i != a.end(); ++i) {
		for (uint32_t j = 0; j < i->bundle->nchannels(); ++j) {

			BundleChannel c[2];
			c[dim] = BundleChannel (sb, channel);
			c[1-dim] = BundleChannel (i->bundle, j);

			if (get_state (c) == PortMatrixNode::ASSOCIATED) {
				set_state (c, false);
			}
		}
	}

	_body->rebuild_and_draw_grid ();
}

void
PortMatrix::setup_global_ports ()
{
	for (int i = 0; i < 2; ++i) {
		if (list_is_global (i)) {
			setup_ports (i);
		}
	}
}

void
PortMatrix::setup_all_ports ()
{
	setup_ports (0);
	setup_ports (1);
}

void
PortMatrix::toggle_show_only_bundles ()
{
	if (_inhibit_toggle_show_only_bundles) {
		return;
	}
	
	_show_only_bundles = !_show_only_bundles;
	_body->setup ();
	setup_scrollbars ();
	queue_draw ();
}

void
PortMatrix::hide_group (boost::weak_ptr<PortGroup> w)
{
	boost::shared_ptr<PortGroup> g = w.lock ();
	if (!g) {
		return;
	}

	g->set_visible (false);
}

void
PortMatrix::show_group (boost::weak_ptr<PortGroup> w)
{
	boost::shared_ptr<PortGroup> g = w.lock ();
	if (!g) {
		return;
	}

	g->set_visible (true);
}

pair<uint32_t, uint32_t>
PortMatrix::max_size () const
{
	pair<uint32_t, uint32_t> m = _body->max_size ();

	m.first += _vscroll.get_width ();
	m.second += _hscroll.get_height ();

	return m;
}

bool
PortMatrix::on_scroll_event (GdkEventScroll* ev)
{
	double const h = _hscroll.get_value ();
	double const v = _vscroll.get_value ();
	
	switch (ev->direction) {
	case GDK_SCROLL_UP:
		_vscroll.set_value (v - PortMatrixComponent::grid_spacing ());
		break;
	case GDK_SCROLL_DOWN:
		_vscroll.set_value (v + PortMatrixComponent::grid_spacing ());
		break;
	case GDK_SCROLL_LEFT:
		_hscroll.set_value (h - PortMatrixComponent::grid_spacing ());
		break;
	case GDK_SCROLL_RIGHT:
		_hscroll.set_value (h + PortMatrixComponent::grid_spacing ());
		break;
	}

	return true;
}

boost::shared_ptr<IO>
PortMatrix::io_from_bundle (boost::shared_ptr<Bundle> b) const
{
	boost::shared_ptr<IO> io = _ports[0].io_from_bundle (b);
	if (!io) {
		io = _ports[1].io_from_bundle (b);
	}

	return io;
}

bool
PortMatrix::can_add_channel (boost::shared_ptr<Bundle> b) const
{
	return io_from_bundle (b);
}

void
PortMatrix::add_channel (boost::shared_ptr<Bundle> b)
{
	boost::shared_ptr<IO> io = io_from_bundle (b);

	if (io) {
		io->add_port ("", this, _type);
	}
}

bool
PortMatrix::can_remove_channels (boost::shared_ptr<Bundle> b) const
{
	return io_from_bundle (b);
}

void
PortMatrix::remove_channel (ARDOUR::BundleChannel b)
{
	boost::shared_ptr<IO> io = io_from_bundle (b.bundle);

	if (io) {
		Port* p = io->nth (b.channel);
		if (p) {
			io->remove_port (p, this);
		}
	}
}

void
PortMatrix::add_channel_proxy (boost::weak_ptr<Bundle> w)
{
	boost::shared_ptr<Bundle> b = w.lock ();
	if (!b) {
		return;
	}

	add_channel (b);
}
