/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#include "export_channel_selector.h"

#include <algorithm>

#include "pbd/convert.h"

#include "ardour/audio_track.h"
#include "ardour/audioregion.h"
#include "ardour/export_channel_configuration.h"
#include "ardour/io.h"
#include "ardour/route.h"
#include "ardour/session.h"

#include <sstream>

#include "i18n.h"

using namespace std;
using namespace Glib;
using namespace ARDOUR;
using namespace PBD;

PortExportChannelSelector::PortExportChannelSelector (ARDOUR::Session * session, ProfileManagerPtr manager) :
  ExportChannelSelector (session, manager),
  channels_label (_("Channels:"), Gtk::ALIGN_LEFT),
  split_checkbox (_("Split to mono files")),
  max_channels (20),
  channel_view (max_channels)
{
	channels_hbox.pack_start (channels_label, false, false, 0);
	channels_hbox.pack_end (channels_spinbutton, false, false, 0);

	channels_vbox.pack_start (channels_hbox, false, false, 0);
	channels_vbox.pack_start (split_checkbox, false, false, 6);

	channel_alignment.add (channel_scroller);
	channel_alignment.set_padding (0, 0, 12, 0);
	channel_scroller.add (channel_view);
	channel_scroller.set_size_request (-1, 130);
	channel_scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

	pack_start (channels_vbox, false, false, 0);
	pack_start (channel_alignment, true, true, 0);

	/* Channels spinbutton */

	channels_spinbutton.set_digits (0);
	channels_spinbutton.set_increments (1, 2);
	channels_spinbutton.set_range (1, max_channels);
	channels_spinbutton.set_value (2);

	channels_spinbutton.signal_value_changed().connect (sigc::mem_fun (*this, &PortExportChannelSelector::update_channel_count));

	/* Other signals */

	split_checkbox.signal_toggled().connect (sigc::mem_fun (*this, &PortExportChannelSelector::update_split_state));
	channel_view.CriticalSelectionChanged.connect (CriticalSelectionChanged.make_slot());

	/* Finalize */

	sync_with_manager();
	show_all_children ();

}

PortExportChannelSelector::~PortExportChannelSelector ()
{
// 	if (session) {
// 		session->add_instant_xml (get_state(), false);
// 	}
}

void
PortExportChannelSelector::sync_with_manager ()
{
	state = manager->get_channel_configs().front();

	split_checkbox.set_active (state->config->get_split());
	channels_spinbutton.set_value (state->config->get_n_chans());

	fill_route_list ();
	channel_view.set_config (state->config);
}

void
PortExportChannelSelector::fill_route_list ()
{
	channel_view.clear_routes ();
	RouteList routes = *_session->get_routes();

	/* Add master bus and then everything else */

	ARDOUR::IO* master = _session->master_out()->output().get();
	channel_view.add_route (master);

	for (RouteList::iterator it = routes.begin(); it != routes.end(); ++it) {
		if ((*it)->is_master () || (*it)->is_monitor ()) {
			continue;
		}
		channel_view.add_route ((*it)->output().get());
	}

	update_channel_count ();
}

void
PortExportChannelSelector::update_channel_count ()
{
	uint32_t chans = static_cast<uint32_t> (channels_spinbutton.get_value());
	channel_view.set_channel_count (chans);
	CriticalSelectionChanged();
}

void
PortExportChannelSelector::update_split_state ()
{
	state->config->set_split (split_checkbox.get_active());
	CriticalSelectionChanged();
}

void
PortExportChannelSelector::RouteCols::add_channels (uint32_t chans)
{
	while (chans > 0) {
		channels.push_back (Channel (*this));
		++n_channels;
		--chans;
	}
}

PortExportChannelSelector::RouteCols::Channel &
PortExportChannelSelector::RouteCols::get_channel (uint32_t channel)
{
	if (channel > n_channels) {
		std::cout << "Invalid channel cout for get_channel!" << std::endl;
	}

	std::list<Channel>::iterator it = channels.begin();

	while (channel > 1) { // Channel count starts from one!
		++it;
		--channel;
	}

	return *it;
}

