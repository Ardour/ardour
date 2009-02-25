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
#include <gtkmm/widget.h>
#include <gtkmm/checkbutton.h>
#include <boost/shared_ptr.hpp>
#include "ardour/data_type.h"
#include "ardour/types.h"

namespace ARDOUR {
	class Session;
	class Bundle;
}

class PortMatrix;

/** A list of bundles and ports, grouped by some aspect of their
 *  type e.g. busses, tracks, system.  Each group has 0 or more bundles
 *  and 0 or more ports, where the ports are not in the bundles.
 */
class PortGroup : public sigc::trackable
{
public:
	PortGroup (std::string const & n);

	void add_bundle (boost::shared_ptr<ARDOUR::Bundle>);
	void remove_bundle (boost::shared_ptr<ARDOUR::Bundle>);
	boost::shared_ptr<ARDOUR::Bundle> only_bundle ();
	void clear ();
	uint32_t total_channels () const;

	std::string name; ///< name for the group

	ARDOUR::BundleList const & bundles () const {
		return _bundles;
	}
	
	bool visible () const {
		return _visible;
	}

	void set_visible (bool v) {
		_visible = v;
		Modified ();
	}

	bool has_port (std::string const &) const;

	sigc::signal<void> Modified;
	sigc::signal<void, ARDOUR::Bundle::Change> BundleChanged;

private:
	void bundle_changed (ARDOUR::Bundle::Change);
	
	ARDOUR::BundleList _bundles;

	typedef std::map<boost::shared_ptr<ARDOUR::Bundle>, sigc::connection> ConnectionList;
	ConnectionList _bundle_changed_connections;
	
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
	ARDOUR::BundleList const & bundles () const;
	void clear ();
	void remove_bundle (boost::shared_ptr<ARDOUR::Bundle>);
	uint32_t total_visible_channels () const;
	uint32_t size () const {
		return _groups.size();
	}

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
	
	ARDOUR::DataType _type;
	mutable ARDOUR::BundleList _bundles;
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
