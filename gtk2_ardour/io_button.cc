/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2021 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/io_plug.h"
#include "ardour/profile.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/track.h"
#include "ardour/user_bundle.h"

#include "gtkmm2ext/menu_elems.h"
#include "gtkmm2ext/utils.h"
#include "widgets/tooltips.h"

#include "ardour_message.h"
#include "gui_thread.h"
#include "io_button.h"
#include "io_selector.h"
#include "route_ui.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace std;

void
IOButtonBase::maybe_update (PropertyChange const& what_changed)
{
	if (what_changed.contains (ARDOUR::Properties::name)) {
		update ();
	}
}

static bool
exclusively_connected (std::shared_ptr<IO> dest_io, std::shared_ptr<IO> io, DataType dt, uint32_t tcnt, std::string const& name, ostringstream& label)
{
	/* check if IO is exclusively connected to a subset of this Route's ports */
	uint32_t           n   = 0;
	uint32_t           cnt = 0;
	std::set<uint32_t> pn;
	PortSet const&     psa (dest_io->ports ());
	PortSet const&     psb (io->ports ());

	for (auto a = psa.begin (dt); a != psa.end (dt); ++a, ++n) {
		for (auto b = psb.begin (dt); b != psb.end (dt); ++b) {
			if (a->connected_to (b->name ())) {
				++cnt;
				pn.insert (n);
			}
		}
	}

	if (cnt != tcnt) {
		/* IO has additional connections.
		 * No need to check other Routes/IOPs (they will produce
		 * the same result).
		 */
		return false;
	}

	label << Gtkmm2ext::markup_escape_text (name) << " ";

	bool first = true;
	for (auto const& num : pn) {
		if (first) {
			first = false;
		} else {
			label << "+";
		}
		label << dest_io->bundle ()->channel_name (num);
	}
	return true;
}

DataType
IOButtonBase::guess_main_type (std::shared_ptr<IO> io)
{
	/* The heuristic follows these principles:
	 *  A) If all ports that the user connected are of the same type, then he
	 *     very probably intends to use the IO with that type. A common subcase
	 *     is when the IO has only ports of the same type (connected or not).
	 *  B) If several types of ports are connected, then we should guess based
	 *     on the likeliness of the user wanting to use a given type.
	 *     We assume that the DataTypes are ordered from the most likely to the
	 *     least likely when iterating or comparing them with "<".
	 *  C) If no port is connected, the same logic can be applied with all ports
	 *     instead of connected ones. TODO: Try other ideas, for instance look at
	 *     the last plugin output when |for_input| is false (note: when StrictIO
	 *     the outs of the last plugin should be the same as the outs of the route
	 *     modulo the panner which forwards non-audio anyway).
	 * All of these constraints are respected by the following algorithm that
	 * just returns the most likely datatype found in connected ports if any, or
	 * available ports if any (since if all ports are of the same type, the most
	 * likely found will be that one obviously). */

	/* Find most likely type among connected ports */
	DataType type = DataType::NIL; /* NIL is always last so least likely */
	for (PortSet::iterator p = io->ports ().begin (); p != io->ports ().end (); ++p) {
		if (p->connected () && p->type () < type)
			type = p->type ();
	}
	if (type != DataType::NIL) {
		/* There has been a connected port (necessarily non-NIL) */
		return type;
	}

	/* Find most likely type among available ports.
	 * The iterator stops before NIL. */
	for (DataType::iterator t = DataType::begin (); t != DataType::end (); ++t) {
		if (io->n_ports ().n (*t) > 0)
			return *t;
	}

	/* No port at all, return the most likely datatype by default */
	return DataType::front ();
}

