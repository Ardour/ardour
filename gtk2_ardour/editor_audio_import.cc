/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2007 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2014 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2015-2017 Nick Mainsbridge <mainsbridge@gmail.com>
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

#include <sndfile.h>

#include "pbd/integer_division.h"
#include "pbd/pthread_utils.h"
#include "pbd/basename.h"
#include "pbd/shortpath.h"
#include "pbd/stateful_diff_command.h"

#include "widgets/choice.h"

#include "ardour/audio_track.h"
#include "ardour/audiofilesource.h"
#include "ardour/audioregion.h"
#include "ardour/midi_region.h"
#include "ardour/midi_track.h"
#include "ardour/operations.h"
#include "ardour/profile.h"
#include "ardour/region_factory.h"
#include "ardour/smf_source.h"
#include "ardour/source_factory.h"
#include "ardour/triggerbox.h"
#include "ardour/utils.h"
#include "pbd/memento_command.h"

#include "ardour_message.h"
#include "ardour_ui.h"
#include "cursor_context.h"
#include "editor.h"
#include "sfdb_ui.h"
#include "editing.h"
#include "audio_time_axis.h"
#include "midi_time_axis.h"
#include "session_import_dialog.h"
#include "tempo_map_change.h"
#include "gui_thread.h"
#include "interthread_progress_window.h"
#include "mouse_cursors.h"
#include "editor_cursors.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Editing;
using namespace Temporal;

using std::string;

/* Functions supporting the incorporation of external (non-captured) audio material into ardour */

void
Editor::add_external_audio_action (ImportMode mode_hint)
{
	if (_session == 0) {
		ArdourMessageDialog msg (_("You can't import or embed an audiofile until you have a session loaded."));
		msg.run ();
		return;
	}

	if (sfbrowser == 0) {
		sfbrowser = new SoundFileOmega (_("Add Existing Media"), _session, 0, 0, true, mode_hint);
	} else {
		sfbrowser->set_mode (mode_hint);
	}

	external_audio_dialog ();
}

void
Editor::external_audio_dialog ()
{
	vector<string> paths;
	uint32_t audio_track_cnt;
	uint32_t midi_track_cnt;

	if (_session == 0) {
		ArdourMessageDialog msg (_("You can't import or embed an audiofile until you have a session loaded."));
		msg.run ();
		return;
	}

	audio_track_cnt = 0;
	midi_track_cnt = 0;

	for (TrackSelection::iterator x = selection->tracks.begin(); x != selection->tracks.end(); ++x) {
		AudioTimeAxisView* atv = dynamic_cast<AudioTimeAxisView*>(*x);

		if (atv) {
			if (atv->is_audio_track()) {
				audio_track_cnt++;
			}

		} else {
			MidiTimeAxisView* mtv = dynamic_cast<MidiTimeAxisView*>(*x);

			if (mtv) {
				if (mtv->is_midi_track()) {
					midi_track_cnt++;
				}
			}
		}
	}

	if (sfbrowser == 0) {
		sfbrowser = new SoundFileOmega (_("Add Existing Media"), _session, audio_track_cnt, midi_track_cnt, true);
	} else {
		sfbrowser->reset (audio_track_cnt, midi_track_cnt);
	}

	sfbrowser->show_all ();
}

void
Editor::session_import_dialog ()
{
	SessionImportDialog dialog (_session);
	dialog.run ();
}

typedef std::map<PBD::ID,std::shared_ptr<ARDOUR::Source> > SourceMap;

/**
 * Updating is still disabled, see note in libs/ardour/import.cc Session::import_files()
 *
 * all_or_nothing:
 *   true  = show "Update", "Import" and "Skip"
 *   false = show "Import", and "Cancel"
 *
 * Returns:
 *     0  To update an existing source of the same name
 *     1  To import/embed the file normally (make sure the new name will be unique)
 *     2  If the user wants to skip this file
 **/
