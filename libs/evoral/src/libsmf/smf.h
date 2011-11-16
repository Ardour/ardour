/*-
 * Copyright (c) 2007, 2008 Edward Tomasz Napierała <trasz@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * ALTHOUGH THIS SOFTWARE IS MADE OF WIN AND SCIENCE, IT IS PROVIDED BY THE
 * AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**
 * \file
 *
 * Public interface declaration for libsmf, Standard MIDI File format library.
 */

/**
 *
 * \page libsmf libsmf - general usage instructions
 *
 * An smf_t structure represents a "song".  Every valid smf contains one or more tracks.
 * Tracks contain zero or more events.  Libsmf doesn't care about actual MIDI data, as long
 * as it is valid from the MIDI specification point of view - it may be realtime message,
 * SysEx, whatever.
 * 
 * The only field in smf_t, smf_track_t, smf_event_t and smf_tempo_t structures your
 * code may modify is event->midi_buffer and event->midi_buffer_length.  Do not modify
 * other fields, _ever_.  You may read them, though.  Do not declare static instances
 * of these types, i.e. never do something like this:  "smf_t smf;".  Always use
 * "smf_t *smf = smf_new();".  The same applies to smf_track_t and smf_event_t.
 * 
 * Say you want to load a Standard MIDI File (.mid) file and play it back somehow.
 * This is (roughly) how you do this:
 * 
 * \code
 * 	smf_t *smf;
 * 	smf_event_t *event;
 *
 * 	smf = smf_load(file_name);
 * 	if (smf == NULL) {
 * 		Whoops, something went wrong.
 * 		return;
 * 	}
 * 
 * 	while ((event = smf_get_next_event(smf)) != NULL) {
 *		if (smf_event_is_metadata(event))
 *			continue;
 * 
 * 		wait until event->time_seconds.
 * 		feed_to_midi_output(event->midi_buffer, event->midi_buffer_length);
 * 	}
 *
 *	smf_delete(smf);
 *
 * \endcode
 * 
 * Saving works like this:
 * 
 * \code
 *
 * 	smf_t *smf;
 *	smf_track_t *track;
 *	smf_event_t *event;
 *
 * 	smf = smf_new();
 * 	if (smf == NULL) {
 * 		Whoops.
 * 		return;
 * 	}
 * 
 * 	for (int i = 1; i <= number of tracks; i++) {
 * 		track = smf_track_new();
 * 		if (track == NULL) {
 * 			Whoops.
 * 			return;
 * 		}
 * 
 * 		smf_add_track(smf, track);
 * 
 * 		for (int j = 1; j <= number of events you want to put into this track; j++) {
 * 			event = smf_event_new_from_pointer(your MIDI message, message length);
 * 			if (event == NULL) {
 * 				Whoops.
 * 				return;
 * 			}
 * 
 * 			smf_track_add_event_seconds(track, event, seconds since start of the song);
 * 		}
 * 	}
 * 
 * 	ret = smf_save(smf, file_name);
 * 	if (ret) {
 * 		Whoops, saving failed for some reason.
 * 		return;
 * 	}
 *
 *	smf_delete(smf);
 *
 * \endcode
 *
 * There are two basic ways of getting MIDI data out of smf - sequential or by track/event number.
 * You may mix them if you need to.  First one is used in the example above - seek to the point
 * from which you want the playback to start (using smf_seek_to_seconds(), smf_seek_to_pulses()
 * or smf_seek_to_event()) and then do smf_get_next_event() in loop, until it returns NULL.
 * Calling smf_load() causes the smf to be rewound to the start of the song.
 *
 * Getting events by number works like this:
 *
 * \code
 *
 * smf_track_t *track = smf_get_track_by_number(smf, track_number);
 * smf_event_t *event = smf_track_get_event_by_number(track, event_number);
 *
 * \endcode
 *
 * To create new event, use smf_event_new(), smf_event_new_from_pointer() or
 * smf_event_new_from_bytes().  First one creates an empty event - you need to manually allocate
 * (using malloc(3)) buffer for MIDI data, write MIDI data into it, put the address of that
 * buffer into event->midi_buffer, and the length of MIDI data into event->midi_buffer_length.
 * Note that deleting the event (using smf_event_delete()) will free the buffer.
 *
 * Second form does most of this for you: it takes an address of the buffer containing MIDI data,
 * allocates storage and copies MIDI data into it.
 *
 * Third form is useful for manually creating short events, up to three bytes in length, for
 * example Note On or Note Off events.  It simply takes three bytes and creates MIDI event
 * containing them.  If you need to create MIDI message that takes only two bytes, pass -1 as
 * the third byte.  For one byte message (System Realtime), pass -1 as second and third byte.
 *
 * To free an event, use smf_event_delete().
 *
 * To add event to the track, use smf_track_add_event_delta_pulses(), smf_track_add_event_pulses(),
 * or smf_track_add_event_seconds().  The difference between them is in the way you specify the
 * time of the event - with the first one, you specify it as an interval, in pulses, from the
 * previous event in this track; with the second one, you specify it as pulses from the start
 * of the song, and with the last one, you specify it as seconds from the start of the song.
 * Obviously, the first version can only append events at the end of the track.
 *
 * To remove an event from the track it's attached to, use smf_event_remove_from_track().
 * You may want to free the event (using smf_event_delete()) afterwards.
 *
 * To create new track, use smf_track_new().  To add track to the smf, use smf_add_track().
 * To remove track from its smf, use smf_track_remove_from_smf().  To free the track structure,
 * use smf_track_delete().
 *
 * Note that libsmf keeps things consistent.  If you free (using smf_track_delete()) a track
 * that is attached to an smf and contains events, libsmf will detach the events, free them,
 * detach the track, free it etc.
 *
 * Tracks and events are numbered consecutively, starting from one.  If you remove a track
 * or event, the rest of tracks/events will get renumbered.  To get the number of a given
 * event in its track, use event->event_number.  To get the number of track in its smf, use
 * track->track_number.  To get the number of events in the track, use track->number_of_events.
 * To get the number of tracks in the smf, use smf->number_of_tracks.
 *
 * In SMF File Format, each track has to end with End Of Track metaevent.  If you load SMF file
 * using smf_load(), that will be the case.  If you want to create or edit an SMF, you don't
 * need to worry about EOT events; libsmf automatically takes care of them for you.  If you
 * try to save an SMF with tracks that do not end with EOTs, smf_save() will append them.
 * If you try to add event that happens after EOT metaevent, libsmf will remove the EOT.
 * If you want to add EOT manually, you can, of course, using smf_track_add_eot_seconds()
 * or smf_track_add_eot_pulses().
 *
 * Each event carries three time values - event->time_seconds, which is seconds since
 * the start of the song, event->time_pulses, which is PPQN clocks since the start of
 * the song, and event->delta_pulses, which is PPQN clocks since the previous event
 * in that track.  These values are invalid if the event is not attached to the track.
 * If event is attached, all three values are valid.  Time of the event is specified when
 * adding the event (using smf_track_add_event_seconds(), smf_track_add_event_pulses() or
 * smf_track_add_event_delta_pulses()); the remaining two values are computed from that.
 *
 * Tempo related stuff happens automatically - when you add a metaevent that is Tempo PropertyChange or
 * Time Signature, libsmf adds that event to the tempo map.  If you remove Tempo PropertyChange event
 * that is in the middle of the song, the rest of the events will have their event->time_seconds
 * recomputed from event->time_pulses before smf_event_remove_from_track() function returns.
 * Adding Tempo PropertyChange in the middle of the song works in a similar way.
 *
 * MIDI data (event->midi_buffer) is always kept in normalized form - it always begins with
 * status byte (no running status), there are no System Realtime events embedded in them etc.
 * Events like SysExes are in "on the wire" form, without embedded length that is used in SMF
 * file format.  Obviously libsmf "normalizes" MIDI data during loading and "denormalizes" (adding
 * length to SysExes, escaping System Common and System Realtime messages etc) during writing.
 *
 * Note that you always have to first add the track to smf, and then add events to the track.
 * Doing it the other way around will trip asserts.  Also, try to add events at the end of the
 * track and remove them from the end of the track, that's much more efficient.
 *
 * All the libsmf functions have prefix "smf_".  First argument for routines whose names start
 * with "smf_event_" is "smf_event_t *", for routines whose names start with "smf_track_" -
 * "smf_track_t *", and for plain "smf_" - "smf_t *".  The only exception are smf_whatever_new
 * routines.  Library does not use any global variables and is thread-safe, as long as you
 * don't try to work on the same SMF (smf_t and its descendant tracks and events) from several
 * threads at once without protecting it with mutex.  Library depends on glib and nothing else.
 * License is BSD, two clause, which basically means you can use it freely in your software,
 * both Open Source (including GPL) and closed source.
 *
 */

