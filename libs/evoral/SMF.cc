/*
 * Copyright (C) 2008-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2018 Robin Gareus <robin@gareus.org>
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

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdint.h>

#include <glib/gstdio.h>
#include <glibmm/convert.h>

#include "pbd/whitespace.h"

#include "libsmf/smf.h"

#include "temporal/tempo.h"

#include "evoral/Event.h"
#include "evoral/SMF.h"
#include "evoral/midi_util.h"

#ifdef COMPILER_MSVC
extern double round(double x);
#endif

using namespace std;

namespace Evoral {

SMF::SMF()
	: _smf (0)
	, _smf_track (0)
	, _empty (true)
	, _n_note_on_events (0)
	, _has_pgm_change (false)
	, _num_channels (0)
	{};

SMF::~SMF()
{
	close ();
}

int
SMF::smf_format () const
{
	return _smf ? _smf->format : 0;
}

uint16_t
SMF::num_tracks() const
{
	Glib::Threads::Mutex::Lock lm (_smf_lock);
	return (uint16_t) (_smf ? _smf->number_of_tracks : 0);
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
	FILE* f = g_fopen(path.c_str(), "r");
	if (f == 0) {
		return false;
	}

	smf_t* test_smf = smf_load(f);
	fclose(f);

	if (!test_smf) {
		return false;
	}
	if (test_smf) {
		smf_delete(test_smf);
	}
	return true;
}

/** Attempt to open the SMF file for reading and/or writing.
 *
 * \return  0 on success
 *         -1 if the file can not be opened or created
 *         -2 if the file exists but specified track does not exist
 */
