/*
 * Copyright (C) 2006-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009 Carl Hetherington <carl@carlh.net>
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

#ifndef __ardour_gtk_time_selection_h__
#define __ardour_gtk_time_selection_h__

#include <list>
#include "ardour/types.h"

namespace ARDOUR {
	class RouteGroup;
}

class TimeSelection : public std::list<ARDOUR::AudioRange>
{
public:
	ARDOUR::AudioRange& operator[](uint32_t);

	ARDOUR::samplepos_t start() const;
	ARDOUR::samplepos_t end_sample() const;
	ARDOUR::samplepos_t length() const;

	bool consolidate ();
};


#endif /* __ardour_gtk_time_selection_h__ */
