/*
 * Copyright (C) 2007-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_profile_h__
#define __ardour_profile_h__

#include <boost/dynamic_bitset.hpp>
#include <stdint.h>

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class LIBARDOUR_API RuntimeProfile {
public:
	enum Element {
		SmallScreen,
		SinglePackage,
		Mixbus,
		LastElement,
	};

    RuntimeProfile() { bits.resize (LastElement); }
    ~RuntimeProfile() {}

    void set_small_screen() { bits[SmallScreen] = true; }
    bool get_small_screen() const { return bits[SmallScreen]; }

    bool get_mixbus() const { return bits[Mixbus]; }
    void set_mixbus() { bits[Mixbus] = true; }

    void set_single_package () { bits[SinglePackage] = true; }
    bool get_single_package () const { return bits[SinglePackage]; }

private:
    boost::dynamic_bitset<uint64_t> bits;

};

LIBARDOUR_API extern RuntimeProfile* Profile;

}; // namespace ARDOUR

#endif /* __ardour_profile_h__ */
