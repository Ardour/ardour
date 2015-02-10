/*
    Copyright (C) 2008 Paul Davis
    Copyright (C) 2015 Waves Audio Ltd.
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

#include "waves_export_channel_selector.h"

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

uint32_t WavesPortExportChannelSelector::__max_channels = 2;

WavesPortExportChannelSelector::WavesPortExportChannelSelector (ARDOUR::Session * session, ProfileManagerPtr manager)
  : WavesExportChannelSelector (session, manager)
  , WavesUI ("waves_port_export_channel_selector.xml", *this)
  , _split_button (get_waves_button ("split_button"))
  , _channels_inc_button (get_waves_button ("channels_inc_button"))
  , _channels_dec_button (get_waves_button ("channels_dec_button"))
  , _channels_entry (get_entry ("channels_entry"))
  , _channel_scroller (get_scrolled_window ("channel_scroller"))
  , _channel_view (__max_channels)
{
	_channel_scroller.add (_channel_view);

	_channels_entry.signal_changed().connect (sigc::mem_fun (*this, &WavesPortExportChannelSelector::update_channel_count));
	_channels_inc_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesPortExportChannelSelector::on_channels_inc_button));
	_channels_dec_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesPortExportChannelSelector::on_channels_dec_button));
	_split_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesPortExportChannelSelector::update_split_state));

	_channel_view.CriticalSelectionChanged.connect (CriticalSelectionChanged.make_slot());

	/* Finalize */
	sync_with_manager();
	show_all_children ();
}

WavesPortExportChannelSelector::~WavesPortExportChannelSelector ()
{
// 	if (session) {
// 		session->add_instant_xml (get_state(), false);
// 	}
}

void
WavesPortExportChannelSelector::sync_with_manager ()
{
	state = manager->get_channel_configs().front();

	_split_button.set_active_state (state->config->get_split() ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	_channels_entry.set_text (string_compose ("%1", state->config->get_n_chans()));

	fill_route_list ();
	_channel_view.set_config (state->config);
}

void
WavesPortExportChannelSelector::fill_route_list ()
{
	_channel_view.clear_routes ();
	RouteList routes = *_session->get_routes();

	/* Add master bus and then everything else */

	if (_session->master_out()) {
		ARDOUR::IO* master = _session->master_out()->output().get();
		_channel_view.add_route (master);
	}

	for (RouteList::iterator it = routes.begin(); it != routes.end(); ++it) {
		if ((*it)->is_master () || (*it)->is_monitor ()) {
			continue;
		}
		_channel_view.add_route ((*it)->output().get());
	}

	update_channel_count ();
}

void
WavesPortExportChannelSelector::update_channel_count ()
{
	uint32_t channels = (uint32_t)PBD::atoi (_channels_entry.get_text());
	uint32_t correct_channels = min (__max_channels, max (uint32_t(1), channels));
	if (correct_channels != channels) {
		_channels_entry.set_text (string_compose ("%1", correct_channels));
	}
	_channel_view.set_channel_count (correct_channels);
	CriticalSelectionChanged();
}

void
WavesPortExportChannelSelector::on_channels_inc_button (WavesButton*)
{
	change_channels_value (1);
}

void
WavesPortExportChannelSelector::on_channels_dec_button (WavesButton*)
{
	change_channels_value (-1);
}

void
WavesPortExportChannelSelector::change_channels_value (int change)
{
	int channels = min ( int (__max_channels), max (1, PBD::atoi (_channels_entry.get_text()) + change));
	_channels_entry.set_text (string_compose ("%1", channels));
	CriticalSelectionChanged();
}

void
WavesPortExportChannelSelector::update_split_state (WavesButton*)
{
	state->config->set_split (_split_button.active_state () == Gtkmm2ext::ExplicitActive);
	CriticalSelectionChanged();
}

void
WavesPortExportChannelSelector::RouteCols::add_channels (uint32_t chans)
{
	while (chans > 0) {
		channels.push_back (Channel (*this));
		++n_channels;
		--chans;
	}
}

WavesPortExportChannelSelector::RouteCols::Channel &
WavesPortExportChannelSelector::RouteCols::get_channel (uint32_t channel)
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

WavesPortExportChannelSelector::ChannelTreeView::ChannelTreeView (uint32_t max_channels) :
  n_channels (0)
{
	/* Main columns */

	route_cols.add_channels (max_channels);

	route_list = Gtk::ListStore::create(route_cols);
	set_model (route_list);

	/* Add column with toggle and text */

	append_column_editable (_(""), route_cols.selected);
	Gtk::CellRendererToggle *toggle = dynamic_cast<Gtk::CellRendererToggle *>(get_column_cell_renderer (0));
	toggle->signal_toggled().connect (sigc::mem_fun (*this, &WavesPortExportChannelSelector::ChannelTreeView::update_toggle_selection));

	append_column (_("Bus or Track"), route_cols.name);
	static_columns = get_columns().size();
}

void
WavesPortExportChannelSelector::ChannelTreeView::set_config (ChannelConfigPtr c)
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
WavesPortExportChannelSelector::ChannelTreeView::add_route (ARDOUR::IO * io)
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
WavesPortExportChannelSelector::ChannelTreeView::set_channel_count (uint32_t channels)
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
		column->pack_start (*combo_renderer, false);

		// append_column (*column);

		column->add_attribute (combo_renderer->property_text(), route_cols.get_channel(n_channels).label);
		column->add_attribute (combo_renderer->property_model(), route_cols.port_list_col);
		column->add_attribute (combo_renderer->property_editable(), route_cols.selected);

		combo_renderer->signal_edited().connect (sigc::bind (sigc::mem_fun (*this, &WavesPortExportChannelSelector::ChannelTreeView::update_selection_text), n_channels));

		/* put data into view */

		for (Gtk::ListStore::Children::iterator it = route_list->children().begin(); it != route_list->children().end(); ++it) {
			std::string label = it->get_value(route_cols.selected) ? "(none)" : "";
			it->set_value (route_cols.get_channel (n_channels).label, label);
			it->set_value (route_cols.get_channel (n_channels).port, boost::weak_ptr<AudioPort> ());
		}

		/* set column width */

		// get_column (static_columns + n_channels - 1)->set_min_width (80);

		--offset;
	}
	/*
	while (offset < 0) {
		--n_channels;

		remove_column (*get_column (n_channels + static_columns));

		++offset;
	}
	*/
	update_config ();
}

