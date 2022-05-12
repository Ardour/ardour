/*
 * Copyright (C) 2022 Robin Gareus <robin@gareus.org>
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

#ifndef _libardour_export_smf_writer_h_
#define _libardour_export_smf_writer_h_

#include <boost/shared_ptr.hpp>

#include "evoral/SMF.h"

#include "ardour/libardour_visibility.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/types.h"

namespace ARDOUR
{
class MidiBuffer;

class LIBARDOUR_API ExportSMFWriter : Evoral::SMF
{
public:
	ExportSMFWriter ();
	~ExportSMFWriter ();

	int init (std::string const& path, samplepos_t);

	void process (MidiBuffer const&, sampleoffset_t, samplecnt_t, bool);

private:
	std::string      _path;
	samplepos_t      _pos;
	samplepos_t      _last_ev_time_samples;
	samplepos_t      _timespan_start;
	MidiStateTracker _tracker;
};

} // namespace ARDOUR

#endif
