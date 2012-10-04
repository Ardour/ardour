
/*
  Copyright (C) 1999-2002 Paul Davis 

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

#include <string>
#include <cmath>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <midi++/mmc.h>
#include <midi++/port.h>
#include <midi++/manager.h>
#include <pbd/error.h>
#include <glibmm/thread.h>
#include <pbd/pthread_utils.h>

#include <ardour/configuration.h>
#include <ardour/audioengine.h>
#include <ardour/session.h>
#include <ardour/audio_track.h>
#include <ardour/audio_diskstream.h>
#include <ardour/slave.h>
#include <ardour/cycles.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace MIDI;

MachineControl::CommandSignature MMC_CommandSignature;
MachineControl::ResponseSignature MMC_ResponseSignature;

MultiAllocSingleReleasePool Session::MIDIRequest::pool ("midi", sizeof (Session::MIDIRequest), 1024);

int
Session::use_config_midi_ports ()
{
	string port_name;

	if (default_mmc_port) {
		set_mmc_port (default_mmc_port->name());
	} else {
		set_mmc_port ("");
	}

	if (default_mtc_port) {
		set_mtc_port (default_mtc_port->name());
	} else {
		set_mtc_port ("");
	}

	if (default_midi_port) {
		set_midi_port (default_midi_port->name());
	} else {
		set_midi_port ("");
	}

	return 0;
}	


/***********************************************************************
 MTC, MMC, etc.
**********************************************************************/

int
Session::set_mtc_port (string port_tag)
{
	MTC_Slave *ms;

	if (port_tag.length() == 0) {

		if (_slave && ((ms = dynamic_cast<MTC_Slave*> (_slave)) != 0)) {
			error << string_compose (_("%1 is slaved to MTC - port cannot be reset"), PROGRAM_NAME) << endmsg;
			return -1;
		}

		if (_mtc_port == 0) {
			return 0;
		}

		_mtc_port = 0;
		goto out;
	}

	MIDI::Port* port;

	if ((port = MIDI::Manager::instance()->port (port_tag)) == 0) {
		error << string_compose (_("unknown port %1 requested for MTC"), port_tag) << endl;
		return -1;
	}

	_mtc_port = port;

	if (_slave && ((ms = dynamic_cast<MTC_Slave*> (_slave)) != 0)) {
		ms->rebind (*port);
	}

	Config->set_mtc_port_name (port_tag);

  out:
	MTC_PortChanged(); /* EMIT SIGNAL */
	change_midi_ports ();
	set_dirty();
	return 0;
}

void
Session::set_mmc_receive_device_id (uint32_t device_id)
{
	if (mmc) {
		mmc->set_receive_device_id (device_id);
	}
}

void
Session::set_mmc_send_device_id (uint32_t device_id)
{
	if (mmc) {
		mmc->set_send_device_id (device_id);
		/* reset MMC buffer */
		mmc_buffer[2] = mmc->send_device_id();
	}
}

int
Session::set_mmc_port (string port_tag)
{
	MIDI::byte old_recv_device_id = 0;
	MIDI::byte old_send_device_id = 0;
	bool reset_id = false;

	if (port_tag.length() == 0) {
		if (_mmc_port == 0) {
			return 0;
		}
		_mmc_port = 0;
		goto out;
	}

	MIDI::Port* port;

	if ((port = MIDI::Manager::instance()->port (port_tag)) == 0) {
		return -1;
	}

	_mmc_port = port;

	if (mmc) {
		old_recv_device_id = mmc->receive_device_id();
		old_recv_device_id = mmc->send_device_id();
		reset_id = true;
		delete mmc;
	}

	mmc = new MIDI::MachineControl (*_mmc_port, 1.0, 
					MMC_CommandSignature,
					MMC_ResponseSignature);

	if (reset_id) {
		set_mmc_receive_device_id (old_recv_device_id);
		set_mmc_send_device_id (old_send_device_id);
	}

	mmc->Play.connect 
		(mem_fun (*this, &Session::mmc_deferred_play));
	mmc->DeferredPlay.connect 
		(mem_fun (*this, &Session::mmc_deferred_play));
	mmc->Stop.connect 
		(mem_fun (*this, &Session::mmc_stop));
	mmc->FastForward.connect 
		(mem_fun (*this, &Session::mmc_fast_forward));
	mmc->Rewind.connect 
		(mem_fun (*this, &Session::mmc_rewind));
	mmc->Pause.connect 
		(mem_fun (*this, &Session::mmc_pause));
	mmc->RecordPause.connect 
		(mem_fun (*this, &Session::mmc_record_pause));
	mmc->RecordStrobe.connect 
		(mem_fun (*this, &Session::mmc_record_strobe));
	mmc->RecordExit.connect 
		(mem_fun (*this, &Session::mmc_record_exit));
	mmc->Locate.connect 
		(mem_fun (*this, &Session::mmc_locate));
	mmc->Step.connect 
		(mem_fun (*this, &Session::mmc_step));
	mmc->Shuttle.connect 
		(mem_fun (*this, &Session::mmc_shuttle));
	mmc->TrackRecordStatusChange.connect
		(mem_fun (*this, &Session::mmc_record_enable));


	/* also handle MIDI SPP because its so common */

	_mmc_port->input()->start.connect (mem_fun (*this, &Session::spp_start));
	_mmc_port->input()->contineu.connect (mem_fun (*this, &Session::spp_continue));
	_mmc_port->input()->stop.connect (mem_fun (*this, &Session::spp_stop));
	
	Config->set_mmc_port_name (port_tag);

  out:
	MMC_PortChanged(); /* EMIT SIGNAL */
	change_midi_ports ();
	set_dirty();
	return 0;
}

