/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#ifndef _FLUID_EVENT_QUEUE_H
#define _FLUID_EVENT_QUEUE_H

#include "fluid_sys.h"
#include "fluid_midi.h"
#include "fluid_ringbuffer.h"

/**
 * Type of queued event.
 */
enum fluid_event_queue_elem
{
  FLUID_EVENT_QUEUE_ELEM_MIDI,          /**< MIDI event. Uses midi field of event value */
  FLUID_EVENT_QUEUE_ELEM_UPDATE_GAIN,   /**< Update synthesizer gain.  No payload value */
  FLUID_EVENT_QUEUE_ELEM_POLYPHONY,     /**< Synth polyphony event. No payload value */
  FLUID_EVENT_QUEUE_ELEM_GEN,           /**< Generator event. Uses gen field of event value */
  FLUID_EVENT_QUEUE_ELEM_PRESET,        /**< Preset set event. Uses preset field of event value */
  FLUID_EVENT_QUEUE_ELEM_STOP_VOICES,   /**< Stop voices event. Uses ival field of event value */
  FLUID_EVENT_QUEUE_ELEM_FREE_PRESET,   /**< Free preset return event. Uses pval field of event value */
  FLUID_EVENT_QUEUE_ELEM_SET_TUNING,    /**< Set tuning event. Uses set_tuning field of event value */
  FLUID_EVENT_QUEUE_ELEM_REPL_TUNING,   /**< Replace tuning event. Uses repl_tuning field of event value */
  FLUID_EVENT_QUEUE_ELEM_UNREF_TUNING  /**< Unref tuning return event. Uses unref_tuning field of event value */
};

/**
 * SoundFont generator set event structure.
 */
typedef struct
{
  int channel;          /**< MIDI channel number */
  int param;            /**< FluidSynth generator ID */
  float value;          /**< Value for the generator (absolute or relative) */
  int absolute;         /**< 1 if value is absolute, 0 if relative */
} fluid_event_gen_t;

/**
 * Preset channel assignment event structure.
 */
typedef struct
{
  int channel;                  /**< MIDI channel number */
  fluid_preset_t *preset;       /**< Preset to assign (synth thread owns) */
} fluid_event_preset_t;

/**
 * Tuning assignment event structure.
 */
typedef struct
{
  char apply;                   /**< TRUE to set tuning in realtime */
  int channel;                  /**< MIDI channel number */
  fluid_tuning_t *tuning;       /**< Tuning to assign */
} fluid_event_set_tuning_t;

/**
 * Tuning replacement event structure.
 */
typedef struct
{
  char apply;                       /**< TRUE if tuning change should be applied in realtime */
  fluid_tuning_t *old_tuning;       /**< Old tuning pointer to replace */
  fluid_tuning_t *new_tuning;       /**< New tuning to assign */
} fluid_event_repl_tuning_t;

/**
 * Tuning unref event structure.
 */
typedef struct
{
  fluid_tuning_t *tuning;           /**< Tuning to unref */
  int count;                        /**< Number of times to unref */
} fluid_event_unref_tuning_t;

/**
 * Structure for an integer parameter sent to a MIDI channel (bank or SoundFont ID for example).
 */
typedef struct
{
  int channel;
  int val;
} fluid_event_channel_int_t;

/**
 * Event queue element structure.
 */
typedef struct
{
  char type;            /**< fluid_event_queue_elem */

  union
  {
    fluid_midi_event_t midi;    /**< If type == FLUID_EVENT_QUEUE_ELEM_MIDI */
    fluid_event_gen_t gen;      /**< If type == FLUID_EVENT_QUEUE_ELEM_GEN */
    fluid_event_preset_t preset;        /**< If type == FLUID_EVENT_QUEUE_ELEM_PRESET */
    fluid_event_set_tuning_t set_tuning;        /**< If type == FLUID_EVENT_QUEUE_ELEM_SET_TUNING */
    fluid_event_repl_tuning_t repl_tuning;      /**< If type == FLUID_EVENT_QUEUE_ELEM_REPL_TUNING */
    fluid_event_unref_tuning_t unref_tuning;    /**< If type == FLUID_EVENT_QUEUE_ELEM_UNREF_TUNING */
    double dval;                /**< A floating point payload value */
    int ival;                   /**< An integer payload value */
    void *pval;                 /**< A pointer payload value */
  };
} fluid_event_queue_elem_t;

typedef struct _fluid_ringbuffer_t fluid_event_queue_t;

static FLUID_INLINE fluid_event_queue_t *
fluid_event_queue_new (int count)
{
  return (fluid_event_queue_t *) new_fluid_ringbuffer(count, sizeof(fluid_event_queue_elem_t));
}

static FLUID_INLINE void fluid_event_queue_free (fluid_event_queue_t *queue)
{
  delete_fluid_ringbuffer(queue);
}

/**
 * Get pointer to next input array element in queue.
 * @param queue Lockless queue instance
 * @return Pointer to array element in queue to store data to or NULL if queue is full
 *
 * This function along with fluid_queue_next_inptr() form a queue "push"
 * operation and is split into 2 functions to avoid an element copy.  Note that
 * the returned array element pointer may contain the data of a previous element
 * if the queue has wrapped around.  This can be used to reclaim pointers to
 * allocated memory, etc.
 */
static FLUID_INLINE fluid_event_queue_elem_t *
fluid_event_queue_get_inptr (fluid_event_queue_t *queue)
{
  return (fluid_event_queue_elem_t *) fluid_ringbuffer_get_inptr(queue, 0);
}

/**
 * Advance the input queue index to complete a "push" operation.
 * @param queue Lockless queue instance
 *
 * This function along with fluid_queue_get_inptr() form a queue "push"
 * operation and is split into 2 functions to avoid element copy.
 */
static FLUID_INLINE void
fluid_event_queue_next_inptr (fluid_event_queue_t *queue)
{
  fluid_ringbuffer_next_inptr(queue, 1);
}

/**
 * Get pointer to next output array element in queue.
 * @param queue Lockless queue instance
 * @return Pointer to array element data in the queue or NULL if empty, can only
 *   be used up until fluid_queue_next_outptr() is called.
 *
 * This function along with fluid_queue_next_outptr() form a queue "pop"
 * operation and is split into 2 functions to avoid an element copy.
 */
static FLUID_INLINE fluid_event_queue_elem_t *
fluid_event_queue_get_outptr (fluid_event_queue_t *queue)
{
  return (fluid_event_queue_elem_t *) fluid_ringbuffer_get_outptr(queue);
}

/**
 * Advance the output queue index to complete a "pop" operation.
 * @param queue Lockless queue instance
 *
 * This function along with fluid_queue_get_outptr() form a queue "pop"
 * operation and is split into 2 functions to avoid an element copy.
 */
static FLUID_INLINE void
fluid_event_queue_next_outptr (fluid_event_queue_t *queue)
{
  fluid_ringbuffer_next_outptr(queue);
}

#endif /* _FLUID_EVENT_QUEUE_H */
