/*
 * Copyright (C) 2011-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2011-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libardour_proxy_controllable_h__
#define __libardour_proxy_controllable_h__

#include <boost/function.hpp>

#include "pbd/controllable.h"

namespace ARDOUR {

/** this class converts a pair of setter/getter functors into a Controllable
    so that it can be used like a regular Controllable, bound to MIDI, OSC etc.
*/

class LIBARDOUR_API ProxyControllable : public PBD::Controllable {
  public:
	ProxyControllable (const std::string& name, PBD::Controllable::Flag flags,
			   boost::function1<bool,double> setter,
			   boost::function0<double> getter)
		: PBD::Controllable (name, flags)
		, _setter (setter)
		, _getter (getter)
	{}

	void set_value (double v, PBD::Controllable::GroupControlDisposition gcd) { if (_setter (v)) { Changed (true, gcd); /* EMIT SIGNAL */ } }
	double get_value () const { return _getter (); }

	std::string get_user_string () const {
		char theBuf[32]; sprintf( theBuf, "%3.1f dB", accurate_coefficient_to_dB (get_value()));
		return std::string(theBuf);
	}

  private:
	boost::function1<bool,double> _setter;
	boost::function0<double> _getter;
};

} // namespace

#endif /* __libardour_proxy_controllable_h__ */
