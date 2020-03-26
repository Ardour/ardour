/*
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
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

#ifndef __ardour_import_status_h__
#define __ardour_import_status_h__

#include <string>
#include <vector>

#include <stdint.h>

#include "ardour/interthread_info.h"
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

class LIBARDOUR_API ImportStatus : public InterThreadInfo {
public:
	virtual ~ImportStatus() {
		clear ();
	}

	virtual void clear () {
		sources.clear ();
		paths.clear ();
	}

	std::string doing_what;

	/* control info */
	uint32_t                   current;
	uint32_t                   total;
	SrcQuality                 quality;
	volatile bool              freeze;
	std::vector<std::string>   paths;
	bool                       replace_existing_source;
	bool                       split_midi_channels;
	MidiTrackNameSource        midi_track_name_source;

	/** set to true when all files have been imported, as distinct from the done in ARDOUR::InterThreadInfo,
	 *  which indicates that one run of the import thread has been completed.
	 */
	bool all_done;

	/* result */
	SourceList sources;
};

} // namespace ARDOUR

#endif /* __ardour_import_status_h__ */
