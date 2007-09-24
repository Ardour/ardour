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

#include <sndfile.h>

#include <pbd/pthread_utils.h>
#include <pbd/basename.h>
#include <pbd/shortpath.h>

#include <gtkmm2ext/choice.h>
#include <gtkmm2ext/window_title.h>

#include <ardour/session.h>
#include <ardour/audioplaylist.h>
#include <ardour/audioregion.h>
#include <ardour/audio_diskstream.h>
#include <ardour/utils.h>
#include <ardour/audio_track.h>
#include <ardour/audioplaylist.h>
#include <ardour/audiofilesource.h>
#include <ardour/region_factory.h>
#include <ardour/source_factory.h>
#include <pbd/memento_command.h>

#include "ardour_ui.h"
#include "editor.h"
#include "sfdb_ui.h"
#include "editing.h"
#include "audio_time_axis.h"
#include "utils.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace sigc;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Editing;
using Glib::ustring;

/* Functions supporting the incorporation of external (non-captured) audio material into ardour */

void
Editor::add_external_audio_action (ImportMode mode)
{
}

void
Editor::external_audio_dialog ()
{
	vector<Glib::ustring> paths;

	if (session == 0) {
		MessageDialog msg (0, _("You can't import or embed an audiofile until you have a session loaded."));
		msg.run ();
		return;
	}
	
	if (sfbrowser == 0) {
		sfbrowser = new SoundFileOmega (*this, _("Add existing audio"), session, selection->tracks.size());
	} else {
		sfbrowser->reset (selection->tracks.size());
	}

	sfbrowser->show_all ();

  again:
	int response = sfbrowser->run ();

	switch (response) {
	case RESPONSE_APPLY:
		// leave the dialog open
		break;

	case RESPONSE_OK:
		sfbrowser->hide ();
		break;

	default:
		// cancel from the browser - we are done
		sfbrowser->hide ();
		return;
	}

	/* lets do it */
	
	paths = sfbrowser->get_paths ();

	ImportPosition pos = sfbrowser->get_position ();
	ImportMode mode = sfbrowser->get_mode ();
	ImportDisposition chns = sfbrowser->get_channel_disposition ();
	nframes64_t where;

	switch (pos) {
	case ImportAtEditCursor:
		where = edit_cursor->current_frame;
		break;
	case ImportAtTimestamp:
		where = -1;
		break;
	case ImportAtPlayhead:
		where = playhead_cursor->current_frame;
		break;
	case ImportAtStart:
		where = session->current_start_frame();
		break;
	}

	SrcQuality quality = sfbrowser->get_src_quality();

	if (sfbrowser->copy_files_btn.get_active()) {
		do_import (paths, chns, mode, quality, where);
	} else {
		do_embed (paths, chns, mode, where);
	}

	if (response == RESPONSE_APPLY) {
		sfbrowser->clear_selection ();
		goto again;
	}
}

boost::shared_ptr<AudioTrack>
Editor::get_nth_selected_audio_track (int nth) const
{
	AudioTimeAxisView* atv;
	TrackSelection::iterator x;
	
	for (x = selection->tracks.begin(); nth > 0 && x != selection->tracks.end(); ++x) {
		if (dynamic_cast<AudioTimeAxisView*>(*x)) {
			--nth;
		}
	}
	
	if (x == selection->tracks.end()) {
		atv = dynamic_cast<AudioTimeAxisView*>(selection->tracks.back());
	} else {
		atv = dynamic_cast<AudioTimeAxisView*>(*x);
	}
	
	if (!atv) {
		return boost::shared_ptr<AudioTrack>();
	}
	
	return atv->audio_track();
}	

