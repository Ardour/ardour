/*
 * Copyright (C) 2018-2019 Damien Zammit <damien@zamaudio.com>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <algorithm>
#include <glibmm.h>

#include "pbd/pthread_utils.h"
#include "pbd/basename.h"
#include "pbd/shortpath.h"
#include "pbd/stateful_diff_command.h"

#include "ardour/audioengine.h"
#include "ardour/audio_track.h"
#include "ardour/audiofilesource.h"
#include "ardour/audioregion.h"
#include "ardour/import_status.h"
#include "ardour/midi_region.h"
#include "ardour/midi_track.h"
#include "ardour/midi_model.h"
#include "ardour/operations.h"
#include "ardour/region_factory.h"
#include "ardour/smf_source.h"
#include "ardour/source_factory.h"
#include "ardour/utils.h"
#include "ardour/playlist.h"
#include "ardour/session.h"
#include "pbd/memento_command.h"

#include "ptformat/ptformat.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Glib;
using std::string;

/* Functions supporting the incorporation of PT sessions into ardour */

struct midipair {
	midipair (uint16_t idx, string n)
		: ptfindex (idx)
		  , trname (n)
	{}
	uint16_t ptfindex;
	string trname;
};

struct PlaylistState {
	PlaylistState () : before (0) {}

	boost::shared_ptr<Playlist> playlist;
	XMLNode* before;
};

bool
Session::import_sndfile_as_region (string path, SrcQuality quality, samplepos_t& pos, SourceList& sources, ImportStatus& status, uint32_t current, uint32_t total)
{
	/* Import the source */
	status.paths.clear();
	status.paths.push_back(path);
	status.current = current;
	status.total = total;
	status.freeze = false;
	status.quality = quality;
	status.replace_existing_source = false;
	status.split_midi_channels = false;
	status.done = false;
	status.cancel = false;

	import_files(status);
	status.progress = 1.0;
	sources.clear();

	/* FIXME: There is no way to tell if cancel button was pressed
	 * or if the file failed to import, just that one of these occurred.
	 * We want status.cancel to reflect the user's choice only
	 */
	if (status.cancel && status.current > current) {
		/* Succeeded to import file, assume user hit cancel */
		return false;
	} else if (status.cancel && status.current == current) {
		/* Failed to import file, assume user did not hit cancel */
		status.cancel = false;
		return false;
	}

	sources.push_back(status.sources.front());

	/* Put the source on a region */
	vector<boost::shared_ptr<Region> > regions;
	string region_name;
	bool use_timestamp;

	use_timestamp = (pos == -1);

	/* take all the sources we have and package them up as a region */

	region_name = region_name_from_path (status.paths.front(), (sources.size() > 1), false);

	/* we checked in import_sndfiles() that there were not too many */

	while (RegionFactory::region_by_name (region_name)) {
		region_name = bump_name_once (region_name, '.');
	}

	PropertyList plist;

	plist.add (ARDOUR::Properties::start, 0);
	plist.add (ARDOUR::Properties::length, sources[0]->length (pos));
	plist.add (ARDOUR::Properties::name, region_name);
	plist.add (ARDOUR::Properties::layer, 0);
	plist.add (ARDOUR::Properties::whole_file, true);
	plist.add (ARDOUR::Properties::external, true);

	boost::shared_ptr<Region> r = RegionFactory::create (sources, plist);

	if (use_timestamp && boost::dynamic_pointer_cast<AudioRegion>(r)) {
		boost::dynamic_pointer_cast<AudioRegion>(r)->special_set_position(sources[0]->natural_position());
	}

	regions.push_back (r);

	/* if we're creating a new track, name it after the cleaned-up
	 * and "merged" region name.
	 */

	int n = 0;

	for (vector<boost::shared_ptr<Region> >::iterator r = regions.begin(); r != regions.end(); ++r, ++n) {
		boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (*r);

		if (use_timestamp) {
			if (ar) {

				/* get timestamp for this region */

				const boost::shared_ptr<Source> s (ar->sources().front());
				const boost::shared_ptr<AudioSource> as = boost::dynamic_pointer_cast<AudioSource> (s);

				assert (as);

				if (as->natural_position() != 0) {
					pos = as->natural_position();
				} else {
					pos = 0;
				}
			} else {
				/* should really get first position in MIDI file, but for now, use 0 */
				pos = 0;
			}
		}
	}

	for (SourceList::iterator x = sources.begin(); x != sources.end(); ++x) {
		SourceFactory::setup_peakfile (*x, true);
	}

	return true;
}


