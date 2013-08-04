/*
    Copyright (C) 2000-2006 Paul Davis

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

#include <fstream>
#include <cassert>
#include <cstdio>
#include <unistd.h>
#include <cmath>
#include <cerrno>
#include <string>
#include <climits>
#include <fcntl.h>
#include <cstdlib>
#include <ctime>
#include <sys/stat.h>

#include <glibmm/threads.h>

#include "pbd/error.h"
#include "pbd/basename.h"
#include "pbd/memento_command.h"
#include "pbd/xml++.h"
#include "pbd/stacktrace.h"

#include "ardour/debug.h"
#include "ardour/diskstream.h"
#include "ardour/io.h"
#include "ardour/pannable.h"
#include "ardour/playlist.h"
#include "ardour/session.h"
#include "ardour/track.h"

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

/* XXX This goes uninitialized when there is no ~/.config/ardour3 directory.
 * I can't figure out why, so this will do for now (just stole the
 * default from configuration_vars.h).  0 is not a good value for
 * allocating buffer sizes..
 */
ARDOUR::framecnt_t Diskstream::disk_io_chunk_frames = 1024 * 256 / sizeof (Sample);

PBD::Signal0<void>                Diskstream::DiskOverrun;
PBD::Signal0<void>                Diskstream::DiskUnderrun;

Diskstream::Diskstream (Session &sess, const string &name, Flag flag)
	: SessionObject(sess, name)
        , i_am_the_modifier (0)
        , _track (0)
        , _record_enabled (0)
        , _visible_speed (1.0f)
        , _actual_speed (1.0f)
        , _buffer_reallocation_required (false)
        , _seek_required (false)
        , capture_start_frame (0)
        , capture_captured (0)
        , was_recording (false)
        , adjust_capture_position (0)
        , _capture_offset (0)
        , _roll_delay (0)
        , first_recordable_frame (max_framepos)
        , last_recordable_frame (max_framepos)
        , last_possibly_recording (0)
        , _alignment_style (ExistingMaterial)
        , _alignment_choice (Automatic)
        , _slaved (false)
        , loop_location (0)
        , overwrite_frame (0)
        , overwrite_offset (0)
        , _pending_overwrite (false)
        , overwrite_queued (false)
        , wrap_buffer_size (0)
        , speed_buffer_size (0)
        , _speed (1.0)
        , _target_speed (_speed)
        , file_frame (0)
        , playback_sample (0)
        , in_set_state (false)
        , _flags (flag)
        , deprecated_io_node (0)
{
}

Diskstream::Diskstream (Session& sess, const XMLNode& /*node*/)
	: SessionObject(sess, "unnamed diskstream")
        , i_am_the_modifier (0)
        , _track (0)
        , _record_enabled (0)
        , _visible_speed (1.0f)
        , _actual_speed (1.0f)
        , _buffer_reallocation_required (false)
        , _seek_required (false)
        , capture_start_frame (0)
        , capture_captured (0)
        , was_recording (false)
        , adjust_capture_position (0)
        , _capture_offset (0)
        , _roll_delay (0)
        , first_recordable_frame (max_framepos)
        , last_recordable_frame (max_framepos)
        , last_possibly_recording (0)
        , _alignment_style (ExistingMaterial)
        , _alignment_choice (Automatic)
        , _slaved (false)
        , loop_location (0)
        , overwrite_frame (0)
        , overwrite_offset (0)
        , _pending_overwrite (false)
        , overwrite_queued (false)
        , wrap_buffer_size (0)
        , speed_buffer_size (0)
        , _speed (1.0)
        , _target_speed (_speed)
        , file_frame (0)
        , playback_sample (0)
        , in_set_state (false)
        , _flags (Recordable)
        , deprecated_io_node (0)
{
}

Diskstream::~Diskstream ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("Diskstream %1 deleted\n", _name));

	if (_playlist) {
		_playlist->release ();
	}

        delete deprecated_io_node;
}

void
Diskstream::set_track (Track* t)
{
	_track = t;
	_io = _track->input();

	ic_connection.disconnect();
	_io->changed.connect_same_thread (ic_connection, boost::bind (&Diskstream::handle_input_change, this, _1, _2));

        if (_io->n_ports() != ChanCount::ZERO) {
                input_change_pending.type = IOChange::Type (IOChange::ConfigurationChanged|IOChange::ConnectionsChanged);
                non_realtime_input_change ();
        }

	_track->Destroyed.connect_same_thread (*this, boost::bind (&Diskstream::route_going_away, this));
}