PortExportChannelSelector::ChannelTreeView::ChannelTreeView (uint32_t max_channels) :
  n_channels (0)
{
	/* Main columns */

	route_cols.add_channels (max_channels);

	route_list = Gtk::ListStore::create(route_cols);
	set_model (route_list);

	/* Add column with toggle and text */

	append_column_editable (_("Bus or Track"), route_cols.selected);

	Gtk::CellRendererText* text_renderer = Gtk::manage (new Gtk::CellRendererText);
	text_renderer->property_editable() = false;

	Gtk::TreeView::Column* column = get_column (0);
	column->pack_start (*text_renderer);
	column->add_attribute (text_renderer->property_text(), route_cols.name);

	Gtk::CellRendererToggle *toggle = dynamic_cast<Gtk::CellRendererToggle *>(get_column_cell_renderer (0));
	toggle->signal_toggled().connect (sigc::mem_fun (*this, &PortExportChannelSelector::ChannelTreeView::update_toggle_selection));

	static_columns = get_columns().size();
}

void
PortExportChannelSelector::ChannelTreeView::set_config (ChannelConfigPtr c)
{
	/* TODO Without the following line, the state might get reset.
	 * Pointing to the same address does not mean the state of the configuration hasn't changed.
	 * In the current code this is safe, but the actual cause of the problem would be good to fix
	 */

	if (config == c) { return; }
	config = c;

	uint32_t i = 1;
	ExportChannelConfiguration::ChannelList chan_list = config->get_channels();
	for (ExportChannelConfiguration::ChannelList::iterator c_it = chan_list.begin(); c_it != chan_list.end(); ++c_it) {

		for (Gtk::ListStore::Children::iterator r_it = route_list->children().begin(); r_it != route_list->children().end(); ++r_it) {

			ARDOUR::PortExportChannel * pec;
			if (!(pec = dynamic_cast<ARDOUR::PortExportChannel *> (c_it->get()))) {
				continue;
			}

			Glib::RefPtr<Gtk::ListStore> port_list = r_it->get_value (route_cols.port_list_col);
			std::set<boost::weak_ptr<AudioPort> > route_ports;
			std::set<boost::weak_ptr<AudioPort> > intersection;
			std::map<boost::weak_ptr<AudioPort>, string> port_labels;

			for (Gtk::ListStore::Children::const_iterator p_it = port_list->children().begin(); p_it != port_list->children().end(); ++p_it) {
				route_ports.insert ((*p_it)->get_value (route_cols.port_cols.port));
				port_labels.insert (make_pair ((*p_it)->get_value (route_cols.port_cols.port),
							       (*p_it)->get_value (route_cols.port_cols.label)));
			}

			std::set_intersection (pec->get_ports().begin(), pec->get_ports().end(),
			                       route_ports.begin(), route_ports.end(),
			                       std::insert_iterator<std::set<boost::weak_ptr<AudioPort> > > (intersection, intersection.begin()));

			intersection.erase (boost::weak_ptr<AudioPort> ()); // Remove "none" selection

			if (intersection.empty()) {
				continue;
			}

			if (!r_it->get_value (route_cols.selected)) {
				r_it->set_value (route_cols.selected, true);

				/* Set previous channels (if any) to none */

				for (uint32_t chn = 1; chn < i; ++chn) {
					r_it->set_value (route_cols.get_channel (chn).port, boost::weak_ptr<AudioPort> ());
					r_it->set_value (route_cols.get_channel (chn).label, string ("(none)"));
				}
			}

			boost::weak_ptr<AudioPort> port = *intersection.begin();
			std::map<boost::weak_ptr<AudioPort>, string>::iterator label_it = port_labels.find (port);
			string label = label_it != port_labels.end() ? label_it->second : "error";

			r_it->set_value (route_cols.get_channel (i).port, port);
			r_it->set_value (route_cols.get_channel (i).label, label);
		}

		++i;
	}
}

