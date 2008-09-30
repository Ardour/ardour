/*
    Copyright (C) 2002-2007 Paul Davis 

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

#ifndef __ardour_ui_port_matrix_h__
#define __ardour_ui_port_matrix_h__

#include <gtkmm/box.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/table.h>
#include <gtkmm/frame.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/scrolledwindow.h>

#include "ardour_dialog.h"

namespace ARDOUR {
	class Session;
	class IO;
	class PortInsert;
}

class PortMatrix;

/// A list of port names, grouped by some aspect of their type e.g. busses, tracks, system
class PortGroup
{
  public:
	/** PortGroup constructor.
	 * @param n Name.
	 * @param p Port name prefix.
	 * @param v true if group should be visible in the UI, otherwise false.
	 */
	PortGroup (std::string const & n, std::string const & p, bool v) : name (n), prefix (p), visible (v) {}

	void add (std::string const & p);

	std::string name; ///< name for the group
	std::string prefix; ///< prefix (before colon) e.g. "ardour:"
	std::vector<std::string> ports; ///< port names
	bool visible; ///< true if the group is visible in the UI
};

/// The UI for a PortGroup
class PortGroupUI
{
  public:
	PortGroupUI (PortMatrix&, PortGroup&);

	Gtk::Widget& get_table ();
	Gtk::Widget& get_visibility_checkbutton ();
	std::pair<int, int> unit_size () const;
	PortGroup& port_group () { return _port_group; }
	void setup_visibility ();

  private:
	void port_checkbutton_toggled (Gtk::CheckButton*, int, int);
	void visibility_checkbutton_toggled ();

	PortMatrix& _port_matrix; ///< the PortMatrix that we are working for
	PortGroup& _port_group; ///< the PortGroup that we are representing
	bool _ignore_check_button_toggle;
	Gtk::Table _table;
	Gtk::EventBox _table_box;
	std::vector<std::vector<Gtk::CheckButton* > > _port_checkbuttons;
	Gtk::CheckButton _visibility_checkbutton;
};

/// A list of PortGroups
class PortGroupList : public std::list<PortGroup*>
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
	int n_visible_ports () const;
	std::string get_port_by_index (int, bool with_prefix = true) const;
	void set_type (ARDOUR::DataType);
	void set_offer_inputs (bool);
	
  private:
	ARDOUR::Session& _session;
	ARDOUR::DataType _type;
	bool _offer_inputs;

	PortGroup buss;
	PortGroup track;
	PortGroup system;
	PortGroup other;
};


/// A widget which provides a set of rotated text labels
class RotatedLabelSet : public Gtk::Widget {
  public:
	RotatedLabelSet (PortGroupList&);
	virtual ~RotatedLabelSet ();

	void set_angle (int);
	void set_base_width (int);
	void update_visibility ();
	
  protected:
	virtual void on_size_request (Gtk::Requisition*);
	virtual void on_size_allocate (Gtk::Allocation&);
	virtual void on_realize ();
	virtual void on_unrealize ();
	virtual bool on_expose_event (GdkEventExpose*);

	Glib::RefPtr<Gdk::Window> _gdk_window;

  private:
	std::pair<int, int> setup_layout (std::string const &);

	PortGroupList& _port_group_list; ///< list of ports to display
	int _angle_degrees; ///< label rotation angle in degrees
	double _angle_radians; ///< label rotation angle in radians
	int _base_width; ///< width of labels; see set_base_width() for more details
	Glib::RefPtr<Pango::Context> _pango_context;
	Glib::RefPtr<Pango::Layout> _pango_layout;
	Glib::RefPtr<Gdk::GC> _gc;
	Gdk::Color _fg_colour;
	Gdk::Color _bg_colour;
};


class PortMatrix : public Gtk::VBox {
  public:
	PortMatrix (ARDOUR::Session&, ARDOUR::DataType, bool, PortGroupList::Mask);
	~PortMatrix ();

	void redisplay ();

	enum Result {
		Cancelled,
		Accepted
	};

	sigc::signal<void, Result> Finished;

	void set_type (ARDOUR::DataType);
	void set_offer_inputs (bool);
	bool offering_input() const { return _offer_inputs; }

	virtual void set_state (int, std::string const &, bool) = 0;
	virtual bool get_state (int, std::string const &) const = 0;
	virtual uint32_t n_rows () const = 0;
	virtual uint32_t maximum_rows () const = 0;
	virtual uint32_t minimum_rows () const = 0;
	virtual std::string row_name (int) const = 0;
	virtual void add_row () = 0;
	virtual void remove_row (int) = 0;
	virtual std::string row_descriptor () const = 0;

  protected:

	bool _offer_inputs;

  private:
	PortGroupList _port_group_list;
	ARDOUR::DataType _type;
	std::vector<PortGroupUI*> _port_group_ui;
	std::vector<Gtk::EventBox*> _row_labels[2];
	Gtk::VBox* _row_labels_vbox[2];
	RotatedLabelSet _column_labels;
	Gtk::HBox _overall_hbox;
	Gtk::VBox _side_vbox[2];
	Gtk::HBox _port_group_hbox;
	Gtk::ScrolledWindow _scrolled_window;
	Gtk::Label* _side_vbox_pad[2];
	Gtk::HBox _visibility_checkbutton_box;

	void setup ();
	void clear ();
	void setup_dimensions ();
	bool row_label_button_pressed (GdkEventButton*, int);
	void reset_visibility ();
};

#endif
