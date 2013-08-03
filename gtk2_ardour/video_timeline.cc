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
#include <algorithm>
#include <sigc++/bind.h>
#include "ardour/tempo.h"

#include "pbd/file_utils.h"
#include "pbd/convert.h"
#include "ardour/session_directory.h"

#include "ardour_ui.h"
#include "public_editor.h"
#include "gui_thread.h"
#include "utils.h"
#include "utils_videotl.h"
#include "rgb_macros.h"
#include "video_timeline.h"

#include <gtkmm2ext/utils.h>
#include <pthread.h>
#include <curl/curl.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Timecode;
using namespace VideoUtils;

VideoTimeLine::VideoTimeLine (PublicEditor *ed, ArdourCanvas::Group *vbg, int initial_height)
	: editor (ed)
		, videotl_group(vbg)
		, bar_height(initial_height)
{
	video_start_offset = 0L;
	video_offset = 0L;
	video_offset_p = 0L;
	video_duration = 0L;
	auto_set_session_fps = false;
	video_offset_lock = false;
	video_aspect_ratio = 4.0/3.0;
	Config->ParameterChanged.connect (*this, invalidator (*this), ui_bind (&VideoTimeLine::parameter_changed, this, _1), gui_context());
	video_server_url = video_get_server_url(Config);
	server_docroot   = video_get_docroot(Config);
	video_filename = "";
	local_file = true;
	video_file_fps = 25.0;
	flush_frames = false;
	vmonitor=0;
	reopen_vmonitor=false;
	find_xjadeo();

	VtlUpdate.connect (*this, invalidator (*this), boost::bind (&PublicEditor::queue_visual_videotimeline_update, editor), gui_context());
	GuiUpdate.connect (*this, invalidator (*this), boost::bind (&VideoTimeLine::gui_update, this, _1), gui_context());
}

VideoTimeLine::~VideoTimeLine ()
{
	close_session();
}

/* close and save settings */
void
VideoTimeLine::save_session ()
{
	if (!_session) {
		return;
	}

	LocaleGuard lg (X_("POSIX"));

	XMLNode* node = new XMLNode(X_("Videomonitor"));
	if (!node) return;
	node->add_property (X_("active"), (vmonitor && vmonitor->is_started())?"yes":"no");
	_session->add_extra_xml (*node);

	if (vmonitor) {
		if (vmonitor->is_started()) {
			vmonitor->query_full_state(true);
		}
		vmonitor->save_session();
	}

	/* VTL settings */
	node = _session->extra_xml (X_("Videotimeline"));
	if (!node) return;
	node->add_property (X_("id"), id().to_s());
	node->add_property (X_("Height"), editor->get_videotl_bar_height());
	node->add_property (X_("VideoOffsetLock"), video_offset_lock?X_("1"):X_("0"));
	node->add_property (X_("VideoOffset"), video_offset);
	node->add_property (X_("AutoFPS"), auto_set_session_fps?X_("1"):X_("0"));
}

/* close and save settings */
void
VideoTimeLine::close_session ()
{
	if (video_duration == 0) {
		return;
	}
	sessionsave.disconnect();
	close_video_monitor();

	remove_frames();
	video_filename = "";
	video_duration = 0;
	GuiUpdate("set-xjadeo-sensitive-off");
	GuiUpdate("video-unavailable");
}

void
VideoTimeLine::sync_session_state ()
{
	if (!_session || !vmonitor || !vmonitor->is_started()) {
		return;
	}
	save_session();
}

