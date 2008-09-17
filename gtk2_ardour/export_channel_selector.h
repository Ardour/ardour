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

#include <ardour/export_profile_manager.h>

#include <gtkmm.h>
#include <sigc++/signal.h>
#include <boost/shared_ptr.hpp>

namespace ARDOUR {
	class Session;
	class ExportChannel;
	class ExportChannelConfiguration;
	class ExportHandler;
	class AudioPort;
	class IO;
}

class XMLNode;

/// 
class ExportChannelSelector : public Gtk::HBox {
  private:

	typedef boost::shared_ptr<ARDOUR::ExportChannelConfiguration> ChannelConfigPtr;
	
	typedef boost::shared_ptr<ARDOUR::ExportHandler> HandlerPtr;

  public:

	ExportChannelSelector ();
	~ExportChannelSelector ();
	
	void set_state (ARDOUR::ExportProfileManager::ChannelConfigStatePtr const state_, ARDOUR::Session * session_);
	
	sigc::signal<void> CriticalSelectionChanged;

  private:

	void fill_route_list ();
	void update_channel_count ();
	void update_split_state ();

	typedef boost::shared_ptr<ARDOUR::ExportChannel> ChannelPtr;
	typedef std::list<ChannelPtr> CahnnelList;

	ARDOUR::Session * session;
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
		Gtk::TreeModelColumn<Glib::ustring>  name;
		Gtk::TreeModelColumn<ARDOUR::IO *>   io;
		
		/* Combo list column (shared by all channels) */
		
		typedef Gtk::TreeModelColumn<Glib::RefPtr<Gtk::ListStore> > ComboCol;
		ComboCol                             port_list_col;
		
		/* Channel struct, that represents the selected port and it's name */
		
		struct Channel {
		  public:
			Channel (RouteCols & cols) { cols.add (port); cols.add (label); }
			
			Gtk::TreeModelColumn<ARDOUR::AudioPort *>  port;
			Gtk::TreeModelColumn<Glib::ustring>        label;
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
			PortCols () { add (selected); add(port); add(label); }
			
			Gtk::TreeModelColumn<bool>                  selected;  // not used ATM
			Gtk::TreeModelColumn<ARDOUR::AudioPort *>   port;
			Gtk::TreeModelColumn<Glib::ustring>         label;
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
	
		void update_toggle_selection (Glib::ustring const & path);
		void update_selection_text (Glib::ustring const & path, Glib::ustring const & new_text, uint32_t channel);
		
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

#endif /* __export_channel_selector_h__ */