void
Editor::do_import (vector<ustring> paths, ImportDisposition chns, ImportMode mode, SrcQuality quality, nframes64_t& pos)
{
	boost::shared_ptr<AudioTrack> track;
	vector<ustring> to_import;
	bool ok = false;
	int nth = 0;

	if (interthread_progress_window == 0) {
		build_interthread_progress_window ();
	}

	switch (chns) {
	case Editing::ImportDistinctFiles:
		for (vector<ustring>::iterator a = paths.begin(); a != paths.end(); ++a) {

			to_import.clear ();
			to_import.push_back (*a);

			if (mode == Editing::ImportToTrack) {
				track = get_nth_selected_audio_track (nth++);
			}
			
			if (import_sndfiles (to_import, mode, quality, pos, 1, -1, track)) {
				goto out;
			}

		}
		break;

	case Editing::ImportDistinctChannels:
		for (vector<ustring>::iterator a = paths.begin(); a != paths.end(); ++a) {

			to_import.clear ();
			to_import.push_back (*a);

			if (import_sndfiles (to_import, mode, quality, pos, -1, -1, track)) {
				goto out;
			}

		}
		break;

	case Editing::ImportMergeFiles:
		/* create 1 region from all paths, add to 1 track,
		   ignore "track"
		*/
		if (import_sndfiles (paths, mode, quality, pos, 1, 1, track)) {
			goto out;
		}
		break;

	case Editing::ImportSerializeFiles:
		for (vector<ustring>::iterator a = paths.begin(); a != paths.end(); ++a) {

			to_import.clear ();
			to_import.push_back (*a);
			
			/* create 1 region from this path, add to 1 track,
			   reuse "track" across paths
			*/

			if (import_sndfiles (to_import, mode, quality, pos, 1, 1, track)) {
				goto out;
			}

		}
		break;
	}

	ok = true;
	
  out:	
	if (ok) {
		session->save_state ("");
	}

	interthread_progress_window->hide_all ();
}

bool
Editor::idle_do_embed (vector<ustring> paths, ImportDisposition chns, ImportMode mode, nframes64_t& pos)
{
	_do_embed (paths, chns, mode, pos);
	return false;
}

void
Editor::do_embed (vector<ustring> paths, ImportDisposition chns, ImportMode mode, nframes64_t& pos)
{
#ifdef GTKOSX
	Glib::signal_idle().connect (bind (mem_fun (*this, &Editor::idle_do_embed), paths, chns, mode, pos));
#else
	_do_embed (paths, chns, mode, pos);
#endif
}

void
Editor::_do_embed (vector<ustring> paths, ImportDisposition chns, ImportMode mode, nframes64_t& pos)
{
	boost::shared_ptr<AudioTrack> track;
	bool check_sample_rate = true;
	bool ok = false;
	vector<ustring> to_embed;
	bool multi = paths.size() > 1;
	int nth = 0;

	switch (chns) {
	case Editing::ImportDistinctFiles:
		for (vector<ustring>::iterator a = paths.begin(); a != paths.end(); ++a) {

			to_embed.clear ();
			to_embed.push_back (*a);

			if (mode == Editing::ImportToTrack) {
				track = get_nth_selected_audio_track (nth++);
			}

			if (embed_sndfiles (to_embed, multi, check_sample_rate, mode, pos, 1, -1, track) < -1) {
				goto out;
			}
		}
		break;
		
	case Editing::ImportDistinctChannels:
		for (vector<ustring>::iterator a = paths.begin(); a != paths.end(); ++a) {

			to_embed.clear ();
			to_embed.push_back (*a);

			if (embed_sndfiles (to_embed, multi, check_sample_rate, mode, pos, -1, -1, track) < -1) {
				goto out;
			}
		}
		break;

	case Editing::ImportMergeFiles:
		if (embed_sndfiles (paths, multi, check_sample_rate, mode, pos, 1, 1, track) < -1) {
			goto out;
		}
	break;

	case Editing::ImportSerializeFiles:
		for (vector<ustring>::iterator a = paths.begin(); a != paths.end(); ++a) {

			to_embed.clear ();
			to_embed.push_back (*a);

			if (embed_sndfiles (to_embed, multi, check_sample_rate, mode, pos, 1, 1, track) < -1) {
				goto out;
			}
		}
		break;
	}

	ok = true;
	
  out:	
	if (ok) {
		session->save_state ("");
	}
}