int
SMF::open(const std::string& path, int track, bool scan)
{
	Glib::Threads::Mutex::Lock lm (_smf_lock);

	_num_channels     = 0;
	_n_note_on_events = 0;
	_has_pgm_change   = false;
	_used_channels.reset ();

	assert(track >= 1);
	if (_smf) {
		smf_delete(_smf);
	}

	FILE* f = g_fopen(path.c_str(), "r");
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

	lm.release ();
	if (!_empty && scan) {
		/* scan the file, set meta-data w/o loading the model */
		bool type0 = _smf->format==0;
		for (int i = 1; i <= _smf->number_of_tracks; ++i) {
			/* scan file for used channels. */
			int ret;
			uint32_t delta_t = 0;
			uint32_t size    = 0;
			uint8_t* buf     = NULL;
			event_id_t event_id = 0;

			if (type0) {
				seek_to_start ();  //type0 files have no 'track' concept, just seek_to_start
			} else {
				seek_to_track (i);
			}

			while ((ret = read_event (&delta_t, &size, &buf, &event_id)) >= 0) {
				if (ret == 0) {
					continue;
				}
				if (size == 0) {
					break;
				}
				uint8_t type = buf[0] & 0xf0;
				uint8_t chan = buf[0] & 0x0f;

				if (type >= 0x80 && type <= 0xE0) {
					_used_channels.set(chan);
					switch (type) {
						case MIDI_CMD_NOTE_ON:
							++_n_note_on_events;
							break;
						case MIDI_CMD_PGM_CHANGE:
							_has_pgm_change = true;
							break;
						default:
							break;
					}
				}
			}
			_num_channels += _used_channels.count ();
			free (buf);
		}
	}
	if (!_empty) {
		seek_to_start();
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
SMF::create(const std::string& path, int track, uint16_t ppqn)
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

		FILE* f = g_fopen (path.c_str(), "w+b");
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
	_num_channels = 0;

	return 0;
}

void
SMF::close()
{
	Glib::Threads::Mutex::Lock lm (_smf_lock);

	if (_smf) {
		smf_delete(_smf);
		_smf = 0;
		_smf_track = 0;
		_num_channels = 0;
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

		uint32_t event_size = (uint32_t) event->midi_buffer_length;
		assert(event_size > 0);

		// Make sure we have enough scratch buffer
		if (*size < (unsigned)event_size) {
			*buf = (uint8_t*)realloc(*buf, event_size);
		}
		assert (*buf);
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

	switch (buf[0]) {
	case 0xf1:
	case 0xf2:
	case 0xf3:
	case 0xf4:
	case 0xf5:
	case 0xf6:
	case 0xf8:
	case 0xf9:
	case 0xfa:
	case 0xfb:
	case 0xfc:
	case 0xfd:
	case 0xfe:
	case 0xff:
		/* System Real Time or System Common event: not valid in SMF
		 */
		return;
	}

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
		c == MIDI_CMD_NOTE_PRESSURE ||
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
SMF::end_write(string const & path)
{
	Glib::Threads::Mutex::Lock lm (_smf_lock);

	if (!_smf) {
		return;
	}

	FILE* f = g_fopen (path.c_str(), "w+b");
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

static bool invalid_char (unsigned char c)
{
	return !isprint (c) && c != '\n';
}

void
SMF::track_names(vector<string>& names) const
{
	if (!_smf) {
		return;
	}

	names.clear ();

	Glib::Threads::Mutex::Lock lm (_smf_lock);

	for (uint16_t n = 0; n < _smf->number_of_tracks; ++n) {
		smf_track_t* trk = smf_get_track_by_number (_smf, n+1);
		if (!trk) {
			names.push_back (string());
		} else {
			if (trk->name) {
				std::string name (Glib::convert_with_fallback (trk->name, "UTF-8", "ISO-8859-1", "_"));
				name.erase (std::remove_if (name.begin(), name.end(), invalid_char), name.end());
				names.push_back (name);
			} else {
				char buf[32];
				sprintf(buf, "t%d", n+1);
				names.push_back (buf);
			}
		}
	}
}

void
SMF::instrument_names(vector<string>& names) const
{
	if (!_smf) {
		return;
	}

	names.clear ();

	Glib::Threads::Mutex::Lock lm (_smf_lock);

	for (uint16_t n = 0; n < _smf->number_of_tracks; ++n) {
		smf_track_t* trk = smf_get_track_by_number (_smf, n+1);
		if (!trk) {
			names.push_back (string());
		} else {
			if (trk->instrument) {
				std::string name (Glib::convert_with_fallback (trk->instrument, "UTF-8", "ISO-8859-1", "_"));
				name.erase (std::remove_if (name.begin(), name.end(), invalid_char), name.end());
				names.push_back (name);
			} else {
				char buf[32];
				sprintf(buf, "i%d", n+1);
				names.push_back (buf);
			}
		}
	}
}

SMF::Tempo::Tempo (smf_tempo_t* smft)
	: time_pulses (smft->time_pulses)
	, microseconds_per_quarter_note (smft->microseconds_per_quarter_note)
	, numerator (smft->numerator)
	, denominator (smft->denominator)
	, clocks_per_click (smft->clocks_per_click)
	, notes_per_note (smft->notes_per_note)
{
}

int
SMF::num_tempos () const
{
	assert (_smf);
	return smf_get_tempo_count (_smf);
}

SMF::Tempo*
SMF::nth_tempo (size_t n) const
{
	assert (_smf);

	smf_tempo_t* t = smf_get_tempo_by_number (_smf, n);
	if (!t) {
		return 0;
	}

	return new Tempo (t);
}

void
SMF::load_markers ()
{
	if (!_smf_track) {
		return;
	}

	Glib::Threads::Mutex::Lock lm (_smf_lock);

	if (_smf_track) {
		_smf_track->next_event_number = std::min(_smf_track->number_of_events, (size_t)1);
	}

	smf_event_t* event;

	while ((event = smf_track_get_next_event(_smf_track)) != NULL) {
		/* compare to smf_event_decode_metadata, smf_event_decode_textual */
		bool allow_empty = false;
		if (smf_event_is_metadata(event)) {
			string name;
			switch (event->midi_buffer[1]) {
				case 0x05:
					name = "Lyric:";
					break;
				case 0x06:
					name = "Marker:";
					break;
				case 0x07:
					name = "Cue Point:";
					allow_empty = true;
					break;
				case 0x01: // "Text:"
					/* fallthtough */
				case 0x02: // "Copyright:"
					/* fallthtough */
				case 0x03: // "Sequence/Track Name:"
					/* fallthtough */
				case 0x04: // "Instrument:"
					/* fallthtough */
				case 0x08: // "Program Name:"
					/* fallthtough */
				case 0x09: // "Device (Port) Name:"
					/* fallthtough */
				default:
					continue;
			}

			char const * txt = smf_event_decode (event);

			if (!txt) {
				continue;
			}

			string marker (txt);

			if (marker.find (name) == 0) {
				marker = marker.substr (name.length ());
			}

			PBD::strip_whitespace_edges (marker);

			if (marker.empty () && !allow_empty) {
				continue;
			}

			_markers.push_back (MarkerAt (marker, event->time_pulses));
		}
	}
}

std::shared_ptr<Temporal::TempoMap>
SMF::tempo_map (bool& provided) const
{
	/* cannot create an empty TempoMap, so create one with "default" single
	   values for tempo and meter, then overwrite them.
	*/

	std::shared_ptr<Temporal::TempoMap> new_map (new Temporal::TempoMap (Temporal::Tempo (120, 4), Temporal::Meter (4, 4)));
	const size_t ntempos = num_tempos ();

	if (ntempos == 0) {
		provided = false;
		return new_map;
	}

	Temporal::Meter last_meter (4, 4);
	bool have_initial_meter = false;

	for (size_t n = 0; n < ntempos; ++n) {

		Evoral::SMF::Tempo* t = nth_tempo (n);
		assert (t);

		Temporal::Tempo tempo (t->tempo(), 32.0 / (double) t->notes_per_note);
		Temporal::Meter meter (t->numerator, t->denominator);

		Temporal::BBT_Argument bbt; /* 1|1|0 which is correct for the no-meter case */

		if (have_initial_meter) {

			bbt = new_map->bbt_at (Temporal::timepos_t (Temporal::Beats (int_div_round (t->time_pulses, (size_t) ppqn()), 0)));
			new_map->set_tempo (tempo, bbt);

			if (!(meter == last_meter)) {
				new_map->set_meter (meter, bbt);
			}

		} else {
			new_map->set_meter (meter, bbt);
			new_map->set_tempo (tempo, bbt);
			have_initial_meter = true;
		}

		last_meter = meter;
	}

	provided = true;
	return new_map;
}

} // namespace Evoral
