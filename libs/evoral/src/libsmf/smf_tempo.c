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
 * Tempo map related part.
 *
 */

#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include "smf.h"
#include "smf_private.h"

static double seconds_from_pulses(const smf_t *smf, size_t pulses);

/**
 * If there is tempo starting at "pulses" already, return it.  Otherwise,
 * allocate new one, fill it with values from previous one (or default ones,
 * if there is no previous one) and attach it to "smf".
 */
static smf_tempo_t *
new_tempo(smf_t *smf, size_t pulses)
{
	smf_tempo_t *tempo, *previous_tempo = NULL;

	if (smf->tempo_array->len > 0) {
		previous_tempo = smf_get_last_tempo(smf);

		/* If previous tempo starts at the same time as new one, reuse it, updating in place. */
		if (previous_tempo->time_pulses == pulses)
			return (previous_tempo);
	}

	tempo = malloc(sizeof(smf_tempo_t));
	if (tempo == NULL) {
		g_critical("Cannot allocate smf_tempo_t.");
		return (NULL);
	}

	tempo->time_pulses = pulses;

	if (previous_tempo != NULL) {
		tempo->microseconds_per_quarter_note = previous_tempo->microseconds_per_quarter_note;
		tempo->numerator = previous_tempo->numerator;
		tempo->denominator = previous_tempo->denominator;
		tempo->clocks_per_click = previous_tempo->clocks_per_click;
		tempo->notes_per_note = previous_tempo->notes_per_note;
	} else {
		tempo->microseconds_per_quarter_note = 500000; /* Initial tempo is 120 BPM. */
		tempo->numerator = 4;
		tempo->denominator = 4;
		tempo->clocks_per_click = -1;
		tempo->notes_per_note = -1;
	}

	g_ptr_array_add(smf->tempo_array, tempo);

	if (pulses == 0)
		tempo->time_seconds = 0.0;
	else
		tempo->time_seconds = seconds_from_pulses(smf, pulses);

	return (tempo);
}

static int
add_tempo(smf_t *smf, int pulses, int tempo)
{
	smf_tempo_t *smf_tempo = new_tempo(smf, pulses);
	if (smf_tempo == NULL)
		return (-1);

	smf_tempo->microseconds_per_quarter_note = tempo;

	return (0);
}

static int
add_time_signature(smf_t *smf, int pulses, int numerator, int denominator, int clocks_per_click, int notes_per_note)
{
	smf_tempo_t *smf_tempo = new_tempo(smf, pulses);
	if (smf_tempo == NULL)
		return (-1);

	smf_tempo->numerator = numerator;
	smf_tempo->denominator = denominator;
	smf_tempo->clocks_per_click = clocks_per_click;
	smf_tempo->notes_per_note = notes_per_note;

	return (0);
}

/**
 * \internal
 */
void
maybe_add_to_tempo_map(smf_event_t *event)
{
	if (!smf_event_is_metadata(event))
		return;

	assert(event->track != NULL);
	assert(event->track->smf != NULL);
	assert(event->midi_buffer_length >= 1);

	/* Tempo Change? */
	if (event->midi_buffer[1] == 0x51) {
		int ntempo = (event->midi_buffer[3] << 16) + (event->midi_buffer[4] << 8) + event->midi_buffer[5];
		if (ntempo <= 0) {
			g_critical("Ignoring invalid tempo change.");
			return;
		}

		add_tempo(event->track->smf, event->time_pulses, ntempo);
	}

	/* Time Signature? */
	if (event->midi_buffer[1] == 0x58) {
		int numerator, denominator, clocks_per_click, notes_per_note;

		if (event->midi_buffer_length < 7) {
			g_critical("Time Signature event seems truncated.");
			return;
		}

		numerator = event->midi_buffer[3];
		denominator = (int)pow(2, event->midi_buffer[4]);
		clocks_per_click = event->midi_buffer[5];
		notes_per_note = event->midi_buffer[6];

		add_time_signature(event->track->smf, event->time_pulses, numerator, denominator, clocks_per_click, notes_per_note);
	}

	return;
}