/*
 * Output port labelling
 *
 * Case 1: Each output has one connection, all connections are to system:playback_%i
 *   out 1 -> system:playback_1
 *   out 2 -> system:playback_2
 *   out 3 -> system:playback_3
 *   Display as: 1/2/3
 *
 * Case 2: Each output has one connection, all connections are to ardour:track_x
 *   out 1 -> ardour:track_x/in 1
 *   out 2 -> ardour:track_x/in 2
 *   Display as: track_x
 *
 * Case 2b: Some outputs are connected, but all connections are to ardour:track_x
 *   out 1 -> ardour:track_x/in 1
 *   out 2 -> N/C
 *   Display as: track_x 1
 *
 * Case 3, 3a:
 *   same as 2, but for I/O Plugins (not routes)
 *
 * Case 4: Each output has one connection, all connections are to Jack client "program x"
 *   out 1 -> program x:foo
 *   out 2 -> program x:foo
 *   Display as: program x
 *  this include internal one to many connections which show as "ardour"
 *
 * Case 4: No connections (Disconnected)
 *   Display as: -
 *
 * Default case (unusual routing):
 *   Display as: *number of connections*
 *
 *
 * Tooltips
 *
 * .-----------------------------------------------.
 * | Mixdown                                       |
 * | out 1 -> ardour:master/in 1, jamin:input/in 1 |
 * | out 2 -> ardour:master/in 2, jamin:input/in 2 |
 * '-----------------------------------------------'
 * .-----------------------------------------------.
 * | Guitar SM58                                   |
 * | Disconnected                                  |
 * '-----------------------------------------------'
 */