/** load settings from session */
void
VideoTimeLine::set_session (ARDOUR::Session *s)
{
	SessionHandlePtr::set_session (s);
	if (!_session) { return ; }

	_session->SaveSession.connect_same_thread (sessionsave, boost::bind (&VideoTimeLine::save_session, this));
	LocaleGuard lg (X_("POSIX"));

	XMLNode* node = _session->extra_xml (X_("Videotimeline"));

	if (!node || !node->property (X_("Filename"))) {
		return;
	}

	ARDOUR_UI::instance()->start_video_server((Gtk::Window*)0, false);

	set_id(*node);

	const XMLProperty* proph = node->property (X_("Height"));
	if (proph) {
		editor->set_video_timeline_height(atoi(proph->value()));
	}
#if 0 /* TODO THINK: set FPS first time only ?! */
	const XMLProperty* propasfps = node->property (X_("AutoFPS"));
	if (propasfps) {
		auto_set_session_fps = atoi(propasfps->value())?true:false;
	}
#endif

	const XMLProperty* propoffset = node->property (X_("VideoOffset"));
	if (propoffset) {
		video_offset = atoll(propoffset->value());
		video_offset_p = video_offset;
	}

	const XMLProperty* proplock = node->property (X_("VideoOffsetLock"));
	if (proplock) {
		video_offset_lock = atoi(proplock->value())?true:false;
	}

	const XMLProperty* localfile = node->property (X_("LocalFile"));
	if (localfile) {
		local_file = atoi(localfile->value())?true:false;
	}

	const XMLProperty* propf = node->property (X_("Filename"));
	video_file_info(propf->value(), local_file);

	if ((node = _session->extra_xml (X_("Videomonitor")))) {
		const XMLProperty* prop = node->property (X_("active"));
		if (prop && prop->value() == "yes" && found_xjadeo() && !video_filename.empty() && local_file) {
			open_video_monitor();
		}
	}

	_session->register_with_memento_command_factory(id(), this);
	_session->config.ParameterChanged.connect (*this, invalidator (*this), ui_bind (&VideoTimeLine::parameter_changed, this, _1), gui_context());
}

void
VideoTimeLine::set_offset_locked (bool v) {
	if (_session && v != video_offset_lock) {
		_session->set_dirty ();
	}
	video_offset_lock = v;
}

void
VideoTimeLine::toggle_offset_locked () {
	video_offset_lock = !video_offset_lock;
	if (_session) {
		_session->set_dirty ();
	}
}

void
VideoTimeLine::save_undo ()
{
	if (_session && video_offset_p != video_offset) {
		_session->set_dirty ();
	}
	video_offset_p = video_offset;
}

int
VideoTimeLine::set_state (const XMLNode& node, int /*version*/)
{
	LocaleGuard lg (X_("POSIX"));
	const XMLProperty* propoffset = node.property (X_("VideoOffset"));
	if (propoffset) {
		video_offset = atoll(propoffset->value());
	}
	ARDOUR_UI::instance()->flush_videotimeline_cache(true);
	return 0;
}

XMLNode&
VideoTimeLine::get_state ()
{
	XMLNode* node = new XMLNode (X_("Videotimeline"));
	LocaleGuard lg (X_("POSIX"));
	node->add_property (X_("VideoOffset"), video_offset_p);
	return *node;
}

void
VideoTimeLine::remove_frames ()
{
	for (VideoFrames::iterator i = video_frames.begin(); i != video_frames.end(); ++i ) {
		VideoImageFrame *frame = (*i);
		delete frame;
		(*i) = 0;
	}
	video_frames.clear();
}

VideoImageFrame *
VideoTimeLine::get_video_frame (framepos_t vfn, int cut, int rightend)
{
	if (vfn==0) cut=0;
	for (VideoFrames::iterator i = video_frames.begin(); i != video_frames.end(); ++i) {
		VideoImageFrame *frame = (*i);
		if (abs(frame->get_video_frame_number()-vfn)<=cut
		    && frame->get_rightend() == rightend) { return frame; }
	}
	return 0;
}

float
VideoTimeLine::get_apv()
{
	// XXX: dup code - TODO use this fn in update_video_timeline()
	float apv = -1; /* audio samples per video frame; */
	if (!_session) return apv;

	if (_session->config.get_use_video_file_fps()) {
		if (video_file_fps == 0 ) return apv;
	} else {
		if (_session->timecode_frames_per_second() == 0 ) return apv;
	}

	if (_session->config.get_videotimeline_pullup()) {
		apv = _session->frame_rate();
	} else {
		apv = _session->nominal_frame_rate();
	}
	if (_session->config.get_use_video_file_fps()) {
		apv /= video_file_fps;
	} else {
		apv /= _session->timecode_frames_per_second();
	}
	return apv;
}

