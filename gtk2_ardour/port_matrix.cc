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
#include "gui_thread.h"

using namespace std;
using namespace sigc;
using namespace Gtk;
using namespace ARDOUR;

/** PortMatrix constructor.
 *  @param session Our session.
 *  @param type Port type that we are handling.
 */
PortMatrix::PortMatrix (Window* parent, Session* session, DataType type)
	: Table (3, 3),
	  _session (session),
	  _parent (parent),
	  _type (type),
	  _menu (0),
	  _arrangement (TOP_TO_RIGHT),
	  _row_index (0),
	  _column_index (1),
	  _min_height_divisor (1),
	  _show_only_bundles (false),
	  _inhibit_toggle_show_only_bundles (false),
	  _ignore_notebook_page_selected (false)
{
	_body = new PortMatrixBody (this);

	_vbox.pack_start (_vnotebook);
	_vbox.pack_start (_vlabel);
	_hbox.pack_start (_hnotebook);
	_hbox.pack_start (_hlabel);

	_vnotebook.signal_switch_page().connect (mem_fun (*this, &PortMatrix::v_page_selected));
	_vnotebook.property_tab_border() = 4;
	_hnotebook.signal_switch_page().connect (mem_fun (*this, &PortMatrix::h_page_selected));
	_hnotebook.property_tab_border() = 4;

	for (int i = 0; i < 2; ++i) {
		_ports[i].set_type (type);
	}

	_vlabel.set_use_markup ();
	_vlabel.set_alignment (0.5, 0);
	_vlabel.set_padding (4, 16);
	_hlabel.set_use_markup ();
	_hlabel.set_alignment (0, 0.5);
	_hlabel.set_padding (16, 4);

	_body->show ();
	_vbox.show ();
	_hbox.show ();
	_vlabel.show ();
	_hlabel.show ();
}

PortMatrix::~PortMatrix ()
{
	delete _body;
	delete _menu;
}

/** Perform initial and once-only setup.  This must be called by
 *  subclasses after they have set up _ports[] to at least some
 *  reasonable extent.  Two-part initialisation is necessary because
 *  setting up _ports is largely done by virtual functions in
 *  subclasses.
 */

void
PortMatrix::init ()
{
	select_arrangement ();
	setup_notebooks ();

	if (!_ports[0].empty()) {
		_visible_ports[0] = *_ports[0].begin();
	}
	
	if (!_ports[1].empty()) {
		_visible_ports[1] = *_ports[1].begin();
	}

	/* Signal handling is kind of split into two parts:
	 *
	 * 1.  When _ports[] changes, we call setup().  This essentially sorts out our visual
	 *     representation of the information in _ports[].
	 *
	 * 2.  When certain other things change, we need to get our subclass to clear and
	 *     re-fill _ports[], which in turn causes appropriate signals to be raised to
	 *     hook into part (1).
	 */


	/* Part 1: the basic _ports[] change -> reset visuals */

	for (int i = 0; i < 2; ++i) {
		/* watch for the content of _ports[] changing */
		_ports[i].Changed.connect (mem_fun (*this, &PortMatrix::setup));

		/* and for bundles in _ports[] changing */
		_ports[i].BundleChanged.connect (sigc::hide (mem_fun (*this, &PortMatrix::setup)));
	}

	/* scrolling stuff */
	_hscroll.signal_value_changed().connect (mem_fun (*this, &PortMatrix::hscroll_changed));
	_vscroll.signal_value_changed().connect (mem_fun (*this, &PortMatrix::vscroll_changed));


	/* Part 2: notice when things have changed that require our subclass to clear and refill _ports[] */
	
	/* watch for routes being added or removed */
	_session->RouteAdded.connect (sigc::hide (mem_fun (*this, &PortMatrix::routes_changed)));

	/* and also bundles */
	_session->BundleAdded.connect (sigc::hide (mem_fun (*this, &PortMatrix::setup_global_ports)));

	/* and also ports */
	_session->engine().PortRegisteredOrUnregistered.connect (mem_fun (*this, &PortMatrix::setup_global_ports));

	_session->GoingAway.connect (mem_fun (*this, &PortMatrix::session_going_away));

	reconnect_to_routes ();
	
	setup ();
}

