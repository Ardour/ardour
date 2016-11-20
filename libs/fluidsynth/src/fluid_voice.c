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

#include "fluidsynth_priv.h"
#include "fluid_voice.h"
#include "fluid_mod.h"
#include "fluid_chan.h"
#include "fluid_conv.h"
#include "fluid_synth.h"
#include "fluid_sys.h"
#include "fluid_sfont.h"
#include "fluid_rvoice_event.h"

/* used for filter turn off optimization - if filter cutoff is above the
   specified value and filter q is below the other value, turn filter off */
#define FLUID_MAX_AUDIBLE_FILTER_FC 19000.0f
#define FLUID_MIN_AUDIBLE_FILTER_Q 1.2f

/* min vol envelope release (to stop clicks) in SoundFont timecents */
#define FLUID_MIN_VOLENVRELEASE -7200.0f /* ~16ms */

static int fluid_voice_calculate_runtime_synthesis_parameters(fluid_voice_t* voice);
static int calculate_hold_decay_buffers(fluid_voice_t* voice, int gen_base,
                                        int gen_key2base, int is_decay);
static fluid_real_t
fluid_voice_get_lower_boundary_for_attenuation(fluid_voice_t* voice);

#define UPDATE_RVOICE0(proc) \
  do { \
    if (voice->can_access_rvoice) proc(voice->rvoice); \
    else fluid_rvoice_eventhandler_push(voice->channel->synth->eventhandler, \
      proc, voice->rvoice, 0, 0.0f); \
  } while (0)

#define UPDATE_RVOICE_PTR(proc, obj) \
  do { \
    if (voice->can_access_rvoice) proc(voice->rvoice, obj); \
    else fluid_rvoice_eventhandler_push_ptr(voice->channel->synth->eventhandler, \
      proc, voice->rvoice, obj); \
  } while (0)


#define UPDATE_RVOICE_GENERIC_R1(proc, obj, rarg) \
  do { \
    if (voice->can_access_rvoice) proc(obj, rarg); \
    else fluid_rvoice_eventhandler_push(voice->channel->synth->eventhandler, \
      proc, obj, 0, rarg); \
  } while (0)

#define UPDATE_RVOICE_GENERIC_I1(proc, obj, iarg) \
  do { \
    if (voice->can_access_rvoice) proc(obj, iarg); \
    else fluid_rvoice_eventhandler_push(voice->channel->synth->eventhandler, \
      proc, obj, iarg, 0.0f); \
  } while (0)

#define UPDATE_RVOICE_GENERIC_IR(proc, obj, iarg, rarg) \
  do { \
    if (voice->can_access_rvoice) proc(obj, iarg, rarg); \
    else fluid_rvoice_eventhandler_push(voice->channel->synth->eventhandler, \
      proc, obj, iarg, rarg); \
  } while (0)

#define UPDATE_RVOICE_GENERIC_ALL(proc, obj, iarg, r1, r2, r3, r4, r5) \
  do { \
    if (voice->can_access_rvoice) proc(obj, iarg, r1, r2, r3, r4, r5); \
    else fluid_rvoice_eventhandler_push5(voice->channel->synth->eventhandler, \
      proc, obj, iarg, r1, r2, r3, r4, r5); \
  } while (0)


#define UPDATE_RVOICE_VOLENV(section, arg1, arg2, arg3, arg4, arg5) \
  do { \
    fluid_adsr_env_set_data(&voice->volenv, section, arg1, arg2, arg3, arg4, arg5) \
    UPDATE_RVOICE_GENERIC_ALL(fluid_adsr_env_set_data, &voice->rvoice->envlfo.volenv, section, arg1, arg2, arg3, arg4, arg5) \
  } while(0)

#define UPDATE_RVOICE_MODENV(section, arg1, arg2, arg3, arg4, arg5) \
  UPDATE_RVOICE_GENERIC_ALL(fluid_adsr_env_set_data, &voice->rvoice->envlfo.modenv, section, arg1, arg2, arg3, arg4, arg5)

#define UPDATE_RVOICE_R1(proc, arg1) UPDATE_RVOICE_GENERIC_R1(proc, voice->rvoice, arg1)
#define UPDATE_RVOICE_I1(proc, arg1) UPDATE_RVOICE_GENERIC_I1(proc, voice->rvoice, arg1)
#define UPDATE_RVOICE_FILTER1(proc, arg1) UPDATE_RVOICE_GENERIC_R1(proc, &voice->rvoice->resonant_filter, arg1)

#define UPDATE_RVOICE2(proc, iarg, rarg) UPDATE_RVOICE_GENERIC_IR(proc, voice->rvoice, iarg, rarg)
#define UPDATE_RVOICE_BUFFERS2(proc, iarg, rarg) UPDATE_RVOICE_GENERIC_IR(proc, &voice->rvoice->buffers, iarg, rarg)
#define UPDATE_RVOICE_ENVLFO_R1(proc, envp, rarg) UPDATE_RVOICE_GENERIC_R1(proc, &voice->rvoice->envlfo.envp, rarg) 
#define UPDATE_RVOICE_ENVLFO_I1(proc, envp, iarg) UPDATE_RVOICE_GENERIC_I1(proc, &voice->rvoice->envlfo.envp, iarg) 

static inline void
fluid_voice_update_volenv(fluid_voice_t* voice, 
			  fluid_adsr_env_section_t section,
                          unsigned int count,
                          fluid_real_t coeff,
                          fluid_real_t increment,
                          fluid_real_t min,
                          fluid_real_t max)
{
  fluid_adsr_env_set_data(&voice->volenv, section, count, coeff, increment, 
			  min, max);
  UPDATE_RVOICE_GENERIC_ALL(fluid_adsr_env_set_data, 
			    &voice->rvoice->envlfo.volenv, section, count, 
			    coeff, increment, min, max);
}

static inline void
fluid_voice_update_modenv(fluid_voice_t* voice, 
			  fluid_adsr_env_section_t section,
                          unsigned int count,
                          fluid_real_t coeff,
                          fluid_real_t increment,
                          fluid_real_t min,
                          fluid_real_t max)
{
  UPDATE_RVOICE_GENERIC_ALL(fluid_adsr_env_set_data, 
			    &voice->rvoice->envlfo.modenv, section, count,
			    coeff, increment, min, max);
}

static inline void fluid_sample_null_ptr(fluid_sample_t** sample)
{
  if (*sample != NULL) {
    fluid_sample_decr_ref(*sample);
    *sample = NULL;
  }
}

/*
 * Swaps the current rvoice with the current overflow_rvoice
 */
static void fluid_voice_swap_rvoice(fluid_voice_t* voice)
{
  fluid_rvoice_t* rtemp = voice->rvoice;
  int ctemp = voice->can_access_rvoice;
  voice->rvoice = voice->overflow_rvoice;
  voice->can_access_rvoice = voice->can_access_overflow_rvoice;
  voice->overflow_rvoice = rtemp;
  voice->can_access_overflow_rvoice = ctemp;
}

static void fluid_voice_initialize_rvoice(fluid_voice_t* voice)
{
  FLUID_MEMSET(voice->rvoice, 0, sizeof(fluid_rvoice_t));

  /* The 'sustain' and 'finished' segments of the volume / modulation
   * envelope are constant. They are never affected by any modulator
   * or generator. Therefore it is enough to initialize them once
   * during the lifetime of the synth.
   */
  fluid_voice_update_volenv(voice, FLUID_VOICE_ENVSUSTAIN, 
                          0xffffffff, 1.0f, 0.0f, -1.0f, 2.0f);
  fluid_voice_update_volenv(voice, FLUID_VOICE_ENVFINISHED, 
                          0xffffffff, 0.0f, 0.0f, -1.0f, 1.0f);
  fluid_voice_update_modenv(voice, FLUID_VOICE_ENVSUSTAIN, 
                          0xffffffff, 1.0f, 0.0f, -1.0f, 2.0f);
  fluid_voice_update_modenv(voice, FLUID_VOICE_ENVFINISHED, 
                          0xffffffff, 0.0f, 0.0f, -1.0f, 1.0f);
}

/*
 * new_fluid_voice
 */
