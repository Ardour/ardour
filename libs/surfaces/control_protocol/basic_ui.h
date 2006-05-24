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

    $Id$
*/

#ifndef __ardour_basic_ui_h__
#define __ardour_basic_ui_h__

#include <string>

#include <jack/types.h>

namespace ARDOUR {
	class Session;
}

class BasicUI {
  public:
	BasicUI (ARDOUR::Session&);
	virtual ~BasicUI ();
	
	void add_marker ();

	void register_thread (std::string name);

	/* transport control */

	void loop_toggle ();
	void goto_start ();
	void goto_end ();
	void rewind ();
	void ffwd ();
	void transport_stop ();
	void transport_play (bool jump_back = true);
	void set_transport_speed (float speed);
	float get_transport_speed ();

	jack_nframes_t transport_frame ();
	void locate (jack_nframes_t frame, bool play = false);
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

	jack_nframes_t smpte_frames_per_hour ();

	struct SMPTE_t {
		bool negative;
		uint32_t hours;
		uint32_t minutes;
		uint32_t seconds;
		uint32_t frames;
		uint32_t subframes; // mostly not used

		SMPTE_t () {
			negative = false;
			hours = 0;
			minutes = 0;
			seconds = 0;
			frames = 0;
			subframes = 0;
		}
	};

	void smpte_time (jack_nframes_t where, SMPTE_t&);
	void smpte_to_sample (SMPTE_t& smpte, jack_nframes_t& sample, bool use_offset, bool use_subframes) const;
	void sample_to_smpte (jack_nframes_t sample, SMPTE_t& smpte, bool use_offset, bool use_subframes) const;

  protected:
	BasicUI ();
	ARDOUR::Session* session;
};

#endif /* __ardour_basic_ui_h__ */
