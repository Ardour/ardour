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
using namespace Editing;
using Glib::ustring;

/* Functions supporting the incorporation of external (non-captured) audio material into ardour */

void
Editor::add_external_audio_action (ImportMode mode)
{
	nframes_t& pos = edit_cursor->current_frame;
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
Editor::bring_in_external_audio (ImportMode mode, AudioTrack* track, nframes_t& pos, bool prompt)
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
		do_import (sfdb.get_paths(), sfdb.get_split(), sfdb.get_mode(), track, pos, prompt);
		break;
		
	case SoundFileOmega::ResponseEmbed:
		do_embed (sfdb.get_paths(), sfdb.get_split(), sfdb.get_mode(), track, pos, prompt);
		break;

	default:
		break;
	}
}

void
Editor::do_import (vector<ustring> paths, bool split, ImportMode mode, AudioTrack* track, nframes_t& pos, bool prompt)
{
	/* SFDB sets "multichan" to true to indicate "split channels"
	   so reverse the setting to match the way libardour
	   interprets it.
	*/
	
	import_status.multichan = !split;

	if (interthread_progress_window == 0) {
		build_interthread_progress_window ();
	}

	vector<ustring> to_import;

	for (vector<ustring>::iterator a = paths.begin(); a != paths.end(); ++a) {

		to_import.clear ();
		to_import.push_back (*a);

		import_sndfile (to_import, mode, track, pos);
	}

	interthread_progress_window->hide_all ();
}

void
Editor::do_embed (vector<ustring> paths, bool split, ImportMode mode, AudioTrack* track, nframes_t& pos, bool prompt)
{
	bool multiple_files = paths.size() > 1;
	bool check_sample_rate = true;
	vector<ustring>::iterator a;

	for (a = paths.begin(); a != paths.end(); ) {

		cerr << "Considering embed of " << (*a) << endl;
	
		Glib::ustring path = *a;
		Glib::ustring pair_base;
		vector<ustring> to_embed;

		to_embed.push_back (path);
		a = paths.erase (a);

		if (path_is_paired (path, pair_base)) {

			ustring::size_type len = pair_base.length();

			for (vector<Glib::ustring>::iterator b = paths.begin(); b != paths.end(); ) {

				if (((*b).substr (0, len) == pair_base) && ((*b).length() == path.length())) {

					to_embed.push_back (*b);
						
					/* don't process this one again */

					b = paths.erase (b);
					break;

				} else {
					++b;
				}
			}
		}

		if (to_embed.size() > 1) {

			vector<string> choices;

			choices.push_back (string_compose (_("Import as a %1 region"),
							   to_embed.size() > 2 ? _("multichannel") : _("stereo")));
			choices.push_back (_("Import as multiple regions"));
			
			Gtkmm2ext::Choice chooser (string_compose (_("Paired files detected (%1, %2 ...).\nDo you want to:"),
								   to_embed[0],
								   to_embed[1]),
						   choices);
			
			if (chooser.run () == 0) {
				
				/* keep them paired */

				if (embed_sndfile (to_embed, split, multiple_files, check_sample_rate, mode, track, pos, prompt) < -1) {
					break;
				}

			} else {

				/* one thing per file */

				vector<ustring> foo;

				for (vector<ustring>::iterator x = to_embed.begin(); x != to_embed.end(); ++x) {

					foo.clear ();
					foo.push_back (*x);

					if (embed_sndfile (foo, split, multiple_files, check_sample_rate, mode, track, pos, prompt) < -1) {
						break;
					}
				}
			}

		} else {
			
			if (embed_sndfile (to_embed, split, multiple_files, check_sample_rate, mode, track, pos, prompt) < -1) {
				break;
			}
		}
	}
	
	if (a == paths.end()) {
		session->save_state ("");
	}
}

