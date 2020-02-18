/*
 * Copyright (C) 2013-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2013-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
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
#include <cstdio>

#include "pbd/file_utils.h"
#include "pbd/convert.h"
#include "ardour/filesystem_paths.h"

#include "gui_thread.h"
#include "timers.h"
#include "utils.h"
#include "public_editor.h"
#include "editor.h"
#include "video_monitor.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR_UI_UTILS;

VideoMonitor::VideoMonitor (PublicEditor *ed, std::string xjadeo_bin_path)
	: editor (ed)
{
	manually_seeked_frame = 0;
	fps =0.0; // = _session->timecode_frames_per_second();
	sync_by_manual_seek = true;
	_restore_settings_mask = 0;
	clock_connection = sigc::connection();
	state_connection = sigc::connection();
	debug_enable = false;
	state_clk_divide = 0;
	starting = 0;
	osdmode = 10; // 1: frameno, 2: timecode, 8: box

	process = new ARDOUR::SystemExec(xjadeo_bin_path, X_("-R -J"));
	process->ReadStdout.connect_same_thread (*this, boost::bind (&VideoMonitor::parse_output, this, _1 ,_2));
	process->Terminated.connect (*this, invalidator (*this), boost::bind (&VideoMonitor::terminated, this), gui_context());
	XJKeyEvent.connect (*this, invalidator (*this), boost::bind (&VideoMonitor::forward_keyevent, this, _1), gui_context());
}

VideoMonitor::~VideoMonitor ()
{
	if (clock_connection.connected()) {
		clock_connection.disconnect();
	}
	if (state_connection.connected()) {
		state_connection.disconnect();
	}
	delete process;
}

bool
VideoMonitor::start ()
{
	if (is_started()) {
		return true;
	}

	manually_seeked_frame = 0;
	sync_by_manual_seek = false;
	if (clock_connection.connected()) { clock_connection.disconnect(); }

	if (process->start (debug_enable ? SystemExec::MergeWithStdin : SystemExec::IgnoreAndClose)) {
		return false;
	}
	return true;
}

void
VideoMonitor::query_full_state (bool wait)
{
	knownstate = 0;
	process->write_to_stdin("get windowsize\n");
	process->write_to_stdin("get windowpos\n");
	process->write_to_stdin("get letterbox\n");
	process->write_to_stdin("get fullscreen\n");
	process->write_to_stdin("get ontop\n");
	process->write_to_stdin("get offset\n");
	process->write_to_stdin("get osdcfg\n");
	int timeout = 40;
	if (wait && knownstate !=127 && --timeout) {
		Glib::usleep(50000);
		sched_yield();
	}
}

void
VideoMonitor::quit ()
{
	if (!is_started()) return;
	if (state_connection.connected()) { state_connection.disconnect(); }
	if (clock_connection.connected()) { clock_connection.disconnect(); }
	query_full_state(true);
	process->write_to_stdin("quit\n");
	/* the 'quit' command should result in process termination
	 * but in case it fails (communication failure, SIGSTOP, ??)
	 * here's a timeout..
	 */
	int timeout = 40;
	while (is_started() && --timeout) {
		Glib::usleep(50000);
		sched_yield();
	}
	if (timeout <= 0) {
		process->terminate();
	}
}

