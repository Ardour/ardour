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
#include <gtkmm/stock.h>
#include <gtkmm/messagedialog.h>
#include "ardour/bundle.h"
#include "ardour/types.h"
#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/audioengine.h"
#include "port_matrix.h"
#include "port_matrix_body.h"
#include "port_matrix_component.h"
#include "ardour_dialog.h"
#include "i18n.h"
#include "gui_thread.h"
#include "utils.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;

/** PortMatrix constructor.
 *  @param session Our session.
 *  @param type Port type that we are handling.
 */
PortMatrix::PortMatrix (Window* parent, Session* session, DataType type)
	: Table (4, 4)
	, _parent (parent)
	, _type (type)
	, _menu (0)
	, _arrangement (TOP_TO_RIGHT)
	, _row_index (0)
	, _column_index (1)
	, _min_height_divisor (1)
	, _show_only_bundles (false)
	, _inhibit_toggle_show_only_bundles (false)
	, _ignore_notebook_page_selected (false)
{
	set_session (session);

	_body = new PortMatrixBody (this);
	_body->DimensionsChanged.connect (sigc::mem_fun (*this, &PortMatrix::body_dimensions_changed));

	_hbox.pack_end (_hspacer, true, true);
	_hbox.pack_end (_hnotebook, false, false);
	_hbox.pack_end (_hlabel, false, false);

	_vnotebook.signal_switch_page().connect (sigc::mem_fun (*this, &PortMatrix::notebook_page_selected));
	_vnotebook.property_tab_border() = 4;
	_vnotebook.set_name (X_("PortMatrixLabel"));
	_hnotebook.signal_switch_page().connect (sigc::mem_fun (*this, &PortMatrix::notebook_page_selected));
	_hnotebook.property_tab_border() = 4;
	_hnotebook.set_name (X_("PortMatrixLabel"));

	_vlabel.set_use_markup ();
	_vlabel.set_alignment (1, 1);
	_vlabel.set_padding (4, 16);
	_vlabel.set_name (X_("PortMatrixLabel"));
	_hlabel.set_use_markup ();
	_hlabel.set_alignment (1, 0.5);
	_hlabel.set_padding (16, 4);
	_hlabel.set_name (X_("PortMatrixLabel"));

	set_row_spacing (0, 8);
	set_col_spacing (0, 8);
	set_row_spacing (2, 8);
	set_col_spacing (2, 8);

	_body->show ();
	_vbox.show ();
	_hbox.show ();
	_vscroll.show ();
	_hscroll.show ();
	_vlabel.show ();
	_hlabel.show ();
	_hspacer.show ();
	_vspacer.show ();
	_vnotebook.show ();
	_hnotebook.show ();
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

	/* Signal handling is kind of split into three parts:
	 *
	 * 1.  When _ports[] changes, we call setup().  This essentially sorts out our visual
	 *     representation of the information in _ports[].
	 *
	 * 2.  When certain other things change, we need to get our subclass to clear and
	 *     re-fill _ports[], which in turn causes appropriate signals to be raised to
	 *     hook into part (1).
	 *
	 * 3.  Assorted other signals.
	 */


	/* Part 1: the basic _ports[] change -> reset visuals */

	for (int i = 0; i < 2; ++i) {
		/* watch for the content of _ports[] changing */
		_ports[i].Changed.connect (_changed_connections, invalidator (*this), boost::bind (&PortMatrix::setup, this), gui_context());

		/* and for bundles in _ports[] changing */
		_ports[i].BundleChanged.connect (_bundle_changed_connections, invalidator (*this), boost::bind (&PortMatrix::setup, this), gui_context());
	}

	/* Part 2: notice when things have changed that require our subclass to clear and refill _ports[] */

	/* watch for routes being added or removed */
	_session->RouteAdded.connect (_session_connections, invalidator (*this), boost::bind (&PortMatrix::routes_changed, this), gui_context());

	/* and also bundles */
	_session->BundleAdded.connect (_session_connections, invalidator (*this), boost::bind (&PortMatrix::setup_global_ports, this), gui_context());

	/* and also ports */
	_session->engine().PortRegisteredOrUnregistered.connect (_session_connections, invalidator (*this), boost::bind (&PortMatrix::setup_global_ports, this), gui_context());

	/* watch for route order keys changing, which changes the order of things in our global ports list(s) */
	Route::SyncOrderKeys.connect (_session_connections, invalidator (*this), boost::bind (&PortMatrix::setup_global_ports_proxy, this, _1), gui_context());

	/* Part 3: other stuff */

	_session->engine().PortConnectedOrDisconnected.connect (_session_connections, invalidator (*this), boost::bind (&PortMatrix::port_connected_or_disconnected, this), gui_context ());

	_hscroll.signal_value_changed().connect (sigc::mem_fun (*this, &PortMatrix::hscroll_changed));
	_vscroll.signal_value_changed().connect (sigc::mem_fun (*this, &PortMatrix::vscroll_changed));

	reconnect_to_routes ();

	setup ();
}