#ifndef SMF_H
#define SMF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <glib.h>

#if defined(__GNUC__) && __GNUC__ >= 4
#define WARN_UNUSED_RESULT __attribute__ ((warn_unused_result))
#else
#define WARN_UNUSED_RESULT
#endif

/** Represents a "song", that is, collection of one or more tracks. */
struct smf_struct {
	int        format;

	/** These fields are extracted from "division" field of MThd header.
	 * Valid is _either_ ppqn or frames_per_second/resolution. */
	uint16_t   ppqn;
	int        frames_per_second;
	int        resolution;
	int        number_of_tracks;

	/** These are private fields using only by loading and saving routines. */
	FILE      *stream;
	void      *file_buffer;
	size_t     file_buffer_length;
	size_t     next_chunk_offset;
	int        expected_number_of_tracks;

	/** Private, used by smf.c. */
	GPtrArray *tracks_array;
	double     last_seek_position;

	/** Private, used by smf_tempo.c. */
	/** Array of pointers to smf_tempo_struct. */
	GPtrArray *tempo_array;
};

typedef struct smf_struct smf_t;

/** Describes a single tempo or time signature change. */
struct smf_tempo_struct {
	size_t time_pulses;
	double time_seconds;
	int    microseconds_per_quarter_note;
	int    numerator;
	int    denominator;
	int    clocks_per_click;
	int    notes_per_note;
};