void
Diskstream::handle_input_change (IOChange change, void * /*src*/)
{
	Glib::Threads::Mutex::Lock lm (state_lock);

        if (change.type & (IOChange::ConfigurationChanged|IOChange::ConnectionsChanged)) {

                /* rather than handle this here on a DS-by-DS basis we defer to the
                   session transport/butler thread, and let it tackle
                   as many diskstreams as need it in one shot. this avoids many repeated
                   takings of the audioengine process lock.
                */

                if (!(input_change_pending.type & change.type)) {
                        input_change_pending.type = IOChange::Type (input_change_pending.type | change.type);
                        _session.request_input_change_handling ();
                }
        }
}

void
Diskstream::non_realtime_set_speed ()
{
	if (_buffer_reallocation_required)
	{
		Glib::Threads::Mutex::Lock lm (state_lock);
		allocate_temporary_buffers ();

		_buffer_reallocation_required = false;
	}

	if (_seek_required) {
		if (speed() != 1.0f || speed() != -1.0f) {
			seek ((framepos_t) (_session.transport_frame() * (double) speed()), true);
		}
		else {
			seek (_session.transport_frame(), true);
		}

		_seek_required = false;
	}
}

bool
Diskstream::realtime_set_speed (double sp, bool global)
{
	bool changed = false;
	double new_speed = sp * _session.transport_speed();

	if (_visible_speed != sp) {
		_visible_speed = sp;
		changed = true;
	}

	if (new_speed != _actual_speed) {

		framecnt_t required_wrap_size = (framecnt_t) ceil (_session.get_block_size() *
                                                                  fabs (new_speed)) + 2;

		if (required_wrap_size > wrap_buffer_size) {
			_buffer_reallocation_required = true;
		}

		_actual_speed = new_speed;
		_target_speed = fabs(_actual_speed);
	}

	if (changed) {
		if (!global) {
			_seek_required = true;
		}
		SpeedChanged (); /* EMIT SIGNAL */
	}

	return _buffer_reallocation_required || _seek_required;
}

void
Diskstream::set_capture_offset ()
{
	if (_io == 0) {
		/* can't capture, so forget it */
		return;
	}

	_capture_offset = _io->latency();
        DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1: using IO latency, capture offset set to %2\n", name(), _capture_offset));
}


void
Diskstream::set_align_style (AlignStyle a, bool force)
{
	if (record_enabled() && _session.actively_recording()) {
		return;
	}

	if ((a != _alignment_style) || force) {
		_alignment_style = a;
		AlignmentStyleChanged ();
	}
}

void
Diskstream::set_align_choice (AlignChoice a, bool force)
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
Diskstream::set_loop (Location *location)
{
	if (location) {
		if (location->start() >= location->end()) {
			error << string_compose(_("Location \"%1\" not valid for track loop (start >= end)"), location->name()) << endl;
			return -1;
		}
	}

	loop_location = location;

	LoopSet (location); /* EMIT SIGNAL */
	return 0;
}

/** Get the start position (in session frames) of the nth capture in the current pass */
ARDOUR::framepos_t
Diskstream::get_capture_start_frame (uint32_t n) const
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
Diskstream::get_captured_frames (uint32_t n) const
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
Diskstream::set_roll_delay (ARDOUR::framecnt_t nframes)
{
	_roll_delay = nframes;
}

