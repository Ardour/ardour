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
#ifndef __ardour_video_monitor_h__
#define __ardour_video_monitor_h__

#include <string>

#include "ardour/ardour.h"
#include "ardour/types.h"
#include "ardour/session.h"
#include "ardour/session_handle.h"
#include "system_exec.h"

namespace ARDOUR {
	class Session;
}
class PublicEditor;

enum XJSettingOptions {
	XJ_WINDOW_SIZE = 1,
	XJ_WINDOW_POS = 2,
	XJ_WINDOW_ONTOP = 4,
	XJ_LETTERBOX = 8,
	XJ_OSD = 16,
	XJ_OFFSET = 32,
	XJ_FULLSCREEN = 64,
};

/** @class VideoMonitor
 *  @brief communication with xjadeo's remote-control interface
 */
class VideoMonitor : public sigc::trackable , public ARDOUR::SessionHandlePtr, public PBD::ScopedConnectionList
{
	public:
	VideoMonitor (PublicEditor*, std::string);
	virtual ~VideoMonitor ();

	void set_filename (std::string filename);
	void set_fps (float f) {fps = f;}
	bool is_started ();
	bool start ();
	void quit ();
	void open (std::string);

	void set_session (ARDOUR::Session *s);
	void save_session ();
	void clear_session_state ();
	void query_full_state (bool);
	bool set_custom_setting (const std::string, const std::string);
	const std::string get_custom_setting (const std::string);
	void restore_settings_mask (int i)  { _restore_settings_mask = i;}
	int restore_settings_mask () const { return _restore_settings_mask;}

	void set_offset (ARDOUR::frameoffset_t);
	void manual_seek (ARDOUR::framepos_t, bool, ARDOUR::frameoffset_t);
	void srsupdate ();
	void querystate ();
	bool synced_by_manual_seeks() { return sync_by_manual_seek; }

	sigc::signal<void> Terminated;
	PBD::Signal1<void,std::string> UiState;
	void send_cmd (int what, int param);

#if 1
	void set_debug (bool onoff) { debug_enable = onoff; }
#endif

	protected:
	PublicEditor *editor;
	SystemExec *process;
	float fps;
	void parse_output (std::string d, size_t s);
	void terminated ();
	void forward_keyevent (unsigned int);

	void parameter_changed (std::string const & p);

	typedef std::map<std::string,std::string> XJSettings;

	int _restore_settings_mask;
	bool skip_setting(std::string);
	XJSettings xjadeo_settings;

	void xjadeo_sync_setup ();
	ARDOUR::framepos_t manually_seeked_frame;
	bool sync_by_manual_seek;
	sigc::connection clock_connection;
	sigc::connection state_connection;
	int state_clk_divide;
	int starting;
	int knownstate;
	int osdmode;

	PBD::Signal1<void, unsigned int> XJKeyEvent;
#if 1
	bool debug_enable;
#endif
};

#endif /* __ardour_video_monitor_h__ */
