/*
 *   VST instrument support
 *
 *   Derived from code that was marked:    
 *   Copyright (C) Kjetil S. Matheussen 2004 (k.s.matheussen@notam02.no)
 *   Alsa-seq midi-code made by looking at the jack-rack source made by Bob Ham.
 *    
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   $Id: vsti.c,v 1.2 2004/04/07 01:56:23 pauld Exp $
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>
#include <fcntl.h>
#include <stdbool.h>
#include <jackvst.h>
#include <pthread.h>
#include <sched.h>
#include "ardour/vestige/aeffectx.h"

#ifdef HAVE_ALSA

snd_seq_t *
create_sequencer (const char* client_name, bool isinput)
{
	snd_seq_t * seq;
	int err;
	
	if ((err = snd_seq_open (&seq, "default", SND_SEQ_OPEN_DUPLEX, 0)) != 0) {
		fst_error ("Could not open ALSA sequencer, aborting\n\n%s\n\n"
			   "Make sure you have configure ALSA properly and that\n"
			   "/proc/asound/seq/clients exists and contains relevant\n"
			   "devices (%s).", 
			   snd_strerror (err));
		return NULL;
	}
	
	snd_seq_set_client_name (seq, client_name);
	
	if ((err = snd_seq_create_simple_port (seq, isinput? "Input" : "Output",
					       (isinput? SND_SEQ_PORT_CAP_WRITE: SND_SEQ_PORT_CAP_READ)| SND_SEQ_PORT_CAP_DUPLEX |
					       SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_SUBS_WRITE,
					       SND_SEQ_PORT_TYPE_APPLICATION|SND_SEQ_PORT_TYPE_SPECIFIC)) != 0) {
		fst_error ("Could not create ALSA port: %s", snd_strerror (err));
		snd_seq_close(seq);
		return NULL;
	}
	
	return seq;
}

static void 
queue_midi (JackVST *jvst, int val1, int val2, int val3)
{
	VstMidiEvent *pevent;
	jack_ringbuffer_data_t vec[2];

	jack_ringbuffer_get_write_vector (jvst->event_queue, vec);

	if (vec[0].len < sizeof (VstMidiEvent)) {
		fst_error ("event queue has no write space");
		return;
	}
		
	pevent = (VstMidiEvent *) vec[0].buf;

	//  printf("note: %d\n",note);
	
	pevent->type = kVstMidiType;
	pevent->byteSize = 24;
	pevent->deltaFrames = 0;
	pevent->flags = 0;
	pevent->detune = 0;
	pevent->noteLength = 0;
	pevent->noteOffset = 0;
	pevent->reserved1 = 0;
	pevent->reserved2 = 0;
	pevent->noteOffVelocity = 0;
	pevent->midiData[0] = val1;
	pevent->midiData[1] = val2;
	pevent->midiData[2] = val3;
	pevent->midiData[3] = 0;
	
	//printf("Sending: %x %x %x\n",val1,val2,val3);

	jack_ringbuffer_write_advance (jvst->event_queue, sizeof (VstMidiEvent));
}

void *midireceiver(void *arg)
{
	snd_seq_event_t *event;
	JackVST *jvst = (JackVST* )arg;
	int val;

	struct sched_param scp;
	scp.sched_priority = 50;

	// Try to set fifo priority...
	// this works, if we are root or newe sched-cap manegment is used...
	pthread_setschedparam( pthread_self(), SCHED_FIFO, &scp ); 
	
	while (1) {

		snd_seq_event_input (jvst->seq, &event);

		if (jvst->midiquit) {
			break;
		}

		switch(event->type){
		case SND_SEQ_EVENT_NOTEON:
			queue_midi(jvst,0x90+event->data.note.channel,event->data.note.note,event->data.note.velocity);
			//printf("Noteon, channel: %d note: %d vol: %d\n",event->data.note.channel,event->data.note.note,event->data.note.velocity);
			break;
		case SND_SEQ_EVENT_NOTEOFF:
			queue_midi(jvst,0x80+event->data.note.channel,event->data.note.note,0);
			//printf("Noteoff, channel: %d note: %d vol: %d\n",event->data.note.channel,event->data.note.note,event->data.note.velocity);
			break;
		case SND_SEQ_EVENT_KEYPRESS:
			//printf("Keypress, channel: %d note: %d vol: %d\n",event->data.note.channel,event->data.note.note,event->data.note.velocity);
			queue_midi(jvst,0xa0+event->data.note.channel,event->data.note.note,event->data.note.velocity);
			break;
		case SND_SEQ_EVENT_CONTROLLER:
			queue_midi(jvst,0xb0+event->data.control.channel,event->data.control.param,event->data.control.value);
			//printf("Control: %d %d %d\n",event->data.control.channel,event->data.control.param,event->data.control.value);
			break;
		case SND_SEQ_EVENT_PITCHBEND:
			val=event->data.control.value + 0x2000;
			queue_midi(jvst,0xe0+event->data.control.channel,val&127,val>>7);
			//printf("Pitch: %d %d %d\n",event->data.control.channel,event->data.control.param,event->data.control.value);
			break;
		case SND_SEQ_EVENT_CHANPRESS:
			//printf("chanpress: %d %d %d\n",event->data.control.channel,event->data.control.param,event->data.control.value);
			queue_midi(jvst,0xd0+event->data.control.channel,event->data.control.value,0);
			break;
		case SND_SEQ_EVENT_PGMCHANGE:
			//printf("pgmchange: %d %d %d\n",event->data.control.channel,event->data.control.param,event->data.control.value);
			queue_midi(jvst,0xc0+event->data.control.channel,event->data.control.value,0);
			break;
		default:
			//printf("Unknown type: %d\n",event->type);
			break;
		}
	}
	
	return NULL;
}

void stop_midireceiver (JackVST *jvst)
{
	int err; 
	snd_seq_event_t event;
	snd_seq_t *seq2 = create_sequencer ("jfstquit", true);
	
	jvst->midiquit = 1;
	
	snd_seq_connect_to (seq2, 0, snd_seq_client_id (jvst->seq),0);
	snd_seq_ev_clear      (&event);
	snd_seq_ev_set_direct (&event);
	snd_seq_ev_set_subs   (&event);
	snd_seq_ev_set_source (&event, 0);
	snd_seq_ev_set_controller (&event,1,0x80,50);
	
	if ((err = snd_seq_event_output (seq2, &event)) < 0) {
		fst_error ("cannot send stop event to midi thread: %s\n",
			   snd_strerror (err));
	}

	snd_seq_drain_output (seq2);
	snd_seq_close (seq2);
	pthread_join (jvst->midi_thread,NULL);
	snd_seq_close (jvst->seq);
}
#endif