void
VideoMonitor::open (std::string filename)
{
	if (!is_started()) return;
	manually_seeked_frame = 0;
	osdmode = 10; // 1: frameno, 2: timecode, 8: box
	starting = 15;

	std::string ardour_mono_file;
	if (find_file (ARDOUR::ardour_data_search_path(), "ArdourMono.ttf", ardour_mono_file)) {
		process->write_to_stdin("osd font "+ ardour_mono_file  +"\n");
	}

	process->write_to_stdin("load " + filename + "\n");

	process->write_to_stdin("set fps -1\n");
	process->write_to_stdin("window resize 100%\n");
	process->write_to_stdin("window ontop on\n");
	process->write_to_stdin("set seekmode 1\n");
	/* override bitwise flags -- see xjadeo.h
	 * 0x0001 : ignore 'q', ESC  / quit
	 * 0x0002 : ignore "window closed by WM" / quit
	 * 0x0004 : (osx only) menu-exit / quit
	 * 0x0008 : ignore mouse-button 1 -- resize
	 * 0x0010 : no A/V offset control with keyboard
	 * 0x0020 : don't use jack-session
	 * 0x0040 : disable jack transport control
	 * 0x0080 : disallow sync source change (OSX menu)
	 * 0x0100 : disallow file open (OSX menu, X11 DnD)
	 */
	process->write_to_stdin("set override 504\n");
	process->write_to_stdin("notify keyboard\n");
	process->write_to_stdin("notify settings\n");
	process->write_to_stdin("window letterbox on\n");
	process->write_to_stdin("osd mode 10\n");

	for(XJSettings::const_iterator it = xjadeo_settings.begin(); it != xjadeo_settings.end(); ++it) {
		if (skip_setting(it->first)) { continue; }
		process->write_to_stdin(it->first + " " + it->second + "\n");
	}
	if (!state_connection.connected()) {
		starting = 15;
		querystate();
		state_clk_divide = 0;
		/* TODO once every two second or so -- state_clk_divide hack below */
		state_connection = Timers::rapid_connect (sigc::mem_fun (*this, &VideoMonitor::querystate));
	}
	sync_by_manual_seek = true;
	clock_connection = Timers::fps_connect (sigc::mem_fun (*this, &VideoMonitor::srsupdate));
	xjadeo_sync_setup();
}

void
VideoMonitor::querystate ()
{
	/* clock-divider hack -- RapidScreenUpdate == every_point_one_seconds */
	state_clk_divide = (state_clk_divide + 1) % 300; // 30 secs
	if (state_clk_divide == 0) {
		// every 30 seconds
		query_full_state(false);
		return;
	}
	if (state_clk_divide%25 != 0) {
		return;
	}
	// every 2.5 seconds:
	process->write_to_stdin("get fullscreen\n");
	process->write_to_stdin("get ontop\n");
	process->write_to_stdin("get osdcfg\n");
	process->write_to_stdin("get letterbox\n");
}

bool
VideoMonitor::skip_setting (std::string which)
{
	if (_restore_settings_mask & XJ_OSD && which == "osd mode") { return true; }
	if (_restore_settings_mask & XJ_LETTERBOX && which == "window letterbox") { return true; }
	if (_restore_settings_mask & XJ_WINDOW_SIZE && which == "window size") { return true; }
	if (_restore_settings_mask & XJ_WINDOW_POS && which == "window xy") { return true; }
	if (_restore_settings_mask & XJ_WINDOW_ONTOP && which == "window ontop") { return true; }
	if (_restore_settings_mask & XJ_LETTERBOX && which == "window letterbox") { return true; }
	if (_restore_settings_mask & XJ_OFFSET && which == "set offset") { return true; }
	if (_restore_settings_mask & XJ_FULLSCREEN && which == "window zoom") { return true; }
	return false;
}

void
VideoMonitor::send_cmd (int what, int param)
{
	bool osd_update = false;
	int prev_osdmode = osdmode;
	if (!is_started()) return;
	switch (what) {
		case 1:
			if (param) process->write_to_stdin("window ontop on\n");
			else process->write_to_stdin("window ontop off\n");
			break;
		case 2:
			if (param) osdmode |= 2;
			else osdmode &= ~2;
			osd_update = (prev_osdmode != osdmode);
			break;
		case 3:
			if (param) osdmode |= 1;
			else osdmode &= ~1;
			osd_update = (prev_osdmode != osdmode);
			break;
		case 4:
			if (param) osdmode |= 8;
			else osdmode &= ~8;
			osd_update = (prev_osdmode != osdmode);
			break;
		case 5:
			if (param) process->write_to_stdin("window zoom on\n");
			else process->write_to_stdin("window zoom off\n");
			break;
		case 6:
			if (param) process->write_to_stdin("window letterbox on\n");
			else process->write_to_stdin("window letterbox off\n");
			break;
		case 7:
			process->write_to_stdin("window resize 100%");
			break;
		default:
			break;
	}
	if (osd_update) {
		std::ostringstream osstream; osstream << "osd mode " << osdmode << "\n";
		process->write_to_stdin(osstream.str());
	}
}

