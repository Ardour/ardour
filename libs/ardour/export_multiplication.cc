/*
    Copyright (C) 2008-2012 Paul Davis 
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

/* This file is not used at the moment. It includes code related to export a
 * multiplication graph system that can be used together with the ExportMultiplicator
 * class in the gtk2_ardour folder.
 * - Sakari Bergen 6.8.2008 -
 */

void
ExportProfileManager::register_all_configs ()
{
	list<TimespanNodePtr>::iterator tsl_it; // timespan list node iterator
	for (tsl_it = graph.timespans.begin(); tsl_it != graph.timespans.end(); ++tsl_it) {
		list<GraphNode *>::const_iterator cc_it; // channel config node iterator
		for (cc_it = (*tsl_it)->get_children().begin(); cc_it != (*tsl_it)->get_children().end(); ++cc_it) {
			list<GraphNode *>::const_iterator f_it; // format node iterator
			for (f_it = (*cc_it)->get_children().begin(); f_it != (*cc_it)->get_children().end(); ++f_it) {
				list<GraphNode *>::const_iterator fn_it; // filename node iterator
				for (fn_it = (*f_it)->get_children().begin(); fn_it != (*f_it)->get_children().end(); ++fn_it) {
					/* Finally loop through each timespan in the timespan list */

					TimespanNodePtr ts_node;
					if (!(ts_node = boost::dynamic_pointer_cast<TimespanNode> (*tsl_it))) {
						throw ExportFailed (X_("Programming error, Invalid pointer cast in ExportProfileManager"));
					}

					TimespanListPtr ts_list = ts_node->data()->timespans;
					TimespanList::iterator ts_it;
					for (ts_it = ts_list->begin(); ts_it != ts_list->end(); ++ts_it) {

						TimespanPtr timespan = *ts_it;

						ChannelConfigNode * cc_node;
						if (!(cc_node = dynamic_cast<ChannelConfigNode *> (*cc_it))) {
							throw ExportFailed (X_("Programming error, Invalid pointer cast in ExportProfileManager"));
						}
						ChannelConfigPtr channel_config = cc_node->data()->config;

						FormatNode * f_node;
						if (!(f_node = dynamic_cast<FormatNode *> (*f_it))) {
							throw ExportFailed (X_("Programming error, Invalid pointer cast in ExportProfileManager"));
						}
						FormatPtr format = f_node->data()->format;

						FilenameNode * fn_node;
						if (!(fn_node = dynamic_cast<FilenameNode *> (*fn_it))) {
							throw ExportFailed (X_("Programming error, Invalid pointer cast in ExportProfileManager"));
						}
						FilenamePtr filename = fn_node->data()->filename;

						handler->add_export_config (timespan, channel_config, format, filename);
					}
				}
			}
		}
	}
}

void
ExportProfileManager::create_empty_config ()
{
	TimespanNodePtr timespan = TimespanNode::create (new TimespanState ());
	timespan->data()->timespans->push_back (handler->add_timespan());

	ChannelConfigNodePtr channel_config = ChannelConfigNode::create (new ChannelConfigState(handler->add_channel_config()));

	FormatNodePtr format;
	load_formats ();
	if (!format_list.empty()) {
		format = FormatNode::create (new FormatState (*format_list.begin ()));
	} else {
		format = FormatNode::create (new FormatState (handler->add_format ()));
	}

	FilenameNodePtr filename = FilenameNode::create (new FilenameState (handler->add_filename()));

	/* Bring everything together */

	timespan->add_child (channel_config.get(), 0);
	channel_config->add_child (format.get(), 0);
	format->add_child (filename.get(), 0);

	graph.timespans.push_back (timespan);
	graph.channel_configs.push_back (channel_config);
	graph.formats.push_back (format);
	graph.filenames.push_back (filename);
}

/*** GraphNode ***/

uint32_t ExportProfileManager::GraphNode::id_counter = 0;

ExportProfileManager::GraphNode::GraphNode ()
{
	_id = ++id_counter;
}

ExportProfileManager::GraphNode::~GraphNode ()
{
	while (!children.empty()) {
		remove_child (children.front());
	}

	while (!parents.empty()) {
		parents.front()->remove_child (this);
	}
}

void
ExportProfileManager::GraphNode::add_parent (GraphNode * parent)
{
	for (list<GraphNode *>::iterator it = parents.begin(); it != parents.end(); ++it) {
		if (*it == parent) {
			return;
		}
	}

	parents.push_back (parent);
}