void
VideoTimeLine::update_video_timeline()
{
	if (!_session) return;

	if (_session->config.get_use_video_file_fps()) {
		if (video_file_fps == 0 ) return;
	} else {
		if (_session->timecode_frames_per_second() == 0 ) return;
	}

	const double samples_per_pixel = editor->get_current_zoom();
	const framepos_t leftmost_sample =  editor->leftmost_sample();

	/* Outline:
	 * 1) calculate how many frames there should be in current zoom (plus 1 page on each side)
	 * 2) calculate first frame and distance between video-frames (according to zoom)
	 * 3) destroy/add frames
	 * 4) reposition existing frames
	 * 5) assign framenumber to frames -> request/decode video.
	 */

	/* video-file and session properties */
	double display_vframe_width; /* unit: pixels ; width of one thumbnail in the timeline */
	float apv; /* audio samples per video frame; */
	framepos_t leftmost_video_frame; /* unit: video-frame number ; temporary var -> vtl_start */

	/* variables needed to render videotimeline -- what needs to computed first */
	framepos_t vtl_start; /* unit: audio-samples ; first displayed video-frame */
	framepos_t vtl_dist;  /* unit: audio-samples ; distance between displayed video-frames */
	unsigned int visible_video_frames; /* number of frames that fit on current canvas */

	if (_session->config.get_videotimeline_pullup()) {
		apv = _session->frame_rate();
	} else {
		apv = _session->nominal_frame_rate();
	}
	if (_session->config.get_use_video_file_fps()) {
		apv /= video_file_fps;
	} else {
		apv /= _session->timecode_frames_per_second();
	}

	display_vframe_width = bar_height * video_aspect_ratio;

	if (apv > samples_per_pixel * display_vframe_width) {
		/* high-zoom: need space between successive video-frames */
		vtl_dist = rint(apv);
	} else {
		/* continous timeline: skip video-frames */
		vtl_dist = ceil(display_vframe_width * samples_per_pixel / apv) * apv;
	}

	assert (vtl_dist > 0);
	assert (apv > 0);

	leftmost_video_frame = floor (floor((leftmost_sample - video_start_offset - video_offset ) / vtl_dist) * vtl_dist / apv);

	vtl_start = rint (video_offset + video_start_offset + leftmost_video_frame * apv);
	visible_video_frames = 2 + ceil(editor->current_page_samples() / vtl_dist); /* +2 left+right partial frames */

	/* expand timeline (cache next/prev page images) */
	vtl_start -= visible_video_frames * vtl_dist;
	visible_video_frames *=3;

	if (vtl_start < video_offset ) {
		visible_video_frames += ceil(vtl_start/vtl_dist);
		vtl_start = video_offset;
	}

	/* apply video-file constraints */
	if (vtl_start > video_start_offset + video_duration + video_offset ) {
		visible_video_frames = 0;
	}
	/* TODO optimize: compute rather than iterate */
	while (visible_video_frames > 0 && vtl_start + (visible_video_frames-1) * vtl_dist >= video_start_offset + video_duration + video_offset) {
		--visible_video_frames;
	}

	if (flush_frames) {
		remove_frames();
		flush_frames=false;
	}

	while (video_frames.size() < visible_video_frames) {
		VideoImageFrame *frame;
		frame = new VideoImageFrame(*editor, *videotl_group, display_vframe_width, bar_height, video_server_url, translated_filename());
		frame->ImgChanged.connect (*this, invalidator (*this), boost::bind (&PublicEditor::queue_visual_videotimeline_update, editor), gui_context());
		video_frames.push_back(frame);
	}

	VideoFrames outdated_video_frames;
	std::list<int> remaining;

	outdated_video_frames = video_frames;

#if 1
	/* when zoomed out, ignore shifts by +-1 frame
	 * which can occur due to rounding errors when
	 * scrolling to a new leftmost-audio frame.
	 */
	int cut =1;
	if (vtl_dist/apv < 3.0) cut =0;
#else
	int cut =0;
#endif

	for (unsigned int vfcount=0; vfcount < visible_video_frames; ++vfcount){
		framepos_t vfpos = vtl_start + vfcount * vtl_dist; /* unit: audio-frames */
		framepos_t vframeno = rint ( (vfpos - video_offset) / apv); /* unit: video-frames */
		vfpos = (vframeno * apv ) + video_offset; /* audio-frame  corresponding to /rounded/ video-frame */

		int rightend = -1; /* unit: pixels */
		if (vfpos + vtl_dist > video_start_offset + video_duration + video_offset) {
			rightend = display_vframe_width * (video_start_offset + video_duration + video_offset - vfpos) / vtl_dist;
			//printf("lf(e): %lu\n", vframeno); // XXX
		}
		VideoImageFrame * frame = get_video_frame(vframeno, cut, rightend);
		if (frame) {
		  frame->set_position(vfpos);
			outdated_video_frames.remove(frame);
		} else {
			remaining.push_back(vfcount);
		}
	}

	for (VideoFrames::iterator i = outdated_video_frames.begin(); i != outdated_video_frames.end(); ++i ) {
		VideoImageFrame *frame = (*i);
		if (remaining.empty()) {
		  frame->set_position(-2 * vtl_dist + leftmost_sample); /* move off screen */
		} else {
			int vfcount=remaining.front();
			remaining.pop_front();
			framepos_t vfpos = vtl_start + vfcount * vtl_dist; /* unit: audio-frames */
			framepos_t vframeno = rint ((vfpos - video_offset) / apv);  /* unit: video-frames */
			int rightend = -1; /* unit: pixels */
			if (vfpos + vtl_dist > video_start_offset + video_duration + video_offset) {
				rightend = display_vframe_width * (video_start_offset + video_duration + video_offset - vfpos) / vtl_dist;
				//printf("lf(n): %lu\n", vframeno); // XXX
			}
			frame->set_position(vfpos);
			frame->set_videoframe(vframeno, rightend);
		}
	}
}

