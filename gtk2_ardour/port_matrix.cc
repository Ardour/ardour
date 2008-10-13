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

#include <gtkmm/label.h>
#include <gtkmm/enums.h>
#include <gtkmm/image.h>
#include <gtkmm/stock.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/menu.h>
#include <gtkmm/menu_elems.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/menushell.h>
#include <glibmm/objectbase.h>
#include <gtkmm2ext/doi.h>
#include <ardour/port_insert.h>
#include "ardour/session.h"
#include "ardour/io.h"
#include "ardour/audioengine.h"
#include "ardour/track.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"
#include "ardour/data_type.h"
#include "io_selector.h"
#include "keyboard.h"
#include "utils.h"
#include "gui_thread.h"
#include "i18n.h"

using namespace Gtk;

/** Add a port to a group.
 *  @param p Port name, with or without prefix.
 */

void
PortGroup::add (std::string const & p)
{
	if (prefix.empty() == false && p.substr (0, prefix.length()) == prefix) {
		ports.push_back (p.substr (prefix.length()));
	} else {
		ports.push_back (p);
	}
}

/** PortGroupUI constructor.
 *  @param m PortMatrix to work for.
 *  @Param g PortGroup to represent.
 */

PortGroupUI::PortGroupUI (PortMatrix& m, PortGroup& g)
	: _port_matrix (m)
	, _port_group (g)
	, _ignore_check_button_toggle (false)
	, _visibility_checkbutton (g.name)
{
	int const ports = _port_group.ports.size();
	int const rows = _port_matrix.n_rows ();

	if (rows == 0 || ports == 0) {
		return;
	}

	/* Sort out the table and the checkbuttons inside it */
	
	_table.resize (rows, ports);
	_port_checkbuttons.resize (rows);
	for (int i = 0; i < rows; ++i) {
		_port_checkbuttons[i].resize (ports);
	}


	for (int i = 0; i < rows; ++i) {
		for (uint32_t j = 0; j < _port_group.ports.size(); ++j) {
			CheckButton* b = new CheckButton;
			
			b->signal_toggled().connect (
				sigc::bind (sigc::mem_fun (*this, &PortGroupUI::port_checkbutton_toggled), b, i, j));
			
			b->signal_button_release_event().connect (
				sigc::bind (sigc::mem_fun (*this, &PortGroupUI::port_checkbutton_release), b, i, j), false);

			_port_checkbuttons[i][j] = b;

			cerr << this << " bind to " << &_port_checkbuttons << " via " << b 
			     << endl;

			_table.attach (*b, j, j + 1, i, i + 1);
		}
	}
	
	_table_box.add (_table);

 	_ignore_check_button_toggle = true;

 	/* Set the state of the check boxes according to current connections */
 	for (int i = 0; i < rows; ++i) {
 		for (uint32_t j = 0; j < _port_group.ports.size(); ++j) {
 			std::string const t = _port_group.prefix + _port_group.ports[j];
			bool const s = _port_matrix.get_state (i, t);
 			_port_checkbuttons[i][j]->set_active (s);
			if (s) {
				_port_group.visible = true;
			}
 		}
 	}

 	_ignore_check_button_toggle = false;

	_visibility_checkbutton.signal_toggled().connect (sigc::mem_fun (*this, &PortGroupUI::visibility_checkbutton_toggled));
}

/** The visibility of a PortGroupUI has been toggled */
void
PortGroupUI::visibility_checkbutton_toggled ()
{
	_port_group.visible = _visibility_checkbutton.get_active ();
}

/** @return Width and height of a single checkbutton in a port group table */
std::pair<int, int>
PortGroupUI::unit_size () const
{
	if (_port_checkbuttons.empty() || _port_checkbuttons[0].empty())
	{
		return std::pair<int, int> (0, 0);
	}

	int r = 0;
	/* We can't ask for row spacing unless there >1 rows, otherwise we get a warning */
	if (_table.property_n_rows() > 1) {
		r = _table.get_row_spacing (0);
	}

	return std::make_pair (
		_port_checkbuttons[0][0]->get_width() + _table.get_col_spacing (0),
		_port_checkbuttons[0][0]->get_height() + r
		);
}

/** @return Table widget containing the port checkbuttons */
Widget&
PortGroupUI::get_table ()
{
	return _table_box;
}