int
Diskstream::use_playlist (boost::shared_ptr<Playlist> playlist)
{
        if (!playlist) {
                return 0;
        }

        bool prior_playlist = false;

	{
		Glib::Threads::Mutex::Lock lm (state_lock);

		if (playlist == _playlist) {
			return 0;
		}

		playlist_connections.drop_connections ();

		if (_playlist) {
			_playlist->release();
                        prior_playlist = true;
		}

		_playlist = playlist;
		_playlist->use();

		if (!in_set_state && recordable()) {
			reset_write_sources (false);
		}

		_playlist->ContentsChanged.connect_same_thread (playlist_connections, boost::bind (&Diskstream::playlist_modified, this));
		_playlist->DropReferences.connect_same_thread (playlist_connections, boost::bind (&Diskstream::playlist_deleted, this, boost::weak_ptr<Playlist>(_playlist)));
		_playlist->RangesMoved.connect_same_thread (playlist_connections, boost::bind (&Diskstream::playlist_ranges_moved, this, _1, _2));
	}

	/* don't do this if we've already asked for it *or* if we are setting up
	   the diskstream for the very first time - the input changed handling will
	   take care of the buffer refill.
	*/

	if (!overwrite_queued && prior_playlist) {
		_session.request_overwrite_buffer (_track);
		overwrite_queued = true;
	}

	PlaylistChanged (); /* EMIT SIGNAL */
	_session.set_dirty ();

	return 0;
}

void
Diskstream::playlist_changed (const PropertyChange&)
{
	playlist_modified ();
}

void
Diskstream::playlist_modified ()
{
	if (!i_am_the_modifier && !overwrite_queued) {
		_session.request_overwrite_buffer (_track);
		overwrite_queued = true;
	}
}

void
Diskstream::playlist_deleted (boost::weak_ptr<Playlist> wpl)
{
	boost::shared_ptr<Playlist> pl (wpl.lock());

	if (pl == _playlist) {

		/* this catches an ordering issue with session destruction. playlists
		   are destroyed before diskstreams. we have to invalidate any handles
		   we have to the playlist.
		*/

		if (_playlist) {
			_playlist.reset ();
		}
	}
}

bool
Diskstream::set_name (const string& str)
{
	if (_name != str) {
		assert(playlist());
		playlist()->set_name (str);
		SessionObject::set_name(str);
	}
        return true;
}

XMLNode&
Diskstream::get_state ()
{
	XMLNode* node = new XMLNode ("Diskstream");
        char buf[64];
	LocaleGuard lg (X_("POSIX"));

	node->add_property ("flags", enum_2_string (_flags));
	node->add_property ("playlist", _playlist->name());
	node->add_property("name", _name);
	id().print (buf, sizeof (buf));
	node->add_property("id", buf);
	snprintf (buf, sizeof(buf), "%f", _visible_speed);
	node->add_property ("speed", buf);
        node->add_property ("capture-alignment", enum_2_string (_alignment_choice));

	if (_extra_xml) {
		node->add_child_copy (*_extra_xml);
	}

        return *node;
}

int
Diskstream::set_state (const XMLNode& node, int /*version*/)
{
	const XMLProperty* prop;

	if ((prop = node.property ("name")) != 0) {
		_name = prop->value();
	}

	if (deprecated_io_node) {
		set_id (*deprecated_io_node);
	} else {
		set_id (node);
	}

	if ((prop = node.property ("flags")) != 0) {
		_flags = Flag (string_2_enum (prop->value(), _flags));
	}

        if ((prop = node.property (X_("capture-alignment"))) != 0) {
                set_align_choice (AlignChoice (string_2_enum (prop->value(), _alignment_choice)), true);
        } else {
                set_align_choice (Automatic, true);
        }

	if ((prop = node.property ("playlist")) == 0) {
		return -1;
	}

	if (find_and_use_playlist (prop->value())) {
		return -1;
	}

	if ((prop = node.property ("speed")) != 0) {
		double sp = atof (prop->value().c_str());

		if (realtime_set_speed (sp, false)) {
			non_realtime_set_speed ();
		}
	}

        return 0;
}

void
Diskstream::playlist_ranges_moved (list< Evoral::RangeMove<framepos_t> > const & movements_frames, bool from_undo)
{
	/* If we're coming from an undo, it will have handled
	   automation undo (it must, since automation-follows-regions
	   can lose automation data).  Hence we can do nothing here.
	*/

	if (from_undo) {
		return;
	}

	if (!_track || Config->get_automation_follows_regions () == false) {
		return;
	}

	list< Evoral::RangeMove<double> > movements;

	for (list< Evoral::RangeMove<framepos_t> >::const_iterator i = movements_frames.begin();
	     i != movements_frames.end();
	     ++i) {

		movements.push_back(Evoral::RangeMove<double>(i->from, i->length, i->to));
	}

	/* move panner automation */
	boost::shared_ptr<Pannable> pannable = _track->pannable();
        Evoral::ControlSet::Controls& c (pannable->controls());

        for (Evoral::ControlSet::Controls::iterator ci = c.begin(); ci != c.end(); ++ci) {
                boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl>(ci->second);
                if (!ac) {
                        continue;
                }
                boost::shared_ptr<AutomationList> alist = ac->alist();

                XMLNode & before = alist->get_state ();
                bool const things_moved = alist->move_ranges (movements);
                if (things_moved) {
                        _session.add_command (new MementoCommand<AutomationList> (
                                                      *alist.get(), &before, &alist->get_state ()));
                }
        }

	/* move processor automation */
	_track->foreach_processor (boost::bind (&Diskstream::move_processor_automation, this, _1, movements_frames));
}