int
Editor::import_sndfile (vector<ustring> paths, ImportMode mode, AudioTrack* track, nframes_t& pos)
{
	interthread_progress_window->set_title (string_compose (_("ardour: importing %1"), paths.front()));
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
	
	import_status.done = true;
	interthread_progress_connection.disconnect ();
	
	/* import thread finished - see if we should build a new track */
	
	if (!import_status.new_regions.empty()) {
		boost::shared_ptr<AudioRegion> region (import_status.new_regions.front());
		finish_bringing_in_audio (region, region->n_channels(), region->n_channels(), track, pos, mode);
	}

	track_canvas.get_window()->set_cursor (*current_canvas_cursor);
	return 0;
}

int
Editor::embed_sndfile (vector<Glib::ustring> paths, bool split, bool multiple_files, bool& check_sample_rate, ImportMode mode, 
		       AudioTrack* track, nframes_t& pos, bool prompt)
{
	boost::shared_ptr<AudioFileSource> source;
	SourceList sources;
	boost::shared_ptr<AudioRegion> region;
	string linked_path;
	SoundFileInfo finfo;
	ustring region_name;
	uint32_t input_chan = 0;
	uint32_t output_chan = 0;
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
		}
		
		/* note that we temporarily truncated _id at the colon */
		
		string error_msg;
		
		if (!AudioFileSource::get_soundfile_info (path, finfo, error_msg)) {
			error << string_compose(_("Editor: cannot open file \"%1\", (%2)"), selection, error_msg ) << endmsg;
			goto out;
		}
		
		if (check_sample_rate  && (finfo.samplerate != (int) session->frame_rate())) {
			vector<string> choices;
			
			if (multiple_files) {
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
				case 1: /* don't import this one */
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
		ARDOUR_UI::instance()->flush_pending ();
	
		/* make the proper number of channels in the region */
		
		input_chan += finfo.channels;

		for (int n = 0; n < finfo.channels; ++n)
		{
			try {
				source = boost::dynamic_pointer_cast<AudioFileSource> (SourceFactory::createReadable 
										       (*session, path,  n,
											(mode == ImportAsTapeTrack ? 
											 AudioFileSource::Destructive : 
											 AudioFileSource::Flag (0))));

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

	if (sources[0]->natural_position() != 0) {
		pos = sources[0]->natural_position();
	} 

	region_name = region_name_from_path (paths.front(), (sources.size() > 1));
	
	region = boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (sources, 0, sources[0]->length(), region_name, 0,
										  Region::Flag (Region::DefaultFlags|Region::WholeFile|Region::External)));

	if (Config->get_output_auto_connect() & AutoConnectMaster) {
		output_chan = (session->master_out() ? session->master_out()->n_inputs() : input_chan);
	} else {
		output_chan = input_chan;
	}

	finish_bringing_in_audio (region, input_chan, output_chan, track, pos, mode);
	
  out:
	track_canvas.get_window()->set_cursor (*current_canvas_cursor);
	return ret;
}

int
Editor::finish_bringing_in_audio (boost::shared_ptr<AudioRegion> region, uint32_t in_chans, uint32_t out_chans, AudioTrack* track, nframes_t& pos, ImportMode mode)
{
	switch (mode) {
	case ImportAsRegion:
		/* relax, its been done */
		break;
		
	case ImportToTrack:
		if (track) {
			boost::shared_ptr<Playlist> playlist = track->diskstream()->playlist();
			
			boost::shared_ptr<AudioRegion> copy (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (region)));
			begin_reversible_command (_("insert sndfile"));
                        XMLNode &before = playlist->get_state();
			playlist->add_region (copy, pos);
			session->add_command (new MementoCommand<Playlist>(*playlist, &before, &playlist->get_state()));
			commit_reversible_command ();

			pos += region->length();
		}
		break;
		
	case ImportAsTrack:
	{ 
		list<boost::shared_ptr<AudioTrack> > at (session->new_audio_track (in_chans, out_chans, Normal, 1));
		if (!at.empty()) {
			boost::shared_ptr<AudioRegion> copy (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (region)));
			at.front()->diskstream()->playlist()->add_region (copy, pos);
		}
		break;
	}

	case ImportAsTapeTrack:
	{
		list<boost::shared_ptr<AudioTrack> > at (session->new_audio_track (in_chans, out_chans, Destructive));
		if (!at.empty()) {
			boost::shared_ptr<AudioRegion> copy (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (region)));
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

