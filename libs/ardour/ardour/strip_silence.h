/*
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2010-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/filter.h"

namespace ARDOUR {

/// A filter to strip silence from regions
class LIBARDOUR_API StripSilence : public Filter
{
  public:
	StripSilence (Session &, const AudioIntervalMap&, samplecnt_t fade_length);

	int run (boost::shared_ptr<ARDOUR::Region>, Progress* progress = 0);

private:
	const AudioIntervalMap& _smap;
	samplecnt_t _fade_length; ///< fade in/out to use on trimmed regions, in samples
};

}