void
WavesPortExportChannelSelector::ChannelTreeView::update_config ()
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
WavesPortExportChannelSelector::ChannelTreeView::update_toggle_selection (std::string const & path)
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
WavesPortExportChannelSelector::ChannelTreeView::update_selection_text (std::string const & path, std::string const & new_text, uint32_t channel)
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

WavesRegionExportChannelSelector::WavesRegionExportChannelSelector (ARDOUR::Session * _session,
																	ProfileManagerPtr manager,
																	ARDOUR::AudioRegion const & region,
																	ARDOUR::AudioTrack & track)
  : WavesExportChannelSelector (_session, manager)
  , WavesUI ("waves_region_export_channel_selector.xml", *this)
  , region (region)
  , track (track)
  , region_chans (region.n_channels())
  , track_chans (track.n_outputs().n_audio())
  , _raw_button (get_waves_button ("raw_button"))
  , _fades_button (get_waves_button ("fades_button"))
  , _processed_button (get_waves_button ("processed_button"))
{
	get_label ("raw_label").set_text (string_compose ("%1", region_chans));
	_raw_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesRegionExportChannelSelector::on_raw_fades_processed_button));

	get_label ("fades_label").set_text (string_compose ("%1", region_chans));
	_fades_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesRegionExportChannelSelector::on_raw_fades_processed_button));

	get_label ("processed_label").set_text (string_compose ("%1", track_chans));
	_processed_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesRegionExportChannelSelector::on_raw_fades_processed_button));

	state = manager->get_channel_configs().front();
	handle_selection (); // To follow the buttons

	sync_with_manager();
	show_all_children ();
}

