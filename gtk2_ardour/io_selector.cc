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
#include "utils.h"
#include "gui_thread.h"
#include "i18n.h"

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


PortGroupTable::PortGroupTable (
	PortGroup& g, boost::shared_ptr<ARDOUR::IO> io, bool for_input
	)
	: _port_group (g), _ignore_check_button_toggle (false),
	  _io (io), _for_input (for_input)
{
	ARDOUR::DataType const t = _io->default_type();

	int rows;
	if (_for_input) {
		rows = _io->n_inputs().get(t);
	} else {
		rows = _io->n_outputs().get(t);
	}	
	
	int const ports = _port_group.ports.size();

	if (rows == 0 || ports == 0) {
		return;
	}

	/* Sort out the table and the checkbuttons inside it */
	
	_table.resize (rows, ports);
	_check_buttons.resize (rows);
	for (int i = 0; i < rows; ++i) {
		_check_buttons[i].resize (ports);
	}

	for (int i = 0; i < rows; ++i) {
		for (uint32_t j = 0; j < _port_group.ports.size(); ++j) {
			Gtk::CheckButton* b = new Gtk::CheckButton;
			b->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &PortGroupTable::check_button_toggled), b, i, _port_group.prefix + _port_group.ports[j]));
			_check_buttons[i][j] = b;
			_table.attach (*b, j, j + 1, i, i + 1);
		}
	}

	_box.add (_table);

 	_ignore_check_button_toggle = true;

 	/* Set the state of the check boxes according to current connections */
 	for (int i = 0; i < rows; ++i) {
 		const char **connections = _for_input ? _io->input(i)->get_connections() : _io->output(i)->get_connections();
 		for (uint32_t j = 0; j < _port_group.ports.size(); ++j) {

 			std::string const t = _port_group.prefix + _port_group.ports[j];
 			int k = 0;
 			bool required_state = false;

 			while (connections && connections[k]) {
 				if (std::string(connections[k]) == t) {
 					required_state = true;
 					break;
 				}
 				++k;
 			}

 			_check_buttons[i][j]->set_active (required_state);
 		}
 	}

 	_ignore_check_button_toggle = false;
}

/** @return Width and height of a single check button in a port group table */
std::pair<int, int>
PortGroupTable::unit_size () const
{
	if (_check_buttons.empty() || _check_buttons[0].empty()) {
		return std::pair<int, int> (0, 0);
	}

	return std::make_pair (
		_check_buttons[0][0]->get_width() + _table.get_col_spacing (0),
		_check_buttons[0][0]->get_height() + _table.get_row_spacing (0)
		);
}

Gtk::Widget&
PortGroupTable::get_widget ()
{
	return _box;
}


/** Handle a toggle of a check button */
void
PortGroupTable::check_button_toggled (Gtk::CheckButton* b, int r, std::string const & p)
{
 	if (_ignore_check_button_toggle) {
 		return;
 	}
	
 	bool const new_state = b->get_active ();

 	if (new_state) {
 		if (_for_input) {
 			_io->connect_input (_io->input(r), p, 0);
 		} else {
 			_io->connect_output (_io->output(r), p, 0);
 		}
 	} else {
 		if (_for_input) {
 			_io->disconnect_input (_io->input(r), p, 0);
 		} else {
 			_io->disconnect_output (_io->output(r), p, 0);
 		}
 	}
}