/** @return Checkbutton used to toggle visibility */
Widget&
PortGroupUI::get_visibility_checkbutton ()
{
	return _visibility_checkbutton;
}


/** Handle a toggle of a port check button */
void
PortGroupUI::port_checkbutton_toggled (CheckButton* b, int r, int c)
{
 	if (_ignore_check_button_toggle == false) {
		_port_matrix.set_state (r, _port_group.prefix + _port_group.ports[c], b->get_active(), 0);
	}
}

bool
PortGroupUI::port_checkbutton_release (GdkEventButton* ev, CheckButton* b, int r, int c)
{
	cerr << this << " RELEASE on " << b << " state = " << ev->state << endl;

	if (ev->state == 0) {
		/* let usual toggle handler take care of it */
		return false;
	}

	/* The fun starts here 
	 */

	const size_t ports = _port_group.ports.size();
	const size_t rows = _port_matrix.n_rows ();

	if (rows == 0 || ports == 0) {
		return true;
	}

	/* For each port in the group, change the state of
	   the connection for the corresponding "connector" port.
	*/
	
	for (size_t j = c; j < ports; ++j) {

		/* we've got a port to connect, now lets find the thing to 
		   connect it too ... (search "down" the rows)
		*/

		cerr << "we're going to connect port " << j << " of " << ports << endl;

		for (size_t i = r; i < rows; ++i) {

			cerr << this << " going to connect to row " << i << " of " << rows << endl;
			cerr << "access [" << i << "][" << j << "]\n";
			cerr << " @ " << &_port_checkbuttons << endl;

			_port_checkbuttons[i][j]->set_active (!_port_checkbuttons[i][j]->get_active());

			/* next time, get at least as far as this port before looking
			   for more.
			*/

			r = i + 1;

			/* changed connection state, stop looking for more */

			break;
		}
	}

	return true;
}

/** Set up visibility of the port group according to PortGroup::visible */
void
PortGroupUI::setup_visibility ()
{
	if (!_port_group.ports.empty() && _port_group.visible) {
		_table_box.show ();
	} else {
		_table_box.hide ();
	}

	if (_visibility_checkbutton.get_active () != _port_group.visible) {

		_visibility_checkbutton.set_active (_port_group.visible);
	}
}

RotatedLabelSet::RotatedLabelSet (PortGroupList& g)
	: Glib::ObjectBase ("RotatedLabelSet"), Widget (), _port_group_list (g), _base_width (128)
{
	set_flags (NO_WINDOW);
	if (getenv ("AD_ANGLE") != 0) {
		set_angle (atoi (getenv ("AD_ANGLE")));
	} else {
		set_angle (45);
	}
}

RotatedLabelSet::~RotatedLabelSet ()
{
	
}


/** Set the angle that the labels are drawn at.
 * @param degrees New angle in degrees.
 */

void
RotatedLabelSet::set_angle (int degrees)
{
	_angle_degrees = degrees;
	_angle_radians = M_PI * _angle_degrees / 180;

	queue_resize ();
}

void
RotatedLabelSet::on_size_request (Requisition* requisition)
{
	*requisition = Requisition ();

	if (_pango_layout == 0) {
		return;
	}

	/* Our height is the highest label */
	requisition->height = 0;
	for (PortGroupList::const_iterator i = _port_group_list.begin(); i != _port_group_list.end(); ++i) {
		for (std::vector<std::string>::const_iterator j = (*i)->ports.begin(); j != (*i)->ports.end(); ++j) {
			std::pair<int, int> const d = setup_layout (*j);
			if (d.second > requisition->height) {
				requisition->height = d.second;
			}
		}
	}

	/* And our width is the base plus the width of the last label */
	requisition->width = _base_width;
	int const n = _port_group_list.n_visible_ports ();
	if (n > 0) {
		std::pair<int, int> const d = setup_layout (_port_group_list.get_port_by_index (n - 1, false));
		requisition->width += d.first;
	}

	cerr << "Labels will be " << requisition->width << " x " << requisition->height << endl;
}

void
RotatedLabelSet::on_size_allocate (Allocation& allocation)
{
	set_allocation (allocation);

	if (_gdk_window) {
		_gdk_window->move_resize (allocation.get_x(), allocation.get_y(), allocation.get_width(), allocation.get_height());
	}
}