fluid_voice_t*
new_fluid_voice(fluid_real_t output_rate)
{
  fluid_voice_t* voice;
  voice = FLUID_NEW(fluid_voice_t);
  if (voice == NULL) {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }
  voice->rvoice = FLUID_NEW(fluid_rvoice_t);
  voice->overflow_rvoice = FLUID_NEW(fluid_rvoice_t);
  if (voice->rvoice == NULL || voice->overflow_rvoice == NULL) {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    FLUID_FREE(voice->rvoice);
    FLUID_FREE(voice);
    return NULL;
  }

  voice->status = FLUID_VOICE_CLEAN;
  voice->chan = NO_CHANNEL;
  voice->key = 0;
  voice->vel = 0;
  voice->channel = NULL;
  voice->sample = NULL;

  /* Initialize both the rvoice and overflow_rvoice */
  voice->can_access_rvoice = 1; 
  voice->can_access_overflow_rvoice = 1; 
  fluid_voice_initialize_rvoice(voice);
  fluid_voice_swap_rvoice(voice);
  fluid_voice_initialize_rvoice(voice);

  fluid_voice_set_output_rate(voice, output_rate);

  return voice;
}

/*
 * delete_fluid_voice
 */
int
delete_fluid_voice(fluid_voice_t* voice)
{
  if (voice == NULL) {
    return FLUID_OK;
  }
  if (!voice->can_access_rvoice || !voice->can_access_overflow_rvoice) {
    /* stop rvoice before deleting voice! */
    return FLUID_FAILED;
  }
  FLUID_FREE(voice->overflow_rvoice);
  FLUID_FREE(voice->rvoice);
  FLUID_FREE(voice);
  return FLUID_OK;
}

/* fluid_voice_init
 *
 * Initialize the synthesis process
 */
int
fluid_voice_init(fluid_voice_t* voice, fluid_sample_t* sample,
		 fluid_channel_t* channel, int key, int vel, unsigned int id,
		 unsigned int start_time, fluid_real_t gain)
{
  /* Note: The voice parameters will be initialized later, when the
   * generators have been retrieved from the sound font. Here, only
   * the 'working memory' of the voice (position in envelopes, history
   * of IIR filters, position in sample etc) is initialized. */
  int i;

  if (!voice->can_access_rvoice) {
    if (voice->can_access_overflow_rvoice) 
      fluid_voice_swap_rvoice(voice);
    else {
      FLUID_LOG(FLUID_ERR, "Internal error: Cannot access an rvoice in fluid_voice_init!");
      return FLUID_FAILED;
    }
  }
  /* We are now guaranteed to have access to the rvoice */

  if (voice->sample)
    fluid_voice_off(voice);

  voice->id = id;
  voice->chan = fluid_channel_get_num(channel);
  voice->key = (unsigned char) key;
  voice->vel = (unsigned char) vel;
  voice->channel = channel;
  voice->mod_count = 0;
  voice->start_time = start_time;
  voice->debug = 0;
  voice->has_noteoff = 0;
  UPDATE_RVOICE0(fluid_rvoice_reset);

  /* Increment the reference count of the sample to prevent the
     unloading of the soundfont while this voice is playing,
     once for us and once for the rvoice. */
  fluid_sample_incr_ref(sample);
  UPDATE_RVOICE_PTR(fluid_rvoice_set_sample, sample);
  fluid_sample_incr_ref(sample);
  voice->sample = sample;

  i = fluid_channel_get_interp_method(channel);
  UPDATE_RVOICE_I1(fluid_rvoice_set_interp_method, i);

  /* Set all the generators to their default value, according to SF
   * 2.01 section 8.1.3 (page 48). The value of NRPN messages are
   * copied from the channel to the voice's generators. The sound font
   * loader overwrites them. The generator values are later converted
   * into voice parameters in
   * fluid_voice_calculate_runtime_synthesis_parameters.  */
  fluid_gen_init(&voice->gen[0], channel);
  UPDATE_RVOICE_I1(fluid_rvoice_set_samplemode, _SAMPLEMODE(voice));

  voice->synth_gain = gain;
  /* avoid division by zero later*/
  if (voice->synth_gain < 0.0000001){
    voice->synth_gain = 0.0000001;
  }
  UPDATE_RVOICE_R1(fluid_rvoice_set_synth_gain, voice->synth_gain);

  /* Set up buffer mapping, should be done more flexible in the future. */
  i = channel->synth->audio_groups;
  UPDATE_RVOICE_BUFFERS2(fluid_rvoice_buffers_set_mapping, 2, i*2 + SYNTH_REVERB_CHANNEL);
  UPDATE_RVOICE_BUFFERS2(fluid_rvoice_buffers_set_mapping, 3, i*2 + SYNTH_CHORUS_CHANNEL);
  i = 2 * (voice->chan % i);
  UPDATE_RVOICE_BUFFERS2(fluid_rvoice_buffers_set_mapping, 0, i);
  UPDATE_RVOICE_BUFFERS2(fluid_rvoice_buffers_set_mapping, 1, i+1);

  return FLUID_OK;
}


/**
 * Update sample rate. 
 * NOTE: If the voice is active, it will be turned off.
 */
int 
fluid_voice_set_output_rate(fluid_voice_t* voice, fluid_real_t value)
{
  if (_PLAYING(voice))
    fluid_voice_off(voice);
  
  voice->output_rate = value;
  UPDATE_RVOICE_R1(fluid_rvoice_set_output_rate, value);
  /* Update the other rvoice as well */
  fluid_voice_swap_rvoice(voice);
  UPDATE_RVOICE_R1(fluid_rvoice_set_output_rate, value);
  fluid_voice_swap_rvoice(voice);

  return FLUID_FAILED;
}


/**
 * Set the value of a generator.
 * @param voice Voice instance
 * @param i Generator ID (#fluid_gen_type)
 * @param val Generator value
 */
void
fluid_voice_gen_set(fluid_voice_t* voice, int i, float val)
{
  voice->gen[i].val = val;
  voice->gen[i].flags = GEN_SET;
  if (i == GEN_SAMPLEMODE)
    UPDATE_RVOICE_I1(fluid_rvoice_set_samplemode, (int) val);
}

/**
 * Offset the value of a generator.
 * @param voice Voice instance
 * @param i Generator ID (#fluid_gen_type)
 * @param val Value to add to the existing value
 */
void
fluid_voice_gen_incr(fluid_voice_t* voice, int i, float val)
{
  voice->gen[i].val += val;
  voice->gen[i].flags = GEN_SET;
}

/**
 * Get the value of a generator.
 * @param voice Voice instance
 * @param gen Generator ID (#fluid_gen_type)
 * @return Current generator value
 */
float
fluid_voice_gen_get(fluid_voice_t* voice, int gen)
{
  return voice->gen[gen].val;
}

fluid_real_t fluid_voice_gen_value(fluid_voice_t* voice, int num)
{
	/* This is an extension to the SoundFont standard. More
	 * documentation is available at the fluid_synth_set_gen2()
	 * function. */
	if (voice->gen[num].flags == GEN_ABS_NRPN) {
		return (fluid_real_t) voice->gen[num].nrpn;
	} else {
		return (fluid_real_t) (voice->gen[num].val + voice->gen[num].mod + voice->gen[num].nrpn);
	}
}


/**
 * Synthesize a voice to a buffer.
 *
 * @param voice Voice to synthesize
 * @param dsp_buf Audio buffer to synthesize to (#FLUID_BUFSIZE in length)
 * @return Count of samples written to dsp_buf (can be 0)
 *
 * Panning, reverb and chorus are processed separately. The dsp interpolation
 * routine is in (fluid_dsp_float.c).
 */
int
fluid_voice_write (fluid_voice_t* voice, fluid_real_t *dsp_buf)
{
  int result;
  if (!voice->can_access_rvoice) 
    return 0;

  result = fluid_rvoice_write(voice->rvoice, dsp_buf);

  if (result == -1)
    return 0;

  if ((result < FLUID_BUFSIZE) && _PLAYING(voice)) /* Voice finished by itself */
    fluid_voice_off(voice);

  return result;
}


/**
 * Mix voice data to left/right (panning), reverb and chorus buffers.
 * @param count Number of samples
 * @param dsp_buf Source buffer
 * @param voice Voice to mix
 * @param left_buf Left audio buffer
 * @param right_buf Right audio buffer
 * @param reverb_buf Reverb buffer
 * @param chorus_buf Chorus buffer
 *
 */
