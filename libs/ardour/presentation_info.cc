/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Len Ovens <len@ovenwerks.net>
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

#include <sstream>
#include <typeinfo>

#include <cassert>

#include "pbd/debug.h"
#include "pbd/enum_convert.h"
#include "pbd/enumwriter.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/xml++.h"

#include "ardour/presentation_info.h"
#include "ardour/selection.h"

#include "pbd/i18n.h"

namespace PBD {
	DEFINE_ENUM_CONVERT(ARDOUR::PresentationInfo::Flag);
}

using namespace ARDOUR;
using namespace PBD;
using std::string;

string PresentationInfo::state_node_name = X_("PresentationInfo");

PBD::Signal1<void,PropertyChange const &> PresentationInfo::Change;
Glib::Threads::Mutex PresentationInfo::static_signal_lock;
GATOMIC_QUAL gint PresentationInfo::_change_signal_suspended = 0;
PBD::PropertyChange PresentationInfo::_pending_static_changes;
int PresentationInfo::selection_counter= 0;

namespace ARDOUR {
	namespace Properties {
		PBD::PropertyDescriptor<bool>     selected;
		PBD::PropertyDescriptor<uint32_t> order;
		PBD::PropertyDescriptor<uint32_t> color;
		PBD::PropertyDescriptor<bool>     trigger_track;
	}
}

void
PresentationInfo::suspend_change_signal ()
{
	g_atomic_int_add (&_change_signal_suspended, 1);
}

void
PresentationInfo::unsuspend_change_signal ()
{
	Glib::Threads::Mutex::Lock lm (static_signal_lock);

	if (g_atomic_int_get (&_change_signal_suspended) == 1) {

		/* atomically grab currently pending flags */

		PropertyChange pc = _pending_static_changes;
		_pending_static_changes.clear ();

		if (!pc.empty()) {

			/* emit the signal with further emissions still blocked
			 * by _change_signal_suspended, but not by the lock.
			 *
			 * This means that if the handlers modify other PI
			 * states, the signal for that won't be sent while they
			 * are handling the current signal.
			 */
			lm.release ();
			Change (pc); /* EMIT SIGNAL */
			lm.acquire ();
		}
	}

	g_atomic_int_add (&_change_signal_suspended, -1);
}

void
PresentationInfo::send_static_change (const PropertyChange& what_changed)
{
	if (what_changed.empty()) {
		return;
	}


	if (g_atomic_int_get (&_change_signal_suspended)) {
		Glib::Threads::Mutex::Lock lm (static_signal_lock);
		_pending_static_changes.add (what_changed);
		return;
	}

	Change (what_changed);
}

const PresentationInfo::order_t PresentationInfo::max_order = UINT32_MAX;
const PresentationInfo::Flag PresentationInfo::Bus = PresentationInfo::Flag (PresentationInfo::AudioBus|PresentationInfo::MidiBus);
const PresentationInfo::Flag PresentationInfo::Track = PresentationInfo::Flag (PresentationInfo::AudioTrack|PresentationInfo::MidiTrack);
const PresentationInfo::Flag PresentationInfo::Route = PresentationInfo::Flag (PresentationInfo::Bus|PresentationInfo::Track);
const PresentationInfo::Flag PresentationInfo::AllRoutes = PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::MasterOut|PresentationInfo::MonitorOut|PresentationInfo::FoldbackBus);
const PresentationInfo::Flag PresentationInfo::MixerRoutes = PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::MasterOut|PresentationInfo::MonitorOut);
const PresentationInfo::Flag PresentationInfo::AllStripables = PresentationInfo::Flag (PresentationInfo::AllRoutes|PresentationInfo::VCA);
const PresentationInfo::Flag PresentationInfo::MixerStripables = PresentationInfo::Flag (PresentationInfo::MixerRoutes|PresentationInfo::VCA);
const PresentationInfo::Flag PresentationInfo::MidiIndicatingFlags = PresentationInfo::Flag (PresentationInfo::MidiTrack|PresentationInfo::MidiBus);

