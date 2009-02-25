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

#include "export_multiplicator.h"

#include <cassert>

#include "pbd/compose.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

#define CALL_MEMBER_FN(object,ptrToMember) ((object).*(ptrToMember))

ExportMultiplicator::ExportMultiplicator () :
  graph (0)
{
	add (table);
}

ExportMultiplicator::~ExportMultiplicator ()
{}

void
ExportMultiplicator::set_manager (boost::shared_ptr<ARDOUR::ExportProfileManager> _manager)
{
	manager = _manager;
	manager->GraphChanged.connect (sigc::mem_fun (*this, &ExportMultiplicator::redraw));
	
	redraw();
}

void
ExportMultiplicator::redraw ()
{
	if (!manager) { return; }

	graph = &manager->get_graph();
	
	/* Empty table */
	
	table.foreach (sigc::mem_fun (table, &Gtk::Table::remove));
	widget_map.clear();

	/* Calculate table dimensions */
	
	uint32_t max_width = 0;
	GraphLevel max_level = NoLevel;
	
	if (graph->timespans.size() > max_width) {
		max_width = graph->timespans.size();
		max_level = Timespans;
	}
	
	if (graph->channel_configs.size() > max_width) {
		max_width = graph->channel_configs.size();
		max_level = ChannelConfigs;
	}
	
	if (graph->formats.size() > max_width) {
		max_width = graph->formats.size();
		max_level = Formats;
	}
	
	if (graph->filenames.size() > max_width) {
		max_width = graph->filenames.size();
		max_level = Filenames;
	}
	
	table.resize (4, max_width);
	
	std::cout << "Table width: " << max_width << std::endl;
	
	/* Fill table */
	
	for (list<ExportProfileManager::TimespanNodePtr>::const_iterator it = graph->timespans.begin(); it != graph->timespans.end(); ++it) {
		draw_timespan (*it, get_bounds (it->get(), Timespans, max_level));
	}
	
	for (list<ExportProfileManager::ChannelConfigNodePtr>::const_iterator it = graph->channel_configs.begin(); it != graph->channel_configs.end(); ++it) {
		draw_channel_config (*it, get_bounds (it->get(), ChannelConfigs, max_level));
	}
	
	for (list<ExportProfileManager::FormatNodePtr>::const_iterator it = graph->formats.begin(); it != graph->formats.end(); ++it) {
		draw_format (*it, get_bounds (it->get(), Formats, max_level));
	}
	
	for (list<ExportProfileManager::FilenameNodePtr>::const_iterator it = graph->filenames.begin(); it != graph->filenames.end(); ++it) {
		draw_filename (*it, get_bounds (it->get(), Filenames, max_level));
	}
	
	show_all_children ();
}

std::pair<uint32_t, uint32_t>
ExportMultiplicator::get_bounds (ARDOUR::ExportProfileManager::GraphNode * node, GraphLevel current_level, GraphLevel max_level) const
{
	assert (current_level != NoLevel && max_level != NoLevel && graph);

	uint32_t left_bound = 0;
	uint32_t right_bound = 0;
	
	bool left_bound_found = false;
	
	bool (ExportProfileManager::GraphNode::*relation_func) (ExportProfileManager::GraphNode const *) const;
	if (max_level < current_level) {
		std::cout << "using 'is_ancestor_of'" << std::endl;
		relation_func = &ExportProfileManager::GraphNode::is_ancestor_of;
	} else if (max_level > current_level) {
		std::cout << "using 'is_descendant_of'" << std::endl;
		relation_func = &ExportProfileManager::GraphNode::is_descendant_of;
	} else {
		std::cout << "using 'equals'" << std::endl;
		relation_func = &ExportProfileManager::GraphNode::equals;
	}
	
	switch (max_level) {
	  case Timespans:
		for (list<ExportProfileManager::TimespanNodePtr>::const_iterator it = graph->timespans.begin(); it != graph->timespans.end(); ++it) {
			if (CALL_MEMBER_FN(**it, relation_func) (node)) {
				left_bound_found = true;
			} else if (!left_bound_found) {
				++left_bound;
			}
			
			if (left_bound_found && !CALL_MEMBER_FN(**it, relation_func) (node)) {
				break;
			} else {
				++right_bound;
			}
		}
		break;
	
	  case ChannelConfigs:
		for (list<ExportProfileManager::ChannelConfigNodePtr>::const_iterator it = graph->channel_configs.begin(); it != graph->channel_configs.end(); ++it) {
			if (CALL_MEMBER_FN(**it, relation_func) (node)) {
				left_bound_found = true;
			} else if (!left_bound_found) {
				++left_bound;
			}
			
			if (left_bound_found && !CALL_MEMBER_FN(**it, relation_func) (node)) {
				break;
			} else {
				++right_bound;
			}
		}
		break;
	
	  case Formats:
		for (list<ExportProfileManager::FormatNodePtr>::const_iterator it = graph->formats.begin(); it != graph->formats.end(); ++it) {
			if (CALL_MEMBER_FN(**it, relation_func) (node)) {
				left_bound_found = true;
			} else if (!left_bound_found) {
				++left_bound;
			}
			
			if (left_bound_found && !CALL_MEMBER_FN(**it, relation_func) (node)) {
				break;
			} else {
				++right_bound;
			}
		}
		break;
	
	  case Filenames:
		for (list<ExportProfileManager::FilenameNodePtr>::const_iterator it = graph->filenames.begin(); it != graph->filenames.end(); ++it) {
			if (CALL_MEMBER_FN(**it, relation_func) (node)) {
				std::cout << "filename relation check returned true" << std::endl;
				left_bound_found = true;
			} else if (!left_bound_found) {
				std::cout << "filename relation check returned false" << std::endl;
				++left_bound;
			}
			
			if (left_bound_found && !CALL_MEMBER_FN(**it, relation_func) (node)) {
				break;
			} else {
				++right_bound;
			}
		}
		break;
	
	  case NoLevel:
		// Not reached !
		break;
	}
	
	return std::pair<uint32_t, uint32_t> (left_bound, right_bound);
}