/** Disconnect from and reconnect to routes' signals that we need to watch for things that affect the matrix */
void
PortMatrix::reconnect_to_routes ()
{
	for (vector<connection>::iterator i = _route_connections.begin(); i != _route_connections.end(); ++i) {
		i->disconnect ();
	}
	_route_connections.clear ();

	boost::shared_ptr<RouteList> routes = _session->get_routes ();
	for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
		_route_connections.push_back (
			(*i)->processors_changed.connect (mem_fun (*this, &PortMatrix::route_processors_changed))
			);
	}
}

void
PortMatrix::route_processors_changed (RouteProcessorChange c)
{
	if (c.type == RouteProcessorChange::MeterPointChange) {
		/* this change has no impact on the port matrix */
		return;
	}

	setup_global_ports ();
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
	_body->setup ();
	setup_scrollbars ();
	setup_notebooks ();
	queue_draw ();
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
		_ports[0].total_channels (),
		_ports[1].total_channels ()
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
		_vlabel.set_label (_("<b>Sources</b>"));
		_hlabel.set_label (_("<b>Destinations</b>"));
		_vlabel.set_angle (90);

		attach (*_body, 1, 2, 0, 1);
		attach (_vscroll, 2, 3, 0, 1, SHRINK);
		attach (_hscroll, 1, 2, 2, 3, FILL | EXPAND, SHRINK);
		attach (_vbox, 0, 1, 0, 1, SHRINK);
		attach (_hbox, 1, 2, 1, 2, FILL | EXPAND, SHRINK);

		set_col_spacing (0, 4);
		set_row_spacing (0, 4);
		
	} else {

		_row_index = 1;
		_column_index = 0;
		_arrangement = TOP_TO_RIGHT;
		_hlabel.set_label (_("<b>Sources</b>"));
		_vlabel.set_label (_("<b>Destinations</b>"));
		_vlabel.set_angle (-90);

		attach (*_body, 0, 1, 1, 2);
		attach (_vscroll, 2, 3, 1, 2, SHRINK);
		attach (_hscroll, 0, 1, 2, 3, FILL | EXPAND, SHRINK);
		attach (_vbox, 1, 2, 1, 2, SHRINK);
		attach (_hbox, 0, 1, 0, 1, FILL | EXPAND, SHRINK);

		set_col_spacing (1, 4);
		set_row_spacing (1, 4);
	}
}

/** @return columns list */
PortGroupList const *
PortMatrix::columns () const
{
	return &_ports[_column_index];
}

boost::shared_ptr<PortGroup>
PortMatrix::visible_columns () const
{
	return _visible_ports[_column_index];
}

/* @return rows list */
PortGroupList const *
PortMatrix::rows () const
{
	return &_ports[_row_index];
}

boost::shared_ptr<PortGroup>
PortMatrix::visible_rows () const
{
	return _visible_ports[_row_index];
}