void
PresentationInfo::make_property_quarks ()
{
	Properties::selected.property_id = g_quark_from_static_string (X_("selected"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for selected = %1\n", Properties::selected.property_id));
	Properties::color.property_id = g_quark_from_static_string (X_("color"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for color = %1\n", Properties::color.property_id));
	Properties::order.property_id = g_quark_from_static_string (X_("order"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for order = %1\n", Properties::order.property_id));
}

PresentationInfo::PresentationInfo (Flag f)
	: _order (0)
	, _flags (Flag (f & ~OrderSet))
	, _color (0)
{
	/* OrderSet is not set */
}

PresentationInfo::PresentationInfo (order_t o, Flag f)
	: _order (o)
	, _flags (Flag (f | OrderSet))
	, _color (0)
{
	/* OrderSet is set */
}
PresentationInfo::PresentationInfo (PresentationInfo const& other)
	: PBD::Stateful ()
	, _order (other.order())
	, _flags (other.flags())
	, _color (other.color())
{
}

XMLNode&
PresentationInfo::get_state ()
{
	XMLNode* node = new XMLNode (state_node_name);
	node->set_property ("order", _order);
	node->set_property ("flags", _flags);
	node->set_property ("color", _color);

	return *node;
}

int
PresentationInfo::set_state (XMLNode const& node, int /* version */)
{
	if (node.name() != state_node_name) {
		return -1;
	}

	PropertyChange pc;

	order_t o;
	if (node.get_property (X_("order"), o)) {
		if (o != _order) {
			pc.add (Properties::order);
			_order = o;
		}
		_order = o; // huh?
	}

	Flag f;
	if (node.get_property (X_("flags"), f)) {
		if ((f&Hidden) != (_flags&Hidden)) {
			pc.add (Properties::hidden);
		}
		if ((f&TriggerTrack) != (_flags&TriggerTrack)) {
			pc.add (Properties::trigger_track);
		}
		_flags = f;
	}

	color_t c;
	if (node.get_property (X_("color"), c)) {
		if (c != _color) {
			pc.add (Properties::color);
			_color = c;
		}
	}

	send_change (PropertyChange (pc));

	return 0;

}

PresentationInfo::Flag
PresentationInfo::get_flags (XMLNode const& node)
{
	XMLNodeList nlist = node.children ();

	for (XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); ++niter){
		XMLNode* child = *niter;

		if (child->name() == PresentationInfo::state_node_name) {
			Flag f;
			if (child->get_property (X_("flags"), f)) {
				return f;
			}
		}
	}
	return Flag (0);
}

PresentationInfo::Flag
PresentationInfo::get_flags2X3X (XMLNode const& node)
{
	/* Ardour 2.x and session-format 300x used <Route flags="MasterOut" .. /> */
	Flag f;
	if (node.get_property (X_("flags"), f)) {
		return f;
	}
	return get_flags (node);
}

void
PresentationInfo::set_color (PresentationInfo::color_t c)
{
	if (c != _color) {
		_color = c;
		send_change (PropertyChange (Properties::color));
		send_static_change (PropertyChange (Properties::color));
	}
}

bool
PresentationInfo::color_set () const
{
	/* all RGBA values zero? not set.
	 *
	 * this is heuristic, but it is fairly realistic. who will ever set
	 * a color to completely transparent black? only the constructor ..
	 */
	return _color != 0;
}

void
PresentationInfo::set_hidden (bool yn)
{
	if (yn != hidden()) {

		if (yn) {
			_flags = Flag (_flags | Hidden);
		} else {
			_flags = Flag (_flags & ~Hidden);
		}

		send_change (PropertyChange (Properties::hidden));
		send_static_change (PropertyChange (Properties::hidden));
	}
}

void
PresentationInfo::set_order (order_t order)
{
	_flags = Flag (_flags|OrderSet);

	if (order != _order) {
		_order = order;
		send_change (PropertyChange (Properties::order));
		send_static_change (PropertyChange (Properties::order));
	}
}

void
PresentationInfo::set_trigger_track (bool yn)
{
	if (yn != trigger_track ()) {

		if (yn) {
			_flags = Flag (_flags | TriggerTrack);
		} else {
			_flags = Flag (_flags & ~TriggerTrack);
		}

		send_change (PropertyChange (Properties::trigger_track));
		send_static_change (PropertyChange (Properties::trigger_track));
	}
}

PresentationInfo&
PresentationInfo::operator= (PresentationInfo const& other)
{
	if (this != &other) {
		_order = other.order();
		_flags = other.flags();
		_color = other.color();
	}

	return *this;
}

std::ostream&
operator<<(std::ostream& o, ARDOUR::PresentationInfo const& pi)
{
	return o << pi.order() << '/' << enum_2_string (pi.flags()) << '/' << pi.color();
}