int
Editor::check_whether_and_how_to_import(string path, bool all_or_nothing)
{
	string wave_name (Glib::path_get_basename(path));

	bool already_exists = false;
	uint32_t existing;

	if ((existing = _session->count_sources_by_origin (path)) > 0) {
		already_exists = true;
	}

	int function = 1;

	if (already_exists) {
		string message;
		if (all_or_nothing) {
			// updating is still disabled
			//message = string_compose(_("The session already contains a source file named %1. Do you want to update that file (and thus all regions using the file) or import this file as a new file?"),wave_name);
			message = string_compose (_("The session already contains a source file named %1.  Do you want to import %1 as a new file, or skip it?"), wave_name);
		} else {
			message = string_compose (_("The session already contains a source file named %1.  Do you want to import %2 as a new source, or skip it?"), wave_name, wave_name);

		}
		ArdourMessageDialog dialog(message, false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_NONE, true);

		if (all_or_nothing) {
			// disabled
			//dialog.add_button("Update", 0);
			dialog.add_button("Import", 1);
			dialog.add_button("Skip",   2);
		} else {
			dialog.add_button("Import", 1);
			dialog.add_button("Cancel", 2);
		}

		//dialog.add_button("Skip all", 4); // All or rest?

		function = dialog.run ();
		dialog.hide();
	}

	return function;
}

std::shared_ptr<AudioTrack>
Editor::get_nth_selected_audio_track (int nth) const
{
	AudioTimeAxisView* atv;
	TrackSelection::iterator x;

	for (x = selection->tracks.begin(); nth > 0 && x != selection->tracks.end(); ++x) {

		atv = dynamic_cast<AudioTimeAxisView*>(*x);

		if (!atv) {
			continue;
		} else if (atv->is_audio_track()) {
			--nth;
		}
	}

	if (x == selection->tracks.end()) {
		atv = dynamic_cast<AudioTimeAxisView*>(selection->tracks.back());
	} else {
		atv = dynamic_cast<AudioTimeAxisView*>(*x);
	}

	if (!atv || !atv->is_audio_track()) {
		return std::shared_ptr<AudioTrack>();
	}

	return atv->audio_track();
}

std::shared_ptr<MidiTrack>
Editor::get_nth_selected_midi_track (int nth) const
{
	MidiTimeAxisView* mtv;
	TrackSelection::iterator x;

	for (x = selection->tracks.begin(); nth > 0 && x != selection->tracks.end(); ++x) {

		mtv = dynamic_cast<MidiTimeAxisView*>(*x);

		if (!mtv) {
			continue;
		} else if (mtv->is_midi_track()) {
			--nth;
		}
	}

	if (x == selection->tracks.end()) {
		mtv = dynamic_cast<MidiTimeAxisView*>(selection->tracks.back());
	} else {
		mtv = dynamic_cast<MidiTimeAxisView*>(*x);
	}

	if (!mtv || !mtv->is_midi_track()) {
		return std::shared_ptr<MidiTrack>();
	}

	return mtv->midi_track();
}

void
Editor::import_smf_tempo_map (Evoral::SMF const & smf, timepos_t const & pos)
{
	if (!_session) {
		return;
	}

	bool provided;
	TempoMap::WritableSharedPtr new_map (smf.tempo_map (provided));

	if (!provided) {
		return;
	}

	TempoMap::WritableSharedPtr wmap = TempoMap::write_copy ();
	TempoMapCutBuffer* tmcb;
	// XMLNode& tm_before (wmap->get_state());

	tmcb = new_map->copy (timepos_t (0), timepos_t::max (Temporal::AudioTime));

	if (tmcb && !tmcb->empty()) {
		wmap->paste (*tmcb, pos, false, _("import"));
		TempoMap::update (wmap);
		delete tmcb;
		// XMLNode& tm_after (wmap->get_state());
		// _session->add_command (new TempoCommand (_("cut tempo map"), &tm_before, &tm_after));
	} else {
		// delete &tm_before;
		TempoMap::abort_update ();
	}
}

