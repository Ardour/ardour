/*
    Copyright (C) 2012 Paul Davis 

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

#ifndef __ardour_readable_h__
#define __ardour_readable_h__

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

class LIBARDOUR_API Readable {
  public:
	Readable () {}
	virtual ~Readable() {}

	virtual framecnt_t read (Sample*, framepos_t pos, framecnt_t cnt, int channel) const = 0;
	virtual framecnt_t readable_length() const = 0;
	virtual uint32_t  n_channels () const = 0;
};

}

#endif /* __ardour_readable_h__ */
