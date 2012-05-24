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

#ifndef __ardour_export_timespan_h__
#define __ardour_export_timespan_h__

#include <string>

#include <boost/shared_ptr.hpp>

#include "ardour/types.h"

namespace ARDOUR
{

class ExportStatus;
class ExportChannel;
class ExportTempFile;

class ExportTimespan
{
  private:
	typedef boost::shared_ptr<ExportStatus> ExportStatusPtr;

  private:
	friend class ExportElementFactory;
	ExportTimespan (ExportStatusPtr status, framecnt_t frame_rate);

  public:
	~ExportTimespan ();

	std::string name () const { return _name; }
	void set_name (std::string name) { _name = name; }

	std::string range_id () const { return _range_id; }
	void set_range_id (std::string range_id) { _range_id = range_id; }

	void set_range (framepos_t start, framepos_t end);
	framecnt_t get_length () const { return end_frame - start_frame; }
	framepos_t get_start () const { return start_frame; }
	framepos_t get_end () const { return end_frame; }

	/// Primarily compare start time, then end time
	bool operator< (ExportTimespan const & other) {
		if (start_frame < other.start_frame) { return true; }
		if (start_frame > other.start_frame) { return false; }
		return end_frame < other.end_frame;
	}

  private:

	ExportStatusPtr status;

	framepos_t      start_frame;
	framepos_t      end_frame;
	framepos_t      position;
	framecnt_t      frame_rate;

	std::string _name;
	std::string _range_id;

};

} // namespace ARDOUR

#endif /* __ardour_export_timespan_h__ */
