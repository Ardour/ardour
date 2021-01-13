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

#ifndef __ardour_solo_safe_control_h__
#define __ardour_solo_safe_control_h__

#include <string>

#include "ardour/slavable_automation_control.h"

#include "ardour/libardour_visibility.h"

class XMLNode;

namespace ARDOUR {

class Session;

class LIBARDOUR_API SoloSafeControl : public SlavableAutomationControl
{
  public:
	SoloSafeControl (Session& session, std::string const & name, Temporal::TimeDomain);

	double get_value () const;

	bool solo_safe() const { return _solo_safe; }

	int set_state (XMLNode const&, int);
	XMLNode& get_state ();

  protected:
	void actually_set_value (double, PBD::Controllable::GroupControlDisposition group_override);

  private:
	bool _solo_safe;
};

} /* namespace */

#endif /* __libardour_solo_safe_control_h__ */