/**
 * \internal
 *
 * This is an internal function, called from smf_track_remove_event when tempo-related
 * event being removed does not require recreation of tempo map, i.e. there are no events
 * after that one.
 */
void
remove_last_tempo_with_pulses(smf_t *smf, size_t pulses)
{
	smf_tempo_t *tempo;

	/* XXX: This is a partial workaround for the following problem: we have two tempo-related
	   events, A and B, that occur at the same time.  We remove B, then try to remove
	   A.  However, both tempo changes got coalesced in new_tempo(), so it is impossible
	   to remove B. */
	if (smf->tempo_array->len == 0)
		return;

	tempo = smf_get_last_tempo(smf);

	/* Workaround part two. */
	if (tempo->time_pulses != pulses)
		return;

	memset(tempo, 0, sizeof(smf_tempo_t));
	free(tempo);

	g_ptr_array_remove_index(smf->tempo_array, smf->tempo_array->len - 1);
}

static double
seconds_from_pulses(const smf_t *smf, size_t pulses)
{
	double seconds;
	smf_tempo_t *tempo;

	tempo = smf_get_tempo_by_pulses(smf, pulses);
	assert(tempo);
	assert(tempo->time_pulses <= pulses);

	seconds = tempo->time_seconds + (double)(pulses - tempo->time_pulses) *
		(tempo->microseconds_per_quarter_note / ((double)smf->ppqn * 1000000.0));

	return (seconds);
}

static int
pulses_from_seconds(const smf_t *smf, double seconds)
{
	int pulses = 0;
	smf_tempo_t *tempo;

	tempo = smf_get_tempo_by_seconds(smf, seconds);
	assert(tempo);
	assert(tempo->time_seconds <= seconds);

	pulses = tempo->time_pulses + (seconds - tempo->time_seconds) *
		((double)smf->ppqn * 1000000.0 / tempo->microseconds_per_quarter_note);

	return (pulses);
}

/**
 * \internal
 *
 * Computes value of event->time_seconds for all events in smf.
 * Warning: rewinds the smf.
 */
void
smf_create_tempo_map_and_compute_seconds(smf_t *smf)
{
	smf_event_t *event;

	smf_rewind(smf);
	smf_init_tempo(smf);

	for (;;) {
		event = smf_get_next_event(smf);
		
		if (event == NULL)
			return;

		maybe_add_to_tempo_map(event);

		event->time_seconds = seconds_from_pulses(smf, event->time_pulses);
	}

	/* Not reached. */
}

smf_tempo_t *
smf_get_tempo_by_number(const smf_t *smf, size_t number)
{
	if (number >= smf->tempo_array->len)
		return (NULL);

	return (g_ptr_array_index(smf->tempo_array, number));
}

/**
 * Return last tempo (i.e. tempo with greatest time_pulses) that happens before "pulses".
 */
smf_tempo_t *
smf_get_tempo_by_pulses(const smf_t *smf, size_t pulses)
{
	size_t i;
	smf_tempo_t *tempo;

	if (pulses == 0)
		return (smf_get_tempo_by_number(smf, 0));

	assert(smf->tempo_array != NULL);
	
	for (i = smf->tempo_array->len; i > 0; i--) {
		tempo = smf_get_tempo_by_number(smf, i - 1);

		assert(tempo);
		if (tempo->time_pulses < pulses)
			return (tempo);
	}

	return (NULL);
}

/**
 * Return last tempo (i.e. tempo with greatest time_seconds) that happens before "seconds".
 */