void
IOButtonBase::set_label (IOButtonBase& self, ARDOUR::Session& session, std::shared_ptr<ARDOUR::Bundle>& bndl, std::shared_ptr<ARDOUR::IO> io)
{
	ostringstream tooltip;
	ostringstream label;
	bool          have_label = false;

	uint32_t total_connection_count             = 0;
	uint32_t typed_connection_count             = 0;
	bool     each_typed_port_has_one_connection = true;

	DataType dt    = guess_main_type (io);
	bool     input = io->direction () == IO::Input;
	string   arrow = Gtkmm2ext::markup_escape_text (input ? " <- " : " -> ");

	/* Fill in the tooltip. Also count:
	 *  - The total number of connections.
	 *  - The number of main-typed connections.
	 *  - Whether each main-typed port has exactly one connection. */
	if (input) {
		tooltip << string_compose (_("<b>INPUT</b> to %1"),
		                           Gtkmm2ext::markup_escape_text (io->name ()));
	} else {
		tooltip << string_compose (_("<b>OUTPUT</b> from %1"),
		                           Gtkmm2ext::markup_escape_text (io->name ()));
	}

	vector<string> port_connections;

	for (auto const& port : io->ports ()) {
		port_connections.clear ();
		port->get_connections (port_connections);

		uint32_t port_connection_count = 0;

		for (auto const& i : port_connections) {
			++port_connection_count;

			if (port_connection_count == 1) {
				tooltip << endl
				        << Gtkmm2ext::markup_escape_text (port->name ().substr (port->name ().find ("/") + 1));
				tooltip << arrow;
			} else {
				tooltip << ", ";
			}

			tooltip << Gtkmm2ext::markup_escape_text (i);
		}

		total_connection_count += port_connection_count;
		if (port->type () == dt) {
			typed_connection_count += port_connection_count;
			each_typed_port_has_one_connection &= (port_connection_count == 1);
		}
	}

	if (total_connection_count == 0) {
		tooltip << endl
		        << _("Disconnected");
	}

	if (typed_connection_count == 0) {
		label << "-";
		have_label = true;
	}

	/* Are all main-typed channels connected to the same route ? */
	if (!have_label) {
		std::shared_ptr<ARDOUR::RouteList const> routes = session.get_routes ();
		for (auto const& route : *routes) {
			std::shared_ptr<IO> dest_io = input ? route->output () : route->input ();
			if (io->bundle ()->connected_to (dest_io->bundle (), session.engine (), dt, true)) {
				label << Gtkmm2ext::markup_escape_text (route->name ());
				have_label = true;
				route->PropertyChanged.connect (self._bundle_connections, invalidator (self), boost::bind (&IOButtonBase::maybe_update, &self, _1), gui_context ());
				break;
			}

			if (!io->connected_to (dest_io)) {
				continue;
			}

			if (exclusively_connected (dest_io, io, dt, typed_connection_count, route->name (), label)) {
				have_label = true;
				route->PropertyChanged.connect (self._bundle_connections, invalidator (self), boost::bind (&IOButtonBase::maybe_update, &self, _1), gui_context ());
			}
			break;
		}
	}

	/* Are all main-typed channels connected to the same (user) bundle ? */
	if (!have_label) {
		std::shared_ptr<ARDOUR::BundleList const> bundles       = session.bundles ();
		std::shared_ptr<ARDOUR::Port>             ap            = std::dynamic_pointer_cast<ARDOUR::Port> (session.vkbd_output_port ());
		std::string                               vkbd_portname = AudioEngine::instance ()->make_port_name_non_relative (ap->name ());
		for (auto const& bundle : *bundles) {
			if (std::dynamic_pointer_cast<UserBundle> (bundle) == 0) {
				if (!bundle->offers_port (vkbd_portname)) {
					continue;
				}
			}
			if (io->bundle ()->connected_to (bundle, session.engine (), dt, true)) {
				label << Gtkmm2ext::markup_escape_text (bundle->name ());
				have_label = true;
				bndl       = bundle;
				break;
			}
		}
	}

	/* Is each main-typed channel only connected to a physical output ? */
	if (!have_label && each_typed_port_has_one_connection) {
		ostringstream  temp_label;
		vector<string> phys;
		string         playorcapture;
		if (input) {
			session.engine ().get_physical_inputs (dt, phys);
			playorcapture = "capture_";
		} else {
			session.engine ().get_physical_outputs (dt, phys);
			playorcapture = "playback_";
		}
		for (PortSet::iterator port = io->ports ().begin (dt);
		     port != io->ports ().end (dt);
		     ++port) {
			string pn = "";
			for (auto const& s : phys) {
				if (!port->connected_to (s)) {
					continue;
				}
				pn = AudioEngine::instance ()->get_pretty_name_by_name (s);
				if (pn.empty ()) {
					string::size_type start = (s).find (playorcapture);
					if (start != string::npos) {
						pn = (s).substr (start + playorcapture.size ());
					}
				}
				break;
			}

			if (pn.empty ()) {
				temp_label.str (""); /* erase the failed attempt */
				break;
			}
			if (port != io->ports ().begin (dt)) {
				temp_label << "/";
			}
			temp_label << pn;
		}

		if (!temp_label.str ().empty ()) {
			label << temp_label.str ();
			have_label = true;
		}
	}

	/* check for direct connections to I/O Plugins */
	if (!have_label) {
		for (auto const& iop : *session.io_plugs ()) {
			std::shared_ptr<IO> i = input ? iop->output () : iop->input ();
			if (!io->connected_to (i)) {
				continue;
			}

			/* direct 1:1 connection to IOP */
			if (io->bundle ()->connected_to (i->bundle (), session.engine (), dt, true)) {
				label << Gtkmm2ext::markup_escape_text (iop->io_name ());
				have_label = true;
				break;
			}

			if (exclusively_connected (i, io, dt, typed_connection_count, iop->io_name (), label)) {
				have_label = true;
			}
			break;
		}
	}

	// TODO check for internal connection (port_is_mine), do not simply
	// print "ardour" as client name

	/* Is each main-typed channel connected to a single and different port with
	 * the same client name (e.g. another JACK client) ? */
	if (!have_label && each_typed_port_has_one_connection) {
		string         maybe_client = "";
		vector<string> connections;
		for (PortSet::iterator port = io->ports ().begin (dt);
		     port != io->ports ().end (dt);
		     ++port) {
			port_connections.clear ();
			port->get_connections (port_connections);
			if (port_connections.empty ()) {
				continue;
			}
			string connection = port_connections.front ();

			vector<string>::iterator i = connections.begin ();
			while (i != connections.end () && *i != connection) {
				++i;
			}
			if (i != connections.end ()) {
				break; /* duplicate connection */
			}
			connections.push_back (connection);

			connection = connection.substr (0, connection.find (":"));

			if (maybe_client.empty ()) {
				maybe_client = connection;
			}
			if (maybe_client != connection) {
				break;
			}
		}
		if (connections.size () == io->n_ports ().n (dt)) {
			label << Gtkmm2ext::markup_escape_text (maybe_client);
			have_label = true;
		}
	}

	/* Odd configuration */
	if (!have_label) {
		label << "*" << total_connection_count << "*";
	}

	if (total_connection_count > typed_connection_count) {
		label << u8"\u2295"; /* circled plus */
	}

	self.set_text (label.str ());
	set_tooltip (&self, tooltip.str ());
}

