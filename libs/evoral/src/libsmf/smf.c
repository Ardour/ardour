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
 * Various functions.
 *
 */

/* Reference: http://www.borg.com/~jglatt/tech/midifile.htm */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <errno.h>
#ifdef WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include "smf.h"
#include "smf_private.h"

/**
 * Allocates new smf_t structure.
 * \return pointer to smf_t or NULL.
 */
smf_t *
smf_new(void)
{
	int cantfail;

	smf_t *smf = (smf_t*)malloc(sizeof(smf_t));
	if (smf == NULL) {
		g_critical("Cannot allocate smf_t structure: %s", strerror(errno));
		return (NULL);
	}

	memset(smf, 0, sizeof(smf_t));

	smf->tracks_array = g_ptr_array_new();
	assert(smf->tracks_array);

	smf->tempo_array = g_ptr_array_new();
	assert(smf->tempo_array);

	cantfail = smf_set_ppqn(smf, 120);
	assert(!cantfail);

	cantfail = smf_set_format(smf, 0);
	assert(!cantfail);

	smf_init_tempo(smf);

	return (smf);
}

/**
 * Frees smf and all it's descendant structures.
 */
void
smf_delete(smf_t *smf)
{
	/* Remove all the tracks, from last to first. */
	while (smf->tracks_array->len > 0)
		smf_track_delete((smf_track_t*)g_ptr_array_index(smf->tracks_array, smf->tracks_array->len - 1));

	smf_fini_tempo(smf);

	assert(smf->tracks_array->len == 0);
	assert(smf->number_of_tracks == 0);
	g_ptr_array_free(smf->tracks_array, TRUE);
	g_ptr_array_free(smf->tempo_array, TRUE);

	memset(smf, 0, sizeof(smf_t));
	free(smf);
}

/**
 * Allocates new smf_track_t structure.
 * \return pointer to smf_track_t or NULL.
 */
smf_track_t *
smf_track_new(void)
{
	smf_track_t *track = (smf_track_t*)malloc(sizeof(smf_track_t));
	if (track == NULL) {
		g_critical("Cannot allocate smf_track_t structure: %s", strerror(errno));
		return (NULL);
	}

	memset(track, 0, sizeof(smf_track_t));
	track->next_event_number = 0;

	track->events_array = g_ptr_array_new();
	assert(track->events_array);

	return (track);
}

/**
 * Detaches track from its smf and frees it.
 */
void
smf_track_delete(smf_track_t *track)
{
	assert(track);
	assert(track->events_array);

	/* Remove all the events, from last to first. */
	while (track->events_array->len > 0)
		smf_event_delete((smf_event_t*)g_ptr_array_index(track->events_array, track->events_array->len - 1));

	if (track->smf)
		smf_track_remove_from_smf(track);

	assert(track->events_array->len == 0);
	assert(track->number_of_events == 0);
	g_ptr_array_free(track->events_array, TRUE);

	memset(track, 0, sizeof(smf_track_t));
	free(track);
}


/**
 * Appends smf_track_t to smf.
 */
void
smf_add_track(smf_t *smf, smf_track_t *track)
{
	int cantfail;

	assert(track->smf == NULL);

	track->smf = smf;
	g_ptr_array_add(smf->tracks_array, track);

	smf->number_of_tracks++;
	track->track_number = smf->number_of_tracks;

	if (smf->number_of_tracks > 1) {
		cantfail = smf_set_format(smf, 1);
		assert(!cantfail);
	}
}

/**
 * Detaches track from the smf.
 */