smf_tempo_t *
smf_get_tempo_by_seconds(const smf_t *smf, double seconds)
{
	size_t i;
	smf_tempo_t *tempo;

	assert(seconds >= 0.0);

	if (seconds == 0.0)
		return (smf_get_tempo_by_number(smf, 0));

	assert(smf->tempo_array != NULL);
	
	for (i = smf->tempo_array->len; i > 0; i--) {
		tempo = smf_get_tempo_by_number(smf, i - 1);

		assert(tempo);
		if (tempo->time_seconds < seconds)
			return (tempo);
	}

	return (NULL);
}


/**
 * Return last tempo.
 */
smf_tempo_t *
smf_get_last_tempo(const smf_t *smf)
{
	smf_tempo_t *tempo;

	assert(smf->tempo_array->len > 0);
	tempo = smf_get_tempo_by_number(smf, smf->tempo_array->len - 1);
	assert(tempo);

	return (tempo);
}

/**
 * \internal 
 *
 * Remove all smf_tempo_t structures from SMF.
 */
void
smf_fini_tempo(smf_t *smf)
{
	smf_tempo_t *tempo;

	while (smf->tempo_array->len > 0) {
		tempo = g_ptr_array_index(smf->tempo_array, smf->tempo_array->len - 1);
		assert(tempo);

		memset(tempo, 0, sizeof(smf_tempo_t));
		free(tempo);

		g_ptr_array_remove_index(smf->tempo_array, smf->tempo_array->len - 1);
	}

	assert(smf->tempo_array->len == 0);
}

/**
 * \internal
 *
 * Remove any existing tempos and add default one.
 *
 * \bug This will abort (by calling g_error) if new_tempo() (memory allocation there) fails.
 */
void
smf_init_tempo(smf_t *smf)
{
	smf_tempo_t *tempo;

	smf_fini_tempo(smf);

	tempo = new_tempo(smf, 0);
	if (tempo == NULL) {
		g_error("tempo_init failed, sorry.");
	}
}

/**
 * Returns ->time_pulses of last event on the given track, or 0, if track is empty.
 */
static int
last_event_pulses(const smf_track_t *track)
{
	/* Get time of last event on this track. */
	if (track->number_of_events > 0) {
		smf_event_t *previous_event = smf_track_get_last_event(track);
		assert(previous_event);

		return (previous_event->time_pulses);
	}

	return (0);
}

/**
 * Adds event to the track at the time "pulses" clocks from the previous event in this track.
 * The remaining two time fields will be computed automatically based on the third argument
 * and current tempo map.  Note that ->delta_pulses is computed by smf.c:smf_track_add_event,
 * not here.
 */
void
smf_track_add_event_delta_pulses(smf_track_t *track, smf_event_t *event, uint32_t delta)
{
	assert(event->time_seconds == -1.0);
	assert(track->smf != NULL);

	if (!smf_event_is_valid(event)) {
		g_critical("Added event is invalid");
	}

	smf_track_add_event_pulses(track, event, last_event_pulses(track) + delta);
}

/**
 * Adds event to the track at the time "pulses" clocks from the start of song.
 * The remaining two time fields will be computed automatically based on the third argument
 * and current tempo map.
 */
void
smf_track_add_event_pulses(smf_track_t *track, smf_event_t *event, size_t pulses)
{
	assert(event->time_seconds == -1.0);
	assert(track->smf != NULL);

	event->time_pulses = pulses;
	event->time_seconds = seconds_from_pulses(track->smf, pulses);
	smf_track_add_event(track, event);
}

/**
 * Adds event to the track at the time "seconds" seconds from the start of song.
 * The remaining two time fields will be computed automatically based on the third argument
 * and current tempo map.
 */
void
smf_track_add_event_seconds(smf_track_t *track, smf_event_t *event, double seconds)
{
	assert(seconds >= 0.0);
	assert(event->time_seconds == -1.0);
	assert(track->smf != NULL);

	event->time_seconds = seconds;
	event->time_pulses = pulses_from_seconds(track->smf, seconds);
	smf_track_add_event(track, event);
}

