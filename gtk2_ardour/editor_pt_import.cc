/*
    Copyright (C) 2000-2006 Paul Davis

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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <algorithm>

#include "pbd/pthread_utils.h"
#include "pbd/basename.h"
#include "pbd/shortpath.h"
#include "pbd/stateful_diff_command.h"

#include <gtkmm2ext/choice.h>

#include "ardour/audio_track.h"
#include "ardour/audiofilesource.h"
#include "ardour/audioregion.h"
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

#include "ptformat/ptfformat.h"

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

/* Functions supporting the incorporation of PT sessions into ardour */

void
Editor::external_pt_dialog ()
{
	std::string ptpath;

	if (_session == 0) {
		MessageDialog msg (_("You can't import a PT session until you have a session loaded."));
		msg.run ();
		return;
	}

	Gtk::FileChooserDialog dialog(_("Import PT Session"), FILE_CHOOSER_ACTION_OPEN);
	dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);

	while (true) {
		int result = dialog.run();

		if (result == Gtk::RESPONSE_OK) {
			ptpath = dialog.get_filename ();

			if (!Glib::file_test (ptpath, Glib::FILE_TEST_IS_DIR|Glib::FILE_TEST_EXISTS)) {
				Gtk::MessageDialog msg (string_compose (_("%1: this is only the directory/folder name, not the filename.\n"), ptpath));
				msg.run ();
				continue;
			}
		}

		if (ptpath.length()) {
			do_ptimport(ptpath, SrcBest);
			break;
		}

		if (result == Gtk::RESPONSE_CANCEL) {
			break;
		}
	}
}