int
Session::set_midi_port (string port_tag)
{
	if (port_tag.length() == 0) {
		if (_midi_port == 0) {
			return 0;
		}
		_midi_port = 0;
		goto out;
	}

	MIDI::Port* port;

	if ((port = MIDI::Manager::instance()->port (port_tag)) == 0) {
		return -1;
	}

	_midi_port = port;
	
	/* XXX need something to forward this to control protocols ? or just
	   use the signal below 
	*/

	Config->set_midi_port_name (port_tag);

  out:
	MIDI_PortChanged(); /* EMIT SIGNAL */
	change_midi_ports ();
	set_dirty();
	return 0;
}

void
Session::set_trace_midi_input (bool yn, MIDI::Port* port)
{
	MIDI::Parser* input_parser;

	if (port) {
		if ((input_parser = port->input()) != 0) {
			input_parser->trace (yn, &cout, "input: ");
		}
	} else {

		if (_mmc_port) {
			if ((input_parser = _mmc_port->input()) != 0) {
				input_parser->trace (yn, &cout, "input: ");
			}
		}
		
		if (_mtc_port && _mtc_port != _mmc_port) {
			if ((input_parser = _mtc_port->input()) != 0) {
				input_parser->trace (yn, &cout, "input: ");
			}
		}

		if (_midi_port && _midi_port != _mmc_port && _midi_port != _mtc_port  ) {
			if ((input_parser = _midi_port->input()) != 0) {
				input_parser->trace (yn, &cout, "input: ");
			}
		}
	}

	Config->set_trace_midi_input (yn);
}

void
Session::set_trace_midi_output (bool yn, MIDI::Port* port)
{
	MIDI::Parser* output_parser;

	if (port) {
		if ((output_parser = port->output()) != 0) {
			output_parser->trace (yn, &cout, "output: ");
		}
	} else {
		if (_mmc_port) {
			if ((output_parser = _mmc_port->output()) != 0) {
				output_parser->trace (yn, &cout, "output: ");
			}
		}
		
		if (_mtc_port && _mtc_port != _mmc_port) {
			if ((output_parser = _mtc_port->output()) != 0) {
				output_parser->trace (yn, &cout, "output: ");
			}
		}

		if (_midi_port && _midi_port != _mmc_port && _midi_port != _mtc_port  ) {
			if ((output_parser = _midi_port->output()) != 0) {
				output_parser->trace (yn, &cout, "output: ");
			}
		}

	}

	Config->set_trace_midi_output (yn);
}

bool
Session::get_trace_midi_input(MIDI::Port *port)
{
	MIDI::Parser* input_parser;
	if (port) {
		if ((input_parser = port->input()) != 0) {
			return input_parser->tracing();
		}
	}
	else {
		if (_mmc_port) {
			if ((input_parser = _mmc_port->input()) != 0) {
				return input_parser->tracing();
			}
		}
		
		if (_mtc_port) {
			if ((input_parser = _mtc_port->input()) != 0) {
				return input_parser->tracing();
			}
		}

		if (_midi_port) {
			if ((input_parser = _midi_port->input()) != 0) {
				return input_parser->tracing();
			}
		}
	}

	return false;
}

bool
Session::get_trace_midi_output(MIDI::Port *port)
{
	MIDI::Parser* output_parser;
	if (port) {
		if ((output_parser = port->output()) != 0) {
			return output_parser->tracing();
		}
	}
	else {
		if (_mmc_port) {
			if ((output_parser = _mmc_port->output()) != 0) {
				return output_parser->tracing();
			}
		}
		
		if (_mtc_port) {
			if ((output_parser = _mtc_port->output()) != 0) {
				return output_parser->tracing();
			}
		}

		if (_midi_port) {
			if ((output_parser = _midi_port->output()) != 0) {
				return output_parser->tracing();
			}
		}
	}

	return false;

}