void
fluid_voice_mix (fluid_voice_t *voice, int count, fluid_real_t* dsp_buf,
		 fluid_real_t* left_buf, fluid_real_t* right_buf,
		 fluid_real_t* reverb_buf, fluid_real_t* chorus_buf)
{
  fluid_rvoice_buffers_t buffers;
  fluid_real_t* dest_buf[4] = {left_buf, right_buf, reverb_buf, chorus_buf};

  fluid_rvoice_buffers_set_amp(&buffers, 0, voice->amp_left);
  fluid_rvoice_buffers_set_amp(&buffers, 1, voice->amp_right);
  fluid_rvoice_buffers_set_amp(&buffers, 2, voice->amp_reverb);
  fluid_rvoice_buffers_set_amp(&buffers, 3, voice->amp_chorus);
 
  fluid_rvoice_buffers_mix(&buffers, dsp_buf, count, dest_buf, 4);

  fluid_check_fpe ("voice_mix");
}



/*
 * fluid_voice_start
 */
void fluid_voice_start(fluid_voice_t* voice)
{
  /* The maximum volume of the loop is calculated and cached once for each
   * sample with its nominal loop settings. This happens, when the sample is used
   * for the first time.*/

  fluid_voice_calculate_runtime_synthesis_parameters(voice);

  voice->ref = fluid_profile_ref();

  voice->status = FLUID_VOICE_ON;

  /* Increment voice count */
  voice->channel->synth->active_voice_count++;
}

void 
fluid_voice_calculate_gen_pitch(fluid_voice_t* voice)
{
  fluid_tuning_t* tuning;
  fluid_real_t x;

  /* The GEN_PITCH is a hack to fit the pitch bend controller into the
   * modulator paradigm.  Now the nominal pitch of the key is set.
   * Note about SCALETUNE: SF2.01 8.1.3 says, that this generator is a
   * non-realtime parameter. So we don't allow modulation (as opposed
   * to _GEN(voice, GEN_SCALETUNE) When the scale tuning is varied,
   * one key remains fixed. Here C3 (MIDI number 60) is used.
   */
  if (fluid_channel_has_tuning(voice->channel)) {
    tuning = fluid_channel_get_tuning (voice->channel);
    x = fluid_tuning_get_pitch (tuning, (int)(voice->root_pitch / 100.0f));
    voice->gen[GEN_PITCH].val = voice->gen[GEN_SCALETUNE].val / 100.0f *
      (fluid_tuning_get_pitch (tuning, voice->key) - x) + x;
  } else {
    voice->gen[GEN_PITCH].val = voice->gen[GEN_SCALETUNE].val
      * (voice->key - voice->root_pitch / 100.0f) + voice->root_pitch;
  }

}

/*
 * fluid_voice_calculate_runtime_synthesis_parameters
 *
 * in this function we calculate the values of all the parameters. the
 * parameters are converted to their most useful unit for the DSP
 * algorithm, for example, number of samples instead of
 * timecents. Some parameters keep their "perceptual" unit and
 * conversion will be done in the DSP function. This is the case, for
 * example, for the pitch since it is modulated by the controllers in
 * cents. */
static int
fluid_voice_calculate_runtime_synthesis_parameters(fluid_voice_t* voice)
{
  int i;

  int list_of_generators_to_initialize[35] = {
    GEN_STARTADDROFS,                    /* SF2.01 page 48 #0   */
    GEN_ENDADDROFS,                      /*                #1   */
    GEN_STARTLOOPADDROFS,                /*                #2   */
    GEN_ENDLOOPADDROFS,                  /*                #3   */
    /* GEN_STARTADDRCOARSEOFS see comment below [1]        #4   */
    GEN_MODLFOTOPITCH,                   /*                #5   */
    GEN_VIBLFOTOPITCH,                   /*                #6   */
    GEN_MODENVTOPITCH,                   /*                #7   */
    GEN_FILTERFC,                        /*                #8   */
    GEN_FILTERQ,                         /*                #9   */
    GEN_MODLFOTOFILTERFC,                /*                #10  */
    GEN_MODENVTOFILTERFC,                /*                #11  */
    /* GEN_ENDADDRCOARSEOFS [1]                            #12  */
    GEN_MODLFOTOVOL,                     /*                #13  */
    /* not defined                                         #14  */
    GEN_CHORUSSEND,                      /*                #15  */
    GEN_REVERBSEND,                      /*                #16  */
    GEN_PAN,                             /*                #17  */
    /* not defined                                         #18  */
    /* not defined                                         #19  */
    /* not defined                                         #20  */
    GEN_MODLFODELAY,                     /*                #21  */
    GEN_MODLFOFREQ,                      /*                #22  */
    GEN_VIBLFODELAY,                     /*                #23  */
    GEN_VIBLFOFREQ,                      /*                #24  */
    GEN_MODENVDELAY,                     /*                #25  */
    GEN_MODENVATTACK,                    /*                #26  */
    GEN_MODENVHOLD,                      /*                #27  */
    GEN_MODENVDECAY,                     /*                #28  */
    /* GEN_MODENVSUSTAIN [1]                               #29  */
    GEN_MODENVRELEASE,                   /*                #30  */
    /* GEN_KEYTOMODENVHOLD [1]                             #31  */
    /* GEN_KEYTOMODENVDECAY [1]                            #32  */
    GEN_VOLENVDELAY,                     /*                #33  */
    GEN_VOLENVATTACK,                    /*                #34  */
    GEN_VOLENVHOLD,                      /*                #35  */
    GEN_VOLENVDECAY,                     /*                #36  */
    /* GEN_VOLENVSUSTAIN [1]                               #37  */
    GEN_VOLENVRELEASE,                   /*                #38  */
    /* GEN_KEYTOVOLENVHOLD [1]                             #39  */
    /* GEN_KEYTOVOLENVDECAY [1]                            #40  */
    /* GEN_STARTLOOPADDRCOARSEOFS [1]                      #45  */
    GEN_KEYNUM,                          /*                #46  */
    GEN_VELOCITY,                        /*                #47  */
    GEN_ATTENUATION,                     /*                #48  */
    /* GEN_ENDLOOPADDRCOARSEOFS [1]                        #50  */
    /* GEN_COARSETUNE           [1]                        #51  */
    /* GEN_FINETUNE             [1]                        #52  */
    GEN_OVERRIDEROOTKEY,                 /*                #58  */
    GEN_PITCH,                           /*                ---  */
    -1};                                 /* end-of-list marker  */

  /* When the voice is made ready for the synthesis process, a lot of
   * voice-internal parameters have to be calculated.
   *
   * At this point, the sound font has already set the -nominal- value
   * for all generators (excluding GEN_PITCH). Most generators can be
   * modulated - they include a nominal value and an offset (which
   * changes with velocity, note number, channel parameters like
   * aftertouch, mod wheel...) Now this offset will be calculated as
   * follows:
   *
   *  - Process each modulator once.
   *  - Calculate its output value.
   *  - Find the target generator.
   *  - Add the output value to the modulation value of the generator.
   *
   * Note: The generators have been initialized with
   * fluid_gen_set_default_values.
   */

  for (i = 0; i < voice->mod_count; i++) {
    fluid_mod_t* mod = &voice->mod[i];
    fluid_real_t modval = fluid_mod_get_value(mod, voice->channel, voice);
    int dest_gen_index = mod->dest;
    fluid_gen_t* dest_gen = &voice->gen[dest_gen_index];
    dest_gen->mod += modval;
    /*      fluid_dump_modulator(mod); */
  }

  /* Now the generators are initialized, nominal and modulation value.
   * The voice parameters (which depend on generators) are calculated
   * with fluid_voice_update_param. Processing the list of generator
   * changes will calculate each voice parameter once.
   *
   * Note [1]: Some voice parameters depend on several generators. For
   * example, the pitch depends on GEN_COARSETUNE, GEN_FINETUNE and
   * GEN_PITCH.  voice->pitch.  Unnecessary recalculation is avoided
   * by removing all but one generator from the list of voice
   * parameters.  Same with GEN_XXX and GEN_XXXCOARSE: the
   * initialisation list contains only GEN_XXX.
   */

  /* Calculate the voice parameter(s) dependent on each generator. */
  for (i = 0; list_of_generators_to_initialize[i] != -1; i++) {
    fluid_voice_update_param(voice, list_of_generators_to_initialize[i]);
  }

  /* Make an estimate on how loud this voice can get at any time (attenuation). */
  UPDATE_RVOICE_R1(fluid_rvoice_set_min_attenuation_cB, 
                 fluid_voice_get_lower_boundary_for_attenuation(voice)); 
  return FLUID_OK;
}

/*
 * calculate_hold_decay_buffers
 */