void
Editor::do_import (vector<string>           paths,
                   ImportDisposition        disposition,
                   ImportMode               mode,
                   SrcQuality               quality,
                   MidiTrackNameSource      midi_track_name_source,
                   MidiTempoMapDisposition  smf_tempo_disposition,
                   timepos_t&               pos,
                   ARDOUR::PluginInfoPtr    instrument,
                   std::shared_ptr<Track> track,
                   bool                     with_markers)
{
	vector<string> to_import;
	int nth = 0;
	bool use_timestamp = (pos == timepos_t::max (pos.time_domain()));
	std::string const& pgroup_id = Playlist::generate_pgroup_id ();

	/* XXX nutempo2: we will import markers using music (beat) time, which
	   will make any imported tempo map irrelevant. Not doing that (in 6.7,
	   before nutempo2) is much more complicated because we don't know
	   which file may have the tempo map, and if we're importing that
	   it will change the marker positions. So for now, there's an implicit
	   limitation that if you import more than 1 MIDI file and the first
	   has markers but the second has the tempo map, the markers could be
	   in the wrong position.
	*/

	if (smf_tempo_disposition == SMFTempoUse) {

		bool tempo_map_done = false;

		for (vector<string>::iterator a = paths.begin(); a != paths.end(); ++a) {

			Evoral::SMF smf;

			if (smf.open (*a, 1, false)) {
				continue;
			}

			/* Find the first MIDI file with a tempo map, and import it
			   before we do anything else.
			*/
			if (!tempo_map_done && smf_tempo_disposition == SMFTempoUse) {
				if (smf.num_tempos() > 0) {
					import_smf_tempo_map (smf, pos);
					tempo_map_done = true;
				}
			}

			smf.close ();
		}
	}

	current_interthread_info = &import_status;
	import_status.current = 1;
	import_status.total = paths.size ();
	import_status.all_done = false;
	import_status.midi_track_name_source = midi_track_name_source;

	ImportProgressWindow ipw (&import_status, _("Import"), _("Cancel Import"));

	if (disposition == Editing::ImportMergeFiles) {

		/* create 1 region from all paths, add to 1 track,
		   ignore "track"
		*/

		bool cancel = false;
		for (vector<string>::iterator a = paths.begin(); a != paths.end(); ++a) {
			int check = check_whether_and_how_to_import(*a, false);
			if (check == 2) {
				cancel = true;
				break;
			}
		}

		if (!cancel) {
			ipw.show ();
			import_sndfiles (paths, disposition, mode, quality, pos, 1, 1, track, pgroup_id, false, with_markers, instrument);
			import_status.clear();
		}

	} else {

		bool replace = false;

		for (vector<string>::iterator a = paths.begin(); a != paths.end() && !import_status.cancel; ++a) {

			const int check = check_whether_and_how_to_import (*a, true);

			switch (check) {
			case 2:
				// user said skip
				continue;
			case 0:
				fatal << "Updating existing sources should be disabled!" << endmsg;
				abort(); /* NOTREACHED*/
				break;
			case 1:
				replace = false;
				break;
			default:
				fatal << "Illegal return " << check <<  " from check_whether_and_how_to_import()!" << endmsg;
				abort(); /* NOTREACHED*/
			}

			/* have to reset this for every file we handle */

			if (use_timestamp) {
				pos = timepos_t::max (pos.time_domain());
			}

			ipw.show ();

			switch (disposition) {
			case Editing::ImportDistinctFiles:

				to_import.clear ();
				to_import.push_back (*a);

				if (mode == Editing::ImportToTrack) {
					track = get_nth_selected_audio_track (nth++);
				}

				import_sndfiles (to_import, disposition, mode, quality, pos, 1, -1, track, pgroup_id, replace, with_markers, instrument);
				import_status.clear();
				break;

			case Editing::ImportDistinctChannels:

				to_import.clear ();
				to_import.push_back (*a);

				import_sndfiles (to_import, disposition, mode, quality, pos, -1, -1, track, pgroup_id, replace, with_markers, instrument);
				import_status.clear();
				break;

			case Editing::ImportSerializeFiles:

				to_import.clear ();
				to_import.push_back (*a);

				import_sndfiles (to_import, disposition, mode, quality, pos, 1, 1, track, pgroup_id, replace, with_markers, instrument);
				import_status.clear();
				break;

			case Editing::ImportMergeFiles:
				// Not entered, handled in earlier if() branch
				break;
			}
		}
	}

	import_status.all_done = true;
}