void
Session::setup_midi_control ()
{
	outbound_mtc_smpte_frame = 0;
	next_quarter_frame_to_send = -1;

	/* setup the MMC buffer */
	
	mmc_buffer[0] = 0xf0; // SysEx
	mmc_buffer[1] = 0x7f; // Real Time SysEx ID for MMC
	mmc_buffer[2] = (mmc ? mmc->send_device_id() : 0x7f);
	mmc_buffer[3] = 0x6;  // MCC

	/* Set up the qtr frame message */
	
	mtc_msg[0] = 0xf1;
	mtc_msg[2] = 0xf1;
	mtc_msg[4] = 0xf1;
	mtc_msg[6] = 0xf1;
	mtc_msg[8] = 0xf1;
	mtc_msg[10] = 0xf1;
	mtc_msg[12] = 0xf1;
	mtc_msg[14] = 0xf1;
}

int
Session::midi_read (MIDI::Port* port)
{
	MIDI::byte buf[512];
	
	/* reading from the MIDI port activates the Parser
	   that in turn generates signals that we care
	   about. the port is already set to NONBLOCK so that
	   can read freely here.
	*/
	
	while (1) {
		
		// cerr << "+++ READ ON " << port->name() << endl;
		
		int nread = port->read (buf, sizeof (buf));

		// cerr << "-- READ (" << nread << " ON " << port->name() << endl;
		
		if (nread > 0) {
			if ((size_t) nread < sizeof (buf)) {
				break;
			} else {
				continue;
			}
		} else if (nread == 0) {
			break;
		} else if (errno == EAGAIN) {
			break;
		} else {
			fatal << string_compose(_("Error reading from MIDI port %1"), port->name()) << endmsg;
			/*NOTREACHED*/
		}
	}

	return 0;
}

void
Session::spp_start (Parser& ignored)
{
	if (Config->get_mmc_control() && (Config->get_slave_source() != MTC)) {
		request_transport_speed (1.0);
	}
}

void
Session::spp_continue (Parser& ignored)
{
	spp_start (ignored);
}

void
Session::spp_stop (Parser& ignored)
{
	if (Config->get_mmc_control()) {
		request_stop ();
	}
}

void
Session::mmc_deferred_play (MIDI::MachineControl &mmc)
{
	if (Config->get_mmc_control() && (Config->get_slave_source() != MTC)) {
		request_transport_speed (1.0);
	}
}

void
Session::mmc_record_pause (MIDI::MachineControl &mmc)
{
	if (Config->get_mmc_control()) {
		maybe_enable_record();
	}
}

void
Session::mmc_record_strobe (MIDI::MachineControl &mmc)
{
	if (!Config->get_mmc_control()) 
		return;

	/* record strobe does an implicit "Play" command */

	if (_transport_speed != 1.0) {

		/* start_transport() will move from Enabled->Recording, so we
		   don't need to do anything here except enable recording.
		   its not the same as maybe_enable_record() though, because
		   that *can* switch to Recording, which we do not want.
		*/
		
		save_state ("", true);
		g_atomic_int_set (&_record_status, Enabled);
		RecordStateChanged (); /* EMIT SIGNAL */
		
		request_transport_speed (1.0);

	} else {

		enable_record ();
	}
}

void
Session::mmc_record_exit (MIDI::MachineControl &mmc)
{
	if (Config->get_mmc_control()) {
		disable_record (false);
	}
}

void
Session::mmc_stop (MIDI::MachineControl &mmc)
{
	if (Config->get_mmc_control()) {
		request_stop ();
	}
}

void
Session::mmc_pause (MIDI::MachineControl &mmc)
{
	if (Config->get_mmc_control()) {

		/* We support RECORD_PAUSE, so the spec says that
		   we must interpret PAUSE like RECORD_PAUSE if
		   recording.
		*/

		if (actively_recording()) {
			maybe_enable_record ();
		} else {
			request_stop ();
		}
	}
}

static bool step_queued = false;

