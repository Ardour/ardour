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

    $Id$
*/

#include <pbd/pthread_utils.h>
#include <pbd/basename.h>

#include <gtkmm2ext/choice.h>

#include <ardour/session.h>
#include <ardour/audioplaylist.h>
#include <ardour/audioregion.h>
#include <ardour/audio_diskstream.h>
#include <ardour/utils.h>
#include <ardour/audio_track.h>
#include <ardour/audioplaylist.h>
#include <ardour/audiofilesource.h>
#include <pbd/memento_command.h>

#include "ardour_ui.h"
#include "editor.h"
#include "sfdb_ui.h"
#include "editing.h"
#include "audio_time_axis.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace sigc;
using namespace Gtk;
using namespace Editing;

/* Functions supporting the incorporation of external (non-captured) audio material into ardour */

void
Editor::add_external_audio_action (ImportMode mode)
{
	jack_nframes_t& pos = edit_cursor->current_frame;
	AudioTrack* track = 0;

	if (!selection->tracks.empty()) {
		AudioTimeAxisView* atv = dynamic_cast<AudioTimeAxisView*>(selection->tracks.front());
		if (atv) {
			track = atv->audio_track();
		}
	}

	bring_in_external_audio (mode, track, pos, false);
}

void
Editor::bring_in_external_audio (ImportMode mode, AudioTrack* track, jack_nframes_t& pos, bool prompt)
{
	if (session == 0) {
		MessageDialog msg (0, _("You can't import or embed an audiofile until you have a session loaded."));
		msg.run ();
		return;
	}

	SoundFileOmega sfdb (_("Add existing audio to session"), session);
	sfdb.set_mode (mode);

	switch (sfdb.run()) {
	case SoundFileOmega::ResponseImport:
		do_import (sfdb.get_paths(), sfdb.get_split(), mode, track, pos, prompt);
		break;
		
	case SoundFileOmega::ResponseEmbed:
		do_embed (sfdb.get_paths(), sfdb.get_split(), mode, track, pos, prompt);
		break;

	default:
		break;
	}
}

void
Editor::do_import (vector<Glib::ustring> paths, bool split, ImportMode mode, AudioTrack* track, jack_nframes_t& pos, bool prompt)
{
	/* SFDB sets "multichan" to true to indicate "split channels"
	   so reverse the setting to match the way libardour
	   interprets it.
	*/
	
	import_status.multichan = !split;

	if (interthread_progress_window == 0) {
		build_interthread_progress_window ();
	}
	
	/* for each path that was selected, import it and then potentially create a new track
	   containing the new region as the sole contents.
	*/

	for (vector<Glib::ustring>::iterator i = paths.begin(); i != paths.end(); ++i ) {
		import_sndfile (*i, mode, track, pos);
	}

	interthread_progress_window->hide_all ();
}

void
Editor::do_embed (vector<Glib::ustring> paths, bool split, ImportMode mode, AudioTrack* track, jack_nframes_t& pos, bool prompt)
{
	bool multiple_files = paths.size() > 1;
	bool check_sample_rate = true;
	vector<Glib::ustring>::iterator i;
	
	for (i = paths.begin(); i != paths.end(); ++i) {
		int ret = embed_sndfile (*i, split, multiple_files, check_sample_rate, mode, track, pos, prompt);

		if (ret < -1) {
			break;
		}
	}

	if (i == paths.end()) {
		session->save_state ("");
	}
}

int
Editor::import_sndfile (Glib::ustring path, ImportMode mode, AudioTrack* track, jack_nframes_t& pos)
{
	interthread_progress_window->set_title (string_compose (_("ardour: importing %1"), path));
	interthread_progress_window->set_position (Gtk::WIN_POS_MOUSE);
	interthread_progress_window->show_all ();
	interthread_progress_bar.set_fraction (0.0f);
	interthread_cancel_label.set_text (_("Cancel Import"));
	current_interthread_info = &import_status;

	import_status.pathname = path;
	import_status.done = false;
	import_status.cancel = false;
	import_status.freeze = false;
	import_status.done = 0.0;
	
	interthread_progress_connection = Glib::signal_timeout().connect 
		(bind (mem_fun(*this, &Editor::import_progress_timeout), (gpointer) 0), 100);
	
	track_canvas.get_window()->set_cursor (Gdk::Cursor (Gdk::WATCH));
	ARDOUR_UI::instance()->flush_pending ();

	/* start import thread for this path. this will ultimately call Session::import_audiofile()
	   and if successful will add the file as a region to the session region list.
	*/
	
	pthread_create_and_store ("import", &import_status.thread, 0, _import_thread, this);
	pthread_detach (import_status.thread);
	
	while (!(import_status.done || import_status.cancel)) {
		gtk_main_iteration ();
	}
	
	import_status.done = true;
	interthread_progress_connection.disconnect ();
	
	/* import thread finished - see if we should build a new track */
	
	if (!import_status.new_regions.empty()) {
		AudioRegion& region (*import_status.new_regions.front());
		finish_bringing_in_audio (region, region.n_channels(), region.n_channels(), track, pos, mode);
	}

	track_canvas.get_window()->set_cursor (*current_canvas_cursor);
	return 0;
}