static int
calculate_hold_decay_buffers(fluid_voice_t* voice, int gen_base,
                             int gen_key2base, int is_decay)
{
  /* Purpose:
   *
   * Returns the number of DSP loops, that correspond to the hold
   * (is_decay=0) or decay (is_decay=1) time.
   * gen_base=GEN_VOLENVHOLD, GEN_VOLENVDECAY, GEN_MODENVHOLD,
   * GEN_MODENVDECAY gen_key2base=GEN_KEYTOVOLENVHOLD,
   * GEN_KEYTOVOLENVDECAY, GEN_KEYTOMODENVHOLD, GEN_KEYTOMODENVDECAY
   */

  fluid_real_t timecents;
  fluid_real_t seconds;
  int buffers;

  /* SF2.01 section 8.4.3 # 31, 32, 39, 40
   * GEN_KEYTOxxxENVxxx uses key 60 as 'origin'.
   * The unit of the generator is timecents per key number.
   * If KEYTOxxxENVxxx is 100, a key one octave over key 60 (72)
   * will cause (60-72)*100=-1200 timecents of time variation.
   * The time is cut in half.
   */
  timecents = (_GEN(voice, gen_base) + _GEN(voice, gen_key2base) * (60.0 - voice->key));

  /* Range checking */
  if (is_decay){
    /* SF 2.01 section 8.1.3 # 28, 36 */
    if (timecents > 8000.0) {
      timecents = 8000.0;
    }
  } else {
    /* SF 2.01 section 8.1.3 # 27, 35 */
    if (timecents > 5000) {
      timecents = 5000.0;
    }
    /* SF 2.01 section 8.1.2 # 27, 35:
     * The most negative number indicates no hold time
     */
    if (timecents <= -32768.) {
      return 0;
    }
  }
  /* SF 2.01 section 8.1.3 # 27, 28, 35, 36 */
  if (timecents < -12000.0) {
    timecents = -12000.0;
  }

  seconds = fluid_tc2sec(timecents);
  /* Each DSP loop processes FLUID_BUFSIZE samples. */

  /* round to next full number of buffers */
  buffers = (int)(((fluid_real_t)voice->output_rate * seconds)
		  / (fluid_real_t)FLUID_BUFSIZE
		  +0.5);

  return buffers;
}

/*
 * The value of a generator (gen) has changed.  (The different
 * generators are listed in fluidsynth.h, or in SF2.01 page 48-49)
 * Now the dependent 'voice' parameters are calculated.
 *
 * fluid_voice_update_param can be called during the setup of the
 * voice (to calculate the initial value for a voice parameter), or
 * during its operation (a generator has been changed due to
 * real-time parameter modifications like pitch-bend).
 *
 * Note: The generator holds three values: The base value .val, an
 * offset caused by modulators .mod, and an offset caused by the
 * NRPN system. _GEN(voice, generator_enumerator) returns the sum
 * of all three.
 */
/**
 * Update all the synthesis parameters, which depend on generator \a gen.
 * @param voice Voice instance
 * @param gen Generator id (#fluid_gen_type)
 *
 * This is only necessary after changing a generator of an already operating voice.
 * Most applications will not need this function.
 */