void
Session::mmc_step (MIDI::MachineControl &mmc, int steps)
{
	if (!Config->get_mmc_control()) {
		return;
	}

	struct timeval now;
	struct timeval diff = { 0, 0 };

	gettimeofday (&now, 0);
	
	timersub (&now, &last_mmc_step, &diff);

	gettimeofday (&now, 0);
	timersub (&now, &last_mmc_step, &diff);

	if (last_mmc_step.tv_sec != 0 && (diff.tv_usec + (diff.tv_sec * 1000000)) < _engine.usecs_per_cycle()) {
		return;
	}
	
	double diff_secs = diff.tv_sec + (diff.tv_usec / 1000000.0);
	double cur_speed = (((steps * 0.5) * smpte_frames_per_second()) / diff_secs) / smpte_frames_per_second();
	
	if (_transport_speed == 0 || cur_speed * _transport_speed < 0) {
		/* change direction */
		step_speed = cur_speed;
	} else {
		step_speed = (0.6 * step_speed) + (0.4 * cur_speed);
	}

	step_speed *= 0.25;

#if 0
	cerr << "delta = " << diff_secs 
	     << " ct = " << _transport_speed
	     << " steps = " << steps
	     << " new speed = " << cur_speed 
	     << " speed = " << step_speed
	     << endl;
#endif	

	request_transport_speed (step_speed);
	last_mmc_step = now;

	if (!step_queued) {
		midi_timeouts.push_back (mem_fun (*this, &Session::mmc_step_timeout));
		step_queued = true;
	}
}

void
Session::mmc_rewind (MIDI::MachineControl &mmc)
{
	if (Config->get_mmc_control()) {
		request_transport_speed(-8.0f);
	}
}

void
Session::mmc_fast_forward (MIDI::MachineControl &mmc)
{
	if (Config->get_mmc_control()) {
		request_transport_speed(8.0f);
	}
}

void
Session::mmc_locate (MIDI::MachineControl &mmc, const MIDI::byte* mmc_tc)
{
	if (!Config->get_mmc_control()) {
		return;
	}

	nframes_t target_frame;
	SMPTE::Time smpte;

	smpte.hours = mmc_tc[0] & 0xf;
	smpte.minutes = mmc_tc[1];
	smpte.seconds = mmc_tc[2];
	smpte.frames = mmc_tc[3];
	smpte.rate = smpte_frames_per_second();
	smpte.drop = smpte_drop_frames();
  
	// Also takes smpte offset into account:
	smpte_to_sample( smpte, target_frame, true /* use_offset */, false /* use_subframes */ );
	
	if (target_frame > max_frames) {
		target_frame = max_frames;
	}

	/* Some (all?) MTC/MMC devices do not send a full MTC frame
	   at the end of a locate, instead sending only an MMC
	   locate command. This causes the current position
	   of an MTC slave to become out of date. Catch this.
	*/

	MTC_Slave* mtcs = dynamic_cast<MTC_Slave*> (_slave);

	if (mtcs != 0) {
		// cerr << "Locate *with* MTC slave\n";
		mtcs->handle_locate (mmc_tc);
	} else {
		// cerr << "Locate without MTC slave\n";
		request_locate (target_frame, false);
	}
}

void
Session::mmc_shuttle (MIDI::MachineControl &mmc, float speed, bool forw)
{
	if (!Config->get_mmc_control()) {
		return;
	}

	if (Config->get_shuttle_speed_threshold() >= 0 && speed > Config->get_shuttle_speed_threshold()) {
		speed *= Config->get_shuttle_speed_factor();
	}

	if (forw) {
		request_transport_speed (speed);
	} else {
		request_transport_speed (-speed);
	}
}

void
Session::mmc_record_enable (MIDI::MachineControl &mmc, size_t trk, bool enabled)
{
	if (Config->get_mmc_control()) {

		RouteList::iterator i;
		boost::shared_ptr<RouteList> r = routes.reader();
		
		for (i = r->begin(); i != r->end(); ++i) {
			AudioTrack *at;

			if ((at = dynamic_cast<AudioTrack*>((*i).get())) != 0) {
				if (trk == at->remote_control_id()) {
					at->set_record_enable (enabled, &mmc);
					break;
				}
			}
		}
	}
}

void
Session::send_full_time_code_in_another_thread ()
{
	send_time_code_in_another_thread (true);
}

void
Session::send_midi_time_code_in_another_thread ()
{
	send_time_code_in_another_thread (false);
}

void
Session::send_time_code_in_another_thread (bool full)
{
	nframes_t two_smpte_frames_duration;
	nframes_t quarter_frame_duration;

	/* Duration of two smpte frames */
	two_smpte_frames_duration = ((long) _frames_per_smpte_frame) << 1;

	/* Duration of one quarter frame */
	quarter_frame_duration = ((long) _frames_per_smpte_frame) >> 2;

	if (_transport_frame < (outbound_mtc_smpte_frame + (next_quarter_frame_to_send * quarter_frame_duration)))
	{
		/* There is no work to do.
		   We throttle this here so that we don't overload
		   the transport thread with requests.
		*/
		return;
	}

	MIDIRequest* request = new MIDIRequest;

	if (full) {
		request->type = MIDIRequest::SendFullMTC;
	} else {
		request->type = MIDIRequest::SendMTC;
	}
	
	midi_requests.write (&request, 1);
	poke_midi_thread ();
}

