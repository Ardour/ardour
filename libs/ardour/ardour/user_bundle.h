/*
    Copyright (C) 2007 Paul Davis

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

#ifndef __ardour_user_bundle_h__
#define __ardour_user_bundle_h__

#include <vector>
#include <glibmm/threads.h>
#include "pbd/stateful.h"
#include "ardour/bundle.h"

namespace ARDOUR {

class Session;

class LIBARDOUR_API UserBundle : public Bundle, public PBD::Stateful {

  public:
	UserBundle (std::string const &);
	UserBundle (XMLNode const &, bool);

	XMLNode& get_state ();

  private:
	int set_state (XMLNode const &, int version);
};

}

#endif