std::string
VideoTimeLine::translated_filename ()
{
	if (!local_file){
		return video_filename;
	} else {
		return video_map_path(server_docroot, video_filename);
	}
}

bool
VideoTimeLine::video_file_info (std::string filename, bool local)
{

	local_file = local;
	if (filename.at(0) == G_DIR_SEPARATOR || !local_file) {
		video_filename = filename;
	}  else {
		video_filename = Glib::build_filename (_session->session_directory().video_path(), filename);
	}

	long long int _duration;
	double _start_offset;

	if (!video_query_info(
			video_server_url, translated_filename(),
			video_file_fps, _duration, _start_offset, video_aspect_ratio)) {
		warning << _("Parsing video file info failed. Is the Video Server running? Is the file readable by the Video Server? Does the docroot match? Is it a video file?") << endmsg;
		video_duration = 0;
		GuiUpdate("set-xjadeo-sensitive-off");
		GuiUpdate("video-unavailable");
		return false;
	}
	video_duration = _duration * _session->nominal_frame_rate() / video_file_fps;
	video_start_offset = _start_offset * _session->nominal_frame_rate();

	if (auto_set_session_fps && video_file_fps != _session->timecode_frames_per_second()) {
		switch ((int)floorf(video_file_fps*1000.0)) {
			case 23976:
				_session->config.set_timecode_format(timecode_23976);
				break;
			case 24000:
				_session->config.set_timecode_format(timecode_24);
				break;
			case 24975:
			case 24976:
				_session->config.set_timecode_format(timecode_24976);
				break;
			case 25000:
				_session->config.set_timecode_format(timecode_25);
				break;
			case 29970:
				_session->config.set_timecode_format(timecode_2997drop);
				break;
			case 30000:
				_session->config.set_timecode_format(timecode_30);
				break;
			case 59940:
				_session->config.set_timecode_format(timecode_5994);
				break;
			case 60000:
				_session->config.set_timecode_format(timecode_60);
				break;
			default:
				warning << string_compose (
						_("Failed to set session-framerate: '%1' does not have a corresponding option setting in %2."),
						video_file_fps, PROGRAM_NAME ) << endmsg;
				break;
		}
		_session->config.set_video_pullup(0); /* TODO only set if set_timecode_format() was successful ?!*/
	}
	if (floor(video_file_fps*100) != floor(_session->timecode_frames_per_second()*100)) {
		warning << string_compose(
				_("Video file's framerate is not equal to %1 session timecode's framerate: '%2' vs '%3'"),
					PROGRAM_NAME, video_file_fps, _session->timecode_frames_per_second())
				<< endmsg;
	}
	flush_local_cache ();

	if (found_xjadeo() && local_file) {
		GuiUpdate("set-xjadeo-sensitive-on");
		if (vmonitor && vmonitor->is_started()) {
#if 1
			/* xjadeo <= 0.6.4 has a bug where changing the video-file may segfauls
			 * if the geometry changes to a different line-size alignment
			 */
			reopen_vmonitor = true;
			vmonitor->quit();
#else
			vmonitor->set_fps(video_file_fps);
			vmonitor->open(video_filename);
#endif
		}
	} else if (!local_file) {
#if 1 /* temp debug/devel message */
		// TODO - call xjremote remotely.
		printf("the given video file can not be accessed on localhost, video monitoring is not currently supported for this case\n");
		GuiUpdate("set-xjadeo-sensitive-off");
#endif
	}
	VtlUpdate();
	GuiUpdate("video-available");
	return true;
}

