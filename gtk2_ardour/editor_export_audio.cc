/*
    Copyright (C) 2001 Paul Davis 

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

#include <unistd.h>
#include <climits>
#include "export_dialog.h"
#include "editor.h"
#include "public_editor.h"
#include "selection.h"
#include "time_axis_view.h"
#include "audio_time_axis.h"
#include "regionview.h"
#include "ardour_message.h"

#include <pbd/pthread_utils.h>
#include <ardour/types.h>
#include <ardour/export.h>
#include <ardour/audio_track.h>
#include <ardour/filesource.h>
#include <ardour/diskstream.h>
#include <ardour/audioregion.h>
#include <ardour/audioplaylist.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace Gtk;

void
Editor::export_session()
{
	if (session) {
		export_range (0, session->current_end_frame());
	}
}

void
Editor::export_selection ()
{
	if (session) {
		if (selection->time.empty()) {
			ArdourMessage message (this, X_("norange"), _("There is no range to export.\n\nSelect a range using the range mouse mode"));
			return;
		}

		export_range (selection->time.front().start, 
			      selection->time.front().end);
	}
}

void
Editor::export_range (jack_nframes_t start, jack_nframes_t end)
{
	if (session) {
		if (export_dialog == 0) {
			export_dialog = new ExportDialog (*this);
		}
		
		export_dialog->connect_to_session (session);
		export_dialog->set_range (start, end);
		export_dialog->start_export();
	}
}	

void
Editor::export_region ()
{
	if (clicked_regionview == 0) {
		return;
	}

	ExportDialog* dialog = new ExportDialog (*this, &clicked_regionview->region);
		
	dialog->connect_to_session (session);
	dialog->set_range (0, clicked_regionview->region.length());
	dialog->start_export();
}

int
Editor::write_region_selection (AudioRegionSelection& regions)
{
	for (AudioRegionSelection::iterator i = regions.begin(); i != regions.end(); ++i) {
		if (write_region ("", (*i)->region) == false) {
			return -1;
		}
	}
	return 0;
}

void
Editor::bounce_region_selection ()
{
	for (AudioRegionSelection::iterator i = selection->audio_regions.begin(); i != selection->audio_regions.end(); ++i) {
		
		AudioRegion& region ((*i)->region);
		AudioTimeAxisView* atv = dynamic_cast<AudioTimeAxisView*>(&(*i)->get_time_axis_view());
		AudioTrack* track = dynamic_cast<AudioTrack*>(&(atv->route()));

		InterThreadInfo itt;

		itt.done = false;
		itt.cancel = false;
		itt.progress = 0.0f;

		track->bounce_range (region.position(), region.position() + region.length(), itt);
	}
}

bool
Editor::write_region (string path, AudioRegion& region)
{
	FileSource* fs;
	const jack_nframes_t chunk_size = 4096;
	jack_nframes_t to_read;
	Sample buf[chunk_size];
	gain_t gain_buffer[chunk_size];
	jack_nframes_t pos;
	char s[PATH_MAX+1];
	uint32_t cnt;
	vector<FileSource *> sources;
	uint32_t nchans;
	
	nchans = region.n_channels();
	
	/* don't do duplicate of the entire source if that's what is going on here */

	if (region.start() == 0 && region.length() == region.source().length()) {
		/* XXX should link(2) to create a new inode with "path" */
		return true;
	}

	if (path.length() == 0) {

		for (uint32_t n=0; n < nchans; ++n) {
			
			for (cnt = 0; cnt < 999999; ++cnt) {
				if (nchans == 1) {
					snprintf (s, sizeof(s), "%s/%s_%" PRIu32 ".wav", session->sound_dir().c_str(),
						  legalize_for_path(region.name()).c_str(), cnt);
				}
				else {
					snprintf (s, sizeof(s), "%s/%s_%" PRIu32 "-%" PRId32 ".wav", session->sound_dir().c_str(),
						  legalize_for_path(region.name()).c_str(), cnt, n);
				}

				path = s;
				
				if (::access (path.c_str(), F_OK) != 0) {
					break;
				}
			}
			
			if (cnt == 999999) {
				error << "" << endmsg;
				goto error_out;
			}
			
		
			
			try {
				fs = new FileSource (path, session->frame_rate());
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
	
	to_read = region.length();
	pos = region.position();

	while (to_read) {
		jack_nframes_t this_time;

		this_time = min (to_read, chunk_size);

		for (vector<FileSource *>::iterator src=sources.begin(); src != sources.end(); ++src) {

			fs = (*src);
			
			if (region.read_at (buf, buf, gain_buffer, pos, this_time) != this_time) {
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
	
	for (vector<FileSource *>::iterator src = sources.begin(); src != sources.end(); ++src) {
		(*src)->update_header (0, *now, tnow);
	}

	return true;

error_out:

	for (vector<FileSource*>::iterator i = sources.begin(); i != sources.end(); ++i) {
		(*i)->mark_for_remove ();
		delete (*i);
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

			Playlist* playlist = atv->get_diskstream()->playlist();
			
			if (playlist && write_audio_range (*playlist, atv->get_diskstream()->n_channels(), ts) == 0) {
				ret = -1;
				break;
			}
		}
	}

	return ret;
}

bool
Editor::write_audio_range (Playlist& playlist, uint32_t channels, list<AudioRange>& range)
{
	FileSource* fs;
	const jack_nframes_t chunk_size = 4096;
	jack_nframes_t nframes;
	Sample buf[chunk_size];
	gain_t gain_buffer[chunk_size];
	jack_nframes_t pos;
	char s[PATH_MAX+1];
	uint32_t cnt;
	string path;
	vector<FileSource *> sources;

	for (uint32_t n=0; n < channels; ++n) {
		
		for (cnt = 0; cnt < 999999; ++cnt) {
			if (channels == 1) {
				snprintf (s, sizeof(s), "%s/%s_%" PRIu32 ".wav", session->sound_dir().c_str(),
					  legalize_for_path(playlist.name()).c_str(), cnt);
			}
			else {
				snprintf (s, sizeof(s), "%s/%s_%" PRIu32 "-%" PRId32 ".wav", session->sound_dir().c_str(),
					  legalize_for_path(playlist.name()).c_str(), cnt, n);
			}
			
			if (::access (s, F_OK) != 0) {
				break;
			}
		}
		
		if (cnt == 999999) {
			error << "" << endmsg;
			goto error_out;
		}

		path = s;
		
		try {
			fs = new FileSource (path, session->frame_rate());
		}
		
		catch (failed_constructor& err) {
			goto error_out;
		}
		
		sources.push_back (fs);

	}
	

	for (list<AudioRange>::iterator i = range.begin(); i != range.end();) {
	
		nframes = (*i).length();
		pos = (*i).start;
		
		while (nframes) {
			jack_nframes_t this_time;
			
			this_time = min (nframes, chunk_size);

			for (uint32_t n=0; n < channels; ++n) {

				fs = sources[n];
				
				if (playlist.read (buf, buf, gain_buffer, pos, this_time, n) != this_time) {
					break;
				}
				
				if (fs->write (buf, this_time) != this_time) {
					goto error_out;
				}
			}
			
			nframes -= this_time;
			pos += this_time;
		}
		
		list<AudioRange>::iterator tmp = i;
		++tmp;

		if (tmp != range.end()) {
			
			/* fill gaps with silence */
			
			nframes = (*tmp).start - (*i).end;

			while (nframes) {

				jack_nframes_t this_time = min (nframes, chunk_size);
				memset (buf, 0, sizeof (Sample) * this_time);

				for (uint32_t n=0; n < channels; ++n) {

					fs = sources[n];
					if (fs->write (buf, this_time) != this_time) {
						goto error_out;
					}
				}

				nframes -= this_time;
			}
		}

		i = tmp;
	}

	time_t tnow;
	struct tm* now;
	time (&tnow);
	now = localtime (&tnow);

	for (uint32_t n=0; n < channels; ++n) {
		sources[n]->update_header (0, *now, tnow);
		// do we need to ref it again?
	}
	
	return true;

error_out:
	/* unref created files */

	for (vector<FileSource*>::iterator i = sources.begin(); i != sources.end(); ++i) {
		(*i)->mark_for_remove ();
		delete *i;
	}

	return false;
}

void
Editor::write_selection ()
{
	if (!selection->time.empty()) {
		write_audio_selection (selection->time);
	} else if (!selection->audio_regions.empty()) {
		write_region_selection (selection->audio_regions);
	}
}
