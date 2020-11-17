/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2017 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2008-2012 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 André Nusser <andre.nusser@googlemail.com>
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

/* Note: public Editor methods are documented in public_editor.h */

#include <inttypes.h>
#include <unistd.h>
#include <climits>

#include <gtkmm/messagedialog.h>

#include "pbd/gstdio_compat.h"

#include "pbd/pthread_utils.h"

#include "ardour/audio_track.h"
#include "ardour/audiofilesource.h"
#include "ardour/audioplaylist.h"
#include "ardour/audioregion.h"
#include "ardour/chan_count.h"
#include "ardour/midi_region.h"
#include "ardour/session.h"
#include "ardour/session_directory.h"
#include "ardour/source_factory.h"
#include "ardour/types.h"

#include "ardour_ui.h"
#include "ardour_message.h"

#include "widgets/prompter.h"

#include "audio_region_view.h"
#include "audio_time_axis.h"
#include "editor.h"
#include "export_dialog.h"
#include "loudness_dialog.h"
#include "midi_export_dialog.h"
#include "midi_region_view.h"
#include "public_editor.h"
#include "selection.h"
#include "time_axis_view.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;

void
Editor::export_audio ()
{
	ExportDialog dialog (*this, _("Export"), ExportProfileManager::RegularExport);
	dialog.set_session (_session);
	dialog.run();
}

void
Editor::stem_export ()
{
	StemExportDialog dialog (*this);
	dialog.set_session (_session);
	dialog.run();
}

void
Editor::export_selection ()
{
	ExportSelectionDialog dialog (*this);
	dialog.set_session (_session);
	dialog.run();
}

