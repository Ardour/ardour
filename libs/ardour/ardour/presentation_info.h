/*
    Copyright (C) 2016 Paul Davis

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

#ifndef __libardour_presentation_info_h__
#define __libardour_presentation_info_h__

#include <iostream>
#include <string>

#include <stdint.h>

#include "ardour/libardour_visibility.h"

class XMLNode;

namespace ARDOUR {

class LIBARDOUR_API PresentationInfo
{
  public:

	/* a PresentationInfo object exists to share information between
	 * different user interfaces (e.g. GUI and a Mackie Control surface)
	 * about:
	 *
	 *     - ordering
	 *     - selection status
	 *     - visibility
	 *     - object identity
	 *
	 * ORDERING
	 *
	 * One UI takes control of ordering by setting the "order" value for
	 * the PresentationInfo component of every Stripable object. In Ardour,
	 * this is done by the GUI (mostly because it is very hard for the user
	 * to re-order things on a control surface).
	 *
	 * Ordering is a complex beast, however. Different user interfaces may
	 * display things in different ways. For example, the GUI of Ardour
	 * allows the user to mix busses in between tracks. A control surface
	 * may do the same, but may also allow the user to press a button that
	 * makes it show only busses, or only MIDI tracks. At that point, the
	 * ordering on the surface differs from the ordering in the GUI.
	 *
	 * The ordering is given via a combination of an object type and a
	 * simple numeric position within that type. The object types at this
	 * time are:
	 *
	 *     Route
	 *        - object has inputs and outputs; processes data
	 *     Output
	 *        - Route used to connect to outside the application (MasterOut, MonitorOut)
	 *     Special
	 *        - special type of Route (e.g. Auditioner)
	 *     VCA
	 *        - no data flows through; control only
	 *
	 * Objects with a numeric order of zero are considered unsorted. This
	 * applies (for now) to special objects such as the master out,
	 * auditioner and monitor out.  The rationale here is that the GUI
	 * presents these objects in special ways, rather than as part of some
	 * (potentially) re-orderable container. The same is true for hardware
	 * surfaces, where the master fader (for instance) is typically
	 * separate and distinct from anything else.
	 *
	 * There are several pathways for the order being set:
	 *
	 *   - object created during session loading from XML
	 *           - numeric order will be set during ::set_state(), based on
	 *           - type will be set during ctor call
	 *
	 *   - object created in response to user request
	 *		- numeric order will be set by Session, before adding
	 *		     to container.
	 *		- type set during ctor call
	 *
	 *
	 * OBJECT IDENTITY
	 *
	 * Control surfaces/protocols often need to be able to get a handle on
	 * an object identified only abstractly, such as the "5th audio track"
	 * or "the master out". A PresentationInfo object uniquely identifies
	 * all objects in this way through the combination of its _order member
	 * and part of its _flags member. The _flags member identifies the type
	 * of object, as well as selection/hidden status. The type may never
	 * change after construction (not strictly the constructor itself, but
	 * a more generalized notion of construction, as in "ready to use").
	 *
	 * SELECTION
	 *
	 * When an object is selected, its _flags member will have the Selected
	 * bit set.
	 *
	 * VISIBILITY
	 *
	 * When an object is hidden, its _flags member will have the Hidden
	 * bit set.
	 *
	 *
	 */


	enum Flag {
		/* Type information */
		AudioTrack = 0x1,
		MidiTrack = 0x2,
		AudioBus = 0x4,
		MidiBus = 0x8,
		VCA = 0x10,

		/* These need to be at the high end */
		MasterOut = 0x800,
		MonitorOut = 0x1000,
		Auditioner = 0x2000,

		/* These are for sharing Stripable states between the GUI and other
		 * user interfaces/control surfaces
		 */
		Selected = 0x4000,
		Hidden = 0x8000,

		/* single bit indicates that the group order is set */
		GroupOrderSet = 0x100000000,

		/* Masks */

		GroupMask = (AudioTrack|MidiTrack|AudioBus|MidiBus|VCA),
		SpecialMask = (MasterOut|MonitorOut|Auditioner),
		StatusMask = (Selected|Hidden),
	};

	static const Flag Route;
	static const Flag Track;
	static const Flag Bus;

	typedef uint32_t order_t;
	typedef uint64_t global_order_t;

	PresentationInfo (Flag f) : _order (0), _flags (Flag (f & ~GroupOrderSet)) { /* GroupOrderSet is not set */ }
	PresentationInfo (order_t o, Flag f) : _order (o), _flags (Flag (f | GroupOrderSet)) { /* GroupOrderSet is set */ }

	static const order_t max_order;

	order_t  group_order() const { return _order; }
	global_order_t global_order () const {
		if (_flags & Route) {

			/* set all bits related to Route so that all Routes
			   sort together, with order() in the range of
			   64424509440..68719476735

			   Consider the following arrangement:

			   Track   1
			   Bus     1
			   Track   2
			   ---------
			   VCA     1
			   ---------
			   Master
			   ---------
			   Monitor

			   these translate into the following

			   _order  |  _flags            | order()
			   --------------------------------------
			   1       |   0x1   AudioTrack | ((0x1|0x2|0x4|0x8)<<32)|1 = 64424509441
			   2       |   0x2   AudioBus   | ((0x1|0x2|0x4|0x8)<<32)|2 = 64424509442
			   3       |   0x1   AudioTrack | ((0x1|0x2|0x4|0x8)<<32)|3 = 64424509443

			   1       |   0x10  VCA        | ((0x10)<<32)|1 = 68719476737

			   0       |   0x800 Master     | (0x800<<32) = 8796093022208

			   0       |   0x1000 Monitor   | (0x1000<<32) = 17592186044416

			*/

			return (((global_order_t) (_flags | Route)) << sizeof(order_t)) | _order;
		} else {
			return (((global_order_t) _flags) << sizeof(order_t)) | _order;
		}
	}

	PresentationInfo::Flag flags() const { return _flags; }

	bool order_set() const { return _order != 0; }

	/* these objects have no defined order */
	bool special () const { return _flags & SpecialMask; }

	/* detect group order set/not set */
	bool unordered() const { return !(_flags & GroupOrderSet); }
	bool ordered() const { return _flags & GroupOrderSet; }

	void set_flag (PresentationInfo::Flag f) {
		_flags = PresentationInfo::Flag (_flags | f);
	}

	void unset_flag (PresentationInfo::Flag f) {
		_flags = PresentationInfo::Flag (_flags & ~f);
	}

	void set_flags (Flag f) {
		_flags = f;
	}

	bool flag_match (Flag f) const {
		/* no flags, match all */

		if (f == Flag (0)) {
			return true;
		}

		if (f & StatusMask) {
			/* status bits set, must match them */
			if ((_flags & StatusMask) != (f & StatusMask)) {
				return false;
			}
		}

		/* Generic flags in f, match the right stuff */

		if (f == Bus && (_flags & Bus)) {
			/* some kind of bus */
			return true;
		}
		if (f == Track && (_flags & Track)) {
			/* some kind of track */
			return true;
		}
		if (f == Route && (_flags & Route)) {
			/* any kind of route */
			return true;
		}

		return f == _flags;
	}

	std::string to_string () const;

	uint64_t to_integer () const {
		return ((uint64_t) _flags << sizeof(order_t)) | _order;
	}

	bool operator< (PresentationInfo const& other) const {
		return global_order() < other.global_order();
	}

	PresentationInfo& operator= (std::string const& str) {
		parse (str);
		return *this;
	}

	bool match (PresentationInfo const& other) const {
		return (_order == other.group_order()) && flag_match (other.flags());
	}

	bool operator==(PresentationInfo const& other) {
		return (_order == other.group_order()) && (_flags == other.flags());
	}

	bool operator!=(PresentationInfo const& other) {
		return (_order != other.group_order()) || (_flags != other.flags());
	}

	static Flag get_flags (XMLNode const& node);

  protected:
	friend class Stripable;
	void set_group_order (order_t order) { _order = order; _flags = Flag (_flags|GroupOrderSet); }

  private:
	order_t _order;
	Flag    _flags;

	PresentationInfo (std::string const & str);
	int parse (std::string const&);
	int parse (order_t, Flag f);
};

}

std::ostream& operator<<(std::ostream& o, ARDOUR::PresentationInfo const& rid);

#endif /* __libardour_presentation_info_h__ */