void
RotatedLabelSet::on_realize ()
{
	Widget::on_realize ();

	Glib::RefPtr<Style> style = get_style ();

	if (!_gdk_window) {
		GdkWindowAttr attributes;
		memset (&attributes, 0, sizeof (attributes));

		Allocation allocation = get_allocation ();
		attributes.x = allocation.get_x ();
		attributes.y = allocation.get_y ();
		attributes.width = allocation.get_width ();
		attributes.height = allocation.get_height ();

		attributes.event_mask = get_events () | Gdk::EXPOSURE_MASK; 
		attributes.window_type = GDK_WINDOW_CHILD;
		attributes.wclass = GDK_INPUT_OUTPUT;

		_gdk_window = Gdk::Window::create (get_window (), &attributes, GDK_WA_X | GDK_WA_Y);
		unset_flags (NO_WINDOW);
		set_window (_gdk_window);

		_bg_colour = style->get_bg (STATE_NORMAL );
		modify_bg (STATE_NORMAL, _bg_colour);
		_fg_colour = style->get_fg (STATE_NORMAL);

		_gdk_window->set_user_data (gobj ());

		/* Set up Pango stuff */
		_pango_context = create_pango_context ();

		Pango::Matrix matrix = PANGO_MATRIX_INIT;
		pango_matrix_rotate (&matrix, _angle_degrees);
		_pango_context->set_matrix (matrix);

		_pango_layout = Pango::Layout::create (_pango_context);
		_gc = Gdk::GC::create (get_window ());
	}
}

void
RotatedLabelSet::on_unrealize()
{
	_gdk_window.clear ();

	Widget::on_unrealize ();
}


/** Set up our Pango layout to plot a given string, and compute its dimensions once
 *  it has been rotated.
 *  @param s String to use.
 *  @return width and height of the rotated string, in pixels.
 */

std::pair<int, int>
RotatedLabelSet::setup_layout (std::string const & s)
{
	_pango_layout->set_text (s);

	/* Here's the unrotated size */
	int w;
	int h;
	_pango_layout->get_pixel_size (w, h);

	/* Rotate the width and height as appropriate.  I thought Pango might be able
	   to do this for us, but I can't find out how... 
	*/

	std::pair<int, int> d;

	// cerr << "\"" << s << "\" was " << w << " x " << h << endl;

	d.first = int (fabs (w * cos (_angle_radians) + h * sin (_angle_radians)));
	d.second = int (fabs (w * sin (_angle_radians) + h * cos (_angle_radians)));

	// cerr << "\trotated by " << _angle_degrees << " = " << d.first << " x " << d.second << endl;

	return d;
}

bool
RotatedLabelSet::on_expose_event (GdkEventExpose* event)
{
	if (!_gdk_window) {
		return true;
	}

	int const height = get_allocation().get_height ();
	double const spacing = double (_base_width) / _port_group_list.n_visible_ports();

	/* Plot all the visible labels; really we should clip for efficiency */
	int n = 0;
	for (PortGroupList::const_iterator i = _port_group_list.begin(); i != _port_group_list.end(); ++i) {
		if ((*i)->visible) {
			for (uint32_t j = 0; j < (*i)->ports.size(); ++j) {
				std::pair<int, int> const d = setup_layout ((*i)->ports[j]);
				int x, y;
				if (getenv ("AD_X_SHIFT") != 0) {
					x = atoi (getenv ("AD_X_SHIFT"));
				} else {
					x = 0;
				}
				if (getenv ("AD_Y_SHIFT") != 0) {
					y = atoi (getenv ("AD_Y_SHIFT"));
				} else {
					y = 0;
				}
				get_window()->draw_layout (_gc, int ((n + 0.25) * spacing) + x, height - d.second + y, _pango_layout, _fg_colour, _bg_colour);
				++n;
			}
		}
	}

	return true;
}

/** Set the `base width'.  This is the width of the base of the label set, ie:
 *
 *     L L L L
 *    E E E E
 *   B B B B
 *  A A A A
 * L L L L
 * <--w-->
 */
    
void
RotatedLabelSet::set_base_width (int w)
{
	_base_width = w;
	queue_resize ();
}


