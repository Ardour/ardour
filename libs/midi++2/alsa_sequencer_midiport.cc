/*
  Copyright (C) 2004 Paul Davis 

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

#include <fcntl.h>
#include <cerrno>

#include <midi++/types.h>
#include <midi++/alsa_sequencer.h>
#include <midi++/port_request.h>

//#define DOTRACE 1

#ifdef DOTRACE
#define TR_FN() (cerr << __FUNCTION__ << endl)
#define TR_VAL(v) (cerr << __FILE__  " " << __LINE__ << " " #v  "=" << v << endl)
#else
#define TR_FN()
#define TR_VAL(v)
#endif




using namespace std;
using namespace MIDI;

ALSA_SequencerMidiPort::ALSA_SequencerMidiPort (PortRequest &req)
	: Port (req)
	, seq (0)
	, decoder (0) 
	, encoder (0) 
{
	TR_FN();
	int err;
	if (0 <= (err = CreatePorts (req)) &&
	    0 <= (err = snd_midi_event_new (1024, &decoder)) &&	// Length taken from ARDOUR::Session::midi_read ()
	    0 <= (err = snd_midi_event_new (64, &encoder))) {	// Length taken from ARDOUR::Session::mmc_buffer
		snd_midi_event_init (decoder);
		snd_midi_event_init (encoder);
		_ok = true;
		req.status = PortRequest::OK;
	} else
		req.status = PortRequest::Unknown;
}

ALSA_SequencerMidiPort::~ALSA_SequencerMidiPort ()
{
	if (decoder)
		snd_midi_event_free (decoder);
	if (encoder)
		snd_midi_event_free (encoder);
	if (seq)
		snd_seq_close (seq);
}

int ALSA_SequencerMidiPort::selectable () const
{
	struct pollfd pfd[1];
	if (0 <= snd_seq_poll_descriptors (seq, pfd, 1, POLLIN | POLLOUT)) {
		return pfd[0].fd;
	}
	return -1;
}

int ALSA_SequencerMidiPort::write (byte *msg, size_t msglen)	
{
	TR_FN ();
	int R;
	int totwritten = 0;
	snd_midi_event_reset_encode (encoder);
	int nwritten = snd_midi_event_encode (encoder, msg, msglen, &SEv);
	TR_VAL (nwritten);
	while (0 < nwritten) {
		if (0 <= (R = snd_seq_event_output (seq, &SEv))  &&
		    0 <= (R = snd_seq_drain_output (seq))) {
			bytes_written += nwritten;
			totwritten += nwritten;
			if (output_parser) {
				output_parser->raw_preparse (*output_parser, msg, nwritten);
				for (int i = 0; i < nwritten; i++) {
					output_parser->scanner (msg[i]);
				}
				output_parser->raw_postparse (*output_parser, msg, nwritten);
			}
		} else {
			TR_VAL(R);
			return R;
		}

		msglen -= nwritten;
		msg += nwritten;
		if (msglen > 0) {
			nwritten = snd_midi_event_encode (encoder, msg, msglen, &SEv);
			TR_VAL(nwritten);
		}
		else {
			break;
		}
	}

	return totwritten;
}

int ALSA_SequencerMidiPort::read (byte *buf, size_t max)
{
	TR_FN();
	int err;
	snd_seq_event_t *ev;
	if (0 <= (err = snd_seq_event_input (seq, &ev))) {
		TR_VAL(err);
		err = snd_midi_event_decode (decoder, buf, max, ev);
	}

	if (err > 0) {
		bytes_read += err;

		if (input_parser) {
			input_parser->raw_preparse (*input_parser, buf, err);
			for (int i = 0; i < err; i++) {
				input_parser->scanner (buf[i]);
			}	
			input_parser->raw_postparse (*input_parser, buf, err);
		}
	}
	return -ENOENT == err ? 0 : err;
}

int ALSA_SequencerMidiPort::CreatePorts (PortRequest &req)
{
	int err;
	if (0 <= (err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 
				     (req.mode & O_NONBLOCK) ? SND_SEQ_NONBLOCK : 0))) {
		snd_seq_set_client_name (seq, req.devname);
		unsigned int caps = 0;
		if (req.mode == O_WRONLY  ||  req.mode == O_RDWR)
			caps |= SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
		if (req.mode == O_RDONLY  ||  req.mode == O_RDWR)
			caps |= SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ;
		err = snd_seq_create_simple_port (seq, req.tagname, caps, SND_SEQ_PORT_TYPE_MIDI_GENERIC);
		if (err >= 0) {
			port_id = err;
			snd_seq_ev_clear (&SEv);
			snd_seq_ev_set_source (&SEv, port_id);
			snd_seq_ev_set_subs (&SEv);
			snd_seq_ev_set_direct (&SEv);
		} else 
			snd_seq_close (seq);
	}
	return err;
}

