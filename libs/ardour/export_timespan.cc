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

#include "ardour/export_timespan.h"

namespace ARDOUR
{

ExportTimespan::ExportTimespan (ExportStatusPtr status, framecnt_t frame_rate) :
	status (status),
	start_frame (0),
	end_frame (0),
	position (0),
	frame_rate (frame_rate)
{

}

ExportTimespan::~ExportTimespan ()
{
}

void
ExportTimespan::set_range (framepos_t start, framepos_t end)
{
	start_frame = start;
	position = start_frame;
	end_frame = end;
}

} // namespace ARDOUR