void
ExportProfileManager::GraphNode::add_child (GraphNode * child, GraphNode * left_sibling)
{
	for (list<GraphNode *>::iterator it = children.begin(); it != children.end(); ++it) {
		if (*it == child) {
			return;
		}
	}

	if (left_sibling) {
		insert_after (children, left_sibling, child);
	} else {
		children.push_back (child);
	}

	child->add_parent (this);
}

bool
ExportProfileManager::GraphNode::is_ancestor_of (GraphNode const * node) const
{
	for (list<GraphNode *>::const_iterator it = children.begin(); it != children.end(); ++it) {
		if (*it == node || (*it)->is_ancestor_of (node)) {
			return true;
		}
	}

	return false;
}

bool
ExportProfileManager::GraphNode::is_descendant_of (GraphNode const * node) const
{
	for (list<GraphNode *>::const_iterator it = parents.begin(); it != parents.end(); ++it) {
		if (*it == node || (*it)->is_descendant_of (node)) {
			return true;
		}
	}

	return false;
}

void
ExportProfileManager::GraphNode::select (bool value)
{
	if (_selected == value) { return; }

	_selected = value;
	SelectChanged (value);
}

void
ExportProfileManager::GraphNode::remove_parent (GraphNode * parent)
{
	for (list<GraphNode *>::iterator it = parents.begin(); it != parents.end(); ++it) {
		if (*it == parent) {
			parents.erase (it);
			break;
		}
	}
}

void
ExportProfileManager::GraphNode::remove_child (GraphNode * child)
{
	for (list<GraphNode *>::iterator it = children.begin(); it != children.end(); ++it) {
		if (*it == child) {
			children.erase (it);
			break;
		}
	}

	child->remove_parent (this);
}

void
ExportProfileManager::split_node (GraphNode * node, float position)
{
	TimespanNode * ts_node;
	if ((ts_node = dynamic_cast<TimespanNode *> (node))) {
		split_timespan (ts_node->self_ptr(), position);
		return;
	}

	ChannelConfigNode * cc_node;
	if ((cc_node = dynamic_cast<ChannelConfigNode *> (node))) {
		split_channel_config (cc_node->self_ptr(), position);
		return;
	}

	FormatNode * f_node;
	if ((f_node = dynamic_cast<FormatNode *> (node))) {
		split_format (f_node->self_ptr(), position);
		return;
	}

	FilenameNode * fn_node;
	if ((fn_node = dynamic_cast<FilenameNode *> (node))) {
		split_filename (fn_node->self_ptr(), position);
		return;
	}
}

void
ExportProfileManager::remove_node (GraphNode * node)
{
	TimespanNode * ts_node;
	if ((ts_node = dynamic_cast<TimespanNode *> (node))) {
		remove_timespan (ts_node->self_ptr());
		return;
	}

	ChannelConfigNode * cc_node;
	if ((cc_node = dynamic_cast<ChannelConfigNode *> (node))) {
		remove_channel_config (cc_node->self_ptr());
		return;
	}

	FormatNode * f_node;
	if ((f_node = dynamic_cast<FormatNode *> (node))) {
		remove_format (f_node->self_ptr());
		return;
	}

	FilenameNode * fn_node;
	if ((fn_node = dynamic_cast<FilenameNode *> (node))) {
		remove_filename (fn_node->self_ptr());
		return;
	}
}

void
ExportProfileManager::purge_graph ()
{
	for (list<TimespanNodePtr>::iterator it = graph.timespans.begin(); it != graph.timespans.end(); ) {
		list<TimespanNodePtr>::iterator tmp = it;
		++it;

		if ((*tmp)->get_children().empty()) {
			graph.timespans.erase (tmp);
		}
	}

	for (list<ChannelConfigNodePtr>::iterator it = graph.channel_configs.begin(); it != graph.channel_configs.end(); ) {
		list<ChannelConfigNodePtr>::iterator tmp = it;
		++it;

		if ((*tmp)->get_parents().empty()) {
			graph.channel_configs.erase (tmp);
		}
	}

	for (list<FormatNodePtr>::iterator it = graph.formats.begin(); it != graph.formats.end(); ) {
		list<FormatNodePtr>::iterator tmp = it;
		++it;

		if ((*tmp)->get_parents().empty()) {
			graph.formats.erase (tmp);
		}
	}

	for (list<FilenameNodePtr>::iterator it = graph.filenames.begin(); it != graph.filenames.end(); ) {
		list<FilenameNodePtr>::iterator tmp = it;
		++it;

		if ((*tmp)->get_parents().empty()) {
			graph.filenames.erase (tmp);
		}
	}

	GraphChanged();
}