void
ExportMultiplicator::draw_timespan (ARDOUR::ExportProfileManager::TimespanNodePtr node, std::pair<uint32_t, uint32_t> bounds)
{
	ButtonWidget * button = Gtk::manage (new ButtonWidget (string_compose ("Timespan %1", node->id()), manager, node.get()));
	get_hbox (TablePosition (bounds.first, bounds.second, Timespans))->pack_end (*button, true, true);
}

void
ExportMultiplicator::draw_channel_config (ARDOUR::ExportProfileManager::ChannelConfigNodePtr node, std::pair<uint32_t, uint32_t> bounds)
{
	ButtonWidget * button = Gtk::manage (new ButtonWidget (string_compose ("Channel config %1", node->id()), manager, node.get()));
	get_hbox (TablePosition (bounds.first, bounds.second, ChannelConfigs))->pack_end (*button, true, true);
}

void
ExportMultiplicator::draw_format (ARDOUR::ExportProfileManager::FormatNodePtr node, std::pair<uint32_t, uint32_t> bounds)
{
	ButtonWidget * button = Gtk::manage (new ButtonWidget (string_compose ("Format %1", node->id()), manager, node.get()));
	get_hbox (TablePosition (bounds.first, bounds.second, Formats))->pack_end (*button, true, true);
}

void
ExportMultiplicator::draw_filename (ARDOUR::ExportProfileManager::FilenameNodePtr node, std::pair<uint32_t, uint32_t> bounds)
{
	ButtonWidget * button = Gtk::manage (new ButtonWidget (string_compose ("Filename %1", node->id()), manager, node.get()));
	get_hbox (TablePosition (bounds.first, bounds.second, Filenames))->pack_end (*button, true, true);
}

boost::shared_ptr<Gtk::HBox>
ExportMultiplicator::get_hbox (TablePosition position)
{
	WidgetMap::iterator it = widget_map.find (position);
	if (it != widget_map.end()) { return it->second; }
	
	boost::shared_ptr<Gtk::HBox> widget = widget_map.insert (WidgetPair (position, boost::shared_ptr<Gtk::HBox> (new Gtk::HBox ()))).first->second;
	table.attach (*widget, position.left, position.right, position.row - 1, position.row);
	
	return widget;
}

ExportMultiplicator::ButtonWidget::ButtonWidget (Glib::ustring name, boost::shared_ptr<ExportProfileManager> m, ExportProfileManager::GraphNode * node) :
  label (name),
  node (node),
  split_position (0.5)
{
	manager = m;

	menu_actions = Gtk::ActionGroup::create();
	menu_actions->add (Gtk::Action::create ("Split", _("_Split here")), sigc::mem_fun (*this, &ExportMultiplicator::ButtonWidget::split));
	menu_actions->add (Gtk::Action::create ("Remove", _("_Remove")), sigc::mem_fun (*this, &ExportMultiplicator::ButtonWidget::remove));
	
	ui_manager = Gtk::UIManager::create();
	ui_manager->insert_action_group (menu_actions);
	
	Glib::ustring ui_info =
		"<ui>"
		"  <popup name='PopupMenu'>"
		"    <menuitem action='Split'/>"
		"    <menuitem action='Remove'/>"
		"  </popup>"
		"</ui>";

	ui_manager->add_ui_from_string (ui_info);
	menu = dynamic_cast<Gtk::Menu*> (ui_manager->get_widget ("/PopupMenu")); 

	add_events (Gdk::BUTTON_PRESS_MASK);
	signal_button_press_event ().connect (sigc::mem_fun (*this, &ExportMultiplicator::ButtonWidget::on_button_press_event));
	
	modify_bg (Gtk::STATE_NORMAL, Gdk::Color ("#0000"));
	set_border_width (1);
	vbox.pack_start (label, true, true, 4);
	add (vbox);
}

bool
ExportMultiplicator::ButtonWidget::on_button_press_event (GdkEventButton* event)
{
	if(event->type != GDK_BUTTON_PRESS) { return false; }
	if (event->button == 1) {
		node->select (!node->selected ());
		
		if (node->selected ()) {
			unset_bg (Gtk::STATE_NORMAL);
			modify_bg (Gtk::STATE_NORMAL, Gdk::Color ("#194756"));
		} else {
			unset_bg (Gtk::STATE_NORMAL);
			modify_bg (Gtk::STATE_NORMAL, Gdk::Color ("#0000"));
		}
		
		return true;
		
	} else if (event->button == 3) {
		int x, y;
		get_pointer (x, y);
		split_position = (float) x / get_width();
	
		menu->popup (event->button, event->time);
		return true;
	}

	return false;
}

void
ExportMultiplicator::ButtonWidget::split ()
{
	manager->split_node (node, split_position);
}

void
ExportMultiplicator::ButtonWidget::remove ()
{
	manager->remove_node (node);
}
