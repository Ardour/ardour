/*
    Copyright (C) 2009 Paul Davis

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

#include "ardour/filter.h"

namespace ARDOUR {

/// A filter to strip silence from regions
class LIBARDOUR_API StripSilence : public Filter
{
  public:
	StripSilence (Session &, const AudioIntervalMap&, framecnt_t fade_length);

	int run (boost::shared_ptr<ARDOUR::Region>, Progress* progress = 0);

private:
	const AudioIntervalMap& _smap;
	framecnt_t _fade_length; ///< fade in/out to use on trimmed regions, in samples
};

}