RotatedLabelSet::RotatedLabelSet (PortGroupList& g)
	: Glib::ObjectBase ("RotatedLabelSet"), Gtk::Widget (), _port_group_list (g), _base_width (128)
{
	set_flags (Gtk::NO_WINDOW);
	set_angle (30);
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
RotatedLabelSet::on_size_request (Gtk::Requisition* requisition)
{
	*requisition = Gtk::Requisition ();

	if (_pango_layout == 0) {
		return;
	}

	/* Our height is the highest label */
	requisition->height = 0;
	for (PortGroupList::const_iterator i = _port_group_list.begin(); i != _port_group_list.end(); ++i) {
		for (std::vector<std::string>::const_iterator j = i->ports.begin(); j != i->ports.end(); ++j) {
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
}

void
RotatedLabelSet::on_size_allocate (Gtk::Allocation& allocation)
{
	set_allocation (allocation);

	if (_gdk_window) {
		_gdk_window->move_resize (
			allocation.get_x(), allocation.get_y(), allocation.get_width(), allocation.get_height()
			);
	}
}

void
RotatedLabelSet::on_realize ()
{
	Gtk::Widget::on_realize ();

	Glib::RefPtr<Gtk::Style> style = get_style ();

	if (!_gdk_window) {
		GdkWindowAttr attributes;
		memset (&attributes, 0, sizeof (attributes));

		Gtk::Allocation allocation = get_allocation ();
		attributes.x = allocation.get_x ();
		attributes.y = allocation.get_y ();
		attributes.width = allocation.get_width ();
		attributes.height = allocation.get_height ();

		attributes.event_mask = get_events () | Gdk::EXPOSURE_MASK; 
		attributes.window_type = GDK_WINDOW_CHILD;
		attributes.wclass = GDK_INPUT_OUTPUT;

		_gdk_window = Gdk::Window::create (get_window (), &attributes, GDK_WA_X | GDK_WA_Y);
		unset_flags (Gtk::NO_WINDOW);
		set_window (_gdk_window);

		_bg_colour = style->get_bg (Gtk::STATE_NORMAL );
		modify_bg (Gtk::STATE_NORMAL, _bg_colour);
		_fg_colour = style->get_fg (Gtk::STATE_NORMAL);
;
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

	Gtk::Widget::on_unrealize ();
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
	   to do this for us, but I can't find out how... */
	std::pair<int, int> d;
	d.first = int (w * cos (_angle_radians) - h * sin (_angle_radians));
	d.second = int (w * sin (_angle_radians) + h * cos (_angle_radians));

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
		if (i->visible) {
			for (uint32_t j = 0; j < i->ports.size(); ++j) {
				std::pair<int, int> const d = setup_layout (i->ports[j]);
				get_window()->draw_layout (_gc, int ((n + 0.25) * spacing), height - d.second, _pango_layout, _fg_colour, _bg_colour);
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


/** Construct an IOSelector.
 *  @param session Session to operate on.
 *  @param io IO to operate on.
 *  @param for_input true if the selector is for an input, otherwise false.
 */
 
IOSelector::IOSelector (ARDOUR::Session& session, boost::shared_ptr<ARDOUR::IO> io, bool for_input)
	: _port_group_list (session, io, for_input), _io (io), _for_input (for_input),
	  _row_labels_vbox (0), _column_labels (_port_group_list), _left_vbox_pad (0)
{
	Gtk::HBox* c = new Gtk::HBox;
	for (PortGroupList::iterator i = _port_group_list.begin(); i != _port_group_list.end(); ++i) {
		Gtk::CheckButton* b = new Gtk::CheckButton (i->name);
		b->set_active (true);
		b->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &IOSelector::group_visible_toggled), b, i->name));
		c->pack_start (*Gtk::manage (b));
	}
	pack_start (*Gtk::manage (c));
	
	_left_vbox.pack_start (*Gtk::manage (new Gtk::Label ("")));
	_overall_hbox.pack_start (_left_vbox, false, false);
	_scrolled_window.set_policy (Gtk::POLICY_ALWAYS, Gtk::POLICY_NEVER);
	_scrolled_window.set_shadow_type (Gtk::SHADOW_NONE);
	Gtk::VBox* b = new Gtk::VBox;
	b->pack_start (_column_labels, false, false);
	b->pack_start (_port_group_hbox, false, false);
	Gtk::Alignment* a = new Gtk::Alignment (0, 1, 0, 0);
	a->add (*Gtk::manage (b));
	_scrolled_window.add (*Gtk::manage (a));
	_overall_hbox.pack_start (_scrolled_window);
	pack_start (_overall_hbox);

	_port_group_hbox.signal_size_allocate().connect (sigc::hide (sigc::mem_fun (*this, &IOSelector::setup_dimensions)));

	/* Listen for ports changing on the IO */
	if (_for_input) {
		_io->input_changed.connect (mem_fun(*this, &IOSelector::ports_changed));
	} else {
		_io->output_changed.connect (mem_fun(*this, &IOSelector::ports_changed));
	}
	
}

IOSelector::~IOSelector ()
{
	clear ();
}

/** Clear out the things that change when the number of source or destination ports changes */
void
IOSelector::clear ()
{
	for (std::vector<Gtk::EventBox*>::iterator i = _row_labels.begin(); i != _row_labels.end(); ++i) {
		delete *i;
	}
	_row_labels.clear ();
	
	if (_row_labels_vbox) {
		_left_vbox.remove (*_row_labels_vbox);
	}
	delete _row_labels_vbox;
	_row_labels_vbox = 0;

	if (_left_vbox_pad) {
		_left_vbox.remove (*_left_vbox_pad);
	}
	delete _left_vbox_pad;
	_left_vbox_pad = 0;
	
	for (std::vector<PortGroupTable*>::iterator i = _port_group_tables.begin(); i != _port_group_tables.end(); ++i) {
		_port_group_hbox.remove ((*i)->get_widget());
		delete *i;
	}

	_port_group_tables.clear ();
}


/** Set up dimensions of some of our widgets which depend on other dimensions
 *  within the dialogue.
 */
void
IOSelector::setup_dimensions ()
{
	/* Get some dimensions from various places */
	int const scrollbar_height = _scrolled_window.get_hscrollbar()->get_height();

	std::pair<int, int> unit_size (0, 0);
	int port_group_tables_height = 0;
	for (std::vector<PortGroupTable*>::iterator i = _port_group_tables.begin(); i != _port_group_tables.end(); ++i) {
		std::pair<int, int> const u = (*i)->unit_size ();
		unit_size.first = std::max (unit_size.first, u.first);
		unit_size.second = std::max (unit_size.second, u.second);
		port_group_tables_height = std::max (
			port_group_tables_height, (*i)->get_widget().get_height()
			);
	}

	/* Column labels */
	_column_labels.set_base_width (_port_group_list.n_visible_ports () * unit_size.first);

	/* Scrolled window */
	/* XXX: really shouldn't set a minimum horizontal size here, but if we don't
	   the window starts up very small */
	_scrolled_window.set_size_request (
		std::min (_column_labels.get_width(), 640),
		_column_labels.get_height() + port_group_tables_height + scrollbar_height + 16
		);
	
	/* Row labels */
	for (std::vector<Gtk::EventBox*>::iterator i = _row_labels.begin(); i != _row_labels.end(); ++i) {
		(*i)->get_child()->set_size_request (-1, unit_size.second);
	}


	if (_left_vbox_pad) {
		_left_vbox_pad->set_size_request (-1, scrollbar_height + unit_size.second / 4);
	}
}


/** Set up the dialogue */
void
IOSelector::setup ()
{
	clear ();

 	/* Work out how many rows we have */
 	ARDOUR::DataType const t = _io->default_type();

 	int rows;
 	if (_for_input) {
 		rows = _io->n_inputs().get(t);
 	} else {
 		rows = _io->n_outputs().get(t);
 	}	
	
 	/* Row labels */
 	_row_labels_vbox = new Gtk::VBox;
 	for (int i = 0; i < rows; ++i) {
 		Gtk::Label* label = new Gtk::Label (_for_input ? _io->input(i)->name() : _io->output(i)->name());
		Gtk::EventBox* b = new Gtk::EventBox;
		b->set_events (Gdk::BUTTON_PRESS_MASK);
		b->signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &IOSelector::row_label_button_pressed), i));
		b->add (*Gtk::manage (label));
		_row_labels.push_back (b);
 		_row_labels_vbox->pack_start (*b, false, false);
 	}
 	_left_vbox.pack_start (*_row_labels_vbox, false, false);
	_left_vbox_pad = new Gtk::Label ("");
	_left_vbox.pack_start (*_left_vbox_pad, false, false);

 	/* Checkbutton tables */
 	int n = 0;
 	for (PortGroupList::iterator i = _port_group_list.begin(); i != _port_group_list.end(); ++i) {
 		PortGroupTable* t = new PortGroupTable (*i, _io, _for_input);

 		/* XXX: this is a bit of a hack; should probably use a configurable colour here */
 		Gdk::Color alt_bg = get_style()->get_bg (Gtk::STATE_NORMAL);
 		alt_bg.set_rgb (alt_bg.get_red() + 4096, alt_bg.get_green() + 4096, alt_bg.get_blue () + 4096);
 		if ((n % 2) == 0) {
 			t->get_widget().modify_bg (Gtk::STATE_NORMAL, alt_bg);
 		}

 		_port_group_tables.push_back (t);
 		_port_group_hbox.pack_start (t->get_widget(), false, false);
 		++n;
 	}

	show_all ();
}

void
IOSelector::ports_changed (ARDOUR::IOChange change, void *src)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &IOSelector::ports_changed), change, src));

	redisplay ();
}