void
Editor::do_embed (vector<string>           paths,
                  ImportDisposition        import_as,
                  ImportMode               mode,
                  timepos_t&               pos,
                  ARDOUR::PluginInfoPtr    instrument,
                  std::shared_ptr<Track> track)
{
	bool check_sample_rate = true;
	vector<string> to_embed;
	bool multi = paths.size() > 1;
	int nth = 0;
	bool use_timestamp = (pos == timepos_t::max (pos.time_domain()));
	std::string const& pgroup_id = Playlist::generate_pgroup_id ();

	switch (import_as) {
	case Editing::ImportDistinctFiles:
		for (vector<string>::iterator a = paths.begin(); a != paths.end(); ++a) {

			/* have to reset this for every file we handle */
			if (use_timestamp) {
				pos = timepos_t::max (pos.time_domain());
			}

			to_embed.clear ();
			to_embed.push_back (*a);

			if (mode == Editing::ImportToTrack) {
				track = get_nth_selected_audio_track (nth++);
			}

			if (embed_sndfiles (to_embed, multi, check_sample_rate, import_as, mode, pos, 1, -1, track, pgroup_id, instrument) < -1) {
				/* error, bail out */
				return;
			}
		}
		break;

	case Editing::ImportDistinctChannels:
		for (vector<string>::iterator a = paths.begin(); a != paths.end(); ++a) {

			/* have to reset this for every file we handle */
			if (use_timestamp) {
				pos = timepos_t::max (pos.time_domain());
			}

			to_embed.clear ();
			to_embed.push_back (*a);

			if (embed_sndfiles (to_embed, multi, check_sample_rate, import_as, mode, pos, -1, -1, track, pgroup_id, instrument) < -1) {
				/* error, bail out */
				return;
			}
		}
		break;

	case Editing::ImportMergeFiles:
		if (embed_sndfiles (paths, multi, check_sample_rate, import_as, mode, pos, 1, 1, track, pgroup_id, instrument) < -1) {
			/* error, bail out */
			return;
		}
		break;

	case Editing::ImportSerializeFiles:
		for (vector<string>::iterator a = paths.begin(); a != paths.end(); ++a) {

			/* have to reset this for every file we handle */
			if (use_timestamp) {
				pos = timepos_t::max (pos.time_domain());
			}

			to_embed.clear ();
			to_embed.push_back (*a);

			if (embed_sndfiles (to_embed, multi, check_sample_rate, import_as, mode, pos, 1, 1, track, pgroup_id, instrument) < -1) {
				/* error, bail out */
				return;
			}
		}
		break;
	}
}

int
Editor::import_sndfiles (vector<string>            paths,
                         ImportDisposition         disposition,
                         ImportMode                mode,
                         SrcQuality                quality,
                         timepos_t&                pos,
                         int                       target_regions,
                         int                       target_tracks,
                         std::shared_ptr<Track>& track,
                         std::string const&        pgroup_id,
                         bool                      replace,
                         bool                      with_markers,
                         ARDOUR::PluginInfoPtr     instrument)
{
	/* skip periodic saves while importing */
	Session::StateProtector sp (_session);

	import_status.paths = paths;
	import_status.done = false;
	import_status.freeze = false;
	import_status.quality = quality;
	import_status.replace_existing_source = replace;
	import_status.split_midi_channels = (disposition == Editing::ImportDistinctChannels);
	import_status.import_markers = with_markers;

	import_status.mode = mode;
	import_status.pos = pos;
	import_status.target_tracks = target_tracks;
	import_status.target_regions = target_regions;
	import_status.track = track;
	import_status.replace = replace;

	CursorContext::Handle cursor_ctx = CursorContext::create(*this, _cursors->wait);
	gdk_flush ();

	/* start import thread for this spec. this will ultimately call Session::import_files()
	   which, if successful, will add the files as regions to the region list. its up to us
	   (the GUI) to direct additional steps after that.
	*/

	pthread_create_and_store ("import", &import_status.thread, _import_thread, this);
	pthread_detach (import_status.thread);

	while (!import_status.done && !import_status.cancel) {
		gtk_main_iteration ();
	}

	// wait for thread to terminate
	while (!import_status.done) {
		gtk_main_iteration ();
	}

	int result = -1;

	if (!import_status.cancel && !import_status.sources.empty()) {
		result = add_sources (
			import_status.paths,
			import_status.sources,
			import_status.pos,
			disposition,
			import_status.mode,
			import_status.target_regions,
			import_status.target_tracks,
			track, pgroup_id, false, instrument
			);

		/* update position from results */

		pos = import_status.pos;
	}

	return result;
}

