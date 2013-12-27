/*
    Copyright (C) 2006 Paul Davis 

    This program is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser
    General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your
    option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __ardour_basic_ui_h__
#define __ardour_basic_ui_h__

#include <string>
#include <stdint.h>

#include "pbd/signals.h"

#include "ardour/types.h"

#include <jack/types.h>

#include "timecode/time.h"

#include "control_protocol/visibility.h"

namespace ARDOUR {
	class Session;
	class SessionEvent;
}

class LIBCONTROLCP_API BasicUI {
  public:
	BasicUI (ARDOUR::Session&);
	virtual ~BasicUI ();
	
	void add_marker (const std::string& = std::string());

	void register_thread (std::string name);

	/* transport control */

	void loop_toggle ();
	void access_action ( std::string action_path );
	static PBD::Signal2<void,std::string,std::string> AccessAction;
	void goto_start ();
	void goto_end ();
	void rewind ();
	void ffwd ();
	void transport_stop ();
	void transport_play (bool jump_back = true);
	void set_transport_speed (double speed);
	double get_transport_speed ();

	ARDOUR::framepos_t transport_frame ();
	void locate (ARDOUR::framepos_t frame, bool play = false);
	bool locating ();
	bool locked ();

	void save_state ();
	void prev_marker ();
	void next_marker ();
	void undo ();
	void redo ();
	void toggle_punch_in ();
	void toggle_punch_out ();

	void set_record_enable (bool yn);
	bool get_record_enabled ();

	void rec_enable_toggle ();
	void toggle_all_rec_enables ();

	ARDOUR::framecnt_t timecode_frames_per_hour ();

	void timecode_time (framepos_t where, Timecode::Time&);
	void timecode_to_sample (Timecode::Time& timecode, framepos_t & sample, bool use_offset, bool use_subframes) const;
	void sample_to_timecode (framepos_t sample, Timecode::Time& timecode, bool use_offset, bool use_subframes) const;

  protected:
	BasicUI ();
	ARDOUR::Session* session;
};

#endif /* __ardour_basic_ui_h__ */
