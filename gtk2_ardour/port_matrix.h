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

#ifndef __gtk_ardour_port_matrix_h__
#define __gtk_ardour_port_matrix_h__

#include <list>
#include <gtkmm/box.h>
#include <gtkmm/scrollbar.h>
#include <boost/shared_ptr.hpp>
#include "port_matrix_body.h"
#include "port_group.h"

/** The `port matrix' UI.  This is a widget which lets the user alter
 *  associations between one set of ports and another.  e.g. to connect
 *  things together.
 *
 *  The columns are labelled with various ports from around Ardour and the
 *  system.
 *
 *  It is made up of a body, PortMatrixBody, which is rendered using cairo,
 *  and some scrollbars.  All of this is arranged inside the VBox that we
 *  inherit from.
 */

namespace ARDOUR {
	class Bundle;
}

class PortMatrix : public Gtk::VBox
{
public:
	PortMatrix (ARDOUR::Session&, ARDOUR::DataType, bool, PortGroupList::Mask);
	~PortMatrix ();

	virtual void setup ();
	void set_offer_inputs (bool);
	void set_type (ARDOUR::DataType);
	bool offering_input () const { return _offer_inputs; }
	void disassociate_all ();

	enum Result {
		Cancelled,
		Accepted
	};

	sigc::signal<void, Result> Finished;

	/** @param ab Our bundle.
	 *  @param ac Channel on our bundle.
	 *  @param bb Other bundle.
	 *  @arapm bc Channel on other bundle.
	 *  @param s New state.
	 *  @param k XXX
	 */
	virtual void set_state (
		boost::shared_ptr<ARDOUR::Bundle> ab,
		uint32_t ac,
		boost::shared_ptr<ARDOUR::Bundle> bb,
		uint32_t bc,
		bool s,
		uint32_t k
		) = 0;

	enum State {
		ASSOCIATED,
		NOT_ASSOCIATED,
		UNKNOWN
	};

	/** @param ab Our bundle.
	 *  @param ac Channel on our bundle.
	 *  @param bb Other bundle.
	 *  @arapm bc Channel on other bundle.
	 *  @return state
	 */
	virtual State get_state (
		boost::shared_ptr<ARDOUR::Bundle> ab,
		uint32_t ac,
		boost::shared_ptr<ARDOUR::Bundle> bb,
		uint32_t bc
		) const = 0;

	virtual void add_channel (boost::shared_ptr<ARDOUR::Bundle>) = 0;
	virtual void remove_channel (boost::shared_ptr<ARDOUR::Bundle>, uint32_t) = 0;
	virtual bool can_rename_channels () const = 0;
	virtual void rename_channel (boost::shared_ptr<ARDOUR::Bundle>, uint32_t) {}
	
	void setup_scrollbars ();

protected:

	std::vector<boost::shared_ptr<ARDOUR::Bundle> > _our_bundles;
	/// list of port groups
	PortGroupList _port_group_list;
	
private:

	void hscroll_changed ();
	void vscroll_changed ();
	
	/// true to offer inputs, otherwise false
	bool _offer_inputs;
	/// port type that we are working with
	ARDOUR::DataType _type;

	PortMatrixBody _body;
	Gtk::HScrollbar _hscroll;
	Gtk::VScrollbar _vscroll;
	std::list<PortGroupUI*> _port_group_uis;
};

#endif
