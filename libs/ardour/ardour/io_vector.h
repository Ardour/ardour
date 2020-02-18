/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_io_vector_h__
#define __ardour_io_vector_h__

#include <vector>
#include <boost/shared_ptr.hpp>
#include "ardour/io.h"

namespace ARDOUR {

class IOVector : public std::vector<boost::weak_ptr<ARDOUR::IO> >
{
public:
#if 0 // unused -- for future reference
	bool connected_to (const IOVector& other) const {
		for (IOVector::const_iterator i = other.begin(); i != other.end(); ++i) {
			boost::shared_ptr<const IO> io = i->lock();
			if (!io) continue;
			if (connected_to (io)) {
				return true;
			}
		}
		return false;
	}

	bool connected_to (boost::shared_ptr<const IO> other) const {
		for (IOVector::const_iterator i = begin(); i != end(); ++i) {
			boost::shared_ptr<const IO> io = i->lock();
			if (!io) continue;
			if (io->connected_to (other)) {
				return true;
			}
		}
		return false;
	}
#endif

	bool fed_by (boost::shared_ptr<const IO> other) const {
		for (IOVector::const_iterator i = begin(); i != end(); ++i) {
			boost::shared_ptr<const IO> io = i->lock();
			if (!io) continue;
			if (other->connected_to (io)) {
				return true;
			}
		}
		return false;
	}
};

} // namespace ARDOUR


#endif
