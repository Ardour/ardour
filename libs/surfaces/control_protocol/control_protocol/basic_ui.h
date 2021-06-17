/*
 * Copyright (C) 2006-2010 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef __ardour_basic_ui_h__
#define __ardour_basic_ui_h__

#include "ardour/presentation_info.h"
#include "ardour/session_controller.h"
#include "ardour/types.h"
#include "control_protocol/visibility.h"
#include "pbd/signals.h"
#include "temporal/time.h"

#include <cstdint>
#include <string>

namespace ARDOUR {

class Session;

class LIBCONTROLCP_API BasicUI {
  public:
	BasicUI (Session&);
	virtual ~BasicUI ();

	void register_thread (std::string name);

	/* Access to GUI actions */

	void access_action (std::string action_path);

	static PBD::Signal2<void,std::string,std::string> AccessAction;

	/* Undo/Redo */

	void undo ();
	void redo ();

	/* Convenience methods that fire actions */

	void mark_in();
	void mark_out();

	void quick_snapshot_stay ();
	void quick_snapshot_switch ();

	void set_punch_range();
	void set_loop_range();
	void set_session_range();

	/* Editor Visibility

	   We need to explicitly bake in the "arguments" here, because GUI actions
	   don't have arguments.
	*/

	void fit_1_track();
	void fit_2_tracks();
	void fit_4_tracks();
	void fit_8_tracks();
	void fit_16_tracks();
	void fit_32_tracks();
	void fit_all_tracks();
	void zoom_10_ms();
	void zoom_100_ms();
	void zoom_1_sec();
	void zoom_10_sec();
	void zoom_1_min();
	void zoom_5_min();
	void zoom_10_min();
	void zoom_to_session();
	void temporal_zoom_in();
	void temporal_zoom_out();

	void scroll_up_1_track();
	void scroll_dn_1_track();
	void scroll_up_1_page();
	void scroll_dn_1_page();

	/* Button state */

	bool stop_button_onoff() const;
	bool play_button_onoff() const;
	bool ffwd_button_onoff() const;
	bool rewind_button_onoff() const;
	bool loop_button_onoff() const;

	SessionController& controller() { return _controller; }

  protected:
	Session* _session;
	SessionController _controller;
};

} // namespace ARDOUR

#endif /* __ardour_basic_ui_h__ */
