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
#include "pbd/unwind.h"

#include "ardour/audio_track.h"
#include "ardour/audiofilesource.h"
#include "ardour/audioplaylist.h"
#include "ardour/audioregion.h"
#include "ardour/chan_count.h"
#include "ardour/clip_library.h"
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
#include "simple_export_dialog.h"
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
	PBD::Unwinder<bool> uw (_no_not_select_reimported_tracks, true);
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
Editor::quick_export ()
{
	SimpleExportDialog dialog (*this);
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
		measure_master_loudness (l->start().samples(), l->end().samples(), true);
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
	/* this code is largely similar to editor_ops ::bounce_range_selection */
	if (selection->regions.empty ()) {
		return;
	}

	bool multiple_selected = selection->regions.size () > 1;
	bool multiple_per_track = false;

	if (multiple_selected) {
		std::set<boost::shared_ptr<Route>> route_set;
		for (auto const& i: selection->regions) {
			RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(&i->get_time_axis_view());
			auto rv = route_set.insert (rtv->route ());
			if (!rv.second) {
				multiple_per_track = true;
				break;
			}
		}
	}

	/* no need to check for bounceable() because this operation never puts
	 * its results back in the playlist (only in the region list).
	 */

	string   bounce_name;
	bool     copy_to_clip_library = false;
	bool     copy_to_trigger      = false;
	uint32_t trigger_slot       = 0;

	{
		/*prompt the user for a new name*/

		ArdourWidgets::Prompter dialog (true);

		if (multiple_selected) {
			dialog.set_prompt (_("Prefix for Bounced Regions:"));
			dialog.set_initial_text ("");
			dialog.set_allow_empty ();
		} else {
			boost::shared_ptr<Region> region (selection->regions.front()->region ());
			dialog.set_prompt (_("Name for Bounced Region:"));
			dialog.set_initial_text (region->name());
		}

		dialog.set_name ("BounceNameWindow");
		dialog.set_size_request (400, -1);
		dialog.set_position (Gtk::WIN_POS_MOUSE);

		dialog.add_button (_("Bounce"), RESPONSE_ACCEPT);

		Table*  table  = manage (new Table);
		table->set_spacings (4);
		table->set_border_width (8);
		dialog.get_vbox()->pack_start (*table);
		dialog.get_vbox()->set_spacing (4);

		/* copy to a slot on this track ? */
		Gtk::CheckButton *to_slot = NULL;
		if (!with_processing && !multiple_per_track) {
			to_slot = manage (new Gtk::CheckButton (_("Bounce to Trigger Slot:")));
			Gtk::Alignment *slot_align = manage (new Gtk::Alignment (0, .5, 0, 0));
			slot_align->add (*to_slot);

			ArdourWidgets::ArdourDropdown *tslot = manage (new ArdourWidgets::ArdourDropdown ());

			for (int c = 0; c < TriggerBox::default_triggers_per_box; ++c) {
				std::string lbl = cue_marker_name (c);
				tslot->AddMenuElem (Menu_Helpers::MenuElem (lbl, sigc::bind ([] (uint32_t* t, uint32_t v, ArdourWidgets::ArdourDropdown* s, std::string l) {*t = v; s->set_text (l);}, &trigger_slot, c, tslot, lbl)));
			}
			tslot->set_active ("A");

			HBox *tbox = manage (new HBox());
			tbox->pack_start(*slot_align, false, false);
			tbox->pack_start(*tslot, false, false);
			table->attach (*tbox,       0, 2, 0,1, Gtk::FILL, Gtk::SHRINK);
		}

		/* copy to the user's Clip Library ? */
		Gtk::CheckButton *cliplib = manage (new Gtk::CheckButton (_("Bounce to Clip Library")));
		Gtk::Alignment *align = manage (new Gtk::Alignment (0, .5, 0, 0));
		align->add (*cliplib);
		align->show_all ();
		table->attach (*align,      0, 2, 1,2, Gtk::FILL, Gtk::SHRINK);

		/* in all cases, the selected Range will appear in the Source list */
		Label* s_label = manage (new Label (_("Bounced Region will appear in the Source list")));
		table->attach (*s_label,      0, 2, 2,3, Gtk::FILL, Gtk::SHRINK);

		dialog.get_vbox()->show_all ();

		dialog.show ();

		switch (dialog.run ()) {
		case RESPONSE_ACCEPT:
			break;
		default:
			return;
		}
		dialog.get_result(bounce_name);

		if (to_slot && to_slot->get_active()) {
			copy_to_trigger = true;
		}
		if (cliplib->get_active ()) {
			copy_to_clip_library = true;
		}
	}

	/* prevent user from accidentally overwriting a slot that they can't see */
	bool overwriting = false;
	if (copy_to_trigger) {
		for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {

			boost::shared_ptr<Region> region ((*i)->region());
			RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(&(*i)->get_time_axis_view());
			boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (rtv->route());
			if (!track) {
				continue;
			}
			if (track->triggerbox()->trigger(trigger_slot)->region()) {
				overwriting = true;
			}
		}
		if (overwriting) {
			ArdourMessageDialog msg (string_compose(_("Are you sure you want to overwrite the contents in slot %1?"),cue_marker_name(trigger_slot)), false, MESSAGE_QUESTION, BUTTONS_YES_NO, true);
			msg.set_title (_("Overwriting slot"));
			msg.set_secondary_text (_("One of your selected tracks has content in this slot."));
			if (msg.run () != RESPONSE_YES) {
				return;
			}
		}
	}

	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {

		boost::shared_ptr<Region> region ((*i)->region());
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(&(*i)->get_time_axis_view());
		boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (rtv->route());

		InterThreadInfo itt;

		std::string name;
		if (multiple_selected) {
			name = string_compose ("%1%2", bounce_name, region->name ());
		} else {
			name = bounce_name;
		}

		boost::shared_ptr<Region> r;

		if (with_processing) {
			r = track->bounce_range (region->position_sample(), region->position_sample() + region->length_samples(), itt, track->main_outs(), false, name);
		} else {
			r = track->bounce_range (region->position_sample(), region->position_sample() + region->length_samples(), itt, boost::shared_ptr<Processor>(), false, name);
		}

		if (copy_to_clip_library) {
			export_to_clip_library (r);
		}

		if (copy_to_trigger) {
			boost::shared_ptr<Trigger::UIState> state (new Trigger::UIState());
			if (multiple_selected) {
				state->name = string_compose ("%1%2", bounce_name, r->name ());
			} else {
				state->name = bounce_name;
			}
			//ToDo: can/should we get the tempo for this region?
			track->triggerbox ()->enqueue_trigger_state_for_region(r, state);
			track->triggerbox ()->set_from_selection (trigger_slot, r);
			track->presentation_info ().set_trigger_track (true);
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

	if (region->start().is_zero () && region->length() == region->source_length(0)) {
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

		uint32_t chn = 0;
		for (vector<boost::shared_ptr<AudioFileSource> >::iterator src=sources.begin(); src != sources.end(); ++src, ++chn) {

			fs = (*src);

			if (region->read_at (buf, buf, gain_buffer, pos, this_time, chn) != this_time) {
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
