/*
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#include <sstream>
#include <CoreAudio/HostTime.h>
#undef nil

#include "coremidi_io.h"
#include "coreaudio_backend.h"

using namespace ARDOUR;

#ifndef NDEBUG
static int _debug_mode = 0;
#endif


/** 
 * MIDI Data flow
 * 
 * (A) INPUT (incoming from outside the application)
 * 
 *    - midiInputCallback (in its own thread, async WRT process callback):
 *       takes OS X MIDIPacket, copies into lock-free ringbuffer
 *
 *    - processCallback (in its own thread):
 *
 *   (1) loop on all input ports:
 *       1A) call recv_event() to read from ringbuffer into stack buffer, also assign-timestamp, 
 *       1B) call parse_events() using stack buffer, when appropriate
 *          pushes CoreMidiEvent into std::vector<CoreMidiEvent>
 *
 *   (2) in MidiPort::cycle_start() (also part of the process callback call tree), MidiPort::get_midi_buffer()
 *       calls CoreAudioBackend::midi_event_get () returns a pointer to the  data of the specified CoreMidiEvent
 */

static void notifyProc (const MIDINotification *message, void *refCon) {
	CoreMidiIo *self = static_cast<CoreMidiIo*>(refCon);
	self->notify_proc(message);
}

#ifndef NDEBUG
static void print_packet (const MIDIPacket *p) {
	fprintf (stderr, "CoreMIDI: Packet %d bytes [ ", p->length);
	for (int bb = 0; bb < p->length; ++bb) {
		fprintf (stderr, "%02x ", ((const uint8_t*)p->data)[bb]);
	}
	fprintf (stderr, "]\n");
}

static void dump_packet_list (const UInt32 numPackets, MIDIPacket const *p) {
	for (UInt32 i = 0; i < numPackets; ++i) {
		print_packet (p);
		p = MIDIPacketNext (p);
	}
}
#endif

static void midiInputCallback(const MIDIPacketList *list, void *procRef, void *srcRef) {
	CoreMidiIo *self = static_cast<CoreMidiIo*> (procRef);
	if (!self || !self->enabled()) {
		// skip while freewheeling
#ifndef NDEBUG
		if (_debug_mode & 2) {
			fprintf (stderr, "Ignored Midi Packet while freewheeling:\n");
			dump_packet_list (list->numPackets, &list->packet[0]);
		}
#endif
		return;
	}
	PBD::RingBuffer<uint8_t> * rb  = static_cast<PBD::RingBuffer < uint8_t > *> (srcRef);
	if (!rb) {
#ifndef NDEBUG
		if (_debug_mode & 4) {
			fprintf (stderr, "Ignored Midi Packet - no ringbuffer:\n");
			dump_packet_list (list->numPackets, &list->packet[0]);
		}
#endif
		return;
	}
	MIDIPacket const *p = &list->packet[0];
	for (UInt32 i = 0; i < list->numPackets; ++i) {
		uint32_t len = ((p->length + 3)&~3) + sizeof(MIDITimeStamp) + sizeof(UInt16);
#ifndef NDEBUG
		if (_debug_mode & 1) {
			print_packet (p);
		}
#endif
		if (rb->write_space() > sizeof(uint32_t) + len) {
			rb->write ((uint8_t*)&len, sizeof(uint32_t));
			rb->write ((uint8_t*)p, len);
		}
#ifndef NDEBUG
		else {
			fprintf (stderr, "CoreMIDI: dropped MIDI event\n");
		}
#endif
		p = MIDIPacketNext (p);
	}
}

static std::string getPropertyString (MIDIObjectRef object, CFStringRef key)
{
	CFStringRef name = NULL;
	std::string rv = "";
	if (noErr == MIDIObjectGetStringProperty(object, key, &name)) {
		const CFIndex size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(name), kCFStringEncodingUTF8);
		char *tmp = (char*) malloc(size);
		if (CFStringGetCString(name, tmp, size, kCFStringEncodingUTF8)) {
			rv = tmp;
		}
		free(tmp);
		CFRelease(name);
	}
	return rv;
}