void
PortMatrix::popup_menu (BundleChannel column, BundleChannel row, uint32_t t)
{
	using namespace Menu_Helpers;

	delete _menu;

	_menu = new Menu;
	_menu->set_name ("ArdourContextMenu");

	MenuList& items = _menu->items ();

	BundleChannel bc[2];
	bc[_column_index] = column;
	bc[_row_index] = row;

	char buf [64];
	bool need_separator = false;

	for (int dim = 0; dim < 2; ++dim) {

		if (bc[dim].bundle) {

			Menu* m = manage (new Menu);
			MenuList& sub = m->items ();

			boost::weak_ptr<Bundle> w (bc[dim].bundle);

			bool can_add_or_rename = false;

			if (can_add_channel (bc[dim].bundle)) {
				snprintf (buf, sizeof (buf), _("Add %s"), channel_noun().c_str());
				sub.push_back (MenuElem (buf, bind (mem_fun (*this, &PortMatrix::add_channel_proxy), w)));
				can_add_or_rename = true;
			}


			if (can_rename_channels (bc[dim].bundle)) {
				snprintf (buf, sizeof (buf), _("Rename '%s'..."), bc[dim].bundle->channel_name (bc[dim].channel).c_str());
				sub.push_back (
					MenuElem (
						buf,
						bind (mem_fun (*this, &PortMatrix::rename_channel_proxy), w, bc[dim].channel)
						)
					);
				can_add_or_rename = true;
			}

			if (can_add_or_rename) {
				sub.push_back (SeparatorElem ());
			}

			if (can_remove_channels (bc[dim].bundle)) {
				snprintf (buf, sizeof (buf), _("Remove '%s'"), bc[dim].bundle->channel_name (bc[dim].channel).c_str());
				sub.push_back (
					MenuElem (
						buf,
						bind (mem_fun (*this, &PortMatrix::remove_channel_proxy), w, bc[dim].channel)
						)
					);
			}

			if (_show_only_bundles || bc[dim].bundle->nchannels() <= 1) {
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
	ENSURE_GUI_THREAD (mem_fun (*this, &PortMatrix::setup_global_ports));
	
	for (int i = 0; i < 2; ++i) {
		if (list_is_global (i)) {
			setup_ports (i);
		}
	}
}

void
PortMatrix::setup_all_ports ()
{
	if (_session->deletion_in_progress()) {
		return;
	}

	ENSURE_GUI_THREAD (mem_fun (*this, &PortMatrix::setup_all_ports));

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
	
	setup ();
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

void
PortMatrix::setup_notebooks ()
{
	int const h_current_page = _hnotebook.get_current_page ();
	int const v_current_page = _vnotebook.get_current_page ();
	
	/* for some reason best known to GTK, erroneous switch_page signals seem to be generated
	   when adding or removing pages to or from notebooks, so ignore them */
	
	_ignore_notebook_page_selected = true;
	
	remove_notebook_pages (_hnotebook);
	remove_notebook_pages (_vnotebook);

	for (PortGroupList::List::const_iterator i = _ports[_row_index].begin(); i != _ports[_row_index].end(); ++i) {
		HBox* dummy = manage (new HBox);
		dummy->show ();
		Label* label = manage (new Label ((*i)->name));
		label->set_angle (_arrangement == LEFT_TO_BOTTOM ? 90 : -90);
		_vnotebook.prepend_page (*dummy, *label);
	}

	for (PortGroupList::List::const_iterator i = _ports[_column_index].begin(); i != _ports[_column_index].end(); ++i) {
		HBox* dummy = manage (new HBox);
		dummy->show ();
		_hnotebook.append_page (*dummy, (*i)->name);
	}

	_ignore_notebook_page_selected = false;

	_vnotebook.set_tab_pos (POS_LEFT);
	_hnotebook.set_tab_pos (POS_TOP);

	if (h_current_page != -1 && _hnotebook.get_n_pages() > h_current_page) {
		_hnotebook.set_current_page (h_current_page);
	} else {
		_hnotebook.set_current_page (0);
	}

	if (v_current_page != -1 && _vnotebook.get_n_pages() > v_current_page) {
		_vnotebook.set_current_page (v_current_page);
	} else {
		_vnotebook.set_current_page (0);
	}

	if (_hnotebook.get_n_pages() <= 1) {
		_hnotebook.hide ();
	} else {
		_hnotebook.show ();
	}

	if (_vnotebook.get_n_pages() <= 1) {
		_vnotebook.hide ();
	} else {
		_vnotebook.show ();
	}
}

void
PortMatrix::remove_notebook_pages (Notebook& n)
{
	int const N = n.get_n_pages ();
	
	for (int i = 0; i < N; ++i) {
		n.remove_page ();
	}
}

void
PortMatrix::v_page_selected (GtkNotebookPage *, guint n)
{
	if (_ignore_notebook_page_selected) {
		return;
	}
	
	PortGroupList& p = _ports[_row_index];

	n = p.size() - n - 1;

	int i = 0;
	PortGroupList::List::const_iterator j = p.begin();
	while (i != int (n) && j != p.end()) {
		++i;
		++j;
	}

	if (j != p.end()) {
		_visible_ports[_row_index] = *j;
		_body->setup ();
		setup_scrollbars ();
		queue_draw ();
	}
}

void
PortMatrix::h_page_selected (GtkNotebookPage *, guint n)
{
	if (_ignore_notebook_page_selected) {
		return;
	}

	PortGroupList& p = _ports[_column_index];
	
	int i = 0;
	PortGroupList::List::const_iterator j = p.begin();
	while (i != int (n) && j != p.end()) {
		++i;
		++j;
	}

	if (j != p.end()) {
		_visible_ports[_column_index] = *j;
		_body->setup ();
		setup_scrollbars ();
		queue_draw ();
	}
}

void
PortMatrix::session_going_away ()
{
	_session = 0;
}
