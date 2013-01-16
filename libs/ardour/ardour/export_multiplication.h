/*
    Copyright (C) 2012 Paul Davis 

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

/*** Graph classes ***/
  public:

	/// A node in the hierarchical graph that represents a multiplicatable export item
	class GraphNode {
	  public:
		GraphNode ();
		virtual ~GraphNode ();

		uint32_t id() const { return _id; }

		/* Children and parents. Note: only children are kept in order! */

		list<GraphNode *> const & get_parents () const { return parents; }

		void add_child (GraphNode * child, GraphNode * left_sibling);
		void remove_child (GraphNode * child);
		GraphNode * first_child () const { return children.front(); }
		GraphNode * last_child () const { return children.back(); }
		list<GraphNode *> const & get_children () const { return children; }

		/* Relation functions */

		bool is_ancestor_of (GraphNode const * node) const;
		bool is_descendant_of (GraphNode const * node) const;
		bool equals (GraphNode const * node) const { return node == this; }

		/* Selection functions */

		bool selected () const { return _selected; }
		void select (bool value);

		PBD::Signal1<void,bool> SelectChanged;

	  protected:

		/* Parent manipulation functions should be used only from child manipulation functions! */

		void add_parent (GraphNode * parent);
		void remove_parent (GraphNode * parent);

		list<GraphNode *> parents;
		list<GraphNode *> children;

		bool           _selected;
		uint32_t       _id;
		static uint32_t id_counter;
	};

	/// A graph node that contains data
	template <typename T>
	class DataNode : public GraphNode {
	  private:
		typedef boost::shared_ptr<T> DataPtr;
		typedef boost::shared_ptr<DataNode<T> > SelfPtr;
		typedef boost::weak_ptr<DataNode<T> > WeakSelfPtr;

		DataNode (DataPtr data) : _data (data) {}
		void set_self_ptr (boost::shared_ptr<DataNode<T> > ptr) { _self_ptr = ptr; }

	  public:
		static SelfPtr create (T * data)
		{
			SelfPtr ptr = SelfPtr (new DataNode<T> (DataPtr (data)));
			ptr->set_self_ptr (ptr);
			return ptr;
		}

		static SelfPtr create (DataPtr data)
		{
			SelfPtr ptr = SelfPtr (new DataNode<T> (data));
			ptr->set_self_ptr (ptr);
			return ptr;
		}

		DataPtr data() { return _data; }
		SelfPtr self_ptr () { return _self_ptr.lock(); }

		template<typename P> // Parent's data type
		void sort_parents (list<boost::shared_ptr<DataNode<P> > > const & sort_list)
		{
			parents.sort (NodeSorter<P> (sort_list));
		}

	  private:
		DataPtr     _data;
		WeakSelfPtr _self_ptr;
	};

  private:
	/* Sorts GraphNodes according to a list of DataNodes */

	template<typename T>
	class NodeSorter {
	  public:
		typedef list<boost::shared_ptr<DataNode<T> > > ListType;

		NodeSorter (ListType const & list) : list (list) {}

		bool operator() (GraphNode * one, GraphNode * other) // '<' operator
		{
			if (one == other) { return false; } // Strict weak ordering
			for (typename ListType::const_iterator it = list.begin(); it != list.end(); ++it) {
				if (it->get() == one) {
					return true;
				}
				if (it->get() == other) {
					return false;
				}
			}

			std::cerr << "Invalid comparison list given to NodeSorter" << std::endl;

			abort();
		}

	  private:
		ListType const & list;
	};

/*** Multiplication management ***/
  public:

	typedef DataNode<TimespanState> TimespanNode;
	typedef boost::shared_ptr<TimespanNode> TimespanNodePtr;

	typedef DataNode<ChannelConfigState> ChannelConfigNode;
	typedef boost::shared_ptr<ChannelConfigNode> ChannelConfigNodePtr;

	typedef DataNode<FormatState> FormatNode;
	typedef boost::shared_ptr<FormatNode> FormatNodePtr;

	typedef DataNode<FilenameState> FilenameNode;
	typedef boost::shared_ptr<FilenameNode> FilenameNodePtr;

	struct MultiplicationGraph {
		list<TimespanNodePtr>      timespans;
		list<ChannelConfigNodePtr> channel_configs;
		list<FormatNodePtr>        formats;
		list<FilenameNodePtr>      filenames;
	};

	MultiplicationGraph const & get_graph () { return graph; }

	void split_node (GraphNode * node, float position);
	void remove_node (GraphNode * node);

	PBD::Signal0<void> GraphChanged;

  private:

	void purge_graph ();

	template<typename T>
	static void insert_after (list<T> & the_list, T const & position, T const & element);

	template<typename T>
	static void remove_by_element (list<T> & the_list, T const & element);

	bool nodes_have_one_common_child (list<GraphNode *> const & the_list);
	list<GraphNode *>::const_iterator end_of_common_child_range (list<GraphNode *> const & the_list, list<GraphNode *>::const_iterator beginning);
	void split_node_at_position (GraphNode * old_node, GraphNode * new_node, float position);

	void split_timespan (TimespanNodePtr node, float position = 0.5);
	void split_channel_config (ChannelConfigNodePtr node, float position = 0.5);
	void split_format (FormatNodePtr node, float position = 0.5);
	void split_filename (FilenameNodePtr node, float position = 0.5);

	void duplicate_timespan_children (TimespanNodePtr source, TimespanNodePtr target, GraphNode * insertion_point = 0);
	void duplicate_channel_config_children (ChannelConfigNodePtr source, ChannelConfigNodePtr target, GraphNode * insertion_point = 0);
	void duplicate_format_children (FormatNodePtr source, FormatNodePtr target, GraphNode * insertion_point = 0);

	TimespanNodePtr duplicate_timespan_node (TimespanNodePtr node);
	ChannelConfigNodePtr duplicate_channel_config_node (ChannelConfigNodePtr node);
	FormatNodePtr duplicate_format_node (FormatNodePtr node);
	FilenameNodePtr duplicate_filename_node (FilenameNodePtr node);

	void remove_timespan (TimespanNodePtr node);
	void remove_channel_config (ChannelConfigNodePtr node);
	void remove_format (FormatNodePtr node);
	void remove_filename (FilenameNodePtr node);

	MultiplicationGraph graph;