static std::string getDisplayName (MIDIObjectRef object) {
	return getPropertyString(object, kMIDIPropertyDisplayName);
}

CoreMidiIo::CoreMidiIo()
	: _midi_client (0)
	, _input_endpoints (0)
	, _output_endpoints (0)
	, _input_ports (0)
	, _output_ports (0)
	, _input_queue (0)
	, _rb (0)
	, _n_midi_in (0)
	, _n_midi_out (0)
	, _time_at_cycle_start (0)
	, _active (false)
	, _enabled (true)
	, _run (false)
	, _changed_callback (0)
	, _changed_arg (0)
{
	pthread_mutex_init (&_discovery_lock, 0);

#ifndef NDEBUG
	const char *p = getenv ("ARDOUR_COREMIDI_DEBUG");
	if (p && *p) _debug_mode = atoi (p);
#endif
}

CoreMidiIo::~CoreMidiIo()
{
	pthread_mutex_lock (&_discovery_lock);
	cleanup();
	if (_midi_client) {
		MIDIClientDispose(_midi_client);
		_midi_client = 0;
	}
	pthread_mutex_unlock (&_discovery_lock);
	pthread_mutex_destroy (&_discovery_lock);
}

void
CoreMidiIo::cleanup()
{
	_active = false;
	for (uint32_t i = 0 ; i < _n_midi_in ; ++i) {
		MIDIPortDispose(_input_ports[i]);
		_input_queue[i].clear();
		delete _rb[i];
	}
	for (uint32_t i = 0 ; i < _n_midi_out ; ++i) {
		MIDIPortDispose(_output_ports[i]);
	}

	free(_input_ports); _input_ports = 0;
	free(_input_endpoints); _input_endpoints = 0;
	free(_input_queue); _input_queue = 0;
	free(_output_ports); _output_ports = 0;
	free(_output_endpoints); _output_endpoints = 0;
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
			/* this one catches all of the added/removed/changed below */
			//printf("kMIDIMsgSetupChanged\n");
			discover();
			break;
		case kMIDIMsgObjectAdded:
			{
			//const MIDIObjectAddRemoveNotification *n = (const MIDIObjectAddRemoveNotification*) message;
			//printf("kMIDIMsgObjectAdded\n");
			}
			break;
		case kMIDIMsgObjectRemoved:
			{
			//const MIDIObjectAddRemoveNotification *n = (const MIDIObjectAddRemoveNotification*) message;
			//printf("kMIDIMsgObjectRemoved\n");
			}
			break;
		case kMIDIMsgPropertyChanged:
			{
			//const MIDIObjectPropertyChangeNotification *n = (const MIDIObjectPropertyChangeNotification*) message;
			//printf("kMIDIMsgObjectRemoved\n");
			}
			break;
		case kMIDIMsgThruConnectionsChanged:
			//printf("kMIDIMsgThruConnectionsChanged\n");
			break;
		case kMIDIMsgSerialPortOwnerChanged:
			//printf("kMIDIMsgSerialPortOwnerChanged\n");
			break;
		case kMIDIMsgIOError:
			fprintf(stderr, "kMIDIMsgIOError\n");
			discover();
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

	const size_t minsize = 1 + sizeof(uint32_t) + sizeof(MIDITimeStamp) + sizeof(UInt16);

	while (_rb[port]->read_space() > minsize) {
		MIDIPacket packet;
		size_t rv;
		uint32_t s = 0;
		rv = _rb[port]->read((uint8_t*)&s, sizeof(uint32_t));
		assert(rv == sizeof(uint32_t));
		rv = _rb[port]->read((uint8_t*)&packet, s);
		assert(rv == s);
		_input_queue[port].push_back(boost::shared_ptr<CoreMIDIPacket>(new _CoreMIDIPacket (&packet)));
	}

	UInt64 start = _time_at_cycle_start;
	UInt64 end = AudioConvertNanosToHostTime(AudioConvertHostTimeToNanos(_time_at_cycle_start) + cycle_time_us * 1e3);

	for (CoreMIDIQueue::iterator it = _input_queue[port].begin (); it != _input_queue[port].end (); ) {
		if ((*it)->timeStamp < end) {
			if ((*it)->timeStamp < start) {
				uint64_t dt = AudioConvertHostTimeToNanos(start - (*it)->timeStamp);
				if (dt > 1e7 && (*it)->timeStamp != 0) { // 10ms slack and a timestamp is given
#ifndef NDEBUG
					printf("Dropped Stale Midi Event. dt:%.2fms\n", dt * 1e-6);
#endif
					it = _input_queue[port].erase(it);
					continue;
				} else {
					/* events without a valid timestamp, or events that arrived
					 * less than 10ms in the past are allowed and
					 * queued at the beginning of the cycle:
					 * time (relative to cycle start) = 0
					 *
					 * The latter use needed for the "Avid Artist" Control Surface
					 * the OSX driver sends no timestamps.
					 */
#if 0
					printf("Stale Midi Event. dt:%.2fms\n", dt * 1e-6);
#endif
				}
				time = 0;
			} else {
				time = AudioConvertHostTimeToNanos((*it)->timeStamp - start);
			}
			s = std::min(s, (size_t) (*it)->length);
			if (s > 0) {
				memcpy(d, (*it)->data, s);
			}
			_input_queue[port].erase(it);
			return s;
		}
		++it;

	}
	return 0;
}

