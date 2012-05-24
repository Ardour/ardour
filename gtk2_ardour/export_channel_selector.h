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

#ifndef __export_channel_selector_h__
#define __export_channel_selector_h__

#include <list>

#include "ardour/export_profile_manager.h"

#include <gtkmm.h>
#include <sigc++/signal.h>
#include <boost/shared_ptr.hpp>

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

class ExportChannelSelector : public Gtk::HBox, public ARDOUR::SessionHandlePtr
{
  protected:
	typedef boost::shared_ptr<ARDOUR::ExportChannelConfiguration> ChannelConfigPtr;
	typedef std::list<ChannelConfigPtr> ChannelConfigList;
	typedef boost::shared_ptr<ARDOUR::ExportProfileManager> ProfileManagerPtr;

	ProfileManagerPtr manager;

  public:
	ExportChannelSelector (ARDOUR::Session * session, ProfileManagerPtr manager)
		: SessionHandlePtr (session)
		, manager (manager)
	{}

	virtual ~ExportChannelSelector () {}

	virtual void sync_with_manager () = 0;

	sigc::signal<void> CriticalSelectionChanged;
};

class PortExportChannelSelector : public ExportChannelSelector
{

  public:

	PortExportChannelSelector (ARDOUR::Session * session, ProfileManagerPtr manager);
	~PortExportChannelSelector ();

	void sync_with_manager ();

  private:

	void fill_route_list ();
	void update_channel_count ();
	void update_split_state ();

	typedef std::list<ARDOUR::ExportChannelPtr> CahnnelList;

	ARDOUR::ExportProfileManager::ChannelConfigStatePtr state;

	/*** GUI stuff ***/

	Gtk::VBox         channels_vbox;
	Gtk::HBox         channels_hbox;

	Gtk::Label        channels_label;
	Gtk::SpinButton   channels_spinbutton;
	Gtk::CheckButton  split_checkbox;

	/* Column record for channel selector view */

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

		/* Channel struct, that represents the selected port and it's name */

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

	/* Channels view */

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

	uint32_t                     max_channels;

	Gtk::ScrolledWindow          channel_scroller;
	Gtk::Alignment               channel_alignment;
	ChannelTreeView              channel_view;

};

class RegionExportChannelSelector : public ExportChannelSelector
{
  public:
	RegionExportChannelSelector (ARDOUR::Session * session,
	                             ProfileManagerPtr manager,
	                             ARDOUR::AudioRegion const & region,
	                             ARDOUR::AudioTrack & track);

	virtual void sync_with_manager ();

  private:

	void handle_selection ();

	ARDOUR::ExportProfileManager::ChannelConfigStatePtr state;
	boost::shared_ptr<ARDOUR::RegionExportChannelFactory> factory;
	ARDOUR::AudioRegion const & region;
	ARDOUR::AudioTrack & track;

	uint32_t region_chans;
	uint32_t track_chans;

	/*** GUI components ***/

	Gtk::VBox             vbox;

	Gtk::RadioButtonGroup type_group;
	Gtk::RadioButton      raw_button;
	Gtk::RadioButton      fades_button;
	Gtk::RadioButton      processed_button;
};

class TrackExportChannelSelector : public ExportChannelSelector
{
  public:
	TrackExportChannelSelector (ARDOUR::Session * session, ProfileManagerPtr manager);

	virtual void sync_with_manager ();

  private:

	void fill_list();
	void add_track(ARDOUR::Route * route);
	void update_config();

	ChannelConfigList configs;

	struct TrackCols : public Gtk::TreeModelColumnRecord
	{
	  public:
		Gtk::TreeModelColumn<ARDOUR::Route *> track;
		Gtk::TreeModelColumn<std::string>     label;
		Gtk::TreeModelColumn<bool>            selected;

		TrackCols () { add (track); add(label); add(selected); }
	};
	TrackCols                    track_cols;

	Glib::RefPtr<Gtk::ListStore> track_list;
	Gtk::TreeView                track_view;

	Gtk::ScrolledWindow          track_scroller;

};

#endif /* __export_channel_selector_h__ */