typedef struct smf_tempo_struct smf_tempo_t;

/** Represents a single track. */
struct smf_track_struct {
	smf_t     *smf;

	int        track_number;
	size_t     number_of_events;

	/** These are private fields using only by loading and saving routines. */
	void      *file_buffer;
	size_t     file_buffer_length;
	int        last_status; /* Used for "running status". */

	/** Private, used by smf.c. */
	/** Offset into buffer, used in parse_next_event(). */
	size_t     next_event_offset;
	size_t     next_event_number;

	/** Absolute time of next event on events_queue. */
	size_t     time_of_next_event;
	GPtrArray *events_array;
};

typedef struct smf_track_struct smf_track_t;

/** Represents a single MIDI event or metaevent. */
struct smf_event_struct {
	/** Pointer to the track, or NULL if event is not attached. */
	smf_track_t *track;

	/** Number of this event in the track.  Events are numbered consecutively, starting from 1. */
	size_t       event_number;

	/** Note that the time fields are invalid, if event is not attached to a track. */
	/** Time, in pulses, since the previous event on this track. */
	int32_t      delta_time_pulses;

	/** Time, in pulses, since the start of the song. */
	size_t       time_pulses;

	/** Time, in seconds, since the start of the song. */
	double       time_seconds;

	/** Tracks are numbered consecutively, starting from 1. */
	int          track_number;

	/** Pointer to the buffer containing MIDI message.  This is freed by smf_event_delete. */
	uint8_t     *midi_buffer;

	/** Length of the MIDI message in the buffer, in bytes. */
	size_t       midi_buffer_length;
};

typedef struct smf_event_struct smf_event_t;

/* Routines for manipulating smf_t. */
smf_t *smf_new(void) WARN_UNUSED_RESULT;
void   smf_delete(smf_t *smf);

int smf_set_format(smf_t *smf, int format) WARN_UNUSED_RESULT;
int smf_set_ppqn(smf_t *smf, uint16_t ppqn) WARN_UNUSED_RESULT;

char *smf_decode(const smf_t *smf) WARN_UNUSED_RESULT;

smf_track_t *smf_get_track_by_number(const smf_t *smf, int track_number) WARN_UNUSED_RESULT;

smf_event_t *smf_peek_next_event(smf_t *smf) WARN_UNUSED_RESULT;
smf_event_t *smf_get_next_event(smf_t *smf) WARN_UNUSED_RESULT;
void         smf_skip_next_event(smf_t *smf);

void smf_rewind(smf_t *smf);
int  smf_seek_to_seconds(smf_t *smf, double seconds) WARN_UNUSED_RESULT;
int  smf_seek_to_pulses(smf_t *smf, size_t pulses) WARN_UNUSED_RESULT;
int  smf_seek_to_event(smf_t *smf, const smf_event_t *event) WARN_UNUSED_RESULT;

