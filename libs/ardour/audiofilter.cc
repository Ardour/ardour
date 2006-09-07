/*
    Copyright (C) 2004 Paul Davis 

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

    $Id$
*/

#include <time.h>
#include <cerrno>

#include <pbd/basename.h>
#include <ardour/sndfilesource.h>
#include <ardour/session.h>
#include <ardour/audioregion.h>
#include <ardour/audiofilter.h>
#include <ardour/region_factory.h>
#include <ardour/source_factory.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

int
AudioFilter::make_new_sources (boost::shared_ptr<AudioRegion> region, SourceList& nsrcs)
{
	vector<string> names = region->master_source_names();

	for (uint32_t i = 0; i < region->n_channels(); ++i) {

		string path = session.path_from_region_name (PBD::basename_nosuffix (names[i]), string (""));

		if (path.length() == 0) {
			error << string_compose (_("audiofilter: error creating name for new audio file based on %1"), region->name()) 
			      << endmsg;
			return -1;
		}

		try {
			nsrcs.push_back (boost::dynamic_pointer_cast<AudioSource> (SourceFactory::createWritable (session, path, false, session.frame_rate())));
		} 

		catch (failed_constructor& err) {
			error << string_compose (_("audiofilter: error creating new audio file %1 (%2)"), path, strerror (errno)) << endmsg;
			return -1;
		}
	}

	return 0;
}

int
AudioFilter::finish (boost::shared_ptr<AudioRegion> region, SourceList& nsrcs)
{
	string region_name;

	/* update headers on new sources */

	time_t xnow;
	struct tm* now;

	time (&xnow);
	now = localtime (&xnow);

	for (SourceList::iterator si = nsrcs.begin(); si != nsrcs.end(); ++si) {
		boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(*si);
		if (afs) {
			afs->update_header (region->position(), *now, xnow);
		}
	}

	/* create a new region */

	region_name = session.new_region_name (region->name());
	results.clear ();
	results.push_back (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (nsrcs, 0, region->length(), region_name, 0, 
											    Region::Flag (Region::WholeFile|Region::DefaultFlags))));
	
	return 0;
}