int
Editor::import_sndfiles (vector<ustring> paths, ImportMode mode, SrcQuality quality, nframes64_t& pos, 
			 int target_regions, int target_tracks, boost::shared_ptr<AudioTrack>& track)
{
	WindowTitle title = string_compose (_("importing %1"), paths.front());

	interthread_progress_window->set_title (title.get_string());
	interthread_progress_window->set_position (Gtk::WIN_POS_MOUSE);
	interthread_progress_window->show_all ();
	interthread_progress_bar.set_fraction (0.0f);
	interthread_cancel_label.set_text (_("Cancel Import"));
	current_interthread_info = &import_status;

	import_status.paths = paths;
	import_status.done = false;
	import_status.cancel = false;
	import_status.freeze = false;
	import_status.done = 0.0;
	import_status.quality = quality;

	interthread_progress_connection = Glib::signal_timeout().connect 
		(bind (mem_fun(*this, &Editor::import_progress_timeout), (gpointer) 0), 100);
	
	track_canvas.get_window()->set_cursor (Gdk::Cursor (Gdk::WATCH));
	ARDOUR_UI::instance()->flush_pending ();

	/* start import thread for this spec. this will ultimately call Session::import_audiofile()
	   and if successful will add the file(s) as a region to the session region list.
	*/
	
	pthread_create_and_store ("import", &import_status.thread, 0, _import_thread, this);
	pthread_detach (import_status.thread);
	
	while (!(import_status.done || import_status.cancel)) {
		gtk_main_iteration ();
	}

	interthread_progress_window->hide ();
	
	import_status.done = true;
	interthread_progress_connection.disconnect ();
	
	/* import thread finished - see if we should build a new track */

	boost::shared_ptr<AudioRegion> r;
	
	if (import_status.cancel || import_status.sources.empty()) {
		goto out;
	}

	if (add_sources (paths, import_status.sources, pos, mode, target_regions, target_tracks, track, false) == 0) {
		session->save_state ("");
	}

  out:
	track_canvas.get_window()->set_cursor (*current_canvas_cursor);
	return 0;
}