void
smf_track_remove_from_smf(smf_track_t *track)
{
	int i;
	size_t j;
	smf_track_t *tmp;
	smf_event_t *ev;

	assert(track->smf != NULL);

	track->smf->number_of_tracks--;

	assert(track->smf->tracks_array);
	g_ptr_array_remove(track->smf->tracks_array, track);

	/* Renumber the rest of the tracks, so they are consecutively numbered. */
	for (i = track->track_number; i <= track->smf->number_of_tracks; i++) {
		tmp = smf_get_track_by_number(track->smf, i);
		tmp->track_number = i;

		/*
		 * Events have track numbers too.  I guess this wasn't a wise
		 * decision.  ;-/
		 */
		for (j = 1; j <= tmp->number_of_events; j++) {
			ev = smf_track_get_event_by_number(tmp, j);
			ev->track_number = i;
		}
	}

	track->track_number = -1;
	track->smf = NULL;
}

/**
 * Allocates new smf_event_t structure.  The caller is responsible for allocating
 * event->midi_buffer, filling it with MIDI data and setting event->midi_buffer_length properly.
 * Note that event->midi_buffer will be freed by smf_event_delete.
 * \return pointer to smf_event_t or NULL.
 */
smf_event_t *
smf_event_new(void)
{
	smf_event_t *event = (smf_event_t*)malloc(sizeof(smf_event_t));
	if (event == NULL) {
		g_critical("Cannot allocate smf_event_t structure: %s", strerror(errno));
		return (NULL);
	}

	memset(event, 0, sizeof(smf_event_t));

	event->delta_time_pulses = -1;
	event->time_pulses = -1;
	event->time_seconds = -1.0;
	event->track_number = -1;

	return (event);
}

/**
 * Allocates an smf_event_t structure and fills it with "len" bytes copied
 * from "midi_data".
 * \param midi_data Pointer to MIDI data.  It sill be copied to the newly allocated event->midi_buffer.
 * \param len Length of the buffer.  It must be proper MIDI event length, e.g. 3 for Note On event.
 * \return Event containing MIDI data or NULL.
 */
smf_event_t *
smf_event_new_from_pointer(const void *midi_data, size_t len)
{
	smf_event_t *event;

	event = smf_event_new();
	if (event == NULL)
		return (NULL);

	event->midi_buffer_length = len;
	event->midi_buffer = (uint8_t*)malloc(event->midi_buffer_length);
	if (event->midi_buffer == NULL) {
		g_critical("Cannot allocate MIDI buffer structure: %s", strerror(errno));
		smf_event_delete(event);

		return (NULL);
	}

	memcpy(event->midi_buffer, midi_data, len);

	return (event);
}

/**
 * Allocates an smf_event_t structure and fills it with at most three bytes of data.
 * For example, if you need to create Note On event, do something like this:
 *
 * smf_event_new_from_bytes(0x90, 0x3C, 0x7f);
 *
 * To create event for MIDI message that is shorter than three bytes, do something
 * like this:
 *
 * smf_event_new_from_bytes(0xC0, 0x42, -1);
 *
 * \param first_byte First byte of MIDI message.  Must be valid status byte.
 * \param second_byte Second byte of MIDI message or -1, if message is one byte long.
 * \param third_byte Third byte of MIDI message or -1, if message is two bytes long.
 * \return Event containing MIDI data or NULL.
 */
smf_event_t *
smf_event_new_from_bytes(int first_byte, int second_byte, int third_byte)
{
	size_t len;

	smf_event_t *event;

	event = smf_event_new();
	if (event == NULL)
		return (NULL);

	if (first_byte < 0) {
		g_critical("First byte of MIDI message cannot be < 0");
		smf_event_delete(event);

		return (NULL);
	}

	if (first_byte > 255) {
		g_critical("smf_event_new_from_bytes: first byte is %d, which is larger than 255.", first_byte);
		return (NULL);
	}

	if (!is_status_byte(first_byte)) {
		g_critical("smf_event_new_from_bytes: first byte is not a valid status byte.");
		return (NULL);
	}


	if (second_byte < 0)
		len = 1;
	else if (third_byte < 0)
		len = 2;
	else
		len = 3;

	if (len > 1) {
		if (second_byte > 255) {
			g_critical("smf_event_new_from_bytes: second byte is %d, which is larger than 255.", second_byte);
			return (NULL);
		}

		if (is_status_byte(second_byte)) {
			g_critical("smf_event_new_from_bytes: second byte cannot be a status byte.");
			return (NULL);
		}
	}

	if (len > 2) {
		if (third_byte > 255) {
			g_critical("smf_event_new_from_bytes: third byte is %d, which is larger than 255.", third_byte);
			return (NULL);
		}

		if (is_status_byte(third_byte)) {
			g_critical("smf_event_new_from_bytes: third byte cannot be a status byte.");
			return (NULL);
		}
	}

	event->midi_buffer_length = len;
	event->midi_buffer = (uint8_t*)malloc(event->midi_buffer_length);
	if (event->midi_buffer == NULL) {
		g_critical("Cannot allocate MIDI buffer structure: %s", strerror(errno));
		smf_event_delete(event);

		return (NULL);
	}

	event->midi_buffer[0] = first_byte;
	if (len > 1)
		event->midi_buffer[1] = second_byte;
	if (len > 2)
		event->midi_buffer[2] = third_byte;

	return (event);
}

