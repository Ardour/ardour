/*
    Copyright (C) 2000 Paul Davis 

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

    $Id$
*/

#ifndef __playlist_const_buffer_h__ 
#define __playlist_const_buffer_h__

#include <string>
#include <cstdlib>

#include "edl.h"

namespace EDL {

class ConstSource : public Source {
  public:
	ConstSource (const gchar *id) {
		_type = Source::Const;
		value = strtod (id, 0);
		strncpy (idstr, id, 15);
		idstr[15] = '\0';
	}
	
	const gchar * const id() { return idstr; }

	uint32_t length() { return ~0U; }

	uint32_t read (Source::Data *dst, uint32_t start, uint32_t cnt) {
		uint32_t n = cnt;
		while (n--) *dst++ = value;
		return cnt;
	}
	void peak (guint8 *max, guint8 *min, uint32_t start, uint32_t cnt) {
		*max = *min = (guint8) value;
	}

  private:
	Source::Data value;
	gchar idstr[16];
};

}; /* namespace EDL */

#endif /* __playlist_const_buffer_h__ */