void
IOSelector::redisplay ()
{
	_port_group_list.refresh ();
	setup ();
}


/** Handle a button press on a row label */
bool
IOSelector::row_label_button_pressed (GdkEventButton* e, int r)
{
	if (e->type != GDK_BUTTON_PRESS || e->button != 3) {
		return false;
	}

	Gtk::Menu* menu = Gtk::manage (new Gtk::Menu);
	Gtk::Menu_Helpers::MenuList& items = menu->items ();
	menu->set_name ("ArdourContextMenu");

	bool can_add;
	bool can_remove;
	std::string name;
	ARDOUR::DataType const t = _io->default_type();

	if (_for_input) {
		can_add = _io->input_maximum().get(t) > _io->n_inputs().get(t);
		can_remove = _io->input_minimum().get(t) < _io->n_inputs().get(t);
		name = _io->input(r)->name();
	} else {
		can_add = _io->output_maximum().get(t) > _io->n_outputs().get(t);
		can_remove = _io->output_minimum().get(t) < _io->n_outputs().get(t);
		name = _io->output(r)->name();
	}
	
	items.push_back (
		Gtk::Menu_Helpers::MenuElem (_("Add port"), sigc::mem_fun (*this, &IOSelector::add_port))
		);

	items.back().set_sensitive (can_add);

	items.push_back (
		Gtk::Menu_Helpers::MenuElem (_("Remove port '") + name + _("'"), sigc::bind (sigc::mem_fun (*this, &IOSelector::remove_port), r))
		);

	items.back().set_sensitive (can_remove);

	menu->popup (e->button, e->time);
	
	return true;
}

