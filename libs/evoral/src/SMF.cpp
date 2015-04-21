/* This file is part of Evoral.
 * Copyright (C) 2008 David Robillard <http://drobilla.net>
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

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdint.h>
#include "libsmf/smf.h"
#include "evoral/Event.hpp"
#include "evoral/SMF.hpp"
#include "evoral/midi_util.h"

#ifdef COMPILER_MSVC
extern double round(double x);
#endif

using namespace std;

namespace Evoral {

SMF::~SMF()
{
	close ();
}

uint16_t
SMF::num_tracks() const
{
	Glib::Threads::Mutex::Lock lm (_smf_lock);
	return _smf ? _smf->number_of_tracks : 0;
}

uint16_t
SMF::ppqn() const
{
	Glib::Threads::Mutex::Lock lm (_smf_lock);
	return _smf->ppqn;
}

/** Seek to the specified track (1-based indexing)
 * \return 0 on success
 */
int
SMF::seek_to_track(int track)
{
	Glib::Threads::Mutex::Lock lm (_smf_lock);
	_smf_track = smf_get_track_by_number(_smf, track);
	if (_smf_track != NULL) {
		_smf_track->next_event_number = (_smf_track->number_of_events == 0) ? 0 : 1;
		return 0;
	} else {
		return -1;
	}
}

/** Attempt to open the SMF file just to see if it is valid.
 *
 * \return  true on success
 *          false on failure
 */
bool
SMF::test(const std::string& path)
{
	FILE* f = fopen(path.c_str(), "r");
	if (f == 0) {
		return false;
	}

	smf_t* test_smf = smf_load(f);
	fclose(f);

	const bool success = (test_smf != NULL);
	smf_delete(test_smf);

	return success;
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
	Glib::Threads::Mutex::Lock lm (_smf_lock);

	assert(track >= 1);
	if (_smf) {
		smf_delete(_smf);
	}

	FILE* f = fopen(path.c_str(), "r");
	if (f == 0) {
		return -1;
	} else if ((_smf = smf_load(f)) == 0) {
		fclose(f);
		return -1;
	} else if ((_smf_track = smf_get_track_by_number(_smf, track)) == 0) {
		fclose(f);
		return -2;
	}

	//cerr << "Track " << track << " # events: " << _smf_track->number_of_events << endl;
	if (_smf_track->number_of_events == 0) {
		_smf_track->next_event_number = 0;
		_empty = true;
	} else {
		_smf_track->next_event_number = 1;
		_empty = false;
	}

	fclose(f);
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
	Glib::Threads::Mutex::Lock lm (_smf_lock);

	assert(track >= 1);
	if (_smf) {
		smf_delete(_smf);
	}

	_smf = smf_new();

	if (_smf == NULL) {
		return -1;
	}

	if (smf_set_ppqn(_smf, ppqn) != 0) {
		return -1;
	}

	for (int i = 0; i < track; ++i) {
		_smf_track = smf_track_new();
		if (!_smf_track) {
			return -2;
		}
		smf_add_track(_smf, _smf_track);
	}

	_smf_track = smf_get_track_by_number(_smf, track);
	if (!_smf_track)
		return -2;

	_smf_track->next_event_number = 0;

	{
		/* put a stub file on disk */

		FILE* f = fopen (path.c_str(), "w+");
		if (f == 0) {
			return -1;
		}

		if (smf_save (_smf, f)) {
			fclose (f);
			return -1;
		}
		fclose (f);
	}

	_empty = true;

	return 0;
}

void
SMF::close() THROW_FILE_ERROR
{
	Glib::Threads::Mutex::Lock lm (_smf_lock);

	if (_smf) {
		smf_delete(_smf);
		_smf = 0;
		_smf_track = 0;
	}
}

