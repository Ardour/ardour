/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_solo_isolate_control_h__
#define __ardour_solo_isolate_control_h__

#include <string>

#include <boost/shared_ptr.hpp>

#include "ardour/slavable_automation_control.h"
#include "ardour/libardour_visibility.h"

class XMLNode;

namespace ARDOUR {

class Session;
class Soloable;
class Muteable;

class LIBARDOUR_API SoloIsolateControl : public SlavableAutomationControl
{
  public:
	SoloIsolateControl (Session& session, std::string const & name, Soloable& soloable, Temporal::TimeDomain);

	double get_value () const;

	/* Export additional API so that objects that only get access
	 * to a Controllable/AutomationControl can do more fine-grained
	 * operations with respect to solo isolate. Obviously, they would need
	 * to dynamic_cast<SoloControl> first.
	 *
	 * Solo Isolate state is not representable by a single scalar value,
	 * so set_value() and get_value() is not enough.
	 *
	 * This means that the Controllable is technically
	 * asymmetric. It is possible to call ::set_value (0.0) to
	 * disable (self)solo, and then call ::get_value() and get a
	 * return of 1.0 because the control is isolated by
	 * upstream/downstream or a master.
	 */

	void mod_solo_isolated_by_upstream (int32_t delta);

	/* API to check different aspects of solo isolate substate
	 */

	uint32_t solo_isolated_by_upstream () const {
		return _solo_isolated_by_upstream;
	}
	bool self_solo_isolated () const {
		return _solo_isolated;
	}
	bool solo_isolated() const { return self_solo_isolated() || solo_isolated_by_upstream(); }

	int set_state (XMLNode const&, int);
	XMLNode& get_state ();

  protected:
	void master_changed (bool from_self, PBD::Controllable::GroupControlDisposition gcd, boost::weak_ptr<AutomationControl>);
	void actually_set_value (double, PBD::Controllable::GroupControlDisposition group_override);

  private:
	Soloable&      _soloable;
	bool           _solo_isolated;
	uint32_t       _solo_isolated_by_upstream;

	void set_solo_isolated (bool yn, Controllable::GroupControlDisposition group_override);

};

} /* namespace */

#endif /* __libardour_solo_isolate_control_h__ */