void
Session::change_midi_ports ()
{
	MIDIRequest* request = new MIDIRequest;

	request->type = MIDIRequest::PortChange;
	midi_requests.write (&request, 1);
	poke_midi_thread ();
}

int
Session::send_full_time_code ()

{
	MIDI::byte msg[10];
	SMPTE::Time smpte;

	_send_smpte_update = false;

	if (_mtc_port == 0 || !session_send_mtc) {
		return 0;
	}

	// Get smpte time for this transport frame
	sample_to_smpte(_transport_frame, smpte, true /* use_offset */, false /* no subframes */);

	// Check for negative smpte time and prepare for quarter frame transmission
	if (smpte.negative) {
		// Negative mtc is not defined, so sync slave to smpte zero.
		// When _transport_frame gets there we will start transmitting quarter frames
		smpte.hours = 0;
		smpte.minutes = 0;
		smpte.seconds = 0;
		smpte.frames = 0;
		smpte.subframes = 0;
		smpte.negative = false;
		smpte_to_sample( smpte, outbound_mtc_smpte_frame, true, false );
		transmitting_smpte_time = smpte;
	} else {
		transmitting_smpte_time = smpte;
		outbound_mtc_smpte_frame = _transport_frame;
		if (((mtc_smpte_bits >> 5) != MIDI::MTC_25_FPS) && (transmitting_smpte_time.frames % 2)) {
			// start MTC quarter frame transmission on an even frame
			SMPTE::increment( transmitting_smpte_time );
			outbound_mtc_smpte_frame += (nframes_t) _frames_per_smpte_frame;
		}
	}

	// Compensate for audio latency
	outbound_mtc_smpte_frame += _worst_output_latency;

	next_quarter_frame_to_send = 0;

	// Sync slave to the same smpte time as we are on (except if negative, see above)
	msg[0] = 0xf0;
	msg[1] = 0x7f;
	msg[2] = 0x7f;
	msg[3] = 0x1;
	msg[4] = 0x1;
	msg[9] = 0xf7;

	msg[5] = mtc_smpte_bits | smpte.hours;
	msg[6] = smpte.minutes;
	msg[7] = smpte.seconds;
	msg[8] = smpte.frames;

	{
		Glib::Mutex::Lock lm (midi_lock);
    
		if (_mtc_port->midimsg (msg, sizeof (msg))) {
			error << _("Session: could not send full MIDI time code") << endmsg;
			
			return -1;
		}
	}

	return 0;
}

int
Session::send_midi_time_code ()
{
	if (_mtc_port == 0 || !session_send_mtc || transmitting_smpte_time.negative || (next_quarter_frame_to_send < 0) )  {
		return 0;
	}

	/* Duration of one quarter frame */
	nframes_t const quarter_frame_duration = ((long) _frames_per_smpte_frame) >> 2;

	while (_transport_frame >= (outbound_mtc_smpte_frame + (next_quarter_frame_to_send * quarter_frame_duration))) {

		// Send quarter frames up to current time
		{
			Glib::Mutex::Lock lm (midi_lock);

			switch(next_quarter_frame_to_send) {
			case 0:
				mtc_msg[1] =  0x00 | (transmitting_smpte_time.frames & 0xf);
				break;
			case 1:
				mtc_msg[1] =  0x10 | ((transmitting_smpte_time.frames & 0xf0) >> 4);
				break;
			case 2:
				mtc_msg[1] =  0x20 | (transmitting_smpte_time.seconds & 0xf);
				break;
			case 3:
				mtc_msg[1] =  0x30 | ((transmitting_smpte_time.seconds & 0xf0) >> 4);
				break;
			case 4:
				mtc_msg[1] =  0x40 | (transmitting_smpte_time.minutes & 0xf);
				break;
			case 5:
				mtc_msg[1] = 0x50 | ((transmitting_smpte_time.minutes & 0xf0) >> 4);
				break;
			case 6:
				mtc_msg[1] = 0x60 | ((mtc_smpte_bits|transmitting_smpte_time.hours) & 0xf);
				break;
			case 7:
				mtc_msg[1] = 0x70 | (((mtc_smpte_bits|transmitting_smpte_time.hours) & 0xf0) >> 4);
				break;
			}			
			
			if (_mtc_port->midimsg (mtc_msg, 2)) {
				error << string_compose(_("Session: cannot send quarter-frame MTC message (%1)"), strerror (errno)) 
				      << endmsg;
				
				return -1;
			}

			//       cout << "smpte = " << transmitting_smpte_time.hours << ":" << transmitting_smpte_time.minutes << ":" << transmitting_smpte_time.seconds << ":" << transmitting_smpte_time.frames << ", qfm = " << next_quarter_frame_to_send << endl;

			// Increment quarter frame counter
			next_quarter_frame_to_send++;
      
			if (next_quarter_frame_to_send >= 8) {
				// Wrap quarter frame counter
				next_quarter_frame_to_send = 0;
				// Increment smpte time twice
				SMPTE::increment( transmitting_smpte_time );
				SMPTE::increment( transmitting_smpte_time );        
				// Re-calculate timing of first quarter frame
				smpte_to_sample( transmitting_smpte_time, outbound_mtc_smpte_frame, true /* use_offset */, false );
				// Compensate for audio latency
				outbound_mtc_smpte_frame += _worst_output_latency;
			}
		}
	}
	return 0;
}

