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

class PortGroup
{
  public:
	PortGroup (std::string const & n, std::string const & p) : name (n), prefix (p) {}

	void add (std::string const & p);

	std::string name;
	std::string prefix;
	std::vector<std::string> ports;
};

class GroupedPortList : public std::list<PortGroup>
{
  public:
	GroupedPortList (ARDOUR::Session &, boost::shared_ptr<ARDOUR::IO>, bool);

	void refresh ();
	int n_ports () const;
	std::string get_port_by_index (int, bool with_prefix = true) const;

  private:
	ARDOUR::Session& _session;
	boost::shared_ptr<ARDOUR::IO> _io;
	bool _for_input;
};


/// A widget which provides a set of rotated text labels
class RotatedLabelSet : public Gtk::Widget {
  public:
	RotatedLabelSet (GroupedPortList&);
	virtual ~RotatedLabelSet ();

	void set_angle (int);
	void set_base_dimensions (int, int);

  protected:
	virtual void on_size_request (Gtk::Requisition*);
	virtual void on_size_allocate (Gtk::Allocation&);
	virtual void on_realize ();
	virtual void on_unrealize ();
	virtual bool on_expose_event (GdkEventExpose*);

	Glib::RefPtr<Gdk::Window> _gdk_window;

  private:
	std::pair<int, int> setup_layout (std::string const &);

	GroupedPortList& _port_list; ///< list of ports to display
	int _angle_degrees; ///< label rotation angle in degrees
	double _angle_radians; ///< label rotation angle in radians
	int _base_start; ///< offset to start of labels; see set_base_dimensions() for more details
	int _base_width; ///< width of labels; see set_base_dimensions() for more details
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

  protected:
	ARDOUR::Session& _session;

  private:
	void setup_table ();
	void setup_row_labels ();
	void setup_check_button_states ();
	void check_button_toggled (int, int);
	void add_port_button_clicked ();
	void remove_port_button_clicked ();
	void set_button_sensitivity ();
	void ports_changed (ARDOUR::IOChange, void *);
	void update_column_label_dimensions ();

	GroupedPortList _port_list;
	boost::shared_ptr<ARDOUR::IO> _io;
	bool _for_input;
	int _width;
	int _height;
	std::vector<Gtk::Label*> _row_labels;
	std::vector<Gtk::EventBox*> _group_labels;
	RotatedLabelSet _column_labels;
	std::vector<std::vector<Gtk::CheckButton*> > _check_buttons;
	bool _ignore_check_button_toggle; ///< check button toggle events are ignored when this is true
	Gtk::Button _add_port_button;
	Gtk::Button _remove_port_button;
	Gtk::VBox _add_remove_box;
	Gtk::HBox _table_hbox;
	Gtk::Label _dummy;
	bool _add_remove_box_added;
	Gtk::Table _table;
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