void
Diskstream::move_processor_automation (boost::weak_ptr<Processor> p, list< Evoral::RangeMove<framepos_t> > const & movements_frames)
{
	boost::shared_ptr<Processor> processor (p.lock ());
	if (!processor) {
		return;
	}

	list< Evoral::RangeMove<double> > movements;
	for (list< Evoral::RangeMove<framepos_t> >::const_iterator i = movements_frames.begin(); i != movements_frames.end(); ++i) {
		movements.push_back(Evoral::RangeMove<double>(i->from, i->length, i->to));
	}

	set<Evoral::Parameter> const a = processor->what_can_be_automated ();

	for (set<Evoral::Parameter>::const_iterator i = a.begin (); i != a.end (); ++i) {
		boost::shared_ptr<AutomationList> al = processor->automation_control(*i)->alist();
		XMLNode & before = al->get_state ();
		bool const things_moved = al->move_ranges (movements);
		if (things_moved) {
			_session.add_command (
				new MementoCommand<AutomationList> (
					*al.get(), &before, &al->get_state ()
					)
				);
		}
	}
}

void
Diskstream::check_record_status (framepos_t transport_frame, bool can_record)
{
	int possibly_recording;
	int rolling;
	int change;
	const int transport_rolling = 0x4;
	const int track_rec_enabled = 0x2;
	const int global_rec_enabled = 0x1;
        const int fully_rec_enabled = (transport_rolling|track_rec_enabled|global_rec_enabled);

	/* merge together the 3 factors that affect record status, and compute
	   what has changed.
	*/

	rolling = _session.transport_speed() != 0.0f;
	possibly_recording = (rolling << 2) | (record_enabled() << 1) | can_record;
	change = possibly_recording ^ last_possibly_recording;

	if (possibly_recording == last_possibly_recording) {
		return;
	}

        framecnt_t existing_material_offset = _session.worst_playback_latency();

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
                                                                      _roll_delay,
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
                                   had to set it there because we likely rolled past the stopping point to declick out,
                                   and then backed up.
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
Diskstream::route_going_away ()
{
	_io.reset ();
}

void
Diskstream::calculate_record_range (Evoral::OverlapType ot, framepos_t transport_frame, framecnt_t nframes,
				    framecnt_t & rec_nframes, framecnt_t & rec_offset)
{
	switch (ot) {
	case Evoral::OverlapNone:
		rec_nframes = 0;
		break;

	case Evoral::OverlapInternal:
		/*     ----------    recrange
		         |---|       transrange
		*/
		rec_nframes = nframes;
		rec_offset = 0;
		break;

	case Evoral::OverlapStart:
		/*    |--------|    recrange
	        -----|          transrange
		*/
		rec_nframes = transport_frame + nframes - first_recordable_frame;
		if (rec_nframes) {
			rec_offset = first_recordable_frame - transport_frame;
		}
		break;

	case Evoral::OverlapEnd:
		/*    |--------|    recrange
		         |--------  transrange
		*/
		rec_nframes = last_recordable_frame - transport_frame;
		rec_offset = 0;
		break;

	case Evoral::OverlapExternal:
		/*    |--------|    recrange
		    --------------  transrange
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
Diskstream::prepare_to_stop (framepos_t pos)
{
        last_recordable_frame = pos + _capture_offset;
}

void
Diskstream::engage_record_enable ()
{
	g_atomic_int_set (&_record_enabled, 1);
}

void
Diskstream::disengage_record_enable ()
{
	g_atomic_int_set (&_record_enabled, 0);
}

