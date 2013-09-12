/*
    Copyright (C) 2004 Paul Barton-Davis

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

#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <iostream>

#include "midi++/types.h"
#include "midi++/parser.h"
#include "midi++/port.h"
#include "midi++/mmc.h"
#include "pbd/transmitter.h"

using namespace std;
using namespace sigc;
using namespace MIDI;

#undef DEBUG_MTC

bool
Parser::possible_mtc (MIDI::byte *sysex_buf, size_t msglen)
{
	byte fake_mtc_time[5];

	if (msglen != 10 || sysex_buf[0] != 0xf0 || sysex_buf[1] != 0x7f || sysex_buf[3] != 0x01 || sysex_buf[4] != 0x01) {
		return false;
	}

        /* full MTC */
	
	fake_mtc_time[0] = sysex_buf[8]; // frames
	fake_mtc_time[1] = sysex_buf[7]; // minutes
	fake_mtc_time[2] = sysex_buf[6]; // seconds
	fake_mtc_time[3] = (sysex_buf[5] & 0x1f); // hours
	
	_mtc_fps = MTC_FPS ((sysex_buf[5] & 0x60) >> 5); // fps
	fake_mtc_time[4] = (byte) _mtc_fps;

	/* wait for first quarter frame, which could indicate forwards
	   or backwards ...
	*/

	reset_mtc_state ();

	/* emit signals */

	mtc (*this, &sysex_buf[1], msglen - 1);
	mtc_time (fake_mtc_time, true, _timestamp);
#ifdef DEBUG_MTC
	cerr << "New full-MTC message marks state stopped" << endl;
#endif
	mtc_status (MTC_Stopped);

	return true;
}

void
Parser::reset_mtc_state ()
{
#ifdef DEBUG_MTC
	cerr << "MTC state reset" << endl;
#endif
	/* MUST REMAIN RT-SAFE */

	_mtc_forward = false;
	_mtc_running = MTC_Stopped;
	_mtc_locked = false;
	expected_mtc_quarter_frame_code = 0;
	memset (_mtc_time, 0, sizeof (_mtc_time));
	memset (_qtr_mtc_time, 0, sizeof (_mtc_time));
	consecutive_qtr_frame_cnt = 0;
	last_qtr_frame = 0;
}