/**
 * Detaches event from its track and frees it.
 */
void
smf_event_delete(smf_event_t *event)
{
	if (event->track != NULL)
		smf_event_remove_from_track(event);

	if (event->midi_buffer != NULL) {
		memset(event->midi_buffer, 0, event->midi_buffer_length);
		free(event->midi_buffer);
	}

	memset(event, 0, sizeof(smf_event_t));
	free(event);
}

/**
 * Used for sorting track->events_array.
 */
static gint
events_array_compare_function(gconstpointer aa, gconstpointer bb)
{
	const smf_event_t *a, *b;

	/* "The comparison function for g_ptr_array_sort() doesn't take the pointers
	    from the array as arguments, it takes pointers to the pointers in the array." */
	a = (const smf_event_t *)*(const gpointer *)aa;
	b = (const smf_event_t *)*(const gpointer *)bb;

	if (a->time_pulses < b->time_pulses)
		return (-1);

	if (a->time_pulses > b->time_pulses)
		return (1);

	/*
	 * We need to preserve original order, otherwise things will break
	 * when there are several events with the same ->time_pulses.
	 * XXX: This is an ugly hack; we should remove sorting altogether.
	 */

	if (a->event_number < b->event_number)
		return (-1);

	if (a->event_number > b->event_number)
		return (1);

	return (0);
}

/*
 * An assumption here is that if there is an EOT event, it will be at the end of the track.
 */
static void
remove_eot_if_before_pulses(smf_track_t *track, size_t pulses)
{
	smf_event_t *event;

	event = smf_track_get_last_event(track);

	if (event == NULL)
		return;

	if (!smf_event_is_eot(event))
		return;

	if (event->time_pulses > pulses)
		return;

	smf_event_remove_from_track(event);
}

/**
 * Adds the event to the track and computes ->delta_pulses.  Note that it is faster
 * to append events to the end of the track than to insert them in the middle.
 * Usually you want to use smf_track_add_event_seconds or smf_track_add_event_pulses
 * instead of this one.  Event needs to have ->time_pulses and ->time_seconds already set.
 * If you try to add event after an EOT, EOT event will be automatically deleted.
 */