void
fluid_voice_update_param(fluid_voice_t* voice, int gen)
{
  double q_dB;
  fluid_real_t x;
  fluid_real_t y;
  unsigned int count, z;
  // Alternate attenuation scale used by EMU10K1 cards when setting the attenuation at the preset or instrument level within the SoundFont bank.
  static const float ALT_ATTENUATION_SCALE = 0.4;

  switch (gen) {

  case GEN_PAN:
    /* range checking is done in the fluid_pan function */
    voice->pan = _GEN(voice, GEN_PAN);
    voice->amp_left = fluid_pan(voice->pan, 1) * voice->synth_gain / 32768.0f;
    voice->amp_right = fluid_pan(voice->pan, 0) * voice->synth_gain / 32768.0f;
    UPDATE_RVOICE_BUFFERS2(fluid_rvoice_buffers_set_amp, 0, voice->amp_left);
    UPDATE_RVOICE_BUFFERS2(fluid_rvoice_buffers_set_amp, 1, voice->amp_right);
    break;

  case GEN_ATTENUATION:
    voice->attenuation = ((fluid_real_t)(voice)->gen[GEN_ATTENUATION].val*ALT_ATTENUATION_SCALE) +
    (fluid_real_t)(voice)->gen[GEN_ATTENUATION].mod + (fluid_real_t)(voice)->gen[GEN_ATTENUATION].nrpn;

    /* Range: SF2.01 section 8.1.3 # 48
     * Motivation for range checking:
     * OHPiano.SF2 sets initial attenuation to a whooping -96 dB */
    fluid_clip(voice->attenuation, 0.0, 1440.0);
    UPDATE_RVOICE_R1(fluid_rvoice_set_attenuation, voice->attenuation);
    break;

    /* The pitch is calculated from three different generators.
     * Read comment in fluidsynth.h about GEN_PITCH.
     */
  case GEN_PITCH:
  case GEN_COARSETUNE:
  case GEN_FINETUNE:
    /* The testing for allowed range is done in 'fluid_ct2hz' */
    voice->pitch = (_GEN(voice, GEN_PITCH)
		    + 100.0f * _GEN(voice, GEN_COARSETUNE)
		    + _GEN(voice, GEN_FINETUNE));
    UPDATE_RVOICE_R1(fluid_rvoice_set_pitch, voice->pitch);
    break;

  case GEN_REVERBSEND:
    /* The generator unit is 'tenths of a percent'. */
    voice->reverb_send = _GEN(voice, GEN_REVERBSEND) / 1000.0f;
    fluid_clip(voice->reverb_send, 0.0, 1.0);
    voice->amp_reverb = voice->reverb_send * voice->synth_gain / 32768.0f;
    UPDATE_RVOICE_BUFFERS2(fluid_rvoice_buffers_set_amp, 2, voice->amp_reverb);
    break;

  case GEN_CHORUSSEND:
    /* The generator unit is 'tenths of a percent'. */
    voice->chorus_send = _GEN(voice, GEN_CHORUSSEND) / 1000.0f;
    fluid_clip(voice->chorus_send, 0.0, 1.0);
    voice->amp_chorus = voice->chorus_send * voice->synth_gain / 32768.0f;
    UPDATE_RVOICE_BUFFERS2(fluid_rvoice_buffers_set_amp, 3, voice->amp_chorus);
    break;

  case GEN_OVERRIDEROOTKEY:
    /* This is a non-realtime parameter. Therefore the .mod part of the generator
     * can be neglected.
     * NOTE: origpitch sets MIDI root note while pitchadj is a fine tuning amount
     * which offsets the original rate.  This means that the fine tuning is
     * inverted with respect to the root note (so subtract it, not add).
     */
    if (voice->sample != NULL) {
      if (voice->gen[GEN_OVERRIDEROOTKEY].val > -1)   //FIXME: use flag instead of -1
        voice->root_pitch = voice->gen[GEN_OVERRIDEROOTKEY].val * 100.0f
	  - voice->sample->pitchadj;
      else
        voice->root_pitch = voice->sample->origpitch * 100.0f - voice->sample->pitchadj;
      x = (fluid_ct2hz(voice->root_pitch) * ((fluid_real_t) voice->output_rate / voice->sample->samplerate));
    } else {
      if (voice->gen[GEN_OVERRIDEROOTKEY].val > -1)    //FIXME: use flag instead of -1
        voice->root_pitch = voice->gen[GEN_OVERRIDEROOTKEY].val * 100.0f;
      else
        voice->root_pitch = 0;
      x = fluid_ct2hz(voice->root_pitch);
    }
    /* voice->pitch depends on voice->root_pitch, so calculate voice->pitch now */
    fluid_voice_calculate_gen_pitch(voice);
    UPDATE_RVOICE_R1(fluid_rvoice_set_root_pitch_hz, x);

    break;

  case GEN_FILTERFC:
    /* The resonance frequency is converted from absolute cents to
     * midicents .val and .mod are both used, this permits real-time
     * modulation.  The allowed range is tested in the 'fluid_ct2hz'
     * function [PH,20021214]
     */
    x = _GEN(voice, GEN_FILTERFC);
    UPDATE_RVOICE_FILTER1(fluid_iir_filter_set_fres, x);
    break;

  case GEN_FILTERQ:
    /* The generator contains 'centibels' (1/10 dB) => divide by 10 to
     * obtain dB */
    q_dB = _GEN(voice, GEN_FILTERQ) / 10.0f;

    /* Range: SF2.01 section 8.1.3 # 8 (convert from cB to dB => /10) */
    fluid_clip(q_dB, 0.0f, 96.0f);

    /* Short version: Modify the Q definition in a way, that a Q of 0
     * dB leads to no resonance hump in the freq. response.
     *
     * Long version: From SF2.01, page 39, item 9 (initialFilterQ):
     * "The gain at the cutoff frequency may be less than zero when
     * zero is specified".  Assume q_dB=0 / q_lin=1: If we would leave
     * q as it is, then this results in a 3 dB hump slightly below
     * fc. At fc, the gain is exactly the DC gain (0 dB).  What is
     * (probably) meant here is that the filter does not show a
     * resonance hump for q_dB=0. In this case, the corresponding
     * q_lin is 1/sqrt(2)=0.707.  The filter should have 3 dB of
     * attenuation at fc now.  In this case Q_dB is the height of the
     * resonance peak not over the DC gain, but over the frequency
     * response of a non-resonant filter.  This idea is implemented as
     * follows: */
    q_dB -= 3.01f;
    UPDATE_RVOICE_FILTER1(fluid_iir_filter_set_q_dB, q_dB);

    break;

  case GEN_MODLFOTOPITCH:
    x = _GEN(voice, GEN_MODLFOTOPITCH);
    fluid_clip(x, -12000.0, 12000.0);
    UPDATE_RVOICE_R1(fluid_rvoice_set_modlfo_to_pitch, x);
    break;

  case GEN_MODLFOTOVOL:
    x = _GEN(voice, GEN_MODLFOTOVOL);
    fluid_clip(x, -960.0, 960.0);
    UPDATE_RVOICE_R1(fluid_rvoice_set_modlfo_to_vol, x);
    break;

  case GEN_MODLFOTOFILTERFC:
    x = _GEN(voice, GEN_MODLFOTOFILTERFC);
    fluid_clip(x, -12000, 12000);
    UPDATE_RVOICE_R1(fluid_rvoice_set_modlfo_to_fc, x);
    break;

  case GEN_MODLFODELAY:
    x = _GEN(voice, GEN_MODLFODELAY);
    fluid_clip(x, -12000.0f, 5000.0f);
    z = (unsigned int) (voice->output_rate * fluid_tc2sec_delay(x));
    UPDATE_RVOICE_ENVLFO_I1(fluid_lfo_set_delay, modlfo, z);
    break;

  case GEN_MODLFOFREQ:
    /* - the frequency is converted into a delta value, per buffer of FLUID_BUFSIZE samples
     * - the delay into a sample delay
     */
    x = _GEN(voice, GEN_MODLFOFREQ);
    fluid_clip(x, -16000.0f, 4500.0f);
    x = (4.0f * FLUID_BUFSIZE * fluid_act2hz(x) / voice->output_rate);
    UPDATE_RVOICE_ENVLFO_R1(fluid_lfo_set_incr, modlfo, x);
    break;

  case GEN_VIBLFOFREQ:
    /* vib lfo
     *
     * - the frequency is converted into a delta value, per buffer of FLUID_BUFSIZE samples
     * - the delay into a sample delay
     */
    x = _GEN(voice, GEN_VIBLFOFREQ);
    fluid_clip(x, -16000.0f, 4500.0f);
    x = 4.0f * FLUID_BUFSIZE * fluid_act2hz(x) / voice->output_rate;
    UPDATE_RVOICE_ENVLFO_R1(fluid_lfo_set_incr, viblfo, x); 
    break;

  case GEN_VIBLFODELAY:
    x = _GEN(voice,GEN_VIBLFODELAY);
    fluid_clip(x, -12000.0f, 5000.0f);
    z = (unsigned int) (voice->output_rate * fluid_tc2sec_delay(x)); 
    UPDATE_RVOICE_ENVLFO_I1(fluid_lfo_set_delay, viblfo, z); 
    break;

  case GEN_VIBLFOTOPITCH:
    x = _GEN(voice, GEN_VIBLFOTOPITCH);
    fluid_clip(x, -12000.0, 12000.0);
    UPDATE_RVOICE_R1(fluid_rvoice_set_viblfo_to_pitch, x); 
    break;

  case GEN_KEYNUM:
    /* GEN_KEYNUM: SF2.01 page 46, item 46
     *
     * If this generator is active, it forces the key number to its
     * value.  Non-realtime controller.
     *
     * There is a flag, which should indicate, whether a generator is
     * enabled or not.  But here we rely on the default value of -1.
     * */
    x = _GEN(voice, GEN_KEYNUM);
    if (x >= 0){
      voice->key = x;
    }
    break;

  case GEN_VELOCITY:
    /* GEN_VELOCITY: SF2.01 page 46, item 47
     *
     * If this generator is active, it forces the velocity to its
     * value. Non-realtime controller.
     *
     * There is a flag, which should indicate, whether a generator is
     * enabled or not. But here we rely on the default value of -1.  */
    x = _GEN(voice, GEN_VELOCITY);
    if (x > 0) {
      voice->vel = x;
    }
    break;

  case GEN_MODENVTOPITCH:
    x = _GEN(voice, GEN_MODENVTOPITCH);
    fluid_clip(x, -12000.0, 12000.0);
    UPDATE_RVOICE_R1(fluid_rvoice_set_modenv_to_pitch, x);
    break;

  case GEN_MODENVTOFILTERFC:
    x = _GEN(voice,GEN_MODENVTOFILTERFC);

    /* Range: SF2.01 section 8.1.3 # 1
     * Motivation for range checking:
     * Filter is reported to make funny noises now and then
     */
    fluid_clip(x, -12000.0, 12000.0);
    UPDATE_RVOICE_R1(fluid_rvoice_set_modenv_to_fc, x);
    break;


    /* sample start and ends points
     *
     * Range checking is initiated via the
     * voice->check_sample_sanity flag,
     * because it is impossible to check here:
     * During the voice setup, all modulators are processed, while
     * the voice is inactive. Therefore, illegal settings may
     * occur during the setup (for example: First move the loop
     * end point ahead of the loop start point => invalid, then
     * move the loop start point forward => valid again.
     */
  case GEN_STARTADDROFS:              /* SF2.01 section 8.1.3 # 0 */
  case GEN_STARTADDRCOARSEOFS:        /* SF2.01 section 8.1.3 # 4 */
    if (voice->sample != NULL) {
      z = (voice->sample->start
			     + (int) _GEN(voice, GEN_STARTADDROFS)
			     + 32768 * (int) _GEN(voice, GEN_STARTADDRCOARSEOFS));
      UPDATE_RVOICE_I1(fluid_rvoice_set_start, z);
    }
    break;
  case GEN_ENDADDROFS:                 /* SF2.01 section 8.1.3 # 1 */
  case GEN_ENDADDRCOARSEOFS:           /* SF2.01 section 8.1.3 # 12 */
    if (voice->sample != NULL) {
      z = (voice->sample->end
			   + (int) _GEN(voice, GEN_ENDADDROFS)
			   + 32768 * (int) _GEN(voice, GEN_ENDADDRCOARSEOFS));
      UPDATE_RVOICE_I1(fluid_rvoice_set_end, z);
    }
    break;
  case GEN_STARTLOOPADDROFS:           /* SF2.01 section 8.1.3 # 2 */
  case GEN_STARTLOOPADDRCOARSEOFS:     /* SF2.01 section 8.1.3 # 45 */
    if (voice->sample != NULL) {
      z = (voice->sample->loopstart
				  + (int) _GEN(voice, GEN_STARTLOOPADDROFS)
				  + 32768 * (int) _GEN(voice, GEN_STARTLOOPADDRCOARSEOFS));
      UPDATE_RVOICE_I1(fluid_rvoice_set_loopstart, z);
    }
    break;

  case GEN_ENDLOOPADDROFS:             /* SF2.01 section 8.1.3 # 3 */
  case GEN_ENDLOOPADDRCOARSEOFS:       /* SF2.01 section 8.1.3 # 50 */
    if (voice->sample != NULL) {
      z = (voice->sample->loopend
				+ (int) _GEN(voice, GEN_ENDLOOPADDROFS)
				+ 32768 * (int) _GEN(voice, GEN_ENDLOOPADDRCOARSEOFS));
      UPDATE_RVOICE_I1(fluid_rvoice_set_loopend, z);
    }
    break;

    /* Conversion functions differ in range limit */
#define NUM_BUFFERS_DELAY(_v)   (unsigned int) (voice->output_rate * fluid_tc2sec_delay(_v) / FLUID_BUFSIZE)
#define NUM_BUFFERS_ATTACK(_v)  (unsigned int) (voice->output_rate * fluid_tc2sec_attack(_v) / FLUID_BUFSIZE)
#define NUM_BUFFERS_RELEASE(_v) (unsigned int) (voice->output_rate * fluid_tc2sec_release(_v) / FLUID_BUFSIZE)

    /* volume envelope
     *
     * - delay and hold times are converted to absolute number of samples
     * - sustain is converted to its absolute value
     * - attack, decay and release are converted to their increment per sample
     */
  case GEN_VOLENVDELAY:                /* SF2.01 section 8.1.3 # 33 */
    x = _GEN(voice, GEN_VOLENVDELAY);
    fluid_clip(x, -12000.0f, 5000.0f);
    count = NUM_BUFFERS_DELAY(x);
    fluid_voice_update_volenv(voice, FLUID_VOICE_ENVDELAY,
                            count, 0.0f, 0.0f, -1.0f, 1.0f);
    break;

  case GEN_VOLENVATTACK:               /* SF2.01 section 8.1.3 # 34 */
    x = _GEN(voice, GEN_VOLENVATTACK);
    fluid_clip(x, -12000.0f, 8000.0f);
    count = 1 + NUM_BUFFERS_ATTACK(x);
    fluid_voice_update_volenv(voice, FLUID_VOICE_ENVATTACK,
                            count, 1.0f, count ? 1.0f / count : 0.0f, -1.0f, 1.0f);
    break;

  case GEN_VOLENVHOLD:                 /* SF2.01 section 8.1.3 # 35 */
  case GEN_KEYTOVOLENVHOLD:            /* SF2.01 section 8.1.3 # 39 */
    count = calculate_hold_decay_buffers(voice, GEN_VOLENVHOLD, GEN_KEYTOVOLENVHOLD, 0); /* 0 means: hold */
    fluid_voice_update_volenv(voice, FLUID_VOICE_ENVHOLD,
                            count, 1.0f, 0.0f, -1.0f, 2.0f);
    break;

  case GEN_VOLENVDECAY:               /* SF2.01 section 8.1.3 # 36 */
  case GEN_VOLENVSUSTAIN:             /* SF2.01 section 8.1.3 # 37 */
  case GEN_KEYTOVOLENVDECAY:          /* SF2.01 section 8.1.3 # 40 */
    y = 1.0f - 0.001f * _GEN(voice, GEN_VOLENVSUSTAIN);
    fluid_clip(y, 0.0f, 1.0f);
    count = calculate_hold_decay_buffers(voice, GEN_VOLENVDECAY, GEN_KEYTOVOLENVDECAY, 1); /* 1 for decay */
    fluid_voice_update_volenv(voice, FLUID_VOICE_ENVDECAY,
                            count, 1.0f, count ? -1.0f / count : 0.0f, y, 2.0f);
    break;

  case GEN_VOLENVRELEASE:             /* SF2.01 section 8.1.3 # 38 */
    x = _GEN(voice, GEN_VOLENVRELEASE);
    fluid_clip(x, FLUID_MIN_VOLENVRELEASE, 8000.0f);
    count = 1 + NUM_BUFFERS_RELEASE(x);
    fluid_voice_update_volenv(voice, FLUID_VOICE_ENVRELEASE,
                            count, 1.0f, count ? -1.0f / count : 0.0f, 0.0f, 1.0f);
    break;

    /* Modulation envelope */
  case GEN_MODENVDELAY:               /* SF2.01 section 8.1.3 # 25 */
    x = _GEN(voice, GEN_MODENVDELAY);
    fluid_clip(x, -12000.0f, 5000.0f);
    fluid_voice_update_modenv(voice, FLUID_VOICE_ENVDELAY,
                            NUM_BUFFERS_DELAY(x), 0.0f, 0.0f, -1.0f, 1.0f);
    break;

  case GEN_MODENVATTACK:               /* SF2.01 section 8.1.3 # 26 */
    x = _GEN(voice, GEN_MODENVATTACK);
    fluid_clip(x, -12000.0f, 8000.0f);
    count = 1 + NUM_BUFFERS_ATTACK(x);
    fluid_voice_update_modenv(voice, FLUID_VOICE_ENVATTACK,
                            count, 1.0f, count ? 1.0f / count : 0.0f, -1.0f, 1.0f);
    break;

  case GEN_MODENVHOLD:               /* SF2.01 section 8.1.3 # 27 */
  case GEN_KEYTOMODENVHOLD:          /* SF2.01 section 8.1.3 # 31 */
    count = calculate_hold_decay_buffers(voice, GEN_MODENVHOLD, GEN_KEYTOMODENVHOLD, 0); /* 1 means: hold */
    fluid_voice_update_modenv(voice, FLUID_VOICE_ENVHOLD,
                            count, 1.0f, 0.0f, -1.0f, 2.0f);
    break;

  case GEN_MODENVDECAY:                                   /* SF 2.01 section 8.1.3 # 28 */
  case GEN_MODENVSUSTAIN:                                 /* SF 2.01 section 8.1.3 # 29 */
  case GEN_KEYTOMODENVDECAY:                              /* SF 2.01 section 8.1.3 # 32 */
    count = calculate_hold_decay_buffers(voice, GEN_MODENVDECAY, GEN_KEYTOMODENVDECAY, 1); /* 1 for decay */
    y = 1.0f - 0.001f * _GEN(voice, GEN_MODENVSUSTAIN);
    fluid_clip(y, 0.0f, 1.0f);
    fluid_voice_update_modenv(voice, FLUID_VOICE_ENVDECAY,
                            count, 1.0f, count ? -1.0f / count : 0.0f, y, 2.0f);
    break;

  case GEN_MODENVRELEASE:                                  /* SF 2.01 section 8.1.3 # 30 */
    x = _GEN(voice, GEN_MODENVRELEASE);
    fluid_clip(x, -12000.0f, 8000.0f);
    count = 1 + NUM_BUFFERS_RELEASE(x);
    fluid_voice_update_modenv(voice, FLUID_VOICE_ENVRELEASE,
                            count, 1.0f, count ? -1.0f / count : 0.0f, 0.0f, 2.0f);

    break;

  } /* switch gen */
}

