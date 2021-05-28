/*
 * Copyright (C) 2000-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2021 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_solo_mute_release_h_
#define _ardour_solo_mute_release_h_

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

class Session;

class LIBARDOUR_API SoloMuteRelease
{
public:
	SoloMuteRelease (bool was_active);

	void set_exclusive (bool exclusive = true);

	void set (boost::shared_ptr<Route>);
	void set (boost::shared_ptr<RouteList>);
	void set (boost::shared_ptr<RouteList>, boost::shared_ptr<RouteList>);
	void set (boost::shared_ptr<std::list<std::string> >);

	void release (Session*, bool mute) const;

private:
	bool active;
	bool exclusive;

	boost::shared_ptr<RouteList> routes_on;
	boost::shared_ptr<RouteList> routes_off;

	boost::shared_ptr<std::list<std::string> > port_monitors;
};

} // namespace ARDOUR
#endif