void
PortExportChannelSelector::ChannelTreeView::add_route (ARDOUR::IO * io)
{
	Gtk::TreeModel::iterator iter = route_list->append();
	Gtk::TreeModel::Row row = *iter;

	row[route_cols.selected] = false;
	row[route_cols.name] = io->name();
	row[route_cols.io] = io;

	/* Initialize port list */

	Glib::RefPtr<Gtk::ListStore> port_list = Gtk::ListStore::create (route_cols.port_cols);
	row[route_cols.port_list_col] = port_list;

	uint32_t outs = io->n_ports().n_audio();
	for (uint32_t i = 0; i < outs; ++i) {
		iter = port_list->append();
		row = *iter;

		row[route_cols.port_cols.selected] = false;
		row[route_cols.port_cols.port] = io->audio (i);

		std::ostringstream oss;
		oss << "Out-" << (i + 1);

		row[route_cols.port_cols.label] = oss.str();
	}

	iter = port_list->append();
	row = *iter;

	row[route_cols.port_cols.selected] = false;
	row[route_cols.port_cols.port] = boost::weak_ptr<AudioPort> ();
	row[route_cols.port_cols.label] = "(none)";

}

void
PortExportChannelSelector::ChannelTreeView::set_channel_count (uint32_t channels)
{
	int offset = channels - n_channels;

	while (offset > 0) {
		++n_channels;

		std::ostringstream oss;
		oss << n_channels;

		/* New column */

		Gtk::TreeView::Column* column = Gtk::manage (new Gtk::TreeView::Column (oss.str()));

		Gtk::CellRendererCombo* combo_renderer = Gtk::manage (new Gtk::CellRendererCombo);
		combo_renderer->property_text_column() = 2;
		column->pack_start (*combo_renderer);

		append_column (*column);

		column->add_attribute (combo_renderer->property_text(), route_cols.get_channel(n_channels).label);
		column->add_attribute (combo_renderer->property_model(), route_cols.port_list_col);
		column->add_attribute (combo_renderer->property_editable(), route_cols.selected);

		combo_renderer->signal_edited().connect (sigc::bind (sigc::mem_fun (*this, &PortExportChannelSelector::ChannelTreeView::update_selection_text), n_channels));

		/* put data into view */

		for (Gtk::ListStore::Children::iterator it = route_list->children().begin(); it != route_list->children().end(); ++it) {
			std::string label = it->get_value(route_cols.selected) ? "(none)" : "";
			it->set_value (route_cols.get_channel (n_channels).label, label);
			it->set_value (route_cols.get_channel (n_channels).port, boost::weak_ptr<AudioPort> ());
		}

		/* set column width */

		get_column (static_columns + n_channels - 1)->set_min_width (80);

		--offset;
	}

	while (offset < 0) {
		--n_channels;

		remove_column (*get_column (n_channels + static_columns));

		++offset;
	}

	update_config ();
}

void
PortExportChannelSelector::ChannelTreeView::update_config ()
{
	if (!config) { return; }

	config->clear_channels();

	for (uint32_t i = 1; i <= n_channels; ++i) {

		ExportChannelPtr channel (new PortExportChannel ());
		PortExportChannel * pec = static_cast<PortExportChannel *> (channel.get());

		for (Gtk::ListStore::Children::iterator it = route_list->children().begin(); it != route_list->children().end(); ++it) {
			Gtk::TreeModel::Row row = *it;

			if (!row[route_cols.selected]) {
				continue;
			}

			boost::weak_ptr<AudioPort> weak_port = row[route_cols.get_channel (i).port];
			boost::shared_ptr<AudioPort> port = weak_port.lock ();
			if (port) {
				pec->add_port (port);
			}
		}

		config->register_channel (channel);
	}

	CriticalSelectionChanged ();
}