/**
 * Recalculate voice parameters for a given control.
 * @param voice the synthesis voice
 * @param cc flag to distinguish between a continous control and a channel control (pitch bend, ...)
 * @param ctrl the control number
 *
 * In this implementation, I want to make sure that all controllers
 * are event based: the parameter values of the DSP algorithm should
 * only be updates when a controller event arrived and not at every
 * iteration of the audio cycle (which would probably be feasible if
 * the synth was made in silicon).
 *
 * The update is done in three steps:
 *
 * - first, we look for all the modulators that have the changed
 * controller as a source. This will yield a list of generators that
 * will be changed because of the controller event.
 *
 * - For every changed generator, calculate its new value. This is the
 * sum of its original value plus the values of al the attached
 * modulators.
 *
 * - For every changed generator, convert its value to the correct
 * unit of the corresponding DSP parameter
 */
int fluid_voice_modulate(fluid_voice_t* voice, int cc, int ctrl)
{
  int i, k;
  fluid_mod_t* mod;
  int gen;
  fluid_real_t modval;

/*    printf("Chan=%d, CC=%d, Src=%d, Val=%d\n", voice->channel->channum, cc, ctrl, val); */

  for (i = 0; i < voice->mod_count; i++) {

    mod = &voice->mod[i];

    /* step 1: find all the modulators that have the changed controller
     * as input source. */
    if (fluid_mod_has_source(mod, cc, ctrl)) {

      gen = fluid_mod_get_dest(mod);
      modval = 0.0;

      /* step 2: for every changed modulator, calculate the modulation
       * value of its associated generator */
      for (k = 0; k < voice->mod_count; k++) {
	if (fluid_mod_has_dest(&voice->mod[k], gen)) {
	  modval += fluid_mod_get_value(&voice->mod[k], voice->channel, voice);
	}
      }

      fluid_gen_set_mod(&voice->gen[gen], modval);

      /* step 3: now that we have the new value of the generator,
       * recalculate the parameter values that are derived from the
       * generator */
      fluid_voice_update_param(voice, gen);
    }
  }
  return FLUID_OK;
}