bool
VideoMonitor::is_started ()
{
	return process->is_running();
}

void
VideoMonitor::forward_keyevent (unsigned int keyval)
{
	emulate_key_event (keyval);
}

void
VideoMonitor::parse_output (std::string d, size_t /*s*/)
{
	std::string line = d;
	std::string::size_type start = 0;
	std::string::size_type end = 0;

	while (1) {
		end = d.find('\n', start);
		if (end == std::string::npos) break;
		line = d.substr(start,end-start);
		start=end+1;
		if (line.length() <4 || line.at(0)!='@') continue;
#if 1 /* DEBUG */
		if (debug_enable) {
			printf("xjadeo: '%s'\n", line.c_str());
		}
#endif
		int status = atoi(line.substr(1,3));
		switch(status / 100) {
			case 4: /* errors */
				if (status == 403) {
					PBD::warning << _("Video Monitor: File Not Found.") << endmsg;
					/* check: we should only write from the main thread.
					 * However it should not matter for 'quit'.
					 */
					process->write_to_stdin("quit\n");
				}
#ifdef DEBUG_XJCOM
			else
				printf("xjadeo: error '%s'\n", line.c_str());
#endif
			break;
			case 3: /* async notifications */
			{
				std::string::size_type equalsign = line.find('=');
				std::string::size_type comment = line.find('#');
				if (comment != std::string::npos) { line = line.substr(0,comment); }
				if (equalsign != std::string::npos) {
					std::string key = line.substr(5, equalsign - 5);
					std::string value = line.substr(equalsign + 1);

					if (status == 310 && key=="keypress") {
						/* keyboard event */
						XJKeyEvent((unsigned int)atoi(value));
					}
#ifdef DEBUG_XJCOM
					else {
						std::string msg = line.substr(5);
						printf("xjadeo: async '%s' -> '%s'\n", key, value);
					}
#endif
				}
#ifdef DEBUG_XJCOM
				else {
					std::string msg = line.substr(5);
					printf("xjadeo: async '%s'\n", msg.c_str());
				}
#endif
			} break;
			case 1: /* text messages - command reply */
				break;
			case 8: /* comments / info for humans */
				break;
			case 2:
/* replies:
 * 201: var=<int>
 * 202: var=<double>
 * 210: var=<int>x<int>
 * 220: var=<string>
 * 228: var=<smpte-string>
 */
			{
				std::string::size_type equalsign = line.find('=');
				std::string::size_type comment = line.find('#');
				if (comment != std::string::npos) { line = line.substr(0,comment); }
				if (equalsign != std::string::npos) {
					std::string key = line.substr(5, equalsign - 5);
					std::string value = line.substr(equalsign + 1);
#if 0 /* DEBUG */
					std::cout << "parsed: " << key << " => " << value << std::endl;
#endif
					if       (key ==  "windowpos") {
						knownstate |= 16;
						if (xjadeo_settings["window xy"] != value) {
							if (!starting && _session) _session->set_dirty ();
						}
						xjadeo_settings["window xy"] = value;
					} else if(key ==  "windowsize") {
						knownstate |= 32;
						if (xjadeo_settings["window size"] != value) {
							if (!starting && _session) _session->set_dirty ();
						}
						xjadeo_settings["window size"] = value;
					} else if(key ==  "windowontop") {
						knownstate |= 2;
						if (starting || xjadeo_settings["window ontop"] != value) {
							if (!starting && _session) _session->set_dirty ();
							if (atoi(value)) { UiState("xjadeo-window-ontop-on"); }
							else { UiState("xjadeo-window-ontop-off"); }
							starting &= ~2;
						}
						xjadeo_settings["window ontop"] = value;
					} else if(key ==  "fullscreen") {
						knownstate |= 4;
						if (starting || xjadeo_settings["window zoom"] != value) {
							if (!starting && _session) _session->set_dirty ();
							if (atoi(value)) { UiState("xjadeo-window-fullscreen-on"); }
							else { UiState("xjadeo-window-fullscreen-off"); }
							starting &= ~4;
						}
						xjadeo_settings["window zoom"] = value;
					} else if(key ==  "letterbox") {
						knownstate |= 8;
						if (starting || xjadeo_settings["window letterbox"] != value) {
							if (!starting && _session) _session->set_dirty ();
							if (atoi(value)) { UiState("xjadeo-window-letterbox-on"); }
							else { UiState("xjadeo-window-letterbox-off"); }
							starting &= ~8;
						}
						xjadeo_settings["window letterbox"] = value;
					} else if(key ==  "osdmode") {
						knownstate |= 1;
						osdmode = atoi(value) & ~0x40;
						if (starting || atoi(xjadeo_settings["osd mode"]) != osdmode) {
							if (!starting && _session) _session->set_dirty ();
							if ((osdmode & 1) == 1) { UiState("xjadeo-window-osd-frame-on"); }
							if ((osdmode & 1) == 0) { UiState("xjadeo-window-osd-frame-off"); }
							if ((osdmode & 2) == 2) { UiState("xjadeo-window-osd-timecode-on"); }
							if ((osdmode & 2) == 0) { UiState("xjadeo-window-osd-timecode-off"); }
							if ((osdmode & 8) == 8) { UiState("xjadeo-window-osd-box-on"); }
							if ((osdmode & 8) == 0) { UiState("xjadeo-window-osd-box-off"); }
						}
						starting &= ~1;
						xjadeo_settings["osd mode"] = value;
					} else if(key ==  "offset") {
						knownstate |= 64;
						if (xjadeo_settings["set offset"] != value) {
							if (!starting && _session) _session->set_dirty ();
						}
						xjadeo_settings["set offset"] = value;
#ifdef DEBUG_XJCOM
					} else {
						printf("xjadeo: '%s' -> '%s'\n", key.c_str(), value.c_str());
#endif
					}
				}
			}
				break;
			default:
				break;
		}
	}
}

