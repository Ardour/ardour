/*
    Copyright (C) 1999-2009 Paul Davis

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

#ifndef __ardour_latent_h__
#define __ardour_latent_h__

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

class LIBARDOUR_API Latent {
  public:
	Latent() : _user_latency (0) {}
	virtual ~Latent() {}

	virtual framecnt_t signal_latency() const = 0;
	framecnt_t user_latency () const { return _user_latency; }

	framecnt_t effective_latency() const {
		if (_user_latency) {
			return _user_latency;
		} else {
			return signal_latency ();
		}
	}

	virtual void set_user_latency (framecnt_t val) { _user_latency = val; }

  protected:
	framecnt_t           _user_latency;
};

}

#endif /* __ardour_latent_h__*/
