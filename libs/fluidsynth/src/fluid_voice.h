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


#ifndef _FLUID_VOICE_H
#define _FLUID_VOICE_H

#include "fluid_phase.h"
#include "fluid_gen.h"
#include "fluid_mod.h"
#include "fluid_iir_filter.h"
#include "fluid_adsr_env.h"
#include "fluid_lfo.h"
#include "fluid_rvoice.h"
#include "fluid_sys.h"

#define NO_CHANNEL             0xff

typedef struct _fluid_overflow_prio_t fluid_overflow_prio_t;

struct _fluid_overflow_prio_t 
{
  fluid_real_t percussion; /**< Is this voice on the drum channel? Then add this score */
  fluid_real_t released; /**< Is this voice in release stage? Then add this score (usually negative) */ 
  fluid_real_t sustained; /**< Is this voice sustained? Then add this score (usually negative) */
  fluid_real_t volume; /**< Multiply current (or future) volume (a value between 0 and 1) */
  fluid_real_t age; /**< This score will be divided by the number of seconds the voice has lasted */
};

enum fluid_voice_status
{
	FLUID_VOICE_CLEAN,
	FLUID_VOICE_ON,
	FLUID_VOICE_SUSTAINED,         /* Sustained by Sustain pedal */
	FLUID_VOICE_HELD_BY_SOSTENUTO, /* Sustained by Sostenuto pedal */
	FLUID_VOICE_OFF
};


/*
 * fluid_voice_t
 */
struct _fluid_voice_t
{
	unsigned int id;                /* the id is incremented for every new noteon.
					   it's used for noteoff's  */
	unsigned char status;
	unsigned char chan;             /* the channel number, quick access for channel messages */
	unsigned char key;              /* the key, quick access for noteoff */
	unsigned char vel;              /* the velocity */
	fluid_channel_t* channel;
	fluid_gen_t gen[GEN_LAST];
	fluid_mod_t mod[FLUID_NUM_MOD];
	int mod_count;
	fluid_sample_t* sample;         /* Pointer to sample (dupe in rvoice) */

	int has_noteoff;                /* Flag set when noteoff has been sent */

	/* basic parameters */
	fluid_real_t output_rate;        /* the sample rate of the synthesizer (dupe in rvoice) */

	unsigned int start_time;
	fluid_adsr_env_t volenv;         /* Volume envelope (dupe in rvoice) */

	/* basic parameters */
	fluid_real_t pitch;              /* the pitch in midicents (dupe in rvoice) */
	fluid_real_t attenuation;        /* the attenuation in centibels (dupe in rvoice) */
	fluid_real_t root_pitch;

	/* master gain (dupe in rvoice) */
	fluid_real_t synth_gain;

	/* pan */
	fluid_real_t pan;
	fluid_real_t amp_left;
	fluid_real_t amp_right;

	/* reverb */
	fluid_real_t reverb_send;
	fluid_real_t amp_reverb;

	/* chorus */
	fluid_real_t chorus_send;
	fluid_real_t amp_chorus;

	/* rvoice control */
	fluid_rvoice_t* rvoice;
	fluid_rvoice_t* overflow_rvoice; /* Used temporarily and only in overflow situations */
	int can_access_rvoice; /* False if rvoice is being rendered in separate thread */ 
	int can_access_overflow_rvoice; /* False if overflow_rvoice is being rendered in separate thread */ 

	/* for debugging */
	int debug;
	double ref;
};


fluid_voice_t* new_fluid_voice(fluid_real_t output_rate);
int delete_fluid_voice(fluid_voice_t* voice);

void fluid_voice_start(fluid_voice_t* voice);
void  fluid_voice_calculate_gen_pitch(fluid_voice_t* voice);

int fluid_voice_write (fluid_voice_t* voice, fluid_real_t *dsp_buf);

int fluid_voice_init(fluid_voice_t* voice, fluid_sample_t* sample,
		     fluid_channel_t* channel, int key, int vel,
		     unsigned int id, unsigned int time, fluid_real_t gain);

int fluid_voice_modulate(fluid_voice_t* voice, int cc, int ctrl);
int fluid_voice_modulate_all(fluid_voice_t* voice);

/** Set the NRPN value of a generator. */
int fluid_voice_set_param(fluid_voice_t* voice, int gen, fluid_real_t value, int abs);


/** Set the gain. */
int fluid_voice_set_gain(fluid_voice_t* voice, fluid_real_t gain);

