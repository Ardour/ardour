/*
 * Copyright (C) 2008-2011 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_export_timespan_h__
#define __ardour_export_timespan_h__

#include <string>

#include <boost/shared_ptr.hpp>

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR
{

class ExportStatus;
class ExportChannel;
class ExportTempFile;

class LIBARDOUR_API ExportTimespan
{
private:
	typedef boost::shared_ptr<ExportStatus> ExportStatusPtr;

private:
	friend class ExportElementFactory;
	ExportTimespan (ExportStatusPtr status, samplecnt_t sample_rate);

public:
	~ExportTimespan ();

	std::string name () const { return _name; }
	void set_name (std::string name) { _name = name; }

	std::string range_id () const { return _range_id; }
	void set_range_id (std::string range_id) { _range_id = range_id; }

	bool realtime () const { return _realtime; }
	void set_realtime (bool rt) { _realtime = rt; }

	void set_range (samplepos_t start, samplepos_t end);
	samplecnt_t get_length () const { return end_sample - start_sample; }
	samplepos_t get_start () const { return start_sample; }
	samplepos_t get_end () const { return end_sample; }

	/// Primarily compare start time, then end time
	bool operator< (ExportTimespan const & other) {
		if (start_sample < other.start_sample) { return true; }
		if (start_sample > other.start_sample) { return false; }
		return end_sample < other.end_sample;
	}

private:

	ExportStatusPtr status;

	samplepos_t start_sample;
	samplepos_t end_sample;
	samplepos_t position;
	samplecnt_t sample_rate;

	std::string _name;
	std::string _range_id;
	bool        _realtime;

};

} // namespace ARDOUR

#endif /* __ardour_export_timespan_h__ */