/***********************************************************************
 OUTBOUND MMC STUFF
**********************************************************************/

void
Session::send_mmc_in_another_thread (MIDI::MachineControl::Command cmd, nframes_t target_frame)
{
	MIDIRequest* request;

	if (_mtc_port == 0 || !session_send_mmc) {
		return;
	}

	request = new MIDIRequest;
	request->type = MIDIRequest::SendMMC;
	request->mmc_cmd = cmd;
	request->locate_frame = target_frame;

	midi_requests.write (&request, 1);
	poke_midi_thread ();
}

void
Session::deliver_mmc (MIDI::MachineControl::Command cmd, nframes_t where)
{
	using namespace MIDI;
	int nbytes = 4;
	SMPTE::Time smpte;

	if (_mmc_port == 0 || !session_send_mmc) {
		return;
	}

	mmc_buffer[nbytes++] = cmd;

	// cerr << "delivering MMC, cmd = " << hex << (int) cmd << dec << endl;
	
	switch (cmd) {
	case MachineControl::cmdLocate:
		smpte_time_subframes (where, smpte);

		mmc_buffer[nbytes++] = 0x6; // byte count
		mmc_buffer[nbytes++] = 0x1; // "TARGET" subcommand
		mmc_buffer[nbytes++] = smpte.hours;
		mmc_buffer[nbytes++] = smpte.minutes;
		mmc_buffer[nbytes++] = smpte.seconds;
		mmc_buffer[nbytes++] = smpte.frames;
		mmc_buffer[nbytes++] = smpte.subframes;
		break;

	case MachineControl::cmdStop:
		break;

	case MachineControl::cmdPlay:
		/* always convert Play into Deferred Play */
		mmc_buffer[4] = MachineControl::cmdDeferredPlay;
		break;

	case MachineControl::cmdDeferredPlay:
		break;

	case MachineControl::cmdRecordStrobe:
		break;

	case MachineControl::cmdRecordExit:
		break;

	case MachineControl::cmdRecordPause:
		break;

	default:
		nbytes = 0;
	};

	if (nbytes) {

		mmc_buffer[nbytes++] = 0xf7; // terminate SysEx/MMC message

		Glib::Mutex::Lock lm (midi_lock);

		if (_mmc_port->write (mmc_buffer, nbytes) != nbytes) {
			error << string_compose(_("MMC: cannot send command %1%2%3"), &hex, cmd, &dec) << endmsg;
		}
	}
}

bool
Session::mmc_step_timeout ()
{
	struct timeval now;
	struct timeval diff;
	double diff_usecs;
	gettimeofday (&now, 0);

	timersub (&now, &last_mmc_step, &diff);
	diff_usecs = diff.tv_sec * 1000000 + diff.tv_usec;

	if (diff_usecs > 1000000.0 || fabs (_transport_speed) < 0.0000001) {
		/* too long or too slow, stop transport */
                // don't ask for "clear state on stop". We are just stationary, not stopped
		request_stop ();
		step_queued = false;
		return false;
	}

	if (diff_usecs < 250000.0) {
		/* too short, just keep going */
		return true;
	}

	/* slow it down */

	request_transport_speed (_transport_speed * 0.75);
	return true;
}


void
Session::send_midi_message (MIDI::Port * port, MIDI::eventType ev, MIDI::channel_t ch, MIDI::EventTwoBytes data)
{
	// in another thread, really
	
	MIDIRequest* request = new MIDIRequest;

	request->type = MIDIRequest::SendMessage;
	request->port = port;
	request->ev = ev;
	request->chan = ch;
	request->data = data;
	
	midi_requests.write (&request, 1);
	poke_midi_thread ();
}