int
CoreMidiIo::send_events (uint32_t port, double timescale, const void *b)
{
	if (!_active || _time_at_cycle_start == 0) {
		return 0;
	}

	assert(port < _n_midi_out);
	const UInt64 ts = AudioConvertHostTimeToNanos(_time_at_cycle_start);

	const CoreMidiBuffer *src = static_cast<CoreMidiBuffer const *>(b);

	int32_t bytes[8192]; // int for alignment
	MIDIPacketList *mpl = (MIDIPacketList*)bytes;
	MIDIPacket *cur = MIDIPacketListInit(mpl);

	for (CoreMidiBuffer::const_iterator mit = src->begin (); mit != src->end (); ++mit) {
		assert(mit->size() < 256);
		cur = MIDIPacketListAdd(mpl, sizeof(bytes), cur,
				AudioConvertNanosToHostTime(ts + mit->timestamp() / timescale),
				mit->size(), mit->data());
		if (!cur) {
#ifndef DEBUG
			printf("CoreMidi: Packet list overflow, dropped events\n");
#endif
			break;
		}
	}
	if (mpl->numPackets > 0) {
		MIDISend(_output_ports[port], _output_endpoints[port], mpl);
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

	MIDISend(_output_ports[port], _output_endpoints[port], &pl);
	return 0;
}


std::string
CoreMidiIo::port_id (uint32_t port, bool input)
{
	std::stringstream ss;
	if (input) {
		ss << "system:midi_capture_";
		SInt32 id;
		if (noErr == MIDIObjectGetIntegerProperty(_input_endpoints[port], kMIDIPropertyUniqueID, &id)) {
			ss << (unsigned int)id;
		} else {
			ss << port;
		}
	} else {
		ss << "system:midi_playback_";
		SInt32 id;
		if (noErr == MIDIObjectGetIntegerProperty(_output_endpoints[port], kMIDIPropertyUniqueID, &id)) {
			ss << (unsigned int)id;
		} else {
			ss << port;
		}
	}
	return ss.str();
}

std::string
CoreMidiIo::port_name (uint32_t port, bool input)
{
	if (input) {
		if (port < _n_midi_in) {
			return getDisplayName(_input_endpoints[port]);
		}
	} else {
		if (port < _n_midi_out) {
			return getDisplayName(_output_endpoints[port]);
		}
	}
	return "";
}

void
CoreMidiIo::start () {
	_run = true;
	if (!_midi_client) {
		OSStatus err;
		err = MIDIClientCreate(CFSTR("Ardour"), &notifyProc, this, &_midi_client);
		if (noErr != err) {
			fprintf(stderr, "Creating Midi Client failed\n");
		}
	}
	discover();
}

void
CoreMidiIo::stop ()
{
	_run = false;
	pthread_mutex_lock (&_discovery_lock);
	cleanup();
	pthread_mutex_unlock (&_discovery_lock);
#if 0
	if (_midi_client) {
		MIDIClientDispose(_midi_client);
		_midi_client = 0;
	}
#endif
}

void
CoreMidiIo::discover()
{
	if (!_run || !_midi_client) { return; }

	if (pthread_mutex_trylock (&_discovery_lock)) {
		return;
	}

	cleanup();

	ItemCount srcCount = MIDIGetNumberOfSources();
	ItemCount dstCount = MIDIGetNumberOfDestinations();

	if (srcCount > 0) {
		_input_ports = (MIDIPortRef *) malloc (srcCount * sizeof(MIDIPortRef));
		_input_endpoints = (MIDIEndpointRef*) malloc (srcCount * sizeof(MIDIEndpointRef));
		_input_queue = (CoreMIDIQueue*) calloc (srcCount, sizeof(CoreMIDIQueue));
		_rb = (PBD::RingBuffer<uint8_t> **) malloc (srcCount * sizeof(PBD::RingBuffer<uint8_t>*));
	}
	if (dstCount > 0) {
		_output_ports = (MIDIPortRef *) malloc (dstCount * sizeof(MIDIPortRef));
		_output_endpoints = (MIDIEndpointRef*) malloc (dstCount * sizeof(MIDIEndpointRef));
	}

	for (ItemCount i = 0; i < srcCount; i++) {
		OSStatus err;
		MIDIEndpointRef src = MIDIGetSource(i);
		if (!src) continue;
#ifndef NDEBUG
		printf("MIDI IN DEVICE: %s\n", getDisplayName(src).c_str());
#endif

		CFStringRef port_name;
		port_name = CFStringCreateWithFormat(NULL, NULL, CFSTR("midi_capture_%lu"), i);

		err = MIDIInputPortCreate (_midi_client, port_name, midiInputCallback, this, &_input_ports[_n_midi_in]);
		if (noErr != err) {
			fprintf(stderr, "Cannot create Midi Output\n");
			continue;
		}
		_rb[_n_midi_in] = new PBD::RingBuffer<uint8_t>(32768);
		_input_queue[_n_midi_in] = CoreMIDIQueue();
		MIDIPortConnectSource(_input_ports[_n_midi_in], src, (void*) _rb[_n_midi_in]);
		CFRelease(port_name);
		_input_endpoints[_n_midi_in] = src;
		++_n_midi_in;
	}

	for (ItemCount i = 0; i < dstCount; i++) {
		MIDIEndpointRef dst = MIDIGetDestination(i);
		CFStringRef port_name;
		port_name = CFStringCreateWithFormat(NULL, NULL, CFSTR("midi_playback_%lu"), i);

		OSStatus err;
		err = MIDIOutputPortCreate (_midi_client, port_name, &_output_ports[_n_midi_out]);
		if (noErr != err) {
			fprintf(stderr, "Cannot create Midi Output\n");
			continue;
		}

#ifndef NDEBUG
		printf("MIDI OUT DEVICE: %s\n", getDisplayName(dst).c_str());
#endif

		MIDIPortConnectSource(_output_ports[_n_midi_out], dst, NULL);
		CFRelease(port_name);
		_output_endpoints[_n_midi_out] = dst;
		++_n_midi_out;
	}

	if (_changed_callback) {
		_changed_callback(_changed_arg);
	}

	_active = true;
	pthread_mutex_unlock (&_discovery_lock);
}