size_t smf_get_length_pulses(const smf_t *smf) WARN_UNUSED_RESULT;
double smf_get_length_seconds(const smf_t *smf) WARN_UNUSED_RESULT;
int    smf_event_is_last(const smf_event_t *event) WARN_UNUSED_RESULT;

void smf_add_track(smf_t *smf, smf_track_t *track);
void smf_track_remove_from_smf(smf_track_t *track);

/* Routines for manipulating smf_track_t. */
smf_track_t *smf_track_new(void) WARN_UNUSED_RESULT;
void         smf_track_delete(smf_track_t *track);

smf_event_t *smf_track_get_next_event(smf_track_t *track) WARN_UNUSED_RESULT;
smf_event_t *smf_track_get_event_by_number(const smf_track_t *track, size_t num) WARN_UNUSED_RESULT;
smf_event_t *smf_track_get_last_event(const smf_track_t *track) WARN_UNUSED_RESULT;

void smf_track_add_event_delta_pulses(smf_track_t *track, smf_event_t *event, uint32_t delta);
void smf_track_add_event_pulses(smf_track_t *track, smf_event_t *event, size_t pulses);
void smf_track_add_event_seconds(smf_track_t *track, smf_event_t *event, double seconds);
int  smf_track_add_eot_delta_pulses(smf_track_t *track, uint32_t delta) WARN_UNUSED_RESULT;
int  smf_track_add_eot_pulses(smf_track_t *track, size_t pulses) WARN_UNUSED_RESULT;
int  smf_track_add_eot_seconds(smf_track_t *track, double seconds) WARN_UNUSED_RESULT;
void smf_event_remove_from_track(smf_event_t *event);

/* Routines for manipulating smf_event_t. */
smf_event_t *smf_event_new(void) WARN_UNUSED_RESULT;
smf_event_t *smf_event_new_from_pointer(const void *midi_data, size_t len) WARN_UNUSED_RESULT;
smf_event_t *smf_event_new_from_bytes(int byte1, int byte2, int byte3) WARN_UNUSED_RESULT;
smf_event_t *smf_event_new_textual(int type, const char *text);
void         smf_event_delete(smf_event_t *event);

int   smf_event_is_valid(const smf_event_t *event) WARN_UNUSED_RESULT;
int   smf_event_is_metadata(const smf_event_t *event) WARN_UNUSED_RESULT;
int   smf_event_is_system_realtime(const smf_event_t *event) WARN_UNUSED_RESULT;
int   smf_event_is_system_common(const smf_event_t *event) WARN_UNUSED_RESULT;
int   smf_event_is_sysex(const smf_event_t *event) WARN_UNUSED_RESULT;
int   smf_event_is_eot(const smf_event_t *event) WARN_UNUSED_RESULT;
int   smf_event_is_textual(const smf_event_t *event) WARN_UNUSED_RESULT;
char *smf_event_decode(const smf_event_t *event) WARN_UNUSED_RESULT;
char *smf_event_extract_text(const smf_event_t *event) WARN_UNUSED_RESULT;

/* Routines for dealing with Variable Length Quantities (VLQ's). 
   Slightly odd names reflect original static names within libsmf
 */
int smf_format_vlq (unsigned char *buf, int length, unsigned long value);
int smf_extract_vlq(const unsigned char *buf, const size_t buffer_length, uint32_t *value, uint32_t *len);

/* Routines for loading SMF files. */
smf_t *smf_load(FILE *) WARN_UNUSED_RESULT;
smf_t *smf_load_from_memory(const void *buffer, const size_t buffer_length) WARN_UNUSED_RESULT;

/* Routine for writing SMF files. */
int smf_save(smf_t *smf, FILE *file) WARN_UNUSED_RESULT;

/* Routines for manipulating smf_tempo_t. */
smf_tempo_t *smf_get_tempo_by_pulses(const smf_t *smf, size_t pulses) WARN_UNUSED_RESULT;
smf_tempo_t *smf_get_tempo_by_seconds(const smf_t *smf, double seconds) WARN_UNUSED_RESULT;
smf_tempo_t *smf_get_tempo_by_number(const smf_t *smf, size_t number) WARN_UNUSED_RESULT;
smf_tempo_t *smf_get_last_tempo(const smf_t *smf) WARN_UNUSED_RESULT;

const char *smf_get_version(void) WARN_UNUSED_RESULT;

#ifdef __cplusplus
}
#endif

#endif /* SMF_H */