void
Session::deliver_midi (MIDI::Port * port, MIDI::byte* buf, int32_t bufsize)
{
	// in another thread, really
	
	MIDIRequest* request = new MIDIRequest;

	request->type = MIDIRequest::Deliver;
	request->port = port;
	request->buf = buf;
	request->size = bufsize;
	
	midi_requests.write (&request, 1);
	poke_midi_thread ();
}

void
Session::deliver_midi_message (MIDI::Port * port, MIDI::eventType ev, MIDI::channel_t ch, MIDI::EventTwoBytes data)
{
	if (port == 0 || ev == MIDI::none) {
		return;
	}

	midi_msg[0] = (ev & 0xF0) | (ch & 0xF); 
	midi_msg[1] = data.controller_number;
	midi_msg[2] = data.value;

	port->write (midi_msg, 3);
}

void
Session::deliver_data (MIDI::Port * port, MIDI::byte* buf, int32_t size)
{
	if (port) {
		port->write (buf, size);
	}

	/* this is part of the semantics of the Deliver request */

	delete [] buf;
}

/*---------------------------------------------------------------------------
  MIDI THREAD 
  ---------------------------------------------------------------------------*/

int
Session::start_midi_thread ()
{
	if (pipe (midi_request_pipe)) {
		error << string_compose(_("Cannot create transport request signal pipe (%1)"), strerror (errno)) << endmsg;
		return -1;
	}

	if (fcntl (midi_request_pipe[0], F_SETFL, O_NONBLOCK)) {
		error << string_compose(_("UI: cannot set O_NONBLOCK on "    "signal read pipe (%1)"), strerror (errno)) << endmsg;
		return -1;
	}

	if (fcntl (midi_request_pipe[1], F_SETFL, O_NONBLOCK)) {
		error << string_compose(_("UI: cannot set O_NONBLOCK on "    "signal write pipe (%1)"), strerror (errno)) << endmsg;
		return -1;
	}

	if (pthread_create_and_store ("transport", &midi_thread, 0, _midi_thread_work, this)) {
		error << _("Session: could not create transport thread") << endmsg;
		return -1;
	}

	// pthread_detach (midi_thread);

	return 0;
}

void
Session::terminate_midi_thread ()
{
	if (midi_thread) {
		MIDIRequest* request = new MIDIRequest;
		void* status;
		
		request->type = MIDIRequest::Quit;
		
		midi_requests.write (&request, 1);
		poke_midi_thread ();
		
		pthread_join (midi_thread, &status);
	}
}

void
Session::poke_midi_thread ()
{
	static char c = 0;

	if (write (midi_request_pipe[1], &c, 1) != 1) {
		error << string_compose(_("cannot send signal to midi thread! (%1)"), strerror (errno)) << endmsg;
	}
}

void *
Session::_midi_thread_work (void* arg)
{
	pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, 0);
	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	((Session *) arg)->midi_thread_work ();
	return 0;
}

