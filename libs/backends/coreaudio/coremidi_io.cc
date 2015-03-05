/*
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "coremidi_io.h"
#include <CoreAudio/HostTime.h>

static void notifyProc (const MIDINotification *message, void *refCon) {
	CoreMidiIo *self = static_cast<CoreMidiIo*>(refCon);
	self->notify_proc(message);
}

static void midiInputCallback(const MIDIPacketList *list, void *procRef, void *srcRef) {
	// TODO skip while freewheeling
	RingBuffer<uint8_t> * rb  = static_cast<RingBuffer < uint8_t > *> (srcRef);
	if (!rb) return;
	for (UInt32 i = 0; i < list->numPackets; i++) {
		const MIDIPacket *packet = &list->packet[i];
		if (rb->write_space() < sizeof(MIDIPacket)) { 
			fprintf(stderr, "CoreMIDI: dropped MIDI event\n");
			continue;
		}
		rb->write((uint8_t*)packet, sizeof(MIDIPacket));
	}
}


CoreMidiIo::CoreMidiIo() 
	: _midiClient (0)
	, _inputEndPoints (0)
	, _outputEndPoints (0)
	, _inputPorts (0)
	, _outputPorts (0)
	, _inputQueue (0)
	, _rb (0)
	, _n_midi_in (0)
	, _n_midi_out (0)
	, _time_at_cycle_start (0)
	, _active (false)
	, _changed_callback (0)
	, _changed_arg (0)
{
	OSStatus err;
	err = MIDIClientCreate(CFSTR("Ardour"), &notifyProc, this, &_midiClient);
	if (noErr != err) {
		fprintf(stderr, "Creating Midi Client failed\n");
	}

}

CoreMidiIo::~CoreMidiIo() 
{
	cleanup();
	MIDIClientDispose(_midiClient); _midiClient = 0;
}

void
CoreMidiIo::cleanup() 
{
	_active = false;
	for (uint32_t i = 0 ; i < _n_midi_in ; ++i) {
		MIDIPortDispose(_inputPorts[i]);
		_inputQueue[i].clear();
		delete _rb[i];
	}
	for (uint32_t i = 0 ; i < _n_midi_out ; ++i) {
		MIDIPortDispose(_outputPorts[i]);
	}

	free(_inputPorts); _inputPorts = 0;
	free(_inputEndPoints); _inputEndPoints = 0;
	free(_inputQueue); _inputQueue = 0;
	free(_outputPorts); _outputPorts = 0;
	free(_outputEndPoints); _outputEndPoints = 0;
	free(_rb); _rb = 0;

	_n_midi_in = 0;
	_n_midi_out = 0;
}

void
CoreMidiIo::start_cycle() 
{
	_time_at_cycle_start = AudioGetCurrentHostTime();
}

void
CoreMidiIo::notify_proc(const MIDINotification *message) 
{
	switch(message->messageID) {
		case kMIDIMsgSetupChanged:
			printf("kMIDIMsgSetupChanged\n");
			discover();
			break;
		case kMIDIMsgObjectAdded:
			{
			const MIDIObjectAddRemoveNotification *n = (const MIDIObjectAddRemoveNotification*) message;
			printf("kMIDIMsgObjectAdded\n");
			}
			break;
		case kMIDIMsgObjectRemoved:
			{
			const MIDIObjectAddRemoveNotification *n = (const MIDIObjectAddRemoveNotification*) message;
			printf("kMIDIMsgObjectRemoved\n");
			}
			break;
		case kMIDIMsgPropertyChanged:
			{
			const MIDIObjectPropertyChangeNotification *n = (const MIDIObjectPropertyChangeNotification*) message;
			printf("kMIDIMsgObjectRemoved\n");
			}
			break;
		case kMIDIMsgThruConnectionsChanged:
			printf("kMIDIMsgThruConnectionsChanged\n");
			break;
		case kMIDIMsgSerialPortOwnerChanged:
			printf("kMIDIMsgSerialPortOwnerChanged\n");
			break;
		case kMIDIMsgIOError:
			printf("kMIDIMsgIOError\n");
			cleanup();
			//discover();
			break;
	}
}

size_t
CoreMidiIo::recv_event (uint32_t port, double cycle_time_us, uint64_t &time, uint8_t *d, size_t &s)
{
	if (!_active || _time_at_cycle_start == 0) {
		return 0;
	}
	assert(port < _n_midi_in);

	while (_rb[port]->read_space() >= sizeof(MIDIPacket)) {
		MIDIPacket packet;
		size_t rv = _rb[port]->read((uint8_t*)&packet, sizeof(MIDIPacket));
		assert(rv == sizeof(MIDIPacket));
		_inputQueue[port].push_back(boost::shared_ptr<CoreMIDIPacket>(new _CoreMIDIPacket (&packet))); 
	}

	UInt64 start = _time_at_cycle_start;
	UInt64 end = AudioConvertNanosToHostTime(AudioConvertHostTimeToNanos(_time_at_cycle_start) + cycle_time_us * 1e3);

	for (CoreMIDIQueue::iterator it = _inputQueue[port].begin (); it != _inputQueue[port].end (); ) {
		if ((*it)->timeStamp < end) {
			if ((*it)->timeStamp < start) {
				uint64_t dt = AudioConvertHostTimeToNanos(start - (*it)->timeStamp);
				//printf("Stale Midi Event dt:%.2fms\n", dt * 1e-6);
				if (dt > 1e-4) { // 100ms, maybe too large
					it = _inputQueue[port].erase(it);
					continue;
				}
				time = 0;
			} else {
				time = AudioConvertHostTimeToNanos((*it)->timeStamp - start);
			}
			s = std::min(s, (size_t) (*it)->length);
			if (s > 0) {
				memcpy(d, (*it)->data, s);
			}
			_inputQueue[port].erase(it);
			return s;
		}
		++it;

	}
	return 0;
}

int
CoreMidiIo::send_event (uint32_t port, double reltime_us, const uint8_t *d, const size_t s)
{
	if (!_active || _time_at_cycle_start == 0) {
		return 0;
	}

	assert(port < _n_midi_out);
	UInt64 ts = AudioConvertHostTimeToNanos(_time_at_cycle_start);
	ts += reltime_us * 1e3;

	// TODO use a single packet list.. queue all events first..
	MIDIPacketList pl;

	pl.numPackets = 1;
	MIDIPacket *mp = &(pl.packet[0]);

	mp->timeStamp = AudioConvertNanosToHostTime(ts);
	mp->length = s;
	assert(s < 256);
	memcpy(mp->data, d, s);

	MIDISend(_outputPorts[port], _outputEndPoints[port], &pl);
	return 0;
}

void
CoreMidiIo::discover() 
{
	cleanup();

	assert(!_active && _midiClient);

	ItemCount srcCount = MIDIGetNumberOfSources();
	ItemCount dstCount = MIDIGetNumberOfDestinations();

	if (srcCount > 0) {
		_inputPorts = (MIDIPortRef *) malloc (srcCount * sizeof(MIDIPortRef));
		_inputEndPoints = (MIDIEndpointRef*) malloc (srcCount * sizeof(MIDIEndpointRef));
		_inputQueue = (CoreMIDIQueue*) calloc (srcCount, sizeof(CoreMIDIQueue));
		_rb = (RingBuffer<uint8_t> **) malloc (srcCount * sizeof(RingBuffer<uint8_t>*));
	}
	if (dstCount > 0) {
		_outputPorts = (MIDIPortRef *) malloc (dstCount * sizeof(MIDIPortRef));
		_outputEndPoints = (MIDIEndpointRef*) malloc (dstCount * sizeof(MIDIEndpointRef));
	}

	for (ItemCount i = 0; i < srcCount; i++) {
		OSStatus err;
		MIDIEndpointRef src = MIDIGetSource(i);
		CFStringRef port_name;
		port_name = CFStringCreateWithFormat(NULL, NULL, CFSTR("midi_capture_%lu"), i);

		err = MIDIInputPortCreate (_midiClient, port_name, midiInputCallback, this, &_inputPorts[_n_midi_in]);
		if (noErr != err) {
			fprintf(stderr, "Cannot create Midi Output\n");
			// TODO  handle errors
			continue;
		}
		_rb[_n_midi_in] = new RingBuffer<uint8_t>(1024 * sizeof(MIDIPacket));
		_inputQueue[_n_midi_in] = CoreMIDIQueue();
		MIDIPortConnectSource(_inputPorts[_n_midi_in], src, (void*) _rb[_n_midi_in]);
		CFRelease(port_name);
		_inputEndPoints[_n_midi_in] = src;
		++_n_midi_in;
	}

	for (ItemCount i = 0; i < dstCount; i++) {
		MIDIEndpointRef dst = MIDIGetDestination(i);
		CFStringRef port_name;
		port_name = CFStringCreateWithFormat(NULL, NULL, CFSTR("midi_playback_%lu"), i);

		OSStatus err;
		err = MIDIOutputPortCreate (_midiClient, port_name, &_outputPorts[_n_midi_out]);
		if (noErr != err) {
			fprintf(stderr, "Cannot create Midi Output\n");
			// TODO  handle errors
			continue;
		}
		MIDIPortConnectSource(_outputPorts[_n_midi_out], dst, NULL);
		CFRelease(port_name);
		_outputEndPoints[_n_midi_out] = dst;
		++_n_midi_out;
	}

	if (_changed_callback) {
		_changed_callback(_changed_arg);
	}

	_active = true;
}
