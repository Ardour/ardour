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
#include "gui_thread.h"
#include "interthread_progress_window.h"
#include "mouse_cursors.h"
#include "editor_cursors.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Editing;
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

typedef std::map<PBD::ID,boost::shared_ptr<ARDOUR::Source> > SourceMap;

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

boost::shared_ptr<AudioTrack>
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
		return boost::shared_ptr<AudioTrack>();
	}

	return atv->audio_track();
}

boost::shared_ptr<MidiTrack>
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
		return boost::shared_ptr<MidiTrack>();
	}

	return mtv->midi_track();
}

void
Editor::import_smf_tempo_map (Evoral::SMF const & smf, samplepos_t pos)
{
	if (!_session) {
		return;
	}

	const size_t num_tempos = smf.num_tempos ();

	if (num_tempos == 0) {
		return;
	}

	const samplecnt_t sample_rate = _session->sample_rate ();
	TempoMap new_map (sample_rate);
	Meter last_meter (4.0, 4.0);
	bool have_initial_meter = false;

	for (size_t n = 0; n < num_tempos; ++n) {

		Evoral::SMF::Tempo* t = smf.nth_tempo (n);
		assert (t);

		Tempo tempo (t->tempo(), 32.0 / (double) t->notes_per_note);
		Meter meter (t->numerator, t->denominator);
		Timecode::BBT_Time bbt; /* 1|1|0 which is correct for the no-meter case */

		if (have_initial_meter) {
			new_map.add_tempo (tempo, t->time_pulses/ (double)smf.ppqn() / 4.0, 0, MusicTime);
			if (!(meter == last_meter)) {
				bbt = new_map.bbt_at_quarter_note (t->time_pulses/(double)smf.ppqn());
				new_map.add_meter (meter, bbt, 0, MusicTime);
			}

		} else {
			new_map.replace_meter (new_map.meter_section_at_sample (0), meter, bbt, pos, AudioTime);
			new_map.replace_tempo (new_map.tempo_section_at_sample (0), tempo, 0.0, pos, AudioTime);
			have_initial_meter = true;

		}

		last_meter = meter;
	}

	_session->tempo_map() = new_map;
}