void
VideoMonitor::terminated ()
{
	process->terminate(); // from gui-context clean up
	Terminated();
}

void
VideoMonitor::save_session ()
{
	if (!_session) { return; }
	XMLNode* node = _session->extra_xml (X_("XJSettings"), true);
	if (!node) return;
	node->remove_nodes_and_delete("XJSetting");

	for(XJSettings::const_iterator it = xjadeo_settings.begin(); it != xjadeo_settings.end(); ++it) {
	  XMLNode* child = node->add_child (X_("XJSetting"));
		child->set_property (X_("k"), it->first);
		child->set_property (X_("v"), it->second);
	}
}


void
VideoMonitor::set_session (ARDOUR::Session *s)
{
	SessionHandlePtr::set_session (s);
	if (!_session) { return; }
	ARDOUR::Config->ParameterChanged.connect (*this, invalidator (*this), ui_bind (&VideoMonitor::parameter_changed, this, _1), gui_context());
	_session->config.ParameterChanged.connect (*this, invalidator (*this), ui_bind (&VideoMonitor::parameter_changed, this, _1), gui_context());
	XMLNode* node = _session->extra_xml (X_("XJSettings"));
	if (!node) { return;}
	xjadeo_settings.clear();

	XMLNodeList nlist = node->children();
	XMLNodeConstIterator niter;
	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		xjadeo_settings[(*niter)->property(X_("k"))->value()] = (*niter)->property(X_("v"))->value();
  }
}