void
PortExportChannelSelector::ChannelTreeView::update_toggle_selection (std::string const & path)
{
	Gtk::TreeModel::iterator iter = get_model ()->get_iter (path);
	bool selected = iter->get_value (route_cols.selected);

	for (uint32_t i = 1; i <= n_channels; ++i) {

		if (!selected) {
			iter->set_value (route_cols.get_channel (i).label, std::string (""));
			continue;
		}

		iter->set_value (route_cols.get_channel (i).label, std::string("(none)"));
		iter->set_value (route_cols.get_channel (i).port, boost::weak_ptr<AudioPort> ());

		Glib::RefPtr<Gtk::ListStore> port_list = iter->get_value (route_cols.port_list_col);
		Gtk::ListStore::Children::iterator port_it;
		uint32_t port_number = 1;

		for (port_it = port_list->children().begin(); port_it != port_list->children().end(); ++port_it) {
			if (port_number == i) {
				iter->set_value (route_cols.get_channel (i).label, (std::string) (*port_it)->get_value (route_cols.port_cols.label));
				iter->set_value (route_cols.get_channel (i).port, (*port_it)->get_value (route_cols.port_cols.port));
			}

			++port_number;
		}
	}

	update_config ();
}

void
PortExportChannelSelector::ChannelTreeView::update_selection_text (std::string const & path, std::string const & new_text, uint32_t channel)
{
	Gtk::TreeModel::iterator iter = get_model ()->get_iter (path);
	iter->set_value (route_cols.get_channel (channel).label, new_text);

	Glib::RefPtr<Gtk::ListStore> port_list = iter->get_value (route_cols.port_list_col);
	Gtk::ListStore::Children::iterator port_it;

	for (port_it = port_list->children().begin(); port_it != port_list->children().end(); ++port_it) {
		std::string label = port_it->get_value (route_cols.port_cols.label);
		if (label == new_text) {
			boost::weak_ptr<AudioPort> w = (*port_it)[route_cols.port_cols.port];
			iter->set_value (route_cols.get_channel (channel).port, w);
		}
	}

	update_config ();
}

RegionExportChannelSelector::RegionExportChannelSelector (ARDOUR::Session * _session,
                                                          ProfileManagerPtr manager,
                                                          ARDOUR::AudioRegion const & region,
                                                          ARDOUR::AudioTrack & track) :
  ExportChannelSelector (_session, manager),
  region (region),
  track (track),
  region_chans (region.n_channels()),
  track_chans (track.n_outputs().n_audio()),

  raw_button (type_group),
  fades_button (type_group),
  processed_button (type_group)
{
	pack_start (vbox);

	/* make fades+region gain be the default */

	fades_button.set_active ();

	raw_button.set_label (string_compose (_("Region contents without fades nor region gain (channels: %1)"), region_chans));
	raw_button.signal_toggled ().connect (sigc::mem_fun (*this, &RegionExportChannelSelector::handle_selection));
	vbox.pack_start (raw_button);

	fades_button.set_label (string_compose (_("Region contents with fades and region gain (channels: %1)"), region_chans));
	fades_button.signal_toggled ().connect (sigc::mem_fun (*this, &RegionExportChannelSelector::handle_selection));
	vbox.pack_start (fades_button);

	processed_button.set_label (string_compose (_("Track output (channels: %1)"), track_chans));
	processed_button.signal_toggled ().connect (sigc::mem_fun (*this, &RegionExportChannelSelector::handle_selection));
	vbox.pack_start (processed_button);

	sync_with_manager();
	vbox.show_all_children ();
	show_all_children ();
}

void
RegionExportChannelSelector::sync_with_manager ()
{
	state = manager->get_channel_configs().front();

	if (!state) { return; }

	switch (state->config->region_processing_type()) {
	case RegionExportChannelFactory::None:
		// Do nothing
		break;
	case RegionExportChannelFactory::Raw:
		raw_button.set_active (true);
		break;
	case RegionExportChannelFactory::Fades:
		fades_button.set_active (true);
		break;
	case RegionExportChannelFactory::Processed:
		processed_button.set_active (true);
		break;
	}

	handle_selection ();
}

