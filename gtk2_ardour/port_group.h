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
#include <ardour/data_type.h>
#include <ardour/types.h>

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
	/** PortGroup constructor.
	 * @param n Name.
	 */
	PortGroup (std::string const & n)
		: name (n), _visible (true) {}

	void add_bundle (boost::shared_ptr<ARDOUR::Bundle>);
	boost::shared_ptr<ARDOUR::Bundle> only_bundle ();
	void add_port (std::string const &);
	void clear ();
	uint32_t total_ports () const;

	std::string name; ///< name for the group
	std::vector<std::string> ports;

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

private:	
	ARDOUR::BundleList _bundles;
	bool _visible; ///< true if the group is visible in the UI
};

/// A list of PortGroups
class PortGroupList
{
  public:
	PortGroupList ();

	typedef std::vector<boost::shared_ptr<PortGroup> > List;

	void add_group (boost::shared_ptr<PortGroup>);
	void set_type (ARDOUR::DataType);
	void gather (ARDOUR::Session &, bool);
	void set_offer_inputs (bool);
	ARDOUR::BundleList const & bundles () const;
	void clear ();
	uint32_t total_visible_ports () const;
	uint32_t size () const {
		return _groups.size();
	}

	List::const_iterator begin () const {
		return _groups.begin();
	}

	List::const_iterator end () const {
		return _groups.end();
	}
	
  private:
	bool port_has_prefix (std::string const &, std::string const &) const;
	std::string common_prefix (std::vector<std::string> const &) const;
	void update_bundles () const;
	void group_modified ();
	
	ARDOUR::DataType _type;
	bool _offer_inputs;
	mutable ARDOUR::BundleList _bundles;
	mutable bool _bundles_dirty;
	List _groups;
};

#endif /* __gtk_ardour_port_group_h__ */