template<typename T>
void
ExportProfileManager::insert_after (list<T> & the_list, T const & position, T const & element)
{
	typename list<T>::iterator it;
	for (it = the_list.begin(); it != the_list.end(); ++it) {
		if (*it == position) {
			the_list.insert (++it, element);
			return;
		}
	}

	std::cerr << "invalid position given to ExportProfileManager::insert_after (aborting)" << std::endl;

	abort();
}

template<typename T>
void
ExportProfileManager::remove_by_element (list<T> & the_list, T const & element)
{
	typename list<T>::iterator it;
	for (it = the_list.begin(); it != the_list.end(); ++it) {
		if (*it == element) {
			the_list.erase (it);
			return;
		}
	}
}

bool
ExportProfileManager::nodes_have_one_common_child (list<GraphNode *> const & the_list)
{
	return end_of_common_child_range (the_list, the_list.begin()) == --the_list.end();
}

list<ExportProfileManager::GraphNode *>::const_iterator
ExportProfileManager::end_of_common_child_range (list<GraphNode *> const & the_list, list<GraphNode *>::const_iterator beginning)
{
	if ((*beginning)->get_children().size() != 1) { return beginning; }
	GraphNode * child = (*beginning)->get_children().front();

	list<GraphNode *>::const_iterator it = beginning;
	while (it != the_list.end() && (*it)->get_children().size() == 1 && (*it)->get_children().front() == child) {
		++it;
	}

	return --it;
}

void
ExportProfileManager::split_node_at_position (GraphNode * old_node, GraphNode * new_node, float position)
{
	list<GraphNode *> const & node_parents = old_node->get_parents();
	uint32_t split_index = (int) (node_parents.size() * position + 0.5);
	split_index = std::max ((uint32_t) 1, std::min (split_index, node_parents.size() - 1));

	list<GraphNode *>::const_iterator it = node_parents.begin();
	for (uint32_t index = 1; it != node_parents.end(); ++index) {
		if (index > split_index) {
			list<GraphNode *>::const_iterator tmp = it++;
			(*tmp)->add_child (new_node, old_node);
			(*tmp)->remove_child (old_node);
		} else {
			++it;
		}
	}
}

void
ExportProfileManager::split_timespan (TimespanNodePtr node, float)
{
	TimespanNodePtr new_timespan = duplicate_timespan_node (node);
	insert_after (graph.timespans, node, new_timespan);

	/* Note: Since a timespan selector allows all combinations of ranges
	 * there is no reason for a channel configuration to have two parents
	 */

	duplicate_timespan_children (node->self_ptr(), new_timespan);

	GraphChanged();
}

void
ExportProfileManager::split_channel_config (ChannelConfigNodePtr node, float)
{
	ChannelConfigNodePtr new_config = duplicate_channel_config_node (node);
	insert_after (graph.channel_configs, node, new_config);

	/* Channel configs have only one parent, see above! */
	node->get_parents().front()->add_child (new_config.get(), node.get());

	if (node->get_children().size() == 1) {
		new_config->add_child (node->first_child(), 0);
	} else {
		duplicate_channel_config_children (node, new_config);
	}

	GraphChanged();
}

void
ExportProfileManager::split_format (FormatNodePtr node, float position)
{
	FormatNodePtr new_format = duplicate_format_node (node);
	insert_after (graph.formats, node, new_format);

	list<GraphNode *> const & node_parents = node->get_parents();
	if (node_parents.size() == 1) {
		node_parents.front()->add_child (new_format.get(), 0);
	} else {
		node->sort_parents (graph.channel_configs);
		split_node_at_position (node.get(), new_format.get(), position);
	}

	if (node->get_children().size() == 1) {
		new_format->add_child (node->first_child(), 0);
	} else {
		duplicate_format_children (node, new_format);
	}

	GraphChanged();
}

void
ExportProfileManager::split_filename (FilenameNodePtr node, float position)
{
	FilenameNodePtr new_filename = duplicate_filename_node (node);
	insert_after (graph.filenames, node, new_filename);

	list<GraphNode *> const & node_parents = node->get_parents();

	if (node_parents.size() == 1) {
		node_parents.front()->add_child (new_filename.get(), 0);
	} else {
		node->sort_parents (graph.formats);
		split_node_at_position (node.get(), new_filename.get(), position);
	}

	GraphChanged();
}

