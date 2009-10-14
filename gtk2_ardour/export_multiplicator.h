/* This file is not used at the moment. It includes code related to export a
 * multiplication graph system that can be used together with the code in
 * libs/ardour/export_multiplication.cc and libs/ardour/ardour/export_multiplication.h
 * - Sakari Bergen 6.8.2008 -
 */

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

#ifndef __export_multiplicator_h__
#define __export_multiplicator_h__

#include <utility>
#include <map>

#include "ardour/export_profile_manager.h"

#include <gtkmm.h>
#include <boost/shared_ptr.hpp>

using ARDOUR::ExportProfileManager;

class ExportMultiplicator : public Gtk::EventBox {
  public:

	ExportMultiplicator ();
	~ExportMultiplicator ();

	void set_manager (boost::shared_ptr<ExportProfileManager> _manager);

  private:

	boost::shared_ptr<ExportProfileManager>  manager;
	ExportProfileManager::MultiplicationGraph const * graph;

	/* Drawing stuff */

	Gtk::Table table;

	void redraw ();

	enum GraphLevel {
		NoLevel = 0,
		Timespans = 1,
		ChannelConfigs = 2,
		Formats = 3,
		Filenames = 4
	};

	std::pair<uint32_t, uint32_t> get_bounds (ExportProfileManager::GraphNode * node, GraphLevel current_level, GraphLevel max_level) const;

	void draw_timespan (ExportProfileManager::TimespanNodePtr node, std::pair<uint32_t, uint32_t> bounds);
	void draw_channel_config (ExportProfileManager::ChannelConfigNodePtr node, std::pair<uint32_t, uint32_t> bounds);
	void draw_format (ExportProfileManager::FormatNodePtr node, std::pair<uint32_t, uint32_t> bounds);
	void draw_filename (ExportProfileManager::FilenameNodePtr node, std::pair<uint32_t, uint32_t> bounds);

	struct TablePosition {
		uint32_t left;
		uint32_t right;
		uint32_t row;

		TablePosition (uint32_t left, uint32_t right, uint32_t row) :
		  left (left), right (right), row (row) {}

		bool operator== (TablePosition const & other) const { return (row == other.row && left == other.left && right == other.right); }
		bool operator< (TablePosition const & other) const { return (row < other.row || left < other.left || right < other.right); }
	};

	typedef std::map<TablePosition, boost::shared_ptr<Gtk::HBox> > WidgetMap;
	typedef std::pair<TablePosition, boost::shared_ptr<Gtk::HBox> > WidgetPair;

	boost::shared_ptr<Gtk::HBox> get_hbox (TablePosition position);
	WidgetMap widget_map;

	/* Button Widget */

	class ButtonWidget : public Gtk::EventBox {
	  public:
		ButtonWidget (Glib::ustring name, boost::shared_ptr<ExportProfileManager> m, ExportProfileManager::GraphNode * node);

	  private:

		Gtk::Label label;
		Gtk::VBox  vbox;

		bool on_button_press_event (GdkEventButton* event);

		void split ();
		void remove ();

		boost::shared_ptr<ExportProfileManager> manager;
		ExportProfileManager::GraphNode * node;
		float split_position;

		/* Context menu */

		Glib::RefPtr<Gtk::ActionGroup> menu_actions;
		Glib::RefPtr<Gtk::UIManager>   ui_manager;
		Gtk::Menu *                    menu;
	};
};

#endif /* __export_multiplicator_h__ */
