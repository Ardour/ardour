/*
    Copyright (C) 2002-2003 Paul Davis 

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
#include <glibmm/objectbase.h>
#include <gtkmm2ext/doi.h>
#include <ardour/port_insert.h>
#include "ardour/session.h"
#include "ardour/io.h"
#include "ardour/audioengine.h"
#include "ardour/track.h"
#include "io_selector.h"
#include "utils.h"
#include "gui_thread.h"
#include "i18n.h"

RotatedLabelSet::RotatedLabelSet (GroupedPortList& g)
	: Glib::ObjectBase ("RotatedLabelSet"), Gtk::Widget (), _port_list (g), _base_start (64), _base_width (128)
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
	for (GroupedPortList::const_iterator i = _port_list.begin(); i != _port_list.end(); ++i) {
		for (std::vector<std::string>::const_iterator j = i->ports.begin(); j != i->ports.end(); ++j) {
			std::pair<int, int> const d = setup_layout (*j);
			if (d.second > requisition->height) {
				requisition->height = d.second;
			}
		}
	}

	/* And our width is the base plus the width of the last label */
	requisition->width = _base_start + _base_width;
	int const n = _port_list.n_ports ();
	if (n > 0) {
		std::pair<int, int> const d = setup_layout (_port_list.get_port_by_index (n - 1, false));
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


/**
 *    Set up our Pango layout to plot a given string, and compute its dimensions once it has been rotated.
 *    @param s String to use.
 *    @return width and height of the rotated string, in pixels.
 */

std::pair<int, int>
RotatedLabelSet::setup_layout (std::string const & s)
{
	_pango_layout->set_text (s);

	/* Here's the unrotated size */
	int w;
	int h;
	_pango_layout->get_pixel_size (w, h);

	/* Rotate the width and height as appropriate.  I thought Pango might be able to do this for us,
	   but I can't find out how... */
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
	double const spacing = double (_base_width) / _port_list.n_ports();

	/* Plot all the labels; really we should clip for efficiency */
	int n = 0;
	for (GroupedPortList::const_iterator i = _port_list.begin(); i != _port_list.end(); ++i) {
		for (uint32_t j = 0; j < i->ports.size(); ++j) {
			std::pair<int, int> const d = setup_layout (i->ports[j]);
			get_window()->draw_layout (_gc, _base_start + int ((n + 0.25) * spacing), height - d.second, _pango_layout, _fg_colour, _bg_colour);
			++n;
		}
	}

	return true;
}

/**
 *  Set the `base dimensions'.  These are the dimensions of the area at which the labels start, and
 *  have to be set up to match whatever they are labelling.
 *
 *  Roughly speaking, we have
 *
 *             L L L L
 *            E E E E
 *           B B B B
 *          A A A A
 *         L L L L
 * <--s--><--w--->
 */
    
void
RotatedLabelSet::set_base_dimensions (int s, int w)
{
	_base_start = s;
	_base_width = w;
	queue_resize ();
}


/**
 *  Construct an IOSelector.
 *  @param session Session to operate on.
 *  @param io IO to operate on.
 *  @param for_input true if the selector is for an input, otherwise false.
 */
 
IOSelector::IOSelector (ARDOUR::Session& session, boost::shared_ptr<ARDOUR::IO> io, bool for_input)
	: _session (session), _port_list (session, io, for_input), _io (io), _for_input (for_input),
	  _width (0), _height (0), _column_labels (_port_list),
	  _ignore_check_button_toggle (false), _add_remove_box_added (false)
{
	/* Column labels */
	pack_start (_column_labels, true, true);

	/* Buttons for adding and removing ports */
	_add_port_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::ADD, Gtk::ICON_SIZE_MENU)));
	_add_port_button.set_label (_("Add port"));
	_add_port_button.signal_clicked().connect (mem_fun (*this, &IOSelector::add_port_button_clicked));
	_add_remove_box.pack_start (_add_port_button);
	_remove_port_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::REMOVE, Gtk::ICON_SIZE_MENU)));
	_remove_port_button.set_label (_("Remove port"));
	_remove_port_button.signal_clicked().connect (mem_fun (*this, &IOSelector::remove_port_button_clicked));
	_add_remove_box.pack_start (_remove_port_button);
	set_button_sensitivity ();

	/* Table.  We need to put in a HBox, with a dummy label to its right,
	   so that the rotated column labels can overhang the right hand side of the table. */

	setup_table ();
	setup_row_labels ();
	setup_check_button_states ();
	_table_hbox.pack_start (_table, false, false);
	_table_hbox.pack_start (_dummy);
	_table.set_col_spacings (4);
	pack_start (_table_hbox);

	show_all ();

	update_column_label_dimensions ();

	/* Listen for ports changing on the IO */
	if (_for_input) {
		_io->input_changed.connect (mem_fun(*this, &IOSelector::ports_changed));
	} else {
		_io->output_changed.connect (mem_fun(*this, &IOSelector::ports_changed));
	}
	
}

