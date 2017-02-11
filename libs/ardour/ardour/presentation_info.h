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

#include "pbd/signals.h"
#include "pbd/stateful.h"
#include "pbd/properties.h"

#include "ardour/libardour_visibility.h"

class XMLNode;

namespace ARDOUR {

namespace Properties {
	LIBARDOUR_API extern PBD::PropertyDescriptor<uint32_t> order;
	LIBARDOUR_API extern PBD::PropertyDescriptor<uint32_t> color;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> selected;
	/* we use this; declared in region.cc */
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> hidden;
}

class LIBARDOUR_API PresentationInfo : public PBD::Stateful
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
		MasterOut = 0x20,
		MonitorOut = 0x40,
		Auditioner = 0x80,
		/* These are for sharing Stripable states between the GUI and other
		 * user interfaces/control surfaces
		 */
		Selected = 0x100,
		Hidden = 0x200,
		/* single bit indicates that the group order is set */
		OrderSet = 0x400,

		/* special mask to delect out "state" bits */
		StatusMask = (Selected|Hidden),
		/* special mask to delect select type bits */
		TypeMask = (AudioBus|AudioTrack|MidiTrack|MidiBus|VCA|MasterOut|MonitorOut|Auditioner)
	};

	static const Flag AllStripables; /* mask to use for any route or VCA (but not auditioner) */
	static const Flag AllRoutes; /* mask to use for any route include master+monitor, but not auditioner */
	static const Flag Route;     /* mask for any route (bus or track */
	static const Flag Track;     /* mask to use for any track */
	static const Flag Bus;       /* mask to use for any bus */

	typedef uint32_t order_t;
	typedef uint32_t color_t;

	PresentationInfo (Flag f);
	PresentationInfo (order_t o, Flag f);
	PresentationInfo (PresentationInfo const &);

	static const order_t max_order;

	PresentationInfo::Flag flags() const { return _flags; }
	order_t  order() const { return _order; }
	color_t  color() const { return _color; }

	bool color_set() const;

	void set_color (color_t);
	void set_selected (bool yn);
	void set_hidden (bool yn);
	void set_flags (Flag f) { _flags = f; }

	bool order_set() const { return _flags & OrderSet; }

	bool hidden() const { return _flags & Hidden; }
	bool selected() const { return _flags & Selected; }
	bool special() const { return _flags & (MasterOut|MonitorOut|Auditioner); }

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
			/* any kind of route, but not master, monitor in
			   or auditioner.
			 */
			return true;
		}

		if (f == AllRoutes && (_flags & AllRoutes)) {
			/* any kind of route, but not auditioner. Ask for that
			   specifically.
			*/
			return true;
		}

		if (f == AllStripables && (_flags & AllStripables)) {
			/* any kind of stripable, but not auditioner. Ask for that
			   specifically.
			*/
			return true;
		}

		/* check for any matching type bits.
		 *
		 * Do comparisoon without status mask or order set bits - we
		 * already checked that above.
		 */

		return ((f & TypeMask) & _flags);
	}

	int set_state (XMLNode const&, int);
	XMLNode& get_state ();

	bool operator==(PresentationInfo const& other) {
		return (_order == other.order()) && (_flags == other.flags());
	}

	bool operator!=(PresentationInfo const& other) {
		return (_order != other.order()) || (_flags != other.flags());
	}

	PresentationInfo& operator= (PresentationInfo const& other);

	static Flag get_flags (XMLNode const& node);
	static std::string state_node_name;

	/* for things concerned about *any* PresentationInfo.
	 */

	static PBD::Signal1<void,PBD::PropertyChange const &> Change;

	static void make_property_quarks ();

  protected:
	friend class ChangeSuspender;
	static void suspend_change_signal ();
	static void unsuspend_change_signal ();

  public:
	class ChangeSuspender {
          public:
		ChangeSuspender() {
			PresentationInfo::suspend_change_signal ();
		}
		~ChangeSuspender() {
			PresentationInfo::unsuspend_change_signal ();
		}
	};

  protected:
	friend class Stripable;
	void set_order (order_t order);

  private:
	order_t _order;
	Flag    _flags;
	color_t _color;

	static PBD::PropertyChange _pending_static_changes;
	static Glib::Threads::Mutex static_signal_lock;
	static int _change_signal_suspended;
	static void send_static_change (const PBD::PropertyChange&);
};

}

std::ostream& operator<<(std::ostream& o, ARDOUR::PresentationInfo const& rid);

#endif /* __libardour_presentation_info_h__ */