int
Editor::embed_sndfile (Glib::ustring path, bool split, bool multiple_files, bool& check_sample_rate, ImportMode mode, 
		       AudioTrack* track, jack_nframes_t& pos, bool prompt)
{
	AudioFileSource *source = 0; /* keep g++ quiet */
	AudioRegion::SourceList sources;
	AudioRegion* region;
	string idspec;
	string linked_path;
	SoundFileInfo finfo;
	string region_name;
	uint32_t input_chan;
	uint32_t output_chan;

	track_canvas.get_window()->set_cursor (Gdk::Cursor (Gdk::WATCH));
	ARDOUR_UI::instance()->flush_pending ();

	/* lets see if we can link it into the session */
	
	linked_path = session->sound_dir();
	linked_path += Glib::path_get_basename (path);

	if (link (path.c_str(), linked_path.c_str()) == 0) {

		/* there are many reasons why link(2) might have failed.
		   but if it succeeds, we now have a link in the
		   session sound dir that will protect against
		   unlinking of the original path. nice.
		*/

		path = linked_path;
	}

	/* note that we temporarily truncated _id at the colon */

	string error_msg;

	if (!AudioFileSource::get_soundfile_info (path, finfo, error_msg)) {
		error << string_compose(_("Editor: cannot open file \"%1\", (%2)"), selection, error_msg ) << endmsg;
		return 0;
	}
	
	if (check_sample_rate  && (finfo.samplerate != (int) session->frame_rate())) {
		vector<string> choices;
		
		if (multiple_files) {
			choices.push_back (_("Cancel entire import"));
			choices.push_back (_("Don't embed it"));
			choices.push_back (_("Embed all without questions"));
		} else {
			choices.push_back (_("Cancel"));
		}

		choices.push_back (_("Embed it anyway"));
		
		Gtkmm2ext::Choice rate_choice (
			string_compose (_("%1\nThis audiofile's sample rate doesn't match the session sample rate!"), path),
			choices, false);
		
		switch (rate_choice.run()) {
		case 0: /* stop a multi-file import */
		case 1: /* don't import this one */
			return -1;
		case 2: /* do it, and the rest without asking */
			check_sample_rate = false;
			break;
		case 3: /* do it */
			break;
		default:
			return -2;
		}
	}
	
	track_canvas.get_window()->set_cursor (Gdk::Cursor (Gdk::WATCH));
	ARDOUR_UI::instance()->flush_pending ();
	
	/* make the proper number of channels in the region */

	for (int n = 0; n < finfo.channels; ++n)
	{
		idspec = path;
		idspec += string_compose(":%1", n);
		
		try {
			source = AudioFileSource::create (idspec.c_str(), (mode == ImportAsTrack ? AudioFileSource::Destructive : AudioFileSource::Flag (0)));
			sources.push_back(source);
		} 
		
		 catch (failed_constructor& err) {
			 error << string_compose(_("could not open %1"), path) << endmsg;
			 goto out;
		 }
		
		 ARDOUR_UI::instance()->flush_pending ();
	}
	
	if (sources.empty()) {
		goto out;
	}
	
	region_name = PBD::basename_nosuffix (path);
	region_name += "-0";
	
	region = new AudioRegion (sources, 0, sources[0]->length(), region_name, 0,
				  Region::Flag (Region::DefaultFlags|Region::WholeFile|Region::External));
	
	input_chan = finfo.channels;

	if (session->get_output_auto_connect() & Session::AutoConnectMaster) {
		output_chan = (session->master_out() ? session->master_out()->n_inputs() : input_chan);
	} else {
		output_chan = input_chan;
	}
	
	finish_bringing_in_audio (*region, input_chan, output_chan, track, pos, mode);
	
  out:
	track_canvas.get_window()->set_cursor (*current_canvas_cursor);
	return 0;
}

int
Editor::finish_bringing_in_audio (AudioRegion& region, uint32_t in_chans, uint32_t out_chans, AudioTrack* track, jack_nframes_t& pos, ImportMode mode)
{
	AudioRegion* copy;

	switch (mode) {
	case ImportAsRegion:
		/* relax, its been done */
		break;
		
	case ImportToTrack:
		if (track) {
			Playlist* playlist  = track->diskstream()->playlist();
			
			AudioRegion* copy = new AudioRegion (region);
			begin_reversible_command (_("insert sndfile"));
                        XMLNode &before = playlist->get_state();
			playlist->add_region (*copy, pos);
			session->add_command (new MementoCommand<Playlist>(*playlist, &before, &playlist->get_state()));
			commit_reversible_command ();

			pos += region.length();
		}
		break;
		
	case ImportAsTrack:
	{ 
		vector<boost::shared_ptr<AudioTrack> > at (session->new_audio_track (in_chans, out_chans, Normal, 1));
		if (!at.empty()) {
			copy = new AudioRegion (region);
			at.front()->diskstream()->playlist()->add_region (*copy, pos);
		}
		break;
	}

	case ImportAsTapeTrack:
	{
		vector<boost::shared_ptr<AudioTrack> > at (session->new_audio_track (in_chans, out_chans, Destructive));
		if (!at.empty()) {
			copy = new AudioRegion (region);
			at.front()->diskstream()->playlist()->add_region (*copy, pos);
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