void
ExportProfileManager::duplicate_timespan_children (TimespanNodePtr source, TimespanNodePtr target, GraphNode * insertion_point)
{
	list<GraphNode *> const & source_children = source->get_children();
	bool one_grandchild = nodes_have_one_common_child (source_children);
	GraphNode * child_insertion_point = 0;

	ChannelConfigNodePtr node_insertion_point;
	ChannelConfigNode * node_insertion_ptr;
	if (!insertion_point) { insertion_point = source->last_child(); }
	if (!(node_insertion_ptr = dynamic_cast<ChannelConfigNode *> (insertion_point))) {
		throw ExportFailed (X_("Programming error, Invalid pointer cast in ExportProfileManager"));
	}
	node_insertion_point = node_insertion_ptr->self_ptr();

	/* Keep track of common children */

	list<GraphNode *>::const_iterator common_children_begin = source_children.begin();
	list<GraphNode *>::const_iterator common_children_end = end_of_common_child_range (source_children, source_children.begin());
	GraphNode * common_child = 0;

	for (list<GraphNode *>::const_iterator it = source_children.begin(); it != source_children.end(); ++it) {
		/* Duplicate node */

		ChannelConfigNode *  node;
		ChannelConfigNodePtr new_node;

		if (!(node = dynamic_cast<ChannelConfigNode *> (*it))) {
			throw ExportFailed (X_("Programming error, Invalid pointer cast in ExportProfileManager"));
		}

		new_node = duplicate_channel_config_node (node->self_ptr());

		/* Insert in gaph's list and update insertion position */

		insert_after (graph.channel_configs, node_insertion_point, new_node);
		node_insertion_point = new_node;

		/* Handle children */

		target->add_child (new_node.get(), child_insertion_point);
		child_insertion_point = new_node.get();

		if (one_grandchild) {
			new_node->add_child (node->first_child(), 0);
		} else {
			list<GraphNode *>::const_iterator past_end = common_children_end;
			if (it == ++past_end) { // At end => start new range
				common_children_begin = it;
				common_children_end = end_of_common_child_range (source_children, it);
			}

			if (it == common_children_begin) { // At beginning => do duplication
				GraphNode * grand_child_ins_pt = common_child;
				if (!grand_child_ins_pt) {
					grand_child_ins_pt = source->last_child()->last_child();
				}
				duplicate_channel_config_children (node->self_ptr(), new_node, grand_child_ins_pt);
				common_child = new_node->first_child();
			} else { // Somewhere in between end and beginning => share child
				new_node->add_child (common_child, 0);
			}
		}
	}
}

void
ExportProfileManager::duplicate_channel_config_children (ChannelConfigNodePtr source, ChannelConfigNodePtr target, GraphNode * insertion_point)
{
	list<GraphNode *> const & source_children = source->get_children();
	bool one_grandchild = nodes_have_one_common_child (source_children);
	GraphNode * child_insertion_point = 0;

	FormatNodePtr node_insertion_point;
	FormatNode * node_insertion_ptr;
	if (!insertion_point) { insertion_point = source->last_child(); }
	if (!(node_insertion_ptr = dynamic_cast<FormatNode *> (insertion_point))) {
		throw ExportFailed (X_("Programming error, Invalid pointer cast in ExportProfileManager"));
	}
	node_insertion_point = node_insertion_ptr->self_ptr();

	/* Keep track of common children */

	list<GraphNode *>::const_iterator common_children_begin = source_children.begin();
	list<GraphNode *>::const_iterator common_children_end = end_of_common_child_range (source_children, source_children.begin());
	GraphNode * common_child = 0;

	for (list<GraphNode *>::const_iterator it = source_children.begin(); it != source_children.end(); ++it) {
		/* Duplicate node */

		FormatNode *  node;
		FormatNodePtr new_node;

		if (!(node = dynamic_cast<FormatNode *> (*it))) {
			throw ExportFailed (X_("Programming error, Invalid pointer cast in ExportProfileManager"));
		}

		new_node = duplicate_format_node (node->self_ptr());

		/* Insert in gaph's list and update insertion position */

		insert_after (graph.formats, node_insertion_point, new_node);
		node_insertion_point = new_node;

		/* Handle children */

		target->add_child (new_node.get(), child_insertion_point);
		child_insertion_point = new_node.get();

		if (one_grandchild) {
			new_node->add_child (node->first_child(), 0);
		} else {
			list<GraphNode *>::const_iterator past_end = common_children_end;
			if (it == ++past_end) { // At end => start new range
				common_children_begin = it;
				common_children_end = end_of_common_child_range (source_children, it);
			}

			if (it == common_children_begin) { // At beginning => do duplication
				GraphNode * grand_child_ins_pt = common_child;
				if (!grand_child_ins_pt) {
					grand_child_ins_pt = source->get_parents().back()->last_child()->last_child()->last_child();
				}
				duplicate_format_children (node->self_ptr(), new_node, grand_child_ins_pt);
				common_child = new_node->first_child();
			} else { // Somewhere in between end and beginning => share child
				new_node->add_child (common_child, 0);
			}
		}
	}
}

