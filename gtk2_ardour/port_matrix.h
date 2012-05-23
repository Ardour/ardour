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
#include <gtkmm/table.h>
#include <gtkmm/label.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/notebook.h>
#include <boost/shared_ptr.hpp>

#include "ardour/bundle.h"
#include "ardour/types.h"
#include "ardour/session_handle.h"

#include "port_group.h"
#include "port_matrix_types.h"

/** The `port matrix' UI.  This is a widget which lets the user alter
 *  associations between one set of ports and another.  e.g. to connect
 *  things together.
 *
 *  It is made up of a body, PortMatrixBody, which is rendered using cairo,
 *  and some scrollbars and other stuff.  All of this is arranged inside the
 *  Table that we inherit from.
 */

namespace ARDOUR {
	class Bundle;
}

namespace Gtk {
	namespace Menu_Helpers {
		class MenuList;
	}
}

class PortMatrixBody;

class PortMatrix : public Gtk::Table, public ARDOUR::SessionHandlePtr
{
public:
	PortMatrix (Gtk::Window*, ARDOUR::Session *, ARDOUR::DataType);
	~PortMatrix ();

	void set_type (ARDOUR::DataType);

	ARDOUR::DataType type () const {
		return _type;
	}

	void disassociate_all ();
	void setup_scrollbars ();
	void popup_menu (ARDOUR::BundleChannel, ARDOUR::BundleChannel, uint32_t);

	int min_height_divisor () const {
		return _min_height_divisor;
	}
	void set_min_height_divisor (int f) {
		_min_height_divisor = f;
	}

	enum Arrangement {
		TOP_TO_RIGHT,  ///< column labels on top, row labels to the right
		LEFT_TO_BOTTOM ///< row labels to the left, column labels on the bottom
	};


	/** @return Arrangement in use */
	Arrangement arrangement () const {
		return _arrangement;
	}

	bool show_only_bundles () const {
		return _show_only_bundles;
	}

	PortGroupList const * columns () const;
	boost::shared_ptr<const PortGroup> visible_columns () const;

	/** @return index into the _ports array for the list which is displayed as columns */
	int column_index () const {
		return _column_index;
	}

	PortGroupList const * rows () const;
	boost::shared_ptr<const PortGroup> visible_rows () const;

	/** @return index into the _ports array for the list which is displayed as rows */
	int row_index () const {
		return _row_index;
	}

	PortGroupList const * ports (int d) const {
		return &_ports[d];
	}

	boost::shared_ptr<const PortGroup> visible_ports (int d) const;

	void init ();
	void setup ();
	virtual void setup_ports (int) = 0;
	void setup_all_ports ();

	std::pair<uint32_t, uint32_t> max_size () const;

	bool should_show (ARDOUR::DataType) const;
	uint32_t count_of_our_type (ARDOUR::ChanCount) const;
	uint32_t count_of_our_type_min_1 (ARDOUR::ChanCount) const;

	PortMatrixNode::State get_association (PortMatrixNode) const;

	/** @param c Channels; where c[0] is from _ports[0] and c[1] is from _ports[1].
	 *  @param s New state.
	 */
	virtual void set_state (ARDOUR::BundleChannel c[2], bool s) = 0;

	/** @param c Channels; where c[0] is from _ports[0] and c[1] is from _ports[1].
	 *  @return state
	 */
	virtual PortMatrixNode::State get_state (ARDOUR::BundleChannel c[2]) const = 0;
	virtual bool list_is_global (int) const = 0;

	virtual bool can_add_channels (boost::shared_ptr<ARDOUR::Bundle>) const;
	virtual void add_channel (boost::shared_ptr<ARDOUR::Bundle>, ARDOUR::DataType);
	virtual bool can_remove_channels (boost::shared_ptr<ARDOUR::Bundle>) const;
	virtual void remove_channel (ARDOUR::BundleChannel);
	virtual void remove_all_channels (boost::weak_ptr<ARDOUR::Bundle>);
	virtual bool can_rename_channels (boost::shared_ptr<ARDOUR::Bundle>) const {
		return false;
	}
	virtual void rename_channel (ARDOUR::BundleChannel) {}
	virtual std::string disassociation_verb () const = 0;
	virtual std::string channel_noun () const;

	enum Result {
		Cancelled,
		Accepted
	};

	sigc::signal<void, Result> Finished;

	static bool bundle_with_channels (boost::shared_ptr<ARDOUR::Bundle>);

protected:

	/** We have two port group lists.  One will be presented on the rows of the matrix,
	    the other on the columns.  The PortMatrix chooses the arrangement based on which has
	    more ports in it.  Subclasses must fill these two lists with the port groups that they
	    wish to present.  The PortMatrix will arrange its layout such that signal flow is vaguely
	    from left to right as you go from list 0 to list 1.  Hence subclasses which deal with
	    inputs and outputs should put outputs in list 0 and inputs in list 1. */
	PortGroupList _ports[2];

private:

	void hscroll_changed ();
	void vscroll_changed ();
	void routes_changed ();
	void reconnect_to_routes ();
	void select_arrangement ();
	void add_channel_proxy (boost::weak_ptr<ARDOUR::Bundle>, ARDOUR::DataType);
	void remove_channel_proxy (boost::weak_ptr<ARDOUR::Bundle>, uint32_t);
	void rename_channel_proxy (boost::weak_ptr<ARDOUR::Bundle>, uint32_t);
	void disassociate_all_on_channel (boost::weak_ptr<ARDOUR::Bundle>, uint32_t, int);
	void disassociate_all_on_bundle (boost::weak_ptr<ARDOUR::Bundle>, int);
	void setup_global_ports ();
	void setup_global_ports_proxy ();
	void toggle_show_only_bundles ();
	bool on_scroll_event (GdkEventScroll *);
	boost::shared_ptr<ARDOUR::IO> io_from_bundle (boost::shared_ptr<ARDOUR::Bundle>) const;
	void setup_notebooks ();
	void remove_notebook_pages (Gtk::Notebook &);
	void notebook_page_selected (GtkNotebookPage *, guint);
	void route_processors_changed (ARDOUR::RouteProcessorChange);
	void body_dimensions_changed ();
	void session_going_away ();
	void add_remove_option (Gtk::Menu_Helpers::MenuList &, boost::weak_ptr<ARDOUR::Bundle>, int);
	void add_disassociate_option (Gtk::Menu_Helpers::MenuList &, boost::weak_ptr<ARDOUR::Bundle>, int, int);
	void port_connected_or_disconnected ();
	void update_tab_highlighting ();

	Gtk::Window* _parent;

	/// port type that we are working with, or NIL if we are working with all of them
	ARDOUR::DataType _type;
	PBD::ScopedConnectionList _route_connections;
	PBD::ScopedConnectionList _changed_connections;
	PBD::ScopedConnectionList _bundle_changed_connections;

	PortMatrixBody* _body;
	Gtk::HScrollbar _hscroll;
	Gtk::VScrollbar _vscroll;
	Gtk::Notebook _vnotebook;
	Gtk::Notebook _hnotebook;
	Gtk::Label _vlabel;
	Gtk::Label _hlabel;
	Gtk::VBox _vbox;
	Gtk::HBox _hbox;
	Gtk::Label _hspacer;
	Gtk::Label _vspacer;
	Gtk::Menu* _menu;
	Arrangement _arrangement;
	int _row_index;
	int _column_index;
	int _min_height_divisor;
	bool _show_only_bundles;
	bool _inhibit_toggle_show_only_bundles;
	bool _ignore_notebook_page_selected;
};

#endif
