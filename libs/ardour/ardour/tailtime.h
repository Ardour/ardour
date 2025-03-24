/*
 * Copyright (C) 2024 Robin Gareus <robin@gareus.org>
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

#pragma once

#include "pbd/signals.h"

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

class LIBARDOUR_API HasTailTime {
public:
	virtual ~HasTailTime () {}
	virtual samplecnt_t signal_tailtime () const = 0;
};

class LIBARDOUR_API TailTime : public HasTailTime {
public:
	TailTime ();
	TailTime (TailTime const&);
	virtual ~TailTime() {}

	samplecnt_t effective_tailtime () const;

	samplecnt_t user_latency () const {
		if (_use_user_tailtime) {
			return _user_tailtime;
		} else {
			return 0;
		}
	}

	void unset_user_tailtime ();
	void set_user_tailtime (samplecnt_t val);

	PBD::Signal<void()> TailTimeChanged;

protected:
	int  set_state (const XMLNode& node, int version);
	void add_state (XMLNode*) const;

private:
	samplecnt_t _use_user_tailtime;
	samplecnt_t _user_tailtime;
};

} /* namespace */



