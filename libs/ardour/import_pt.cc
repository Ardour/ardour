/*
 * Copyright (C) 2018-2025 Damien Zammit <damien@zamaudio.com>
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
#include <errno.h>
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
#include "ardour/debug.h"
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

	std::shared_ptr<Playlist> playlist;
	XMLNode* before;
};

static bool
import_pt_sndfile (Session* s, PTFFormat& ptf, PTFFormat::wav_t& w, std::string path, std::vector<Session::PtfLookup>& wavchans,
		   SourceList& sources, ImportStatus& status, uint32_t current, uint32_t total)
{
	bool ok = true;
	Session::PtfLookup p;

	status.paths.clear();
	status.current = current;
	status.total = total;
	status.freeze = false;
	status.quality = SrcBest;
	status.replace_existing_source = false;
	status.split_midi_channels = false;
	status.import_markers = false;
	status.cancel = false;

	/* Check if no sound file was passed in */
	if (path == "") {
		/* ptformat knows length of sources *in PT sample rate*
		 * BUT if ardour user later resolves missing file,
		 * it won't be resampled, so we can only do this
		 * when sample rates are matching
		 */
		if (s->sample_rate () == ptf.sessionrate ()) {
			/* Insert reference to missing source */
			samplecnt_t sourcelen = w.length;
			XMLNode srcxml (X_("Source"));
			srcxml.set_property ("name", w.filename);
			srcxml.set_property ("type", "audio");
			srcxml.set_property ("id", PBD::ID ().to_s ());
			std::shared_ptr<ARDOUR::Source> source = SourceFactory::createSilent (*s, srcxml, sourcelen, s->sample_rate ());
			sources.push_back (source);

			p.index1 = w.index;
			p.index2 = 0; /* unused */
			p.id = sources.front ()->id ();
			wavchans.push_back (p);

			warning << string_compose (_("PT Import : MISSING `%1`, inserting ref to missing source"), w.filename) << endmsg;
		} else {
			/* no sound file and mismatching sample rate to ptf */
			warning << string_compose (_("PT Import : MISSING `%1`, please check Audio Files"), w.filename) << endmsg;
			ok = false;
		}
		status.done = false;
	} else {
		/* Import the source */
		status.paths.push_back(path);
		status.done = false;
		s->import_files(status);

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

		assert (status.sources.size () > 0);

		sources.push_back(status.sources.front());

		p.index1 = w.index;
		p.index2 = 0; /* unused */
		p.id = sources.front ()->id ();
		wavchans.push_back (p);
	}

	return ok;
}

static bool
import_pt_source_channels_or_empty (Session* s, PTFFormat& ptf, std::vector<PTFFormat::wav_t>& ws, std::vector<Session::PtfLookup>& wavchans,
				    SourceList& ch_sources, ImportStatus& status, uint32_t current, uint32_t total)
{
	bool ok, onefailed;
	string fullpath;

	onefailed = false;
	for (std::vector<PTFFormat::wav_t>::iterator w = ws.begin (); w != ws.end (); ++w) {
		ok = true;

		/* Try audio file */
		fullpath = Glib::build_filename (Glib::path_get_dirname (ptf.path ()), "Audio Files");
		fullpath = Glib::build_filename (fullpath, w->filename);
		if (Glib::file_test (fullpath, Glib::FILE_TEST_EXISTS)) {
			/* fullpath has a valid audio file - load it */
			ok = import_pt_sndfile (s, ptf, *w, fullpath, wavchans, ch_sources, status, current, total);
		} else {
			/* Try fade file */
			fullpath = Glib::build_filename (Glib::path_get_dirname (ptf.path ()), "Fade Files");
			fullpath = Glib::build_filename (fullpath, w->filename);
			if (Glib::file_test (fullpath, Glib::FILE_TEST_EXISTS)) {
				/* fullpath has a valid fade file - load it */
				ok = import_pt_sndfile (s, ptf, *w, fullpath, wavchans, ch_sources, status, current, total);
			} else {
				/* no sound file - fill source with silence */
				ok = import_pt_sndfile (s, ptf, *w, "", wavchans, ch_sources, status, current, total);
			}
		}
		if (!ok) {
			onefailed = true;
		} else {
			current++;
		}
	}

	if (onefailed)
		return false;

	/* now we have ch_sources with either silent sources or populated with sound file backed sources,
	 * and wavchans with vector of matching ids per channel */
	return true;
}

