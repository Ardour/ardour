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
	 * @param v true if group should be visible in the UI, otherwise false.
	 */
	PortGroup (std::string const & n, bool v)
		: name (n), _visible (v) {}

	void add_bundle (boost::shared_ptr<ARDOUR::Bundle>);
	void add_port (std::string const &);
	void clear ();

	std::string name; ///< name for the group
	std::vector<boost::shared_ptr<ARDOUR::Bundle> > bundles;
	std::vector<std::string> ports;
	bool visible () const {
		return _visible;
	}

	void set_visible (bool v) {
		_visible = v;
		VisibilityChanged ();
	}

	sigc::signal<void> VisibilityChanged;

private:	
	bool _visible; ///< true if the group is visible in the UI
};

/// The UI for a PortGroup
class PortGroupUI
{
  public:
	PortGroupUI (PortMatrix*, PortGroup*);

	Gtk::Widget& visibility_checkbutton () {
		return _visibility_checkbutton;
	}

  private:
	void visibility_checkbutton_toggled ();
	void setup_visibility_checkbutton ();

	PortMatrix* _port_matrix; ///< the PortMatrix that we are working for
	PortGroup* _port_group; ///< the PortGroup that we are representing
	Gtk::CheckButton _visibility_checkbutton;
};

/// A list of PortGroups
class PortGroupList : public std::list<PortGroup*>, public sigc::trackable
{
  public:
	enum Mask {
		BUSS = 0x1,
		TRACK = 0x2,
		SYSTEM = 0x4,
		OTHER = 0x8
	};

	PortGroupList (ARDOUR::Session &, ARDOUR::DataType, bool, Mask);

	void refresh ();
	void set_type (ARDOUR::DataType);
	void set_offer_inputs (bool);
	std::vector<boost::shared_ptr<ARDOUR::Bundle> > bundles ();
	void take_visibility_from (PortGroupList const &);

	sigc::signal<void> VisibilityChanged;
	
  private:
	void maybe_add_session_bundle (boost::shared_ptr<ARDOUR::Bundle>);
	std::string common_prefix (std::vector<std::string> const &) const;
	void visibility_changed ();
	
	ARDOUR::Session& _session;
	ARDOUR::DataType _type;
	bool _offer_inputs;

	PortGroup _buss;
	PortGroup _track;
	PortGroup _system;
	PortGroup _other;
};

#endif /* __gtk_ardour_port_group_h__ */