/* ****************************************************************************/

IOButton::IOButton (bool input)
	: _input (input)
	, _route_ui (0)
{
	set_text (input ? _("Input") : _("Output"));
	set_name ("mixer strip button");
	set_text_ellipsize (Pango::ELLIPSIZE_MIDDLE);

	signal_button_press_event ().connect (sigc::mem_fun (*this, &IOButton::button_press), false);
	signal_button_release_event ().connect (sigc::mem_fun (*this, &IOButton::button_release), false);
	signal_size_allocate ().connect (sigc::mem_fun (*this, &IOButton::button_resized));
}

void
IOButton::set_route (std::shared_ptr<ARDOUR::Route> rt, RouteUI* routeui)
{
	_connections.drop_connections ();
	_bundle_connections.drop_connections ();

	_route    = rt;
	_route_ui = routeui;

	if (!_route) {
		_route_ui = NULL;
		return;
	}

	AudioEngine::instance ()->PortConnectedOrDisconnected.connect (_connections, invalidator (*this), boost::bind (&IOButton::port_connected_or_disconnected, this, _1, _3), gui_context ());
	AudioEngine::instance ()->PortPrettyNameChanged.connect (_connections, invalidator (*this), boost::bind (&IOButton::port_pretty_name_changed, this, _1), gui_context ());

	io ()->changed.connect (_connections, invalidator (*this), boost::bind (&IOButton::update, this), gui_context ());
	/* We're really only interested in BundleRemoved when connected to that bundle */
	_route->session ().BundleAddedOrRemoved.connect (_connections, invalidator (*this), boost::bind (&IOButton::update, this), gui_context ());

	update ();
}

IOButton::~IOButton ()
{
}

std::shared_ptr<IO>
IOButton::io () const
{
	return _input ? _route->input () : _route->output ();
}

std::shared_ptr<Track>
IOButton::track () const
{
	return std::dynamic_pointer_cast<Track> (_route);
}

void
IOButton::port_pretty_name_changed (std::string pn)
{
	if (io ()->connected_to (pn)) {
		update ();
	}
}

void
IOButton::port_connected_or_disconnected (std::weak_ptr<Port> wa, std::weak_ptr<Port> wb)
{
	if (!_route) {
		return;
	}
	std::shared_ptr<Port> a = wa.lock ();
	std::shared_ptr<Port> b = wb.lock ();

	if ((a && io ()->has_port (a)) || (b && io ()->has_port (b))) {
		update ();
	}
}

void
IOButton::bundle_chosen (std::shared_ptr<ARDOUR::Bundle> c)
{
	if (_input) {
		_route->input ()->connect_ports_to_bundle (c, true, this);
	} else {
		_route->output ()->connect_ports_to_bundle (c, true, true, this);
	}
}

void
IOButton::disconnect ()
{
	io ()->disconnect (this);
}

void
IOButton::add_port (DataType t)
{
	if (io ()->add_port ("", this, t) != 0) {
		ArdourMessageDialog msg (_("It is not possible to add a port here."));
		msg.set_title (_("Cannot add port"));
		msg.run ();
	}
}

void
IOButton::button_resized (Gtk::Allocation& alloc)
{
	set_layout_ellipsize_width (alloc.get_width () * PANGO_SCALE);
}