void
IOSelector::add_port ()
{
	// The IO selector only works for single typed IOs
	const ARDOUR::DataType t = _io->default_type ();

	if (_for_input) {

		try {
			_io->add_input_port ("", this);
		}

		catch (ARDOUR::AudioEngine::PortRegistrationFailure& err) {
			Gtk::MessageDialog msg (0,  _("There are no more JACK ports available."));
			msg.run ();
		}

	} else {

		try {
			_io->add_output_port ("", this);
		}

		catch (ARDOUR::AudioEngine::PortRegistrationFailure& err) {
			Gtk::MessageDialog msg (0, _("There are no more JACK ports available."));
			msg.run ();
		}
	}
}

void
IOSelector::remove_port (int r)
{
	// The IO selector only works for single typed IOs
	const ARDOUR::DataType t = _io->default_type ();
	
	if (_for_input) {
		_io->remove_input_port (_io->input (r), this);
	} else {
		_io->remove_output_port (_io->output (r), this);
	}
}

void
IOSelector::group_visible_toggled (Gtk::CheckButton* b, std::string const & n)
{
	PortGroupList::iterator i = _port_group_list.begin();
	while (i != _port_group_list.end() & i->name != n) {
		++i;
	}

	if (i == _port_group_list.end()) {
		return;
	}

	i->visible = b->get_active ();

	/* Update PortGroupTable visibility */

	for (std::vector<PortGroupTable*>::iterator j = _port_group_tables.begin(); j != _port_group_tables.end(); ++j) {
		if ((*j)->port_group().visible) {
			(*j)->get_widget().show();
		} else {
			(*j)->get_widget().hide();
		}
	}

	_column_labels.queue_draw ();
}


PortGroupList::PortGroupList (ARDOUR::Session & session, boost::shared_ptr<ARDOUR::IO> io, bool for_input)
	: _session (session), _io (io), _for_input (for_input)
{
	refresh ();
}