/**
 * Update all the modulators. This function is called after a
 * ALL_CTRL_OFF MIDI message has been received (CC 121).
 *
 */
int fluid_voice_modulate_all(fluid_voice_t* voice)
{
  fluid_mod_t* mod;
  int i, k, gen;
  fluid_real_t modval;

  /* Loop through all the modulators.

     FIXME: we should loop through the set of generators instead of
     the set of modulators. We risk to call 'fluid_voice_update_param'
     several times for the same generator if several modulators have
     that generator as destination. It's not an error, just a wast of
     energy (think polution, global warming, unhappy musicians,
     ...) */

  for (i = 0; i < voice->mod_count; i++) {

    mod = &voice->mod[i];
    gen = fluid_mod_get_dest(mod);
    modval = 0.0;

    /* Accumulate the modulation values of all the modulators with
     * destination generator 'gen' */
    for (k = 0; k < voice->mod_count; k++) {
      if (fluid_mod_has_dest(&voice->mod[k], gen)) {
	modval += fluid_mod_get_value(&voice->mod[k], voice->channel, voice);
      }
    }

    fluid_gen_set_mod(&voice->gen[gen], modval);

    /* Update the parameter values that are depend on the generator
     * 'gen' */
    fluid_voice_update_param(voice, gen);
  }

  return FLUID_OK;
}

/*
 Force the voice into release stage. Useful anywhere a voice
 needs to be damped even if pedals (sustain sostenuto) are depressed.
 See fluid_synth_damp_voices_by_sustain_LOCAL(),
 fluid_synth_damp_voices_by_sostenuto_LOCAL,
 fluid_voice_noteoff().
*/
void
fluid_voice_release(fluid_voice_t* voice)
{
    unsigned int at_tick = fluid_channel_get_min_note_length_ticks (voice->channel);
    UPDATE_RVOICE_I1(fluid_rvoice_noteoff, at_tick);
    voice->has_noteoff = 1; // voice is marked as noteoff occured
}

/*
 * fluid_voice_noteoff
 */
int
fluid_voice_noteoff(fluid_voice_t* voice)
{
  fluid_channel_t* channel;

  fluid_profile(FLUID_PROF_VOICE_NOTE, voice->ref);

  channel = voice->channel;

  /* Sustain a note under Sostenuto pedal */
  if (fluid_channel_sostenuto(channel) &&
      channel->sostenuto_orderid > voice->id)
  { // Sostenuto depressed after note
    voice->status = FLUID_VOICE_HELD_BY_SOSTENUTO;
  }
  /* Or sustain a note under Sustain pedal */
  else if (fluid_channel_sustained(channel)) {
     voice->status = FLUID_VOICE_SUSTAINED;
  }
  /* Or force the voice to release stage */
  else
    fluid_voice_release(voice);

  return FLUID_OK;
}

/*
 * fluid_voice_kill_excl
 *
 * Percussion sounds can be mutually exclusive: for example, a 'closed
 * hihat' sound will terminate an 'open hihat' sound ringing at the
 * same time. This behaviour is modeled using 'exclusive classes',
 * turning on a voice with an exclusive class other than 0 will kill
 * all other voices having that exclusive class within the same preset
 * or channel.  fluid_voice_kill_excl gets called, when 'voice' is to
 * be killed for that reason.
 */

int
fluid_voice_kill_excl(fluid_voice_t* voice){

  unsigned int at_tick;

  if (!_PLAYING(voice)) {
    return FLUID_OK;
  }

  /* Turn off the exclusive class information for this voice,
     so that it doesn't get killed twice
  */
  fluid_voice_gen_set(voice, GEN_EXCLUSIVECLASS, 0);

  /* Speed up the volume envelope */
  /* The value was found through listening tests with hi-hat samples. */
  fluid_voice_gen_set(voice, GEN_VOLENVRELEASE, -200);
  fluid_voice_update_param(voice, GEN_VOLENVRELEASE);

  /* Speed up the modulation envelope */
  fluid_voice_gen_set(voice, GEN_MODENVRELEASE, -200);
  fluid_voice_update_param(voice, GEN_MODENVRELEASE);

  at_tick = fluid_channel_get_min_note_length_ticks (voice->channel);
  UPDATE_RVOICE_I1(fluid_rvoice_noteoff, at_tick);


  return FLUID_OK;
}

/*
 * Called by fluid_synth when the overflow rvoice can be reclaimed. 
 */
void fluid_voice_overflow_rvoice_finished(fluid_voice_t* voice)
{
  voice->can_access_overflow_rvoice = 1;
  fluid_sample_null_ptr(&voice->overflow_rvoice->dsp.sample);
}


/*
 * fluid_voice_off
 *
 * Purpose:
 * Turns off a voice, meaning that it is not processed
 * anymore by the DSP loop.
 */
int
fluid_voice_off(fluid_voice_t* voice)
{
  fluid_profile(FLUID_PROF_VOICE_RELEASE, voice->ref);

  voice->chan = NO_CHANNEL;
  UPDATE_RVOICE0(fluid_rvoice_voiceoff);
  
  if (voice->can_access_rvoice)
    fluid_sample_null_ptr(&voice->rvoice->dsp.sample);

  voice->status = FLUID_VOICE_OFF;
  voice->has_noteoff = 1;

  /* Decrement the reference count of the sample. */
  fluid_sample_null_ptr(&voice->sample);

  /* Decrement voice count */
  voice->channel->synth->active_voice_count--;

  return FLUID_OK;
}

/**
 * Adds a modulator to the voice.
 * @param voice Voice instance
 * @param mod Modulator info (copied)
 * @param mode Determines how to handle an existing identical modulator
 *   #FLUID_VOICE_ADD to add (offset) the modulator amounts,
 *   #FLUID_VOICE_OVERWRITE to replace the modulator,
 *   #FLUID_VOICE_DEFAULT when adding a default modulator - no duplicate should
 *   exist so don't check.
 */
void
fluid_voice_add_mod(fluid_voice_t* voice, fluid_mod_t* mod, int mode)
{
  int i;

  /*
   * Some soundfonts come with a huge number of non-standard
   * controllers, because they have been designed for one particular
   * sound card.  Discard them, maybe print a warning.
   */

  if (((mod->flags1 & FLUID_MOD_CC) == 0)
      && ((mod->src1 != 0)          /* SF2.01 section 8.2.1: Constant value */
	  && (mod->src1 != 2)       /* Note-on velocity */
	  && (mod->src1 != 3)       /* Note-on key number */
	  && (mod->src1 != 10)      /* Poly pressure */
	  && (mod->src1 != 13)      /* Channel pressure */
	  && (mod->src1 != 14)      /* Pitch wheel */
	  && (mod->src1 != 16))) {  /* Pitch wheel sensitivity */
    FLUID_LOG(FLUID_WARN, "Ignoring invalid controller, using non-CC source %i.", mod->src1);
    return;
  }

  if (mode == FLUID_VOICE_ADD) {

    /* if identical modulator exists, add them */
    for (i = 0; i < voice->mod_count; i++) {
      if (fluid_mod_test_identity(&voice->mod[i], mod)) {
	//		printf("Adding modulator...\n");
	voice->mod[i].amount += mod->amount;
	return;
      }
    }

  } else if (mode == FLUID_VOICE_OVERWRITE) {

    /* if identical modulator exists, replace it (only the amount has to be changed) */
    for (i = 0; i < voice->mod_count; i++) {
      if (fluid_mod_test_identity(&voice->mod[i], mod)) {
	//		printf("Replacing modulator...amount is %f\n",mod->amount);
	voice->mod[i].amount = mod->amount;
	return;
      }
    }
  }

  /* Add a new modulator (No existing modulator to add / overwrite).
     Also, default modulators (FLUID_VOICE_DEFAULT) are added without
     checking, if the same modulator already exists. */
  if (voice->mod_count < FLUID_NUM_MOD) {
    fluid_mod_clone(&voice->mod[voice->mod_count++], mod);
  }
}

/**
 * Get the unique ID of the noteon-event.
 * @param voice Voice instance
 * @return Note on unique ID
 *
 * A SoundFont loader may store the voice processes it has created for
 * real-time control during the operation of a voice (for example: parameter
 * changes in SoundFont editor). The synth uses a pool of voices, which are
 * 'recycled' and never deallocated.
 *
 * Before modifying an existing voice, check
 * - that its state is still 'playing'
 * - that the ID is still the same
 *
 * Otherwise the voice has finished playing.
 */
