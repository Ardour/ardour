/*
    Copyright (C) 2000-2020 Paul Davis

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

#ifndef __ardour_gtk_time_selection_h__
#define __ardour_gtk_time_selection_h__

#include <list>
#include "ardour/types.h"

namespace ARDOUR {
	class RouteGroup;
}

class TimeSelection : public std::list<ARDOUR::TimelineRange>
{
public:
	ARDOUR::TimelineRange & operator[](uint32_t);

	ARDOUR::samplepos_t start_sample() const;
	ARDOUR::samplepos_t end_sample() const;
	ARDOUR::samplepos_t length_samples() const;

	Temporal::timepos_t start_time() const;
	Temporal::timepos_t end_time() const;
	Temporal::timecnt_t length() const;

	void set (Temporal::timepos_t const &, Temporal::timepos_t const &);

	bool consolidate ();
};


#endif /* __ardour_gtk_time_selection_h__ */