void
RegionExportChannelSelector::handle_selection ()
{
	if (!state) {
		return;
	}

	state->config->clear_channels ();

	RegionExportChannelFactory::Type type = RegionExportChannelFactory::None;
	if (raw_button.get_active ()) {
		type = RegionExportChannelFactory::Raw;
	} else if (fades_button.get_active ()) {
		type = RegionExportChannelFactory::Fades;
	} else if (processed_button.get_active ()) {
		type = RegionExportChannelFactory::Processed;
	} else {
		CriticalSelectionChanged ();
		return;
	}

	factory.reset (new RegionExportChannelFactory (_session, region, track, type));
	state->config->set_region_processing_type (type);

	for (size_t chan = 0; chan < region_chans; ++chan) {
		state->config->register_channel (factory->create (chan));
	}

	CriticalSelectionChanged ();
}

TrackExportChannelSelector::TrackExportChannelSelector (ARDOUR::Session * session, ProfileManagerPtr manager)
  : ExportChannelSelector(session, manager)
{
	track_scroller.add (track_view);
	track_scroller.set_size_request (-1, 130);
	track_scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	pack_start(track_scroller);

	// Track list
	track_list = Gtk::ListStore::create (track_cols);
	track_view.set_model (track_list);
	track_view.set_headers_visible (true);

	track_view.append_column_editable (_("Track"), track_cols.selected);
	Gtk::CellRendererToggle *toggle = dynamic_cast<Gtk::CellRendererToggle *>(track_view.get_column_cell_renderer (0));
	toggle->signal_toggled().connect (sigc::hide (sigc::mem_fun (*this, &TrackExportChannelSelector::update_config)));

	Gtk::CellRendererText* text_renderer = Gtk::manage (new Gtk::CellRendererText);
	text_renderer->property_editable() = false;

	Gtk::TreeView::Column* column = track_view.get_column (0);
	column->pack_start (*text_renderer);
	column->add_attribute (text_renderer->property_text(), track_cols.label);

	fill_list();

	show_all_children ();
}

void
TrackExportChannelSelector::sync_with_manager ()
{
	// TODO implement properly
	update_config();
}

void
TrackExportChannelSelector::fill_list()
{
	track_list->clear();
	RouteList routes = *_session->get_routes();

	for (RouteList::iterator it = routes.begin(); it != routes.end(); ++it) {
		Route * route = it->get();
		if(dynamic_cast<AudioTrack *>(route)) {
			add_track(route);
		}
	}
}

void
TrackExportChannelSelector::add_track(Route * route)
{
	Gtk::TreeModel::iterator iter = track_list->append();
	Gtk::TreeModel::Row row = *iter;

	row[track_cols.selected] = true;
	row[track_cols.label] = route->name();
	row[track_cols.track] = route;
}

void
TrackExportChannelSelector::update_config()
{
	manager->clear_channel_configs();

	for (Gtk::ListStore::Children::iterator it = track_list->children().begin(); it != track_list->children().end(); ++it) {
		Gtk::TreeModel::Row row = *it;

		if (!row[track_cols.selected]) {
			continue;
		}

		ExportProfileManager::ChannelConfigStatePtr state = manager->add_channel_config();

		Route * track = row[track_cols.track];

		/* Output of track code. TODO make this an option also
		uint32_t outs = track->n_ports().n_audio();
		for (uint32_t i = 0; i < outs; ++i) {
			AudioPort * port = track->audio (i);
			if(port) {
				ExportChannelPtr channel (new PortExportChannel ());
				PortExportChannel * pec = static_cast<PortExportChannel *> (channel.get());
				pec->add_port(port);
				state->config->register_channel(channel);
			}
		}
		*/

		std::list<ExportChannelPtr> list;
		RouteExportChannel::create_from_route (list, *track);
		state->config->register_channels (list);
		state->config->set_name(track->name());
	}

	CriticalSelectionChanged ();
}