void
ExportProfileManager::duplicate_format_children (FormatNodePtr source, FormatNodePtr target, GraphNode * insertion_point)
{
	GraphNode * child_insertion_point = 0;

	FilenameNodePtr node_insertion_point;
	FilenameNode * node_insertion_ptr;
	if (!insertion_point) { insertion_point = source->last_child(); }
	if (!(node_insertion_ptr = dynamic_cast<FilenameNode *> (insertion_point))) {
		throw ExportFailed (X_("Programming error, Invalid pointer cast in ExportProfileManager"));
	}
	node_insertion_point = node_insertion_ptr->self_ptr();

	for (list<GraphNode *>::const_iterator it = source->get_children().begin(); it != source->get_children().end(); ++it) {
		/* Duplicate node */

		FilenameNode *  node;
		FilenameNodePtr new_node;

		if (!(node = dynamic_cast<FilenameNode *> (*it))) {
			throw ExportFailed (X_("Programming error, Invalid pointer cast in ExportProfileManager"));
		}

		new_node = duplicate_filename_node (node->self_ptr());

		/* Insert in gaph's list and update insertion position */

		insert_after (graph.filenames, node_insertion_point, new_node);
		node_insertion_point = new_node;

		/* Handle children */

		target->add_child (new_node.get(), child_insertion_point);
		child_insertion_point = new_node.get();
	}
}

ExportProfileManager::TimespanNodePtr
ExportProfileManager::duplicate_timespan_node (TimespanNodePtr node)
{
	TimespanStatePtr state = node->data();
	TimespanStatePtr new_state (new TimespanState ());
	TimespanNodePtr  new_node = TimespanNode::create (new_state);

	for (TimespanList::iterator it = state->timespans->begin(); it != state->timespans->end(); ++it) {
		new_state->timespans->push_back (handler->add_timespan_copy (*it));
	}

	new_state->time_format = state->time_format;
	new_state->marker_format = state->marker_format;

	return new_node;
}

ExportProfileManager::ChannelConfigNodePtr
ExportProfileManager::duplicate_channel_config_node (ChannelConfigNodePtr node)
{
	ChannelConfigStatePtr state = node->data();
	ChannelConfigStatePtr new_state (new ChannelConfigState (handler->add_channel_config_copy (state->config)));
	ChannelConfigNodePtr  new_node = ChannelConfigNode::create (new_state);

	return new_node;
}

ExportProfileManager::FormatNodePtr
ExportProfileManager::duplicate_format_node (FormatNodePtr node)
{
	FormatStatePtr state = node->data();
	FormatStatePtr new_state (new FormatState (handler->add_format_copy (state->format)));
	FormatNodePtr  new_node = FormatNode::create (new_state);

	return new_node;
}

ExportProfileManager::FilenameNodePtr
ExportProfileManager::duplicate_filename_node (FilenameNodePtr node)
{
	FilenameStatePtr state = node->data();
	FilenameStatePtr new_state (new FilenameState (handler->add_filename_copy (state->filename)));
	FilenameNodePtr  new_node = FilenameNode::create (new_state);

	return new_node;
}

void
ExportProfileManager::remove_timespan (TimespanNodePtr node)
{
	remove_by_element (graph.timespans, node);
	purge_graph ();
}

void
ExportProfileManager::remove_channel_config (ChannelConfigNodePtr node)
{
	remove_by_element (graph.channel_configs, node);
	purge_graph ();
}

void
ExportProfileManager::remove_format (FormatNodePtr node)
{
	remove_by_element (graph.formats, node);
	purge_graph ();
}

void
ExportProfileManager::remove_filename (FilenameNodePtr node)
{
	remove_by_element (graph.filenames, node);
	purge_graph ();
}