void
Editor::loudness_assistant_marker ()
{
	ArdourMarker* marker;

	if ((marker = reinterpret_cast<ArdourMarker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	Location* l;
	bool is_start;

	if (((l = find_location_from_marker (marker, is_start)) != 0) && (l->end() > l->start())) {
		measure_master_loudness (l->start(), l->end(), true);
	}
}

void
Editor::loudness_assistant (bool range_selection)
{
	samplepos_t start, end;
	TimeSelection const& ts (get_selection().time);
	if (range_selection && !ts.empty ()) {
		start = ts.start_sample();
		end = ts.end_sample();
	} else {
		start = _session->current_start_sample();
		end   = _session->current_end_sample();
	}
	measure_master_loudness (start, end, range_selection);
}

void
Editor::measure_master_loudness (samplepos_t start, samplepos_t end, bool is_range_selection)
{
	if (!Config->get_use_master_volume ()) {
		ArdourMessageDialog md (_("Master bus output gain control is disabled.\nVisit preferences to enable it?"), false,
				MESSAGE_QUESTION, BUTTONS_YES_NO);
		if (md.run () == RESPONSE_YES) {
			ARDOUR_UI::instance()->show_mixer_prefs ();
		}
		return;
	}

	if (start >= end) {
		if (is_range_selection) {
			ArdourMessageDialog (_("Loudness Analysis requires a session-range or range-selection."), false, MESSAGE_ERROR).run ();
		} else {
			ArdourMessageDialog (_("Loudness Analysis requires a session-range."), false, MESSAGE_ERROR).run ();
		}
		return;
	}

	if (!_session->master_volume()) {
		ArdourMessageDialog (_("Loudness Analysis is only available for sessions with a master-bus"), false, MESSAGE_ERROR).run ();
		return;
	}
	assert (_session->master_out());
	if (_session->master_out()->output()->n_ports().n_audio() != 2) {
		ArdourMessageDialog (_("Loudness Analysis is only available for sessions with a stereo master-bus"), false, MESSAGE_ERROR).run ();
		return;
	}

	ARDOUR::TimelineRange ar (timepos_t (start), timepos_t (end), 0);

	LoudnessDialog ld (_session, ar, is_range_selection);

	if (own_window ()) {
		ld.set_transient_for (*own_window ());
	}

	ld.run ();
}

void
Editor::export_range ()
{
	ArdourMarker* marker;

	if ((marker = reinterpret_cast<ArdourMarker *> (marker_menu_item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		abort(); /*NOTREACHED*/
	}

	Location* l;
	bool is_start;

	if (((l = find_location_from_marker (marker, is_start)) != 0) && (l->end() > l->start())) {
		ExportRangeDialog dialog (*this, l->id().to_s());
		dialog.set_session (_session);
		dialog.run();
	}
}

bool
Editor::process_midi_export_dialog (MidiExportDialog& dialog, boost::shared_ptr<MidiRegion> midi_region)
{
	string path = dialog.get_path ();

	if (Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
		bool overwrite = ARDOUR_UI_UTILS::overwrite_file_dialog (dialog,
									 _("Confirm MIDI File Overwrite"),
									 _("A file with the same name already exists. Do you want to overwrite it?"));

		if (!overwrite) {
			return false;
		}

		/* force ::g_unlink because the backend code will
		   go wrong if it tries to open an existing
		   file for writing.
		*/
		::g_unlink (path.c_str());
	}

	return midi_region->do_export (path);
}

/** Export the first selected region */
void
Editor::export_region ()
{
	if (selection->regions.empty()) {
		return;
	}

	boost::shared_ptr<Region> r = selection->regions.front()->region();
	boost::shared_ptr<AudioRegion> audio_region = boost::dynamic_pointer_cast<AudioRegion>(r);
	boost::shared_ptr<MidiRegion> midi_region = boost::dynamic_pointer_cast<MidiRegion>(r);

	if (audio_region) {

		RouteTimeAxisView & rtv (dynamic_cast<RouteTimeAxisView &> (selection->regions.front()->get_time_axis_view()));
		AudioTrack & track (dynamic_cast<AudioTrack &> (*rtv.route()));

		ExportRegionDialog dialog (*this, *(audio_region.get()), track);
		dialog.set_session (_session);
		dialog.run ();

	} else if (midi_region) {

		MidiExportDialog dialog (*this, midi_region);
		dialog.set_session (_session);

		bool finished = false;
		while (!finished) {
			switch (dialog.run ()) {
			case Gtk::RESPONSE_ACCEPT:
				finished = process_midi_export_dialog (dialog, midi_region);
				break;
			default:
				return;
			}
		}
	}
}

int
Editor::write_region_selection (RegionSelection& regions)
{
	for (RegionSelection::iterator i = regions.begin(); i != regions.end(); ++i) {
		AudioRegionView* arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv) {
			if (write_region ("", arv->audio_region()) == false)
				return -1;
		}

		MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(*i);
		if (mrv) {
			warning << "MIDI region export not implemented" << endmsg;
		}
	}

	return 0;
}

void
Editor::bounce_region_selection (bool with_processing)
{
	/* no need to check for bounceable() because this operation never puts
	 * its results back in the playlist (only in the region list).
	 */

	/*prompt the user for a new name*/
	string bounce_name;
	{
		ArdourWidgets::Prompter dialog (true);

		dialog.set_prompt (_("Name for Bounced Region:"));

		dialog.set_name ("BounceNameWindow");
		dialog.set_size_request (400, -1);
		dialog.set_position (Gtk::WIN_POS_MOUSE);

		dialog.add_button (_("Rename"), RESPONSE_ACCEPT);
		dialog.set_initial_text (bounce_name);

		Label label;
		label.set_text (_("Bounced Region will appear in the Source list."));
		dialog.get_vbox()->set_spacing (8);
		dialog.get_vbox()->pack_start (label);
		label.show();

		dialog.show ();

		switch (dialog.run ()) {
		case RESPONSE_ACCEPT:
			break;
		default:
			return;
		}
		dialog.get_result(bounce_name);
	}

	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {

		boost::shared_ptr<Region> region ((*i)->region());
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(&(*i)->get_time_axis_view());
		boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (rtv->route());

		InterThreadInfo itt;

		boost::shared_ptr<Region> r;

		if (with_processing) {
			r = track->bounce_range (region->position_sample(), region->position_sample() + region->length_samples(), itt, track->main_outs(), false, bounce_name);
		} else {
			r = track->bounce_range (region->position_sample(), region->position_sample() + region->length_samples(), itt, boost::shared_ptr<Processor>(), false, bounce_name);
		}
	}
}

bool
Editor::write_region (string path, boost::shared_ptr<AudioRegion> region)
{
	boost::shared_ptr<AudioFileSource> fs;
	const samplepos_t chunk_size = 4096;
	samplepos_t to_read;
	Sample buf[chunk_size];
	gain_t gain_buffer[chunk_size];
	samplepos_t pos;
	char s[PATH_MAX+1];
	uint32_t cnt;
	vector<boost::shared_ptr<AudioFileSource> > sources;
	uint32_t nchans;

	const string sound_directory = _session->session_directory().sound_path();

	nchans = region->n_channels();

	/* don't do duplicate of the entire source if that's what is going on here */

	if (region->nt_start().zero() && region->nt_length() == region->source_length(0)) {
		/* XXX should link(2) to create a new inode with "path" */
		return true;
	}

	if (path.length() == 0) {

		for (uint32_t n=0; n < nchans; ++n) {

			for (cnt = 0; cnt < 999999; ++cnt) {
				if (nchans == 1) {
					snprintf (s, sizeof(s), "%s/%s_%" PRIu32 ".wav", sound_directory.c_str(),
						  legalize_for_universal_path(region->name()).c_str(), cnt);
				}
				else {
					snprintf (s, sizeof(s), "%s/%s_%" PRIu32 "-%" PRId32 ".wav", sound_directory.c_str(),
						  legalize_for_universal_path(region->name()).c_str(), cnt, n);
				}

				path = s;

				if (!Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
					break;
				}
			}

			if (cnt == 999999) {
				error << "" << endmsg;
				goto error_out;
			}



			try {
				fs = boost::dynamic_pointer_cast<AudioFileSource> (SourceFactory::createWritable (DataType::AUDIO, *_session, path, _session->sample_rate()));
			}

			catch (failed_constructor& err) {
				goto error_out;
			}

			sources.push_back (fs);
		}
	}
	else {
		/* TODO: make filesources based on passed path */

	}

	to_read = region->length_samples();
	pos = region->position_sample();

	while (to_read) {
		samplepos_t this_time;

		this_time = min (to_read, chunk_size);

		for (vector<boost::shared_ptr<AudioFileSource> >::iterator src=sources.begin(); src != sources.end(); ++src) {

			fs = (*src);

			if (region->read_at (buf, buf, gain_buffer, pos, this_time) != this_time) {
				break;
			}

			if (fs->write (buf, this_time) != this_time) {
				error << "" << endmsg;
				goto error_out;
			}
		}

		to_read -= this_time;
		pos += this_time;
	}

	time_t tnow;
	struct tm* now;
	time (&tnow);
	now = localtime (&tnow);

	for (vector<boost::shared_ptr<AudioFileSource> >::iterator src = sources.begin(); src != sources.end(); ++src) {
		(*src)->update_header (0, *now, tnow);
		(*src)->mark_immutable ();
	}

	return true;

error_out:

	for (vector<boost::shared_ptr<AudioFileSource> >::iterator i = sources.begin(); i != sources.end(); ++i) {
		(*i)->mark_for_remove ();
	}

	return 0;
}

int
Editor::write_audio_selection (TimeSelection& ts)
{
	int ret = 0;

	if (selection->tracks.empty()) {
		return 0;
	}

	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {

		AudioTimeAxisView* atv;

		if ((atv = dynamic_cast<AudioTimeAxisView*>(*i)) == 0) {
			continue;
		}

		if (atv->is_audio_track()) {

			boost::shared_ptr<AudioPlaylist> playlist = boost::dynamic_pointer_cast<AudioPlaylist>(atv->track()->playlist());

			if (playlist && write_audio_range (*playlist, atv->track()->n_channels(), ts) == 0) {
				ret = -1;
				break;
			}
		}
	}

	return ret;
}

bool
Editor::write_audio_range (AudioPlaylist& playlist, const ChanCount& count, list<TimelineRange>& range)
{
	boost::shared_ptr<AudioFileSource> fs;
	const samplepos_t chunk_size = 4096;
	samplecnt_t nframes;
	Sample buf[chunk_size];
	gain_t gain_buffer[chunk_size];
	samplepos_t pos;
	char s[PATH_MAX+1];
	uint32_t cnt;
	string path;
	vector<boost::shared_ptr<AudioFileSource> > sources;

	const string sound_directory = _session->session_directory().sound_path();

	uint32_t channels = count.n_audio();

	for (uint32_t n=0; n < channels; ++n) {

		for (cnt = 0; cnt < 999999; ++cnt) {
			if (channels == 1) {
				snprintf (s, sizeof(s), "%s/%s_%" PRIu32 ".wav", sound_directory.c_str(),
					  legalize_for_universal_path(playlist.name()).c_str(), cnt);
			}
			else {
				snprintf (s, sizeof(s), "%s/%s_%" PRIu32 "-%" PRId32 ".wav", sound_directory.c_str(),
					  legalize_for_universal_path(playlist.name()).c_str(), cnt, n);
			}

			if (!Glib::file_test (s, Glib::FILE_TEST_EXISTS)) {
				break;
			}
		}

		if (cnt == 999999) {
			error << "" << endmsg;
			goto error_out;
		}

		path = s;

		try {
			fs = boost::dynamic_pointer_cast<AudioFileSource> (SourceFactory::createWritable (DataType::AUDIO, *_session, path, _session->sample_rate()));
		}

		catch (failed_constructor& err) {
			goto error_out;
		}

		sources.push_back (fs);

	}


	for (list<TimelineRange>::iterator i = range.begin(); i != range.end();) {

		nframes = (*i).length().samples();
		pos = (*i).start().samples();

		while (nframes) {

			timecnt_t this_time = timecnt_t (min (nframes, chunk_size));

			for (uint32_t n=0; n < channels; ++n) {

				fs = sources[n];

				if (playlist.read (buf, buf, gain_buffer, timepos_t (pos), this_time, n) != this_time) {
					break;
				}

				if (fs->write (buf, this_time.samples()) != this_time.samples()) {
					goto error_out;
				}
			}

			nframes -= this_time.samples();
			pos += this_time.samples();
		}

		list<TimelineRange>::iterator tmp = i;
		++tmp;

		if (tmp != range.end()) {

			/* fill gaps with silence */

			nframes = (*i).end().distance ((*tmp).start()).samples();

			while (nframes) {

				timecnt_t this_time = timecnt_t (min (nframes, chunk_size));
				memset (buf, 0, sizeof (Sample) * this_time.samples());

				for (uint32_t n=0; n < channels; ++n) {

					fs = sources[n];
					if (fs->write (buf, this_time.samples()) != this_time.samples()) {
						goto error_out;
					}
				}

				nframes -= this_time.samples();
			}
		}

		i = tmp;
	}

	time_t tnow;
	struct tm* now;
	time (&tnow);
	now = localtime (&tnow);

	for (vector<boost::shared_ptr<AudioFileSource> >::iterator s = sources.begin(); s != sources.end(); ++s) {
		(*s)->update_header (0, *now, tnow);
		(*s)->mark_immutable ();
		// do we need to ref it again?
	}

	return true;

error_out:
	/* unref created files */

	for (vector<boost::shared_ptr<AudioFileSource> >::iterator i = sources.begin(); i != sources.end(); ++i) {
		(*i)->mark_for_remove ();
	}

	return false;
}

void
Editor::write_selection ()
{
	if (!selection->time.empty()) {
		write_audio_selection (selection->time);
	} else if (!selection->regions.empty()) {
		write_region_selection (selection->regions);
	}
}
