/*
    Copyright (C) 2007 Paul Davis
    Author: David Robillard

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

#ifndef __ardour_filter_h__
#define __ardour_filter_h__

#include <vector>

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

class Region;
class Session;
class Progress;

class LIBARDOUR_API Filter {

  public:
	virtual ~Filter() {}

	virtual int run (boost::shared_ptr<ARDOUR::Region>, Progress* progress = 0) = 0;
	std::vector<boost::shared_ptr<ARDOUR::Region> > results;

  protected:
	Filter (ARDOUR::Session& s) : session(s) {}

	int make_new_sources (boost::shared_ptr<ARDOUR::Region>, ARDOUR::SourceList&, std::string suffix = "");
	int finish (boost::shared_ptr<ARDOUR::Region>, ARDOUR::SourceList&, std::string region_name = "");

	ARDOUR::Session& session;
};

} /* namespace */

#endif /* __ardour_filter_h__ */
