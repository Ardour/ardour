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

#ifndef __ardour_ui_io_selector_h__
#define __ardour_ui_io_selector_h__

#include <gtkmm/box.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/table.h>
#include <gtkmm/frame.h>

#include "ardour_dialog.h"

namespace ARDOUR {
	class Session;
	class IO;
	class PortInsert;
}

/// A group of port names
class PortGroup
{
  public:
	PortGroup (std::string const & n, std::string const & p) : name (n), prefix (p), visible (true) {}

	void add (std::string const & p);

	std::string name;
	std::string prefix; ///< prefix (before colon) e.g. "ardour:"
	std::vector<std::string> ports; ///< port names
	bool visible;
};

/// A table of checkbuttons to provide the GUI for connecting to a PortGroup
class PortGroupTable
{
  public:
	PortGroupTable (PortGroup&, boost::shared_ptr<ARDOUR::IO>, bool);

	Gtk::Widget& get_widget ();
	std::pair<int, int> unit_size () const;
	PortGroup& port_group () { return _port_group; }

  private:
	void check_button_toggled (Gtk::CheckButton*, int, std::string const &);
	
	Gtk::Table _table;
	Gtk::EventBox _box;
	PortGroup& _port_group;
	std::vector<std::vector<Gtk::CheckButton* > > _check_buttons;
	bool _ignore_check_button_toggle;
	boost::shared_ptr<ARDOUR::IO> _io;
	bool _for_input;
};

/// A list of PortGroups
class PortGroupList : public std::list<PortGroup>
{
  public:
	PortGroupList (ARDOUR::Session &, boost::shared_ptr<ARDOUR::IO>, bool);

	void refresh ();
	int n_visible_ports () const;
	std::string get_port_by_index (int, bool with_prefix = true) const;

  private:
	ARDOUR::Session& _session;
	boost::shared_ptr<ARDOUR::IO> _io;
	bool _for_input;
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



/// Widget for selecting what an IO is connected to
class IOSelector : public Gtk::VBox {
  public:
	IOSelector (ARDOUR::Session&, boost::shared_ptr<ARDOUR::IO>, bool);
	~IOSelector ();

	void redisplay ();

	enum Result {
		Cancelled,
		Accepted
	};

	sigc::signal<void, Result> Finished;

  private:
	void setup ();
	void clear ();
	void setup_dimensions ();
	void ports_changed (ARDOUR::IOChange, void*);
	bool row_label_button_pressed (GdkEventButton*, int);
	void add_port ();
	void remove_port (int);
	void group_visible_toggled (Gtk::CheckButton*, std::string const &);

	PortGroupList _port_group_list;
	boost::shared_ptr<ARDOUR::IO> _io;
	bool _for_input;
	std::vector<PortGroupTable*> _port_group_tables;
	std::vector<Gtk::EventBox*> _row_labels[2];
	Gtk::VBox* _row_labels_vbox[2];
	RotatedLabelSet _column_labels;
	Gtk::HBox _overall_hbox;
	Gtk::VBox _side_vbox[2];
	Gtk::HBox _port_group_hbox;
	Gtk::ScrolledWindow _scrolled_window;
	Gtk::Label* _side_vbox_pad[2];
};


class IOSelectorWindow : public ArdourDialog
{
  public:
	IOSelectorWindow (ARDOUR::Session&, boost::shared_ptr<ARDOUR::IO>, bool for_input, bool can_cancel = false);
	~IOSelectorWindow ();

	IOSelector& selector() { return _selector; }

  protected:
	void on_map ();
	
  private:
	IOSelector _selector;

	/* overall operation buttons */

	Gtk::Button ok_button;
	Gtk::Button cancel_button;
	Gtk::Button rescan_button;

	void rescan ();
	void cancel ();
	void accept ();
};


class PortInsertUI : public Gtk::VBox
{
  public: 
	PortInsertUI (ARDOUR::Session&, boost::shared_ptr<ARDOUR::PortInsert>);
	
	void redisplay ();
	void finished (IOSelector::Result);

  private:
	Gtk::HBox hbox;
	IOSelector input_selector;
	IOSelector output_selector;
};

class PortInsertWindow : public ArdourDialog
{
  public: 
	PortInsertWindow (ARDOUR::Session&, boost::shared_ptr<ARDOUR::PortInsert>, bool can_cancel = false);
	
  protected:
	void on_map ();
	
  private:
	PortInsertUI _portinsertui;
	Gtk::VBox vbox;
	
	Gtk::Button ok_button;
	Gtk::Button cancel_button;
	Gtk::Button rescan_button;
	Gtk::Frame button_frame;
	
	void rescan ();
	void cancel ();
	void accept ();

	void plugin_going_away ();
	sigc::connection going_away_connection;
};


#endif
