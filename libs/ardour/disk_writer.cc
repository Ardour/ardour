/*
    Copyright (C) 2009-2016 Paul Davis

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

#include "pbd/i18n.h"

#include "ardour/debug.h"
#include "ardour/disk_writer.h"
#include "ardour/session.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

ARDOUR::framecnt_t DiskWriter::_chunk_frames = DiskWriter::default_chunk_frames ();

DiskWriter::DiskWriter (Session& s, string const & str, DiskIOProcessor::Flag f)
	: DiskIOProcessor (s, str, f)
        , capture_start_frame (0)
        , capture_captured (0)
        , was_recording (false)
        , adjust_capture_position (0)
        , _capture_offset (0)
        , first_recordable_frame (max_framepos)
        , last_recordable_frame (max_framepos)
        , last_possibly_recording (0)
        , _alignment_style (ExistingMaterial)
        , _alignment_choice (Automatic)
{
}

framecnt_t
DiskWriter::default_chunk_frames ()
{
	return 65536;
}

bool
DiskWriter::set_write_source_name (string const & str)
{
	_write_source_name = str;
	return true;
}

void
DiskWriter::check_record_status (framepos_t transport_frame, bool can_record)
{
	int possibly_recording;
	int rolling;
	int change;
	const int transport_rolling = 0x4;
	const int track_rec_enabled = 0x2;
	const int global_rec_enabled = 0x1;
	const int fully_rec_enabled = (transport_rolling|track_rec_enabled|global_rec_enabled);

	/* merge together the 3 factors that affect record status, and compute
	 * what has changed.
	 */

	rolling = _session.transport_speed() != 0.0f;
	possibly_recording = (rolling << 2) | ((int)record_enabled() << 1) | (int)can_record;
	change = possibly_recording ^ last_possibly_recording;

	if (possibly_recording == last_possibly_recording) {
		return;
	}

	const framecnt_t existing_material_offset = _session.worst_playback_latency();

	if (possibly_recording == fully_rec_enabled) {

		if (last_possibly_recording == fully_rec_enabled) {
			return;
		}

		capture_start_frame = _session.transport_frame();
		first_recordable_frame = capture_start_frame + _capture_offset;
		last_recordable_frame = max_framepos;

                DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1: @ %7 (%9) FRF = %2 CSF = %4 CO = %5, EMO = %6 RD = %8 WOL %10 WTL %11\n",
                                                                      name(), first_recordable_frame, last_recordable_frame, capture_start_frame,
                                                                      _capture_offset,
                                                                      existing_material_offset,
                                                                      transport_frame,
                                                                      _session.transport_frame(),
                                                                      _session.worst_output_latency(),
                                                                      _session.worst_track_latency()));


                if (_alignment_style == ExistingMaterial) {
                        first_recordable_frame += existing_material_offset;
                        DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("\tshift FRF by EMO %1\n",
                                                                              first_recordable_frame));
                }

		prepare_record_status (capture_start_frame);

	} else {

		if (last_possibly_recording == fully_rec_enabled) {

			/* we were recording last time */

			if (change & transport_rolling) {

				/* transport-change (stopped rolling): last_recordable_frame was set in ::prepare_to_stop(). We
				 * had to set it there because we likely rolled past the stopping point to declick out,
				 * and then backed up.
				 */

			} else {
				/* punch out */

				last_recordable_frame = _session.transport_frame() + _capture_offset;

				if (_alignment_style == ExistingMaterial) {
					last_recordable_frame += existing_material_offset;
				}
			}
		}
	}

	last_possibly_recording = possibly_recording;
}

void
DiskWriter::calculate_record_range (Evoral::OverlapType ot, framepos_t transport_frame, framecnt_t nframes,
				    framecnt_t & rec_nframes, framecnt_t & rec_offset)
{
	switch (ot) {
	case Evoral::OverlapNone:
		rec_nframes = 0;
		break;

	case Evoral::OverlapInternal:
		/*     ----------    recrange
		 *       |---|       transrange
		 */
		rec_nframes = nframes;
		rec_offset = 0;
		break;

	case Evoral::OverlapStart:
		/*    |--------|    recrange
		 *  -----|          transrange
		 */
		rec_nframes = transport_frame + nframes - first_recordable_frame;
		if (rec_nframes) {
			rec_offset = first_recordable_frame - transport_frame;
		}
		break;

	case Evoral::OverlapEnd:
		/*    |--------|    recrange
		 *       |--------  transrange
		 */
		rec_nframes = last_recordable_frame - transport_frame;
		rec_offset = 0;
		break;

	case Evoral::OverlapExternal:
		/*    |--------|    recrange
		 *  --------------  transrange
		 */
		rec_nframes = last_recordable_frame - first_recordable_frame;
		rec_offset = first_recordable_frame - transport_frame;
		break;
	}

        DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1 rec? %2 @ %3 (for %4) FRF %5 LRF %6 : rf %7 @ %8\n",
                                                              _name, enum_2_string (ot), transport_frame, nframes,
                                                              first_recordable_frame, last_recordable_frame, rec_nframes, rec_offset));
}