struct RouteCompareByName {
	bool operator() (std::shared_ptr<Route> a, std::shared_ptr<Route> b)
	{
		return a->name ().compare (b->name ()) < 0;
	}
};

bool
IOButton::button_release (GdkEventButton* ev)
{
	if (!_route || !_route_ui) {
		return false;
	}
	if (ev->button == 3) {
		if (_input) {
			_route_ui->edit_input_configuration ();
		} else {
			_route_ui->edit_output_configuration ();
		}
	}
	return false;
}

bool
IOButton::button_press (GdkEventButton* ev)
{
	using namespace Gtk::Menu_Helpers;

	if (!ARDOUR_UI_UTILS::engine_is_running () || !_route || !_route_ui) {
		return true;
	}

	MenuList& citems = _menu.items ();
	_menu.set_name ("ArdourContextMenu");
	citems.clear ();

	if (_route->session ().actively_recording () && track () && track ()->rec_enable_control ()->get_value ()) {
		return true;
	}

	switch (ev->button) {
		case 3:
			/* don't handle the mouse-down here, parent handles mouse-up if needed. */
			return false;
		case 1:
			break;
		default:
			/* do nothing */
			return true;
	}

	citems.push_back (MenuElem (_("Disconnect"), sigc::mem_fun (*this, &IOButton::disconnect)));
	citems.push_back (SeparatorElem ());
	uint32_t const n_with_separator = citems.size ();

	_menu_bundles.clear ();
	ARDOUR::BundleList                        current = io ()->bundles_connected ();
	std::shared_ptr<ARDOUR::BundleList const> b       = _route->session ().bundles ();

	if (_input) {
		/* give user bundles first chance at being in the menu */
		for (auto const& i : *b) {
			if (std::dynamic_pointer_cast<UserBundle> (i)) {
				maybe_add_bundle_to_menu (i, current);
			}
		}

		for (auto const& i : *b) {
			if (std::dynamic_pointer_cast<UserBundle> (i) == 0) {
				maybe_add_bundle_to_menu (i, current);
			}
		}
	} else {
		/* guess the user-intended main type of the route output */
		DataType intended_type = guess_main_type (_input ? _route->input () : _route->output ());

		/* try adding the master bus first */
		std::shared_ptr<Route> master = _route->session ().master_out ();
		if (master && !_route->is_monitor ()) {
			maybe_add_bundle_to_menu (master->input ()->bundle (), current, intended_type);
		}
	}

	RouteList copy = *_route->session ().get_routes ();
	copy.sort (RouteCompareByName ());

	if (_input) {
		/* other routes outputs */
		for (ARDOUR::RouteList::const_iterator i = copy.begin (); i != copy.end (); ++i) {
			if ((*i)->is_foldbackbus ()) {
				continue;
			}
			if (_route->feeds (*i)) {
				/* do not offer connections that would cause feedback */
				continue;
			}
			maybe_add_bundle_to_menu ((*i)->output ()->bundle (), current);
		}

		for (auto const& iop : *_route->session ().io_plugs ()) {
			if (!iop->is_pre ()) {
				continue;
			}
			maybe_add_bundle_to_menu (iop->output ()->bundle (), current);
		}
	} else {
		DataType intended_type = guess_main_type (_input ? _route->input () : _route->output ());

		/* other routes inputs */
		for (ARDOUR::RouteList::const_iterator i = copy.begin (); i != copy.end (); ++i) {
			if ((*i)->is_foldbackbus () || _route->is_foldbackbus ()) {
				continue;
			}
			if ((*i)->feeds (_route)) {
				/* do not offer connections that would cause feedback */
				continue;
			}
			maybe_add_bundle_to_menu ((*i)->input ()->bundle (), current, intended_type);
		}

		/* then try adding user output bundles, often labeled/grouped physical inputs */
		for (auto const& i : *b) {
			if (std::dynamic_pointer_cast<UserBundle> (i)) {
				maybe_add_bundle_to_menu (i, current, intended_type);
			}
		}

		/* then all other bundles, including physical outs or other software */
		for (auto const& i : *b) {
			if (std::dynamic_pointer_cast<UserBundle> (i) == 0) {
				maybe_add_bundle_to_menu (i, current, intended_type);
			}
		}

		for (auto const& iop : *_route->session ().io_plugs ()) {
			if (iop->is_pre ()) {
				continue;
			}
			maybe_add_bundle_to_menu (iop->input ()->bundle (), current, intended_type);
		}
	}

	if (citems.size () > n_with_separator) {
		citems.push_back (SeparatorElem ());
	}

	if (_input || !ARDOUR::Profile->get_mixbus ()) {
		bool need_separator = false;
		for (DataType::iterator i = DataType::begin (); i != DataType::end (); ++i) {
			if (!io ()->can_add_port (*i)) {
				continue;
			}
			need_separator = true;
			citems.push_back (
			    MenuElem (
			        string_compose (_("Add %1 port"), (*i).to_i18n_string ()),
			        sigc::bind (sigc::mem_fun (*this, &IOButton::add_port), *i)));
		}
		if (need_separator) {
			citems.push_back (SeparatorElem ());
		}
	}

	if (_input) {
		citems.push_back (MenuElem (_("Routing Grid"), sigc::mem_fun (*_route_ui, &RouteUI::edit_input_configuration)));
	} else {
		citems.push_back (MenuElem (_("Routing Grid"), sigc::mem_fun (*_route_ui, &RouteUI::edit_output_configuration)));
	}

	Gtkmm2ext::anchored_menu_popup (&_menu, this, "", 1, ev->time);
	return true;
}