void
Session::import_pt_sources (PTFFormat& ptf, ImportStatus& status)
{
	string fullpath;
	bool ok = false;
	bool onefailed = false;
	samplepos_t pos = -1;

	vector<PTFFormat::wav_t>::const_iterator w;
	uint32_t wth = 0;

	SourceList just_one_src;

	ptfwavpair.clear();
	pt_imported_sources.clear();
	status.clear();

	for (w = ptf.audiofiles ().begin (); w != ptf.audiofiles ().end () && !status.cancel; ++w) {
		struct ptflookup p;
		wth++;
		ok = false;
		/* Try audio file */
		fullpath = Glib::build_filename (Glib::path_get_dirname (ptf.path ()), "Audio Files");
		fullpath = Glib::build_filename (fullpath, w->filename);
		if (Glib::file_test (fullpath, Glib::FILE_TEST_EXISTS)) {
			just_one_src.clear();
			ok = import_sndfile_as_region (fullpath, SrcBest, pos, just_one_src, status, wth, ptf.audiofiles ().size ());
		} else {
			/* Try fade file */
			fullpath = Glib::build_filename (Glib::path_get_dirname (ptf.path ()), "Fade Files");
			fullpath = Glib::build_filename (fullpath, w->filename);
			if (Glib::file_test (fullpath, Glib::FILE_TEST_EXISTS)) {
				just_one_src.clear();
				ok = import_sndfile_as_region (fullpath, SrcBest, pos, just_one_src, status, wth, ptf.audiofiles ().size ());
			} else {
				onefailed = true;

				/* ptformat knows length of sources *in PT sample rate*
				 * BUT if ardour user later resolves missing file,
				 * it won't be resampled, so we can only do this
				 * when sample rates are matching
				 */
				if (sample_rate () == ptf.sessionrate ()) {
					/* Insert reference to missing source */
					samplecnt_t sourcelen = w->length;
					XMLNode srcxml (X_("Source"));
					srcxml.set_property ("name", w->filename);
					srcxml.set_property ("type", "audio");
					srcxml.set_property ("id", PBD::ID ().to_s ());
					boost::shared_ptr<Source> source = SourceFactory::createSilent (*this, srcxml, sourcelen, sample_rate ());
					p.index1 = w->index;
					p.id = source->id ();
					ptfwavpair.push_back (p);
					pt_imported_sources.push_back (source);
					warning << string_compose (_("PT Import : MISSING `%1`, inserting ref to missing source"), fullpath) << endmsg;
				} else {
					warning << string_compose (_("PT Import : MISSING `%1`, please check Audio Files"), fullpath) << endmsg;
				}
			}
		}
		if (ok) {
			p.index1 = w->index;
			p.id = just_one_src.back ()->id ();

			ptfwavpair.push_back (p);
			pt_imported_sources.push_back (just_one_src.back ());
		} else {
			onefailed = true;
		}
	}

	if (pt_imported_sources.empty ()) {
		error << _("Failed to find any audio for PT import") << endmsg;
	} else if (onefailed) {
		warning << _("Failed to load one or more of the audio files for PT import, see above list") << endmsg;
	} else {
		info << _("All audio files found for PT import!") << endmsg;
	}

	status.progress = 1.0;
	status.sources.clear ();
	status.done = true;
	status.all_done = true;
}