IOSelector::~IOSelector ()
{
	for (std::vector<Gtk::Label*>::iterator i = _row_labels.begin(); i != _row_labels.end(); ++i) {
		delete *i;
	}

	for (uint32_t x = 0; x < _check_buttons.size(); ++x) {
		for (uint32_t y = 0; y < _check_buttons[x].size(); ++y) {
			delete _check_buttons[x][y];
		}
	}
}


/**
 *    Sets up the sizing of the column label widget to match the table that's underneath it.
 */

void
IOSelector::update_column_label_dimensions ()
{
	if (_row_labels.empty() || _check_buttons.empty() || _check_buttons[0].empty()) {
		return;
	}
	
	_column_labels.set_base_dimensions (
		/* width of the row label + a column spacing */
		_row_labels.front()->get_width() + _table.get_col_spacing (0),
		/* width of a check button + a column spacing for each column */
		_check_buttons.size() * ( _check_buttons[0][0]->get_width() + _table.get_col_spacing(1))
		);
}

void
IOSelector::setup_table ()
{
	if (_add_remove_box_added) {
		_table.remove (_add_remove_box);
	}

	for (std::vector<Gtk::EventBox*>::iterator i = _group_labels.begin(); i != _group_labels.end(); ++i) {
		_table.remove (**i);
		delete *i;
	}

	_group_labels.clear ();
	
	/* New width */
	
	int const old_width = _width;
	_width = _port_list.n_ports ();

	/* New height */

	int const old_height = _height;

	ARDOUR::DataType const t = _io->default_type();
	
	if (_for_input) {
		_height = _io->n_inputs().get(t);
	} else {
		_height = _io->n_outputs().get(t);
	}

	_table.resize (_width + 1, _height + 1);

	/* Add checkbuttons where required, and remove those that aren't */
	for (int x = _width; x < old_width; ++x) {
		for (int y = 0; y < old_height; ++y) {
			delete _check_buttons[x][y];
		}
	}
	_check_buttons.resize (_width);
	
	for (int x = 0; x < _width; x++) {

		for (int y = _height; y < old_height; ++y) {
			delete _check_buttons[x][y];
		}
		_check_buttons[x].resize (_height);
		
		for (int y = 0; y < _height; y++) {

			if (x >= old_width || y >= old_height) {
				Gtk::CheckButton* button = new Gtk::CheckButton;
				button->signal_toggled().connect (
					sigc::bind (sigc::mem_fun (*this, &IOSelector::check_button_toggled), x, y)
					);
				_check_buttons[x][y] = button;
				_table.attach (*button, x + 1, x + 2, y, y + 1);
			}
		}
	}

	/* Add more row labels where required, and remove those that aren't */
	for (int y = _height; y < old_height; ++y) {
		delete _row_labels[y];
	}
	_row_labels.resize (_height);
	for (int y = old_height; y < _height; ++y) {
		Gtk::Label* label = new Gtk::Label;
		_row_labels[y] = label;
		_table.attach (*label, 0, 1, y, y + 1);
	}

	_table.attach (_add_remove_box, 0, 1, _height, _height + 1);
	_add_remove_box_added = true;

	/* Add group labels */
	int n = 1;
	int m = 0;

	/* XXX: this is a bit of a hack; should probably use a configurable colour here */
	Gdk::Color alt_bg = get_style()->get_bg (Gtk::STATE_NORMAL);
	alt_bg.set_rgb (alt_bg.get_red() + 4096, alt_bg.get_green() + 4096, alt_bg.get_blue () + 4096);
	for (GroupedPortList::iterator i = _port_list.begin(); i != _port_list.end(); ++i) {
		if (i->ports.empty() == false) {
			Gtk::Label* label = new Gtk::Label ("<b>" + i->name + "</b>");
			label->set_use_markup (true);
			Gtk::EventBox* box = new Gtk::EventBox ();
			box->add (*Gtk::manage (label));
			if (m % 2 == 0) {
				box->modify_bg (Gtk::STATE_NORMAL, alt_bg);
			}
			_group_labels.push_back (box);
			_table.attach (*box, n, n + i->ports.size(), _height, _height + 1);
			n += i->ports.size();
			++m;
		}
	}

	show_all ();
}


/**
 *  Write the correct text to the row labels.
 */

void
IOSelector::setup_row_labels ()
{
	for (int y = 0; y < _height; y++) {
		_row_labels[y]->set_text (_for_input ? _io->input(y)->name() : _io->output(y)->name());
	}
}


/**
 *  Set up the state of each check button according to what connections are made.
 */

void
IOSelector::setup_check_button_states ()
{
	_ignore_check_button_toggle = true;

	/* Set the state of the check boxes according to current connections */
	for (int i = 0; i < _height; ++i) {
		const char **connections = _for_input ? _io->input(i)->get_connections() : _io->output(i)->get_connections();
		for (int j = 0; j < _width; ++j) {

			std::string const t = _port_list.get_port_by_index (j);
			int k = 0;
			bool required_state = false;

			while (connections && connections[k]) {
				if (std::string(connections[k]) == t) {
					required_state = true;
					break;
				}
				++k;
			}

			_check_buttons[j][i]->set_active (required_state);
		}
	}

	_ignore_check_button_toggle = false;
}