bool
VideoTimeLine::check_server ()
{
	bool ok = false;
	char url[1024];
	snprintf(url, sizeof(url), "%s%sstatus"
			, video_server_url.c_str()
			, (video_server_url.length()>0 && video_server_url.at(video_server_url.length()-1) == '/')?"":"/"
			);
	char *res=a3_curl_http_get(url, NULL);
	if (res) {
		if (strstr(res, "status: ok, online.")) { ok = true; }
		free(res);
	}
	return ok;
}

bool
VideoTimeLine::check_server_docroot ()
{
	bool ok = true;
	char url[1024];
	std::vector<std::vector<std::string> > lines;

	if (video_server_url.find("/localhost:") == string::npos) {
		return true;
	}
	snprintf(url, sizeof(url), "%s%src?format=csv"
			, video_server_url.c_str()
			, (video_server_url.length()>0 && video_server_url.at(video_server_url.length()-1) == '/')?"":"/"
			);
	char *res=a3_curl_http_get(url, NULL);
	if (!res) {
		return false;
	}

	ParseCSV(std::string(res), lines);
	if (   lines.empty()
			|| lines.at(0).empty()
			|| lines.at(0).at(0) != video_get_docroot(Config)) {
		warning << string_compose(
				_("Video-server docroot mismatch. %1: '%2', video-server: '%3'. This usually means that the video server was not started by ardour and uses a different document-root."),
				PROGRAM_NAME, video_get_docroot(Config), lines.at(0).at(0))
		<< endmsg;
		ok = false; // TODO allow to override
	}
	free(res);
	return ok;
}