void
WavesRegionExportChannelSelector::sync_with_manager ()
{

	if (!state) { return; }

	RegionExportChannelFactory::Type current_type (state->config->region_processing_type());

	_raw_button.set_active_state (current_type == RegionExportChannelFactory::Raw ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	_fades_button.set_active_state (current_type == RegionExportChannelFactory::Fades ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	_processed_button.set_active_state (current_type == RegionExportChannelFactory::Processed ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);

	handle_selection ();
}

void
WavesRegionExportChannelSelector::on_raw_fades_processed_button (WavesButton* button)
{
	_raw_button.set_active_state (button == &_raw_button ? Gtkmm2ext::ExplicitActive:Gtkmm2ext::Off);
	_fades_button.set_active_state (button == &_fades_button ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	_processed_button.set_active_state (button == &_processed_button ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);

	handle_selection ();
}

void
WavesRegionExportChannelSelector::handle_selection ()
{
	if (!state) {
		return;
	}

	state->config->clear_channels ();

	RegionExportChannelFactory::Type type = RegionExportChannelFactory::None;
	if (_raw_button.active_state () == Gtkmm2ext::ExplicitActive) {
		type = RegionExportChannelFactory::Raw;
	} else if (_fades_button.active_state () == Gtkmm2ext::ExplicitActive) {
		type = RegionExportChannelFactory::Fades;
	} else if (_processed_button.active_state () == Gtkmm2ext::ExplicitActive) {
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

/* Track export channel selector */

WavesTrackExportChannelSelector::WavesTrackExportChannelSelector (ARDOUR::Session * session, ProfileManagerPtr manager)
  : WavesExportChannelSelector(session, manager)
  , WavesUI ("waves_track_export_channel_selector.xml", *this)
  , _region_contents_button (get_waves_button ("region_contents_button"))
  , _track_output_button (get_waves_button ("track_output_button"))
  , _track_scroller (get_scrolled_window ("track_scroller"))
{
	// Track scroller
	_region_contents_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesTrackExportChannelSelector::on_region_contents_track_output_button));
	_track_output_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesTrackExportChannelSelector::on_region_contents_track_output_button));
	_track_scroller.add (_track_view);

	// Track list
	track_list = Gtk::ListStore::create (track_cols);
	_track_view.set_model (track_list);
	_track_view.set_headers_visible (true);

	_track_view.append_column_editable (_(""), track_cols.selected);
	Gtk::CellRendererToggle *toggle = dynamic_cast<Gtk::CellRendererToggle *>(_track_view.get_column_cell_renderer (0));
	toggle->signal_toggled().connect (sigc::hide (sigc::mem_fun (*this, &WavesTrackExportChannelSelector::update_config)));

	_track_view.append_column (_("Track"), track_cols.label);

	fill_list();
	show_all_children ();
}

void
WavesTrackExportChannelSelector::sync_with_manager ()
{
	// TODO implement properly
	update_config();
}

void
WavesTrackExportChannelSelector::fill_list()
{
	track_list->clear();
	RouteList routes = *_session->get_routes();

	for (RouteList::iterator it = routes.begin(); it != routes.end(); ++it) {
		if (!boost::dynamic_pointer_cast<Track>(*it)) {
			// not a track, must be a bus
			if ((*it)->is_master () || (*it)->is_monitor ()) {
				continue;
			}
			// not monitor or master bus
			add_track (*it);
		}
	}
	for (RouteList::iterator it = routes.begin(); it != routes.end(); ++it) {
		if (boost::dynamic_pointer_cast<AudioTrack>(*it)) {
			add_track (*it);
		}
	}
}

void
WavesTrackExportChannelSelector::add_track (boost::shared_ptr<Route> route)
{
	Gtk::TreeModel::iterator iter = track_list->append();
	Gtk::TreeModel::Row row = *iter;

	row[track_cols.selected] = true;
	row[track_cols.label] = route->name();
	row[track_cols.route] = route;
}

void
WavesTrackExportChannelSelector::update_config()
{
	manager->clear_channel_configs();

	for (Gtk::ListStore::Children::iterator it = track_list->children().begin(); it != track_list->children().end(); ++it) {
		Gtk::TreeModel::Row row = *it;

		if (!row[track_cols.selected]) {
			continue;
		}

		ExportProfileManager::ChannelConfigStatePtr state = manager->add_channel_config();

		boost::shared_ptr<Route> route = row[track_cols.route];

		if (_track_output_button.active_state () == Gtkmm2ext::ExplicitActive) {
			uint32_t outs = route->n_outputs().n_audio();
			for (uint32_t i = 0; i < outs; ++i) {
				boost::shared_ptr<AudioPort> port = route->output()->audio (i);
				if (port) {
					ExportChannelPtr channel (new PortExportChannel ());
					PortExportChannel * pec = static_cast<PortExportChannel *> (channel.get());
					pec->add_port(port);
					state->config->register_channel(channel);
				}
			}
		} else {
			std::list<ExportChannelPtr> list;
			RouteExportChannel::create_from_route (list, route);
			state->config->register_channels (list);
		}

		state->config->set_name (route->name());
	}

	CriticalSelectionChanged ();
}

void
WavesTrackExportChannelSelector::on_region_contents_track_output_button (WavesButton* button)
{
	_region_contents_button.set_active_state (button == &_region_contents_button ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	_track_output_button.set_active_state (button == &_track_output_button ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	update_config ();
}