void
PortGroupList::refresh ()
{
	clear ();

	/* Find the ports provided by ardour; we can't derive their type just from their
	   names, so we'll have to be more devious. */

	boost::shared_ptr<ARDOUR::Session::RouteList> routes = _session.get_routes ();

	PortGroup buss (_("Buss"), "ardour:");
	PortGroup track (_("Track"), "ardour:");

	for (ARDOUR::Session::RouteList::const_iterator i = routes->begin(); i != routes->end(); ++i) {

		PortGroup* g = 0;
		if (_io->default_type() == ARDOUR::DataType::AUDIO && dynamic_cast<ARDOUR::AudioTrack*> ((*i).get())) {
			/* Audio track for an audio IO */
			g = &track;
		} else if (_io->default_type() == ARDOUR::DataType::MIDI && dynamic_cast<ARDOUR::MidiTrack*> ((*i).get())) {
			/* Midi track for a MIDI IO */
			g = &track;
		} else if (_io->default_type() == ARDOUR::DataType::AUDIO && dynamic_cast<ARDOUR::MidiTrack*> ((*i).get()) == 0) {
			/* Non-MIDI track for an Audio IO; must be an audio buss */
			g = &buss;
		}

		if (g) {
			ARDOUR::PortSet const & p = _for_input ? ((*i)->outputs()) : ((*i)->inputs());
			for (uint32_t j = 0; j < p.num_ports(); ++j) {
				g->add (p.port(j)->name ());
			}

			std::sort (g->ports.begin(), g->ports.end());
		}
	}
	

	/* XXX: inserts, sends, plugin inserts? */
	
	/* Now we need to find the non-ardour ports; we do this by first
	   finding all the ports that we can connect to. */
	const char **ports = _session.engine().get_ports (
		"", _io->default_type().to_jack_type(), _for_input ? JackPortIsOutput : JackPortIsInput
		);

	PortGroup system (_("System"), "system:");
	PortGroup other (_("Other"), "");
	
	if (ports) {

		int n = 0;
		while (ports[n]) {
			std::string const p = ports[n];

			if (p.substr(0, strlen ("system:")) == "system:") {
				/* system: prefix */
				system.add (p);
			} else {
				if (p.substr(0, strlen("ardour:")) != "ardour:") {
					/* other (non-ardour) prefix */
					other.add (p);
				}
			}

			++n;
		}
	}

	push_back (buss);
	push_back (track);
	push_back (system);
	push_back (other);
}

int
PortGroupList::n_visible_ports () const
{
	int n = 0;
	
	for (const_iterator i = begin(); i != end(); ++i) {
		if (i->visible) {
			n += i->ports.size();
		}
	}

	return n;
}

std::string
PortGroupList::get_port_by_index (int n, bool with_prefix) const
{
	/* XXX: slightly inefficient algorithm */

	for (const_iterator i = begin(); i != end(); ++i) {
		for (std::vector<std::string>::const_iterator j = i->ports.begin(); j != i->ports.end(); ++j) {
			if (n == 0) {
				if (with_prefix) {
					return i->prefix + *j;
				} else {
					return *j;
				}
			}
			--n;
		}
	}

	return "";
}


IOSelectorWindow::IOSelectorWindow (
	ARDOUR::Session& session, boost::shared_ptr<ARDOUR::IO> io, bool for_input, bool can_cancel
	)
	: ArdourDialog ("I/O selector"),
	  _selector (session, io, for_input),
	  ok_button (can_cancel ? _("OK"): _("Close")),
	  cancel_button (_("Cancel")),
	  rescan_button (_("Rescan"))

{
	add_events (Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
	set_name ("IOSelectorWindow2");

	string title;
	if (for_input) {
		title = string_compose(_("%1 input"), io->name());
	} else {
		title = string_compose(_("%1 output"), io->name());
	}

	ok_button.set_name ("IOSelectorButton");
	if (!can_cancel) {
		ok_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::CLOSE, Gtk::ICON_SIZE_BUTTON)));
	}
	cancel_button.set_name ("IOSelectorButton");
	rescan_button.set_name ("IOSelectorButton");
	rescan_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::REFRESH, Gtk::ICON_SIZE_BUTTON)));

	get_action_area()->pack_start (rescan_button, false, false);

	if (can_cancel) {
		cancel_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::CANCEL, Gtk::ICON_SIZE_BUTTON)));
		get_action_area()->pack_start (cancel_button, false, false);
	} else {
		cancel_button.hide();
	}
		
	get_action_area()->pack_start (ok_button, false, false);

	get_vbox()->set_spacing (8);
	get_vbox()->pack_start (_selector);

	ok_button.signal_clicked().connect (mem_fun(*this, &IOSelectorWindow::accept));
	cancel_button.signal_clicked().connect (mem_fun(*this, &IOSelectorWindow::cancel));
	rescan_button.signal_clicked().connect (mem_fun(*this, &IOSelectorWindow::rescan));

	set_title (title);
	set_position (Gtk::WIN_POS_MOUSE);

	show_all ();

	signal_delete_event().connect (bind (sigc::ptr_fun (just_hide_it), reinterpret_cast<Window *> (this)));
}