void
DiskWriter::prepare_to_stop (framepos_t transport_frame, framepos_t audible_frame)
{
	switch (_alignment_style) {
	case ExistingMaterial:
		last_recordable_frame = transport_frame + _capture_offset;
		DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose("%1: prepare to stop sets last recordable frame to %2 + %3 = %4\n", _name, transport_frame, _capture_offset, last_recordable_frame));
		break;

	case CaptureTime:
		last_recordable_frame = audible_frame; // note that capture_offset is zero
		/* we may already have captured audio before the last_recordable_frame (audible frame),
		   so deal with this.
		*/
		if (last_recordable_frame > capture_start_frame) {
			capture_captured = min (capture_captured, last_recordable_frame - capture_start_frame);
		}
		DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose("%1: prepare to stop sets last recordable frame to audible frame @ %2\n", _name, audible_frame));
		break;
	}

}

void
DiskWriter::engage_record_enable ()
{
	g_atomic_int_set (&_record_enabled, 1);
}

void
DiskWriter::disengage_record_enable ()
{
	g_atomic_int_set (&_record_enabled, 0);
}

void
DiskWriter::engage_record_safe ()
{
	g_atomic_int_set (&_record_safe, 1);
}

void
DiskWriter::disengage_record_safe ()
{
	g_atomic_int_set (&_record_safe, 0);
}

/** Get the start position (in session frames) of the nth capture in the current pass */
ARDOUR::framepos_t
DiskWriter::get_capture_start_frame (uint32_t n) const
{
	Glib::Threads::Mutex::Lock lm (capture_info_lock);

	if (capture_info.size() > n) {
		/* this is a completed capture */
		return capture_info[n]->start;
	} else {
		/* this is the currently in-progress capture */
		return capture_start_frame;
	}
}

ARDOUR::framecnt_t
DiskWriter::get_captured_frames (uint32_t n) const
{
	Glib::Threads::Mutex::Lock lm (capture_info_lock);

	if (capture_info.size() > n) {
		/* this is a completed capture */
		return capture_info[n]->frames;
	} else {
		/* this is the currently in-progress capture */
		return capture_captured;
	}
}

void
DiskWriter::set_input_latency (framecnt_t l)
{
	_input_latency = l;
}

void
DiskWriter::set_capture_offset ()
{
	switch (_alignment_style) {
	case ExistingMaterial:
		_capture_offset = _input_latency;
		break;

	case CaptureTime:
	default:
		_capture_offset = 0;
		break;
	}

        DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1: using IO latency, capture offset set to %2 with style = %3\n", name(), _capture_offset, enum_2_string (_alignment_style)));
}


void
DiskWriter::set_align_style (AlignStyle a, bool force)
{
	if (record_enabled() && _session.actively_recording()) {
		return;
	}

	if ((a != _alignment_style) || force) {
		_alignment_style = a;
		set_capture_offset ();
		AlignmentStyleChanged ();
	}
}

void
DiskWriter::set_align_choice (AlignChoice a, bool force)
{
	if (record_enabled() && _session.actively_recording()) {
		return;
	}

	if ((a != _alignment_choice) || force) {
		_alignment_choice = a;

		switch (_alignment_choice) {
			case Automatic:
				set_align_style_from_io ();
				break;
			case UseExistingMaterial:
				set_align_style (ExistingMaterial);
				break;
			case UseCaptureTime:
				set_align_style (CaptureTime);
				break;
		}
	}
}

int
DiskWriter::set_state (const XMLNode& node, int version)
{
	XMLProperty const * prop;

	if (DiskIOProcessor::set_state (node, version)) {
		return -1;
	}

	if ((prop = node.property (X_("capture-alignment"))) != 0) {
                set_align_choice (AlignChoice (string_2_enum (prop->value(), _alignment_choice)), true);
        } else {
                set_align_choice (Automatic, true);
        }


	if ((prop = node.property ("record-safe")) != 0) {
	    _record_safe = PBD::string_is_affirmative (prop->value()) ? 1 : 0;
	}

	return 0;
}