void
smf_track_add_event(smf_track_t *track, smf_event_t *event)
{
	size_t i, last_pulses = 0;

	assert(track->smf != NULL);
	assert(event->track == NULL);
	assert(event->delta_time_pulses == -1);
	assert(event->time_seconds >= 0.0);

	remove_eot_if_before_pulses(track, event->time_pulses);

	event->track = track;
	event->track_number = track->track_number;

	if (track->number_of_events == 0) {
		assert(track->next_event_number == 0);
		track->next_event_number = 1;
	}

	if (track->number_of_events > 0)
		last_pulses = smf_track_get_last_event(track)->time_pulses;

	track->number_of_events++;

	/* Are we just appending element at the end of the track? */
	if (last_pulses <= event->time_pulses) {
		event->delta_time_pulses = event->time_pulses - last_pulses;
		assert(event->delta_time_pulses >= 0);
		g_ptr_array_add(track->events_array, event);
		event->event_number = track->number_of_events;

	/* We need to insert in the middle of the track.  XXX: This is slow. */
	} else {
		/* Append, then sort according to ->time_pulses. */
		g_ptr_array_add(track->events_array, event);
		g_ptr_array_sort(track->events_array, events_array_compare_function);

		/* Renumber entries and fix their ->delta_pulses. */
		for (i = 1; i <= track->number_of_events; i++) {
			smf_event_t *tmp = smf_track_get_event_by_number(track, i);
			tmp->event_number = i;

			if (tmp->delta_time_pulses != -1)
				continue;

			if (i == 1) {
				tmp->delta_time_pulses = tmp->time_pulses;
			} else {
				tmp->delta_time_pulses = tmp->time_pulses -
					smf_track_get_event_by_number(track, i - 1)->time_pulses;
				assert(tmp->delta_time_pulses >= 0);
			}
		}

		/* Adjust ->delta_time_pulses of the next event. */
		if (event->event_number < track->number_of_events) {
			smf_event_t *next_event = smf_track_get_event_by_number(track, event->event_number + 1);
			assert(next_event);
			assert(next_event->time_pulses >= event->time_pulses);
			next_event->delta_time_pulses -= event->delta_time_pulses;
			assert(next_event->delta_time_pulses >= 0);
		}
	}

	if (smf_event_is_tempo_change_or_time_signature(event)) {
		if (smf_event_is_last(event))
			maybe_add_to_tempo_map(event);
		else
			smf_create_tempo_map_and_compute_seconds(event->track->smf);
	}
}

/**
 * Add End Of Track metaevent.  Using it is optional, libsmf will automatically
 * add EOT to the tracks during smf_save, with delta_pulses 0.  If you try to add EOT
 * in the middle of the track, it will fail and nonzero value will be returned.
 * If you try to add EOT after another EOT event, it will be added, but the existing
 * EOT event will be removed.
 *
 * \return 0 if everything went ok, nonzero otherwise.
 */
int
smf_track_add_eot_delta_pulses(smf_track_t *track, uint32_t delta)
{
	smf_event_t *event;

	event = smf_event_new_from_bytes(0xFF, 0x2F, 0x00);
	if (event == NULL)
		return (-1);

	smf_track_add_event_delta_pulses(track, event, delta);

	return (0);
}

int
smf_track_add_eot_pulses(smf_track_t *track, size_t pulses)
{
	smf_event_t *event, *last_event;

	last_event = smf_track_get_last_event(track);
	if (last_event != NULL) {
		if (last_event->time_pulses > pulses)
			return (-2);
	}

	event = smf_event_new_from_bytes(0xFF, 0x2F, 0x00);
	if (event == NULL)
		return (-3);

	smf_track_add_event_pulses(track, event, pulses);

	return (0);
}

int
smf_track_add_eot_seconds(smf_track_t *track, double seconds)
{
	smf_event_t *event, *last_event;

	last_event = smf_track_get_last_event(track);
	if (last_event != NULL) {
		if (last_event->time_seconds > seconds)
			return (-2);
	}

	event = smf_event_new_from_bytes(0xFF, 0x2F, 0x00);
	if (event == NULL)
		return (-1);

	smf_track_add_event_seconds(track, event, seconds);

	return (0);
}

/**
 * Detaches event from its track.
 */