int
Editor::embed_sndfiles (vector<Glib::ustring> paths, bool multifile,
			bool& check_sample_rate, ImportMode mode, nframes64_t& pos, int target_regions, int target_tracks,
			boost::shared_ptr<AudioTrack>& track)
{
	boost::shared_ptr<AudioFileSource> source;
	SourceList sources;
	string linked_path;
	SoundFileInfo finfo;
	int ret = 0;

	track_canvas.get_window()->set_cursor (Gdk::Cursor (Gdk::WATCH));
	ARDOUR_UI::instance()->flush_pending ();

	for (vector<Glib::ustring>::iterator p = paths.begin(); p != paths.end(); ++p) {

		ustring path = *p;

		/* lets see if we can link it into the session */
		
		linked_path = session->sound_dir();
		linked_path += '/';
		linked_path += Glib::path_get_basename (path);
		
		if (link (path.c_str(), linked_path.c_str()) == 0) {

			/* there are many reasons why link(2) might have failed.
			   but if it succeeds, we now have a link in the
			   session sound dir that will protect against
			   unlinking of the original path. nice.
			*/
			
			path = linked_path;

		} else {

			/* one possible reason is that its already linked */

			if (errno == EEXIST) {
				struct stat sb;

				if (stat (linked_path.c_str(), &sb) == 0) {
					if (sb.st_nlink > 1) { // its a hard link, assume its the one we want
						path = linked_path;
					}
				}
			}
		}
		
		/* note that we temporarily truncated _id at the colon */
		
		string error_msg;

		if (!AudioFileSource::get_soundfile_info (path, finfo, error_msg)) {
			error << string_compose(_("Editor: cannot open file \"%1\", (%2)"), path, error_msg ) << endmsg;
			goto out;
		}
		
		if (check_sample_rate  && (finfo.samplerate != (int) session->frame_rate())) {
			vector<string> choices;
			
			if (multifile) {
				choices.push_back (_("Cancel entire import"));
				choices.push_back (_("Don't embed it"));
				choices.push_back (_("Embed all without questions"));
			
				Gtkmm2ext::Choice rate_choice (
					string_compose (_("%1\nThis audiofile's sample rate doesn't match the session sample rate!"), 
							short_path (path, 40)),
					choices, false);
				
				int resx = rate_choice.run ();
				
				switch (resx) {
				case 0: /* stop a multi-file import */
					ret = -2;
					goto out;
				case 1: /* don't embed this one */
					ret = -1;
					goto out;
				case 2: /* do it, and the rest without asking */
					check_sample_rate = false;
					break;
				case 3: /* do it */
					break;
				default:
					ret = -2;
					goto out;
				}
			} else {
				choices.push_back (_("Cancel"));
				choices.push_back (_("Embed it anyway"));
			
				Gtkmm2ext::Choice rate_choice (
					string_compose (_("%1\nThis audiofile's sample rate doesn't match the session sample rate!"), path),
					choices, false);
				
				int resx = rate_choice.run ();
				
				switch (resx) {
				case 0: /* don't import */
					ret = -1;
					goto out;
				case 1: /* do it */
					break;
				default:
					ret = -2;
					goto out;
				}
			}
		}
		
		track_canvas.get_window()->set_cursor (Gdk::Cursor (Gdk::WATCH));

		for (int n = 0; n < finfo.channels; ++n) {
			try {

				/* check if we have this thing embedded already */

				boost::shared_ptr<Source> s;

				if ((s = session->source_by_path_and_channel (path, n)) == 0) {
					source = boost::dynamic_pointer_cast<AudioFileSource> (SourceFactory::createReadable 
											       (*session, path,  n,
												(mode == ImportAsTapeTrack ? 
												 AudioFileSource::Destructive : 
												 AudioFileSource::Flag (0)),
												true, true));
				} else {
					source = boost::dynamic_pointer_cast<AudioFileSource> (s);
				}

				sources.push_back(source);
			} 
			
			catch (failed_constructor& err) {
				error << string_compose(_("could not open %1"), path) << endmsg;
				goto out;
			}
			
			ARDOUR_UI::instance()->flush_pending ();
		}
	}

	if (sources.empty()) {
		goto out;
	}

	ret = add_sources (paths, sources, pos, mode, target_regions, target_tracks, track, true);

  out:
	track_canvas.get_window()->set_cursor (*current_canvas_cursor);
	return ret;
}