void
Editor::do_ptimport (std::string ptpath,
                      SrcQuality  quality)
{
	vector<boost::shared_ptr<Region> > regions;
	boost::shared_ptr<ARDOUR::Track> track;
	ARDOUR::PluginInfoPtr instrument;
	vector<string> to_import;
	string fullpath;
	bool ok = false;
	bool onefailed = false;
	PTFFormat ptf;
	framepos_t pos = -1;

	vector<ptflookup_t> ptfwavpair;
	vector<ptflookup_t> ptfregpair;

	if (ptf.load(ptpath, _session->frame_rate()) == -1) {
		MessageDialog msg (_("Doesn't seem to be a valid PT session file"));
		msg.run ();
		return;
	} else {
		MessageDialog msg (string_compose (_("PT v%1 Session @ %2Hz\n\n%3 audio files\n%4 regions\n%5 active regions\n\nContinue..."), (int)ptf.version, ptf.sessionrate, ptf.audiofiles.size(), ptf.regions.size(), ptf.tracks.size()));
		msg.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);

		int result = msg.run ();
		if (result != Gtk::RESPONSE_OK) {
			return;
		}
	}
	current_interthread_info = &import_status;
	import_status.current = 1;
	import_status.total = ptf.audiofiles.size ();
	import_status.all_done = false;

	ImportProgressWindow ipw (&import_status, _("Import"), _("Cancel Import"));

	SourceList just_one;
	SourceList imported;

	for (vector<PTFFormat::wav_t>::iterator a = ptf.audiofiles.begin(); a != ptf.audiofiles.end(); ++a) {
		ptflookup_t p;

		fullpath = Glib::build_filename (Glib::path_get_dirname(ptpath), "Audio Files");
		fullpath = Glib::build_filename (fullpath, a->filename);
		to_import.clear ();
		to_import.push_back (fullpath);
		ipw.show ();
		ok = import_sndfiles (to_import, Editing::ImportDistinctFiles, Editing::ImportAsRegion, quality, pos, 1, -1, track, false, instrument);
		if (!import_status.sources.empty()) {
			p.index1 = a->index;
			p.id = import_status.sources.back()->id();

			ptfwavpair.push_back(p);
			imported.push_back(import_status.sources.back());
		} else {
			onefailed = true;
		}
	}

	if (onefailed) {
		MessageDialog msg (_("Failed to load one or more of the audio files, but continuing to attempt import."));
		msg.run ();
	} else {
		MessageDialog msg (_("Success! Import should complete soon."));
		msg.run ();
	}

	// Create a dummy midi track first to get a midi Source
	list<boost::shared_ptr<MidiTrack> > mt (
		_session->new_midi_track (ChanCount (DataType::MIDI, 1),
		                          ChanCount (DataType::MIDI, 1),
		                          true,
		                          instrument, (Plugin::PresetRecord*) 0,
		                          (RouteGroup*) 0,
		                          1,
		                          string(),
		                          PresentationInfo::max_order));
	if (mt.empty()) {
		return;
	}

	for (vector<PTFFormat::region_t>::iterator a = ptf.regions.begin();
			a != ptf.regions.end(); ++a) {
		for (vector<ptflookup_t>::iterator p = ptfwavpair.begin();
				p != ptfwavpair.end(); ++p) {
			if ((p->index1 == a->wave.index) && (strcmp(a->wave.filename.c_str(), "") != 0)) {
				for (SourceList::iterator x = imported.begin();
						x != imported.end(); ++x) {
					if ((*x)->id() == p->id) {
						// Matched an uncreated ptf region to ardour region
						ptflookup_t rp;
						PropertyList plist;

						plist.add (ARDOUR::Properties::start, a->sampleoffset);
						plist.add (ARDOUR::Properties::position, 0);
						plist.add (ARDOUR::Properties::length, a->length);
						plist.add (ARDOUR::Properties::name, a->name);
						plist.add (ARDOUR::Properties::layer, 0);
						plist.add (ARDOUR::Properties::whole_file, false);
						plist.add (ARDOUR::Properties::external, true);

						just_one.clear();
						just_one.push_back(*x);

						boost::shared_ptr<Region> r = RegionFactory::create (just_one, plist);
						regions.push_back(r);

						rp.id = regions.back()->id();
						rp.index1 = a->index;
						ptfregpair.push_back(rp);
					}
				}
			}
		}
		if (strcmp(a->wave.filename.c_str(), "") == 0) {
			/* Empty wave - assume MIDI region */
			boost::shared_ptr<MidiTrack> midi_track = mt.back();
			boost::shared_ptr<Playlist> playlist = midi_track->playlist();
			framepos_t f = (framepos_t)a->startpos;
			framecnt_t length = (framecnt_t)a->length;
			MusicFrame pos (f, 0);
			boost::shared_ptr<Source> src = _session->create_midi_source_by_stealing_name (midi_track);
			PropertyList plist;
			plist.add (ARDOUR::Properties::start, 0);
			plist.add (ARDOUR::Properties::length, length);
			plist.add (ARDOUR::Properties::name, PBD::basename_nosuffix(src->name()));
			boost::shared_ptr<Region> region = (RegionFactory::create (src, plist));
			/* sets beat position */
			region->set_position (pos.frame, pos.division);
			midi_track->playlist()->add_region (region, pos.frame, 1.0, false, pos.division);

			boost::shared_ptr<MidiRegion> mr = boost::dynamic_pointer_cast<MidiRegion>(region);
			boost::shared_ptr<MidiModel> mm = mr->midi_source(0)->model();
			MidiModel::NoteDiffCommand *midicmd;
			midicmd = mm->new_note_diff_command ("Import ProTools MIDI");

			for (vector<PTFFormat::midi_ev_t>::iterator
					j = a->midi.begin();
					j != a->midi.end(); ++j) {
				Evoral::Beats start = (Evoral::Beats)(j->pos/960000.);
				Evoral::Beats len = (Evoral::Beats)(j->length/960000.);
				// PT C-2 = 0, Ardour C-1 = 0, subtract twelve to convert...
				midicmd->add(boost::shared_ptr<Evoral::Note<Evoral::Beats> >
					(new Evoral::Note<Evoral::Beats>( (uint8_t)1, start, len, j->note - 12, j->velocity )));
			}
			mm->apply_command (_session, midicmd);
			boost::shared_ptr<Region> copy (RegionFactory::create (mr, true));
			playlist->clear_changes ();
			playlist->add_region (copy, a->startpos);
		}
	}

	boost::shared_ptr<AudioTrack> existing_track;
	uint16_t nth = 0;
	vector<ptflookup_t> usedtracks;
	ptflookup_t utr;

	for (vector<PTFFormat::track_t>::iterator a = ptf.tracks.begin();
			a != ptf.tracks.end(); ++a) {
		for (vector<ptflookup_t>::iterator p = ptfregpair.begin();
				p != ptfregpair.end(); ++p) {

			if (p->index1 == a->reg.index)  {
				// Matched a ptf active region to an ardour region
				utr.index1 = a->index;
				utr.index2 = nth;
				utr.id = p->id;
				boost::shared_ptr<Region> r = RegionFactory::region_by_id (p->id);
				vector<ptflookup_t>::iterator lookuptr = usedtracks.begin();
				vector<ptflookup_t>::iterator found;
				if ((found = std::find(lookuptr, usedtracks.end(), utr)) != usedtracks.end()) {
					DEBUG_TRACE (DEBUG::FileUtils, string_compose ("\twav(%1) reg(%2) ptf_tr(%3) ard_tr(%4)\n", a->reg.wave.filename.c_str(), a->reg.index, found->index1, found->index2));
					existing_track =  get_nth_selected_audio_track(found->index2);
					// Put on existing track
					boost::shared_ptr<Playlist> playlist = existing_track->playlist();
					boost::shared_ptr<Region> copy (RegionFactory::create (r, true));
					playlist->clear_changes ();
					playlist->add_region (copy, a->reg.startpos);
					//_session->add_command (new StatefulDiffCommand (playlist));
				} else {
					// Put on a new track
					DEBUG_TRACE (DEBUG::FileUtils, string_compose ("\twav(%1) reg(%2) new_tr(%3)\n", a->reg.wave.filename.c_str(), a->reg.index, nth));
					list<boost::shared_ptr<AudioTrack> > at (_session->new_audio_track (1, 2, 0, 1, string(), PresentationInfo::max_order, Normal));
					if (at.empty()) {
						return;
					}
					existing_track = at.back();
					std::string trackname;
					try {
						trackname = Glib::convert_with_fallback (a->name, "UTF-8", "UTF-8", "_");
					} catch (Glib::ConvertError& err) {
						trackname = string_compose ("Invalid %1", a->index);
					}
					// TODO legalize track name (no slashes, no colons)
#if 0 // TODO --  "find_route_name" is currently private
					/* generate a unique name by adding a number if needed */
					uint32_t id = 0;
					if (!_session->find_route_name (trackname.c_str (), id, trackname, false)) {
						fatal << _("PTImport: UINT_MAX routes? impossible!") << endmsg;
						abort(); /*NOTREACHED*/
					}
#endif
					existing_track->set_name (trackname);
					boost::shared_ptr<Playlist> playlist = existing_track->playlist();
					boost::shared_ptr<Region> copy (RegionFactory::create (r, true));
					playlist->clear_changes ();
					playlist->add_region (copy, a->reg.startpos);
					//_session->add_command (new StatefulDiffCommand (playlist));
					nth++;
				}
				usedtracks.push_back(utr);
			}
		}
	}

	import_status.sources.clear();

	if (ok) {
		_session->save_state ("");
	}
	import_status.all_done = true;
}
