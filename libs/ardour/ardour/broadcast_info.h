/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#ifndef __ardour_broadcast_info_h__
#define __ardour_broadcast_info_h__

#include <string>

#include "audiographer/broadcast_info.h"

namespace ARDOUR
{

class Session;

class BroadcastInfo : public AudioGrapher::BroadcastInfo
{
  public:
	BroadcastInfo ();

	void set_from_session (Session const & session, int64_t time_ref);

	void set_originator (std::string const & str = "");
	void set_originator_ref_from_session (Session const &);
};



} // namespace ARDOUR

#endif /* __ardour_broadcast_info_h__ */