int
Editor::add_sources (vector<Glib::ustring> paths, SourceList& sources, nframes64_t& pos, ImportMode mode, 
		     int target_regions, int target_tracks, boost::shared_ptr<AudioTrack>& track, bool add_channel_suffix)
{
	vector<boost::shared_ptr<AudioRegion> > regions;
	ustring region_name;
	uint32_t input_chan = 0;
	uint32_t output_chan = 0;

	if (pos == -1) { // "use timestamp"
		if (sources[0]->natural_position() != 0) {
			pos = sources[0]->natural_position();
		} else {
			// XXX is this the best alternative ?
			pos = edit_cursor->current_frame;
		}
	}

	if (target_regions == 1) {

		/* take all the sources we have and package them up as a region */

		region_name = region_name_from_path (paths.front(), (sources.size() > 1), false);
		
		regions.push_back (boost::dynamic_pointer_cast<AudioRegion> 
				   (RegionFactory::create (sources, 0, sources[0]->length(), region_name, 0,
							   Region::Flag (Region::DefaultFlags|Region::WholeFile|Region::External))));
		
	} else if (target_regions == -1) {

		/* take each source and create a region for each one */

		SourceList just_one;
		SourceList::iterator x;
		uint32_t n;

		for (n = 0, x = sources.begin(); x != sources.end(); ++x, ++n) {

			just_one.clear ();
			just_one.push_back (*x);
			
			boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource> (*x);

			region_name = region_name_from_path (afs->path(), false, true, sources.size(), n);

			regions.push_back (boost::dynamic_pointer_cast<AudioRegion> 
					   (RegionFactory::create (just_one, 0, (*x)->length(), region_name, 0,
								   Region::Flag (Region::DefaultFlags|Region::WholeFile|Region::External))));

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
		output_chan = (session->master_out() ? session->master_out()->n_inputs() : input_chan);
	} else {
		output_chan = input_chan;
	}

	int n = 0;

	for (vector<boost::shared_ptr<AudioRegion> >::iterator r = regions.begin(); r != regions.end(); ++r, ++n) {

		finish_bringing_in_audio (*r, input_chan, output_chan, pos, mode, track);

		if (target_tracks != 1) {
			track.reset ();
		} else {
			pos += (*r)->length();
		} 
	}

	/* setup peak file building in another thread */

	for (SourceList::iterator x = sources.begin(); x != sources.end(); ++x) {
		SourceFactory::setup_peakfile (*x, true);
	}

	return 0;
}
	
int
Editor::finish_bringing_in_audio (boost::shared_ptr<AudioRegion> region, uint32_t in_chans, uint32_t out_chans, nframes64_t& pos, 
				  ImportMode mode, boost::shared_ptr<AudioTrack>& existing_track)
{
	switch (mode) {
	case ImportAsRegion:
		/* relax, its been done */
		break;
		
	case ImportToTrack:
	{
		if (!existing_track) {

			if (selection->tracks.empty()) {
				return -1;
			}
			
			AudioTimeAxisView* atv = dynamic_cast<AudioTimeAxisView*>(selection->tracks.front());
			
			if (!atv) {
				return -1;
			}
			
			existing_track = atv->audio_track();
		}

		boost::shared_ptr<Playlist> playlist = existing_track->diskstream()->playlist();
		boost::shared_ptr<AudioRegion> copy (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (region)));
		begin_reversible_command (_("insert sndfile"));
		XMLNode &before = playlist->get_state();
		playlist->add_region (copy, pos);
		session->add_command (new MementoCommand<Playlist>(*playlist, &before, &playlist->get_state()));
		commit_reversible_command ();
		break;
	}

	case ImportAsTrack:
	{ 
		if (!existing_track) {
			list<boost::shared_ptr<AudioTrack> > at (session->new_audio_track (in_chans, out_chans, Normal, 1));

			if (at.empty()) {
				return -1;
			}

			existing_track = at.front();
			existing_track->set_name (region->name(), this);
		}

		boost::shared_ptr<AudioRegion> copy (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (region)));
		existing_track->diskstream()->playlist()->add_region (copy, pos);
		break;
	}


	case ImportAsTapeTrack:
	{
		list<boost::shared_ptr<AudioTrack> > at (session->new_audio_track (in_chans, out_chans, Destructive));
		if (!at.empty()) {
			boost::shared_ptr<AudioRegion> copy (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (region)));
			at.front()->set_name (basename_nosuffix (copy->name()), this);
			at.front()->diskstream()->playlist()->add_region (copy, pos);
		}
		break;
	}
	}

	return 0;
}

void *
Editor::_import_thread (void *arg)
{
	PBD::ThreadCreated (pthread_self(), X_("Import"));

	Editor *ed = (Editor *) arg;
	return ed->import_thread ();
}

void *
Editor::import_thread ()
{
	session->import_audiofile (import_status);
	pthread_exit_pbd (0);
	/*NOTREACHED*/
	return 0;
}

gint
Editor::import_progress_timeout (void *arg)
{
	interthread_progress_label.set_text (import_status.doing_what);

	if (import_status.freeze) {
		interthread_cancel_button.set_sensitive(false);
	} else {
		interthread_cancel_button.set_sensitive(true);
	}

	if (import_status.doing_what == "building peak files") {
		interthread_progress_bar.pulse ();
		return FALSE;
	} else {
		interthread_progress_bar.set_fraction (import_status.progress);
	}

	return !(import_status.done || import_status.cancel);
}