PortMatrix::PortMatrix (ARDOUR::Session& session, ARDOUR::DataType type, bool offer_inputs, PortGroupList::Mask mask)
	: _offer_inputs (offer_inputs), _port_group_list (session, type, offer_inputs, mask), _type (type),
	  _column_labels (_port_group_list)
{
	_row_labels_vbox = 0;
	_side_vbox_pad = 0;

	_visibility_checkbutton_box.pack_start (*(manage (new Label (_("Connections displayed: ")))), false, false, 10);
 	pack_start (_visibility_checkbutton_box, false, false);
	
	_scrolled_window.set_policy (POLICY_ALWAYS, POLICY_AUTOMATIC);
	_scrolled_window.set_shadow_type (SHADOW_NONE);

	VBox* b = manage (new VBox);

	if (offer_inputs) {
		b->pack_start (_port_group_hbox, false, false);
		b->pack_start (_column_labels, false, false);
	} else {
		b->pack_start (_column_labels, false, false);
		b->pack_start (_port_group_hbox, false, false);
	}

	Alignment* a; 

	if (offer_inputs) {
		a = manage (new Alignment (1, 0, 0, 0));
	} else {
		a = manage (new Alignment (0, 1, 0, 0));
	}

	a->add (*b);

	_scrolled_window.add (*a);

	if (offer_inputs) {
		_overall_hbox.pack_start (_side_vbox, false, false, 6);
		_overall_hbox.pack_start (_scrolled_window, true, true);
	} else {
		_overall_hbox.pack_start (_scrolled_window, true, true, 6);
		_overall_hbox.pack_start (_side_vbox, false, false);
	}

	pack_start (_overall_hbox);

	_port_group_hbox.signal_size_allocate().connect (sigc::hide (sigc::mem_fun (*this, &IOSelector::setup_dimensions)));
}

PortMatrix::~PortMatrix ()
{
	clear ();
}

/** Clear out the things that change when the number of source or destination ports changes */
void
PortMatrix::clear ()
{
	for (std::vector<EventBox*>::iterator j = _row_labels.begin(); j != _row_labels.end(); ++j) {
		delete *j;
	}
	_row_labels.clear ();
		
	if (_row_labels_vbox) {
		_side_vbox.remove (*_row_labels_vbox);
		delete _row_labels_vbox;
		_row_labels_vbox = 0;
	}
	
	/* remove lurking, invisible label and padding */
	
	_side_vbox.children().clear ();

	if (_side_vbox_pad) {
		delete _side_vbox_pad;
		_side_vbox_pad = 0;
	}

	for (std::vector<PortGroupUI*>::iterator i = _port_group_ui.begin(); i != _port_group_ui.end(); ++i) {
		_port_group_hbox.remove ((*i)->get_table());
		_visibility_checkbutton_box.remove ((*i)->get_visibility_checkbutton());
		delete *i;
	}

	_port_group_ui.clear ();
}


/** Set up dimensions of some of our widgets which depend on other dimensions
 *  within the dialogue.
 */
void
PortMatrix::setup_dimensions ()
{
	/* Get some dimensions from various places */
	int const scrollbar_height = _scrolled_window.get_hscrollbar()->get_height();

	std::pair<int, int> unit_size (0, 0);
	int port_group_tables_height = 0;
	for (std::vector<PortGroupUI*>::iterator i = _port_group_ui.begin(); i != _port_group_ui.end(); ++i) {
		std::pair<int, int> const u = (*i)->unit_size ();
		unit_size.first = std::max (unit_size.first, u.first);
		unit_size.second = std::max (unit_size.second, u.second);
		port_group_tables_height = std::max (
			port_group_tables_height, (*i)->get_table().get_height()
			);
	}

	/* Column labels */
	_column_labels.set_base_width (_port_group_list.n_visible_ports () * unit_size.first);

	/* Scrolled window */
	/* XXX: really shouldn't set a minimum horizontal size here, but if we don't
	   the window starts up very small.
	   The constant value in the set_size_request() computation will control
	   how big the scrolled window will be if we fill the port matrix will a gajillion
	   ports.
	*/

	_scrolled_window.set_size_request (
		std::min (_column_labels.get_width(), 400),
		_column_labels.get_height() + port_group_tables_height + scrollbar_height + 16
		);
	
	/* Row labels */
	for (std::vector<EventBox*>::iterator j = _row_labels.begin(); j != _row_labels.end(); ++j) {
		(*j)->get_child()->set_size_request (-1, unit_size.second);
	}
	
	if (_side_vbox_pad) {
		if (_offer_inputs) {
			_side_vbox_pad->set_size_request (-1, unit_size.second / 4);
		} else {
			_side_vbox_pad->set_size_request (-1, scrollbar_height + unit_size.second / 4);
		}
	} 
}