int fluid_voice_set_output_rate(fluid_voice_t* voice, fluid_real_t value);


/** Update all the synthesis parameters, which depend on generator
    'gen'. This is only necessary after changing a generator of an
    already operating voice.  Most applications will not need this
    function.*/
void fluid_voice_update_param(fluid_voice_t* voice, int gen);

/**  fluid_voice_release
 Force the voice into release stage. Usefuf anywhere a voice
 needs to be damped even if pedals (sustain sostenuto) are depressed.
 See fluid_synth_damp_voices_LOCAL(), fluid_synth_damp_voices_by_sostenuto_LOCAL,
 fluid_voice_noteoff(), fluid_synth_stop_LOCAL().
*/
void fluid_voice_release(fluid_voice_t* voice);
int fluid_voice_noteoff(fluid_voice_t* voice);
int fluid_voice_off(fluid_voice_t* voice);
void fluid_voice_overflow_rvoice_finished(fluid_voice_t* voice);
void fluid_voice_mix (fluid_voice_t *voice, int count, fluid_real_t* dsp_buf,
		 fluid_real_t* left_buf, fluid_real_t* right_buf,
		 fluid_real_t* reverb_buf, fluid_real_t* chorus_buf);

int fluid_voice_kill_excl(fluid_voice_t* voice);
fluid_real_t fluid_voice_get_overflow_prio(fluid_voice_t* voice, 
					    fluid_overflow_prio_t* score,
					    unsigned int cur_time);

#define OVERFLOW_PRIO_CANNOT_KILL 999999.

/**
 * Locks the rvoice for rendering, so it can't be modified directly
 */
static FLUID_INLINE fluid_rvoice_t* 
fluid_voice_lock_rvoice(fluid_voice_t* voice)
{
  voice->can_access_rvoice = 0;
  return voice->rvoice;
}

/**
 * Unlocks the rvoice for rendering, so it can be modified directly
 */
static FLUID_INLINE void 
fluid_voice_unlock_rvoice(fluid_voice_t* voice)
{
  voice->can_access_rvoice = 1;
}


#define fluid_voice_get_channel(voice)  ((voice)->channel)


#define fluid_voice_set_id(_voice, _id)  { (_voice)->id = (_id); }
#define fluid_voice_get_chan(_voice)     (_voice)->chan


#define _PLAYING(voice)  (((voice)->status == FLUID_VOICE_ON) || \
                                           _SUSTAINED(voice)  || \
                                           _HELD_BY_SOSTENUTO(voice) )

/* A voice is 'ON', if it has not yet received a noteoff
 * event. Sending a noteoff event will advance the envelopes to
 * section 5 (release). */
#define _ON(voice)  ((voice)->status == FLUID_VOICE_ON && !voice->has_noteoff)
#define _SUSTAINED(voice)  ((voice)->status == FLUID_VOICE_SUSTAINED)
#define _HELD_BY_SOSTENUTO(voice)  ((voice)->status == FLUID_VOICE_HELD_BY_SOSTENUTO)
#define _AVAILABLE(voice)  ((voice)->can_access_rvoice && \
 (((voice)->status == FLUID_VOICE_CLEAN) || ((voice)->status == FLUID_VOICE_OFF)))
//#define _RELEASED(voice)  ((voice)->chan == NO_CHANNEL)
#define _SAMPLEMODE(voice) ((int)(voice)->gen[GEN_SAMPLEMODE].val)


/* FIXME - This doesn't seem to be used anywhere - JG */
fluid_real_t fluid_voice_gen_value(fluid_voice_t* voice, int num);

#define fluid_voice_get_loudness(voice) (fluid_adsr_env_get_max_val(&voice->volenv))

#define _GEN(_voice, _n) \
  ((fluid_real_t)(_voice)->gen[_n].val \
   + (fluid_real_t)(_voice)->gen[_n].mod \
   + (fluid_real_t)(_voice)->gen[_n].nrpn)

/* defined in fluid_dsp_float.c */

void fluid_dsp_float_config (void);
int fluid_dsp_float_interpolate_none (fluid_voice_t *voice);
int fluid_dsp_float_interpolate_linear (fluid_voice_t *voice);
int fluid_dsp_float_interpolate_4th_order (fluid_voice_t *voice);
int fluid_dsp_float_interpolate_7th_order (fluid_voice_t *voice);

#endif /* _FLUID_VOICE_H */
