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

#ifndef __ardour_soloable_h__
#define __ardour_soloable_h__

#include <stdint.h>

namespace ARDOUR {

class Soloable {
    public:
	virtual ~Soloable() {}

	virtual void push_solo_upstream (int32_t delta) = 0;
	virtual void push_solo_isolate_upstream (int32_t delta) = 0;
	virtual bool is_safe () const = 0;
	virtual bool can_solo () const = 0;
};

} /* namespace */

#endif /* __ardour_soloable_h__ */
