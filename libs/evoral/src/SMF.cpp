/* This file is part of Evoral.
 * Copyright (C) 2008 Dave Robillard <http://drobilla.net>
 * Copyright (C) 2000-2008 Paul Davis
 * Author: Hans Baier
 * 
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Evoral is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define __STDC_LIMIT_MACROS 1
#include <cassert>
#include <iostream>
#include <stdint.h>
#include "libsmf/smf.h"
#include "evoral/Event.hpp"
#include "evoral/SMF.hpp"
#include "evoral/midi_util.h"

using namespace std;

namespace Evoral {

SMF::~SMF()
{	
	if (_smf) {
		smf_delete(_smf);
		_smf = 0;
		_smf_track = 0;
	} 
}

uint16_t
SMF::num_tracks() const
{
	return _smf->number_of_tracks;
}

uint16_t
SMF::ppqn() const
{
	return _smf->ppqn;
}

/** Seek to the specified track (1-based indexing)
 * \return 0 on success
 */
int
SMF::seek_to_track(int track)
{
	_smf_track = smf_get_track_by_number(_smf, track);
	if (_smf_track != NULL) {
		_smf_track->next_event_number = (_smf_track->number_of_events == 0) ? 0 : 1;
		return 0;
	} else {
		return -1;
	}
}

/** Attempt to open the SMF file for reading and/or writing.
 *
 * \return  0 on success
 *         -1 if the file can not be opened or created
 *         -2 if the file exists but specified track does not exist
 */
int
SMF::open(const std::string& path, int track) THROW_FILE_ERROR
{
	assert(track >= 1);
	if (_smf) { 
		smf_delete(_smf);
	}
	
	_path = path;
	_smf = smf_load(_path.c_str());
	if (_smf == NULL) {
		return -1;
	}

	_smf_track = smf_get_track_by_number(_smf, track);
	if (!_smf_track)
		return -2;

	//cerr << "Track " << track << " # events: " << _smf_track->number_of_events << endl;
	if (_smf_track->number_of_events == 0) {
		_smf_track->next_event_number = 0;
		_empty = true;
	} else {
		_smf_track->next_event_number = 1;
		_empty = false;
	}
	
	return 0;
}


/** Attempt to create a new SMF file for reading and/or writing.
 *
 * \return  0 on success
 *         -1 if the file can not be created
 *         -2 if the track can not be created
 */
int
SMF::create(const std::string& path, int track, uint16_t ppqn) THROW_FILE_ERROR
{
	assert(track >= 1);
	if (_smf) { 
		smf_delete(_smf);
	}
	
	_path = path;

	_smf = smf_new();
	if (smf_set_ppqn(_smf, ppqn) != 0) {
		throw FileError();
	}
	
	if (_smf == NULL) {
		return -1;
	}
	
	for (int i = 0; i < track; ++i) {
		_smf_track = smf_track_new();
		assert(_smf_track);
		smf_add_track(_smf, _smf_track);
	}

	_smf_track = smf_get_track_by_number(_smf, track);
	if (!_smf_track)
		return -2;

	_smf_track->next_event_number = 0;
	_empty = true;
	
	return 0;
}

void
SMF::close() THROW_FILE_ERROR
{
	if (_smf) {
		if (smf_save(_smf, _path.c_str()) != 0) {
			throw FileError();
		}
		smf_delete(_smf);
		_smf = 0;
		_smf_track = 0;
	}
}

void
SMF::seek_to_start() const
{
	_smf_track->next_event_number = 1;
}

/** Read an event from the current position in file.
 *
 * File position MUST be at the beginning of a delta time, or this will die very messily.
 * ev.buffer must be of size ev.size, and large enough for the event.  The returned event
 * will have it's time field set to it's delta time, in SMF tempo-based ticks, using the
 * rate given by ppqn() (it is the caller's responsibility to calculate a real time).
 *
 * \a buf must be a pointer to a buffer allocated with malloc, or a pointer to NULL.
 * \a size must be the capacity of \a buf.  If it is not large enough, \a buf will
 * be reallocated and *size will be set to the new size of buf.
 *
 * \return event length (including status byte) on success, 0 if event was
 * skipped (e.g. a meta event), or -1 on EOF (or end of track).
 */
int
SMF::read_event(uint32_t* delta_t, uint32_t* size, uint8_t** buf) const
{
	smf_event_t* event;
	
	assert(delta_t);
	assert(size);
	assert(buf);
	
    if ((event = smf_track_get_next_event(_smf_track)) != NULL) {
    	if (smf_event_is_metadata(event)) {
    		return 0;
    	}
    	*delta_t = event->delta_time_pulses;
    	
    	int event_size = event->midi_buffer_length;
    	assert(event_size > 0);
    		
    	// Make sure we have enough scratch buffer
    	if (*size < (unsigned)event_size) {
    		*buf = (uint8_t*)realloc(*buf, event_size);
    	}
    	memcpy(*buf, event->midi_buffer, size_t(event_size));
    	*size = event_size;
	
		assert(midi_event_is_valid(*buf, *size));

		/*printf("SMF::read_event:\n");
		for (size_t i = 0; i < *size; ++i) {
			printf("%X ", (*buf)[i]);
		} printf("\n");*/
    	
    	return event_size;
    } else {
    	return -1;
    }
}

void
SMF::append_event_delta(uint32_t delta_t, uint32_t size, const uint8_t* buf)
{
	if (size == 0) {
		return;
	}
	
	/*printf("SMF::append_event_delta:\n");
	for (size_t i = 0; i < size; ++i) {
		printf("%X ", buf[i]);
	} printf("\n");*/

	if (!midi_event_is_valid(buf, size)) {
		cerr << "WARNING: Ignoring illegal MIDI event" << endl;
		return;
	}

	smf_event_t* event;

	event = smf_event_new_from_pointer(buf, size);
	assert(event != NULL);
	
	assert(_smf_track);
	smf_track_add_event_delta_pulses(_smf_track, event, delta_t);
	_empty = false;
}

void
SMF::begin_write()
{
	assert(_smf_track);
	smf_track_delete(_smf_track);
	
	_smf_track = smf_track_new();
	assert(_smf_track);
	
	smf_add_track(_smf, _smf_track);
	assert(_smf->number_of_tracks == 1);
}

void
SMF::end_write() THROW_FILE_ERROR
{
	if (smf_save(_smf, _path.c_str()) != 0)
		throw FileError();
}


} // namespace Evoral