int
Editor::embed_sndfiles (vector<string>            paths,
                        bool                      multifile,
                        bool&                     check_sample_rate,
                        ImportDisposition         disposition,
                        ImportMode                mode,
                        timepos_t&              pos,
                        int                       target_regions,
                        int                       target_tracks,
                        std::shared_ptr<Track>& track,
                        std::string const&        pgroup_id,
                        ARDOUR::PluginInfoPtr     instrument)
{
	std::shared_ptr<AudioFileSource> source;
	SourceList sources;
	string linked_path;
	SoundFileInfo finfo;

	/* skip periodic saves while importing */
	Session::StateProtector sp (_session);

	CursorContext::Handle cursor_ctx = CursorContext::create(*this, _cursors->wait);
	gdk_flush ();

	for (vector<string>::iterator p = paths.begin(); p != paths.end(); ++p) {

		string path = *p;
		string error_msg;

		/* note that we temporarily truncated _id at the colon */

		if (!AudioFileSource::get_soundfile_info (path, finfo, error_msg)) {
			error << string_compose(_("Editor: cannot open file \"%1\", (%2)"), path, error_msg ) << endmsg;
			return -3;
		}

		if (!finfo.seekable) {
			ArdourMessageDialog msg (string_compose (_("%1\nThis audiofile cannot be embedded. It must be imported!"), short_path (path, 40)), false, Gtk::MESSAGE_ERROR);
			msg.run ();
			return -2;
		}

		if (check_sample_rate  && (finfo.samplerate != (int) _session->sample_rate())) {
			vector<string> choices;

			if (multifile) {
				choices.push_back (_("Cancel entire import"));
				choices.push_back (_("Don't embed it"));
				choices.push_back (_("Embed all without questions"));

				ArdourWidgets::Choice rate_choice (
					_("Sample Rate"),
					string_compose (_("%1\nThis audiofile's sample rate doesn't match the session sample rate!"),
							short_path (path, 40)),
					choices, false
					);

				int resx = rate_choice.run ();

				switch (resx) {
				case 0: /* stop a multi-file import */
					return -2;
				case 1: /* don't embed this one */
					return -1;
				case 2: /* do it, and the rest without asking */
					check_sample_rate = false;
					break;
				case 3: /* do it */
					break;
				default:
					return -2;
				}
			} else {
				choices.push_back (_("Cancel"));
				choices.push_back (_("Embed it anyway"));

				ArdourWidgets::Choice rate_choice (
					_("Sample Rate"),
					string_compose (_("%1\nThis audiofile's sample rate doesn't match the session sample rate!"), path),
					choices, false
					);

				int resx = rate_choice.run ();

				switch (resx) {
				case 0: /* don't import */
					return -1;
				case 1: /* do it */
					break;
				default:
					return -2;
				}
			}
		}

		for (int n = 0; n < finfo.channels; ++n) {

			try {

				/* check if we have this thing embedded already */

				std::shared_ptr<Source> s;

				if ((s = _session->audio_source_by_path_and_channel (path, n)) == 0) {

					source = std::dynamic_pointer_cast<AudioFileSource> (
						SourceFactory::createExternal (DataType::AUDIO, *_session,
									       path, n,
						                               Source::Flag (0),
									true, true));
				} else {
					source = std::dynamic_pointer_cast<AudioFileSource> (s);
				}

				sources.push_back(source);
			}

			catch (failed_constructor& err) {
				error << string_compose(_("could not open %1"), path) << endmsg;
				return -3;
			}

			gtk_main_iteration();
		}
	}

	if (!sources.empty()) {
		return add_sources (paths, sources, pos, disposition, mode, target_regions, target_tracks, track, pgroup_id, true, instrument);
	}

	return 0;
}

