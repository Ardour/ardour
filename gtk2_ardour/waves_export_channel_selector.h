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

#ifndef __export_channel_selector_h__
#define __export_channel_selector_h__

#include <list>

#include "ardour/export_profile_manager.h"

#ifdef interface
#undef interface
#endif

#include <gtkmm.h>
#include <sigc++/signal.h>
#include <boost/shared_ptr.hpp>
#include "waves_ui.h"

namespace ARDOUR {
	class Session;
	class ExportChannelConfiguration;
	class RegionExportChannelFactory;
	class ExportHandler;
	class AudioPort;
	class IO;
	class AudioRegion;
	class AudioTrack;
}

class XMLNode;

class WavesExportChannelSelector : public Gtk::HBox, public ARDOUR::SessionHandlePtr
{
  protected:
	typedef boost::shared_ptr<ARDOUR::ExportChannelConfiguration> ChannelConfigPtr;
	typedef std::list<ChannelConfigPtr> ChannelConfigList;
	typedef boost::shared_ptr<ARDOUR::ExportProfileManager> ProfileManagerPtr;

	ProfileManagerPtr manager;

  public:
	WavesExportChannelSelector (ARDOUR::Session * session, ProfileManagerPtr manager)
		: SessionHandlePtr (session)
		, manager (manager)
	{}

	virtual ~WavesExportChannelSelector () {}

	virtual void sync_with_manager () = 0;

	sigc::signal<void> CriticalSelectionChanged;
};

class WavesPortExportChannelSelector : public WavesExportChannelSelector, public WavesUI
{
  public:

	WavesPortExportChannelSelector (ARDOUR::Session * session, ProfileManagerPtr manager);
	~WavesPortExportChannelSelector ();

	void sync_with_manager ();

  private:

	void fill_route_list ();
	void update_channel_count ();
	void update_split_state (WavesButton*);
	void on_channels_dec_button (WavesButton*);
	void on_channels_inc_button (WavesButton*);
	void change_channels_value (int change);

	typedef std::list<ARDOUR::ExportChannelPtr> CahnnelList;

	ARDOUR::ExportProfileManager::ChannelConfigStatePtr state;

	/*** GUI stuff ***/
	class RouteCols : public Gtk::TreeModelColumnRecord
	{
	  public:

		struct Channel;

		RouteCols () : n_channels (0)
			{ add (selected); add (name); add (io); add (port_list_col); }

		void add_channels (uint32_t chans);
		uint32_t n_channels;

		/* Channel count starts from one! */

		Channel & get_channel (uint32_t channel);

		/* Static columns */

		Gtk::TreeModelColumn<bool>           selected;
		Gtk::TreeModelColumn<std::string>  name;
		Gtk::TreeModelColumn<ARDOUR::IO *>   io;

		/* Combo list column (shared by all channels) */

		typedef Gtk::TreeModelColumn<Glib::RefPtr<Gtk::ListStore> > ComboCol;
		ComboCol                             port_list_col;

		/* Channel struct, that represents the selected port and its name */

		struct Channel {
		  public:
			Channel (RouteCols & cols) { cols.add (port); cols.add (label); }

			Gtk::TreeModelColumn<boost::weak_ptr<ARDOUR::AudioPort> > port;
			Gtk::TreeModelColumn<std::string> label;
		};
		std::list<Channel> channels;

		/* List of available ports
		 * Note: We need only one list of selectable ports per route,
		 * so the list is kept in the column record
		 */

		/* Column record for selecting ports for a channel from a route */

		class PortCols : public Gtk::TreeModel::ColumnRecord
		{
		  public:
			PortCols () { add(selected); add(port); add(label); }

			Gtk::TreeModelColumn<bool> selected;  // not used ATM
			Gtk::TreeModelColumn<boost::weak_ptr<ARDOUR::AudioPort> > port;
			Gtk::TreeModelColumn<std::string> label;
		};
		PortCols port_cols;
	};
	class ChannelTreeView : public Gtk::TreeView {
	  public:

		ChannelTreeView (uint32_t max_channels);
		void set_config (ChannelConfigPtr c);

		/* Routes have to be added before adding channels */

		void clear_routes () { route_list->clear (); }
		void add_route (ARDOUR::IO * route);
		void set_channel_count (uint32_t channels);

		sigc::signal<void> CriticalSelectionChanged;

	  private:

		ChannelConfigPtr config;
		void update_config ();

		/* Signal handlers for selections changes in the view */

		void update_toggle_selection (std::string const & path);
		void update_selection_text (std::string const & path, std::string const & new_text, uint32_t channel);

		RouteCols                     route_cols;
		Glib::RefPtr<Gtk::ListStore>  route_list;

		uint32_t                      static_columns;
		uint32_t                      n_channels;
	};

	Gtk::Entry&  _channels_entry;
	WavesButton& _channels_inc_button;
	WavesButton& _channels_dec_button;
	WavesButton& _split_button;
	Gtk::ScrolledWindow &_channel_scroller;
	ChannelTreeView _channel_view;
	static uint32_t __max_channels;
};

class WavesRegionExportChannelSelector : public WavesExportChannelSelector, public WavesUI
{
  public:
	WavesRegionExportChannelSelector (ARDOUR::Session * session,
									  ProfileManagerPtr manager,
									  ARDOUR::AudioRegion const & region,
									  ARDOUR::AudioTrack & track);

	virtual void sync_with_manager ();

  private:

	void handle_selection ();
	void on_raw_fades_processed_button (WavesButton*);

	ARDOUR::ExportProfileManager::ChannelConfigStatePtr state;
	boost::shared_ptr<ARDOUR::RegionExportChannelFactory> factory;
	ARDOUR::AudioRegion const & region;
	ARDOUR::AudioTrack & track;

	uint32_t region_chans;
	uint32_t track_chans;

	WavesButton& _raw_button;
	WavesButton& _fades_button;
	WavesButton& _processed_button;
};

class WavesTrackExportChannelSelector : public WavesExportChannelSelector, WavesUI
{
  public:
	WavesTrackExportChannelSelector (ARDOUR::Session * session, ProfileManagerPtr manager);

	virtual void sync_with_manager ();

  private:

	void fill_list();
	void add_track (boost::shared_ptr<ARDOUR::Route> route);
	void update_config();
	void on_region_contents_track_output_button (WavesButton*);

	ChannelConfigList configs;

	struct TrackCols : public Gtk::TreeModelColumnRecord
	{
	  public:
		Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Route> > route;
		Gtk::TreeModelColumn<std::string>     label;
		Gtk::TreeModelColumn<bool>            selected;

		TrackCols () { add (route); add(label); add(selected); }
	};
	TrackCols                    track_cols;

	Glib::RefPtr<Gtk::ListStore> track_list;
	Gtk::TreeView _track_view;

	Gtk::ScrolledWindow& _track_scroller;
	WavesButton& _region_contents_button;
	WavesButton& _track_output_button;
};

#endif /* __export_channel_selector_h__ */
