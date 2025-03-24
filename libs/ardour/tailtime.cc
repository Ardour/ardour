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

#include "pbd/xml++.h"

#include "ardour/tailtime.h"
#include "ardour/rc_configuration.h"

using namespace ARDOUR;

TailTime::TailTime ()
	: HasTailTime ()
	, _use_user_tailtime (false)
	, _user_tailtime (0)
{}

TailTime::TailTime (TailTime const& other)
	: HasTailTime ()
	, _use_user_tailtime (other._use_user_tailtime)
	, _user_tailtime (other._user_tailtime)
{}

samplecnt_t
TailTime::effective_tailtime () const
{
	if (_use_user_tailtime) {
		return _user_tailtime;
	} else {
		return std::max<samplecnt_t> (0, std::min<samplecnt_t> (signal_tailtime (), Config->get_max_tail_samples ()));
	}
}

void
TailTime::set_user_tailtime (samplecnt_t val)
{
	if (_use_user_tailtime && _user_tailtime == val) {
		return;
	}
	_use_user_tailtime = true;
	_user_tailtime = val;
	TailTimeChanged (); /* EMIT SIGNAL */
}

void
TailTime::unset_user_tailtime ()
{
	if (!_use_user_tailtime) {
		return;
	}
	_use_user_tailtime = false;
	_user_tailtime = 0;
	TailTimeChanged (); /* EMIT SIGNAL */
}



int
TailTime::set_state (const XMLNode& node, int version)
{
	node.get_property ("user-tailtime", _user_tailtime);
	if (!node.get_property ("use-user-tailtime", _use_user_tailtime)) {
		_use_user_tailtime = _user_tailtime > 0;
	}
	return 0;
}

void
TailTime::add_state (XMLNode* node) const
{
	node->set_property ("user-tailtime", _user_tailtime);
	node->set_property ("use-user-tailtime", _use_user_tailtime);
}