int
Editor::add_sources (vector<string>            paths,
                     SourceList&               sources,
                     timepos_t&              pos,
                     ImportDisposition         disposition,
                     ImportMode                mode,
                     int                       target_regions,
                     int                       target_tracks,
                     std::shared_ptr<Track>& track,
                     std::string const&        pgroup_id,
                     bool                      /*add_channel_suffix*/,
                     ARDOUR::PluginInfoPtr     instrument)
{
	vector<std::shared_ptr<Region> > regions;
	string region_name;
	uint32_t input_chan = 0;
	uint32_t output_chan = 0;
	bool use_timestamp;
	vector<string> track_names;

	use_timestamp = (pos == timepos_t::max (pos.time_domain()));

	// kludge (for MIDI we're abusing "channel" for "track" here)
	if (SMFSource::safe_midi_file_extension (paths.front())) {
		target_regions = -1;
	}

	if (target_regions == 1) {

		/* take all the sources we have and package them up as a region */

		region_name = region_name_from_path (paths.front(), (sources.size() > 1), false);

		/* we checked in import_sndfiles() that there were not too many */

		while (RegionFactory::region_by_name (region_name)) {
			region_name = bump_name_once (region_name, '.');
		}

		PropertyList plist;

		plist.add (ARDOUR::Properties::start, timecnt_t (sources[0]->type() == DataType::AUDIO ? Temporal::AudioTime : Temporal::BeatTime));
		plist.add (ARDOUR::Properties::length, sources[0]->length ());
		plist.add (ARDOUR::Properties::name, region_name);
		plist.add (ARDOUR::Properties::layer, 0);
		plist.add (ARDOUR::Properties::whole_file, true);
		plist.add (ARDOUR::Properties::external, true);
		plist.add (ARDOUR::Properties::opaque, true);

		std::shared_ptr<Region> r = RegionFactory::create (sources, plist);

		if (std::dynamic_pointer_cast<AudioRegion>(r)) {
			std::dynamic_pointer_cast<AudioRegion>(r)->special_set_position(sources[0]->natural_position());
		}

		regions.push_back (r);

		/* if we're creating a new track, name it after the cleaned-up
		 * and "merged" region name.
		 */

		track_names.push_back (region_name);

	} else if (target_regions == -1 || target_regions > 1) {

		/* take each source and create a region for each one */

		SourceList just_one;
		SourceList::iterator x;
		uint32_t n;

		for (n = 0, x = sources.begin(); x != sources.end(); ++x, ++n) {

			just_one.clear ();
			just_one.push_back (*x);

			std::shared_ptr<FileSource> fs = std::dynamic_pointer_cast<FileSource> (*x);

			if (sources.size() > 1 && disposition == ImportDistinctChannels) {

				/* generate a per-channel region name so that things work as
				 * intended
				 */

				string path;

				if (fs) {
					region_name = basename_nosuffix (fs->path());
				} else {
					region_name = (*x)->name();
				}

				if (sources.size() == 2) {
					if (n == 0) {
						region_name += "-L";
					} else {
						region_name += "-R";
					}
				} else if (sources.size() > 2) {
					region_name += string_compose ("-%1", n+1);
				}

				track_names.push_back (region_name);

			} else {
				if (fs) {
					region_name = region_name_from_path (fs->path(), false, false, sources.size(), n);
				} else {
					region_name = (*x)->name();
				}

				if (SMFSource::safe_midi_file_extension (paths.front())) {
					string track_name = string_compose ("%1-t%2", PBD::basename_nosuffix (fs->path()), (n + 1));
					track_names.push_back (track_name);
				} else {
					track_names.push_back (PBD::basename_nosuffix (paths[n]));
				}
			}

			PropertyList plist;

			/* Fudge region length to ensure it is non-zero; make it 1 beat at 120bpm
			   for want of a better idea.
			*/
			timepos_t len = (*x)->length ();
			cerr << "for " << (*x)->name() << " source length appears to be " << len << endl;
			if (len.is_zero()) {
				if ((*x)->type() == DataType::AUDIO) {
					len = timepos_t (_session->sample_rate () / 2);
				} else {
					len = timepos_t (Beats (1, 0));
				}
				cerr << " reset to use " << len << endl;
			}

			plist.add (ARDOUR::Properties::start, timepos_t ((*x)->type() == DataType::AUDIO ? Temporal::AudioTime : Temporal::BeatTime));
			plist.add (ARDOUR::Properties::length, len);
			plist.add (ARDOUR::Properties::name, region_name);
			plist.add (ARDOUR::Properties::layer, 0);
			plist.add (ARDOUR::Properties::whole_file, true);
			plist.add (ARDOUR::Properties::external, true);
			plist.add (ARDOUR::Properties::opaque, true);

			std::shared_ptr<Region> r = RegionFactory::create (just_one, plist);

			if (std::dynamic_pointer_cast<AudioRegion>(r)) {
				std::dynamic_pointer_cast<AudioRegion>(r)->special_set_position((*x)->natural_position());
			}

			regions.push_back (r);
		}
	}

	if (target_regions == 1) {
		input_chan = regions.front()->sources().size();
	} else {
		if (target_tracks == 1) {
			input_chan = regions.size();
		} else {
			input_chan = 1;
		}
	}

#ifdef MIXBUS
	if (mode == ImportAsTrigger) {
		/* Mixbus will only ever use stereo tracks when using DnD to import to triggers */
		input_chan = 2;
	}
#endif

	if (Config->get_output_auto_connect() & AutoConnectMaster) {
		output_chan = (_session->master_out() ? _session->master_out()->n_inputs().n_audio() : input_chan);
	} else {
		output_chan = input_chan;
	}

	int n = 0;
	timecnt_t rlen;

	begin_reversible_command (Operations::insert_file);

	/* we only use tracks names when importing to new tracks, but we
	 * require that one is defined for every region, just to keep
	 * the API simpler.
	 */
	assert (regions.size() == track_names.size());

	for (vector<std::shared_ptr<Region> >::iterator r = regions.begin(); r != regions.end(); ++r, ++n) {
		std::shared_ptr<AudioRegion> ar = std::dynamic_pointer_cast<AudioRegion> (*r);

		if (use_timestamp) {
			if (ar) {

				/* get timestamp for this region */

				const std::shared_ptr<Source> s (ar->sources().front());
				const std::shared_ptr<AudioSource> as = std::dynamic_pointer_cast<AudioSource> (s);

				assert (as);

				if (as->natural_position() != 0) {
					pos = as->natural_position();
				} else if (target_tracks == 1) {
					/* hmm, no timestamp available, put it after the previous region
					 */
					if (n == 0) {
						pos = get_preferred_edit_position ();
					} else {
						pos += rlen;
					}
				} else {
					pos = get_preferred_edit_position ();
				}
			} else {
				/* should really get first position in MIDI file, but for now, use edit position*/
				pos = get_preferred_edit_position ();
			}
		}

		if (track_names.size() > 2 && current_interthread_info) {
			import_status.current = n;
			import_status.total = track_names.size ();
			import_status.progress = 0.5;
			import_status.doing_what = "Creating Tracks";
			ARDOUR::GUIIdle ();
		}
		finish_bringing_in_material (*r, input_chan, output_chan, pos, mode, track, track_names[n], pgroup_id, instrument);

		rlen = (*r)->length();

		if (target_tracks != 1) {
			track.reset ();
		} else {
			if (!use_timestamp || !ar) {
				/* line each one up right after the other */
				pos += (*r)->length();
			}
		}
	}

	commit_reversible_command ();

	/* setup peak file building in another thread */

	for (SourceList::iterator x = sources.begin(); x != sources.end(); ++x) {
		SourceFactory::setup_peakfile (*x, true);
	}

	return 0;
}

