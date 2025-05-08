/*
 * Copyright (C) 2007-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
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

#include <time.h>
#include <cerrno>

#include "pbd/basename.h"

#include "ardour/analyser.h"
#include "ardour/audiofilesource.h"
#include "ardour/audioregion.h"
#include "ardour/filter.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/smf_source.h"
#include "ardour/source_factory.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

int
Filter::make_new_sources (std::shared_ptr<Region> region, SourceList& nsrcs, std::string suffix, bool use_session_sample_rate)
{
	vector<string> names = region->master_source_names();
	const SourceList::size_type nsrc = region->sources().size();
	assert (nsrc <= names.size());

	for (SourceList::size_type i = 0; i < nsrc; ++i) {

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

		const string path = (region->data_type() == DataType::MIDI)
			? session.new_midi_source_path (name)
			: session.new_audio_source_path (name, nsrc, i, false);

		if (path.empty()) {
			error << string_compose (_("filter: error creating name for new file based on %1"), region->name())
			      << endmsg;
			return -1;
		}

		try {
			samplecnt_t sample_rate = session.sample_rate ();
			if (!use_session_sample_rate) {
				std::shared_ptr<AudioRegion> aregion = std::dynamic_pointer_cast<AudioRegion>(region);

				if (aregion) {
					sample_rate = aregion->audio_source()->sample_rate();
				}
			}

			nsrcs.push_back (std::dynamic_pointer_cast<Source> (
				                 SourceFactory::createWritable (region->data_type(), session,
				                                                path, sample_rate)));
		}

		catch (failed_constructor& err) {
			error << string_compose (_("filter: error creating new file %1 (%2)"), path, strerror (errno)) << endmsg;
			return -1;
		}
	}

	return 0;
}

int
Filter::finish (std::shared_ptr<Region> region, SourceList& nsrcs, string region_name)
{
	/* update headers on new sources */

	time_t xnow;
	struct tm* now;

	time (&xnow);
	now = localtime (&xnow);

	/* this is ugly. */
	for (SourceList::iterator si = nsrcs.begin(); si != nsrcs.end(); ++si) {
		std::shared_ptr<AudioFileSource> afs = std::dynamic_pointer_cast<AudioFileSource>(*si);
		if (afs) {
			afs->done_with_peakfile_writes ();
			afs->update_header (region->position_sample(), *now, xnow);
			afs->mark_immutable ();
		}

		std::shared_ptr<SMFSource> smfs = std::dynamic_pointer_cast<SMFSource>(*si);
		if (smfs) {
			smfs->set_natural_position (region->position());
			smfs->flush ();
		}

		/* now that there is data there, requeue the file for analysis */

		Analyser::queue_source_for_analysis (*si, false);
	}

	/* create a new region */

	if (region_name.empty()) {
		region_name = RegionFactory::new_region_name (region->name());
	}
	results.clear ();

	PropertyList plist (region->derive_properties (true, true));

	plist.add (Properties::start, std::numeric_limits<timepos_t>::min());
	plist.add (Properties::name, region_name);
	plist.add (Properties::whole_file, true);

	std::shared_ptr<Region> r = RegionFactory::create (nsrcs, plist);
	std::shared_ptr<AudioRegion> ar = std::dynamic_pointer_cast<AudioRegion> (r);
	if (ar) {
		ar->copy_plugin_state (static_pointer_cast<AudioRegion const> (region));
	}
	results.push_back (r);

	return 0;
}