/** Set up the dialogue */
void
PortMatrix::setup ()
{
	clear ();

 	int const rows = n_rows ();
	
 	/* Row labels */

	_row_labels_vbox = new VBox;
	int const run_rows = std::max (1, rows);

	for (int j = 0; j < run_rows; ++j) {
		
		/* embolden the port/channel name */
		
		string s = "<b>";
		s += row_name (j);
		s += "</b>";
		
		Label* label = manage (new Label (s));
		EventBox* b = manage (new EventBox);
		
		label->set_use_markup (true);
		
		b->set_events (Gdk::BUTTON_PRESS_MASK);
		b->signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &IOSelector::row_label_button_pressed), j));
		b->add (*label);
		
		_row_labels.push_back (b);
		_row_labels_vbox->pack_start (*b, false, false);
	}

	_side_vbox_pad = new Label (""); /* unmanaged, explicitly deleted */

	if (_offer_inputs) {
		_side_vbox.pack_start (*_side_vbox_pad, false, false);
		_side_vbox.pack_start (*_row_labels_vbox, false, false);
		_side_vbox.pack_start (*manage (new Label ("")));
	} else {
		_side_vbox.pack_start (*manage (new Label ("")));
		_side_vbox.pack_start (*_row_labels_vbox, false, false);
		_side_vbox.pack_start (*_side_vbox_pad, false, false);
	}

 	/* Checkbutton tables and visibility checkbuttons */
 	for (PortGroupList::iterator i = _port_group_list.begin(); i != _port_group_list.end(); ++i) {

		PortGroupUI* t = new PortGroupUI (*this, **i);
		
		_port_group_ui.push_back (t);
		_port_group_hbox.pack_start (t->get_table(), false, false);
		
		_visibility_checkbutton_box.pack_start (t->get_visibility_checkbutton(), false, false);

		CheckButton* chk = dynamic_cast<CheckButton*>(&t->get_visibility_checkbutton());

		if (chk) { 
			chk->signal_toggled().connect (sigc::mem_fun (*this, &PortMatrix::reset_visibility));
		}
	}

	show_all ();

	reset_visibility ();
}

void
PortMatrix::reset_visibility ()
{
	/* now adjust visibility and coloring */

	bool even = true;
	Gdk::Color odd_bg (color_from_style ("OddPortGroups", STATE_NORMAL, "fg"));
	Gdk::Color even_bg (color_from_style ("EvenPortGroups", STATE_NORMAL, "fg"));

	for (std::vector<PortGroupUI*>::iterator i = _port_group_ui.begin(); i != _port_group_ui.end(); ++i) {

		(*i)->setup_visibility ();
		
		if ((*i)->port_group().visible) {
			if (even) {
				(*i)->get_table().modify_bg (STATE_NORMAL, even_bg);
			} else {
				(*i)->get_table().modify_bg (STATE_NORMAL, odd_bg);
			}
			even = !even;
		}
	}
}

void
PortMatrix::redisplay ()
{
	_port_group_list.refresh ();
	setup ();
}


/** Handle a button press on a row label */
bool
PortMatrix::row_label_button_pressed (GdkEventButton* e, int r)
{
	if (e->type != GDK_BUTTON_PRESS || e->button != 3) {
		return false;
	}

	Menu* menu = manage (new Menu);
	Menu_Helpers::MenuList& items = menu->items ();
	menu->set_name ("ArdourContextMenu");

	bool const can_add = maximum_rows () > n_rows ();
	bool const can_remove = minimum_rows () < n_rows ();
	std::string const name = row_name (r);
	
	items.push_back (
		Menu_Helpers::MenuElem (string_compose(_("Add %1"), row_descriptor()), sigc::mem_fun (*this, &PortMatrix::add_row))
		);

	items.back().set_sensitive (can_add);

	items.push_back (
		Menu_Helpers::MenuElem (string_compose(_("Remove %1 \"%2\""), row_descriptor(), name), sigc::bind (sigc::mem_fun (*this, &PortMatrix::remove_row), r))
		);

	items.back().set_sensitive (can_remove);

	menu->popup (e->button, e->time);
	
	return true;
}