void
VideoTimeLine::gui_update(std::string const & t) {
	/* this is to be called via GuiUpdate() only. */
	ENSURE_GUI_THREAD (*this, &VideoTimeLine::queue_visual_videotimeline_update)
	if (t == "videotimeline-update") {
		editor->queue_visual_videotimeline_update();
	} else if (t == "set-xjadeo-active-off") {
		editor->toggle_xjadeo_proc(0);
	} else if (t == "set-xjadeo-active-on") {
		editor->toggle_xjadeo_proc(1);
	} else if (t == "set-xjadeo-sensitive-on") {
		editor->set_xjadeo_sensitive(true);
	} else if (t == "set-xjadeo-sensitive-off") {
		editor->toggle_xjadeo_proc(0);
		//close_video_monitor();
		editor->set_xjadeo_sensitive(false);
	} else if (t == "xjadeo-window-ontop-on") {
		editor->toggle_xjadeo_viewoption(1, 1);
	} else if (t == "xjadeo-window-ontop-off") {
		editor->toggle_xjadeo_viewoption(1, 0);
	} else if (t == "xjadeo-window-osd-timecode-on") {
		editor->toggle_xjadeo_viewoption(2, 1);
	} else if (t == "xjadeo-window-osd-timecode-off") {
		editor->toggle_xjadeo_viewoption(2, 0);
	} else if (t == "xjadeo-window-osd-frame-on") {
		editor->toggle_xjadeo_viewoption(3, 1);
	} else if (t == "xjadeo-window-osd-frame-off") {
		editor->toggle_xjadeo_viewoption(3, 0);
	} else if (t == "xjadeo-window-osd-box-on") {
		editor->toggle_xjadeo_viewoption(4, 1);
	} else if (t == "xjadeo-window-osd-box-off") {
		editor->toggle_xjadeo_viewoption(4, 0);
	} else if (t == "xjadeo-window-fullscreen-on") {
		editor->toggle_xjadeo_viewoption(5, 1);
	} else if (t == "xjadeo-window-fullscreen-off") {
		editor->toggle_xjadeo_viewoption(5, 0);
	} else if (t == "xjadeo-window-letterbox-on") {
		editor->toggle_xjadeo_viewoption(6, 1);
	} else if (t == "xjadeo-window-letterbox-off") {
		editor->toggle_xjadeo_viewoption(6, 0);
	} else if (t == "video-available") {
		editor->set_close_video_sensitive(true);
	} else if (t == "video-unavailable") {
		editor->set_close_video_sensitive(false);
	}
}

void
VideoTimeLine::set_height (int height) {
	if (_session && bar_height != height) {
		_session->set_dirty ();
	}
	bar_height = height;
	flush_local_cache();
}

void
VideoTimeLine::vmon_update () {
	if (vmonitor && vmonitor->is_started()) {
		vmonitor->set_offset(video_offset); // TODO proper re-init xjadeo w/o restart not just offset.
	}
}

void
VideoTimeLine::flush_local_cache () {
	flush_frames = true;
	vmon_update();
}

void
VideoTimeLine::flush_cache () {
	flush_local_cache();
	char url[1024];
	snprintf(url, sizeof(url), "%s%sadmin/flush_cache"
			, video_server_url.c_str()
			, (video_server_url.length()>0 && video_server_url.at(video_server_url.length()-1) == '/')?"":"/"
			);
	char *res=a3_curl_http_get(url, NULL);
	if (res) {
		free (res);
	}
	if (vmonitor && vmonitor->is_started()) {
		reopen_vmonitor=true;
		vmonitor->quit();
	}
	video_file_info(video_filename, local_file);
}

/* config */
void
VideoTimeLine::parameter_changed (std::string const & p)
{
	if (p == "video-server-url") {
		set_video_server_url (video_get_server_url(Config));
	} else if (p == "video-server-docroot") {
		set_video_server_docroot (video_get_docroot(Config));
	} else if (p == "video-advanced-setup") {
		set_video_server_url (video_get_server_url(Config));
		set_video_server_docroot (video_get_docroot(Config));
	}
	if (p == "use-video-file-fps" || p == "videotimeline-pullup" ) { /* session->config parameter */
		VtlUpdate();
	}
}

void
VideoTimeLine::set_video_server_url(std::string vsu) {
	flush_local_cache ();
	video_server_url = vsu;
	VtlUpdate();
}

void
VideoTimeLine::set_video_server_docroot(std::string vsr) {
	flush_local_cache ();
	server_docroot = vsr;
	VtlUpdate();
}

