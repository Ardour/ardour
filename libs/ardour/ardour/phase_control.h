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

#ifndef __ardour_phase_control_h__
#define __ardour_phase_control_h__

#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/dynamic_bitset.hpp>

#include "ardour/slavable_automation_control.h"
#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class Session;

/* Note that PhaseControl is not Slavable. There's no particular reason for
 * this, it could be changed at any time. But it seems useless.
 */

class LIBARDOUR_API PhaseControl : public AutomationControl
{
  public:
	PhaseControl (Session& session, std::string const & name, Temporal::TimeDomain);

	/* There are two approaches to designing/using a PhaseControl. One is
	 * to have one such control for every channel of the control's
	 * owner. The other is to have a single control which manages all
	 * channels. For now (Spring 2016) we're using the second design.
	 */

	void set_phase_invert (uint32_t, bool yn);
	void set_phase_invert (boost::dynamic_bitset<>);
	bool inverted (uint32_t chn) const { return _phase_invert[chn]; }

	bool none () const { return !_phase_invert.any(); }
	bool any() const { return _phase_invert.any(); }
	uint64_t count() const { return _phase_invert.count(); }
	uint64_t size() const { return _phase_invert.size(); }
	void resize (uint32_t);

	int set_state (XMLNode const&, int);
	XMLNode& get_state ();

  protected:
	void actually_set_value (double, PBD::Controllable::GroupControlDisposition group_override);

  private:
	boost::dynamic_bitset<> _phase_invert;
};

} /* namespace */

#endif /* __libardour_phase_control_h__ */