void
Session::import_pt_rest (PTFFormat& ptf)
{
	vector<boost::shared_ptr<Region> > regions;
	boost::shared_ptr<ARDOUR::Track> track;
	ARDOUR::PluginInfoPtr instrument;
	vector<string> to_import;
	string fullpath;
	uint32_t srate = sample_rate ();

	vector<struct ptflookup> ptfregpair;

	SourceList just_one_src;

	boost::shared_ptr<AudioTrack> existing_track;
	uint16_t i;
	uint16_t nth = 0;
	uint16_t ntr = 0;
	vector<struct ptflookup> usedtracks;
	struct ptflookup utr;
	vector<midipair> uniquetr;

	vector<PlaylistState> playlists;
	vector<PlaylistState>::iterator pl;

	usedtracks.clear();
	just_one_src.clear();
	uniquetr.clear();
	ptfregpair.clear();
	to_import.clear();
	regions.clear();
	playlists.clear();

	for (vector<PTFFormat::region_t>::const_iterator a = ptf.regions ().begin ();
			a != ptf.regions ().end (); ++a) {
		for (vector<struct ptflookup>::iterator p = ptfwavpair.begin ();
				p != ptfwavpair.end (); ++p) {
			if ((p->index1 == a->wave.index) && (strcmp (a->wave.filename.c_str (), "") != 0)) {
				for (SourceList::iterator x = pt_imported_sources.begin (); x != pt_imported_sources.end (); ++x) {
					if ((*x)->id () == p->id) {
						/* Matched an uncreated ptf region to ardour region */
						struct ptflookup rp;
						PropertyList plist;

						plist.add (ARDOUR::Properties::start, a->sampleoffset);
						plist.add (ARDOUR::Properties::position, 0);
						plist.add (ARDOUR::Properties::length, a->length);
						plist.add (ARDOUR::Properties::name, a->name);
						plist.add (ARDOUR::Properties::layer, 0);
						plist.add (ARDOUR::Properties::whole_file, false);
						plist.add (ARDOUR::Properties::external, true);

						just_one_src.clear ();
						just_one_src.push_back (*x);

						boost::shared_ptr<Region> r = RegionFactory::create (just_one_src, plist);
						regions.push_back (r);

						rp.id = regions.back ()->id ();
						rp.index1 = a->index;
						ptfregpair.push_back (rp);
					}
				}
			}
		}
	}

	/* Check for no audio */
	if (ptf.tracks ().size () == 0) {
		goto no_audio_tracks;
	}

	/* Create all tracks */
	ntr = (ptf.tracks ().at (ptf.tracks ().size () - 1)).index + 1;
	nth = -1;
	for (vector<PTFFormat::track_t>::const_iterator a = ptf.tracks ().begin (); a != ptf.tracks ().end (); ++a) {
		if (a->index != nth) {
			nth++;
			DEBUG_TRACE (DEBUG::FileUtils, string_compose ("\tcreate tr(%1) %2\n", nth, a->name.c_str()));
			list<boost::shared_ptr<AudioTrack> > at (new_audio_track (1, 2, 0, 1, a->name.c_str(), PresentationInfo::max_order, Normal));
			if (at.empty ()) {
				return;
			}
		}
	}

	/* Get all playlists of all tracks and Playlist::freeze() all tracks */
	assert (ntr == nth + 1);
	for (i = 0; i < ntr; ++i) {
		existing_track = get_nth_audio_track (i);
		boost::shared_ptr<Playlist> playlist = existing_track->playlist();

		PlaylistState before;
		before.playlist = playlist;
		before.before = &playlist->get_state();
		playlist->clear_changes ();
		playlist->freeze ();
		playlists.push_back(before);
	}

	/* Add regions */
	for (vector<PTFFormat::track_t>::const_iterator a = ptf.tracks ().begin (); a != ptf.tracks ().end (); ++a) {
		for (vector<struct ptflookup>::iterator p = ptfregpair.begin ();
				p != ptfregpair.end (); ++p) {

			if (p->index1 == a->reg.index)  {

				/* Matched a ptf active region to an ardour region */
				boost::shared_ptr<Region> r = RegionFactory::region_by_id (p->id);
				DEBUG_TRACE (DEBUG::FileUtils, string_compose ("\twav(%1) reg(%2) tr(%3)\n", a->reg.wave.filename.c_str (), a->reg.index, a->index));

				/* Use existing playlists */
				boost::shared_ptr<Playlist> playlist = playlists[a->index].playlist;
				boost::shared_ptr<Region> copy (RegionFactory::create (r, true));
				playlist->add_region (copy, a->reg.startpos);
			}
		}
	}

	/* Playlist::thaw() all tracks */
	for (pl = playlists.begin(); pl != playlists.end(); ++pl) {
		(*pl).playlist->thaw ();
	}

no_audio_tracks:
	/* MIDI - Find list of unique midi tracks first */

	for (vector<PTFFormat::track_t>::const_iterator a = ptf.miditracks ().begin (); a != ptf.miditracks ().end (); ++a) {
		bool found = false;
		for (vector<midipair>::iterator b = uniquetr.begin (); b != uniquetr.end (); ++b) {
			if (b->trname == a->name) {
				found = true;
				break;
			}
		}
		if (!found) {
			uniquetr.push_back (midipair (a->index, a->name));
			//printf(" : %d : %s\n", a->index, a->name.c_str());
		}
	}

	std::map <int, boost::shared_ptr<MidiTrack> > midi_tracks;
	/* MIDI - Create unique midi tracks and a lookup table for used tracks */
	for (vector<midipair>::iterator a = uniquetr.begin (); a != uniquetr.end (); ++a) {
		struct ptflookup miditr;
		list<boost::shared_ptr<MidiTrack> > mt (new_midi_track (
				ChanCount (DataType::MIDI, 1),
				ChanCount (DataType::MIDI, 1),
				true,
				instrument, (Plugin::PresetRecord*) 0,
				(RouteGroup*) 0,
				1,
				a->trname,
				PresentationInfo::max_order,
				Normal));
		assert (mt.size () == 1);
		midi_tracks[a->ptfindex] = mt.front ();
	}

	/* MIDI - Add midi regions one-by-one to corresponding midi tracks */
	for (vector<PTFFormat::track_t>::const_iterator a = ptf.miditracks ().begin (); a != ptf.miditracks ().end (); ++a) {

		boost::shared_ptr<MidiTrack> midi_track = midi_tracks[a->index];
		assert (midi_track);
		boost::shared_ptr<Playlist> playlist = midi_track->playlist ();
		samplepos_t f = (samplepos_t)a->reg.startpos * srate / 1920000.;
		samplecnt_t length = (samplecnt_t)a->reg.length * srate / 1920000.;
		MusicSample pos (f, 0);
		boost::shared_ptr<Source> src = create_midi_source_by_stealing_name (midi_track);
		PropertyList plist;
		plist.add (ARDOUR::Properties::start, 0);
		plist.add (ARDOUR::Properties::length, length);
		plist.add (ARDOUR::Properties::name, PBD::basename_nosuffix (src->name ()));
		//printf(" : %d - trackname: (%s)\n", a->index, src->name ().c_str ());
		boost::shared_ptr<Region> region = (RegionFactory::create (src, plist));
		/* sets beat position */
		region->set_position (pos.sample, pos.division);
		midi_track->playlist ()->add_region (region, pos.sample, 1.0, false, pos.division);

		boost::shared_ptr<MidiRegion> mr = boost::dynamic_pointer_cast<MidiRegion>(region);
		boost::shared_ptr<MidiModel> mm = mr->midi_source (0)->model ();
		MidiModel::NoteDiffCommand *midicmd;
		midicmd = mm->new_note_diff_command ("Import ProTools MIDI");

		for (vector<PTFFormat::midi_ev_t>::const_iterator j = a->reg.midi.begin (); j != a->reg.midi.end (); ++j) {
			//printf(" : MIDI : pos=%f len=%f\n", (float)j->pos / 960000., (float)j->length / 960000.);
			Temporal::Beats start = Temporal::Beats::from_double (j->pos / 960000.);
			Temporal::Beats len = Temporal::Beats::from_double(j->length / 960000.);
			/* PT C-2 = 0, Ardour C-1 = 0, subtract twelve to convert ? */
			midicmd->add (boost::shared_ptr<Evoral::Note<Temporal::Beats> > (new Evoral::Note<Temporal::Beats> ((uint8_t)1, start, len, j->note, j->velocity)));
		}
		mm->apply_command (this, midicmd);
		boost::shared_ptr<Region> copy (RegionFactory::create (mr, true));
		playlist->clear_changes ();
		playlist->add_region (copy, f);
	}
}