void
SMF::seek_to_start() const
{
	Glib::Threads::Mutex::Lock lm (_smf_lock);
	if (_smf_track) {
		_smf_track->next_event_number = std::min(_smf_track->number_of_events, (size_t)1);
	} else {
		cerr << "WARNING: SMF seek_to_start() with no track" << endl;
	}
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
 * if the event is a meta-event and is an Evoral Note ID, then \a note_id will be set
 * to the value of the NoteID; otherwise, meta-events will set \a note_id to -1.
 *
 * \return event length (including status byte) on success, 0 if event was
 * a meta event, or -1 on EOF (or end of track).
 */
int
SMF::read_event(uint32_t* delta_t, uint32_t* size, uint8_t** buf, event_id_t* note_id) const
{
	Glib::Threads::Mutex::Lock lm (_smf_lock);

	smf_event_t* event;

	assert(delta_t);
	assert(size);
	assert(buf);
	assert(note_id);

	if ((event = smf_track_get_next_event(_smf_track)) != NULL) {

		*delta_t = event->delta_time_pulses;

		if (smf_event_is_metadata(event)) {
			*note_id = -1; // "no note id in this meta-event */

			if (event->midi_buffer[1] == 0x7f) { // Sequencer-specific

				uint32_t evsize;
				uint32_t lenlen;

				if (smf_extract_vlq (&event->midi_buffer[2], event->midi_buffer_length-2, &evsize, &lenlen) == 0) {

					if (event->midi_buffer[2+lenlen] == 0x99 &&  // Evoral
					    event->midi_buffer[3+lenlen] == 0x1) { // Evoral Note ID

						uint32_t id;
						uint32_t idlen;

						if (smf_extract_vlq (&event->midi_buffer[4+lenlen], event->midi_buffer_length-(4+lenlen), &id, &idlen) == 0) {
							*note_id = id;
						}
					}
				}
			}
			return 0; /* this is a meta-event */
		}

		int event_size = event->midi_buffer_length;
		assert(event_size > 0);

		// Make sure we have enough scratch buffer
		if (*size < (unsigned)event_size) {
			*buf = (uint8_t*)realloc(*buf, event_size);
		}
		memcpy(*buf, event->midi_buffer, size_t(event_size));
		*size = event_size;
		if (((*buf)[0] & 0xF0) == 0x90 && (*buf)[2] == 0) {
			/* normalize note on with velocity 0 to proper note off */
			(*buf)[0] = 0x80 | ((*buf)[0] & 0x0F);  /* note off */
			(*buf)[2] = 0x40;  /* default velocity */
		}

		if (!midi_event_is_valid(*buf, *size)) {
			cerr << "WARNING: SMF ignoring illegal MIDI event" << endl;
			*size = 0;
			return -1;
		}

		/* printf("SMF::read_event @ %u: ", *delta_t);
		   for (size_t i = 0; i < *size; ++i) {
		   printf("%X ", (*buf)[i]);
		   } printf("\n") */

		return event_size;
	} else {
		return -1;
	}
}

void
SMF::append_event_delta(uint32_t delta_t, uint32_t size, const uint8_t* buf, event_id_t note_id)
{
	Glib::Threads::Mutex::Lock lm (_smf_lock);

	if (size == 0) {
		return;
	}

	/* printf("SMF::append_event_delta @ %u:", delta_t);
	   for (size_t i = 0; i < size; ++i) {
	   printf("%X ", buf[i]);
	   } printf("\n"); */

	if (!midi_event_is_valid(buf, size)) {
		cerr << "WARNING: SMF ignoring illegal MIDI event" << endl;
		return;
	}

	smf_event_t* event;

	/* XXX july 2010: currently only store event ID's for notes, program changes and bank changes
	 */

	uint8_t const c = buf[0] & 0xf0;
	bool const store_id = (
		c == MIDI_CMD_NOTE_ON ||
		c == MIDI_CMD_NOTE_OFF ||
		c == MIDI_CMD_PGM_CHANGE ||
		(c == MIDI_CMD_CONTROL && (buf[1] == MIDI_CTL_MSB_BANK || buf[1] == MIDI_CTL_LSB_BANK))
	                       );

	if (store_id && note_id >= 0) {
		int idlen;
		int lenlen;
		uint8_t idbuf[16];
		uint8_t lenbuf[16];

		event = smf_event_new ();
		assert(event != NULL);

		/* generate VLQ representation of note ID */
		idlen = smf_format_vlq (idbuf, sizeof(idbuf), note_id);

		/* generate VLQ representation of meta event length,
		   which is the idlen + 2 bytes (Evoral type ID plus Note ID type)
		*/

		lenlen = smf_format_vlq (lenbuf, sizeof(lenbuf), idlen+2);

		event->midi_buffer_length = 2 + lenlen + 2 + idlen;
		/* this should be allocated by malloc(3) because libsmf will
		   call free(3) on it
		*/
		event->midi_buffer = (uint8_t*) malloc (sizeof(uint8_t) * event->midi_buffer_length);

		event->midi_buffer[0] = 0xff; // Meta-event
		event->midi_buffer[1] = 0x7f; // Sequencer-specific
		memcpy (&event->midi_buffer[2], lenbuf, lenlen);
		event->midi_buffer[2+lenlen] = 0x99; // Evoral type ID
		event->midi_buffer[3+lenlen] = 0x1;  // Evoral type Note ID
		memcpy (&event->midi_buffer[4+lenlen], idbuf, idlen);

		assert(_smf_track);
		smf_track_add_event_delta_pulses(_smf_track, event, 0);
	}

	event = smf_event_new_from_pointer(buf, size);
	assert(event != NULL);

	assert(_smf_track);
	smf_track_add_event_delta_pulses(_smf_track, event, delta_t);
	_empty = false;
}

void
SMF::begin_write()
{
	Glib::Threads::Mutex::Lock lm (_smf_lock);

	assert(_smf_track);
	smf_track_delete(_smf_track);

	_smf_track = smf_track_new();
	assert(_smf_track);

	smf_add_track(_smf, _smf_track);
	assert(_smf->number_of_tracks == 1);
}

void
SMF::end_write(string const & path) THROW_FILE_ERROR
{
	Glib::Threads::Mutex::Lock lm (_smf_lock);

	if (!_smf) {
		return;
	}

	FILE* f = fopen (path.c_str(), "w+");
	if (f == 0) {
		throw FileError (path);
	}

	if (smf_save(_smf, f) != 0) {
		fclose(f);
		throw FileError (path);
	}

	fclose(f);
}

double
SMF::round_to_file_precision (double val) const
{
	double div = ppqn();

	return round (val * div) / div;
}

} // namespace Evoral
