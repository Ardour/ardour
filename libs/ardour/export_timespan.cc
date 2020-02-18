/*
 * Copyright (C) 2008-2009 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
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

#include "ardour/export_timespan.h"

namespace ARDOUR
{

ExportTimespan::ExportTimespan (ExportStatusPtr status, samplecnt_t sample_rate) :
	status (status),
	start_sample (0),
	end_sample (0),
	position (0),
	sample_rate (sample_rate),
	_realtime (false)
{

}

ExportTimespan::~ExportTimespan ()
{
}

void
ExportTimespan::set_range (samplepos_t start, samplepos_t end)
{
	start_sample = start;
	position = start_sample;
	end_sample = end;
}

} // namespace ARDOUR