int
Editor::finish_bringing_in_material (std::shared_ptr<Region> region,
                                     uint32_t                  in_chans,
                                     uint32_t                  out_chans,
                                     timepos_t&                pos,
                                     ImportMode                mode,
                                     std::shared_ptr<Track>& existing_track,
                                     string const&             new_track_name,
                                     string const&             pgroup_id,
                                     ARDOUR::PluginInfoPtr     instrument)
{
	std::shared_ptr<AudioRegion> ar = std::dynamic_pointer_cast<AudioRegion>(region);
	std::shared_ptr<MidiRegion> mr = std::dynamic_pointer_cast<MidiRegion>(region);

	switch (mode) {
	case ImportAsRegion:
		/* relax, its been done */
		break;

	case ImportToTrack:
	{
		if (!existing_track) {

			if (ar) {
				existing_track = get_nth_selected_audio_track (0);
			} else if (mr) {
				existing_track = get_nth_selected_midi_track (0);
			}

			if (!existing_track) {
				return -1;
			}
		}

		std::shared_ptr<Playlist> playlist = existing_track->playlist();
		std::shared_ptr<Region> copy (RegionFactory::create (region, region->derive_properties ()));
		copy->set_region_group(Region::get_retained_group_id());
		playlist->clear_changes ();
		playlist->clear_owned_changes ();
		playlist->add_region (copy, pos);

		if (should_ripple()) {
			do_ripple (playlist, pos, copy->length(), copy, true);
		} else {
			playlist->rdiff_and_add_command (_session);
		}

		break;
	}

	case ImportAsTrigger:
	/* fallthrough */
	case ImportAsTrack:
	{
		if (!existing_track) {
			if (ar) {
				list<std::shared_ptr<AudioTrack> > at (
					_session->new_audio_track (in_chans, out_chans,
					                           0, 1, string(),
					                           PresentationInfo::max_order,
					                           Normal,
					                           true,
					                           mode == ImportAsTrigger
					));
				if (at.empty()) {
					return -1;
				}
				for (list<std::shared_ptr<AudioTrack> >::iterator i = at.begin(); i != at.end(); ++i) {
					if (Config->get_strict_io ()) {
						(*i)->set_strict_io (true);
					}
					(*i)->playlist()->set_pgroup_id (pgroup_id);
				}

				existing_track = at.front();
			} else if (mr) {
				list<std::shared_ptr<MidiTrack> > mt (
					_session->new_midi_track (ChanCount (DataType::MIDI, 1),
					                          ChanCount (DataType::MIDI, 1),
					                          Config->get_strict_io () || Profile->get_mixbus (),
					                          instrument, (Plugin::PresetRecord*) 0,
					                          (RouteGroup*) 0,
					                          1,
					                          string(),
					                          PresentationInfo::max_order,
					                          Normal,
					                          true,
					                          mode == ImportAsTrigger
						));

				if (mt.empty()) {
					return -1;
				}

				for (list<std::shared_ptr<MidiTrack> >::iterator i = mt.begin(); i != mt.end(); ++i) {
					if (Config->get_strict_io ()) {
						(*i)->set_strict_io (true);
					}
					(*i)->playlist()->set_pgroup_id (pgroup_id);
				}

				existing_track = mt.front();
			}

			if (!new_track_name.empty()) {
				existing_track->set_name (new_track_name);
			} else {
				existing_track->set_name (region->name());
			}
		}

		if (mode == ImportAsTrigger) {
			std::shared_ptr<Region> copy (RegionFactory::create (region, true));
			for (int s = 0; s < TriggerBox::default_triggers_per_box; ++s) {
				if (!existing_track->triggerbox ()->trigger (s)->region ()) {
					existing_track->triggerbox ()->set_from_selection (s, copy);
#if 1 /* assume drop from sidebar */
					ARDOUR_UI_UTILS::copy_patch_changes (_session->the_auditioner (), existing_track->triggerbox ()->trigger (s));
#endif
					break;
				}
			}
		} else {
			std::shared_ptr<Playlist> playlist = existing_track->playlist();
			playlist->clear_changes ();
			playlist->add_region (region, pos);
			_session->add_command (new StatefulDiffCommand (playlist));
		}
		break;
	}

	}

	return 0;
}

void *
Editor::_import_thread (void *arg)
{
	SessionEvent::create_per_thread_pool ("import events", 64);

	Editor *ed = (Editor *) arg;
	return ed->import_thread ();
}

void *
Editor::import_thread ()
{
	Temporal::TempoMap::fetch ();

	_session->import_files (import_status);
	return 0;
}
