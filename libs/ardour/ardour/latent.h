/*
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_latent_h__
#define __ardour_latent_h__

#include "pbd/signals.h"

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

class LIBARDOUR_API HasLatency {
public:
	virtual ~HasLatency() {}
	virtual samplecnt_t signal_latency() const = 0;
};

class LIBARDOUR_API Latent : public HasLatency {
public:
	Latent ();
	Latent (Latent const&);
	virtual ~Latent() {}


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
		if (_zero_latency == en) {
			return;
		}
		_zero_latency = en;
		DisableSwitchChanged (); /* EMIT SIGNAL */
	}

	static bool zero_latency () {
		return _zero_latency;
	}

	static PBD::Signal0<void> DisableSwitchChanged;
	PBD::Signal0<void> LatencyChanged;

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