IOSelectorWindow::~IOSelectorWindow()
{
	
}

void
IOSelectorWindow::rescan ()
{
	_selector.redisplay ();
}

void
IOSelectorWindow::cancel ()
{
	_selector.Finished (IOSelector::Cancelled);
	hide ();
}

void
IOSelectorWindow::accept ()
{
	_selector.Finished (IOSelector::Accepted);
	hide ();
}

void
IOSelectorWindow::on_map ()
{
	_selector.redisplay ();
	Window::on_map ();
}


PortInsertUI::PortInsertUI (ARDOUR::Session& sess, boost::shared_ptr<ARDOUR::PortInsert> pi)
	: input_selector (sess, pi->io(), true),
	  output_selector (sess, pi->io(), false)
{
	hbox.pack_start (output_selector, true, true);
	hbox.pack_start (input_selector, true, true);

	pack_start (hbox);
}

void
PortInsertUI::redisplay ()
{
	input_selector.redisplay();
	output_selector.redisplay();
}

void
PortInsertUI::finished (IOSelector::Result r)
{
	input_selector.Finished (r);
	output_selector.Finished (r);
}


PortInsertWindow::PortInsertWindow (ARDOUR::Session& sess, boost::shared_ptr<ARDOUR::PortInsert> pi, bool can_cancel)
	: ArdourDialog ("port insert dialog"),
	  _portinsertui (sess, pi),
	  ok_button (can_cancel ? _("OK"): _("Close")),
	  cancel_button (_("Cancel")),
	  rescan_button (_("Rescan"))
{

	set_name ("IOSelectorWindow");
	string title = _("ardour: ");
	title += pi->name();
	set_title (title);
	
	ok_button.set_name ("IOSelectorButton");
	if (!can_cancel) {
		ok_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::CLOSE, Gtk::ICON_SIZE_BUTTON)));
	}
	cancel_button.set_name ("IOSelectorButton");
	rescan_button.set_name ("IOSelectorButton");
	rescan_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::REFRESH, Gtk::ICON_SIZE_BUTTON)));

	get_action_area()->pack_start (rescan_button, false, false);
	if (can_cancel) {
		cancel_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::CANCEL, Gtk::ICON_SIZE_BUTTON)));
		get_action_area()->pack_start (cancel_button, false, false);
	} else {
		cancel_button.hide();
	}
	get_action_area()->pack_start (ok_button, false, false);

	get_vbox()->pack_start (_portinsertui);

	ok_button.signal_clicked().connect (mem_fun (*this, &PortInsertWindow::accept));
	cancel_button.signal_clicked().connect (mem_fun (*this, &PortInsertWindow::cancel));
	rescan_button.signal_clicked().connect (mem_fun (*this, &PortInsertWindow::rescan));

	signal_delete_event().connect (bind (sigc::ptr_fun (just_hide_it), reinterpret_cast<Window *> (this)));	

	going_away_connection = pi->GoingAway.connect (mem_fun (*this, &PortInsertWindow::plugin_going_away));
}

void
PortInsertWindow::plugin_going_away ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &PortInsertWindow::plugin_going_away));
	
	going_away_connection.disconnect ();
	delete_when_idle (this);
}

void
PortInsertWindow::on_map ()
{
	_portinsertui.redisplay ();
	Window::on_map ();
}


void
PortInsertWindow::rescan ()
{
	_portinsertui.redisplay ();
}

void
PortInsertWindow::cancel ()
{
	_portinsertui.finished (IOSelector::Cancelled);
	hide ();
}

void
PortInsertWindow::accept ()
{
	_portinsertui.finished (IOSelector::Accepted);
	hide ();
}