void
Session::midi_thread_work ()
{
	MIDIRequest* request;
	struct pollfd *pfd = 0;
        size_t pfd_size = 0;
	size_t nfds = 0;
	int timeout;
	int fds_ready;
	struct sched_param rtparam;
	int x;
	bool restart;
        set<int> fds_used;
        vector<MIDI::Port*> ports;
        bool port_assign_required = true;
        size_t n;

	PBD::notify_gui_about_thread_creation (pthread_self(), X_("MIDI"), 2048);

	memset (&rtparam, 0, sizeof (rtparam));
	rtparam.sched_priority = 9; /* XXX should be relative to audio (JACK) thread */
	
	if ((x = pthread_setschedparam (pthread_self(), SCHED_FIFO, &rtparam)) != 0) {
		// failure ... do we care? not particularly.
	} 

	while (1) {

                const MIDI::Manager::PortMap& pm = MIDI::Manager::instance()->get_midi_ports ();
                        
                if (port_assign_required || (pm.size() != (pfd_size - 1))) {
                        
                        delete [] pfd;

                        ports.clear ();

                        for (MIDI::Manager::PortMap::const_iterator i = pm.begin(); i != pm.end(); ++i) {
                                ports.push_back (i->second);
                        }
                        
                        pfd_size = ports.size() + 1;
                        pfd = new pollfd[pfd_size];
                        port_assign_required = false;
                }
                
		nfds = 0;
                fds_used.clear ();

		pfd[nfds].fd = midi_request_pipe[0];
		pfd[nfds].events = POLLIN|POLLHUP|POLLERR;
		nfds++;

                /* only poll ports if we're meant to be busy */
                
                vector<MIDI::Port*>::const_iterator p;
                size_t n;

                for (p = ports.begin(), n = 1; p != ports.end() && n < pfd_size; ++n, ++p) {
                        
                        int fd = (*p)->selectable();
                        
                        if (fds_used.find (fd) != fds_used.end()) {
                                continue;
                        }
                        
                        fds_used.insert (fd);
                        
                        pfd[n].fd = fd;
                        pfd[n].events = POLLIN|POLLHUP|POLLERR;
                        nfds++;
                }
		
		if (!midi_timeouts.empty()) {
			timeout = 100; /* 10msecs */
		} else {
			timeout = -1; /* if there is no data, we don't care */
		}

	  again:
		// cerr << "MIDI poll on " << nfds << " for " << timeout << endl;
		if (poll (pfd, nfds, timeout) < 0) {
			if (errno == EINTR) {
				/* gdb at work, perhaps */
				goto again;
			}

			error << string_compose(_("MIDI thread poll failed (%1)"), strerror (errno)) << endmsg;

			break;
		}
		// cerr << "MIDI thread wakes at " << get_cycles () << endl;

		fds_ready = 0;
		restart = false;

		/* check the transport request pipe */

		if (pfd[0].revents & ~POLLIN) {
			error << _("Error on transport thread request pipe") << endmsg;
			break;
		}

		if (pfd[0].revents & POLLIN) {

			char foo[16];
			
			// cerr << "MIDI request FIFO ready\n";
			fds_ready++;

			/* empty the pipe of all current requests */

			while (1) {
				size_t nread = read (midi_request_pipe[0], &foo, sizeof (foo));

				if (nread > 0) {
					if ((size_t) nread < sizeof (foo)) {
						break;
					} else {
						continue;
					}
				} else if (nread == 0) {
					break;
				} else if (errno == EAGAIN) {
					break;
				} else {
					fatal << _("Error reading from transport request pipe") << endmsg;
					/*NOTREACHED*/
				}
			}

			while (midi_requests.read (&request, 1) == 1) {

				switch (request->type) {
					
				case MIDIRequest::SendFullMTC:
					// cerr << "send full MTC\n";
					send_full_time_code ();
					// cerr << "... done\n";
					break;
					
				case MIDIRequest::SendMTC:
					// cerr << "send qtr MTC\n";
					send_midi_time_code ();
					// cerr << "... done\n";
					break;
					
				case MIDIRequest::SendMMC:
					// cerr << "send MMC\n";
					deliver_mmc (request->mmc_cmd, request->locate_frame);
					// cerr << "... done\n";
					break;

				case MIDIRequest::SendMessage:
					// cerr << "send Message\n";
					deliver_midi_message (request->port, request->ev, request->chan, request->data);
					// cerr << "... done\n";
					break;
					
				case MIDIRequest::Deliver:
					// cerr << "deliver\n";
					deliver_data (_midi_port, request->buf, request->size);
					// cerr << "... done\n";
					break;
						
				case MIDIRequest::PortChange:
					/* restart poll with new ports */
					// cerr << "rebind\n";
					restart = true;
                                        port_assign_required = true;
					break;
						
				case MIDIRequest::Quit:
					delete request;
					pthread_exit_pbd (0);
					/*NOTREACHED*/
					break;
					
				default:
					break;
				}


				delete request;
			}

		} 

		if (restart) {
			continue;
		}

                /* now check all sequencer related fds, and do port i/o if any data is necessary.
                 */

                MIDI::Manager::PreRead(); /* EMIT SIGNAL */
                
                for (n = 1; n < nfds; ++n) {
                        
			if (pfd[n].revents & ~POLLIN) {
                                // error << string_compose(_("Transport: error polling MIDI port %1 (revents =%2%3%4"), p, &hex, pfd[p].revents, &dec) << endmsg;
                                break;

			} else if (pfd[n].revents & POLLIN) {

                                /* note for ALSA sequencer ports, this can be wierd, since there may only
                                   be 1 selectable/pollable fd for multiple ports. this will catch the "data 
                                   ready" condition for all of them, and then the ALSA specific port
                                   code will figure out how to demux it across the relevant ports.
                                */
                                
                                midi_read (ports[n]);
			}
		}

		/* timeout driven */
		
		if (fds_ready < 2 && timeout != -1) {

			for (MidiTimeoutList::iterator i = midi_timeouts.begin(); i != midi_timeouts.end(); ) {
				
				MidiTimeoutList::iterator tmp;
				tmp = i;
				++tmp;
				
				if (!(*i)()) {
					midi_timeouts.erase (i);
				}
				
				i = tmp;
			}
		}
	}

        delete [] pfd;
}

