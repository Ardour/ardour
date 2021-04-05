/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_readonly_control_h__
#define __ardour_readonly_control_h__

#include <boost/weak_ptr.hpp>

#include "ardour/parameter_descriptor.h"
#include "evoral/Parameter.h"

namespace ARDOUR {

class LIBARDOUR_API HasReadableCtrl
{
public:
	virtual ~HasReadableCtrl() {}
	virtual float       get_parameter (uint32_t which) const = 0;
	virtual std::string describe_parameter (Evoral::Parameter) = 0;
};

class LIBARDOUR_API ReadOnlyControl : public PBD::Destructible
{
public:
	ReadOnlyControl (HasReadableCtrl*, const ParameterDescriptor&, uint32_t pnum);

	double get_parameter () const;
	std::string describe_parameter ();
	const ParameterDescriptor& desc() const { return _desc; }

private:
	HasReadableCtrl*          _ctrl;
	const ParameterDescriptor _desc;
	uint32_t                  _parameter_num;
};

} // namespace ARDOUR

#endif /* __ardour_readonly_control_h__ */
