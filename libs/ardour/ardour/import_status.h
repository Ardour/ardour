/*
    Copyright (C) 2000 Paul Davis

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

#ifndef __ardour_import_status_h__
#define __ardour_import_status_h__

#include <string>
#include <vector>

#include <stdint.h>

#include "ardour/interthread_info.h"
#include "ardour/types.h"

namespace ARDOUR {

class ImportStatus : public InterThreadInfo {
public:
	std::string doing_what;

	/* control info */
	uint32_t                   current;
	uint32_t                   total;
	SrcQuality                 quality;
	volatile bool              freeze;
	std::vector<std::string>   paths;
	bool                       replace_existing_source;

	/** set to true when all files have been imported, as distinct from the done in ARDOUR::InterThreadInfo,
	 *  which indicates that one run of the import thread has been completed.
	 */
	bool all_done;

	/* result */
	SourceList sources;
};

} // namespace ARDOUR

#endif /* __ardour_import_status_h__ */