void
smf_event_remove_from_track(smf_event_t *event)
{
	size_t i;
	int was_last;
	smf_event_t *tmp;
	smf_track_t *track;

	assert(event->track != NULL);
	assert(event->track->smf != NULL);

	track = event->track;
	was_last = smf_event_is_last(event);

	/* Adjust ->delta_time_pulses of the next event. */
	if (event->event_number < track->number_of_events) {
		tmp = smf_track_get_event_by_number(track, event->event_number + 1);
		assert(tmp);
		tmp->delta_time_pulses += event->delta_time_pulses;
	}

	track->number_of_events--;
	g_ptr_array_remove(track->events_array, event);

	if (track->number_of_events == 0)
		track->next_event_number = 0;

	/* Renumber the rest of the events, so they are consecutively numbered. */
	for (i = event->event_number; i <= track->number_of_events; i++) {
		tmp = smf_track_get_event_by_number(track, i);
		tmp->event_number = i;
	}

	if (smf_event_is_tempo_change_or_time_signature(event)) {
		/* XXX: This will cause problems, when there is more than one Tempo Change event at a given time. */
		if (was_last)
			remove_last_tempo_with_pulses(event->track->smf, event->time_pulses);
		else
			smf_create_tempo_map_and_compute_seconds(track->smf);
	}

	event->track = NULL;
	event->event_number = 0;
	event->delta_time_pulses = -1;
	event->time_pulses = 0;
	event->time_seconds = -1.0;
}

/**
  * \return Nonzero if event is Tempo Change or Time Signature metaevent.
  */
int
smf_event_is_tempo_change_or_time_signature(const smf_event_t *event)
{
	if (!smf_event_is_metadata(event))
		return (0);

	assert(event->midi_buffer_length >= 2);

	if (event->midi_buffer[1] == 0x51 || event->midi_buffer[1] == 0x58)
		return (1);

	return (0);
}

/**
  * Sets "Format" field of MThd header to the specified value.  Note that you
  * don't really need to use this, as libsmf will automatically change format
  * from 0 to 1 when you add the second track.
  * \param smf SMF.
  * \param format 0 for one track per file, 1 for several tracks per file.
  */
int
smf_set_format(smf_t *smf, int format)
{
	assert(format == 0 || format == 1);

	if (smf->number_of_tracks > 1 && format == 0) {
		g_critical("There is more than one track, cannot set format to 0.");
		return (-1);
	}

	smf->format = format;

	return (0);
}

/**
  * Sets the PPQN ("Division") field of MThd header.  This is mandatory, you
  * should call it right after smf_new.  Note that changing PPQN will change time_seconds
  * of all the events.
  * \param smf SMF.
  * \param ppqn New PPQN.
  */
int
smf_set_ppqn(smf_t *smf, uint16_t ppqn)
{
	smf->ppqn = ppqn;

	return (0);
}

/**
  * Returns next event from the track given and advances next event counter.
  * Do not depend on End Of Track event being the last event on the track - it
  * is possible that the track will not end with EOT if you haven't added it
  * yet.  EOTs are added automatically during smf_save().
  *
  * \return Event or NULL, if there are no more events left in this track.
  */
smf_event_t *
smf_track_get_next_event(smf_track_t *track)
{
	smf_event_t *event, *next_event;

	/* Track is empty? */
	if (track->number_of_events == 0)
		return (NULL);

	/* End of track? */
	if (track->next_event_number == 0)
		return (NULL);

	assert(track->next_event_number >= 1);

	event = smf_track_get_event_by_number(track, track->next_event_number);

	assert(event != NULL);

	/* Is this the last event in the track? */
	if (track->next_event_number < track->number_of_events) {
		next_event = smf_track_get_event_by_number(track, track->next_event_number + 1);
		assert(next_event);

		track->time_of_next_event = next_event->time_pulses;
		track->next_event_number++;
	} else {
		track->next_event_number = 0;
	}

	return (event);
}

/**
  * Returns next event from the track given.  Does not change next event counter,
  * so repeatedly calling this routine will return the same event.
  * \return Event or NULL, if there are no more events left in this track.
  */
static smf_event_t *
smf_peek_next_event_from_track(smf_track_t *track)
{
	smf_event_t *event;

	/* End of track? */
	if (track->next_event_number == 0)
		return (NULL);

	assert(track->next_event_number >= 1);
	assert(track->events_array->len != 0);

	event = smf_track_get_event_by_number(track, track->next_event_number);

	return (event);
}

/**
 * \return Track with a given number or NULL, if there is no such track.
 * Tracks are numbered consecutively starting from one.
 */