/* video-monitor for this timeline */
void
VideoTimeLine::find_xjadeo () {
	std::string xjadeo_file_path;
	if (getenv("XJREMOTE")) {
		_xjadeo_bin = strdup(getenv("XJREMOTE")); // XXX TODO: free it?!
	} else if (find_file_in_search_path (SearchPath(Glib::getenv("PATH")), X_("xjremote"), xjadeo_file_path)) {
		_xjadeo_bin = xjadeo_file_path;
	}
	else if (Glib::file_test(X_("/Applications/Jadeo.app/Contents/MacOS/xjremote"), Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_EXECUTABLE)) {
		_xjadeo_bin = X_("/Applications/Jadeo.app/Contents/MacOS/xjremote");
	}
	/* TODO: win32: allow to configure PATH to xjremote */
	else if (Glib::file_test(X_("C:\\Program Files\\xjadeo\\xjremote.exe"), Glib::FILE_TEST_EXISTS)) {
		_xjadeo_bin = X_("C:\\Program Files\\xjadeo\\xjremote.exe");
	}
	else if (Glib::file_test(X_("C:\\Program Files\\xjadeo\\xjremote.bat"), Glib::FILE_TEST_EXISTS)) {
		_xjadeo_bin = X_("C:\\Program Files\\xjadeo\\xjremote.bat");
	}
	else  {
		_xjadeo_bin = X_("");
		warning << _("Video-monitor 'xjadeo' was not found. Please install http://xjadeo.sf.net/ "
				"(a custom path to xjadeo can be specified by setting the XJREMOTE environment variable. "
				"It should point to an application compatible with xjadeo's remote-control interface 'xjremote').")
			<< endmsg;
	}
}

void
VideoTimeLine::open_video_monitor() {
	if (!found_xjadeo()) return;
	if (!vmonitor) {
		vmonitor = new VideoMonitor(editor, _xjadeo_bin);
		vmonitor->set_session(_session);
		vmonitor->set_offset(video_offset);
		vmonitor->Terminated.connect (sigc::mem_fun (*this, &VideoTimeLine::terminated_video_monitor));
		vmonitor->UiState.connect (*this, invalidator (*this), boost::bind (&VideoTimeLine::gui_update, this, _1), gui_context());
	} else if (vmonitor->is_started()) {
		return;
	}

#if 0
	/* unused for now.
	 * the idea is to selective ignore certain monitor window
	 * states if xjadeo is not running on the same host as ardour.
	 * However with the removal of the video-monitor-startup-dialogue
	 * (git rev 5a4d0fff0) these settings are currently not accessible.
	 */
	int xj_settings_mask = vmonitor->restore_settings_mask();
	if (_session) {
		/* load mask from Session */
		XMLNode* node = _session->extra_xml (X_("XJRestoreSettings"));
		if (node) {
			const XMLProperty* prop = node->property (X_("mask"));
			if (prop) {
				xj_settings_mask = atoi(prop->value());
			}
		}
	}

	vmonitor->restore_settings_mask(xj_settings_mask);
#endif

	if (!vmonitor->start()) {
		warning << "launching xjadeo failed.." << endmsg;
		close_video_monitor();
	} else {
		GuiUpdate("set-xjadeo-active-on");
		vmonitor->set_fps(video_file_fps);
		vmonitor->open(video_filename);

		if (_session) {
			XMLNode* node = _session->extra_xml (X_("Videomonitor"));
			if (node) {
				const XMLProperty* prop = node->property (X_("active"));
				if (prop && prop->value() != "yes") _session->set_dirty ();
			} else {
				_session->set_dirty ();
			}
		}

	}
}

void
VideoTimeLine::close_video_monitor() {
	if (vmonitor && vmonitor->is_started()) {
		vmonitor->quit();
	}
}

void
VideoTimeLine::control_video_monitor(int what, int param) {
	if (!vmonitor || !vmonitor->is_started()) {
		return;
	}
	vmonitor->send_cmd(what, param);
}


void
VideoTimeLine::terminated_video_monitor () {
	if (vmonitor) {
		vmonitor->save_session();
		delete vmonitor;
	}
	vmonitor=0;
	GuiUpdate("set-xjadeo-active-off");
	if (reopen_vmonitor) {
		reopen_vmonitor=false;
		open_video_monitor();
	} else {
		if (_session) {
			_session->set_dirty ();
		}
	}
}

void
VideoTimeLine::manual_seek_video_monitor (framepos_t pos)
{
	if (!vmonitor) { return; }
	if (!vmonitor->is_started()) { return; }
	if (!vmonitor->synced_by_manual_seeks()) { return; }
	vmonitor->manual_seek(pos, false, video_offset); // XXX -> set offset in xjadeo
}