unsigned int fluid_voice_get_id(fluid_voice_t* voice)
{
  return voice->id;
}

/**
 * Check if a voice is still playing.
 * @param voice Voice instance
 * @return TRUE if playing, FALSE otherwise
 */
int fluid_voice_is_playing(fluid_voice_t* voice)
{
  return _PLAYING(voice);
}

/*
 * fluid_voice_get_lower_boundary_for_attenuation
 *
 * Purpose:
 *
 * A lower boundary for the attenuation (as in 'the minimum
 * attenuation of this voice, with volume pedals, modulators
 * etc. resulting in minimum attenuation, cannot fall below x cB) is
 * calculated.  This has to be called during fluid_voice_init, after
 * all modulators have been run on the voice once.  Also,
 * voice->attenuation has to be initialized.
 */
static fluid_real_t
fluid_voice_get_lower_boundary_for_attenuation(fluid_voice_t* voice)
{
  int i;
  fluid_mod_t* mod;
  fluid_real_t possible_att_reduction_cB=0;
  fluid_real_t lower_bound;

  for (i = 0; i < voice->mod_count; i++) {
    mod = &voice->mod[i];

    /* Modulator has attenuation as target and can change over time? */
    if ((mod->dest == GEN_ATTENUATION)
	&& ((mod->flags1 & FLUID_MOD_CC) || (mod->flags2 & FLUID_MOD_CC))) {

      fluid_real_t current_val = fluid_mod_get_value(mod, voice->channel, voice);
      fluid_real_t v = fabs(mod->amount);

      if ((mod->src1 == FLUID_MOD_PITCHWHEEL)
	  || (mod->flags1 & FLUID_MOD_BIPOLAR)
	  || (mod->flags2 & FLUID_MOD_BIPOLAR)
	  || (mod->amount < 0)) {
	/* Can this modulator produce a negative contribution? */
	v *= -1.0;
      } else {
	/* No negative value possible. But still, the minimum contribution is 0. */
	v = 0;
      }

      /* For example:
       * - current_val=100
       * - min_val=-4000
       * - possible_att_reduction_cB += 4100
       */
      if (current_val > v){
	possible_att_reduction_cB += (current_val - v);
      }
    }
  }

  lower_bound = voice->attenuation-possible_att_reduction_cB;

  /* SF2.01 specs do not allow negative attenuation */
  if (lower_bound < 0) {
    lower_bound = 0;
  }
  return lower_bound;
}




int fluid_voice_set_param(fluid_voice_t* voice, int gen, fluid_real_t nrpn_value, int abs)
{
  voice->gen[gen].nrpn = nrpn_value;
  voice->gen[gen].flags = (abs)? GEN_ABS_NRPN : GEN_SET;
  fluid_voice_update_param(voice, gen);
  return FLUID_OK;
}

int fluid_voice_set_gain(fluid_voice_t* voice, fluid_real_t gain)
{
  /* avoid division by zero*/
  if (gain < 0.0000001){
    gain = 0.0000001;
  }

  voice->synth_gain = gain;
  voice->amp_left = fluid_pan(voice->pan, 1) * gain / 32768.0f;
  voice->amp_right = fluid_pan(voice->pan, 0) * gain / 32768.0f;
  voice->amp_reverb = voice->reverb_send * gain / 32768.0f;
  voice->amp_chorus = voice->chorus_send * gain / 32768.0f;

  UPDATE_RVOICE_R1(fluid_rvoice_set_synth_gain, gain);
  UPDATE_RVOICE_BUFFERS2(fluid_rvoice_buffers_set_amp, 0, voice->amp_left);
  UPDATE_RVOICE_BUFFERS2(fluid_rvoice_buffers_set_amp, 1, voice->amp_right);
  UPDATE_RVOICE_BUFFERS2(fluid_rvoice_buffers_set_amp, 2, voice->amp_reverb);
  UPDATE_RVOICE_BUFFERS2(fluid_rvoice_buffers_set_amp, 3, voice->amp_chorus);

  return FLUID_OK;
}

/* - Scan the loop
 * - determine the peak level
 * - Calculate, what factor will make the loop inaudible
 * - Store in sample
 */
/**
 * Calculate the peak volume of a sample for voice off optimization.
 * @param s Sample to optimize
 * @return #FLUID_OK on success, #FLUID_FAILED otherwise
 *
 * If the peak volume during the loop is known, then the voice can
 * be released earlier during the release phase. Otherwise, the
 * voice will operate (inaudibly), until the envelope is at the
 * nominal turnoff point.  So it's a good idea to call
 * fluid_voice_optimize_sample() on each sample once.
 */
int
fluid_voice_optimize_sample(fluid_sample_t* s)
{
  signed short peak_max = 0;
  signed short peak_min = 0;
  signed short peak;
  fluid_real_t normalized_amplitude_during_loop;
  double result;
  int i;

  /* ignore ROM and other(?) invalid samples */
  if (!s->valid) return (FLUID_OK);

  if (!s->amplitude_that_reaches_noise_floor_is_valid){ /* Only once */
    /* Scan the loop */
    for (i = (int)s->loopstart; i < (int) s->loopend; i ++){
      signed short val = s->data[i];
      if (val > peak_max) {
	peak_max = val;
      } else if (val < peak_min) {
	peak_min = val;
      }
    }

    /* Determine the peak level */
    if (peak_max >- peak_min){
      peak = peak_max;
    } else {
      peak =- peak_min;
    };
    if (peak == 0){
      /* Avoid division by zero */
      peak = 1;
    };

    /* Calculate what factor will make the loop inaudible
     * For example: Take a peak of 3277 (10 % of 32768).  The
     * normalized amplitude is 0.1 (10 % of 32768).  An amplitude
     * factor of 0.0001 (as opposed to the default 0.00001) will
     * drop this sample to the noise floor.
     */

    /* 16 bits => 96+4=100 dB dynamic range => 0.00001 */
    normalized_amplitude_during_loop = ((fluid_real_t)peak)/32768.;
    result = FLUID_NOISE_FLOOR / normalized_amplitude_during_loop;

    /* Store in sample */
    s->amplitude_that_reaches_noise_floor = (double)result;
    s->amplitude_that_reaches_noise_floor_is_valid = 1;
#if 0
    printf("Sample peak detection: factor %f\n", (double)result);
#endif
  };
  return FLUID_OK;
}

fluid_real_t 
fluid_voice_get_overflow_prio(fluid_voice_t* voice, 
			       fluid_overflow_prio_t* score,
			       unsigned int cur_time)
{
  fluid_real_t this_voice_prio = 0;

  /* Are we already overflowing? */
  if (!voice->can_access_overflow_rvoice) {
    return OVERFLOW_PRIO_CANNOT_KILL;
  }

  /* Is this voice on the drum channel?
   * Then it is very important.
   * Also skip the released and sustained scores.
   */
  if (voice->channel->channel_type == CHANNEL_TYPE_DRUM){
    this_voice_prio += score->percussion;
  } 
  else if (voice->has_noteoff) {
    /* Noteoff has */
    this_voice_prio += score->released;
  } else if (_SUSTAINED(voice) || _HELD_BY_SOSTENUTO(voice)) {
    /* This voice is still active, since the sustain pedal is held down.
     * Consider it less important than non-sustained channels.
     * This decision is somehow subjective. But usually the sustain pedal
     * is used to play 'more-voices-than-fingers', so it shouldn't hurt
     * if we kill one voice.
     */
    this_voice_prio += score->sustained;
  }

  /* We are not enthusiastic about releasing voices, which have just been started.
   * Otherwise hitting a chord may result in killing notes belonging to that very same
   * chord. So give newer voices a higher score. */
  if (score->age) {
    cur_time -= voice->start_time;
    if (cur_time < 1) 
      cur_time = 1; // Avoid div by zero
    this_voice_prio += (score->age * voice->output_rate) / cur_time;
  }

  /* take a rough estimate of loudness into account. Louder voices are more important. */
  if (score->volume) {
    fluid_real_t a = voice->attenuation;
    if (voice->has_noteoff) {
      // FIXME: Should take into account where on the envelope we are...?
    }
    if (a < 0.1) 
      a = 0.1; // Avoid div by zero
      this_voice_prio += score->volume / a;
    }
    
  return this_voice_prio;
}
