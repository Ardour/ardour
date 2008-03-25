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
#include <ardour/analyser.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

int
AudioFilter::make_new_sources (boost::shared_ptr<AudioRegion> region, SourceList& nsrcs, string suffix)
{
	vector<string> names = region->master_source_names();

	if (names.size() != region->n_channels()) {
		warning << _("This is an old Ardour session that does not have\n\
sufficient information for rendered FX") << endmsg;
		return -1;
	}

	for (uint32_t i = 0; i < region->n_channels(); ++i) {

		string name = PBD::basename_nosuffix (names[i]);

		/* remove any existing version of suffix by assuming it starts
		   with some kind of "special" character.
		*/

		if (!suffix.empty()) {
			string::size_type pos = name.find (suffix[0]);
			if (pos != string::npos && pos > 2) {
				name = name.substr (0, pos - 1);
			}
		}

		string path = session.path_from_region_name (name, suffix);

		if (path.length() == 0) {
			error << string_compose (_("audiofilter: error creating name for new audio file based on %1"), region->name()) 
			      << endmsg;
			return -1;
		}

		try {
			nsrcs.push_back (boost::dynamic_pointer_cast<AudioSource> (SourceFactory::createWritable (session, path, false, session.frame_rate())));
			nsrcs.back()->prepare_for_peakfile_writes ();
		} 

		catch (failed_constructor& err) {
			error << string_compose (_("audiofilter: error creating new audio file %1 (%2)"), path, strerror (errno)) << endmsg;
			return -1;
		}
	}

	return 0;
}

int
AudioFilter::finish (boost::shared_ptr<AudioRegion> region, SourceList& nsrcs, string region_name)
{
	/* update headers on new sources */

	time_t xnow;
	struct tm* now;

	time (&xnow);
	now = localtime (&xnow);

	for (SourceList::iterator si = nsrcs.begin(); si != nsrcs.end(); ++si) {
		boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(*si);
		boost::shared_ptr<AudioSource> as = boost::dynamic_pointer_cast<AudioSource>(*si);

		if (as) {
			as->done_with_peakfile_writes ();
		}

		if (afs) {
			afs->update_header (region->position(), *now, xnow);
			afs->mark_immutable ();
		}
		
		/* now that there is data there, requeue the file for analysis */
		
		if (Config->get_auto_analyse_audio()) {
			Analyser::queue_source_for_analysis (*si, false);
		}
	}

	/* create a new region */

	if (region_name.empty()) {
		region_name = session.new_region_name (region->name());
	}
	results.clear ();
	results.push_back (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (nsrcs, 0, nsrcs.front()->length(), region_name, 0, 
											    Region::Flag (Region::WholeFile|Region::DefaultFlags))));
	
	return 0;
}