void
IOButton::update ()
{
	std::shared_ptr<ARDOUR::Bundle> bundle;
	_bundle_connections.drop_connections ();

	if (!_route) {
		/* There may still be a signal queued before `set_route (0)` unsets the route
		 * and unsubscribes. invalidation only happens when the button is destroyed. */
		set_text (_input ? _("Input") : _("Output"));
		set_tooltip (this, "");
		return;
	}

	set_label (*this, _route->session (), bundle, _input ? _route->input () : _route->output ());

	if (bundle) {
		bundle->Changed.connect (_bundle_connections, invalidator (*this), boost::bind (&IOButton::update, this), gui_context ());
	}
}

void
IOButton::maybe_add_bundle_to_menu (std::shared_ptr<Bundle> b, ARDOUR::BundleList const& /*current*/, ARDOUR::DataType type)
{
	using namespace Gtk::Menu_Helpers;

	if (_input) {
		/* The bundle should be a source with matching inputs, but not ours */
		if (b->ports_are_outputs () == false || b->nchannels () != _route->n_inputs () || *b == *_route->output ()->bundle ()) {
			return;
		}
	} else {
		/* The bundle should be sink, but not ours */
		if (b->ports_are_inputs () == false || *b == *_route->input ()->bundle ()) {
			return;
		}

		/* Don't add the monitor input unless we are Master */
		std::shared_ptr<Route> monitor = _route->session ().monitor_out ();
		if ((!_route->is_master ()) && monitor && b->has_same_ports (monitor->input ()->bundle ())) {
			return;
		}

		/* It should either match exactly our outputs (if |type| is DataType::NIL)
		 * or have the same number of |type| channels than our outputs. */
		if (type == DataType::NIL) {
			if (b->nchannels () != _route->n_outputs ()) {
				return;
			}
		} else {
			if (b->nchannels ().n (type) != _route->n_outputs ().n (type))
				return;
		}
	}

	/* Avoid adding duplicates */
	list<std::shared_ptr<Bundle>>::iterator i = _menu_bundles.begin ();
	while (i != _menu_bundles.end () && b->has_same_ports (*i) == false) {
		++i;
	}
	if (i != _menu_bundles.end ()) {
		return;
	}

	/* Finally add the bundle to the menu */
	_menu_bundles.push_back (b);

	MenuList& citems = _menu.items ();
	citems.push_back (MenuElemNoMnemonic (b->name (), sigc::bind (sigc::mem_fun (*this, &IOButton::bundle_chosen), b)));
}