void
PortMatrix::set_type (ARDOUR::DataType t)
{
	_type = t;
	_port_group_list.set_type (t);
	redisplay ();
}

void
PortMatrix::set_offer_inputs (bool i)
{
	_offer_inputs = i;
	_port_group_list.set_offer_inputs (i);
	redisplay ();
}

/** PortGroupList constructor.
 *  @param session Session to get ports from.
 *  @param type Type of ports to offer (audio or MIDI)
 *  @param offer_inputs true to offer output ports, otherwise false.
 *  @param mask Mask of groups to make visible by default.
 */

PortGroupList::PortGroupList (ARDOUR::Session & session, ARDOUR::DataType type, bool offer_inputs, Mask mask)
	: _session (session), _type (type), _offer_inputs (offer_inputs),
	  buss (_("Bus"), "ardour:", mask & BUSS),
	  track (_("Track"), "ardour:", mask & TRACK),
	  system (_("System"), "system:", mask & SYSTEM),
	  other (_("Other"), "", mask & OTHER)
{
	refresh ();
}

void
PortGroupList::refresh ()
{
	clear ();
	
	buss.ports.clear ();
	track.ports.clear ();
	system.ports.clear ();
	other.ports.clear ();

	/* Find the ports provided by ardour; we can't derive their type just from their
	   names, so we'll have to be more devious. 
	*/

	boost::shared_ptr<ARDOUR::Session::RouteList> routes = _session.get_routes ();

	for (ARDOUR::Session::RouteList::const_iterator i = routes->begin(); i != routes->end(); ++i) {

		PortGroup* g = 0;

		if (_type == ARDOUR::DataType::AUDIO) {

			if (boost::dynamic_pointer_cast<ARDOUR::AudioTrack> (*i)) {
				g = &track;
			} else if (!boost::dynamic_pointer_cast<ARDOUR::MidiTrack>(*i)) {
				g = &buss;
			} 


		} else if (_type == ARDOUR::DataType::MIDI) {

			if (boost::dynamic_pointer_cast<ARDOUR::MidiTrack> (*i)) {
				g = &track;
			}

			/* No MIDI busses yet */
		} 
			
		if (g) {
			ARDOUR::PortSet const & p = _offer_inputs ? ((*i)->inputs()) : ((*i)->outputs());
			for (uint32_t j = 0; j < p.num_ports(); ++j) {
				g->add (p.port(j)->name ());
			}

			std::sort (g->ports.begin(), g->ports.end());
		}
	}
	
	/* XXX: inserts, sends, plugin inserts? */
	
	/* Now we need to find the non-ardour ports; we do this by first
	   finding all the ports that we can connect to. 
	*/

	const char **ports = _session.engine().get_ports ("", _type.to_jack_type(), _offer_inputs ? 
							  JackPortIsInput : JackPortIsOutput);
	if (ports) {

		int n = 0;
		string client_matching_string;

		client_matching_string = _session.engine().client_name();
		client_matching_string += ':';

		while (ports[n]) {
			std::string const p = ports[n];

			if (p.substr(0, strlen ("system:")) == "system:") {
				/* system: prefix */
				system.add (p);
			} else {
				if (p.substr(0, client_matching_string.length()) != client_matching_string) {
					/* other (non-ardour) prefix */
					other.add (p);
				}
			}

			++n;
		}

		free (ports);
	}

	push_back (&system);
	push_back (&buss);
	push_back (&track);
	push_back (&other);
}

int
PortGroupList::n_visible_ports () const
{
	int n = 0;
	
	for (const_iterator i = begin(); i != end(); ++i) {
		if ((*i)->visible) {
			n += (*i)->ports.size();
		}
	}

	return n;
}

std::string
PortGroupList::get_port_by_index (int n, bool with_prefix) const
{
	/* XXX: slightly inefficient algorithm */

	for (const_iterator i = begin(); i != end(); ++i) {
		for (std::vector<std::string>::const_iterator j = (*i)->ports.begin(); j != (*i)->ports.end(); ++j) {
			if (n == 0) {
				if (with_prefix) {
					return (*i)->prefix + *j;
				} else {
					return *j;
				}
			}
			--n;
		}
	}

	return "";
}

void
PortGroupList::set_type (ARDOUR::DataType t)
{
	_type = t;
}

void
PortGroupList::set_offer_inputs (bool i)
{
	_offer_inputs = i;
}