void
Session::import_pt_sources (PTFFormat& ptf, ImportStatus& status)
{
	bool ok = false;
	bool onefailed = false;
	bool allfailed = true;
	int src_cnt = 1;
	string base_name;
	map<string,vector<PTFFormat::wav_t> > multi_ch;
	vector<Session::PtfLookup> ptfwavpair;
	SourceList source_group;
	vector<std::shared_ptr<Region> > regions;
	timepos_t pos;

	status.clear ();
	ptfregpair.clear ();

	/* Collect multi channel info from sources */
	for (vector<PTFFormat::wav_t>::const_iterator w = ptf.audiofiles ().begin ();
	     w != ptf.audiofiles ().end (); ++w) {
		base_name = region_name_from_path (w->filename, true, false);
		multi_ch[base_name].push_back(*w);
	}

	/* Import all other regions for potentially single or multi channel grouped sources */
	for (map<string,vector<PTFFormat::wav_t> >::iterator m = multi_ch.begin (); m != multi_ch.end (); ++m) {
		ptfwavpair.clear ();
		source_group.clear ();
		ok = import_pt_source_channels_or_empty (this, ptf, multi_ch[(*m).first], ptfwavpair, source_group,
							 status, src_cnt, ptf.audiofiles ().size ());
		if (!ok) {
			onefailed = true;
			continue;
		} else {
			allfailed = false;
			src_cnt += multi_ch[(*m).first].size (); /* progress bar is 1-based */
		}

		/* Import whole_file region for potentially single or multi channel grouped sources */
		{
			Session::PtfLookup rp;
			PropertyList plist;

			plist.add (ARDOUR::Properties::start, timepos_t (0));
			plist.add (ARDOUR::Properties::length, multi_ch[(*m).first][0].length);
			plist.add (ARDOUR::Properties::name, (*m).first);
			plist.add (ARDOUR::Properties::layer, 0);
			plist.add (ARDOUR::Properties::whole_file, true);
			plist.add (ARDOUR::Properties::external, true);

			std::shared_ptr<Region> rg = RegionFactory::create (source_group, plist);
			regions.push_back (rg);

			rp.id = regions.back ()->id ();
			rp.index1 = -1; /* Special: this region is maybe from two merged srcs */
			ptfregpair.push_back (rp);
		}

		/* Create regions only for this multi channel source group */
		for (vector<PTFFormat::region_t>::const_iterator r = ptf.regions ().begin ();
		     r != ptf.regions ().end (); ++r) {
			for (vector<Session::PtfLookup>::iterator p = ptfwavpair.begin ();
			     p != ptfwavpair.end (); ++p) {
				if (p->index1 == r->wave.index) {
					/* Create an ardour region from multi channel source group */
					Session::PtfLookup rp;
					PropertyList plist;

					plist.add (ARDOUR::Properties::start, timepos_t (r->sampleoffset));
					plist.add (ARDOUR::Properties::length, r->length);
					plist.add (ARDOUR::Properties::name, (*m).first);
					plist.add (ARDOUR::Properties::layer, 0);
					plist.add (ARDOUR::Properties::whole_file, false);
					plist.add (ARDOUR::Properties::external, true);

					std::shared_ptr<Region> rg = RegionFactory::create (source_group, plist);
					regions.push_back (rg);

					rp.id = regions.back ()->id ();
					rp.index1 = r->index;
					ptfregpair.push_back (rp);
				}
			}
		}
	}

	if (allfailed) {
		error << _("Failed to find any audio for PT import") << endmsg;
	} else if (onefailed) {
		warning << _("Failed to load one or more of the audio files for PT import, see above list") << endmsg;
	} else {
		for (SourceList::iterator x = status.sources.begin (); x != status.sources.end (); ++x) {
			SourceFactory::setup_peakfile (*x, true);
		}
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
	bool ok;
	std::shared_ptr<ARDOUR::Track> track;
	ARDOUR::PluginInfoPtr instrument;
	vector<string> to_import;
	string fullpath;
	uint32_t srate = sample_rate ();
	timepos_t latest = timepos_t (0);

	SourceList all_ch_srcs;

	RouteList routes;
	list<std::shared_ptr<AudioTrack> > tracks;
	std::shared_ptr<AudioTrack> existing_track;
	Session::PtfLookup utr;
	vector<midipair> uniquetr;

	vector<PlaylistState> playlists;
	vector<PlaylistState>::iterator pl;

	all_ch_srcs.clear();
	uniquetr.clear();
	to_import.clear();
	playlists.clear();

	std::map<std::string, shared_ptr<AudioTrack> > track_map;

	/* name -> <channel count, last known index> */
	std::map<std::string, std::pair<int,int> > tr_multi;

	/* Check for no audio */
	if (ptf.tracks ().size () == 0) {
		goto no_audio_tracks;
	}

	/* Initialise index sentinels so we can match on .second in the next loop */
	for (vector<PTFFormat::track_t>::const_iterator a = ptf.tracks ().begin (); a != ptf.tracks ().end (); ++a) {
		tr_multi[a->name].second = -1;
	}

	/* Count the occurrences of unique indexes with the same track name, these are multichannel tracks */
	for (vector<PTFFormat::track_t>::const_iterator a = ptf.tracks ().begin (); a != ptf.tracks ().end (); ++a) {
		if (tr_multi[a->name].second != a->index) {
			tr_multi[a->name].first++;
			tr_multi[a->name].second = a->index;
		}
	}

	/* Freeze playlists of tracks that already exist in ardour that we will touch */
	for (vector<PTFFormat::track_t>::const_iterator a = ptf.tracks ().begin (); a != ptf.tracks ().end (); ++a) {
		if ((existing_track = dynamic_pointer_cast<AudioTrack> (route_by_name (a->name)))) {
			if (track_map[a->name] != existing_track) {
				track_map[a->name] = existing_track;
				std::shared_ptr<Playlist> playlist = existing_track->playlist();

				PlaylistState before;
				before.playlist = playlist;
				before.before = &playlist->get_state();
				playlist->clear_changes ();
				playlist->freeze ();
				playlists.push_back(before);
			}
		}
	}

	/* Create all remaining missing PT tracks and freeze playlists of those */
	for (vector<PTFFormat::track_t>::const_iterator a = ptf.tracks ().begin (); a != ptf.tracks ().end (); ++a) {
		if (!track_map[a->name]) {

			/* Create missing track */
			DEBUG_TRACE (DEBUG::PTImport, string_compose ("Create tr(%1) %2ch '%3'\n", tr_multi[a->name].second, tr_multi[a->name].first, a->name));
			ok = new_audio_routes_tracks_bulk (routes,
							   tracks,
							   tr_multi[a->name].first,
							   std::max (2, tr_multi[a->name].first),
							   0,
							   1,
							   a->name.c_str (),
							   PresentationInfo::max_order,
							   Normal
							  );

			if (ok) {
				existing_track = tracks.back();

				track_map[a->name] = existing_track;
				std::shared_ptr<Playlist> playlist = existing_track->playlist();

				PlaylistState before;
				before.playlist = playlist;
				before.before = &playlist->get_state();
				playlist->clear_changes ();
				playlist->freeze ();
				playlists.push_back(before);
			}
		}
	}

	/* Finish bringing in routes */
	if (!routes.empty ()) {
		add_routes (routes, true, true, PresentationInfo::max_order);
	}

	/* Add regions (already done) */

	/* Iterate over all pt region -> track entries  */
	for (vector<PTFFormat::track_t>::const_iterator a = ptf.tracks ().begin (); a != ptf.tracks ().end (); ++a) {
		/* Select only one relevant pt track from a multichannel track */
		if (a->index == tr_multi[a->name].second) {
			for (vector<Session::PtfLookup>::iterator p = ptfregpair.begin ();
			     p != ptfregpair.end (); ++p) {
				if (p->index1 == a->reg.index)  {

					/* Matched a ptf active region to an ardour region */
					std::shared_ptr<Region> r = RegionFactory::region_by_id (p->id);
					DEBUG_TRACE (DEBUG::PTImport, string_compose ("wav(%1) reg(%2) tr(%3)-%4ch '%5'\n", a->reg.name, a->reg.index, a->index, tr_multi[a->name].first, a->name));

					/* Use audio track we know exists */
					existing_track = track_map[a->name];
					assert (existing_track);

					/* Put on existing track */
					std::shared_ptr<Playlist> playlist = existing_track->playlist ();
					std::shared_ptr<Region> copy (RegionFactory::create (r, true));
					playlist->clear_changes ();
					playlist->add_region (copy, timepos_t (a->reg.startpos));
					//add_command (new StatefulDiffCommand (playlist));

					/* Collect latest end of all regions */
					timepos_t end_of_region = timepos_t (a->reg.startpos + a->reg.length);
					if (latest < end_of_region) {
						latest = end_of_region;
					}
				}
			}
		}
	}

	track_map.clear ();

	maybe_update_session_range (timepos_t (0), latest);

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

	std::map <int, std::shared_ptr<MidiTrack> > midi_tracks;
	/* MIDI - Create unique midi tracks and a lookup table for used tracks */
	for (vector<midipair>::iterator a = uniquetr.begin (); a != uniquetr.end (); ++a) {
		Session::PtfLookup miditr;
		list<std::shared_ptr<MidiTrack> > mt (new_midi_track (
				ChanCount (DataType::MIDI, 1),
				ChanCount (DataType::MIDI, 1),
				true,
				instrument, (Plugin::PresetRecord*) 0,
				nullptr,
				1,
				a->trname,
				PresentationInfo::max_order,
				Normal, true));
		assert (mt.size () == 1);
		midi_tracks[a->ptfindex] = mt.front ();
	}

	/* MIDI - Add midi regions one-by-one to corresponding midi tracks */
	for (vector<PTFFormat::track_t>::const_iterator a = ptf.miditracks ().begin (); a != ptf.miditracks ().end (); ++a) {

		std::shared_ptr<MidiTrack> midi_track = midi_tracks[a->index];
		assert (midi_track);
		std::shared_ptr<Playlist> playlist = midi_track->playlist ();
		samplepos_t f = (samplepos_t)a->reg.startpos * srate / 1920000.;
		samplecnt_t length = (samplecnt_t)a->reg.length * srate / 1920000.;
		MusicSample pos (f, 0);
		std::shared_ptr<Source> src = create_midi_source_by_stealing_name (midi_track);
		PropertyList plist;
		plist.add (ARDOUR::Properties::start, 0);
		plist.add (ARDOUR::Properties::length, length);
		plist.add (ARDOUR::Properties::name, PBD::basename_nosuffix (src->name ()));
		//printf(" : %d - trackname: (%s)\n", a->index, src->name ().c_str ());
		std::shared_ptr<Region> region = (RegionFactory::create (src, plist));
		/* sets position */
		region->set_position (timepos_t (pos.sample));
		midi_track->playlist ()->add_region (region, timepos_t (pos.sample), 1.0, false);

		std::shared_ptr<MidiRegion> mr = std::dynamic_pointer_cast<MidiRegion>(region);
		std::shared_ptr<MidiModel> mm = mr->midi_source (0)->model ();
		MidiModel::NoteDiffCommand *midicmd;
		midicmd = mm->new_note_diff_command ("Import ProTools MIDI");

		for (vector<PTFFormat::midi_ev_t>::const_iterator j = a->reg.midi.begin (); j != a->reg.midi.end (); ++j) {
			//printf(" : MIDI : pos=%f len=%f\n", (float)j->pos / 960000., (float)j->length / 960000.);
			Temporal::Beats start = Temporal::Beats::from_double (j->pos / 960000.);
			Temporal::Beats len = Temporal::Beats::from_double(j->length / 960000.);
			/* PT C-2 = 0, Ardour C-1 = 0, subtract twelve to convert ? */
			midicmd->add (std::shared_ptr<Evoral::Note<Temporal::Beats> > (new Evoral::Note<Temporal::Beats> ((uint8_t)1, start, len, j->note, j->velocity)));
		}
		mm->apply_diff_command_only (midicmd);
		delete midicmd;
		std::shared_ptr<Region> copy (RegionFactory::create (mr, true));
		playlist->clear_changes ();
		playlist->add_region (copy, timepos_t (f));
	}
}