void
Parser::process_mtc_quarter_frame (MIDI::byte *msg)
{
	int which_quarter_frame = (msg[1] & 0xf0) >> 4;

	/* Is it an expected frame?  
	   Remember, the first can be frame 7 or frame 0, 
	   depending on the direction of the MTC generator ...
	*/

#ifdef DEBUG_MTC
	 cerr << "MTC: (state = " << _mtc_running << ") " 
	      << which_quarter_frame << " vs. " << expected_mtc_quarter_frame_code
	      << " consecutive ? " << consecutive_qtr_frame_cnt
	      << endl;
#endif

	if (_mtc_running == MTC_Stopped) {
	
		/* we are stopped but are seeing qtr frame messages */

		if (consecutive_qtr_frame_cnt == 0) {

			/* first quarter frame */

			if (which_quarter_frame != 0 && which_quarter_frame != 7) {
				
				last_qtr_frame = which_quarter_frame;
				consecutive_qtr_frame_cnt++;
			}
			
			// cerr << "first seen qframe = " << (int) last_qtr_frame << endl;

			return;

		} else if (consecutive_qtr_frame_cnt == 1) {

			/* third quarter frame */
			
#ifdef DEBUG_MTC
			cerr << "second seen qframe = " << (int) which_quarter_frame << endl;
#endif
			if (last_qtr_frame < which_quarter_frame) {
				_mtc_running = MTC_Forward;
			} else if (last_qtr_frame > which_quarter_frame) {
				_mtc_running = MTC_Backward;
			}
#ifdef DEBUG_MTC
			cerr << "Send MTC status as " << _mtc_running << endl;
#endif
			mtc_status (_mtc_running);
		} 

		switch (_mtc_running) {
		case MTC_Forward:
			if (which_quarter_frame == 7) {
				expected_mtc_quarter_frame_code = 0;
			} else {
				expected_mtc_quarter_frame_code = which_quarter_frame + 1;
			}
			break;

		case MTC_Backward:
			if (which_quarter_frame == 0) {
				expected_mtc_quarter_frame_code = 7;
				
			} else {
				expected_mtc_quarter_frame_code = which_quarter_frame - 1;
			}
			break;

		case MTC_Stopped:
			break;
		}
		
	} else {
		
		/* already running */

// for testing bad MIDI connections etc.
//		if ((random() % 500) < 10) {

		if (which_quarter_frame != expected_mtc_quarter_frame_code) {

			consecutive_qtr_frame_cnt = 0;

#ifdef DEBUG_MTC
			cerr << "MTC: (state = " << _mtc_running << ") " 
			     << which_quarter_frame << " vs. " << expected_mtc_quarter_frame_code << endl;
#endif

			/* tell listener(s) that we skipped. if they return
			   true, just ignore this in terms of it being an error.
			*/

			boost::optional<bool> res = mtc_skipped ();
			
			if (res.get_value_or (false)) {

				/* no error, reset next expected frame */

				switch (_mtc_running) {
				case MTC_Forward:
					if (which_quarter_frame == 7) {
						expected_mtc_quarter_frame_code = 0;
					} else {
						expected_mtc_quarter_frame_code = which_quarter_frame + 1;
					}
					break;

				case MTC_Backward:
					if (which_quarter_frame == 0) {
						expected_mtc_quarter_frame_code = 7;
						
					} else {
						expected_mtc_quarter_frame_code = which_quarter_frame - 1;
					}
					break;

				case MTC_Stopped:
					break;
				}

#ifdef DEBUG_MTC
				cerr << "SKIPPED, next expected = " << expected_mtc_quarter_frame_code << endl;
#endif				
				return;
			}

			/* skip counts as an error ... go back to waiting for the first frame */

#ifdef DEBUG_MTC
			cerr << "Skipped MTC qtr frame, return to stopped state" << endl;
#endif
			reset_mtc_state ();
			mtc_status (MTC_Stopped);
			
			return;

		} else {

			/* received qtr frame matched expected */
			consecutive_qtr_frame_cnt++;

		}
	}

	/* time code is looking good */

#ifdef DEBUG_MTC
	cerr << "for quarter frame " << which_quarter_frame << " byte = " << hex << (int) msg[1] << dec << endl;
#endif

	switch (which_quarter_frame) {
	case 0: // frames LS nibble
		_qtr_mtc_time[0] |= msg[1] & 0xf;
		break;

	case 1:  // frames MS nibble
		_qtr_mtc_time[0] |= (msg[1] & 0xf)<<4;
		break;

	case 2: // seconds LS nibble
		_qtr_mtc_time[1] |= msg[1] & 0xf;
		break;

	case 3: // seconds MS nibble
		_qtr_mtc_time[1] |= (msg[1] & 0xf)<<4;
		break;

	case 4: // minutes LS nibble
		_qtr_mtc_time[2] |= msg[1] & 0xf;
		break;

	case 5: // minutes MS nibble
		_qtr_mtc_time[2] |= (msg[1] & 0xf)<<4;
		break;
		
	case 6: // hours LS nibble
		_qtr_mtc_time[3] |= msg[1] & 0xf;
		break;

	case 7: 
		
		/* last quarter frame msg has the MS bit of
		   the hour in bit 0, and the SMPTE FPS type
		   in bits 5 and 6 
		*/
		
		_qtr_mtc_time[3] |= ((msg[1] & 0x1) << 4);
		_mtc_fps = MTC_FPS ((msg[1] & 0x6) >> 1);
		_qtr_mtc_time[4] = _mtc_fps;
		break;

	default:
		/*NOTREACHED*/
		break;

	} 
	
#ifdef DEBUG_MTC
	cerr << "Emit MTC Qtr\n";
#endif

	mtc_qtr (*this, which_quarter_frame, _timestamp); /* EMIT_SIGNAL */

	// mtc (*this, &msg[1], msglen - 1);

	switch (_mtc_running) {
	case MTC_Forward:
		if (which_quarter_frame == 7) {
			
			/* we've reached the final of 8 quarter frame messages.
			   store the time, reset the pending time holder,
			   and signal anyone who wants to know the time.
			*/
			
			if (consecutive_qtr_frame_cnt >= 8) {
				memcpy (_mtc_time, _qtr_mtc_time, sizeof (_mtc_time));
				memset (_qtr_mtc_time, 0, sizeof (_qtr_mtc_time));
				if (!_mtc_locked) {
					_mtc_locked = true;
				}

				mtc_time (_mtc_time, false, _timestamp);
			}
			expected_mtc_quarter_frame_code = 0;
			
		} else {
			expected_mtc_quarter_frame_code = which_quarter_frame + 1;
		}
		break;
		
	case MTC_Backward:
		if (which_quarter_frame == 0) {
			
			/* we've reached the final of 8 quarter frame messages.
			   store the time, reset the pending time holder,
			   and signal anyone who wants to know the time.
			*/

			if (consecutive_qtr_frame_cnt >= 8) {
				memcpy (_mtc_time, _qtr_mtc_time, sizeof (_mtc_time));
				memset (_qtr_mtc_time, 0, sizeof (_qtr_mtc_time));
				if (!_mtc_locked) {
					_mtc_locked = true;
				}
				mtc_time (_mtc_time, false, _timestamp);
			}

			expected_mtc_quarter_frame_code = 7;

		} else {
			expected_mtc_quarter_frame_code = which_quarter_frame - 1;
		}
		break;

	default:
		break;
	}

}
