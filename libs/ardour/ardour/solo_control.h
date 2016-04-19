/*
    Copyright (C) 2016 Paul Davis

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_solo_control_h__
#define __ardour_solo_control_h__

#include <string>

#include <boost/shared_ptr.hpp>

#include "ardour/slavable_automation_control.h"
#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class Session;
class Soloable;
class Muteable;

class LIBARDOUR_API SoloControl : public SlavableAutomationControl
{
  public:
	SoloControl (Session& session, std::string const & name, Soloable& soloable, Muteable& m);

	double get_value () const;

	/* Export additional API so that objects that only get access
	 * to a Controllable/AutomationControl can do more fine-grained
	 * operations with respect to solo. Obviously, they would need
	 * to dynamic_cast<SoloControl> first.
	 *
	 * Solo state is not representable by a single scalar value,
	 * so set_value() and get_value() is not enough.
	 *
	 * This means that the Controllable is technically
	 * asymmetric. It is possible to call ::set_value (0.0) to
	 * disable (self)solo, and then call ::get_value() and get a
	 * return of 1.0 because the control is soloed by
	 * upstream/downstream or a master.
	 */

	void mod_solo_by_others_upstream (int32_t delta);
	void mod_solo_by_others_downstream (int32_t delta);

	/* API to check different aspects of solo substate
	 */

	bool soloed_by_others () const {
		return _soloed_by_others_downstream || _soloed_by_others_downstream;
	}
	uint32_t soloed_by_others_upstream () const {
		return _soloed_by_others_upstream;
	}
	uint32_t soloed_by_others_downstream () const {
		return _soloed_by_others_downstream;
	}
	bool self_soloed () const {
		return _self_solo;
	}
	bool soloed() const { return self_soloed() || soloed_by_others(); }

	void clear_all_solo_state ();

	int set_state (XMLNode const&, int);
	XMLNode& get_state ();

  protected:
	void actually_set_value (double, PBD::Controllable::GroupControlDisposition group_override);

  private:
	Soloable&      _soloable;
	Muteable&      _muteable;
	bool           _self_solo;
	uint32_t       _soloed_by_others_upstream;
	uint32_t       _soloed_by_others_downstream;

	void set_self_solo (bool yn);
	void set_mute_master_solo ();
};

} /* namespace */

#endif /* __libardour_solo_control_h__ */