/** Disconnect from and reconnect to routes' signals that we need to watch for things that affect the matrix */
void
PortMatrix::reconnect_to_routes ()
{
	_route_connections.drop_connections ();

	boost::shared_ptr<RouteList> routes = _session->get_routes ();
	for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
		(*i)->processors_changed.connect (_route_connections, invalidator (*this), boost::bind (&PortMatrix::route_processors_changed, this, _1), gui_context());
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
	if (!_session) return; // session went away

	/* this needs to be done first, as the visible_ports() method uses the
	   notebook state to decide which ports are being shown */

	setup_notebooks ();

	_body->setup ();
	setup_scrollbars ();
	update_tab_highlighting ();
	queue_draw ();
}

void
PortMatrix::set_type (DataType t)
{
	_type = t;
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
		for (uint32_t j = 0; j < (*i)->bundle->nchannels().n_total(); ++j) {
			for (PortGroup::BundleList::iterator k = b.begin(); k != b.end(); ++k) {
				for (uint32_t l = 0; l < (*k)->bundle->nchannels().n_total(); ++l) {

					if (!should_show ((*i)->bundle->channel_type(j)) || !should_show ((*k)->bundle->channel_type(l))) {
						continue;
					}

					BundleChannel c[2] = {
						BundleChannel ((*i)->bundle, j),
						BundleChannel ((*k)->bundle, l)
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
		count_of_our_type_min_1 (_ports[0].total_channels()),
		count_of_our_type_min_1 (_ports[1].total_channels())
	};

	/* XXX: shirley there's an easier way than this */

	if (_vspacer.get_parent()) {
		_vbox.remove (_vspacer);
	}

	if (_vnotebook.get_parent()) {
		_vbox.remove (_vnotebook);
	}

	if (_vlabel.get_parent()) {
		_vbox.remove (_vlabel);
	}

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

		_vbox.pack_end (_vlabel, false, false);
		_vbox.pack_end (_vnotebook, false, false);
		_vbox.pack_end (_vspacer, true, true);

		attach (*_body, 2, 3, 1, 2, FILL | EXPAND, FILL | EXPAND);
		attach (_vscroll, 3, 4, 1, 2, SHRINK);
		attach (_hscroll, 2, 3, 3, 4, FILL | EXPAND, SHRINK);
		attach (_vbox, 1, 2, 1, 2, SHRINK);
		attach (_hbox, 2, 3, 2, 3, FILL | EXPAND, SHRINK);

	} else {

		_row_index = 1;
		_column_index = 0;
		_arrangement = TOP_TO_RIGHT;
		_hlabel.set_label (_("<b>Sources</b>"));
		_vlabel.set_label (_("<b>Destinations</b>"));
		_vlabel.set_angle (-90);

		_vbox.pack_end (_vspacer, true, true);
		_vbox.pack_end (_vnotebook, false, false);
		_vbox.pack_end (_vlabel, false, false);

		attach (*_body, 1, 2, 2, 3, FILL | EXPAND, FILL | EXPAND);
		attach (_vscroll, 3, 4, 2, 3, SHRINK);
		attach (_hscroll, 1, 2, 3, 4, FILL | EXPAND, SHRINK);
		attach (_vbox, 2, 3, 2, 3, SHRINK);
		attach (_hbox, 1, 2, 1, 2, FILL | EXPAND, SHRINK);
	}
}

/** @return columns list */
PortGroupList const *
PortMatrix::columns () const
{
	return &_ports[_column_index];
}

boost::shared_ptr<const PortGroup>
PortMatrix::visible_columns () const
{
	return visible_ports (_column_index);
}

/* @return rows list */
PortGroupList const *
PortMatrix::rows () const
{
	return &_ports[_row_index];
}

boost::shared_ptr<const PortGroup>
PortMatrix::visible_rows () const
{
	return visible_ports (_row_index);
}

/** @param column Column; its bundle may be 0 if we are over a row heading.
 *  @param row Row; its bundle may be 0 if we are over a column heading.
 */
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

			if (can_add_channels (bc[dim].bundle)) {
				/* Start off with options for the `natural' port type */
				for (DataType::iterator i = DataType::begin(); i != DataType::end(); ++i) {
					if (should_show (*i)) {
						snprintf (buf, sizeof (buf), _("Add %s %s"), (*i).to_i18n_string(), channel_noun().c_str());
						sub.push_back (MenuElem (buf, sigc::bind (sigc::mem_fun (*this, &PortMatrix::add_channel_proxy), w, *i)));
					}
				}
				
				/* Now add other ones */
				for (DataType::iterator i = DataType::begin(); i != DataType::end(); ++i) {
					if (!should_show (*i)) {
						snprintf (buf, sizeof (buf), _("Add %s %s"), (*i).to_i18n_string(), channel_noun().c_str());
						sub.push_back (MenuElem (buf, sigc::bind (sigc::mem_fun (*this, &PortMatrix::add_channel_proxy), w, *i)));
					}
				}
			}

			if (can_rename_channels (bc[dim].bundle) && bc[dim].channel != -1) {
				snprintf (
					buf, sizeof (buf), _("Rename '%s'..."),
					escape_underscores (bc[dim].bundle->channel_name (bc[dim].channel)).c_str()
					);
				sub.push_back (
					MenuElem (
						buf,
						sigc::bind (sigc::mem_fun (*this, &PortMatrix::rename_channel_proxy), w, bc[dim].channel)
						)
					);
			}

			if (can_remove_channels (bc[dim].bundle) && bc[dim].bundle->nchannels() != ARDOUR::ChanCount::ZERO) {
				if (bc[dim].channel != -1) {
					add_remove_option (sub, w, bc[dim].channel);
				} else {
					sub.push_back (
						MenuElem (_("Remove all"), sigc::bind (sigc::mem_fun (*this, &PortMatrix::remove_all_channels), w))
						);

					if (bc[dim].bundle->nchannels().n_total() > 1) {
                                                for (uint32_t i = 0; i < bc[dim].bundle->nchannels().n_total(); ++i) {
                                                        if (should_show (bc[dim].bundle->channel_type(i))) {
                                                                add_remove_option (sub, w, i);
                                                        }
                                                }
                                        }
				}
			}

			uint32_t c = count_of_our_type (bc[dim].bundle->nchannels ());
			if ((_show_only_bundles && c > 0) || c == 1) {

				/* we're looking just at bundles, or our bundle has only one channel, so just offer
				   to disassociate all on the bundle.
				*/
				
				snprintf (buf, sizeof (buf), _("%s all"), disassociation_verb().c_str());
				sub.push_back (
					MenuElem (buf, sigc::bind (sigc::mem_fun (*this, &PortMatrix::disassociate_all_on_bundle), w, dim))
					);
					
			} else if (c != 0) {

				if (bc[dim].channel != -1) {
					/* specific channel under the menu, so just offer to disassociate that */
					add_disassociate_option (sub, w, dim, bc[dim].channel);
				} else {
					/* no specific channel; offer to disassociate all, or any one in particular */
					snprintf (buf, sizeof (buf), _("%s all"), disassociation_verb().c_str());
					sub.push_back (
						MenuElem (buf, sigc::bind (sigc::mem_fun (*this, &PortMatrix::disassociate_all_on_bundle), w, dim))
						);

					for (uint32_t i = 0; i < bc[dim].bundle->nchannels().n_total(); ++i) {
						if (should_show (bc[dim].bundle->channel_type(i))) {
							add_disassociate_option (sub, w, dim, i);
						}
					}
				}
			}

			items.push_back (MenuElem (escape_underscores (bc[dim].bundle->name()).c_str(), *m));
			need_separator = true;
		}

	}

	if (need_separator) {
		items.push_back (SeparatorElem ());
	}

	items.push_back (MenuElem (_("Rescan"), sigc::mem_fun (*this, &PortMatrix::setup_all_ports)));

	items.push_back (CheckMenuElem (_("Show individual ports"), sigc::mem_fun (*this, &PortMatrix::toggle_show_only_bundles)));
	Gtk::CheckMenuItem* i = dynamic_cast<Gtk::CheckMenuItem*> (&items.back());
	_inhibit_toggle_show_only_bundles = true;
	i->set_active (!_show_only_bundles);
	_inhibit_toggle_show_only_bundles = false;

	items.push_back (MenuElem (_("Flip"), sigc::mem_fun (*this, &PortMatrix::flip)));
	items.back().set_sensitive (can_flip ());
	
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
PortMatrix::disassociate_all_on_bundle (boost::weak_ptr<Bundle> bundle, int dim)
{
	boost::shared_ptr<Bundle> sb = bundle.lock ();
	if (!sb) {
		return;
	}

	for (uint32_t i = 0; i < sb->nchannels().n_total(); ++i) {
		if (should_show (sb->channel_type(i))) {
			disassociate_all_on_channel (bundle, i, dim);
		}
	}
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
		for (uint32_t j = 0; j < (*i)->bundle->nchannels().n_total(); ++j) {

			if (!should_show ((*i)->bundle->channel_type(j))) {
				continue;
			}

			BundleChannel c[2];
			c[dim] = BundleChannel (sb, channel);
			c[1-dim] = BundleChannel ((*i)->bundle, j);

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
	ENSURE_GUI_THREAD (*this, &PortMatrix::setup_global_ports)

	for (int i = 0; i < 2; ++i) {
		if (list_is_global (i)) {
			setup_ports (i);
		}
	}
}

void
PortMatrix::setup_global_ports_proxy (RouteSortOrderKey sk)
{
	if (sk == EditorSort) {
		/* Avoid a deadlock by calling this in an idle handler: see IOSelector::io_changed_proxy
		   for a discussion.
		*/
		
		Glib::signal_idle().connect_once (sigc::mem_fun (*this, &PortMatrix::setup_global_ports));
	}
}

void
PortMatrix::setup_all_ports ()
{
	if (_session->deletion_in_progress()) {
		return;
	}

	ENSURE_GUI_THREAD (*this, &PortMatrix::setup_all_ports)

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

	/* The way in which hardware ports are grouped changes depending on the _show_only_bundles
	   setting, so we need to set things up again now.
	*/
	setup_all_ports ();
}

pair<uint32_t, uint32_t>
PortMatrix::max_size () const
{
	pair<uint32_t, uint32_t> m = _body->max_size ();

	m.first += _vscroll.get_width () + _vbox.get_width () + 4;
	m.second += _hscroll.get_height () + _hbox.get_height () + 4;

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
PortMatrix::can_add_channels (boost::shared_ptr<Bundle> b) const
{
	return io_from_bundle (b);
}

void
PortMatrix::add_channel (boost::shared_ptr<Bundle> b, DataType t)
{
	boost::shared_ptr<IO> io = io_from_bundle (b);

	if (io) {
		int const r = io->add_port ("", this, t);
		if (r == -1) {
			Gtk::MessageDialog msg (_("It is not possible to add a port here, as the first processor in the track or buss cannot "
						  "support the new configuration."
							));
			msg.set_title (_("Cannot add port"));
			msg.run ();
		}
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
		boost::shared_ptr<Port> p = io->nth (b.channel);
		if (p) {
			int const r = io->remove_port (p, this);
			if (r == -1) {
				ArdourDialog d (_("Port removal not allowed"));
				Label l (_("This port cannot be removed, as the first plugin in the track or buss cannot accept the new number of inputs."));
				d.get_vbox()->pack_start (l);
				d.add_button (Stock::OK, RESPONSE_ACCEPT);
				d.set_modal (true);
				d.show_all ();
				d.run ();
			}
		}
	}
}

void
PortMatrix::remove_all_channels (boost::weak_ptr<Bundle> w)
{
	boost::shared_ptr<Bundle> b = w.lock ();
	if (!b) {
		return;
	}

	/* Remove channels backwards so that we don't renumber channels
	   that we are about to remove.
	*/
	for (int i = (b->nchannels().n_total() - 1); i >= 0; --i) {
		if (should_show (b->channel_type(i))) {
			remove_channel (ARDOUR::BundleChannel (b, i));
		}
	}
}

void
PortMatrix::add_channel_proxy (boost::weak_ptr<Bundle> w, DataType t)
{
	boost::shared_ptr<Bundle> b = w.lock ();
	if (!b) {
		return;
	}

	add_channel (b, t);
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
		label->set_use_markup ();
		label->show ();
		if (_arrangement == LEFT_TO_BOTTOM) {
			_vnotebook.prepend_page (*dummy, *label);
		} else {
			/* Reverse the order of vertical tabs when they are on the right hand side
			   so that from top to bottom it is the same order as that from left to right
			   for the top tabs.
			*/
			_vnotebook.append_page (*dummy, *label);
		}
	}

	for (PortGroupList::List::const_iterator i = _ports[_column_index].begin(); i != _ports[_column_index].end(); ++i) {
		HBox* dummy = manage (new HBox);
		dummy->show ();
		Label* label = manage (new Label ((*i)->name));
		label->set_use_markup ();
		label->show ();
		_hnotebook.append_page (*dummy, *label);
	}

	_ignore_notebook_page_selected = false;

	if (_arrangement == TOP_TO_RIGHT) {
		_vnotebook.set_tab_pos (POS_RIGHT);
		_hnotebook.set_tab_pos (POS_TOP);
	} else {
		_vnotebook.set_tab_pos (POS_LEFT);
		_hnotebook.set_tab_pos (POS_BOTTOM);
	}

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
		_hbox.hide ();
	} else {
		_hbox.show ();
	}

	if (_vnotebook.get_n_pages() <= 1) {
		_vbox.hide ();
	} else {
		_vbox.show ();
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
PortMatrix::notebook_page_selected (GtkNotebookPage *, guint)
{
	if (_ignore_notebook_page_selected) {
		return;
	}

	_body->setup ();
	setup_scrollbars ();
	queue_draw ();
}

void
PortMatrix::session_going_away ()
{
	_session = 0;
}

void
PortMatrix::body_dimensions_changed ()
{
	_hspacer.set_size_request (_body->column_labels_border_x (), -1);
	if (_arrangement == TOP_TO_RIGHT) {
		_vspacer.set_size_request (-1, _body->column_labels_height ());
		_vspacer.show ();
	} else {
		_vspacer.hide ();
	}

	int curr_width;
	int curr_height;
	_parent->get_size (curr_width, curr_height);

	pair<uint32_t, uint32_t> m = max_size ();

	/* Don't shrink the window */
	m.first = max (int (m.first), curr_width);
	m.second = max (int (m.second), curr_height);

	resize_window_to_proportion_of_monitor (_parent, m.first, m.second);
}

/** @return The PortGroup that is currently visible (ie selected by
 *  the notebook) along a given axis.
 */
boost::shared_ptr<const PortGroup>
PortMatrix::visible_ports (int d) const
{
	PortGroupList const & p = _ports[d];
	PortGroupList::List::const_iterator j = p.begin ();

	/* The logic to compute the index here is a bit twisty because for
	   the TOP_TO_RIGHT arrangement we reverse the order of the vertical
	   tabs in setup_notebooks ().
	*/
	   
	int n = 0;
	if (d == _row_index) {
		if (_arrangement == LEFT_TO_BOTTOM) {
			n = p.size() - _vnotebook.get_current_page () - 1;
		} else {
			n = _vnotebook.get_current_page ();
		}
	} else {
		n = _hnotebook.get_current_page ();
	}

	int i = 0;
	while (i != int (n) && j != p.end ()) {
		++i;
		++j;
	}

	if (j == p.end()) {
		return boost::shared_ptr<const PortGroup> ();
	}

	return *j;
}

void
PortMatrix::add_remove_option (Menu_Helpers::MenuList& m, boost::weak_ptr<Bundle> w, int c)
{
	using namespace Menu_Helpers;

	boost::shared_ptr<Bundle> b = w.lock ();
	if (!b) {
		return;
	}

	char buf [64];
	snprintf (buf, sizeof (buf), _("Remove '%s'"), escape_underscores (b->channel_name (c)).c_str());
	m.push_back (MenuElem (buf, sigc::bind (sigc::mem_fun (*this, &PortMatrix::remove_channel_proxy), w, c)));
}

void
PortMatrix::add_disassociate_option (Menu_Helpers::MenuList& m, boost::weak_ptr<Bundle> w, int d, int c)
{
	using namespace Menu_Helpers;

	boost::shared_ptr<Bundle> b = w.lock ();
	if (!b) {
		return;
	}

	char buf [64];
	snprintf (buf, sizeof (buf), _("%s all from '%s'"), disassociation_verb().c_str(), escape_underscores (b->channel_name (c)).c_str());
	m.push_back (MenuElem (buf, sigc::bind (sigc::mem_fun (*this, &PortMatrix::disassociate_all_on_channel), w, c, d)));
}

void
PortMatrix::port_connected_or_disconnected ()
{
	_body->rebuild_and_draw_grid ();
	update_tab_highlighting ();
}

/** Update the highlighting of tab names to reflect which ones
 *  have connections.  This is pretty inefficient, unfortunately,
 *  but maybe that doesn't matter too much.
 */
void
PortMatrix::update_tab_highlighting ()
{
	if (!_session) {
		return;
	}
	
	for (int i = 0; i < 2; ++i) {

		Gtk::Notebook* notebook = row_index() == i ? &_vnotebook : &_hnotebook;
		
		PortGroupList const * gl = ports (i);
		int p = 0;
		for (PortGroupList::List::const_iterator j = gl->begin(); j != gl->end(); ++j) {
			bool has_connection = false;
			PortGroup::BundleList const & bl = (*j)->bundles ();
			PortGroup::BundleList::const_iterator k = bl.begin ();
			while (k != bl.end()) {
				if ((*k)->bundle->connected_to_anything (_session->engine())) {
					has_connection = true;
					break;
				}
				++k;
			}

			/* Find the page index that we should update; this is backwards
			   for the vertical tabs in the LEFT_TO_BOTTOM arrangement.
			*/
			int page = p;
			if (i == row_index() && _arrangement == LEFT_TO_BOTTOM) {
				page = notebook->get_n_pages() - p - 1;
			}

			Gtk::Label* label = dynamic_cast<Gtk::Label*> (notebook->get_tab_label(*notebook->get_nth_page (page)));
			string c = label->get_label ();
			if (c.length() && c[0] == '<' && !has_connection) {
				/* this label is marked up with <b> but shouldn't be */
				label->set_text ((*j)->name);
			} else if (c.length() && c[0] != '<' && has_connection) {
				/* this label is not marked up with <b> but should be */
				label->set_markup (string_compose ("<b>%1</b>", Glib::Markup::escape_text ((*j)->name)));
			}

			++p;
		}
	}
}

string
PortMatrix::channel_noun () const
{
	return _("channel");
}

/** @return true if this matrix should show bundles / ports of type \t */
bool
PortMatrix::should_show (DataType t) const
{
	return (_type == DataType::NIL || t == _type);
}

uint32_t
PortMatrix::count_of_our_type (ChanCount c) const
{
	if (_type == DataType::NIL) {
		return c.n_total ();
	}

	return c.get (_type);
}

/** @return The number of ports of our type in the given channel count,
 *  but returning 1 if there are no ports.
 */
uint32_t
PortMatrix::count_of_our_type_min_1 (ChanCount c) const
{
	uint32_t n = count_of_our_type (c);
	if (n == 0) {
		n = 1;
	}

	return n;
}

PortMatrixNode::State
PortMatrix::get_association (PortMatrixNode node) const
{
	if (show_only_bundles ()) {

		bool have_off_diagonal_association = false;
		bool have_diagonal_association = false;
		bool have_diagonal_not_association = false;

		for (uint32_t i = 0; i < node.row.bundle->nchannels().n_total(); ++i) {

			for (uint32_t j = 0; j < node.column.bundle->nchannels().n_total(); ++j) {

				if (!should_show (node.row.bundle->channel_type(i)) || !should_show (node.column.bundle->channel_type(j))) {
					continue;
				}

				ARDOUR::BundleChannel c[2];
				c[row_index()] = ARDOUR::BundleChannel (node.row.bundle, i);
				c[column_index()] = ARDOUR::BundleChannel (node.column.bundle, j);

				PortMatrixNode::State const s = get_state (c);

				switch (s) {
				case PortMatrixNode::ASSOCIATED:
					if (i == j) {
						have_diagonal_association = true;
					} else {
						have_off_diagonal_association = true;
					}
					break;

				case PortMatrixNode::NOT_ASSOCIATED:
					if (i == j) {
						have_diagonal_not_association = true;
					}
					break;

				default:
					break;
				}
			}
		}

		if (have_diagonal_association && !have_off_diagonal_association && !have_diagonal_not_association) {
			return PortMatrixNode::ASSOCIATED;
		} else if (!have_diagonal_association && !have_off_diagonal_association) {
			return PortMatrixNode::NOT_ASSOCIATED;
		}

		return PortMatrixNode::PARTIAL;

	} else {

		ARDOUR::BundleChannel c[2];
		c[column_index()] = node.column;
		c[row_index()] = node.row;
		return get_state (c);

	}

	/* NOTREACHED */
	return PortMatrixNode::NOT_ASSOCIATED;
}

/** @return true if b is a non-zero pointer and the bundle it points to has some channels */
bool
PortMatrix::bundle_with_channels (boost::shared_ptr<ARDOUR::Bundle> b)
{
	return b && b->nchannels() != ARDOUR::ChanCount::ZERO;
}

/** See if a `flip' is possible.
 *  @return If flip is possible, the new (row, column) notebook indices that
 *  should be selected; otherwise, (-1, -1)
 */
pair<int, int>
PortMatrix::check_flip () const
{
	/* Look for the row's port group name in the columns */
	
	int new_column = 0;
	boost::shared_ptr<const PortGroup> r = visible_ports (_row_index);
	PortGroupList::List::const_iterator i = _ports[_column_index].begin();
	while (i != _ports[_column_index].end() && (*i)->name != r->name) {
		++i;
		++new_column;
	}

	if (i == _ports[_column_index].end ()) {
		return make_pair (-1, -1);
	}

	/* Look for the column's port group name in the rows */
	
	int new_row = 0;
	boost::shared_ptr<const PortGroup> c = visible_ports (_column_index);
	i = _ports[_row_index].begin();
	while (i != _ports[_row_index].end() && (*i)->name != c->name) {
		++i;
		++new_row;
	}

	if (i == _ports[_row_index].end ()) {
		return make_pair (-1, -1);
	}

	if (_arrangement == LEFT_TO_BOTTOM) {
		new_row = _ports[_row_index].size() - new_row - 1;
	}

	return make_pair (new_row, new_column);
}

bool
PortMatrix::can_flip () const
{
	return check_flip().first != -1;
}

/** Flip the column and row pages around, if possible */
void
PortMatrix::flip ()
{
	pair<int, int> n = check_flip ();
	if (n.first == -1) {
		return;
	}

	_vnotebook.set_current_page (n.first);
	_hnotebook.set_current_page (n.second);
}

bool
PortMatrix::key_press (GdkEventKey* k)
{
	if (k->keyval == GDK_f) {
		flip ();
		return true;
	}

	return false;
}