bool
VideoMonitor::set_custom_setting (const std::string k, const std::string v)
{
	xjadeo_settings[k] = v;
	return true; /* TODO: check if key is valid */
}

const std::string
VideoMonitor::get_custom_setting (const std::string k)
{
	return (xjadeo_settings[k]);
}

#define NO_OFFSET (Temporal::max_samplepos) //< skip setting or modifying offset
void
VideoMonitor::srsupdate ()
{
	if (!_session) { return; }
	if (editor->dragging_playhead() || editor->preview_video_drag_active()) {
		return;
	}
	manual_seek(_session->audible_sample(), false, NO_OFFSET);
}

void
VideoMonitor::set_offset (ARDOUR::sampleoffset_t offset)
{
	if (!is_started()) { return; }
	if (!_session) { return; }
	if (offset == NO_OFFSET ) { return; }

	samplecnt_t video_frame_offset;
	samplecnt_t audio_sample_rate;
	if (_session->config.get_videotimeline_pullup()) {
		audio_sample_rate = _session->sample_rate();
	} else {
		audio_sample_rate = _session->nominal_sample_rate();
	}

	/* Note: pull-up/down are applied here: sample_rate() vs. nominal_sample_rate() */
	if (_session->config.get_use_video_file_fps()) {
		video_frame_offset = floor(offset * fps / audio_sample_rate);
	} else {
		video_frame_offset = floor(offset * _session->timecode_frames_per_second() / audio_sample_rate);
	}

	if (video_offset == video_frame_offset) { return; }
	video_offset = video_frame_offset;

	std::ostringstream osstream1; osstream1 << -1 * video_frame_offset;
	process->write_to_stdin("set offset " + osstream1.str() + "\n");
}

void
VideoMonitor::manual_seek (samplepos_t when, bool /*force*/, ARDOUR::sampleoffset_t offset)
{
	if (!is_started()) { return; }
	if (!_session) { return; }
	samplecnt_t video_frame;
	samplecnt_t audio_sample_rate;
	if (_session->config.get_videotimeline_pullup()) {
		audio_sample_rate = _session->sample_rate();
	} else {
		audio_sample_rate = _session->nominal_sample_rate();
	}

	/* Note: pull-up/down are applied here: sample_rate() vs. nominal_sample_rate() */
	if (_session->config.get_use_video_file_fps()) {
		video_frame = floor(when * fps / audio_sample_rate);
	} else {
		video_frame = floor(when * _session->timecode_frames_per_second() / audio_sample_rate);
	}
	if (video_frame < 0 ) video_frame = 0;

	if (video_frame == manually_seeked_frame) { return; }
	manually_seeked_frame = video_frame;

#if 0 /* DEBUG */
	std::cout <<"seek: " << video_frame << std::endl;
#endif
	std::ostringstream osstream; osstream << video_frame;
	process->write_to_stdin("seek " + osstream.str() + "\n");

	set_offset(offset);
}

void
VideoMonitor::parameter_changed (std::string const & p)
{
	if (!is_started()) { return; }
	if (!_session) { return; }
	if (p != "external-sync" && p != "sync-source") {
		return;
	}
	xjadeo_sync_setup();
}

void
VideoMonitor::xjadeo_sync_setup ()
{
	if (!is_started()) { return; }
	if (!_session) { return; }

	bool my_manual_seek = true;
	if (_session->synced_to_engine ()) {
		my_manual_seek = false;
	}

	if (my_manual_seek != sync_by_manual_seek) {
		if (sync_by_manual_seek) {
			if (clock_connection.connected()) {
				clock_connection.disconnect();
			}
			process->write_to_stdin("jack connect\n");
		} else {
			process->write_to_stdin("jack disconnect\n");
			clock_connection = Timers::fps_connect (sigc::mem_fun (*this, &VideoMonitor::srsupdate));
		}
		sync_by_manual_seek = my_manual_seek;
	}
}
