/*
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
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

#include "pbd/gstdio_compat.h"

#include "ardour/profile.h"
#include "ardour/rc_configuration.h"
#include "ardour/audio_track.h"
#include "ardour/audioregion.h"

#include "ardour_ui.h"
#include "editor.h"
#include "canvas/rectangle.h"
#include "editing.h"
#include "audio_time_axis.h"
#include "video_image_frame.h"
#include "export_video_dialog.h"
#include "interthread_progress_window.h"

#include "pbd/openuri.h"
#include "pbd/i18n.h"

using namespace std;

void
Editor::set_video_timeline_height (const int h)
{
	if (videotl_bar_height == h) { return; }
	if (h < 2 || h > 8) { return; }
  videotl_bar_height = h;
	videotl_label.set_size_request (-1, (int)timebar_height * videotl_bar_height);
	ARDOUR_UI::instance()->video_timeline->set_height(videotl_bar_height * timebar_height);
	update_ruler_visibility();
}

void
Editor::update_video_timeline (bool flush)
{
	if (!ARDOUR_UI::instance()->video_timeline) {
		return;
	}

	if (flush) {
		ARDOUR_UI::instance()->video_timeline->flush_local_cache();
	}
	if (!ruler_video_action->get_active()) return;
	ARDOUR_UI::instance()->video_timeline->update_video_timeline();
}

bool
Editor::is_video_timeline_locked ()
{
	return ARDOUR_UI::instance()->video_timeline->is_offset_locked();
}

void
Editor::set_video_timeline_locked (const bool l)
{
	ARDOUR_UI::instance()->video_timeline->set_offset_locked(l);
}

void
Editor::toggle_video_timeline_locked ()
{
	ARDOUR_UI::instance()->video_timeline->toggle_offset_locked();
}

void
Editor::embed_audio_from_video (std::string path, samplepos_t n, bool lock_position_to_video)
{
	vector<std::string> paths;
	paths.push_back(path);
	current_interthread_info = &import_status;
	import_status.current = 1;
	import_status.total = paths.size ();
	import_status.all_done = false;

	ImportProgressWindow ipw (&import_status, _("Import"), _("Cancel Import"));
	ipw.show ();

	boost::shared_ptr<ARDOUR::Track> track;
	std::string const& gid = ARDOUR::Playlist::generate_pgroup_id ();
	Temporal::timepos_t pos (n);

	bool ok = (import_sndfiles (paths, Editing::ImportDistinctFiles, Editing::ImportAsTrack, ARDOUR::SrcBest, pos, 1, 1, track, gid, false) == 0);

	if (ok && track) {
		if (lock_position_to_video) {
			boost::shared_ptr<ARDOUR::Playlist> pl = track->playlist();
			pl->find_next_region (pos, ARDOUR::End, 0)->set_video_locked (true);
		}
		_session->save_state ("", true);
	}

	import_status.all_done = true;
	::g_unlink(path.c_str());
}
