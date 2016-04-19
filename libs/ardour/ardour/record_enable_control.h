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

#ifndef __ardour_record_enable_control_h__
#define __ardour_record_enable_control_h__

#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/dynamic_bitset.hpp>

#include "ardour/slavable_automation_control.h"
#include "ardour/recordable.h"

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class Session;

class LIBARDOUR_API RecordEnableControl : public SlavableAutomationControl
{
  public:
	RecordEnableControl (Session& session, std::string const & name, Recordable& m);
	~RecordEnableControl() {}

	/* Most (Slavable)AutomationControls do not override this, but we need
	 * to in order to prepare the Recordable for a change that will happen
	 * subsequently, in a realtime context. So the change is divided into
	 * two parts: the non-RT preparation, executed inside ::set_value(),
	 * then the second RT part.
	 */

	void set_value (double, PBD::Controllable::GroupControlDisposition);

  protected:
	void actually_set_value (double val, Controllable::GroupControlDisposition gcd);

  private:
	Recordable& _recordable;
};

} /* namespace */

#endif /* __libardour_record_enable_control_h__ */