/**
 *  Handle a toggle of a check button.
 */

void
IOSelector::check_button_toggled (int x, int y)
{
	if (_ignore_check_button_toggle) {
		return;
	}
	
	bool const new_state = _check_buttons[x][y]->get_active ();
	std::string const port = _port_list.get_port_by_index (x);

	if (new_state) {
		if (_for_input) {
			_io->connect_input (_io->input(y), port, 0);
		} else {
			_io->connect_output (_io->output(y), port, 0);
		}
	} else {
		if (_for_input) {
			_io->disconnect_input (_io->input(y), port, 0);
		} else {
			_io->disconnect_output (_io->output(y), port, 0);
		}
	}
}

void
IOSelector::add_port_button_clicked ()
{
	/* add a new port, then hide the button if we're up to the maximum allowed */

	// The IO selector only works for single typed IOs
	const ARDOUR::DataType t = _io->default_type();

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
		
	set_button_sensitivity ();
}


void
IOSelector::remove_port_button_clicked ()
{
	uint32_t nports;

	// The IO selector only works for single typed IOs
	const ARDOUR::DataType t = _io->default_type();
	
	// always remove last port
	
	if (_for_input) {
		if ((nports = _io->n_inputs().get(t)) > 0) {
			_io->remove_input_port (_io->input(nports - 1), this);
		}
	} else {
		if ((nports = _io->n_outputs().get(t)) > 0) {
			_io->remove_output_port (_io->output(nports - 1), this);
		}
	}
	
	set_button_sensitivity ();
}


void 
IOSelector::set_button_sensitivity ()
{
	ARDOUR::DataType const t = _io->default_type();

	if (_for_input) {

		_add_port_button.set_sensitive (
			_io->input_maximum().get(t) > _io->n_inputs().get(t)
			);
	} else {

		_add_port_button.set_sensitive (
			_io->output_maximum().get(t) > _io->n_outputs().get(t)
			);
	}

	if (_for_input) {
		
		_remove_port_button.set_sensitive (
			_io->n_inputs().get(t) && _io->input_minimum().get(t) < _io->n_inputs().get(t)
			);
	} else {

		_remove_port_button.set_sensitive (
			_io->n_outputs().get(t) && _io->output_minimum().get(t) < _io->n_outputs().get(t)
			);
	}
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
	_port_list.refresh ();
	setup_table ();
	setup_row_labels ();
	setup_check_button_states ();
	update_column_label_dimensions ();
}




GroupedPortList::GroupedPortList (ARDOUR::Session & session, boost::shared_ptr<ARDOUR::IO> io, bool for_input)
	: _session (session), _io (io), _for_input (for_input)
{
	refresh ();
}

void
GroupedPortList::refresh ()
{
	clear ();

	/* Find the ports provided by ardour; we can't derive their type just from their
	   names, so we'll have to be more devious. */

	boost::shared_ptr<ARDOUR::Session::RouteList> routes = _session.get_routes ();

	PortGroup buss (_("Buss"), "ardour:");
	PortGroup track (_("Track"), "ardour:");

	for (ARDOUR::Session::RouteList::const_iterator i = routes->begin(); i != routes->end(); ++i) {

		PortGroup& g = dynamic_cast<ARDOUR::Track*> ((*i).get()) == 0 ? buss : track;
		
		ARDOUR::PortSet const & p = _for_input ? ((*i)->outputs()) : ((*i)->inputs());
		for (uint32_t j = 0; j < p.num_ports(); ++j) {
			std::string const n = p.port(j)->name ();
			g.ports.push_back (n.substr(strlen ("ardour:")));
		}

		std::sort (g.ports.begin(), g.ports.end());
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

		/* Count them */
		int n = 0;
		while (ports[n]) {
			++n;
		}
		
		for (int i = 0; i < n; ++i) {
			std::string const p = ports[i];

			if (p.substr(0, strlen ("system:")) == "system:") {
				/* system: prefix */
				system.ports.push_back (p.substr (strlen ("system:")));
			} else {
				if (p.substr(0, strlen("ardour:")) != "ardour:") {
					/* other (non-ardour) prefix */
					other.ports.push_back (p);
				}
			}
		}
	}

	push_back (buss);
	push_back (track);
	push_back (system);
	push_back (other);
}

int
GroupedPortList::n_ports () const
{
	int n = 0;
	
	for (const_iterator i = begin(); i != end(); ++i) {
		for (std::vector<std::string>::const_iterator j = i->ports.begin(); j != i->ports.end(); ++j) {
			++n;
		}
	}

	return n;
}

std::string
GroupedPortList::get_port_by_index (int n, bool with_prefix) const
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
