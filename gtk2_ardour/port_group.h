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

#ifndef  __gtk_ardour_port_group_h__
#define  __gtk_ardour_port_group_h__

#include <vector>
#include <string>
#include <set>
#include <gtkmm/widget.h>
#include <gtkmm/checkbutton.h>
#include <boost/shared_ptr.hpp>
#include "ardour/data_type.h"
#include "ardour/types.h"

namespace ARDOUR {
	class Session;
	class Bundle;
	class Processor;
	class IO;
}

class PortMatrix;
class RouteBundle;
class PublicEditor;

/** A list of bundles and ports, grouped by some aspect of their
 *  type e.g. busses, tracks, system.  Each group has 0 or more bundles
 *  and 0 or more ports, where the ports are not in the bundles.
 */
class PortGroup : public sigc::trackable
{
public:
	PortGroup (std::string const & n);

	void add_bundle (boost::shared_ptr<ARDOUR::Bundle>);
	void add_bundle (boost::shared_ptr<ARDOUR::Bundle>, boost::shared_ptr<ARDOUR::IO> io);
	void add_bundle (boost::shared_ptr<ARDOUR::Bundle>, boost::shared_ptr<ARDOUR::IO>, Gdk::Color);
	void remove_bundle (boost::shared_ptr<ARDOUR::Bundle>);
	boost::shared_ptr<ARDOUR::Bundle> only_bundle ();
	void clear ();
	uint32_t total_channels () const;
	boost::shared_ptr<ARDOUR::IO> io_from_bundle (boost::shared_ptr<ARDOUR::Bundle>) const;

	std::string name; ///< name for the group

	bool visible () const {
		return _visible;
	}

	void set_visible (bool v) {
		_visible = v;
		Changed ();
	}

	bool has_port (std::string const &) const;

	sigc::signal<void> Changed;
	sigc::signal<void, ARDOUR::Bundle::Change> BundleChanged;

	struct BundleRecord {
		boost::shared_ptr<ARDOUR::Bundle> bundle;
		boost::shared_ptr<ARDOUR::IO> io;
		Gdk::Color colour;
		bool has_colour;
		sigc::connection changed_connection;
	};

	typedef std::list<BundleRecord> BundleList;

	BundleList const & bundles () const {
		return _bundles;
	}

private:
	void bundle_changed (ARDOUR::Bundle::Change);

	BundleList _bundles;
	bool _visible; ///< true if the group is visible in the UI
};

/// A list of PortGroups
class PortGroupList : public sigc::trackable
{
  public:
	PortGroupList ();

	typedef std::vector<boost::shared_ptr<PortGroup> > List;

	void add_group (boost::shared_ptr<PortGroup>);
	void set_type (ARDOUR::DataType);
	void gather (ARDOUR::Session &, bool);
	PortGroup::BundleList const & bundles () const;
	void clear ();
	void remove_bundle (boost::shared_ptr<ARDOUR::Bundle>);
	uint32_t total_visible_channels () const;
	uint32_t size () const {
		return _groups.size();
	}
	boost::shared_ptr<ARDOUR::IO> io_from_bundle (boost::shared_ptr<ARDOUR::Bundle>) const;

	void suspend_signals ();
	void resume_signals ();

	List::const_iterator begin () const {
		return _groups.begin();
	}

	List::const_iterator end () const {
		return _groups.end();
	}

	sigc::signal<void> Changed;

  private:
	bool port_has_prefix (std::string const &, std::string const &) const;
	std::string common_prefix (std::vector<std::string> const &) const;
	std::string common_prefix_before (std::vector<std::string> const &, std::string const &) const;
	void emit_changed ();
	boost::shared_ptr<ARDOUR::Bundle> make_bundle_from_ports (std::vector<std::string> const &, bool) const;
	void maybe_add_processor_to_bundle (boost::weak_ptr<ARDOUR::Processor>, boost::shared_ptr<RouteBundle>, bool, std::set<boost::shared_ptr<ARDOUR::IO> > &);

	ARDOUR::DataType _type;
	mutable PortGroup::BundleList _bundles;
	List _groups;
	std::vector<sigc::connection> _bundle_changed_connections;
	bool _signals_suspended;
	bool _pending_change;
};


class RouteBundle : public ARDOUR::Bundle
{
public:
	RouteBundle (boost::shared_ptr<ARDOUR::Bundle>);

	void add_processor_bundle (boost::shared_ptr<ARDOUR::Bundle>);

private:
	void reread_component_bundles ();

	boost::shared_ptr<ARDOUR::Bundle> _route;
	std::vector<boost::shared_ptr<ARDOUR::Bundle> > _processor;
};

#endif /* __gtk_ardour_port_group_h__ */