smf_track_t *
smf_get_track_by_number(const smf_t *smf, int track_number)
{
	smf_track_t *track;

	assert(track_number >= 1);

	if (track_number > smf->number_of_tracks)
		return (NULL);

	track = (smf_track_t *)g_ptr_array_index(smf->tracks_array, track_number - 1);

	assert(track);

	return (track);
}

/**
 * \return Event with a given number or NULL, if there is no such event.
 * Events are numbered consecutively starting from one.
 */
smf_event_t *
smf_track_get_event_by_number(const smf_track_t *track, size_t event_number)
{
	smf_event_t *event;

	assert(event_number >= 1);

	if (event_number > track->number_of_events)
		return (NULL);

	event = (smf_event_t*)g_ptr_array_index(track->events_array, event_number - 1);

	assert(event);

	return (event);
}

/**
 * \return Last event on the track or NULL, if track is empty.
 */
smf_event_t *
smf_track_get_last_event(const smf_track_t *track)
{
	smf_event_t *event;

	if (track->number_of_events == 0)
		return (NULL);

	event = smf_track_get_event_by_number(track, track->number_of_events);

	return (event);
}

/**
 * Searches for track that contains next event, in time order.  In other words,
 * returns the track that contains event that should be played next.
 * \return Track with next event or NULL, if there are no events left.
 */
smf_track_t *
smf_find_track_with_next_event(smf_t *smf)
{
	int i;
	size_t min_time = 0;
	smf_track_t *track = NULL, *min_time_track = NULL;

	/* Find track with event that should be played next. */
	for (i = 1; i <= smf->number_of_tracks; i++) {
		track = smf_get_track_by_number(smf, i);

		assert(track);

		/* No more events in this track? */
		if (track->next_event_number == 0)
			continue;

		if (track->time_of_next_event < min_time || min_time_track == NULL) {
			min_time = track->time_of_next_event;
			min_time_track = track;
		}
	}

	return (min_time_track);
}

/**
  * \return Next event, in time order, or NULL, if there are none left.
  */
smf_event_t *
smf_get_next_event(smf_t *smf)
{
	smf_event_t *event;
	smf_track_t *track = smf_find_track_with_next_event(smf);

	if (track == NULL) {
#if 0
		g_debug("End of the song.");
#endif

		return (NULL);
	}

	event = smf_track_get_next_event(track);

	assert(event != NULL);

	event->track->smf->last_seek_position = -1.0;

	return (event);
}

/**
  * Advance the "next event counter".  This is functionally the same as calling
  * smf_get_next_event and ignoring the return value.
  */
void
smf_skip_next_event(smf_t *smf)
{
	void *notused;

	notused = smf_get_next_event(smf);
}

/**
  * \return Next event, in time order, or NULL, if there are none left.  Does
  * not advance position in song.
  */
smf_event_t *
smf_peek_next_event(smf_t *smf)
{
	smf_event_t *event;
	smf_track_t *track = smf_find_track_with_next_event(smf);

	if (track == NULL) {
#if 0
		g_debug("End of the song.");
#endif

		return (NULL);
	}

	event = smf_peek_next_event_from_track(track);

	assert(event != NULL);

	return (event);
}

/**
  * Rewinds the SMF.  What that means is, after calling this routine, smf_get_next_event
  * will return first event in the song.
  */
void
smf_rewind(smf_t *smf)
{
	int i;
	smf_track_t *track = NULL;
	smf_event_t *event;

	assert(smf);

	smf->last_seek_position = 0.0;

	for (i = 1; i <= smf->number_of_tracks; i++) {
		track = smf_get_track_by_number(smf, i);

		assert(track != NULL);

		if (track->number_of_events > 0) {
			track->next_event_number = 1;
			event = smf_peek_next_event_from_track(track);
			assert(event);
			track->time_of_next_event = event->time_pulses;
		} else {
			track->next_event_number = 0;
			track->time_of_next_event = 0;
#if 0
			g_warning("Warning: empty track.");
#endif
		}
	}
}

