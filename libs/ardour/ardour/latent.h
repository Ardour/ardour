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

	virtual samplecnt_t signal_latency() const = 0;

	/* effective latency to be used while processing */
	samplecnt_t effective_latency() const {
		if (_zero_latency) {
			return 0;
		} else if (_use_user_latency) {
			return _user_latency;
		} else {
			return signal_latency ();
		}
	}

	/* custom user-set latency, if any */
	samplecnt_t user_latency () const {
		if (_use_user_latency) {
			return _user_latency;
		} else {
			return 0;
		}
	}

	void unset_user_latency () {
		_use_user_latency = false;
		_user_latency = 0;
	}

	virtual void set_user_latency (samplecnt_t val) {
		_use_user_latency = true;
		_user_latency = val;
	}

	static void force_zero_latency (bool en) {
		_zero_latency = en;
	}

	static bool zero_latency () {
		return _zero_latency;
	}

protected:
	int  set_state (const XMLNode& node, int version);
	void add_state (XMLNode*) const;

private:
	samplecnt_t _use_user_latency;
	samplecnt_t _user_latency;
	static bool _zero_latency;
};

} /* namespace */


#endif /* __ardour_latent_h__*/
