/*
    Copyright (C) 2010 Paul Davis
    Author: Robin Gareus <robin@gareus.org>

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
#ifndef __ardour_video_timeline_h__
#define __ardour_video_timeline_h__

#include <string>

#include <sigc++/signal.h>
#include "ardour/ardour.h"
#include "ardour/session.h"
#include "ardour/session_handle.h"
#include "video_image_frame.h"
#include "video_monitor.h"
#include "pbd/signals.h"
#include "canvas/group.h"

namespace ARDOUR {
	class Session;
}

class PublicEditor;

/** @class VideoTimeLine
 *  @brief video-timline controller and display
 *
 *  The video-timeline can be displayed in a canvas-group. Given a filename
 *  it queries the video-server about file-information and
 *  creates \ref VideoImageFrame as neccesary (which
 *  query the server for image-data).
 *
 *  This class contains the algorithm to position the single frames
 *  on the timeline according to current-zoom level and video-file
 *  attributes. see \ref update_video_timeline()
 *
 *  The VideoTimeLine class includes functionality to launch a video-monitor
 *  corresponding to its currently diplayed file.
 */
class VideoTimeLine : public sigc::trackable, public ARDOUR::SessionHandlePtr, public PBD::ScopedConnectionList, public PBD::StatefulDestructible
{
	public:
	VideoTimeLine (PublicEditor*, ArdourCanvas::Group*, int);
	virtual ~VideoTimeLine ();

	void set_session (ARDOUR::Session *s);
	void update_video_timeline ();
	void set_height (int);

	void save_undo (void);
	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	bool video_file_info (std::string, bool);
	double get_video_file_fps () { return video_file_fps; }
	void set_update_session_fps (bool v=true) { auto_set_session_fps = v; }

	void set_offset_locked (bool v);
	void toggle_offset_locked ();
	bool is_offset_locked () { return video_offset_lock; }

	void open_video_monitor ();
	void close_video_monitor ();
	void control_video_monitor (int, int);
	void terminated_video_monitor ();
	void manual_seek_video_monitor (framepos_t pos);

	void parameter_changed (std::string const & p);
	void set_video_server_url (std::string);
	void set_video_server_docroot (std::string);

	bool found_xjadeo () { return ((_xjadeo_bin.empty())?false:true); }
	bool check_server ();
	void flush_local_cache ();
	void vmon_update ();
	void flush_cache ();
	void save_session ();
	void close_session ();
	void sync_session_state (); /* video-monitor does not actively report window/pos changes, query it */
	float get_apv(); /* audio samples per video frame; */
	ARDOUR::framecnt_t get_duration () { return video_duration;}
	ARDOUR::frameoffset_t get_offset () { return video_offset;}
	ARDOUR::frameoffset_t quantify_frames_to_apv (ARDOUR::frameoffset_t offset) { return floor(offset/get_apv())*get_apv(); }
	void set_offset (ARDOUR::frameoffset_t offset) { video_offset = quantify_frames_to_apv(offset); } // this function does not update video_offset_p, call save_undo() to finalize changes to this! - this fn is currently only used from editor_drag.cc

	protected:

	PublicEditor *editor;
	ArdourCanvas::Group *videotl_group;
	int bar_height;

	std::string _xjadeo_bin;
	void find_xjadeo ();


	ARDOUR::frameoffset_t video_start_offset; /**< unit: audio-samples - video-file */
	ARDOUR::frameoffset_t video_offset; /**< unit: audio-samples - session */
	ARDOUR::frameoffset_t video_offset_p; /**< used for undo from editor_drag.cc */
	framepos_t video_duration;     /**< unit: audio-samples */
	std::string video_filename;
	bool        local_file;
	double      video_aspect_ratio;
	double      video_file_fps;
	bool        auto_set_session_fps;
	bool        video_offset_lock;

	std::string video_server_url;
	std::string server_docroot;

	typedef std::list<VideoImageFrame*> VideoFrames;
	VideoFrames video_frames;
	VideoImageFrame *get_video_frame (framepos_t vfn, int cut=0, int rightend = -1);
	bool        flush_frames;
	void        remove_frames ();

	std::string translated_filename ();

	VideoMonitor *vmonitor;
	bool reopen_vmonitor;

	PBD::Signal0<void> VtlUpdate;
	PBD::Signal1<void,std::string> GuiUpdate;
	void gui_update (const std::string &);

	PBD::ScopedConnection sessionsave;
};

#endif /* __ardour_video_timeline_h__ */