/**
  * Seeks the SMF to the given event.  After calling this routine, smf_get_next_event
  * will return the event that was the second argument of this call.
  */
int
smf_seek_to_event(smf_t *smf, const smf_event_t *target)
{
	smf_event_t *event;

	smf_rewind(smf);

#if 0
	g_debug("Seeking to event %d, track %d.", target->event_number, target->track->track_number);
#endif

	for (;;) {
		event = smf_peek_next_event(smf);

		/* There can't be NULL here, unless "target" is not in this smf. */
		assert(event);

		if (event != target)
			smf_skip_next_event(smf);
		else
			break;
	}

	smf->last_seek_position = event->time_seconds;

	return (0);
}

/**
  * Seeks the SMF to the given position.  For example, after seeking to 1.0 seconds,
  * smf_get_next_event will return first event that happens after the first second of song.
  */
int
smf_seek_to_seconds(smf_t *smf, double seconds)
{
	smf_event_t *event;

	assert(seconds >= 0.0);

	if (seconds == smf->last_seek_position) {
#if 0
		g_debug("Avoiding seek to %f seconds.", seconds);
#endif
		return (0);
	}

	smf_rewind(smf);

#if 0
	g_debug("Seeking to %f seconds.", seconds);
#endif

	for (;;) {
		event = smf_peek_next_event(smf);

		if (event == NULL) {
			g_critical("Trying to seek past the end of song.");
			return (-1);
		}

		if (event->time_seconds < seconds)
			smf_skip_next_event(smf);
		else
			break;
	}

	smf->last_seek_position = seconds;

	return (0);
}

/**
  * Seeks the SMF to the given position.  For example, after seeking to 10 pulses,
  * smf_get_next_event will return first event that happens after the first ten pulses.
  */
int
smf_seek_to_pulses(smf_t *smf, size_t pulses)
{
	smf_event_t *event;

	smf_rewind(smf);

#if 0
	g_debug("Seeking to %d pulses.", pulses);
#endif

	for (;;) {
		event = smf_peek_next_event(smf);

		if (event == NULL) {
			g_critical("Trying to seek past the end of song.");
			return (-1);
		}

		if (event->time_pulses < pulses)
			smf_skip_next_event(smf);
		else
			break;
	}

	smf->last_seek_position = event->time_seconds;

	return (0);
}

/**
  * \return Length of SMF, in pulses.
  */
size_t
smf_get_length_pulses(const smf_t *smf)
{
	int i;
	size_t pulses = 0;

	for (i = 1; i <= smf->number_of_tracks; i++) {
		smf_track_t *track;
		smf_event_t *event;

		track = smf_get_track_by_number(smf, i);
		assert(track);

		event = smf_track_get_last_event(track);
		/* Empty track? */
		if (event == NULL)
			continue;

		if (event->time_pulses > pulses)
			pulses = event->time_pulses;
	}

	return (pulses);
}

/**
  * \return Length of SMF, in seconds.
  */
double
smf_get_length_seconds(const smf_t *smf)
{
	int i;
	double seconds = 0.0;

	for (i = 1; i <= smf->number_of_tracks; i++) {
		smf_track_t *track;
		smf_event_t *event;

	       	track = smf_get_track_by_number(smf, i);
		assert(track);

		event = smf_track_get_last_event(track);
		/* Empty track? */
		if (event == NULL)
			continue;

		if (event->time_seconds > seconds)
			seconds = event->time_seconds;
	}

	return (seconds);
}

/**
  * \return Nonzero, if there are no events in the SMF after this one.
  * Note that may be more than one "last event", if they occur at the same time.
  */
int
smf_event_is_last(const smf_event_t *event)
{
	if (smf_get_length_pulses(event->track->smf) <= event->time_pulses)
		return (1);

	return (0);
}

/**
  * \return Version of libsmf.
  */
const char *
smf_get_version(void)
{
	return (SMF_VERSION);
}