void
Editor::do_import (vector<string>          paths,
                   ImportDisposition       disposition,
                   ImportMode              mode,
                   SrcQuality              quality,
                   MidiTrackNameSource     midi_track_name_source,
                   MidiTempoMapDisposition smf_tempo_disposition,
                   samplepos_t&            pos,
                   ARDOUR::PluginInfoPtr   instrument)
{
	boost::shared_ptr<Track> track;
	vector<string> to_import;
	int nth = 0;
	bool use_timestamp = (pos == -1);

	if (smf_tempo_disposition == SMFTempoUse) {
		/* Find the first MIDI file with a tempo map, and import it
		   before we do anything else.
		*/

		for (vector<string>::iterator a = paths.begin(); a != paths.end(); ++a) {
			Evoral::SMF smf;
			if (smf.open (*a)) {
				continue;
			}
			if (smf.num_tempos() > 0) {
				import_smf_tempo_map (smf, pos);
				smf.close ();
				break;
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

	bool ok = true;

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

		if (cancel) {
			ok = false;
		} else {
			ipw.show ();
			ok = (import_sndfiles (paths, disposition, mode, quality, pos, 1, 1, track, false, instrument) == 0);
			import_status.clear();
		}

	} else {

		bool replace = false;

		for (vector<string>::iterator a = paths.begin(); a != paths.end(); ++a) {

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
				pos = -1;
			}

			ipw.show ();

			switch (disposition) {
			case Editing::ImportDistinctFiles:

				to_import.clear ();
				to_import.push_back (*a);

				if (mode == Editing::ImportToTrack) {
					track = get_nth_selected_audio_track (nth++);
				}

				ok = (import_sndfiles (to_import, disposition, mode, quality, pos, 1, -1, track, replace, instrument) == 0);
				import_status.clear();
				break;

			case Editing::ImportDistinctChannels:

				to_import.clear ();
				to_import.push_back (*a);

				ok = (import_sndfiles (to_import, disposition, mode, quality, pos, -1, -1, track, replace, instrument) == 0);
				import_status.clear();
				break;

			case Editing::ImportSerializeFiles:

				to_import.clear ();
				to_import.push_back (*a);

				ok = (import_sndfiles (to_import, disposition, mode, quality, pos, 1, 1, track, replace, instrument) == 0);
				import_status.clear();
				break;

			case Editing::ImportMergeFiles:
				// Not entered, handled in earlier if() branch
				break;
			}
		}
	}

	if (ok) {
		_session->save_state ("");
	}

	import_status.all_done = true;
}

void
Editor::do_embed (vector<string> paths, ImportDisposition import_as, ImportMode mode, samplepos_t& pos, ARDOUR::PluginInfoPtr instrument)
{
	boost::shared_ptr<Track> track;
	bool check_sample_rate = true;
	bool ok = false;
	vector<string> to_embed;
	bool multi = paths.size() > 1;
	int nth = 0;
	bool use_timestamp = (pos == -1);

	switch (import_as) {
	case Editing::ImportDistinctFiles:
		for (vector<string>::iterator a = paths.begin(); a != paths.end(); ++a) {

			/* have to reset this for every file we handle */
			if (use_timestamp) {
				pos = -1;
			}

			to_embed.clear ();
			to_embed.push_back (*a);

			if (mode == Editing::ImportToTrack) {
				track = get_nth_selected_audio_track (nth++);
			}

			if (embed_sndfiles (to_embed, multi, check_sample_rate, import_as, mode, pos, 1, -1, track, instrument) < -1) {
				goto out;
			}
		}
		break;

	case Editing::ImportDistinctChannels:
		for (vector<string>::iterator a = paths.begin(); a != paths.end(); ++a) {

			/* have to reset this for every file we handle */
			if (use_timestamp) {
				pos = -1;
			}

			to_embed.clear ();
			to_embed.push_back (*a);

			if (embed_sndfiles (to_embed, multi, check_sample_rate, import_as, mode, pos, -1, -1, track, instrument) < -1) {
				goto out;
			}
		}
		break;

	case Editing::ImportMergeFiles:
		if (embed_sndfiles (paths, multi, check_sample_rate, import_as, mode, pos, 1, 1, track, instrument) < -1) {
			goto out;
		}
		break;

	case Editing::ImportSerializeFiles:
		for (vector<string>::iterator a = paths.begin(); a != paths.end(); ++a) {

			/* have to reset this for every file we handle */
			if (use_timestamp) {
				pos = -1;
			}

			to_embed.clear ();
			to_embed.push_back (*a);

			if (embed_sndfiles (to_embed, multi, check_sample_rate, import_as, mode, pos, 1, 1, track, instrument) < -1) {
				goto out;
			}
		}
		break;
	}

	ok = true;

  out:
	if (ok) {
		_session->save_state ("");
	}
}

int
Editor::import_sndfiles (vector<string>            paths,
                         ImportDisposition         disposition,
                         ImportMode                mode,
                         SrcQuality                quality,
                         samplepos_t&              pos,
                         int                       target_regions,
                         int                       target_tracks,
                         boost::shared_ptr<Track>& track,
                         bool                      replace,
                         ARDOUR::PluginInfoPtr     instrument)
{
	import_status.paths = paths;
	import_status.done = false;
	import_status.cancel = false;
	import_status.freeze = false;
	import_status.quality = quality;
	import_status.replace_existing_source = replace;
	import_status.split_midi_channels = (disposition == Editing::ImportDistinctChannels);

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
			track, false, instrument
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
                        samplepos_t&              pos,
                        int                       target_regions,
                        int                       target_tracks,
                        boost::shared_ptr<Track>& track,
                        ARDOUR::PluginInfoPtr     instrument)
{
	boost::shared_ptr<AudioFileSource> source;
	SourceList sources;
	string linked_path;
	SoundFileInfo finfo;

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
					_("Sample rate"),
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
					_("Sample rate"),
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

				boost::shared_ptr<Source> s;

				if ((s = _session->audio_source_by_path_and_channel (path, n)) == 0) {

					source = boost::dynamic_pointer_cast<AudioFileSource> (
						SourceFactory::createExternal (DataType::AUDIO, *_session,
									       path, n,
						                               Source::Flag (0),
									true, true));
				} else {
					source = boost::dynamic_pointer_cast<AudioFileSource> (s);
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
		return add_sources (paths, sources, pos, disposition, mode, target_regions, target_tracks, track, true, instrument);
	}

	return 0;
}

int
Editor::add_sources (vector<string>            paths,
                     SourceList&               sources,
                     samplepos_t&              pos,
                     ImportDisposition         disposition,
                     ImportMode                mode,
                     int                       target_regions,
                     int                       target_tracks,
                     boost::shared_ptr<Track>& track,
                     bool                      /*add_channel_suffix*/,
                     ARDOUR::PluginInfoPtr     instrument)
{
	vector<boost::shared_ptr<Region> > regions;
	string region_name;
	uint32_t input_chan = 0;
	uint32_t output_chan = 0;
	bool use_timestamp;
	vector<string> track_names;

	use_timestamp = (pos == -1);

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

		plist.add (ARDOUR::Properties::start, 0);
		plist.add (ARDOUR::Properties::length, sources[0]->length (pos));
		plist.add (ARDOUR::Properties::name, region_name);
		plist.add (ARDOUR::Properties::layer, 0);
		plist.add (ARDOUR::Properties::whole_file, true);
		plist.add (ARDOUR::Properties::external, true);

		boost::shared_ptr<Region> r = RegionFactory::create (sources, plist);

		if (boost::dynamic_pointer_cast<AudioRegion>(r)) {
			boost::dynamic_pointer_cast<AudioRegion>(r)->special_set_position(sources[0]->natural_position());
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

			boost::shared_ptr<FileSource> fs = boost::dynamic_pointer_cast<FileSource> (*x);

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
			   for want of a better idea.  It can't be too small, otherwise if this
			   is a MIDI region the conversion from samples -> beats -> samples will
			   round it back down to 0 again.
			*/
			samplecnt_t len = (*x)->length (pos);
			if (len == 0) {
				len = (60.0 / 120.0) * _session->sample_rate ();
			}

			plist.add (ARDOUR::Properties::start, 0);
			plist.add (ARDOUR::Properties::length, len);
			plist.add (ARDOUR::Properties::name, region_name);
			plist.add (ARDOUR::Properties::layer, 0);
			plist.add (ARDOUR::Properties::whole_file, true);
			plist.add (ARDOUR::Properties::external, true);

			boost::shared_ptr<Region> r = RegionFactory::create (just_one, plist);

			if (boost::dynamic_pointer_cast<AudioRegion>(r)) {
				boost::dynamic_pointer_cast<AudioRegion>(r)->special_set_position((*x)->natural_position());
			}

			regions.push_back (r);
		}
	}

	if (target_regions == 1) {
		input_chan = regions.front()->n_channels();
	} else {
		if (target_tracks == 1) {
			input_chan = regions.size();
		} else {
			input_chan = 1;
		}
	}

	if (Config->get_output_auto_connect() & AutoConnectMaster) {
		output_chan = (_session->master_out() ? _session->master_out()->n_inputs().n_audio() : input_chan);
	} else {
		output_chan = input_chan;
	}

	int n = 0;
	samplepos_t rlen = 0;

	begin_reversible_command (Operations::insert_file);

	/* we only use tracks names when importing to new tracks, but we
	 * require that one is defined for every region, just to keep
	 * the API simpler.
	 */
	assert (regions.size() == track_names.size());

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

		finish_bringing_in_material (*r, input_chan, output_chan, pos, mode, track, track_names[n], instrument);

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
Editor::finish_bringing_in_material (boost::shared_ptr<Region> region,
                                     uint32_t                  in_chans,
                                     uint32_t                  out_chans,
                                     samplepos_t&               pos,
                                     ImportMode                mode,
                                     boost::shared_ptr<Track>& existing_track,
                                     const string&             new_track_name,
                                     ARDOUR::PluginInfoPtr     instrument)
{
	boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion>(region);
	boost::shared_ptr<MidiRegion> mr = boost::dynamic_pointer_cast<MidiRegion>(region);

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

		boost::shared_ptr<Playlist> playlist = existing_track->playlist();
		boost::shared_ptr<Region> copy (RegionFactory::create (region, region->properties()));
		playlist->clear_changes ();
		playlist->add_region (copy, pos);
		if (Config->get_edit_mode() == Ripple)
			playlist->ripple (pos, copy->length(), copy);

		_session->add_command (new StatefulDiffCommand (playlist));
		break;
	}

	case ImportAsTrack:
	{
		if (!existing_track) {
			if (ar) {
				list<boost::shared_ptr<AudioTrack> > at (_session->new_audio_track (in_chans, out_chans, 0, 1, string(), PresentationInfo::max_order, Normal));

				if (at.empty()) {
					return -1;
				}
				if (Config->get_strict_io ()) {
					for (list<boost::shared_ptr<AudioTrack> >::iterator i = at.begin(); i != at.end(); ++i) {
						(*i)->set_strict_io (true);
					}
				}

				existing_track = at.front();
			} else if (mr) {
				list<boost::shared_ptr<MidiTrack> > mt (
					_session->new_midi_track (ChanCount (DataType::MIDI, 1),
						                  ChanCount (DataType::MIDI, 1),
					                          Config->get_strict_io () || Profile->get_mixbus (),
					                          instrument, (Plugin::PresetRecord*) 0,
					                          (RouteGroup*) 0,
					                          1,
					                          string(),
					                          PresentationInfo::max_order));

				if (mt.empty()) {
					return -1;
				}

				// TODO set strict_io from preferences
				existing_track = mt.front();
			}

			if (!new_track_name.empty()) {
				existing_track->set_name (new_track_name);
			} else {
				existing_track->set_name (region->name());
			}
		}

		boost::shared_ptr<Playlist> playlist = existing_track->playlist();
		boost::shared_ptr<Region> copy (RegionFactory::create (region, true));
		playlist->clear_changes ();
		playlist->add_region (copy, pos);
		_session->add_command (new StatefulDiffCommand (playlist));
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
	_session->import_files (import_status);
	return 0;
}
