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

#include <math.h>

#include "fluid_synth.h"
#include "fluid_sys.h"
#include "fluid_chan.h"
#include "fluid_tuning.h"
#include "fluid_settings.h"
#include "fluid_sfont.h"
#include "fluid_hash.h"
#include "fluid_defsfont.h"

#ifdef TRAP_ON_FPE
#define _GNU_SOURCE
#include <fenv.h>

/* seems to not be declared in fenv.h */
extern int feenableexcept (int excepts);
#endif

static void fluid_synth_init(void);

static int fluid_synth_noteon_LOCAL(fluid_synth_t* synth, int chan, int key,
                                       int vel);
static int fluid_synth_noteoff_LOCAL(fluid_synth_t* synth, int chan, int key);
static int fluid_synth_cc_LOCAL(fluid_synth_t* synth, int channum, int num);
static int fluid_synth_update_device_id (fluid_synth_t *synth, char *name,
                                         int value);
static int fluid_synth_update_overflow (fluid_synth_t *synth, char *name,
                                         fluid_real_t value);
static int fluid_synth_sysex_midi_tuning (fluid_synth_t *synth, const char *data,
                                          int len, char *response,
                                          int *response_len, int avail_response,
                                          int *handled, int dryrun);
static int fluid_synth_all_notes_off_LOCAL(fluid_synth_t* synth, int chan);
static int fluid_synth_all_sounds_off_LOCAL(fluid_synth_t* synth, int chan);
static int fluid_synth_system_reset_LOCAL(fluid_synth_t* synth);
static int fluid_synth_modulate_voices_LOCAL(fluid_synth_t* synth, int chan,
                                             int is_cc, int ctrl);
static int fluid_synth_modulate_voices_all_LOCAL(fluid_synth_t* synth, int chan);
static int fluid_synth_update_channel_pressure_LOCAL(fluid_synth_t* synth, int channum);
static int fluid_synth_update_pitch_bend_LOCAL(fluid_synth_t* synth, int chan);
static int fluid_synth_update_pitch_wheel_sens_LOCAL(fluid_synth_t* synth, int chan);
static int fluid_synth_set_preset (fluid_synth_t *synth, int chan,
                                   fluid_preset_t *preset);
static fluid_preset_t*
fluid_synth_get_preset(fluid_synth_t* synth, unsigned int sfontnum,
                       unsigned int banknum, unsigned int prognum);
static fluid_preset_t*
fluid_synth_get_preset_by_sfont_name(fluid_synth_t* synth, const char *sfontname,
                                     unsigned int banknum, unsigned int prognum);

static void fluid_synth_update_presets(fluid_synth_t* synth);
static int fluid_synth_update_sample_rate(fluid_synth_t* synth,
                                   char* name, double value);
static int fluid_synth_update_gain(fluid_synth_t* synth,
                                   char* name, double value);
static void fluid_synth_update_gain_LOCAL(fluid_synth_t* synth);
static int fluid_synth_update_polyphony(fluid_synth_t* synth,
                                        char* name, int value);
static int fluid_synth_update_polyphony_LOCAL(fluid_synth_t* synth, int new_polyphony);
static void init_dither(void);
static inline int roundi (float x);
static int fluid_synth_render_blocks(fluid_synth_t* synth, int blockcount);
//static void fluid_synth_core_thread_func (void* data);
//static FLUID_INLINE void fluid_synth_process_event_queue_LOCAL
//  (fluid_synth_t *synth, fluid_event_queue_t *queue);
static fluid_voice_t* fluid_synth_free_voice_by_kill_LOCAL(fluid_synth_t* synth);
static void fluid_synth_kill_by_exclusive_class_LOCAL(fluid_synth_t* synth,
                                                      fluid_voice_t* new_voice);
static fluid_sfont_info_t *new_fluid_sfont_info (fluid_synth_t *synth,
                                                 fluid_sfont_t *sfont);
static int fluid_synth_sfunload_callback(void* data, unsigned int msec);
static void fluid_synth_release_voice_on_same_note_LOCAL(fluid_synth_t* synth,
                                                            int chan, int key);
static fluid_tuning_t* fluid_synth_get_tuning(fluid_synth_t* synth,
                                              int bank, int prog);
static int fluid_synth_replace_tuning_LOCK (fluid_synth_t* synth,
                                            fluid_tuning_t *tuning,
                                            int bank, int prog, int apply);
static void fluid_synth_replace_tuning_LOCAL (fluid_synth_t *synth,
                                              fluid_tuning_t *old_tuning,
                                              fluid_tuning_t *new_tuning,
                                              int apply, int unref_new);
static void fluid_synth_update_voice_tuning_LOCAL (fluid_synth_t *synth,
                                                   fluid_channel_t *channel);
static int fluid_synth_set_tuning_LOCAL (fluid_synth_t *synth, int chan,
                                         fluid_tuning_t *tuning, int apply);
static void fluid_synth_set_gen_LOCAL (fluid_synth_t* synth, int chan,
                                       int param, float value, int absolute);
static void fluid_synth_stop_LOCAL (fluid_synth_t *synth, unsigned int id);



/***************************************************************
 *
 *                         GLOBAL
 */

/* has the synth module been initialized? */
static int fluid_synth_initialized = 0;
static void fluid_synth_init(void);
static void init_dither(void);

/* default modulators
 * SF2.01 page 52 ff:
 *
 * There is a set of predefined default modulators. They have to be
 * explicitly overridden by the sound font in order to turn them off.
 */

fluid_mod_t default_vel2att_mod;        /* SF2.01 section 8.4.1  */
fluid_mod_t default_vel2filter_mod;     /* SF2.01 section 8.4.2  */
fluid_mod_t default_at2viblfo_mod;      /* SF2.01 section 8.4.3  */
fluid_mod_t default_mod2viblfo_mod;     /* SF2.01 section 8.4.4  */
fluid_mod_t default_att_mod;            /* SF2.01 section 8.4.5  */
fluid_mod_t default_pan_mod;            /* SF2.01 section 8.4.6  */
fluid_mod_t default_expr_mod;           /* SF2.01 section 8.4.7  */
fluid_mod_t default_reverb_mod;         /* SF2.01 section 8.4.8  */
fluid_mod_t default_chorus_mod;         /* SF2.01 section 8.4.9  */
fluid_mod_t default_pitch_bend_mod;     /* SF2.01 section 8.4.10 */

/* reverb presets */
static fluid_revmodel_presets_t revmodel_preset[] = {
  /* name */    /* roomsize */ /* damp */ /* width */ /* level */
  { "Test 1",          0.2f,      0.0f,       0.5f,       0.9f },
  { "Test 2",          0.4f,      0.2f,       0.5f,       0.8f },
  { "Test 3",          0.6f,      0.4f,       0.5f,       0.7f },
  { "Test 4",          0.8f,      0.7f,       0.5f,       0.6f },
  { "Test 5",          0.8f,      1.0f,       0.5f,       0.5f },
  { NULL, 0.0f, 0.0f, 0.0f, 0.0f }
};


/***************************************************************
 *
 *               INITIALIZATION & UTILITIES
 */

static void fluid_synth_register_overflow(fluid_settings_t* settings,
					  fluid_num_update_t update_func,
					  void* update_data)
{
  fluid_settings_register_num(settings, "synth.overflow.percussion",
			      4000, -10000, 10000, 0, update_func, update_data);
  fluid_settings_register_num(settings, "synth.overflow.sustained",
			      -1000, -10000, 10000, 0, update_func, update_data);
  fluid_settings_register_num(settings, "synth.overflow.released",
			      -2000, -10000, 10000, 0, update_func, update_data);
  fluid_settings_register_num(settings, "synth.overflow.age",
			      1000, -10000, 10000, 0, update_func, update_data);
  fluid_settings_register_num(settings, "synth.overflow.volume",
			      500, -10000, 10000, 0, update_func, update_data);  
}

void fluid_synth_settings(fluid_settings_t* settings)
{
  fluid_settings_register_int(settings, "synth.verbose", 0, 0, 1,
                              FLUID_HINT_TOGGLED, NULL, NULL);
  fluid_settings_register_int(settings, "synth.dump", 0, 0, 1,
                              FLUID_HINT_TOGGLED, NULL, NULL);
  fluid_settings_register_int(settings, "synth.reverb.active", 1, 0, 1,
                              FLUID_HINT_TOGGLED, NULL, NULL);
  fluid_settings_register_int(settings, "synth.chorus.active", 1, 0, 1,
                              FLUID_HINT_TOGGLED, NULL, NULL);
  fluid_settings_register_int(settings, "synth.ladspa.active", 0, 0, 1,
                              FLUID_HINT_TOGGLED, NULL, NULL);
  fluid_settings_register_int(settings, "synth.lock-memory", 1, 0, 1,
                              FLUID_HINT_TOGGLED, NULL, NULL);
  fluid_settings_register_str(settings, "midi.portname", "", 0, NULL, NULL);

  fluid_settings_register_str(settings, "synth.default-soundfont",
			      DEFAULT_SOUNDFONT, 0, NULL, NULL);

  fluid_settings_register_int(settings, "synth.polyphony",
			      256, 1, 65535, 0, NULL, NULL);
  fluid_settings_register_int(settings, "synth.midi-channels",
			      16, 16, 256, 0, NULL, NULL);
  fluid_settings_register_num(settings, "synth.gain",
			      0.2f, 0.0f, 10.0f,
			      0, NULL, NULL);
  fluid_settings_register_int(settings, "synth.audio-channels",
			      1, 1, 128, 0, NULL, NULL);
  fluid_settings_register_int(settings, "synth.audio-groups",
			      1, 1, 128, 0, NULL, NULL);
  fluid_settings_register_int(settings, "synth.effects-channels",
			      2, 2, 2, 0, NULL, NULL);
  fluid_settings_register_num(settings, "synth.sample-rate",
			      44100.0f, 8000.0f, 96000.0f,
			      0, NULL, NULL);
  fluid_settings_register_int(settings, "synth.device-id",
			      0, 0, 126, 0, NULL, NULL);
  fluid_settings_register_int(settings, "synth.cpu-cores", 1, 1, 256, 0, NULL, NULL);

  fluid_settings_register_int(settings, "synth.min-note-length", 10, 0, 65535, 0, NULL, NULL);
  
  fluid_settings_register_int(settings, "synth.threadsafe-api", 1, 0, 1,
                              FLUID_HINT_TOGGLED, NULL, NULL);
  fluid_settings_register_int(settings, "synth.parallel-render", 1, 0, 1,
                              FLUID_HINT_TOGGLED, NULL, NULL);

  fluid_synth_register_overflow(settings, NULL, NULL);

  fluid_settings_register_str(settings, "synth.midi-bank-select", "gs", 0, NULL, NULL);
  fluid_settings_add_option(settings, "synth.midi-bank-select", "gm");
  fluid_settings_add_option(settings, "synth.midi-bank-select", "gs");
  fluid_settings_add_option(settings, "synth.midi-bank-select", "xg");
  fluid_settings_add_option(settings, "synth.midi-bank-select", "mma");
  
}

/**
 * Get FluidSynth runtime version.
 * @param major Location to store major number
 * @param minor Location to store minor number
 * @param micro Location to store micro number
 */
void fluid_version(int *major, int *minor, int *micro)
{
  *major = FLUIDSYNTH_VERSION_MAJOR;
  *minor = FLUIDSYNTH_VERSION_MINOR;
  *micro = FLUIDSYNTH_VERSION_MICRO;
}

/**
 * Get FluidSynth runtime version as a string.
 * @return FluidSynth version string, which is internal and should not be
 *   modified or freed.
 */
char *
fluid_version_str (void)
{
  return FLUIDSYNTH_VERSION;
}

#define FLUID_API_ENTRY_CHAN(fail_value)  \
  fluid_return_val_if_fail (synth != NULL, fail_value); \
  fluid_return_val_if_fail (chan >= 0, fail_value); \
  fluid_synth_api_enter(synth); \
  if (chan >= synth->midi_channels) { \
    fluid_synth_api_exit(synth); \
    return fail_value; \
  } \

#define FLUID_API_RETURN(return_value) \
  do { fluid_synth_api_exit(synth); \
  return return_value; } while (0)
  
/*
 * void fluid_synth_init
 *
 * Does all the initialization for this module.
 */
static void
fluid_synth_init(void)
{
  fluid_synth_initialized++;

#ifdef TRAP_ON_FPE
  /* Turn on floating point exception traps */
  feenableexcept (FE_DIVBYZERO | FE_UNDERFLOW | FE_OVERFLOW | FE_INVALID);
#endif

  fluid_conversion_config();

  fluid_rvoice_dsp_config();

  fluid_sys_config();

  init_dither();


  /* SF2.01 page 53 section 8.4.1: MIDI Note-On Velocity to Initial Attenuation */
  fluid_mod_set_source1(&default_vel2att_mod, /* The modulator we are programming here */
		       FLUID_MOD_VELOCITY,    /* Source. VELOCITY corresponds to 'index=2'. */
		       FLUID_MOD_GC           /* Not a MIDI continuous controller */
		       | FLUID_MOD_CONCAVE    /* Curve shape. Corresponds to 'type=1' */
		       | FLUID_MOD_UNIPOLAR   /* Polarity. Corresponds to 'P=0' */
		       | FLUID_MOD_NEGATIVE   /* Direction. Corresponds to 'D=1' */
		       );
  fluid_mod_set_source2(&default_vel2att_mod, 0, 0); /* No 2nd source */
  fluid_mod_set_dest(&default_vel2att_mod, GEN_ATTENUATION);  /* Target: Initial attenuation */
  fluid_mod_set_amount(&default_vel2att_mod, 960.0);          /* Modulation amount: 960 */



  /* SF2.01 page 53 section 8.4.2: MIDI Note-On Velocity to Filter Cutoff
   * Have to make a design decision here. The specs don't make any sense this way or another.
   * One sound font, 'Kingston Piano', which has been praised for its quality, tries to
   * override this modulator with an amount of 0 and positive polarity (instead of what
   * the specs say, D=1) for the secondary source.
   * So if we change the polarity to 'positive', one of the best free sound fonts works...
   */
  fluid_mod_set_source1(&default_vel2filter_mod, FLUID_MOD_VELOCITY, /* Index=2 */
		       FLUID_MOD_GC                        /* CC=0 */
		       | FLUID_MOD_LINEAR                  /* type=0 */
		       | FLUID_MOD_UNIPOLAR                /* P=0 */
                       | FLUID_MOD_NEGATIVE                /* D=1 */
		       );
  fluid_mod_set_source2(&default_vel2filter_mod, FLUID_MOD_VELOCITY, /* Index=2 */
		       FLUID_MOD_GC                                 /* CC=0 */
		       | FLUID_MOD_SWITCH                           /* type=3 */
		       | FLUID_MOD_UNIPOLAR                         /* P=0 */
		       // do not remove       | FLUID_MOD_NEGATIVE                         /* D=1 */
		       | FLUID_MOD_POSITIVE                         /* D=0 */
		       );
  fluid_mod_set_dest(&default_vel2filter_mod, GEN_FILTERFC);        /* Target: Initial filter cutoff */
  fluid_mod_set_amount(&default_vel2filter_mod, -2400);



  /* SF2.01 page 53 section 8.4.3: MIDI Channel pressure to Vibrato LFO pitch depth */
  fluid_mod_set_source1(&default_at2viblfo_mod, FLUID_MOD_CHANNELPRESSURE, /* Index=13 */
		       FLUID_MOD_GC                        /* CC=0 */
		       | FLUID_MOD_LINEAR                  /* type=0 */
		       | FLUID_MOD_UNIPOLAR                /* P=0 */
		       | FLUID_MOD_POSITIVE                /* D=0 */
		       );
  fluid_mod_set_source2(&default_at2viblfo_mod, 0,0); /* no second source */
  fluid_mod_set_dest(&default_at2viblfo_mod, GEN_VIBLFOTOPITCH);        /* Target: Vib. LFO => pitch */
  fluid_mod_set_amount(&default_at2viblfo_mod, 50);



  /* SF2.01 page 53 section 8.4.4: Mod wheel (Controller 1) to Vibrato LFO pitch depth */
  fluid_mod_set_source1(&default_mod2viblfo_mod, 1, /* Index=1 */
		       FLUID_MOD_CC                        /* CC=1 */
		       | FLUID_MOD_LINEAR                  /* type=0 */
		       | FLUID_MOD_UNIPOLAR                /* P=0 */
		       | FLUID_MOD_POSITIVE                /* D=0 */
		       );
  fluid_mod_set_source2(&default_mod2viblfo_mod, 0,0); /* no second source */
  fluid_mod_set_dest(&default_mod2viblfo_mod, GEN_VIBLFOTOPITCH);        /* Target: Vib. LFO => pitch */
  fluid_mod_set_amount(&default_mod2viblfo_mod, 50);



  /* SF2.01 page 55 section 8.4.5: MIDI continuous controller 7 to initial attenuation*/
  fluid_mod_set_source1(&default_att_mod, 7,                     /* index=7 */
		       FLUID_MOD_CC                              /* CC=1 */
		       | FLUID_MOD_CONCAVE                       /* type=1 */
		       | FLUID_MOD_UNIPOLAR                      /* P=0 */
		       | FLUID_MOD_NEGATIVE                      /* D=1 */
		       );
  fluid_mod_set_source2(&default_att_mod, 0, 0);                 /* No second source */
  fluid_mod_set_dest(&default_att_mod, GEN_ATTENUATION);         /* Target: Initial attenuation */
  fluid_mod_set_amount(&default_att_mod, 960.0);                 /* Amount: 960 */



  /* SF2.01 page 55 section 8.4.6 MIDI continuous controller 10 to Pan Position */
  fluid_mod_set_source1(&default_pan_mod, 10,                    /* index=10 */
		       FLUID_MOD_CC                              /* CC=1 */
		       | FLUID_MOD_LINEAR                        /* type=0 */
		       | FLUID_MOD_BIPOLAR                       /* P=1 */
		       | FLUID_MOD_POSITIVE                      /* D=0 */
		       );
  fluid_mod_set_source2(&default_pan_mod, 0, 0);                 /* No second source */
  fluid_mod_set_dest(&default_pan_mod, GEN_PAN);                 /* Target: pan */
  /* Amount: 500. The SF specs $8.4.6, p. 55 syas: "Amount = 1000
     tenths of a percent". The center value (64) corresponds to 50%,
     so it follows that amount = 50% x 1000/% = 500. */
  fluid_mod_set_amount(&default_pan_mod, 500.0);


  /* SF2.01 page 55 section 8.4.7: MIDI continuous controller 11 to initial attenuation*/
  fluid_mod_set_source1(&default_expr_mod, 11,                     /* index=11 */
		       FLUID_MOD_CC                              /* CC=1 */
		       | FLUID_MOD_CONCAVE                       /* type=1 */
		       | FLUID_MOD_UNIPOLAR                      /* P=0 */
		       | FLUID_MOD_NEGATIVE                      /* D=1 */
		       );
  fluid_mod_set_source2(&default_expr_mod, 0, 0);                 /* No second source */
  fluid_mod_set_dest(&default_expr_mod, GEN_ATTENUATION);         /* Target: Initial attenuation */
  fluid_mod_set_amount(&default_expr_mod, 960.0);                 /* Amount: 960 */



  /* SF2.01 page 55 section 8.4.8: MIDI continuous controller 91 to Reverb send */
  fluid_mod_set_source1(&default_reverb_mod, 91,                 /* index=91 */
		       FLUID_MOD_CC                              /* CC=1 */
		       | FLUID_MOD_LINEAR                        /* type=0 */
		       | FLUID_MOD_UNIPOLAR                      /* P=0 */
		       | FLUID_MOD_POSITIVE                      /* D=0 */
		       );
  fluid_mod_set_source2(&default_reverb_mod, 0, 0);              /* No second source */
  fluid_mod_set_dest(&default_reverb_mod, GEN_REVERBSEND);       /* Target: Reverb send */
  fluid_mod_set_amount(&default_reverb_mod, 200);                /* Amount: 200 ('tenths of a percent') */



  /* SF2.01 page 55 section 8.4.9: MIDI continuous controller 93 to Chorus send */
  fluid_mod_set_source1(&default_chorus_mod, 93,                 /* index=93 */
		       FLUID_MOD_CC                              /* CC=1 */
		       | FLUID_MOD_LINEAR                        /* type=0 */
		       | FLUID_MOD_UNIPOLAR                      /* P=0 */
		       | FLUID_MOD_POSITIVE                      /* D=0 */
		       );
  fluid_mod_set_source2(&default_chorus_mod, 0, 0);              /* No second source */
  fluid_mod_set_dest(&default_chorus_mod, GEN_CHORUSSEND);       /* Target: Chorus */
  fluid_mod_set_amount(&default_chorus_mod, 200);                /* Amount: 200 ('tenths of a percent') */



  /* SF2.01 page 57 section 8.4.10 MIDI Pitch Wheel to Initial Pitch ... */
  fluid_mod_set_source1(&default_pitch_bend_mod, FLUID_MOD_PITCHWHEEL, /* Index=14 */
		       FLUID_MOD_GC                              /* CC =0 */
		       | FLUID_MOD_LINEAR                        /* type=0 */
		       | FLUID_MOD_BIPOLAR                       /* P=1 */
		       | FLUID_MOD_POSITIVE                      /* D=0 */
		       );
  fluid_mod_set_source2(&default_pitch_bend_mod, FLUID_MOD_PITCHWHEELSENS,  /* Index = 16 */
		       FLUID_MOD_GC                                        /* CC=0 */
		       | FLUID_MOD_LINEAR                                  /* type=0 */
		       | FLUID_MOD_UNIPOLAR                                /* P=0 */
		       | FLUID_MOD_POSITIVE                                /* D=0 */
		       );
  fluid_mod_set_dest(&default_pitch_bend_mod, GEN_PITCH);                 /* Destination: Initial pitch */
  fluid_mod_set_amount(&default_pitch_bend_mod, 12700.0);                 /* Amount: 12700 cents */
}

static FLUID_INLINE unsigned int fluid_synth_get_ticks(fluid_synth_t* synth)
{
  if (synth->eventhandler->is_threadsafe)
    return fluid_atomic_int_get(&synth->ticks_since_start);
  else
    return synth->ticks_since_start;
}

static FLUID_INLINE void fluid_synth_add_ticks(fluid_synth_t* synth, int val)
{
  if (synth->eventhandler->is_threadsafe)
    fluid_atomic_int_add((int*) &synth->ticks_since_start, val);
  else
    synth->ticks_since_start += val;
}


/***************************************************************
 *                    FLUID SAMPLE TIMERS 
 *    Timers that use written audio data as timing reference       
 */
struct _fluid_sample_timer_t
{
	fluid_sample_timer_t* next; /* Single linked list of timers */
	unsigned long starttick;
	fluid_timer_callback_t callback;
	void* data;
	int isfinished;
};

/*
 * fluid_sample_timer_process - called when synth->ticks is updated
 */
static void fluid_sample_timer_process(fluid_synth_t* synth)
{
	fluid_sample_timer_t* st;
	long msec;
	int cont;
        unsigned int ticks = fluid_synth_get_ticks(synth);
	
	for (st=synth->sample_timers; st; st=st->next) {
		if (st->isfinished) {
			continue;
		}

		msec = (long) (1000.0*((double) (ticks - st->starttick))/synth->sample_rate);
		cont = (*st->callback)(st->data, msec);
		if (cont == 0) {
			st->isfinished = 1;
		}
	}
}

fluid_sample_timer_t* new_fluid_sample_timer(fluid_synth_t* synth, fluid_timer_callback_t callback, void* data)
{
	fluid_sample_timer_t* result = FLUID_NEW(fluid_sample_timer_t);
	if (result == NULL) {
		FLUID_LOG(FLUID_ERR, "Out of memory");
		return NULL;
	}
	result->starttick = fluid_synth_get_ticks(synth);
	result->isfinished = 0;
	result->data = data;
	result->callback = callback;
	result->next = synth->sample_timers;
	synth->sample_timers = result;
	return result;		
}

int delete_fluid_sample_timer(fluid_synth_t* synth, fluid_sample_timer_t* timer)
{
	fluid_sample_timer_t** ptr = &synth->sample_timers;
	while (*ptr) {
		if (*ptr == timer) {
			*ptr = timer->next; 
			FLUID_FREE(timer);
			return FLUID_OK;
		}
		ptr = &((*ptr)->next);
	}
	FLUID_LOG(FLUID_ERR,"delete_fluid_sample_timer failed, no timer found");
	return FLUID_FAILED;
}


/***************************************************************
 *
 *                      FLUID SYNTH
 */

static FLUID_INLINE void
fluid_synth_update_mixer(fluid_synth_t* synth, void* method, int intparam,
			 fluid_real_t realparam)
{
  fluid_return_if_fail(synth != NULL || synth->eventhandler != NULL);
  fluid_return_if_fail(synth->eventhandler->mixer != NULL);
  fluid_rvoice_eventhandler_push(synth->eventhandler, method, 
				 synth->eventhandler->mixer,
				 intparam, realparam);
}


/**
 * Create new FluidSynth instance.
 * @param settings Configuration parameters to use (used directly).
 * @return New FluidSynth instance or NULL on error
 *
 * NOTE: The settings parameter is used directly and should not be modified
 * or freed independently.
 */
fluid_synth_t*
new_fluid_synth(fluid_settings_t *settings)
{
  fluid_synth_t* synth;
  fluid_sfloader_t* loader;
  double gain;
  int i, nbuf;

  /* initialize all the conversion tables and other stuff */
  if (fluid_synth_initialized == 0) {
    fluid_synth_init();
  }

  /* allocate a new synthesizer object */
  synth = FLUID_NEW(fluid_synth_t);
  if (synth == NULL) {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }
  FLUID_MEMSET(synth, 0, sizeof(fluid_synth_t));

  fluid_rec_mutex_init(synth->mutex);
  fluid_settings_getint(settings, "synth.threadsafe-api", &synth->use_mutex);
  synth->public_api_count = 0;
  
  synth->settings = settings;

  fluid_settings_getint(settings, "synth.reverb.active", &synth->with_reverb);
  fluid_settings_getint(settings, "synth.chorus.active", &synth->with_chorus);
  fluid_settings_getint(settings, "synth.verbose", &synth->verbose);
  fluid_settings_getint(settings, "synth.dump", &synth->dump);

  fluid_settings_getint(settings, "synth.polyphony", &synth->polyphony);
  fluid_settings_getnum(settings, "synth.sample-rate", &synth->sample_rate);
  fluid_settings_getint(settings, "synth.midi-channels", &synth->midi_channels);
  fluid_settings_getint(settings, "synth.audio-channels", &synth->audio_channels);
  fluid_settings_getint(settings, "synth.audio-groups", &synth->audio_groups);
  fluid_settings_getint(settings, "synth.effects-channels", &synth->effects_channels);
  fluid_settings_getnum(settings, "synth.gain", &gain);
  synth->gain = gain;
  fluid_settings_getint(settings, "synth.device-id", &synth->device_id);
  fluid_settings_getint(settings, "synth.cpu-cores", &synth->cores);

  /* register the callbacks */
  fluid_settings_register_num(settings, "synth.sample-rate",
			      44100.0f, 8000.0f, 96000.0f, 0,
			      (fluid_num_update_t) fluid_synth_update_sample_rate, synth);
  fluid_settings_register_num(settings, "synth.gain",
			      0.2f, 0.0f, 10.0f, 0,
			      (fluid_num_update_t) fluid_synth_update_gain, synth);
  fluid_settings_register_int(settings, "synth.polyphony",
			      synth->polyphony, 1, 65535, 0,
			      (fluid_int_update_t) fluid_synth_update_polyphony,
                              synth);
  fluid_settings_register_int(settings, "synth.device-id",
			      synth->device_id, 126, 0, 0,
                              (fluid_int_update_t) fluid_synth_update_device_id, synth);

  fluid_synth_register_overflow(settings, 
				(fluid_num_update_t) fluid_synth_update_overflow, synth);
				
  /* do some basic sanity checking on the settings */

  if (synth->midi_channels % 16 != 0) {
    int n = synth->midi_channels / 16;
    synth->midi_channels = (n + 1) * 16;
    fluid_settings_setint(settings, "synth.midi-channels", synth->midi_channels);
    FLUID_LOG(FLUID_WARN, "Requested number of MIDI channels is not a multiple of 16. "
	     "I'll increase the number of channels to the next multiple.");
  }

  if (synth->audio_channels < 1) {
    FLUID_LOG(FLUID_WARN, "Requested number of audio channels is smaller than 1. "
	     "Changing this setting to 1.");
    synth->audio_channels = 1;
  } else if (synth->audio_channels > 128) {
    FLUID_LOG(FLUID_WARN, "Requested number of audio channels is too big (%d). "
	     "Limiting this setting to 128.", synth->audio_channels);
    synth->audio_channels = 128;
  }

  if (synth->audio_groups < 1) {
    FLUID_LOG(FLUID_WARN, "Requested number of audio groups is smaller than 1. "
	     "Changing this setting to 1.");
    synth->audio_groups = 1;
  } else if (synth->audio_groups > 128) {
    FLUID_LOG(FLUID_WARN, "Requested number of audio groups is too big (%d). "
	     "Limiting this setting to 128.", synth->audio_groups);
    synth->audio_groups = 128;
  }

  if (synth->effects_channels < 2) {
    FLUID_LOG(FLUID_WARN, "Invalid number of effects channels (%d)."
	     "Setting effects channels to 2.", synth->effects_channels);
    synth->effects_channels = 2;
  }


  /* The number of buffers is determined by the higher number of nr
   * groups / nr audio channels.  If LADSPA is unused, they should be
   * the same. */
  nbuf = synth->audio_channels;
  if (synth->audio_groups > nbuf) {
    nbuf = synth->audio_groups;
  }

  /* as soon as the synth is created it starts playing. */
  synth->state = FLUID_SYNTH_PLAYING;
  synth->sfont_info = NULL;
  synth->sfont_hash = new_fluid_hashtable (NULL, NULL);
  synth->noteid = 0;
  synth->ticks_since_start = 0;
  synth->tuning = NULL;
  fluid_private_init(synth->tuning_iter);

  /* Allocate event queue for rvoice mixer */
  fluid_settings_getint(settings, "synth.parallel-render", &i);
  /* In an overflow situation, a new voice takes about 50 spaces in the queue! */
  synth->eventhandler = new_fluid_rvoice_eventhandler(i, synth->polyphony*64,
	synth->polyphony, nbuf, synth->effects_channels, synth->sample_rate);

  if (synth->eventhandler == NULL)
    goto error_recovery; 

#ifdef LADSPA
  /* Create and initialize the Fx unit.*/
  synth->LADSPA_FxUnit = new_fluid_LADSPA_FxUnit(synth);
  fluid_rvoice_mixer_set_ladspa(synth->eventhandler->mixer, synth->LADSPA_FxUnit);
#endif
  
  /* allocate and add the default sfont loader */
  loader = new_fluid_defsfloader(settings);

  if (loader == NULL) {
    FLUID_LOG(FLUID_WARN, "Failed to create the default SoundFont loader");
  } else {
    fluid_synth_add_sfloader(synth, loader);
  }

  /* allocate all channel objects */
  synth->channel = FLUID_ARRAY(fluid_channel_t*, synth->midi_channels);
  if (synth->channel == NULL) {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    goto error_recovery;
  }
  for (i = 0; i < synth->midi_channels; i++) {
    synth->channel[i] = new_fluid_channel(synth, i);
    if (synth->channel[i] == NULL) {
      goto error_recovery;
    }
  }

  /* allocate all synthesis processes */
  synth->nvoice = synth->polyphony;
  synth->voice = FLUID_ARRAY(fluid_voice_t*, synth->nvoice);
  if (synth->voice == NULL) {
    goto error_recovery;
  }
  for (i = 0; i < synth->nvoice; i++) {
    synth->voice[i] = new_fluid_voice(synth->sample_rate);
    if (synth->voice[i] == NULL) {
      goto error_recovery;
    }
  }

  fluid_synth_set_sample_rate(synth, synth->sample_rate);
  
  fluid_synth_update_overflow(synth, "", 0.0f);
  fluid_synth_update_mixer(synth, fluid_rvoice_mixer_set_polyphony, 
			   synth->polyphony, 0.0f);
  fluid_synth_set_reverb_on(synth, synth->with_reverb);
  fluid_synth_set_chorus_on(synth, synth->with_chorus);
				 
  synth->cur = FLUID_BUFSIZE;
  synth->curmax = 0;
  synth->dither_index = 0;

  synth->reverb_roomsize = FLUID_REVERB_DEFAULT_ROOMSIZE;
  synth->reverb_damping = FLUID_REVERB_DEFAULT_DAMP;
  synth->reverb_width = FLUID_REVERB_DEFAULT_WIDTH;
  synth->reverb_level = FLUID_REVERB_DEFAULT_LEVEL;

  fluid_rvoice_eventhandler_push5(synth->eventhandler, 
				  fluid_rvoice_mixer_set_reverb_params,
				  synth->eventhandler->mixer, 
				  FLUID_REVMODEL_SET_ALL, synth->reverb_roomsize, 
				  synth->reverb_damping, synth->reverb_width, 
				  synth->reverb_level, 0.0f);

  /* Initialize multi-core variables if multiple cores enabled */
  if (synth->cores > 1)
  {
    int prio_level = 0;
    fluid_settings_getint (synth->settings, "audio.realtime-prio", &prio_level);
    fluid_synth_update_mixer(synth, fluid_rvoice_mixer_set_threads, 
			     synth->cores-1, prio_level);
  }

  synth->bank_select = FLUID_BANK_STYLE_GS;
  if (fluid_settings_str_equal (settings, "synth.midi-bank-select", "gm") == 1)
    synth->bank_select = FLUID_BANK_STYLE_GM;
  else if (fluid_settings_str_equal (settings, "synth.midi-bank-select", "gs") == 1)
    synth->bank_select = FLUID_BANK_STYLE_GS;
  else if (fluid_settings_str_equal (settings, "synth.midi-bank-select", "xg") == 1)
    synth->bank_select = FLUID_BANK_STYLE_XG;
  else if (fluid_settings_str_equal (settings, "synth.midi-bank-select", "mma") == 1)
    synth->bank_select = FLUID_BANK_STYLE_MMA;

  fluid_synth_process_event_queue(synth);

  /* FIXME */
  synth->start = fluid_curtime();

  return synth;

 error_recovery:
  delete_fluid_synth(synth);
  return NULL;
}


/**
 * Delete a FluidSynth instance.
 * @param synth FluidSynth instance to delete
 * @return FLUID_OK
 *
 * NOTE: Other users of a synthesizer instance, such as audio and MIDI drivers,
 * should be deleted prior to freeing the FluidSynth instance.
 */
int
delete_fluid_synth(fluid_synth_t* synth)
{
  int i, k;
  fluid_list_t *list;
  fluid_sfont_info_t* sfont_info;
//  fluid_event_queue_t* queue;
  fluid_sfloader_t* loader;

  if (synth == NULL) {
    return FLUID_OK;
  }

  fluid_profiling_print();

  /* turn off all voices, needed to unload SoundFont data */
  if (synth->voice != NULL) {
    for (i = 0; i < synth->nvoice; i++) {
      fluid_voice_t* voice = synth->voice[i];
      if (!voice)
        continue;
      fluid_voice_unlock_rvoice(voice);
      fluid_voice_overflow_rvoice_finished(voice); 
      if (fluid_voice_is_playing(voice))
	fluid_voice_off(voice);
    }
  }

  /* also unset all presets for clean SoundFont unload */
  if (synth->channel != NULL)
    for (i = 0; i < synth->midi_channels; i++)
      if (synth->channel[i] != NULL)
        fluid_channel_set_preset(synth->channel[i], NULL);

  if (synth->eventhandler)
    delete_fluid_rvoice_eventhandler(synth->eventhandler);

  /* delete all the SoundFonts */
  for (list = synth->sfont_info; list; list = fluid_list_next (list)) {
    sfont_info = (fluid_sfont_info_t *)fluid_list_get (list);
    delete_fluid_sfont (sfont_info->sfont);
    FLUID_FREE (sfont_info);
  }

  delete_fluid_list(synth->sfont_info);


  /* Delete the SoundFont info hash */
  if (synth->sfont_hash) delete_fluid_hashtable (synth->sfont_hash);


  /* delete all the SoundFont loaders */

  for (list = synth->loaders; list; list = fluid_list_next(list)) {
    loader = (fluid_sfloader_t*) fluid_list_get(list);
    fluid_sfloader_delete(loader);
  }

  delete_fluid_list(synth->loaders);


  if (synth->channel != NULL) {
    for (i = 0; i < synth->midi_channels; i++) {
      if (synth->channel[i] != NULL) {
	delete_fluid_channel(synth->channel[i]);
      }
    }
    FLUID_FREE(synth->channel);
  }

  if (synth->voice != NULL) {
    for (i = 0; i < synth->nvoice; i++) {
      if (synth->voice[i] != NULL) {
	delete_fluid_voice(synth->voice[i]);
      }
    }
    FLUID_FREE(synth->voice);
  }


  /* free the tunings, if any */
  if (synth->tuning != NULL) {
    for (i = 0; i < 128; i++) {
      if (synth->tuning[i] != NULL) {
	for (k = 0; k < 128; k++) {
	  if (synth->tuning[i][k] != NULL) {
	    delete_fluid_tuning(synth->tuning[i][k]);
	  }
	}
	FLUID_FREE(synth->tuning[i]);
      }
    }
    FLUID_FREE(synth->tuning);
  }

  fluid_private_free (synth->tuning_iter);

#ifdef LADSPA
  /* Release the LADSPA Fx unit */
  fluid_LADSPA_shutdown(synth->LADSPA_FxUnit);
  FLUID_FREE(synth->LADSPA_FxUnit);
#endif

  fluid_rec_mutex_destroy(synth->mutex);

  FLUID_FREE(synth);

  return FLUID_OK;
}

/**
 * Get a textual representation of the last error
 * @param synth FluidSynth instance
 * @return Pointer to string of last error message.  Valid until the same
 *   calling thread calls another FluidSynth function which fails.  String is
 *   internal and should not be modified or freed.
 */
/* FIXME - The error messages are not thread-safe, yet. They are still stored
 * in a global message buffer (see fluid_sys.c). */
char*
fluid_synth_error(fluid_synth_t* synth)
{
  return fluid_error();
}

/**
 * Send a note-on event to a FluidSynth object.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param key MIDI note number (0-127)
 * @param vel MIDI velocity (0-127, 0=noteoff)
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_noteon(fluid_synth_t* synth, int chan, int key, int vel)
{
  int result;
  fluid_return_val_if_fail (key >= 0 && key <= 127, FLUID_FAILED);
  fluid_return_val_if_fail (vel >= 0 && vel <= 127, FLUID_FAILED);
  FLUID_API_ENTRY_CHAN(FLUID_FAILED);

  result = fluid_synth_noteon_LOCAL (synth, chan, key, vel);
  FLUID_API_RETURN(result);
}

/* Local synthesis thread variant of fluid_synth_noteon */
static int
fluid_synth_noteon_LOCAL(fluid_synth_t* synth, int chan, int key, int vel)
{
  fluid_channel_t* channel;

  /* notes with velocity zero go to noteoff  */
  if (vel == 0) return fluid_synth_noteoff_LOCAL(synth, chan, key);

  channel = synth->channel[chan];
  
  /* make sure this channel has a preset */
  if (channel->preset == NULL) {
    if (synth->verbose) {
      FLUID_LOG(FLUID_INFO, "noteon\t%d\t%d\t%d\t%05d\t%.3f\t%.3f\t%.3f\t%d\t%s",
	       chan, key, vel, 0,
	       fluid_synth_get_ticks(synth) / 44100.0f,
	       (fluid_curtime() - synth->start) / 1000.0f,
	       0.0f, 0, "channel has no preset");
    }
    return FLUID_FAILED;
  }

  /* If there is another voice process on the same channel and key,
     advance it to the release phase. */
  fluid_synth_release_voice_on_same_note_LOCAL(synth, chan, key);


  return fluid_preset_noteon(channel->preset, synth, chan, key, vel);
}

/**
 * Send a note-off event to a FluidSynth object.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param key MIDI note number (0-127)
 * @return FLUID_OK on success, FLUID_FAILED otherwise (may just mean that no
 *   voices matched the note off event)
 */
int
fluid_synth_noteoff(fluid_synth_t* synth, int chan, int key)
{
  int result;
  fluid_return_val_if_fail (key >= 0 && key <= 127, FLUID_FAILED);
  FLUID_API_ENTRY_CHAN(FLUID_FAILED);

  result = fluid_synth_noteoff_LOCAL (synth, chan, key);
  
  FLUID_API_RETURN(result);
}

/* Local synthesis thread variant of fluid_synth_noteoff */
static int
fluid_synth_noteoff_LOCAL(fluid_synth_t* synth, int chan, int key)
{
  fluid_voice_t* voice;
  int status = FLUID_FAILED;
  int i;

  for (i = 0; i < synth->polyphony; i++) {
    voice = synth->voice[i];
    if (_ON(voice) && (voice->chan == chan) && (voice->key == key)) {
      if (synth->verbose) {
	int used_voices = 0;
	int k;
	for (k = 0; k < synth->polyphony; k++) {
	  if (!_AVAILABLE(synth->voice[k])) {
	    used_voices++;
	  }
	}
	FLUID_LOG(FLUID_INFO, "noteoff\t%d\t%d\t%d\t%05d\t%.3f\t%d",
		 voice->chan, voice->key, 0, voice->id,
		 (fluid_curtime() - synth->start) / 1000.0f,
		 used_voices);
      } /* if verbose */

      fluid_voice_noteoff(voice);
      status = FLUID_OK;
    } /* if voice on */
  } /* for all voices */
  return status;
}

/* Damp voices on a channel (turn notes off), if they're sustained by
   sustain pedal */
static int
fluid_synth_damp_voices_by_sustain_LOCAL(fluid_synth_t* synth, int chan)
{
  fluid_voice_t* voice;
  int i;

  for (i = 0; i < synth->polyphony; i++) {
    voice = synth->voice[i];

    if ((voice->chan == chan) && _SUSTAINED(voice))
     fluid_voice_release(voice);
  }

  return FLUID_OK;
}

/* Damp voices on a channel (turn notes off), if they're sustained by
   sostenuto pedal */
static int
fluid_synth_damp_voices_by_sostenuto_LOCAL(fluid_synth_t* synth, int chan)
{
  fluid_voice_t* voice;
  int i;

  for (i = 0; i < synth->polyphony; i++) {
    voice = synth->voice[i];

    if ((voice->chan == chan) && _HELD_BY_SOSTENUTO(voice))
     fluid_voice_release(voice);
  }

  return FLUID_OK;
}


/**
 * Send a MIDI controller event on a MIDI channel.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param num MIDI controller number (0-127)
 * @param val MIDI controller value (0-127)
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_cc(fluid_synth_t* synth, int chan, int num, int val)
{
  int result;
  fluid_return_val_if_fail (num >= 0 && num <= 127, FLUID_FAILED);
  fluid_return_val_if_fail (val >= 0 && val <= 127, FLUID_FAILED);
  FLUID_API_ENTRY_CHAN(FLUID_FAILED);

  if (synth->verbose)
    FLUID_LOG(FLUID_INFO, "cc\t%d\t%d\t%d", chan, num, val);

  fluid_channel_set_cc (synth->channel[chan], num, val);
  result = fluid_synth_cc_LOCAL (synth, chan, num);
  FLUID_API_RETURN(result);
}

/* Local synthesis thread variant of MIDI CC set function. */
static int
fluid_synth_cc_LOCAL (fluid_synth_t* synth, int channum, int num)
{
  fluid_channel_t* chan = synth->channel[channum];
  int nrpn_select;
  int value;

  value = fluid_channel_get_cc (chan, num);

  switch (num) {
  case SUSTAIN_SWITCH:
    /* Release voices if Sustain switch is released */
    if (value < 64) /* Sustain is released */
        fluid_synth_damp_voices_by_sustain_LOCAL (synth, channum);
    break;

  case SOSTENUTO_SWITCH:
    /* Release voices if Sostetuno switch is released */
    if (value < 64) /* Sostenuto is released */
        fluid_synth_damp_voices_by_sostenuto_LOCAL(synth, channum);
    else /* Sostenuto is depressed */
         /* Update sostenuto order id when pedaling on Sostenuto */
        chan->sostenuto_orderid = synth->noteid; /* future voice id value */
    break;

  case BANK_SELECT_MSB:
    fluid_channel_set_bank_msb (chan, value & 0x7F);
    break;
  case BANK_SELECT_LSB:
    fluid_channel_set_bank_lsb (chan, value & 0x7F);
    break;
  case ALL_NOTES_OFF:
    fluid_synth_all_notes_off_LOCAL (synth, channum);
    break;
  case ALL_SOUND_OFF:
    fluid_synth_all_sounds_off_LOCAL (synth, channum);
    break;
  case ALL_CTRL_OFF:
    fluid_channel_init_ctrl (chan, 1);
    fluid_synth_modulate_voices_all_LOCAL (synth, channum);
    break;
  case DATA_ENTRY_MSB:
    {
      int data = (value << 7) + fluid_channel_get_cc (chan, DATA_ENTRY_LSB);

      if (chan->nrpn_active)  /* NRPN is active? */
      { /* SontFont 2.01 NRPN Message (Sect. 9.6, p. 74)  */
        if ((fluid_channel_get_cc (chan, NRPN_MSB) == 120)
            && (fluid_channel_get_cc (chan, NRPN_LSB) < 100))
        {
          nrpn_select = chan->nrpn_select;

          if (nrpn_select < GEN_LAST)
          {
            float val = fluid_gen_scale_nrpn (nrpn_select, data);
            fluid_synth_set_gen_LOCAL (synth, channum, nrpn_select, val, FALSE);
          }

          chan->nrpn_select = 0;  /* Reset to 0 */
        }
      }
      else if (fluid_channel_get_cc (chan, RPN_MSB) == 0)    /* RPN is active: MSB = 0? */
      {
        switch (fluid_channel_get_cc (chan, RPN_LSB))
        {
          case RPN_PITCH_BEND_RANGE:    /* Set bend range in semitones */
            fluid_channel_set_pitch_wheel_sensitivity (synth->channel[channum], value);
            fluid_synth_update_pitch_wheel_sens_LOCAL (synth, channum);   /* Update bend range */
            /* FIXME - Handle LSB? (Fine bend range in cents) */
            break;
          case RPN_CHANNEL_FINE_TUNE:   /* Fine tune is 14 bit over 1 semitone (+/- 50 cents, 8192 = center) */
            fluid_synth_set_gen_LOCAL (synth, channum, GEN_FINETUNE,
                                       (data - 8192) / 8192.0 * 50.0, FALSE);
            break;
          case RPN_CHANNEL_COARSE_TUNE: /* Coarse tune is 7 bit and in semitones (64 is center) */
            fluid_synth_set_gen_LOCAL (synth, channum, GEN_COARSETUNE,
                                       value - 64, FALSE);
            break;
          case RPN_TUNING_PROGRAM_CHANGE:
            fluid_channel_set_tuning_prog (chan, value);
            fluid_synth_activate_tuning (synth, channum,
                                         fluid_channel_get_tuning_bank (chan),
                                         value, TRUE);
            break;
          case RPN_TUNING_BANK_SELECT:
            fluid_channel_set_tuning_bank (chan, value);
            break;
          case RPN_MODULATION_DEPTH_RANGE:
            break;
        }
      }
      break;
    }
  case NRPN_MSB:
    fluid_channel_set_cc (chan, NRPN_LSB, 0);
    chan->nrpn_select = 0;
    chan->nrpn_active = 1;
    break;
  case NRPN_LSB:
    /* SontFont 2.01 NRPN Message (Sect. 9.6, p. 74)  */
    if (fluid_channel_get_cc (chan, NRPN_MSB) == 120) {
      if (value == 100) {
        chan->nrpn_select += 100;
      } else if (value == 101) {
        chan->nrpn_select += 1000;
      } else if (value == 102) {
        chan->nrpn_select += 10000;
      } else if (value < 100) {
        chan->nrpn_select += value;
      }
    }

    chan->nrpn_active = 1;
    break;
  case RPN_MSB:
  case RPN_LSB:
    chan->nrpn_active = 0;
    break;
  default:
    return fluid_synth_modulate_voices_LOCAL (synth, channum, 1, num);
  }

  return FLUID_OK;
}

/**
 * Get current MIDI controller value on a MIDI channel.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param num MIDI controller number (0-127)
 * @param pval Location to store MIDI controller value (0-127)
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_get_cc(fluid_synth_t* synth, int chan, int num, int* pval)
{
  fluid_return_val_if_fail (num >= 0 && num < 128, FLUID_FAILED);
  fluid_return_val_if_fail (pval != NULL, FLUID_FAILED);

  FLUID_API_ENTRY_CHAN(FLUID_FAILED);
  
  *pval = fluid_channel_get_cc (synth->channel[chan], num);
  FLUID_API_RETURN(FLUID_OK);
}

/*
 * Handler for synth.device-id setting.
 */
static int
fluid_synth_update_device_id (fluid_synth_t *synth, char *name, int value)
{
  fluid_synth_api_enter(synth);
  synth->device_id = value;
  fluid_synth_api_exit(synth);
  return 0;
}

/**
 * Process a MIDI SYSEX (system exclusive) message.
 * @param synth FluidSynth instance
 * @param data Buffer containing SYSEX data (not including 0xF0 and 0xF7)
 * @param len Length of data in buffer
 * @param response Buffer to store response to or NULL to ignore
 * @param response_len IN/OUT parameter, in: size of response buffer, out:
 *   amount of data written to response buffer (if FLUID_FAILED is returned and
 *   this value is non-zero, it indicates the response buffer is too small)
 * @param handled Optional location to store boolean value if message was
 *   recognized and handled or not (set to TRUE if it was handled)
 * @param dryrun TRUE to just do a dry run but not actually execute the SYSEX
 *   command (useful for checking if a SYSEX message would be handled)
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 * @since 1.1.0
 */
/* SYSEX format (0xF0 and 0xF7 not passed to this function):
 * Non-realtime:    0xF0 0x7E <DeviceId> [BODY] 0xF7
 * Realtime:        0xF0 0x7F <DeviceId> [BODY] 0xF7
 * Tuning messages: 0xF0 0x7E/0x7F <DeviceId> 0x08 <sub ID2> [BODY] <ChkSum> 0xF7
 */
int
fluid_synth_sysex(fluid_synth_t *synth, const char *data, int len,
                  char *response, int *response_len, int *handled, int dryrun)
{
  int avail_response = 0;

  if (handled) *handled = FALSE;

  if (response_len)
  {
    avail_response = *response_len;
    *response_len = 0;
  }

  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (data != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (len > 0, FLUID_FAILED);
  fluid_return_val_if_fail (!response || response_len, FLUID_FAILED);

  if (len < 4) return FLUID_OK;

  /* MIDI tuning SYSEX message? */
  if ((data[0] == MIDI_SYSEX_UNIV_NON_REALTIME || data[0] == MIDI_SYSEX_UNIV_REALTIME)
      && (data[1] == synth->device_id || data[1] == MIDI_SYSEX_DEVICE_ID_ALL)
      && data[2] == MIDI_SYSEX_MIDI_TUNING_ID)
  {
    int result;
    fluid_synth_api_enter(synth);
    result = fluid_synth_sysex_midi_tuning (synth, data, len, response, 
						response_len, avail_response, 
						handled, dryrun);

    FLUID_API_RETURN(result);
  }
  return FLUID_OK;
}

/* Handler for MIDI tuning SYSEX messages */
static int
fluid_synth_sysex_midi_tuning (fluid_synth_t *synth, const char *data, int len,
                               char *response, int *response_len, int avail_response,
                               int *handled, int dryrun)
{
  int realtime, msgid;
  int bank = 0, prog, channels;
  double tunedata[128];
  int keys[128];
  char name[17];
  int note, frac, frac2;
  uint8 chksum;
  int i, count, index;
  const char *dataptr;
  char *resptr;;

  realtime = data[0] == MIDI_SYSEX_UNIV_REALTIME;
  msgid = data[3];

  switch (msgid)
  {
    case MIDI_SYSEX_TUNING_BULK_DUMP_REQ:
    case MIDI_SYSEX_TUNING_BULK_DUMP_REQ_BANK:
      if (data[3] == MIDI_SYSEX_TUNING_BULK_DUMP_REQ)
      {
        if (len != 5 || data[4] & 0x80 || !response)
          return FLUID_OK;

        *response_len = 406;
        prog = data[4];
      }
      else
      {
        if (len != 6 || data[4] & 0x80 || data[5] & 0x80 || !response)
          return FLUID_OK;

        *response_len = 407;
        bank = data[4];
        prog = data[5];
      }

      if (dryrun)
      {
        if (handled) *handled = TRUE;
        return FLUID_OK;
      }

      if (avail_response < *response_len) return FLUID_FAILED;

      /* Get tuning data, return if tuning not found */
      if (fluid_synth_tuning_dump (synth, bank, prog, name, 17, tunedata) == FLUID_FAILED)
      {
        *response_len = 0;
        return FLUID_OK;
      }

      resptr = response;

      *resptr++ = MIDI_SYSEX_UNIV_NON_REALTIME;
      *resptr++ = synth->device_id;
      *resptr++ = MIDI_SYSEX_MIDI_TUNING_ID;
      *resptr++ = MIDI_SYSEX_TUNING_BULK_DUMP;

      if (msgid == MIDI_SYSEX_TUNING_BULK_DUMP_REQ_BANK)
        *resptr++ = bank;

      *resptr++ = prog;
      FLUID_STRNCPY (resptr, name, 16);
      resptr += 16;

      for (i = 0; i < 128; i++)
      {
        note = tunedata[i] / 100.0;
        fluid_clip (note, 0, 127);

        frac = ((tunedata[i] - note * 100.0) * 16384.0 + 50.0) / 100.0;
        fluid_clip (frac, 0, 16383);

        *resptr++ = note;
        *resptr++ = frac >> 7;
        *resptr++ = frac & 0x7F;
      }

      if (msgid == MIDI_SYSEX_TUNING_BULK_DUMP_REQ)
      {  /* NOTE: Checksum is not as straight forward as the bank based messages */
        chksum = MIDI_SYSEX_UNIV_NON_REALTIME ^ MIDI_SYSEX_MIDI_TUNING_ID
          ^ MIDI_SYSEX_TUNING_BULK_DUMP ^ prog;

        for (i = 21; i < 128 * 3 + 21; i++)
          chksum ^= response[i];
      }
      else
      {
        for (i = 1, chksum = 0; i < 406; i++)
          chksum ^= response[i];
      }

      *resptr++ = chksum & 0x7F;

      if (handled) *handled = TRUE;
      break;
    case MIDI_SYSEX_TUNING_NOTE_TUNE:
    case MIDI_SYSEX_TUNING_NOTE_TUNE_BANK:
      dataptr = data + 4;

      if (msgid == MIDI_SYSEX_TUNING_NOTE_TUNE)
      {
        if (len < 10 || data[4] & 0x80 || data[5] & 0x80 || len != data[5] * 4 + 6)
          return FLUID_OK;
      }
      else
      {
        if (len < 11 || data[4] & 0x80 || data[5] & 0x80 || data[6] & 0x80
            || len != data[5] * 4 + 7)
          return FLUID_OK;

        bank = *dataptr++;
      }

      if (dryrun)
      {
        if (handled) *handled = TRUE;
        return FLUID_OK;
      }

      prog = *dataptr++;
      count = *dataptr++;

      for (i = 0, index = 0; i < count; i++)
      {
        note = *dataptr++;
        if (note & 0x80) return FLUID_OK;
        keys[index] = note;

        note = *dataptr++;
        frac = *dataptr++;
        frac2 = *dataptr++;

        if (note & 0x80 || frac & 0x80 || frac2 & 0x80)
          return FLUID_OK;

        frac = frac << 7 | frac2;

        /* No change pitch value?  Doesn't really make sense to send that, but.. */
        if (note == 0x7F && frac == 16383) continue;

        tunedata[index] = note * 100.0 + (frac * 100.0 / 16384.0);
        index++;
      }

      if (index > 0)
      {
        if (fluid_synth_tune_notes (synth, bank, prog, index, keys, tunedata,
                                    realtime) == FLUID_FAILED)
          return FLUID_FAILED;
      }

      if (handled) *handled = TRUE;
      break;
    case MIDI_SYSEX_TUNING_OCTAVE_TUNE_1BYTE:
    case MIDI_SYSEX_TUNING_OCTAVE_TUNE_2BYTE:
      if ((msgid == MIDI_SYSEX_TUNING_OCTAVE_TUNE_1BYTE && len != 19)
          || (msgid == MIDI_SYSEX_TUNING_OCTAVE_TUNE_2BYTE && len != 31))
        return FLUID_OK;

      if (data[4] & 0x80 || data[5] & 0x80 || data[6] & 0x80)
        return FLUID_OK;

      if (dryrun)
      {
        if (handled) *handled = TRUE;
        return FLUID_OK;
      }

      channels = (data[4] & 0x03) << 14 | data[5] << 7 | data[6];

      if (msgid == MIDI_SYSEX_TUNING_OCTAVE_TUNE_1BYTE)
      {
        for (i = 0; i < 12; i++)
        {
          frac = data[i + 7];
          if (frac & 0x80) return FLUID_OK;
          tunedata[i] = (int)frac - 64;
        }
      }
      else
      {
        for (i = 0; i < 12; i++)
        {
          frac = data[i * 2 + 7];
          frac2 = data[i * 2 + 8];
          if (frac & 0x80 || frac2 & 0x80) return FLUID_OK;
          tunedata[i] = (((int)frac << 7 | (int)frac2) - 8192) * (200.0 / 16384.0);
        }
      }

      if (fluid_synth_activate_octave_tuning (synth, 0, 0, "SYSEX",
                                              tunedata, realtime) == FLUID_FAILED)
        return FLUID_FAILED;

      if (channels)
      {
        for (i = 0; i < 16; i++)
        {
          if (channels & (1 << i))
            fluid_synth_activate_tuning (synth, i, 0, 0, realtime);
        }
      }

      if (handled) *handled = TRUE;
      break;
  }

  return FLUID_OK;
}

/**
 * Turn off all notes on a MIDI channel (put them into release phase).
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1), (chan=-1 selects all channels)
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 * @since 1.1.4
 */
int
fluid_synth_all_notes_off(fluid_synth_t* synth, int chan)
{
  int result;

  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (chan >= -1, FLUID_FAILED);
  fluid_synth_api_enter(synth);
  if (chan >= synth->midi_channels) 
    result = FLUID_FAILED;
  else
    result = fluid_synth_all_notes_off_LOCAL (synth, chan);
  FLUID_API_RETURN(result);
}

/* Local synthesis thread variant of all notes off, (chan=-1 selects all channels) */
static int
fluid_synth_all_notes_off_LOCAL(fluid_synth_t* synth, int chan)
{
  fluid_voice_t* voice;
  int i;

  for (i = 0; i < synth->polyphony; i++) {
    voice = synth->voice[i];

    if (_PLAYING(voice) && ((-1 == chan) || (chan == voice->chan)))
      fluid_voice_noteoff(voice);
  }
  return FLUID_OK;
}

/**
 * Immediately stop all notes on a MIDI channel (skips release phase).
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1), (chan=-1 selects all channels)
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 * @since 1.1.4
 */
int
fluid_synth_all_sounds_off(fluid_synth_t* synth, int chan)
{
  int result;

  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (chan >= -1, FLUID_FAILED);
  fluid_synth_api_enter(synth);
  if (chan >= synth->midi_channels) 
    result = FLUID_FAILED;
  else
    result = fluid_synth_all_sounds_off_LOCAL (synth, chan);
  FLUID_API_RETURN(result);
}

/* Local synthesis thread variant of all sounds off, (chan=-1 selects all channels) */
static int
fluid_synth_all_sounds_off_LOCAL(fluid_synth_t* synth, int chan)
{
  fluid_voice_t* voice;
  int i;

  for (i = 0; i < synth->polyphony; i++) {
    voice = synth->voice[i];

    if (_PLAYING(voice) && ((-1 == chan) || (chan == voice->chan)))
      fluid_voice_off(voice);
  }
  return FLUID_OK;
}

/**
 * Reset reverb engine
 * @param synth FluidSynth instance
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_reset_reverb(fluid_synth_t* synth)
{
  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_synth_api_enter(synth);
  fluid_synth_update_mixer(synth, fluid_rvoice_mixer_reset_reverb, 0, 0.0f);
  FLUID_API_RETURN(FLUID_OK);
}

/**
 * Reset chorus engine
 * @param synth FluidSynth instance
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_reset_chorus(fluid_synth_t* synth)
{
  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_synth_api_enter(synth);
  fluid_synth_update_mixer(synth, fluid_rvoice_mixer_reset_chorus, 0, 0.0f);
  FLUID_API_RETURN(FLUID_OK);
}


/**
 * Send MIDI system reset command (big red 'panic' button), turns off notes and
 *   resets controllers.
 * @param synth FluidSynth instance
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_system_reset(fluid_synth_t* synth)
{
  int result;
  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_synth_api_enter(synth);
  result = fluid_synth_system_reset_LOCAL (synth);
  FLUID_API_RETURN(result);
}

/* Local variant of the system reset command */
static int
fluid_synth_system_reset_LOCAL(fluid_synth_t* synth)
{
  fluid_voice_t* voice;
  int i;

  for (i = 0; i < synth->polyphony; i++) {
    voice = synth->voice[i];

    if (_PLAYING(voice))
      fluid_voice_off(voice);
  }

  for (i = 0; i < synth->midi_channels; i++)
    fluid_channel_reset(synth->channel[i]);

  fluid_synth_update_mixer(synth, fluid_rvoice_mixer_reset_fx, 0, 0.0f); 

  return FLUID_OK;
}

/**
 * Update voices on a MIDI channel after a MIDI control change.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param is_cc Boolean value indicating if ctrl is a CC controller or not
 * @param ctrl MIDI controller value
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
static int
fluid_synth_modulate_voices_LOCAL(fluid_synth_t* synth, int chan, int is_cc, int ctrl)
{
  fluid_voice_t* voice;
  int i;

  for (i = 0; i < synth->polyphony; i++) {
    voice = synth->voice[i];

    if (voice->chan == chan)
      fluid_voice_modulate(voice, is_cc, ctrl);
  }
  return FLUID_OK;
}

/**
 * Update voices on a MIDI channel after all MIDI controllers have been changed.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
static int
fluid_synth_modulate_voices_all_LOCAL(fluid_synth_t* synth, int chan)
{
  fluid_voice_t* voice;
  int i;

  for (i = 0; i < synth->polyphony; i++) {
    voice = synth->voice[i];

    if (voice->chan == chan)
      fluid_voice_modulate_all(voice);
  }
  return FLUID_OK;
}

/**
 * Set the MIDI channel pressure controller value.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param val MIDI channel pressure value (0-127)
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_channel_pressure(fluid_synth_t* synth, int chan, int val)
{
  int result;
  fluid_return_val_if_fail (val >= 0 && val <= 127, FLUID_FAILED);

  FLUID_API_ENTRY_CHAN(FLUID_FAILED);
  
  if (synth->verbose)
    FLUID_LOG(FLUID_INFO, "channelpressure\t%d\t%d", chan, val);

  fluid_channel_set_channel_pressure (synth->channel[chan], val);

  result = fluid_synth_update_channel_pressure_LOCAL (synth, chan);
  FLUID_API_RETURN(result);
}

/* Updates channel pressure from within synthesis thread */
static int
fluid_synth_update_channel_pressure_LOCAL(fluid_synth_t* synth, int chan)
{
  return fluid_synth_modulate_voices_LOCAL (synth, chan, 0, FLUID_MOD_CHANNELPRESSURE);
}

/**
 * Set the MIDI pitch bend controller value on a MIDI channel.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param val MIDI pitch bend value (0-16383 with 8192 being center)
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_pitch_bend(fluid_synth_t* synth, int chan, int val)
{
  int result;
  fluid_return_val_if_fail (val >= 0 && val <= 16383, FLUID_FAILED);
  FLUID_API_ENTRY_CHAN(FLUID_FAILED);
  
  if (synth->verbose)
    FLUID_LOG(FLUID_INFO, "pitchb\t%d\t%d", chan, val);

  fluid_channel_set_pitch_bend (synth->channel[chan], val);

  result = fluid_synth_update_pitch_bend_LOCAL (synth, chan);
  FLUID_API_RETURN(result);
}

/* Local synthesis thread variant of pitch bend */
static int
fluid_synth_update_pitch_bend_LOCAL(fluid_synth_t* synth, int chan)
{
  return fluid_synth_modulate_voices_LOCAL (synth, chan, 0, FLUID_MOD_PITCHWHEEL);
}

/**
 * Get the MIDI pitch bend controller value on a MIDI channel.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param ppitch_bend Location to store MIDI pitch bend value (0-16383 with
 *   8192 being center)
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_get_pitch_bend(fluid_synth_t* synth, int chan, int* ppitch_bend)
{
  fluid_return_val_if_fail (ppitch_bend != NULL, FLUID_FAILED);
  FLUID_API_ENTRY_CHAN(FLUID_FAILED);
  
  *ppitch_bend = fluid_channel_get_pitch_bend (synth->channel[chan]);
  FLUID_API_RETURN(FLUID_OK);
}

/**
 * Set MIDI pitch wheel sensitivity on a MIDI channel.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param val Pitch wheel sensitivity value in semitones
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_pitch_wheel_sens(fluid_synth_t* synth, int chan, int val)
{
  int result;
  fluid_return_val_if_fail (val >= 0 && val <= 72, FLUID_FAILED);       /* 6 octaves!?  Better than no limit.. */
  FLUID_API_ENTRY_CHAN(FLUID_FAILED);
  
  if (synth->verbose)
    FLUID_LOG(FLUID_INFO, "pitchsens\t%d\t%d", chan, val);

  fluid_channel_set_pitch_wheel_sensitivity (synth->channel[chan], val);

  result = fluid_synth_update_pitch_wheel_sens_LOCAL (synth, chan);
  FLUID_API_RETURN(result);
}

/* Local synthesis thread variant of set pitch wheel sensitivity */
static int
fluid_synth_update_pitch_wheel_sens_LOCAL(fluid_synth_t* synth, int chan)
{
  return fluid_synth_modulate_voices_LOCAL (synth, chan, 0, FLUID_MOD_PITCHWHEELSENS);
}

/**
 * Get MIDI pitch wheel sensitivity on a MIDI channel.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param pval Location to store pitch wheel sensitivity value in semitones
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 * @since Sometime AFTER v1.0 API freeze.
 */
int
fluid_synth_get_pitch_wheel_sens(fluid_synth_t* synth, int chan, int* pval)
{
  fluid_return_val_if_fail (pval != NULL, FLUID_FAILED);
  FLUID_API_ENTRY_CHAN(FLUID_FAILED);
  
  *pval = fluid_channel_get_pitch_wheel_sensitivity (synth->channel[chan]);
  FLUID_API_RETURN(FLUID_OK);
}

/**
 * Assign a preset to a MIDI channel.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param preset Preset to assign to channel or NULL to clear (ownership is taken over)
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
static int
fluid_synth_set_preset (fluid_synth_t *synth, int chan, fluid_preset_t *preset)
{
  fluid_channel_t *channel;

  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (chan >= 0 && chan < synth->midi_channels, FLUID_FAILED);

  channel = synth->channel[chan];

  return fluid_channel_set_preset (channel, preset);
}

/* Get a preset by SoundFont, bank and program numbers.
 * Returns preset pointer or NULL.
 *
 * NOTE: The returned preset has been allocated, caller owns it and should
 *       free it when finished using it.
 */
static fluid_preset_t*
fluid_synth_get_preset(fluid_synth_t* synth, unsigned int sfontnum,
                       unsigned int banknum, unsigned int prognum)
{
  fluid_preset_t *preset = NULL;
  fluid_sfont_info_t *sfont_info;
  fluid_list_t *list;

  /* 128 indicates an "unset" operation" */
  if (prognum == FLUID_UNSET_PROGRAM) return NULL;

  for (list = synth->sfont_info; list; list = fluid_list_next (list)) {
    sfont_info = (fluid_sfont_info_t *)fluid_list_get (list);

    if (fluid_sfont_get_id (sfont_info->sfont) == sfontnum)
    {
      preset = fluid_sfont_get_preset (sfont_info->sfont,
                                       banknum - sfont_info->bankofs, prognum);
      if (preset) sfont_info->refcount++;       /* Add reference to SoundFont */
      break;
    }
  }

  return preset;
}

/* Get a preset by SoundFont name, bank and program.
 * Returns preset pointer or NULL.
 *
 * NOTE: The returned preset has been allocated, caller owns it and should
 *       free it when finished using it.
 */
static fluid_preset_t*
fluid_synth_get_preset_by_sfont_name(fluid_synth_t* synth, const char *sfontname,
                                     unsigned int banknum, unsigned int prognum)
{
  fluid_preset_t *preset = NULL;
  fluid_sfont_info_t *sfont_info;
  fluid_list_t *list;

  for (list = synth->sfont_info; list; list = fluid_list_next (list)) {
    sfont_info = (fluid_sfont_info_t *)fluid_list_get (list);

    if (FLUID_STRCMP (fluid_sfont_get_name (sfont_info->sfont), sfontname) == 0)
    {
      preset = fluid_sfont_get_preset (sfont_info->sfont,
                                       banknum - sfont_info->bankofs, prognum);
      if (preset) sfont_info->refcount++;       /* Add reference to SoundFont */
      break;
    }
  }

  return preset;
}

/* Find a preset by bank and program numbers.
 * Returns preset pointer or NULL.
 *
 * NOTE: The returned preset has been allocated, caller owns it and should
 *       free it when finished using it. */
fluid_preset_t*
fluid_synth_find_preset(fluid_synth_t* synth, unsigned int banknum,
                        unsigned int prognum)
{
  fluid_preset_t *preset = NULL;
  fluid_sfont_info_t *sfont_info;
  fluid_list_t *list;

  for (list = synth->sfont_info; list; list = fluid_list_next (list)) {
    sfont_info = (fluid_sfont_info_t *)fluid_list_get (list);

    preset = fluid_sfont_get_preset (sfont_info->sfont,
                                     banknum - sfont_info->bankofs, prognum);
    if (preset)
    {
      sfont_info->refcount++;       /* Add reference to SoundFont */
      break;
    }
  }

  return preset;
}

/**
 * Send a program change event on a MIDI channel.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param prognum MIDI program number (0-127)
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
/* FIXME - Currently not real-time safe, due to preset allocation and mutex lock,
 * and may be called from within synthesis context. */

/* As of 1.1.1 prognum can be set to 128 to unset the preset.  Not documented
 * since fluid_synth_unset_program() should be used instead. */
int
fluid_synth_program_change(fluid_synth_t* synth, int chan, int prognum)
{
  fluid_preset_t* preset = NULL;
  fluid_channel_t* channel;
  int subst_bank, subst_prog, banknum = 0, result;

  fluid_return_val_if_fail (prognum >= 0 && prognum <= 128, FLUID_FAILED);
  FLUID_API_ENTRY_CHAN(FLUID_FAILED);
  
  channel = synth->channel[chan];
  if (channel->channel_type == CHANNEL_TYPE_DRUM) 
    banknum = DRUM_INST_BANK;
  else
    fluid_channel_get_sfont_bank_prog(channel, NULL, &banknum, NULL);

  if (synth->verbose)
    FLUID_LOG(FLUID_INFO, "prog\t%d\t%d\t%d", chan, banknum, prognum);

  /* I think this is a hack for MIDI files that do bank changes in GM mode.  
   * Proper way to handle this would probably be to ignore bank changes when in 
   * GM mode. - JG
   * This is now possible by setting synth.midi-bank-select=gm, but let the hack
   * stay for the time being. - DH
   */
  if (prognum != FLUID_UNSET_PROGRAM)
  {
    subst_bank = banknum;
    subst_prog = prognum;
    
    preset = fluid_synth_find_preset(synth, subst_bank, subst_prog);
    
    /* Fallback to another preset if not found */
    if (!preset) {
      /* Percussion: Fallback to preset 0 in percussion bank */
      if (subst_bank == DRUM_INST_BANK) {
        subst_prog = 0;
        preset = fluid_synth_find_preset(synth, subst_bank, subst_prog);
      }
      /* Melodic instrument */
      else { 
        /* Fallback first to bank 0:prognum */
        subst_bank = 0;
        preset = fluid_synth_find_preset(synth, subst_bank, subst_prog);

        /* Fallback to first preset in bank 0 (usually piano...) */
        if (!preset)
        {
	  subst_prog = 0;
          preset = fluid_synth_find_preset(synth, subst_bank, subst_prog);
        }
      }

      if (preset)
        FLUID_LOG(FLUID_WARN, "Instrument not found on channel %d [bank=%d prog=%d], substituted [bank=%d prog=%d]",
                  chan, banknum, prognum, subst_bank, subst_prog); 
      else
        FLUID_LOG(FLUID_WARN, "No preset found on channel %d [bank=%d prog=%d]",
                  chan, banknum, prognum); 
    }
  }

  /* Assign the SoundFont ID and program number to the channel */
  fluid_channel_set_sfont_bank_prog (channel, preset ? fluid_sfont_get_id (preset->sfont) : 0,
                                     -1, prognum);
  result = fluid_synth_set_preset (synth, chan, preset);
  FLUID_API_RETURN(result);
}

/**
 * Set instrument bank number on a MIDI channel.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param bank MIDI bank number
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_bank_select(fluid_synth_t* synth, int chan, unsigned int bank)
{
  fluid_return_val_if_fail (bank <= 16383, FLUID_FAILED);
  FLUID_API_ENTRY_CHAN(FLUID_FAILED);
  
  fluid_channel_set_sfont_bank_prog (synth->channel[chan], -1, bank, -1);
  FLUID_API_RETURN(FLUID_OK);
}

/**
 * Set SoundFont ID on a MIDI channel.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param sfont_id ID of a loaded SoundFont
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_sfont_select(fluid_synth_t* synth, int chan, unsigned int sfont_id)
{
  FLUID_API_ENTRY_CHAN(FLUID_FAILED);
  
  fluid_channel_set_sfont_bank_prog(synth->channel[chan], sfont_id, -1, -1);

  FLUID_API_RETURN(FLUID_OK);
}

/**
 * Set the preset of a MIDI channel to an unassigned state.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @return #FLUID_OK on success, #FLUID_FAILED otherwise
 * @since 1.1.1
 *
 * Note: Channel retains its SoundFont ID and bank numbers, while the program
 * number is set to an "unset" state.  MIDI program changes may re-assign a
 * preset if one matches.
 */
int
fluid_synth_unset_program (fluid_synth_t *synth, int chan)
{
  int result;
  FLUID_API_ENTRY_CHAN(FLUID_FAILED);

  result = fluid_synth_program_change (synth, chan, FLUID_UNSET_PROGRAM);
  FLUID_API_RETURN(result);
}

/**
 * Get current SoundFont ID, bank number and program number for a MIDI channel.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param sfont_id Location to store SoundFont ID
 * @param bank_num Location to store MIDI bank number
 * @param preset_num Location to store MIDI program number
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_get_program(fluid_synth_t* synth, int chan, unsigned int* sfont_id,
                        unsigned int* bank_num, unsigned int* preset_num)
{
  fluid_channel_t* channel;

  fluid_return_val_if_fail (sfont_id != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (bank_num != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (preset_num != NULL, FLUID_FAILED);
  FLUID_API_ENTRY_CHAN(FLUID_FAILED);

  channel = synth->channel[chan];
  fluid_channel_get_sfont_bank_prog(channel, (int *)sfont_id, (int *)bank_num,
                                    (int *)preset_num);

  /* 128 indicates that the preset is unset.  Set to 0 to be backwards compatible. */
  if (*preset_num == FLUID_UNSET_PROGRAM) *preset_num = 0;

  FLUID_API_RETURN(FLUID_OK);
}

/**
 * Select an instrument on a MIDI channel by SoundFont ID, bank and program numbers.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param sfont_id ID of a loaded SoundFont
 * @param bank_num MIDI bank number
 * @param preset_num MIDI program number
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_program_select(fluid_synth_t* synth, int chan, unsigned int sfont_id,
			   unsigned int bank_num, unsigned int preset_num)
{
  fluid_preset_t* preset = NULL;
  fluid_channel_t* channel;
  int result;
  FLUID_API_ENTRY_CHAN(FLUID_FAILED);

  channel = synth->channel[chan];

  /* ++ Allocate preset */
  preset = fluid_synth_get_preset (synth, sfont_id, bank_num, preset_num);

  if (preset == NULL) {
    FLUID_LOG(FLUID_ERR,
	      "There is no preset with bank number %d and preset number %d in SoundFont %d",
	      bank_num, preset_num, sfont_id);
    FLUID_API_RETURN(FLUID_FAILED);
  }

  /* Assign the new SoundFont ID, bank and program number to the channel */
  fluid_channel_set_sfont_bank_prog (channel, sfont_id, bank_num, preset_num);
  result = fluid_synth_set_preset (synth, chan, preset);
  
  FLUID_API_RETURN(result);
}

/**
 * Select an instrument on a MIDI channel by SoundFont name, bank and program numbers.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param sfont_name Name of a loaded SoundFont
 * @param bank_num MIDI bank number
 * @param preset_num MIDI program number
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 * @since 1.1.0
 */
int
fluid_synth_program_select_by_sfont_name (fluid_synth_t* synth, int chan,
                                          const char *sfont_name, unsigned int bank_num,
                                          unsigned int preset_num)
{
  fluid_preset_t* preset = NULL;
  fluid_channel_t* channel;
  int result;
  fluid_return_val_if_fail (sfont_name != NULL, FLUID_FAILED);
  FLUID_API_ENTRY_CHAN(FLUID_FAILED);

  channel = synth->channel[chan];

  /* ++ Allocate preset */
  preset = fluid_synth_get_preset_by_sfont_name (synth, sfont_name, bank_num,
                                                 preset_num);
  if (preset == NULL) {
    FLUID_LOG(FLUID_ERR,
	      "There is no preset with bank number %d and preset number %d in SoundFont %s",
	      bank_num, preset_num, sfont_name);
    FLUID_API_RETURN(FLUID_FAILED);
  }

  /* Assign the new SoundFont ID, bank and program number to the channel */
  fluid_channel_set_sfont_bank_prog (channel, fluid_sfont_get_id (preset->sfont),
                                     bank_num, preset_num);
  result = fluid_synth_set_preset (synth, chan, preset);
  FLUID_API_RETURN(result);  
}

/*
 * This function assures that every MIDI channel has a valid preset
 * (NULL is okay). This function is called after a SoundFont is
 * unloaded or reloaded.
 */
static void
fluid_synth_update_presets(fluid_synth_t* synth)
{
  fluid_channel_t *channel;
  fluid_preset_t *preset;
  int sfont, bank, prog;
  int chan;

  for (chan = 0; chan < synth->midi_channels; chan++) {
    channel = synth->channel[chan];
    fluid_channel_get_sfont_bank_prog (channel, &sfont, &bank, &prog);
    preset = fluid_synth_get_preset (synth, sfont, bank, prog);
    fluid_synth_set_preset (synth, chan, preset);
  }
}

/* Handler for synth.gain setting. */
static int
fluid_synth_update_sample_rate(fluid_synth_t* synth, char* name, double value)
{
  fluid_synth_set_sample_rate(synth, (float) value);
  return 0;
}

/**
 * Set sample rate of the synth. 
 * NOTE: This function is currently experimental and should only be 
 * used when no voices or notes are active, and before any rendering calls.
 * @param synth FluidSynth instance
 * @param sample_rate New sample rate (Hz)
 * @since 1.1.2
 */
void 
fluid_synth_set_sample_rate(fluid_synth_t* synth, float sample_rate)
{
  int i;
  fluid_return_if_fail (synth != NULL);
  fluid_synth_api_enter(synth);
  fluid_clip (sample_rate, 8000.0f, 96000.0f);
  synth->sample_rate = sample_rate;
  
  fluid_settings_getint(synth->settings, "synth.min-note-length", &i);
  synth->min_note_length_ticks = (unsigned int) (i*synth->sample_rate/1000.0f);
  
  for (i=0; i < synth->polyphony; i++)
    fluid_voice_set_output_rate(synth->voice[i], sample_rate);
  fluid_synth_update_mixer(synth, fluid_rvoice_mixer_set_samplerate, 
			   0, sample_rate);
  fluid_synth_api_exit(synth);
}


/* Handler for synth.gain setting. */
static int
fluid_synth_update_gain(fluid_synth_t* synth, char* name, double value)
{
  fluid_synth_set_gain(synth, (float) value);
  return 0;
}

/**
 * Set synth output gain value.
 * @param synth FluidSynth instance
 * @param gain Gain value (function clamps value to the range 0.0 to 10.0)
 */
void
fluid_synth_set_gain(fluid_synth_t* synth, float gain)
{
  fluid_return_if_fail (synth != NULL);
  fluid_synth_api_enter(synth);
  
  fluid_clip (gain, 0.0f, 10.0f);

  synth->gain = gain;
  fluid_synth_update_gain_LOCAL (synth);
  fluid_synth_api_exit(synth);
}

/* Called by synthesis thread to update the gain in all voices */
static void
fluid_synth_update_gain_LOCAL(fluid_synth_t* synth)
{
  fluid_voice_t *voice;
  float gain;
  int i;

  gain = synth->gain;

  for (i = 0; i < synth->polyphony; i++)
  {
    voice = synth->voice[i];
    if (_PLAYING (voice)) fluid_voice_set_gain (voice, gain);
  }
}

/**
 * Get synth output gain value.
 * @param synth FluidSynth instance
 * @return Synth gain value (0.0 to 10.0)
 */
float
fluid_synth_get_gain(fluid_synth_t* synth)
{
  float result;
  fluid_return_val_if_fail (synth != NULL, 0.0);
  fluid_synth_api_enter(synth);

  result = synth->gain;
  FLUID_API_RETURN(result);
}

/*
 * Handler for synth.polyphony setting.
 */
static int
fluid_synth_update_polyphony(fluid_synth_t* synth, char* name, int value)
{
  fluid_synth_set_polyphony(synth, value);
  return 0;
}

/**
 * Set synthesizer polyphony (max number of voices).
 * @param synth FluidSynth instance
 * @param polyphony Polyphony to assign
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 * @since 1.0.6
 */
int
fluid_synth_set_polyphony(fluid_synth_t* synth, int polyphony)
{
  int result;
  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (polyphony >= 1 && polyphony <= 65535, FLUID_FAILED);
  fluid_synth_api_enter(synth);

  result = fluid_synth_update_polyphony_LOCAL(synth, polyphony);

  FLUID_API_RETURN(result);
}

/* Called by synthesis thread to update the polyphony value */
static int
fluid_synth_update_polyphony_LOCAL(fluid_synth_t* synth, int new_polyphony)
{
  fluid_voice_t *voice;
  int i;

  if (new_polyphony > synth->nvoice) {
    /* Create more voices */
    fluid_voice_t** new_voices = FLUID_REALLOC(synth->voice, 
					       sizeof(fluid_voice_t*) * new_polyphony);
    if (new_voices == NULL) 
      return FLUID_FAILED;
    synth->voice = new_voices;
    for (i = synth->nvoice; i < new_polyphony; i++) {
      synth->voice[i] = new_fluid_voice(synth->sample_rate);
      if (synth->voice[i] == NULL) 
	return FLUID_FAILED;
    }
    synth->nvoice = new_polyphony;
  }
  
  synth->polyphony = new_polyphony;
  /* turn off any voices above the new limit */
  for (i = synth->polyphony; i < synth->nvoice; i++)
  {
    voice = synth->voice[i];
    if (_PLAYING (voice)) fluid_voice_off (voice);
  }

  fluid_synth_update_mixer(synth, fluid_rvoice_mixer_set_polyphony, 
			   synth->polyphony, 0.0f);

  return FLUID_OK;
}

/**
 * Get current synthesizer polyphony (max number of voices).
 * @param synth FluidSynth instance
 * @return Synth polyphony value.
 * @since 1.0.6
 */
int
fluid_synth_get_polyphony(fluid_synth_t* synth)
{
  int result;
  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_synth_api_enter(synth);
  
  result = synth->polyphony;
  FLUID_API_RETURN(result);
}

/**
 * Get current number of active voices.
 * @param synth FluidSynth instance
 * @return Number of currently active voices.
 * @since 1.1.0
 *
 * Note: To generate accurate continuous statistics of the voice count, caller
 * should ensure this function is called synchronously with the audio synthesis
 * process.  This can be done in the new_fluid_audio_driver2() audio callback
 * function for example.
 */
int
fluid_synth_get_active_voice_count(fluid_synth_t* synth)
{
  int result;
  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_synth_api_enter(synth);

  result = synth->active_voice_count;
  FLUID_API_RETURN(result);
}

/**
 * Get the internal synthesis buffer size value.
 * @param synth FluidSynth instance
 * @return Internal buffer size in audio frames.
 *
 * Audio is synthesized this number of frames at a time.  Defaults to 64 frames.
 */
int
fluid_synth_get_internal_bufsize(fluid_synth_t* synth)
{
  return FLUID_BUFSIZE;
}

/**
 * Resend a bank select and a program change for every channel.
 * @param synth FluidSynth instance
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 *
 * This function is called mainly after a SoundFont has been loaded,
 * unloaded or reloaded.
 */
int
fluid_synth_program_reset(fluid_synth_t* synth)
{
  int i, prog;
  fluid_synth_api_enter(synth);
  /* try to set the correct presets */
  for (i = 0; i < synth->midi_channels; i++){
    fluid_channel_get_sfont_bank_prog (synth->channel[i], NULL, NULL, &prog);
    fluid_synth_program_change(synth, i, prog);
  }
  FLUID_API_RETURN(FLUID_OK);
}

/**
 * Synthesize a block of floating point audio to audio buffers.
 * @param synth FluidSynth instance
 * @param len Count of audio frames to synthesize
 * @param left Array of floats to store left channel of audio (len in size)
 * @param right Array of floats to store right channel of audio (len in size)
 * @param fx_left Not currently used
 * @param fx_right Not currently used
 * @return FLUID_OK on success, FLUID_FAIL otherwise
 *
 * NOTE: Should only be called from synthesis thread.
 */
int
fluid_synth_nwrite_float(fluid_synth_t* synth, int len,
                         float** left, float** right,
                         float** fx_left, float** fx_right)
{
  fluid_real_t** left_in;
  fluid_real_t** right_in;
  double time = fluid_utime();
  int i, num, available, count;
#ifdef WITH_FLOAT
  int bytes;
#endif
  float cpu_load;

  if (!synth->eventhandler->is_threadsafe)
    fluid_synth_api_enter(synth);
  
  /* First, take what's still available in the buffer */
  count = 0;
  num = synth->cur;
  if (synth->cur < FLUID_BUFSIZE) {
    available = FLUID_BUFSIZE - synth->cur;
    fluid_rvoice_mixer_get_bufs(synth->eventhandler->mixer, &left_in, &right_in);

    num = (available > len)? len : available;
#ifdef WITH_FLOAT
    bytes = num * sizeof(float);
#endif

    for (i = 0; i < synth->audio_channels; i++) {
#ifdef WITH_FLOAT
      FLUID_MEMCPY(left[i], left_in[i] + synth->cur, bytes);
      FLUID_MEMCPY(right[i], right_in[i] + synth->cur, bytes);
#else //WITH_FLOAT
      int j;
      for (j = 0; j < num; j++) {
          left[i][j] = (float) left_in[i][j + synth->cur];
          right[i][j] = (float) right_in[i][j + synth->cur];
      }
#endif //WITH_FLOAT
    }
    count += num;
    num += synth->cur; /* if we're now done, num becomes the new synth->cur below */
  }

  /* Then, run one_block() and copy till we have 'len' samples  */
  while (count < len) {
    fluid_rvoice_mixer_set_mix_fx(synth->eventhandler->mixer, 0);
    fluid_synth_render_blocks(synth, 1); // TODO: 
    fluid_rvoice_mixer_get_bufs(synth->eventhandler->mixer, &left_in, &right_in);

    num = (FLUID_BUFSIZE > len - count)? len - count : FLUID_BUFSIZE;
#ifdef WITH_FLOAT
    bytes = num * sizeof(float);
#endif

    for (i = 0; i < synth->audio_channels; i++) {
#ifdef WITH_FLOAT
      FLUID_MEMCPY(left[i] + count, left_in[i], bytes);
      FLUID_MEMCPY(right[i] + count, right_in[i], bytes);
#else //WITH_FLOAT
      int j;
      for (j = 0; j < num; j++) {
          left[i][j + count] = (float) left_in[i][j];
          right[i][j + count] = (float) right_in[i][j];
      }
#endif //WITH_FLOAT
    }

    count += num;
  }

  synth->cur = num;

  time = fluid_utime() - time;
  cpu_load = 0.5 * (synth->cpu_load + time * synth->sample_rate / len / 10000.0);
  fluid_atomic_float_set (&synth->cpu_load, cpu_load);

  if (!synth->eventhandler->is_threadsafe)
    fluid_synth_api_exit(synth);
  
  return FLUID_OK;
}

/**
 * Synthesize floating point audio to audio buffers.
 * @param synth FluidSynth instance
 * @param len Count of audio frames to synthesize
 * @param nin Ignored
 * @param in Ignored
 * @param nout Count of arrays in 'out'
 * @param out Array of arrays to store audio to
 * @return FLUID_OK on success, FLUID_FAIL otherwise
 *
 * This function implements the default interface defined in fluidsynth/audio.h.
 * NOTE: Should only be called from synthesis thread.
 */
/*
 * FIXME: Currently if nout != 2 memory allocation will occur!
 */
int
fluid_synth_process(fluid_synth_t* synth, int len, int nin, float** in,
                    int nout, float** out)
{
  if (nout==2) {
    return fluid_synth_write_float(synth, len, out[0], 0, 1, out[1], 0, 1);
  }
  else {
    float **left, **right;
    int i;
    left = FLUID_ARRAY(float*, nout/2);
    right = FLUID_ARRAY(float*, nout/2);
    for(i=0; i<nout/2; i++) {
      left[i] = out[2*i];
      right[i] = out[2*i+1];
    }
    fluid_synth_nwrite_float(synth, len, left, right, NULL, NULL);
    FLUID_FREE(left);
    FLUID_FREE(right);
    return FLUID_OK;
  }
}

/**
 * Synthesize a block of floating point audio samples to audio buffers.
 * @param synth FluidSynth instance
 * @param len Count of audio frames to synthesize
 * @param lout Array of floats to store left channel of audio
 * @param loff Offset index in 'lout' for first sample
 * @param lincr Increment between samples stored to 'lout'
 * @param rout Array of floats to store right channel of audio
 * @param roff Offset index in 'rout' for first sample
 * @param rincr Increment between samples stored to 'rout'
 * @return FLUID_OK on success, FLUID_FAIL otherwise
 *
 * Useful for storing interleaved stereo (lout = rout, loff = 0, roff = 1,
 * lincr = 2, rincr = 2).
 *
 * NOTE: Should only be called from synthesis thread.
 */
int
fluid_synth_write_float(fluid_synth_t* synth, int len,
                        void* lout, int loff, int lincr,
                        void* rout, int roff, int rincr)
{
  int i, j, k, l;
  float* left_out = (float*) lout;
  float* right_out = (float*) rout;
  fluid_real_t** left_in;
  fluid_real_t** right_in;
  double time = fluid_utime();
  float cpu_load;

  fluid_profile_ref_var (prof_ref);
  if (!synth->eventhandler->is_threadsafe)
    fluid_synth_api_enter(synth);
  
  fluid_rvoice_mixer_set_mix_fx(synth->eventhandler->mixer, 1);
  l = synth->cur;
  fluid_rvoice_mixer_get_bufs(synth->eventhandler->mixer, &left_in, &right_in);

  for (i = 0, j = loff, k = roff; i < len; i++, l++, j += lincr, k += rincr) {
    /* fill up the buffers as needed */
      if (l >= synth->curmax) {
	int blocksleft = (len-i+FLUID_BUFSIZE-1) / FLUID_BUFSIZE;
	synth->curmax = FLUID_BUFSIZE * fluid_synth_render_blocks(synth, blocksleft);
        fluid_rvoice_mixer_get_bufs(synth->eventhandler->mixer, &left_in, &right_in);

	l = 0;
      }

      left_out[j] = (float) left_in[0][l];
      right_out[k] = (float) right_in[0][l];
  }

  synth->cur = l;

  time = fluid_utime() - time;
  cpu_load = 0.5 * (synth->cpu_load + time * synth->sample_rate / len / 10000.0);
  fluid_atomic_float_set (&synth->cpu_load, cpu_load);

  if (!synth->eventhandler->is_threadsafe)
    fluid_synth_api_exit(synth);
  fluid_profile(FLUID_PROF_WRITE, prof_ref);
  
  return FLUID_OK;
}

#define DITHER_SIZE 48000
#define DITHER_CHANNELS 2

static float rand_table[DITHER_CHANNELS][DITHER_SIZE];

/* Init dither table */
static void 
init_dither(void)
{
  float d, dp;
  int c, i;

  for (c = 0; c < DITHER_CHANNELS; c++) {
    dp = 0;
    for (i = 0; i < DITHER_SIZE-1; i++) {
      d = rand() / (float)RAND_MAX - 0.5f;
      rand_table[c][i] = d - dp;
      dp = d;
    }
    rand_table[c][DITHER_SIZE-1] = 0 - dp;
  }
}

/* A portable replacement for roundf(), seems it may actually be faster too! */
static inline int
roundi (float x)
{
  if (x >= 0.0f)
    return (int)(x+0.5f);
  else
    return (int)(x-0.5f);
}

/**
 * Synthesize a block of 16 bit audio samples to audio buffers.
 * @param synth FluidSynth instance
 * @param len Count of audio frames to synthesize
 * @param lout Array of 16 bit words to store left channel of audio
 * @param loff Offset index in 'lout' for first sample
 * @param lincr Increment between samples stored to 'lout'
 * @param rout Array of 16 bit words to store right channel of audio
 * @param roff Offset index in 'rout' for first sample
 * @param rincr Increment between samples stored to 'rout'
 * @return FLUID_OK on success, FLUID_FAIL otherwise
 *
 * Useful for storing interleaved stereo (lout = rout, loff = 0, roff = 1,
 * lincr = 2, rincr = 2).
 *
 * NOTE: Should only be called from synthesis thread.
 * NOTE: Dithering is performed when converting from internal floating point to
 * 16 bit audio.
 */
int
fluid_synth_write_s16(fluid_synth_t* synth, int len,
                      void* lout, int loff, int lincr,
                      void* rout, int roff, int rincr)
{
  int i, j, k, cur;
  signed short* left_out = (signed short*) lout;
  signed short* right_out = (signed short*) rout;
  fluid_real_t** left_in;
  fluid_real_t** right_in;
  fluid_real_t left_sample;
  fluid_real_t right_sample;
  double time = fluid_utime();
  int di; 
  //double prof_ref_on_block;
  float cpu_load;
  fluid_profile_ref_var (prof_ref);
  
  if (!synth->eventhandler->is_threadsafe)
    fluid_synth_api_enter(synth);
  
  fluid_rvoice_mixer_set_mix_fx(synth->eventhandler->mixer, 1);
  fluid_rvoice_mixer_get_bufs(synth->eventhandler->mixer, &left_in, &right_in);

  cur = synth->cur;
  di = synth->dither_index;

  for (i = 0, j = loff, k = roff; i < len; i++, cur++, j += lincr, k += rincr) {

    /* fill up the buffers as needed */
    if (cur >= synth->curmax) { 
      int blocksleft = (len-i+FLUID_BUFSIZE-1) / FLUID_BUFSIZE;
      //prof_ref_on_block = fluid_profile_ref();
      synth->curmax = FLUID_BUFSIZE * fluid_synth_render_blocks(synth, blocksleft);
      fluid_rvoice_mixer_get_bufs(synth->eventhandler->mixer, &left_in, &right_in);
      cur = 0;

      //fluid_profile(FLUID_PROF_ONE_BLOCK, prof_ref_on_block);
    }

    left_sample = roundi (left_in[0][cur] * 32766.0f + rand_table[0][di]);
    right_sample = roundi (right_in[0][cur] * 32766.0f + rand_table[1][di]);

    di++;
    if (di >= DITHER_SIZE) di = 0;

    /* digital clipping */
    if (left_sample > 32767.0f) left_sample = 32767.0f;
    if (left_sample < -32768.0f) left_sample = -32768.0f;
    if (right_sample > 32767.0f) right_sample = 32767.0f;
    if (right_sample < -32768.0f) right_sample = -32768.0f;

    left_out[j] = (signed short) left_sample;
    right_out[k] = (signed short) right_sample;
  }

  synth->cur = cur;
  synth->dither_index = di;	/* keep dither buffer continous */

  fluid_profile(FLUID_PROF_WRITE, prof_ref);

  time = fluid_utime() - time;
  cpu_load = 0.5 * (synth->cpu_load + time * synth->sample_rate / len / 10000.0);
  fluid_atomic_float_set (&synth->cpu_load, cpu_load);

  if (!synth->eventhandler->is_threadsafe)
    fluid_synth_api_exit(synth);
  
  return 0;
}

/**
 * Converts stereo floating point sample data to signed 16 bit data with dithering.
 * @param dither_index Pointer to an integer which should be initialized to 0
 *   before the first call and passed unmodified to additional calls which are
 *   part of the same synthesis output.
 * @param len Length in frames to convert
 * @param lin Buffer of left audio samples to convert from
 * @param rin Buffer of right audio samples to convert from
 * @param lout Array of 16 bit words to store left channel of audio
 * @param loff Offset index in 'lout' for first sample
 * @param lincr Increment between samples stored to 'lout'
 * @param rout Array of 16 bit words to store right channel of audio
 * @param roff Offset index in 'rout' for first sample
 * @param rincr Increment between samples stored to 'rout'
 *
 * NOTE: Currently private to libfluidsynth.
 */
void
fluid_synth_dither_s16(int *dither_index, int len, float* lin, float* rin,
		       void* lout, int loff, int lincr,
		       void* rout, int roff, int rincr)
{
  int i, j, k;
  signed short* left_out = (signed short*) lout;
  signed short* right_out = (signed short*) rout;
  fluid_real_t left_sample;
  fluid_real_t right_sample;
  int di = *dither_index;
  fluid_profile_ref_var (prof_ref);

  for (i = 0, j = loff, k = roff; i < len; i++, j += lincr, k += rincr) {

    left_sample = roundi (lin[i] * 32766.0f + rand_table[0][di]);
    right_sample = roundi (rin[i] * 32766.0f + rand_table[1][di]);

    di++;
    if (di >= DITHER_SIZE) di = 0;

    /* digital clipping */
    if (left_sample > 32767.0f) left_sample = 32767.0f;
    if (left_sample < -32768.0f) left_sample = -32768.0f;
    if (right_sample > 32767.0f) right_sample = 32767.0f;
    if (right_sample < -32768.0f) right_sample = -32768.0f;

    left_out[j] = (signed short) left_sample;
    right_out[k] = (signed short) right_sample;
  }

  *dither_index = di;	/* keep dither buffer continous */

  fluid_profile(FLUID_PROF_WRITE, prof_ref);
}

static void
fluid_synth_check_finished_voices(fluid_synth_t* synth)
{
  int j;
  fluid_rvoice_t* fv;
  
  while (NULL != (fv = fluid_rvoice_eventhandler_get_finished_voice(synth->eventhandler))) {
    for (j=0; j < synth->polyphony; j++) {
      if (synth->voice[j]->rvoice == fv) {
        fluid_voice_unlock_rvoice(synth->voice[j]);
        fluid_voice_off(synth->voice[j]);
        break;
      }
      else if (synth->voice[j]->overflow_rvoice == fv) {
        fluid_voice_overflow_rvoice_finished(synth->voice[j]);
        break;
      }
    }
  }
}

/**
 * Process all waiting events in the rvoice queue.
 * Make sure no (other) rendering is running in parallel when
 * you call this function!
 */
void fluid_synth_process_event_queue(fluid_synth_t* synth)
{
  fluid_rvoice_eventhandler_dispatch_all(synth->eventhandler);
}


/**
 * Process blocks (FLUID_BUFSIZE) of audio.
 * Must be called from renderer thread only!
 * @return number of blocks rendered. Might (often) return less than requested
 */
static int
fluid_synth_render_blocks(fluid_synth_t* synth, int blockcount)
{
  int i;
  fluid_profile_ref_var (prof_ref);

  /* Assign ID of synthesis thread */
//  synth->synth_thread_id = fluid_thread_get_id ();

  fluid_check_fpe("??? Just starting up ???");
  
  fluid_rvoice_eventhandler_dispatch_all(synth->eventhandler);
  
  for (i=0; i < blockcount; i++) {
    fluid_sample_timer_process(synth);
    fluid_synth_add_ticks(synth, FLUID_BUFSIZE);
    if (fluid_rvoice_eventhandler_dispatch_count(synth->eventhandler)) {
      // Something has happened, we can't process more
      blockcount = i+1;
      break; 
    }
  }

  fluid_check_fpe("fluid_sample_timer_process");

  blockcount = fluid_rvoice_mixer_render(synth->eventhandler->mixer, blockcount);

  /* Testcase, that provokes a denormal floating point error */
#if 0
  {float num=1;while (num != 0){num*=0.5;};};
#endif
  fluid_check_fpe("??? Remainder of synth_one_block ???");
  fluid_profile(FLUID_PROF_ONE_BLOCK, prof_ref);
  return blockcount;
}


static int fluid_synth_update_overflow (fluid_synth_t *synth, char *name,
                                         fluid_real_t value)
{
  double d;
  fluid_synth_api_enter(synth);
  
  fluid_settings_getnum(synth->settings, "synth.overflow.percussion", &d);
  synth->overflow.percussion = d;
  fluid_settings_getnum(synth->settings, "synth.overflow.released", &d);
  synth->overflow.released = d;
  fluid_settings_getnum(synth->settings, "synth.overflow.sustained", &d);
  synth->overflow.sustained = d;
  fluid_settings_getnum(synth->settings, "synth.overflow.volume", &d);
  synth->overflow.volume = d;
  fluid_settings_getnum(synth->settings, "synth.overflow.age", &d);
  synth->overflow.age = d;
  
  FLUID_API_RETURN(0);
}


/* Selects a voice for killing. */
static fluid_voice_t*
fluid_synth_free_voice_by_kill_LOCAL(fluid_synth_t* synth)
{
  int i;
  fluid_real_t best_prio = OVERFLOW_PRIO_CANNOT_KILL-1;
  fluid_real_t this_voice_prio;
  fluid_voice_t* voice;
  int best_voice_index=-1;
  unsigned int ticks = fluid_synth_get_ticks(synth);
  
  for (i = 0; i < synth->polyphony; i++) {

    voice = synth->voice[i];

    /* safeguard against an available voice. */
    if (_AVAILABLE(voice)) {
      return voice;
    }
    this_voice_prio = fluid_voice_get_overflow_prio(voice, &synth->overflow,
						    ticks);

    /* check if this voice has less priority than the previous candidate. */
    if (this_voice_prio < best_prio) {
      best_voice_index = i;
      best_prio = this_voice_prio;
    }
  }

  if (best_voice_index < 0) {
    return NULL;
  }

  voice = synth->voice[best_voice_index];
  FLUID_LOG(FLUID_DBG, "Killing voice %d, index %d, chan %d, key %d ",
	    voice->id, best_voice_index, voice->chan, voice->key);
  fluid_voice_off(voice);

  return voice;
}


/**
 * Allocate a synthesis voice.
 * @param synth FluidSynth instance
 * @param sample Sample to assign to the voice
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param key MIDI note number for the voice (0-127)
 * @param vel MIDI velocity for the voice (0-127)
 * @return Allocated synthesis voice or NULL on error
 *
 * This function is called by a SoundFont's preset in response to a noteon event.
 * The returned voice comes with default modulators and generators.
 * A single noteon event may create any number of voices, when the preset is layered.
 *
 * NOTE: Should only be called from within synthesis thread, which includes
 * SoundFont loader preset noteon method.
 */
fluid_voice_t*
fluid_synth_alloc_voice(fluid_synth_t* synth, fluid_sample_t* sample, int chan, int key, int vel)
{
  int i, k;
  fluid_voice_t* voice = NULL;
  fluid_channel_t* channel = NULL;
  unsigned int ticks;

  fluid_return_val_if_fail (sample != NULL, NULL);
  FLUID_API_ENTRY_CHAN(NULL);

  /* check if there's an available synthesis process */
  for (i = 0; i < synth->polyphony; i++) {
    if (_AVAILABLE(synth->voice[i])) {
      voice = synth->voice[i];
      break;
    }
  }

  /* No success yet? Then stop a running voice. */
  if (voice == NULL) {
    FLUID_LOG(FLUID_DBG, "Polyphony exceeded, trying to kill a voice");
    voice = fluid_synth_free_voice_by_kill_LOCAL(synth);
  }

  if (voice == NULL) {
    FLUID_LOG(FLUID_WARN, "Failed to allocate a synthesis process. (chan=%d,key=%d)", chan, key);
    FLUID_API_RETURN(NULL);
  }
  ticks = fluid_synth_get_ticks(synth);

  if (synth->verbose) {
    k = 0;
    for (i = 0; i < synth->polyphony; i++) {
      if (!_AVAILABLE(synth->voice[i])) {
	k++;
      }
    }

    FLUID_LOG(FLUID_INFO, "noteon\t%d\t%d\t%d\t%05d\t%.3f\t%.3f\t%.3f\t%d",
	     chan, key, vel, synth->storeid,
	     (float) ticks / 44100.0f,
	     (fluid_curtime() - synth->start) / 1000.0f,
	     0.0f,
	     k);
  }

  if (chan >= 0) {
	  channel = synth->channel[chan];
  }

  if (fluid_voice_init (voice, sample, channel, key, vel,
                        synth->storeid, ticks, synth->gain) != FLUID_OK) {
    FLUID_LOG(FLUID_WARN, "Failed to initialize voice");
    FLUID_API_RETURN(NULL);
  }

  /* add the default modulators to the synthesis process. */
  fluid_voice_add_mod(voice, &default_vel2att_mod, FLUID_VOICE_DEFAULT);    /* SF2.01 $8.4.1  */
  fluid_voice_add_mod(voice, &default_vel2filter_mod, FLUID_VOICE_DEFAULT); /* SF2.01 $8.4.2  */
  fluid_voice_add_mod(voice, &default_at2viblfo_mod, FLUID_VOICE_DEFAULT);  /* SF2.01 $8.4.3  */
  fluid_voice_add_mod(voice, &default_mod2viblfo_mod, FLUID_VOICE_DEFAULT); /* SF2.01 $8.4.4  */
  fluid_voice_add_mod(voice, &default_att_mod, FLUID_VOICE_DEFAULT);        /* SF2.01 $8.4.5  */
  fluid_voice_add_mod(voice, &default_pan_mod, FLUID_VOICE_DEFAULT);        /* SF2.01 $8.4.6  */
  fluid_voice_add_mod(voice, &default_expr_mod, FLUID_VOICE_DEFAULT);       /* SF2.01 $8.4.7  */
  fluid_voice_add_mod(voice, &default_reverb_mod, FLUID_VOICE_DEFAULT);     /* SF2.01 $8.4.8  */
  fluid_voice_add_mod(voice, &default_chorus_mod, FLUID_VOICE_DEFAULT);     /* SF2.01 $8.4.9  */
  fluid_voice_add_mod(voice, &default_pitch_bend_mod, FLUID_VOICE_DEFAULT); /* SF2.01 $8.4.10 */

  FLUID_API_RETURN(voice);
}

/* Kill all voices on a given channel, which have the same exclusive class
 * generator as new_voice.
 */
static void
fluid_synth_kill_by_exclusive_class_LOCAL(fluid_synth_t* synth,
                                          fluid_voice_t* new_voice)
{
  int excl_class = _GEN(new_voice,GEN_EXCLUSIVECLASS);
  fluid_voice_t* existing_voice;
  int i;

  /* Excl. class 0: No exclusive class */
  if (excl_class == 0) return;

  /* Kill all notes on the same channel with the same exclusive class */
  for (i = 0; i < synth->polyphony; i++) {
    existing_voice = synth->voice[i];

    /* If voice is playing, on the same channel, has same exclusive
     * class and is not part of the same noteon event (voice group), then kill it */

    if (_PLAYING(existing_voice)
        && existing_voice->chan == new_voice->chan
        && (int)_GEN (existing_voice, GEN_EXCLUSIVECLASS) == excl_class
        && fluid_voice_get_id (existing_voice) != fluid_voice_get_id(new_voice))
      fluid_voice_kill_excl(existing_voice);
  }
}

/**
 * Activate a voice previously allocated with fluid_synth_alloc_voice().
 * @param synth FluidSynth instance
 * @param voice Voice to activate
 *
 * This function is called by a SoundFont's preset in response to a noteon
 * event.  Exclusive classes are processed here.
 *
 * NOTE: Should only be called from within synthesis thread, which includes
 * SoundFont loader preset noteon method.
 */
void
fluid_synth_start_voice(fluid_synth_t* synth, fluid_voice_t* voice)
{
  fluid_return_if_fail (synth != NULL);
  fluid_return_if_fail (voice != NULL);
//  fluid_return_if_fail (fluid_synth_is_synth_thread (synth));
  fluid_synth_api_enter(synth);

  /* Find the exclusive class of this voice. If set, kill all voices
   * that match the exclusive class and are younger than the first
   * voice process created by this noteon event. */
  fluid_synth_kill_by_exclusive_class_LOCAL(synth, voice);

  fluid_voice_start(voice);     /* Start the new voice */
  if (synth->eventhandler->is_threadsafe)
    fluid_voice_lock_rvoice(voice);
  fluid_rvoice_eventhandler_add_rvoice(synth->eventhandler, voice->rvoice);
  fluid_synth_api_exit(synth);
}

/**
 * Add a SoundFont loader interface.
 * @param synth FluidSynth instance
 * @param loader Loader API structure, used directly and should remain allocated
 *   as long as the synth instance is used.
 *
 * SoundFont loaders are used to add custom instrument loading to FluidSynth.
 * The caller supplied functions for loading files, allocating presets,
 * retrieving information on them and synthesizing note-on events.  Using this
 * method even non SoundFont instruments can be synthesized, although limited
 * to the SoundFont synthesis model.
 *
 * NOTE: Should only be called before any SoundFont files are loaded.
 */
void
fluid_synth_add_sfloader(fluid_synth_t* synth, fluid_sfloader_t* loader)
{
  gboolean sfont_already_loaded;

  fluid_return_if_fail (synth != NULL);
  fluid_return_if_fail (loader != NULL);
  fluid_synth_api_enter(synth);
  sfont_already_loaded = synth->sfont_info != NULL;
  if (!sfont_already_loaded) 
    synth->loaders = fluid_list_prepend(synth->loaders, loader);
  fluid_synth_api_exit(synth);
}

/**
 * Load a SoundFont file (filename is interpreted by SoundFont loaders).
 * The newly loaded SoundFont will be put on top of the SoundFont
 * stack. Presets are searched starting from the SoundFont on the
 * top of the stack, working the way down the stack until a preset is found.
 *
 * @param synth SoundFont instance
 * @param filename File to load
 * @param reset_presets TRUE to re-assign presets for all MIDI channels
 * @return SoundFont ID on success, FLUID_FAILED on error
 */
int
fluid_synth_sfload(fluid_synth_t* synth, const char* filename, int reset_presets)
{
  fluid_sfont_info_t *sfont_info;
  fluid_sfont_t *sfont;
  fluid_list_t *list;
  fluid_sfloader_t *loader;
  unsigned int sfont_id;

  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (filename != NULL, FLUID_FAILED);
  fluid_synth_api_enter(synth);
  
  /* MT NOTE: Loaders list should not change. */

  for (list = synth->loaders; list; list = fluid_list_next(list)) {
    loader = (fluid_sfloader_t*) fluid_list_get(list);

    sfont = fluid_sfloader_load(loader, filename);

    if (sfont != NULL) {
      sfont_info = new_fluid_sfont_info (synth, sfont);

      if (!sfont_info)
      {
        delete_fluid_sfont (sfont);
        FLUID_API_RETURN(FLUID_FAILED);
      }

      sfont->id = sfont_id = ++synth->sfont_id;
      synth->sfont_info = fluid_list_prepend(synth->sfont_info, sfont_info);   /* prepend to list */
      fluid_hashtable_insert (synth->sfont_hash, sfont, sfont_info);       /* Hash sfont->sfont_info */

      /* reset the presets for all channels if requested */
      if (reset_presets) fluid_synth_program_reset(synth);

      FLUID_API_RETURN((int)sfont_id);
    }
  }

  FLUID_LOG(FLUID_ERR, "Failed to load SoundFont \"%s\"", filename);
  FLUID_API_RETURN(FLUID_FAILED);
}

/* Create a new SoundFont info structure, free with FLUID_FREE */
static fluid_sfont_info_t *
new_fluid_sfont_info (fluid_synth_t *synth, fluid_sfont_t *sfont)
{
  fluid_sfont_info_t *sfont_info;

  sfont_info = FLUID_NEW (fluid_sfont_info_t);

  if (!sfont_info)
  {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }

  sfont_info->sfont = sfont;
  sfont_info->synth = synth;
  sfont_info->refcount = 1;     /* Start with refcount of 1 for owning synth */
  sfont_info->bankofs = 0;

  return (sfont_info);
}

/**
 * Unload a SoundFont.
 * @param synth SoundFont instance
 * @param id ID of SoundFont to unload
 * @param reset_presets TRUE to re-assign presets for all MIDI channels
 * @return FLUID_OK on success, FLUID_FAILED on error
 */
int
fluid_synth_sfunload(fluid_synth_t* synth, unsigned int id, int reset_presets)
{
  fluid_sfont_info_t *sfont_info = NULL;
  fluid_list_t *list;

  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_synth_api_enter(synth);
  
  /* remove the SoundFont from the list */
  for (list = synth->sfont_info; list; list = fluid_list_next(list)) {
    sfont_info = (fluid_sfont_info_t*) fluid_list_get(list);

    if (fluid_sfont_get_id (sfont_info->sfont) == id)
    {
      synth->sfont_info = fluid_list_remove (synth->sfont_info, sfont_info);
      break;
    }
  }

  if (!list) {
    FLUID_LOG(FLUID_ERR, "No SoundFont with id = %d", id);
    FLUID_API_RETURN(FLUID_FAILED);
  }

  /* reset the presets for all channels (SoundFont will be freed when there are no more references) */
  if (reset_presets) fluid_synth_program_reset (synth);
  else fluid_synth_update_presets (synth);

  /* -- Remove synth->sfont_info list's reference to SoundFont */
  fluid_synth_sfont_unref (synth, sfont_info->sfont);

  FLUID_API_RETURN(FLUID_OK);
}

/* Unref a SoundFont and destroy if no more references */
void
fluid_synth_sfont_unref (fluid_synth_t *synth, fluid_sfont_t *sfont)
{
  fluid_sfont_info_t *sfont_info;
  int refcount = 0;
  
  sfont_info = fluid_hashtable_lookup (synth->sfont_hash, sfont);

  if (sfont_info)
  {
    sfont_info->refcount--;             /* -- Remove the sfont_info list's reference */
    refcount = sfont_info->refcount;

    if (refcount == 0)    /* Remove SoundFont from hash if no more references */
      fluid_hashtable_remove (synth->sfont_hash, sfont_info->sfont);
  }

  fluid_return_if_fail (sfont_info != NULL);    /* Shouldn't happen, programming error if so */

  if (refcount == 0)                    /* No more references? - Attempt delete */
  {
    if (delete_fluid_sfont (sfont_info->sfont) == 0)    /* SoundFont loader can block SoundFont unload */
    {
      FLUID_FREE (sfont_info);
      FLUID_LOG (FLUID_DBG, "Unloaded SoundFont");
    } /* spin off a timer thread to unload the sfont later (SoundFont loader blocked unload) */
    else new_fluid_timer (100, fluid_synth_sfunload_callback, sfont_info, TRUE, TRUE, FALSE);    
  }
}

/* Callback to continually attempt to unload a SoundFont,
 * only if a SoundFont loader blocked the unload operation */
static int
fluid_synth_sfunload_callback(void* data, unsigned int msec)
{
  fluid_sfont_info_t *sfont_info = (fluid_sfont_info_t *)data;

  if (delete_fluid_sfont (sfont_info->sfont) == 0)
  {
    FLUID_FREE (sfont_info);
    FLUID_LOG (FLUID_DBG, "Unloaded SoundFont");
    return FALSE;
  }
  else return TRUE;
}

/**
 * Reload a SoundFont.  The SoundFont retains its ID and index on the SoundFont stack.
 * @param synth SoundFont instance
 * @param id ID of SoundFont to reload
 * @return SoundFont ID on success, FLUID_FAILED on error
 */
int
fluid_synth_sfreload(fluid_synth_t* synth, unsigned int id)
{
  char filename[1024];
  fluid_sfont_info_t *sfont_info, *old_sfont_info;
  fluid_sfont_t* sfont;
  fluid_sfloader_t* loader;
  fluid_list_t *list;
  int index;

  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_synth_api_enter(synth);

  /* Search for SoundFont and get its index */
  for (list = synth->sfont_info, index = 0; list; list = fluid_list_next (list), index++) {
    old_sfont_info = (fluid_sfont_info_t *)fluid_list_get (list);
    if (fluid_sfont_get_id (old_sfont_info->sfont) == id) break;
  }

  if (!list) {
    FLUID_LOG(FLUID_ERR, "No SoundFont with id = %d", id);
    FLUID_API_RETURN(FLUID_FAILED);
  }

  /* keep a copy of the SoundFont's filename */
  FLUID_STRCPY (filename, fluid_sfont_get_name (old_sfont_info->sfont));

  if (fluid_synth_sfunload (synth, id, FALSE) != FLUID_OK)
    FLUID_API_RETURN(FLUID_FAILED);

  /* MT Note: SoundFont loader list will not change */

  for (list = synth->loaders; list; list = fluid_list_next(list)) {
    loader = (fluid_sfloader_t*) fluid_list_get(list);

    sfont = fluid_sfloader_load(loader, filename);

    if (sfont != NULL) {
      sfont->id = id;

      sfont_info = new_fluid_sfont_info (synth, sfont);

      if (!sfont_info)
      {
        delete_fluid_sfont (sfont);
        FLUID_API_RETURN(FLUID_FAILED);
      }

      synth->sfont_info = fluid_list_insert_at(synth->sfont_info, index, sfont_info);  /* insert the sfont at the same index */
      fluid_hashtable_insert (synth->sfont_hash, sfont, sfont_info);       /* Hash sfont->sfont_info */

      /* reset the presets for all channels */
      fluid_synth_update_presets(synth);
      FLUID_API_RETURN(sfont->id);
    }
  }

  FLUID_LOG(FLUID_ERR, "Failed to load SoundFont \"%s\"", filename);
  FLUID_API_RETURN(FLUID_FAILED);  
}

/**
 * Add a SoundFont.  The SoundFont will be added to the top of the SoundFont stack.
 * @param synth FluidSynth instance
 * @param sfont SoundFont to add
 * @return New assigned SoundFont ID or FLUID_FAILED on error
 */
int
fluid_synth_add_sfont(fluid_synth_t* synth, fluid_sfont_t* sfont)
{
  fluid_sfont_info_t *sfont_info;
  unsigned int sfont_id;

  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (sfont != NULL, FLUID_FAILED);
  fluid_synth_api_enter(synth);
  
  sfont_info = new_fluid_sfont_info (synth, sfont);
  if (!sfont_info) 
      FLUID_API_RETURN(FLUID_FAILED);

  sfont->id = sfont_id = ++synth->sfont_id;
  synth->sfont_info = fluid_list_prepend (synth->sfont_info, sfont_info);       /* prepend to list */
  fluid_hashtable_insert (synth->sfont_hash, sfont, sfont_info);   /* Hash sfont->sfont_info */

  /* reset the presets for all channels */
  fluid_synth_program_reset (synth);

  FLUID_API_RETURN(sfont_id);
}

/**
 * Remove a SoundFont from the SoundFont stack without deleting it.
 * @param synth FluidSynth instance
 * @param sfont SoundFont to remove
 *
 * SoundFont is not freed and is left as the responsibility of the caller.
 *
 * NOTE: The SoundFont should only be freed after there are no presets
 *   referencing it.  This can only be ensured by the SoundFont loader and
 *   therefore this function should not normally be used.
 */
void
fluid_synth_remove_sfont(fluid_synth_t* synth, fluid_sfont_t* sfont)
{
  fluid_sfont_info_t *sfont_info;
  fluid_list_t *list;

  fluid_return_if_fail (synth != NULL);
  fluid_return_if_fail (sfont != NULL);
  fluid_synth_api_enter(synth);
  
  /* remove the SoundFont from the list */
  for (list = synth->sfont_info; list; list = fluid_list_next(list)) {
    sfont_info = (fluid_sfont_info_t*) fluid_list_get(list);

    if (sfont_info->sfont == sfont)
    {
      synth->sfont_info = fluid_list_remove (synth->sfont_info, sfont_info);

      /* Remove from SoundFont hash regardless of refcount (SoundFont delete is up to caller) */
      fluid_hashtable_remove (synth->sfont_hash, sfont_info->sfont);
      break;
    }
  }

  /* reset the presets for all channels */
  fluid_synth_program_reset (synth);
  fluid_synth_api_exit(synth);
}

/**
 * Count number of loaded SoundFont files.
 * @param synth FluidSynth instance
 * @return Count of loaded SoundFont files.
 */
int
fluid_synth_sfcount(fluid_synth_t* synth)
{
  int count;
  
  fluid_return_val_if_fail (synth != NULL, 0);
  fluid_synth_api_enter(synth);
  count = fluid_list_size (synth->sfont_info);
  FLUID_API_RETURN(count);
}

/**
 * Get SoundFont by index.
 * @param synth FluidSynth instance
 * @param num SoundFont index on the stack (starting from 0 for top of stack).
 * @return SoundFont instance or NULL if invalid index
 *
 * NOTE: Caller should be certain that SoundFont is not deleted (unloaded) for
 * the duration of use of the returned pointer.
 */
fluid_sfont_t *
fluid_synth_get_sfont(fluid_synth_t* synth, unsigned int num)
{
  fluid_sfont_t *sfont = NULL;
  fluid_list_t *list;

  fluid_return_val_if_fail (synth != NULL, NULL);
  fluid_synth_api_enter(synth);
  list = fluid_list_nth (synth->sfont_info, num);
  if (list) sfont = ((fluid_sfont_info_t *)fluid_list_get (list))->sfont;
  FLUID_API_RETURN(sfont);
}

/**
 * Get SoundFont by ID.
 * @param synth FluidSynth instance
 * @param id SoundFont ID
 * @return SoundFont instance or NULL if invalid ID
 *
 * NOTE: Caller should be certain that SoundFont is not deleted (unloaded) for
 * the duration of use of the returned pointer.
 */
fluid_sfont_t *
fluid_synth_get_sfont_by_id(fluid_synth_t* synth, unsigned int id)
{
  fluid_sfont_t* sfont = NULL;
  fluid_list_t* list;

  fluid_return_val_if_fail (synth != NULL, NULL);
  fluid_synth_api_enter(synth);

  for (list = synth->sfont_info; list; list = fluid_list_next(list)) {
    sfont = ((fluid_sfont_info_t *)fluid_list_get (list))->sfont;
    if (fluid_sfont_get_id (sfont) == id)
      break;
  }

  FLUID_API_RETURN(list ? sfont : NULL);
}

/**
 * Get SoundFont by name.
 * @param synth FluidSynth instance
 * @param name Name of SoundFont
 * @return SoundFont instance or NULL if invalid name
 * @since 1.1.0
 *
 * NOTE: Caller should be certain that SoundFont is not deleted (unloaded) for
 * the duration of use of the returned pointer.
 */
fluid_sfont_t *
fluid_synth_get_sfont_by_name(fluid_synth_t* synth, const char *name)
{
  fluid_sfont_t* sfont = NULL;
  fluid_list_t* list;

  fluid_return_val_if_fail (synth != NULL, NULL);
  fluid_return_val_if_fail (name != NULL, NULL);
  fluid_synth_api_enter(synth);

  for (list = synth->sfont_info; list; list = fluid_list_next(list)) {
    sfont = ((fluid_sfont_info_t *)fluid_list_get (list))->sfont;
    if (FLUID_STRCMP(fluid_sfont_get_name(sfont), name) == 0)
      break;
  }

  FLUID_API_RETURN(list ? sfont : NULL);
}

/**
 * Get active preset on a MIDI channel.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @return Preset or NULL if no preset active on channel
 * @deprecated fluid_synth_get_channel_info() should replace most use cases.
 *
 * NOTE: Should only be called from within synthesis thread, which includes
 * SoundFont loader preset noteon methods.  Not thread safe otherwise.
 */
fluid_preset_t *
fluid_synth_get_channel_preset(fluid_synth_t* synth, int chan)
{
  fluid_preset_t* result;
  fluid_channel_t *channel;
  FLUID_API_ENTRY_CHAN(NULL);

  channel = synth->channel[chan];
  result = channel->preset;
  fluid_synth_api_exit(synth);
  return result;
}

/**
 * Get information on the currently selected preset on a MIDI channel.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param info Caller supplied structure to fill with preset information
 * @return #FLUID_OK on success, #FLUID_FAILED otherwise
 * @since 1.1.1
 */
int
fluid_synth_get_channel_info (fluid_synth_t *synth, int chan,
                              fluid_synth_channel_info_t *info)
{
  fluid_channel_t *channel;
  fluid_preset_t *preset;
  char *name;

  if (info)
  {
    info->assigned = FALSE;
    info->name[0] = '\0';
  }

  fluid_return_val_if_fail (info != NULL, FLUID_FAILED);
  FLUID_API_ENTRY_CHAN(FLUID_FAILED);
  
  channel = synth->channel[chan];
  preset = channel->preset;

  if (preset)
  {
    info->assigned = TRUE;
    name = fluid_preset_get_name (preset);

    if (name)
    {
      strncpy (info->name, name, FLUID_SYNTH_CHANNEL_INFO_NAME_SIZE);
      info->name[FLUID_SYNTH_CHANNEL_INFO_NAME_SIZE - 1] = '\0';
    }
    else info->name[0] = '\0';

    info->sfont_id = preset->sfont->id;
    info->bank = fluid_preset_get_banknum (preset);
    info->program = fluid_preset_get_num (preset);
  }
  else
  {
    info->assigned = FALSE;
    fluid_channel_get_sfont_bank_prog (channel, &info->sfont_id, &info->bank, &info->program);
    info->name[0] = '\0';
  }

  fluid_synth_api_exit(synth);
  return FLUID_OK;
}

/**
 * Get list of voices.
 * @param synth FluidSynth instance
 * @param buf Array to store voices to (NULL terminated if not filled completely)
 * @param bufsize Count of indexes in buf
 * @param id Voice ID to search for or < 0 to return list of all playing voices
 *
 * NOTE: Should only be called from within synthesis thread, which includes
 * SoundFont loader preset noteon methods.  Voices are only guaranteed to remain
 * unchanged until next synthesis process iteration.
 */
void
fluid_synth_get_voicelist(fluid_synth_t* synth, fluid_voice_t* buf[], int bufsize,
                          int id)
{
  int count = 0;
  int i;

  fluid_return_if_fail (synth != NULL);
  fluid_return_if_fail (buf != NULL);
  fluid_synth_api_enter(synth);

  for (i = 0; i < synth->polyphony && count < bufsize; i++) {
    fluid_voice_t* voice = synth->voice[i];

    if (_PLAYING(voice) && (id < 0 || (int)voice->id == id))
      buf[count++] = voice;
  }

  if (count < bufsize) buf[count] = NULL;
  fluid_synth_api_exit(synth);
}

/**
 * Enable or disable reverb effect.
 * @param synth FluidSynth instance
 * @param on TRUE to enable reverb, FALSE to disable
 */
void
fluid_synth_set_reverb_on(fluid_synth_t* synth, int on)
{
  fluid_return_if_fail (synth != NULL);

  fluid_atomic_int_set (&synth->with_reverb, on != 0);
  fluid_synth_update_mixer(synth, fluid_rvoice_mixer_set_reverb_enabled,
			   on != 0, 0.0f);
}

/**
 * Activate a reverb preset.
 * @param synth FluidSynth instance
 * @param num Reverb preset number
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 *
 * NOTE: Currently private to libfluidsynth.
 */
int
fluid_synth_set_reverb_preset(fluid_synth_t* synth, int num)
{
  int i = 0;
  while (revmodel_preset[i].name != NULL) {
    if (i == num) {
      fluid_synth_set_reverb (synth, revmodel_preset[i].roomsize,
                              revmodel_preset[i].damp, revmodel_preset[i].width,
                              revmodel_preset[i].level);
      return FLUID_OK;
    }
    i++;
  }
  return FLUID_FAILED;
}

/**
 * Set reverb parameters.
 * @param synth FluidSynth instance
 * @param roomsize Reverb room size value (0.0-1.2)
 * @param damping Reverb damping value (0.0-1.0)
 * @param width Reverb width value (0.0-100.0)
 * @param level Reverb level value (0.0-1.0)
 *
 * NOTE: Not realtime safe and therefore should not be called from synthesis
 * context at the risk of stalling audio output.
 */
void
fluid_synth_set_reverb(fluid_synth_t* synth, double roomsize, double damping,
                       double width, double level)
{
  fluid_synth_set_reverb_full (synth, FLUID_REVMODEL_SET_ALL,
                               roomsize, damping, width, level);
}

/**
 * Set one or more reverb parameters.
 * @param synth FluidSynth instance
 * @param set Flags indicating which parameters should be set (#fluid_revmodel_set_t)
 * @param roomsize Reverb room size value (0.0-1.2)
 * @param damping Reverb damping value (0.0-1.0)
 * @param width Reverb width value (0.0-100.0)
 * @param level Reverb level value (0.0-1.0)
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 *
 * NOTE: Not realtime safe and therefore should not be called from synthesis
 * context at the risk of stalling audio output.
 */
int
fluid_synth_set_reverb_full(fluid_synth_t* synth, int set, double roomsize,
                            double damping, double width, double level)
{
  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);

  if (!(set & FLUID_REVMODEL_SET_ALL))
    set = FLUID_REVMODEL_SET_ALL; 

  /* Synth shadow values are set here so that they will be returned if querried */

  fluid_synth_api_enter(synth);

  if (set & FLUID_REVMODEL_SET_ROOMSIZE)
    fluid_atomic_float_set (&synth->reverb_roomsize, roomsize);

  if (set & FLUID_REVMODEL_SET_DAMPING)
    fluid_atomic_float_set (&synth->reverb_damping, damping);

  if (set & FLUID_REVMODEL_SET_WIDTH)
    fluid_atomic_float_set (&synth->reverb_width, width);

  if (set & FLUID_REVMODEL_SET_LEVEL)
    fluid_atomic_float_set (&synth->reverb_level, level);

  fluid_rvoice_eventhandler_push5(synth->eventhandler, 
				  fluid_rvoice_mixer_set_reverb_params, 
				  synth->eventhandler->mixer, set, 
				  roomsize, damping, width, level, 0.0f);
  
  FLUID_API_RETURN(FLUID_OK);
}

/**
 * Get reverb room size.
 * @param synth FluidSynth instance
 * @return Reverb room size (0.0-1.2)
 */
double
fluid_synth_get_reverb_roomsize(fluid_synth_t* synth)
{
  double result;
  fluid_return_val_if_fail (synth != NULL, 0.0);
  fluid_synth_api_enter(synth);
  result = fluid_atomic_float_get (&synth->reverb_roomsize);
  FLUID_API_RETURN(result);
}

/**
 * Get reverb damping.
 * @param synth FluidSynth instance
 * @return Reverb damping value (0.0-1.0)
 */
double
fluid_synth_get_reverb_damp(fluid_synth_t* synth)
{
  double result;
  fluid_return_val_if_fail (synth != NULL, 0.0);
  fluid_synth_api_enter(synth);

  result = fluid_atomic_float_get (&synth->reverb_damping);
  FLUID_API_RETURN(result);
}

/**
 * Get reverb level.
 * @param synth FluidSynth instance
 * @return Reverb level value (0.0-1.0)
 */
double
fluid_synth_get_reverb_level(fluid_synth_t* synth)
{
  double result;
  fluid_return_val_if_fail (synth != NULL, 0.0);
  fluid_synth_api_enter(synth);

  result = fluid_atomic_float_get (&synth->reverb_level);
  FLUID_API_RETURN(result);
}

/**
 * Get reverb width.
 * @param synth FluidSynth instance
 * @return Reverb width value (0.0-100.0)
 */
double
fluid_synth_get_reverb_width(fluid_synth_t* synth)
{
  double result;
  fluid_return_val_if_fail (synth != NULL, 0.0);
  fluid_synth_api_enter(synth);

  result = fluid_atomic_float_get (&synth->reverb_width);
  FLUID_API_RETURN(result);
}

/**
 * Enable or disable chorus effect.
 * @param synth FluidSynth instance
 * @param on TRUE to enable chorus, FALSE to disable
 */
void 
fluid_synth_set_chorus_on(fluid_synth_t* synth, int on)
{
  fluid_return_if_fail (synth != NULL);
  fluid_synth_api_enter(synth);

  fluid_atomic_int_set (&synth->with_chorus, on != 0);
  fluid_synth_update_mixer(synth, fluid_rvoice_mixer_set_chorus_enabled,
			   on != 0, 0.0f);
  fluid_synth_api_exit(synth);
}

/**
 * Set chorus parameters.
 * @param synth FluidSynth instance
 * @param nr Chorus voice count (0-99, CPU time consumption proportional to
 *   this value)
 * @param level Chorus level (0.0-10.0)
 * @param speed Chorus speed in Hz (0.29-5.0)
 * @param depth_ms Chorus depth (max value depends on synth sample rate,
 *   0.0-21.0 is safe for sample rate values up to 96KHz)
 * @param type Chorus waveform type (#fluid_chorus_mod)
 */
void
fluid_synth_set_chorus(fluid_synth_t* synth, int nr, double level,
                       double speed, double depth_ms, int type)
{
  fluid_synth_set_chorus_full (synth, FLUID_CHORUS_SET_ALL, nr, level, speed,
                               depth_ms, type);
}

/**
 * Set one or more chorus parameters.
 * @param synth FluidSynth instance
 * @param set Flags indicating which chorus parameters to set (#fluid_chorus_set_t)
 * @param nr Chorus voice count (0-99, CPU time consumption proportional to
 *   this value)
 * @param level Chorus level (0.0-10.0)
 * @param speed Chorus speed in Hz (0.29-5.0)
 * @param depth_ms Chorus depth (max value depends on synth sample rate,
 *   0.0-21.0 is safe for sample rate values up to 96KHz)
 * @param type Chorus waveform type (#fluid_chorus_mod)
 */
int
fluid_synth_set_chorus_full(fluid_synth_t* synth, int set, int nr, double level,
                            double speed, double depth_ms, int type)
{
  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);

  if (!(set & FLUID_CHORUS_SET_ALL))
    set = FLUID_CHORUS_SET_ALL;

  /* Synth shadow values are set here so that they will be returned if queried */
  fluid_synth_api_enter(synth);

  if (set & FLUID_CHORUS_SET_NR)
    fluid_atomic_int_set (&synth->chorus_nr, nr);

  if (set & FLUID_CHORUS_SET_LEVEL)
    fluid_atomic_float_set (&synth->chorus_level, level);

  if (set & FLUID_CHORUS_SET_SPEED)
    fluid_atomic_float_set (&synth->chorus_speed, speed);

  if (set & FLUID_CHORUS_SET_DEPTH)
    fluid_atomic_float_set (&synth->chorus_depth, depth_ms);

  if (set & FLUID_CHORUS_SET_TYPE)
    fluid_atomic_int_set (&synth->chorus_type, type);
  
  fluid_rvoice_eventhandler_push5(synth->eventhandler, 
				  fluid_rvoice_mixer_set_chorus_params,
				  synth->eventhandler->mixer, set,
				  nr, level, speed, depth_ms, type);

  FLUID_API_RETURN(FLUID_OK);
}

/**
 * Get chorus voice number (delay line count) value.
 * @param synth FluidSynth instance
 * @return Chorus voice count (0-99)
 */
int
fluid_synth_get_chorus_nr(fluid_synth_t* synth)
{
  double result;
  fluid_return_val_if_fail (synth != NULL, 0.0);
  fluid_synth_api_enter(synth);

  result = fluid_atomic_int_get (&synth->chorus_nr);
  FLUID_API_RETURN(result);
}

/**
 * Get chorus level.
 * @param synth FluidSynth instance
 * @return Chorus level value (0.0-10.0)
 */
double
fluid_synth_get_chorus_level(fluid_synth_t* synth)
{
  double result;
  fluid_return_val_if_fail (synth != NULL, 0.0);
  fluid_synth_api_enter(synth);

  result = fluid_atomic_float_get (&synth->chorus_level);
  FLUID_API_RETURN(result);
}

/**
 * Get chorus speed in Hz.
 * @param synth FluidSynth instance
 * @return Chorus speed in Hz (0.29-5.0)
 */
double
fluid_synth_get_chorus_speed_Hz(fluid_synth_t* synth)
{
  double result;
  fluid_return_val_if_fail (synth != NULL, 0.0);
  fluid_synth_api_enter(synth);

  result = fluid_atomic_float_get (&synth->chorus_speed);
  FLUID_API_RETURN(result);
}

/**
 * Get chorus depth.
 * @param synth FluidSynth instance
 * @return Chorus depth
 */
double
fluid_synth_get_chorus_depth_ms(fluid_synth_t* synth)
{
  double result;
  fluid_return_val_if_fail (synth != NULL, 0.0);
  fluid_synth_api_enter(synth);

  result = fluid_atomic_float_get (&synth->chorus_depth);
  FLUID_API_RETURN(result);
}

/**
 * Get chorus waveform type.
 * @param synth FluidSynth instance
 * @return Chorus waveform type (#fluid_chorus_mod)
 */
int
fluid_synth_get_chorus_type(fluid_synth_t* synth)
{
  double result;
  fluid_return_val_if_fail (synth != NULL, 0.0);
  fluid_synth_api_enter(synth);

  result = fluid_atomic_int_get (&synth->chorus_type);
  FLUID_API_RETURN(result);
}

/*
 * If the same note is hit twice on the same channel, then the older
 * voice process is advanced to the release stage.  Using a mechanical
 * MIDI controller, the only way this can happen is when the sustain
 * pedal is held.  In this case the behaviour implemented here is
 * natural for many instruments.  Note: One noteon event can trigger
 * several voice processes, for example a stereo sample.  Don't
 * release those...
 */
static void
fluid_synth_release_voice_on_same_note_LOCAL(fluid_synth_t* synth, int chan,
                                             int key)
{
  int i;
  fluid_voice_t* voice;

  synth->storeid = synth->noteid++;

  for (i = 0; i < synth->polyphony; i++) {
    voice = synth->voice[i];
    if (_PLAYING(voice)
	&& (voice->chan == chan)
	&& (voice->key == key)
	&& (fluid_voice_get_id(voice) != synth->noteid)) {
      /* Id of voices that was sustained by sostenuto */
      if(_HELD_BY_SOSTENUTO(voice))
        synth->storeid = voice->id;
      /* Force the voice into release stage (pedaling is ignored) */
      fluid_voice_release(voice);
    }
  }
}

/**
 * Set synthesis interpolation method on one or all MIDI channels.
 * @param synth FluidSynth instance
 * @param chan MIDI channel to set interpolation method on or -1 for all channels
 * @param interp_method Interpolation method (#fluid_interp)
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_set_interp_method(fluid_synth_t* synth, int chan, int interp_method)
{
  int i;
  
  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED); 
  fluid_synth_api_enter(synth); 
  if (chan < -1 || chan >= synth->midi_channels) 
    FLUID_API_RETURN(FLUID_FAILED);

  if (synth->channel[0] == NULL) {
    FLUID_LOG (FLUID_ERR, "Channels don't exist (yet)!");
    FLUID_API_RETURN(FLUID_FAILED);
  }

  for (i = 0; i < synth->midi_channels; i++) {
    if (chan < 0 || fluid_channel_get_num(synth->channel[i]) == chan)
      fluid_channel_set_interp_method(synth->channel[i], interp_method);
  }

  FLUID_API_RETURN(FLUID_OK);
};

/**
 * Get the total count of MIDI channels.
 * @param synth FluidSynth instance
 * @return Count of MIDI channels
 */
int
fluid_synth_count_midi_channels(fluid_synth_t* synth)
{
  int result;
  fluid_return_val_if_fail (synth != NULL, 0);
  fluid_synth_api_enter(synth);

  result = synth->midi_channels;
  FLUID_API_RETURN(result);
}

/**
 * Get the total count of audio channels.
 * @param synth FluidSynth instance
 * @return Count of audio channel stereo pairs (1 = 2 channels, 2 = 4, etc)
 */
int
fluid_synth_count_audio_channels(fluid_synth_t* synth)
{
  int result;
  fluid_return_val_if_fail (synth != NULL, 0);
  fluid_synth_api_enter(synth);

  result = synth->audio_channels;
  FLUID_API_RETURN(result);
}

/**
 * Get the total number of allocated audio channels.  Usually identical to the
 * number of audio channels.  Can be employed by LADSPA effects subsystem.
 *
 * @param synth FluidSynth instance
 * @return Count of audio group stereo pairs (1 = 2 channels, 2 = 4, etc)
 */
int
fluid_synth_count_audio_groups(fluid_synth_t* synth)
{
  int result;
  fluid_return_val_if_fail (synth != NULL, 0);
  fluid_synth_api_enter(synth);

  result = synth->audio_groups;
  FLUID_API_RETURN(result);
}

/**
 * Get the total number of allocated effects channels.
 * @param synth FluidSynth instance
 * @return Count of allocated effects channels
 */
int
fluid_synth_count_effects_channels(fluid_synth_t* synth)
{
  int result;
  fluid_return_val_if_fail (synth != NULL, 0);
  fluid_synth_api_enter(synth);

  result = synth->effects_channels;
  FLUID_API_RETURN(result);
}

/**
 * Get the synth CPU load value.
 * @param synth FluidSynth instance
 * @return Estimated CPU load value in percent (0-100)
 */
double
fluid_synth_get_cpu_load(fluid_synth_t* synth)
{
  fluid_return_val_if_fail (synth != NULL, 0);
  return fluid_atomic_float_get (&synth->cpu_load);
}

/* Get tuning for a given bank:program */
static fluid_tuning_t *
fluid_synth_get_tuning(fluid_synth_t* synth, int bank, int prog)
{

  if ((synth->tuning == NULL) ||
      (synth->tuning[bank] == NULL) ||
      (synth->tuning[bank][prog] == NULL))
    return NULL;

  return synth->tuning[bank][prog];
}

/* Replace tuning on a given bank:program (need not already exist).
 * Synth mutex should already be locked by caller. */
static int
fluid_synth_replace_tuning_LOCK (fluid_synth_t* synth, fluid_tuning_t *tuning,
                                 int bank, int prog, int apply)
{
  fluid_tuning_t *old_tuning;
//  fluid_event_queue_t *queue;
//  fluid_event_queue_elem_t *event;

  if (synth->tuning == NULL) {
    synth->tuning = FLUID_ARRAY(fluid_tuning_t**, 128);
    if (synth->tuning == NULL) {
      FLUID_LOG(FLUID_PANIC, "Out of memory");
      return FLUID_FAILED;
    }
    FLUID_MEMSET(synth->tuning, 0, 128 * sizeof(fluid_tuning_t**));
  }

  if (synth->tuning[bank] == NULL) {
    synth->tuning[bank] = FLUID_ARRAY(fluid_tuning_t*, 128);
    if (synth->tuning[bank] == NULL) {
      FLUID_LOG(FLUID_PANIC, "Out of memory");
      return FLUID_FAILED;
    }
    FLUID_MEMSET(synth->tuning[bank], 0, 128 * sizeof(fluid_tuning_t*));
  }

  old_tuning = synth->tuning[bank][prog];
  synth->tuning[bank][prog] = tuning;

  if (old_tuning) {
    if (!fluid_tuning_unref (old_tuning, 1))     /* -- unref old tuning */
    { /* Replace old tuning if present */
      fluid_synth_replace_tuning_LOCAL (synth, old_tuning, tuning, apply, FALSE);
    }
  }

  return FLUID_OK;
}

/* Replace a tuning with a new one in all MIDI channels.  new_tuning can be
 * NULL, in which case channels are reset to default equal tempered scale. */
static void
fluid_synth_replace_tuning_LOCAL (fluid_synth_t *synth, fluid_tuning_t *old_tuning,
                                  fluid_tuning_t *new_tuning, int apply, int unref_new)
{
//  fluid_event_queue_elem_t *event;
  fluid_channel_t *channel;
  int old_tuning_unref = 0;
  int i;

  for (i = 0; i < synth->midi_channels; i++)
  {
    channel = synth->channel[i];

    if (fluid_channel_get_tuning (channel) == old_tuning)
    {
      old_tuning_unref++;
      if (new_tuning) fluid_tuning_ref (new_tuning);    /* ++ ref new tuning for channel */
      fluid_channel_set_tuning (channel, new_tuning);

      if (apply) fluid_synth_update_voice_tuning_LOCAL (synth, channel);
    }
  }

  /* Send unref old tuning event if any unrefs */
  if (old_tuning && old_tuning_unref)
    fluid_tuning_unref (old_tuning, old_tuning_unref);
  if (!unref_new || !new_tuning) return;

  fluid_tuning_unref (new_tuning, 1);
}

/* Update voice tunings in realtime */
static void
fluid_synth_update_voice_tuning_LOCAL (fluid_synth_t *synth, fluid_channel_t *channel)
{
  fluid_voice_t *voice;
  int i;

  for (i = 0; i < synth->polyphony; i++)
  {
    voice = synth->voice[i];

    if (_ON (voice) && (voice->channel == channel))
    {
      fluid_voice_calculate_gen_pitch (voice);
      fluid_voice_update_param (voice, GEN_PITCH);
    }
  }
}

/**
 * Set the tuning of the entire MIDI note scale.
 * @param synth FluidSynth instance
 * @param bank Tuning bank number (0-127), not related to MIDI instrument bank
 * @param prog Tuning preset number (0-127), not related to MIDI instrument program
 * @param name Label name for this tuning
 * @param pitch Array of pitch values (length of 128, each value is number of
 *   cents, for example normally note 0 is 0.0, 1 is 100.0, 60 is 6000.0, etc).
 *   Pass NULL to create a well-tempered (normal) scale.
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 *
 * NOTE: Tuning is not applied in realtime to existing notes of the replaced
 * tuning (if any), use fluid_synth_activate_key_tuning() instead to specify
 * this behavior.
 */
int
fluid_synth_create_key_tuning(fluid_synth_t* synth, int bank, int prog,
                              const char* name, const double* pitch)
{
  return fluid_synth_activate_key_tuning (synth, bank, prog, name, pitch, FALSE);
}

/**
 * Set the tuning of the entire MIDI note scale.
 * @param synth FluidSynth instance
 * @param bank Tuning bank number (0-127), not related to MIDI instrument bank
 * @param prog Tuning preset number (0-127), not related to MIDI instrument program
 * @param name Label name for this tuning
 * @param pitch Array of pitch values (length of 128, each value is number of
 *   cents, for example normally note 0 is 0.0, 1 is 100.0, 60 is 6000.0, etc).
 *   Pass NULL to create a well-tempered (normal) scale.
 * @param apply TRUE to apply new tuning in realtime to existing notes which
 *   are using the replaced tuning (if any), FALSE otherwise
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 * @since 1.1.0
 */
int
fluid_synth_activate_key_tuning(fluid_synth_t* synth, int bank, int prog,
                                const char* name, const double* pitch, int apply)
{
  fluid_tuning_t* tuning;
  int retval = FLUID_OK;

  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (bank >= 0 && bank < 128, FLUID_FAILED);
  fluid_return_val_if_fail (prog >= 0 && prog < 128, FLUID_FAILED);
  fluid_return_val_if_fail (name != NULL, FLUID_FAILED);

  fluid_synth_api_enter(synth);

  tuning = new_fluid_tuning (name, bank, prog);

  if (tuning)
  {
    if (pitch) fluid_tuning_set_all (tuning, pitch);
    retval = fluid_synth_replace_tuning_LOCK (synth, tuning, bank, prog, apply);
    if (retval == FLUID_FAILED) fluid_tuning_unref (tuning, 1);
  }
  else retval = FLUID_FAILED;
  FLUID_API_RETURN(retval);
}

/**
 * Apply an octave tuning to every octave in the MIDI note scale.
 * @param synth FluidSynth instance
 * @param bank Tuning bank number (0-127), not related to MIDI instrument bank
 * @param prog Tuning preset number (0-127), not related to MIDI instrument program
 * @param name Label name for this tuning
 * @param pitch Array of pitch values (length of 12 for each note of an octave
 *   starting at note C, values are number of offset cents to add to the normal
 *   tuning amount)
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 *
 * NOTE: Tuning is not applied in realtime to existing notes of the replaced
 * tuning (if any), use fluid_synth_activate_octave_tuning() instead to specify
 * this behavior.
 */
int
fluid_synth_create_octave_tuning(fluid_synth_t* synth, int bank, int prog,
                                 const char* name, const double* pitch)
{
  return fluid_synth_activate_octave_tuning (synth, bank, prog, name, pitch, FALSE);
}

/**
 * Activate an octave tuning on every octave in the MIDI note scale.
 * @param synth FluidSynth instance
 * @param bank Tuning bank number (0-127), not related to MIDI instrument bank
 * @param prog Tuning preset number (0-127), not related to MIDI instrument program
 * @param name Label name for this tuning
 * @param pitch Array of pitch values (length of 12 for each note of an octave
 *   starting at note C, values are number of offset cents to add to the normal
 *   tuning amount)
 * @param apply TRUE to apply new tuning in realtime to existing notes which
 *   are using the replaced tuning (if any), FALSE otherwise
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 * @since 1.1.0
 */
int
fluid_synth_activate_octave_tuning(fluid_synth_t* synth, int bank, int prog,
                                   const char* name, const double* pitch, int apply)
{
  fluid_tuning_t* tuning;
  int retval = FLUID_OK;

  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (bank >= 0 && bank < 128, FLUID_FAILED);
  fluid_return_val_if_fail (prog >= 0 && prog < 128, FLUID_FAILED);
  fluid_return_val_if_fail (name != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (pitch != NULL, FLUID_FAILED);

  fluid_synth_api_enter(synth);
  tuning = new_fluid_tuning (name, bank, prog);

  if (tuning)
  {
    fluid_tuning_set_octave (tuning, pitch);
    retval = fluid_synth_replace_tuning_LOCK (synth, tuning, bank, prog, apply);
    if (retval == FLUID_FAILED) fluid_tuning_unref (tuning, 1);
  }
  else retval = FLUID_FAILED;

  FLUID_API_RETURN(retval);
}

/**
 * Set tuning values for one or more MIDI notes for an existing tuning.
 * @param synth FluidSynth instance
 * @param bank Tuning bank number (0-127), not related to MIDI instrument bank
 * @param prog Tuning preset number (0-127), not related to MIDI instrument program
 * @param len Number of MIDI notes to assign
 * @param key Array of MIDI key numbers (length of 'len', values 0-127)
 * @param pitch Array of pitch values (length of 'len', values are number of
 *   cents from MIDI note 0)
 * @param apply TRUE to apply tuning change in realtime to existing notes using
 *   the specified tuning, FALSE otherwise
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 *
 * NOTE: Prior to version 1.1.0 it was an error to specify a tuning that didn't
 * already exist.  Starting with 1.1.0, the default equal tempered scale will be
 * used as a basis, if no tuning exists for the given bank and prog.
 */
int
fluid_synth_tune_notes(fluid_synth_t* synth, int bank, int prog,
                       int len, const int *key, const double* pitch, int apply)
{
  fluid_tuning_t* old_tuning, *new_tuning;
  int retval = FLUID_OK;
  int i;

  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (bank >= 0 && bank < 128, FLUID_FAILED);
  fluid_return_val_if_fail (prog >= 0 && prog < 128, FLUID_FAILED);
  fluid_return_val_if_fail (len > 0, FLUID_FAILED);
  fluid_return_val_if_fail (key != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (pitch != NULL, FLUID_FAILED);
  
  fluid_synth_api_enter(synth);

  old_tuning = fluid_synth_get_tuning (synth, bank, prog);

  if (old_tuning)
    new_tuning = fluid_tuning_duplicate (old_tuning);
  else new_tuning = new_fluid_tuning ("Unnamed", bank, prog);

  if (new_tuning)
  {
    for (i = 0; i < len; i++)
      fluid_tuning_set_pitch (new_tuning, key[i], pitch[i]);

    retval = fluid_synth_replace_tuning_LOCK (synth, new_tuning, bank, prog, apply);
    if (retval == FLUID_FAILED) fluid_tuning_unref (new_tuning, 1);
  }
  else retval = FLUID_FAILED;

  FLUID_API_RETURN(retval);
}

/**
 * Select a tuning scale on a MIDI channel.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param bank Tuning bank number (0-127), not related to MIDI instrument bank
 * @param prog Tuning preset number (0-127), not related to MIDI instrument program
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 *
 * NOTE: This function does NOT activate tuning in realtime, use
 * fluid_synth_activate_tuning() instead to specify whether tuning change
 * should cause existing notes to update.
 *
 * NOTE: Prior to version 1.1.0 it was an error to select a tuning that didn't
 * already exist.  Starting with 1.1.0, a default equal tempered scale will be
 * created, if no tuning exists for the given bank and prog.
 */
int
fluid_synth_select_tuning(fluid_synth_t* synth, int chan, int bank, int prog)
{
  return fluid_synth_activate_tuning (synth, chan, bank, prog, FALSE);
}

/**
 * Activate a tuning scale on a MIDI channel.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param bank Tuning bank number (0-127), not related to MIDI instrument bank
 * @param prog Tuning preset number (0-127), not related to MIDI instrument program
 * @param apply TRUE to apply tuning change to active notes, FALSE otherwise
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 * @since 1.1.0
 *
 * NOTE: A default equal tempered scale will be created, if no tuning exists
 * on the given bank and prog.
 */
int
fluid_synth_activate_tuning(fluid_synth_t* synth, int chan, int bank, int prog,
                            int apply)
{
  //fluid_event_queue_elem_t *event;
  //fluid_event_queue_t *queue;
  fluid_tuning_t* tuning;
  int retval = FLUID_OK;

  //fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  //fluid_return_val_if_fail (chan >= 0 && chan < synth->midi_channels, FLUID_FAILED);
  fluid_return_val_if_fail (bank >= 0 && bank < 128, FLUID_FAILED);
  fluid_return_val_if_fail (prog >= 0 && prog < 128, FLUID_FAILED);

  FLUID_API_ENTRY_CHAN(FLUID_FAILED);

  tuning = fluid_synth_get_tuning (synth, bank, prog);

 /* If no tuning exists, create a new default tuning.  We do this, so that
  * it can be replaced later, if any changes are made. */
  if (!tuning)
  {
    tuning = new_fluid_tuning ("Unnamed", bank, prog);
    if (tuning) fluid_synth_replace_tuning_LOCK (synth, tuning, bank, prog, FALSE);
  }

  if (tuning) fluid_tuning_ref (tuning);  /* ++ ref for outside of lock */

  if (!tuning) 
    FLUID_API_RETURN(FLUID_FAILED);

  fluid_tuning_ref (tuning);    /* ++ ref new tuning for following function */
  retval = fluid_synth_set_tuning_LOCAL (synth, chan, tuning, apply);

  fluid_tuning_unref (tuning, 1);   /* -- unref for outside of lock */

  FLUID_API_RETURN(retval);
}

/* Local synthesis thread set tuning function (takes over tuning reference) */
static int
fluid_synth_set_tuning_LOCAL (fluid_synth_t *synth, int chan,
                              fluid_tuning_t *tuning, int apply)
{
  fluid_tuning_t *old_tuning;
  fluid_channel_t *channel;

  channel = synth->channel[chan];

  old_tuning = fluid_channel_get_tuning (channel);
  fluid_channel_set_tuning (channel, tuning);   /* !! Takes over callers reference */

  if (apply) fluid_synth_update_voice_tuning_LOCAL (synth, channel);

  /* Send unref old tuning event */
  if (old_tuning)
  {
    fluid_tuning_unref (old_tuning, 1);
  }


  return FLUID_OK;
}

/**
 * Clear tuning scale on a MIDI channel (set it to the default well-tempered scale).
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 *
 * NOTE: This function does NOT activate tuning change in realtime, use
 * fluid_synth_deactivate_tuning() instead to specify whether tuning change
 * should cause existing notes to update.
 */
int
fluid_synth_reset_tuning(fluid_synth_t* synth, int chan)
{
  return fluid_synth_deactivate_tuning (synth, chan, FALSE);
}

/**
 * Clear tuning scale on a MIDI channel (use default equal tempered scale).
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param apply TRUE to apply tuning change to active notes, FALSE otherwise
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 * @since 1.1.0
 */
int
fluid_synth_deactivate_tuning(fluid_synth_t* synth, int chan, int apply)
{
  int retval = FLUID_OK;

  FLUID_API_ENTRY_CHAN(FLUID_FAILED);

  retval = fluid_synth_set_tuning_LOCAL (synth, chan, NULL, apply);

  FLUID_API_RETURN(retval);
}

/**
 * Start tuning iteration.
 * @param synth FluidSynth instance
 */
void
fluid_synth_tuning_iteration_start(fluid_synth_t* synth)
{
  fluid_return_if_fail (synth != NULL);
  fluid_synth_api_enter(synth);
  fluid_private_set (synth->tuning_iter, FLUID_INT_TO_POINTER (0));
  fluid_synth_api_exit(synth);
}

/**
 * Advance to next tuning.
 * @param synth FluidSynth instance
 * @param bank Location to store MIDI bank number of next tuning scale
 * @param prog Location to store MIDI program number of next tuning scale
 * @return 1 if tuning iteration advanced, 0 if no more tunings
 */
int
fluid_synth_tuning_iteration_next(fluid_synth_t* synth, int* bank, int* prog)
{
  void *pval;
  int b = 0, p = 0;

  fluid_return_val_if_fail (synth != NULL, 0);
  fluid_return_val_if_fail (bank != NULL, 0);
  fluid_return_val_if_fail (prog != NULL, 0);
  fluid_synth_api_enter(synth);

  /* Current tuning iteration stored as: bank << 8 | program */
  pval = fluid_private_get (synth->tuning_iter);
  p = FLUID_POINTER_TO_INT (pval);
  b = (p >> 8) & 0xFF;
  p &= 0xFF;

  if (!synth->tuning)
  {
    FLUID_API_RETURN(0);
  }

  for (; b < 128; b++, p = 0)
  {
    if (synth->tuning[b] == NULL) continue;

    for (; p < 128; p++)
    {
      if (synth->tuning[b][p] == NULL) continue;

      *bank = b;
      *prog = p;

      if (p < 127) fluid_private_set (synth->tuning_iter,
                                      FLUID_INT_TO_POINTER (b << 8 | (p + 1)));
      else fluid_private_set (synth->tuning_iter,
                              FLUID_INT_TO_POINTER ((b + 1) << 8));

      FLUID_API_RETURN(1);
    }
  }

  FLUID_API_RETURN(0);
}

/**
 * Get the entire note tuning for a given MIDI bank and program.
 * @param synth FluidSynth instance
 * @param bank MIDI bank number of tuning
 * @param prog MIDI program number of tuning
 * @param name Location to store tuning name or NULL to ignore
 * @param len Maximum number of chars to store to 'name' (including NULL byte)
 * @param pitch Array to store tuning scale to or NULL to ignore (len of 128)
 * @return FLUID_OK if matching tuning was found, FLUID_FAILED otherwise
 */
int
fluid_synth_tuning_dump(fluid_synth_t* synth, int bank, int prog,
                        char* name, int len, double* pitch)
{
  fluid_tuning_t* tuning;

  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_synth_api_enter(synth);
  
  tuning = fluid_synth_get_tuning (synth, bank, prog);

  if (tuning)
  {
    if (name)
    {
      snprintf (name, len - 1, "%s", fluid_tuning_get_name (tuning));
      name[len - 1] = 0;  /* make sure the string is null terminated */
    }

    if (pitch)
      FLUID_MEMCPY (pitch, fluid_tuning_get_all (tuning), 128 * sizeof (double));
  }

  FLUID_API_RETURN(tuning ? FLUID_OK : FLUID_FAILED);
}

/**
 * Get settings assigned to a synth.
 * @param synth FluidSynth instance
 * @return FluidSynth settings which are assigned to the synth
 */
fluid_settings_t *
fluid_synth_get_settings(fluid_synth_t* synth)
{
  fluid_return_val_if_fail (synth != NULL, NULL);

  return synth->settings;
}

/**
 * Convenience function to set a string setting of a synth.
 * @param synth FluidSynth instance
 * @param name Name of setting parameter
 * @param str Value to assign to the setting
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_setstr(fluid_synth_t* synth, const char* name, const char* str)
{
  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (name != NULL, FLUID_FAILED);

  return fluid_settings_setstr(synth->settings, name, str);
}

/**
 * Convenience function to duplicate a string setting of a synth.
 * @param synth FluidSynth instance
 * @param name Name of setting parameter
 * @param str Location to store a pointer to the newly allocated string value
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 *
 * The returned string is owned by the caller and should be freed with free()
 * when finished with it.
 */
int
fluid_synth_dupstr(fluid_synth_t* synth, const char* name, char** str)
{
  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (name != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (str != NULL, FLUID_FAILED);

  return fluid_settings_dupstr(synth->settings, name, str);
}

/**
 * Convenience function to set a floating point setting of a synth.
 * @param synth FluidSynth instance
 * @param name Name of setting parameter
 * @param val Value to assign to the setting
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_setnum(fluid_synth_t* synth, const char* name, double val)
{
  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (name != NULL, FLUID_FAILED);

  return fluid_settings_setnum(synth->settings, name, val);
}

/**
 * Convenience function to get a floating point setting of a synth.
 * @param synth FluidSynth instance
 * @param name Name of setting parameter
 * @param val Location to store the current value of the setting
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_getnum(fluid_synth_t* synth, const char* name, double* val)
{
  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (name != NULL, FLUID_FAILED);

  return fluid_settings_getnum(synth->settings, name, val);
}

/**
 * Convenience function to set an integer setting of a synth.
 * @param synth FluidSynth instance
 * @param name Name of setting parameter
 * @param val Value to assign to the setting
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_setint(fluid_synth_t* synth, const char* name, int val)
{
  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (name != NULL, FLUID_FAILED);

  return fluid_settings_setint(synth->settings, name, val);
}

/**
 * Convenience function to get an integer setting of a synth.
 * @param synth FluidSynth instance
 * @param name Name of setting parameter
 * @param val Location to store the current value of the setting
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_getint(fluid_synth_t* synth, const char* name, int* val)
{
  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (name != NULL, FLUID_FAILED);

  return fluid_settings_getint(synth->settings, name, val);
}

/**
 * Set a SoundFont generator (effect) value on a MIDI channel in real-time.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param param SoundFont generator ID (#fluid_gen_type)
 * @param value Offset generator value to assign to the MIDI channel
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 *
 * Parameter numbers and ranges are described in the SoundFont 2.01
 * specification PDF, paragraph 8.1.3, page 48.  See #fluid_gen_type.
 */
int
fluid_synth_set_gen(fluid_synth_t* synth, int chan, int param, float value)
{
  fluid_return_val_if_fail (param >= 0 && param < GEN_LAST, FLUID_FAILED);
  FLUID_API_ENTRY_CHAN(FLUID_FAILED);

  fluid_synth_set_gen_LOCAL (synth, chan, param, value, FALSE);

  FLUID_API_RETURN(FLUID_OK);
}

/* Synthesis thread local set gen function */
static void
fluid_synth_set_gen_LOCAL (fluid_synth_t* synth, int chan, int param, float value,
                           int absolute)
{
  fluid_voice_t* voice;
  int i;

  fluid_channel_set_gen (synth->channel[chan], param, value, absolute);

  for (i = 0; i < synth->polyphony; i++) {
    voice = synth->voice[i];

    if (voice->chan == chan)
      fluid_voice_set_param (voice, param, value, absolute);
  }
}

/**
 * Set a SoundFont generator (effect) value on a MIDI channel in real-time.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param param SoundFont generator ID (#fluid_gen_type)
 * @param value Offset or absolute generator value to assign to the MIDI channel
 * @param absolute 0 to assign a relative value, non-zero to assign an absolute value
 * @param normalized 0 if value is specified in the native units of the generator,
 *   non-zero to take the value as a 0.0-1.0 range and apply it to the valid
 *   generator effect range (scaled and shifted as necessary).
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 * @since 1.1.0
 *
 * This function allows for setting all effect parameters in real time on a
 * MIDI channel.  Setting absolute to non-zero will cause the value to override
 * any generator values set in the instruments played on the MIDI channel.
 * See SoundFont 2.01 spec, paragraph 8.1.3, page 48 for details on SoundFont
 * generator parameters and valid ranges.
 */
int
fluid_synth_set_gen2(fluid_synth_t* synth, int chan, int param,
		     float value, int absolute, int normalized)
{
  float v;
  fluid_return_val_if_fail (param >= 0 && param < GEN_LAST, FLUID_FAILED);
  FLUID_API_ENTRY_CHAN(FLUID_FAILED);

  v = normalized ? fluid_gen_scale(param, value) : value;

  fluid_synth_set_gen_LOCAL (synth, chan, param, v, absolute);

  FLUID_API_RETURN(FLUID_OK);
}

/**
 * Get generator value assigned to a MIDI channel.
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param param SoundFont generator ID (#fluid_gen_type)
 * @return Current generator value assigned to MIDI channel
 */
float
fluid_synth_get_gen(fluid_synth_t* synth, int chan, int param)
{
  float result;
  fluid_return_val_if_fail (param >= 0 && param < GEN_LAST, FLUID_FAILED);
  FLUID_API_ENTRY_CHAN(FLUID_FAILED);

  result = fluid_channel_get_gen(synth->channel[chan], param);
  FLUID_API_RETURN(result);
}

/**
 * Assign a MIDI router to a synth.
 * @param synth FluidSynth instance
 * @param router MIDI router to assign to the synth
 *
 * NOTE: This should only be done once and prior to using the synth.
 */
void
fluid_synth_set_midi_router(fluid_synth_t* synth, fluid_midi_router_t* router)
{
  fluid_return_if_fail (synth != NULL);
  fluid_synth_api_enter(synth);

  synth->midi_router = router;
  fluid_synth_api_exit(synth);
};

/**
 * Handle MIDI event from MIDI router, used as a callback function.
 * @param data FluidSynth instance
 * @param event MIDI event to handle
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 */
int
fluid_synth_handle_midi_event(void* data, fluid_midi_event_t* event)
{
  fluid_synth_t* synth = (fluid_synth_t*) data;
  int type = fluid_midi_event_get_type(event);
  int chan = fluid_midi_event_get_channel(event);

  switch(type) {
      case NOTE_ON:
	return fluid_synth_noteon(synth, chan,
                                  fluid_midi_event_get_key(event),
                                  fluid_midi_event_get_velocity(event));

      case NOTE_OFF:
	return fluid_synth_noteoff(synth, chan, fluid_midi_event_get_key(event));

      case CONTROL_CHANGE:
	return fluid_synth_cc(synth, chan,
                              fluid_midi_event_get_control(event),
                              fluid_midi_event_get_value(event));

      case PROGRAM_CHANGE:
	return fluid_synth_program_change(synth, chan, fluid_midi_event_get_program(event));

      case CHANNEL_PRESSURE:
	return fluid_synth_channel_pressure(synth, chan, fluid_midi_event_get_program(event));

      case PITCH_BEND:
	return fluid_synth_pitch_bend(synth, chan, fluid_midi_event_get_pitch(event));

      case MIDI_SYSTEM_RESET:
	return fluid_synth_system_reset(synth);
      case MIDI_SYSEX:
        return fluid_synth_sysex (synth, event->paramptr, event->param1, NULL, NULL, NULL, FALSE);
  }
  return FLUID_FAILED;
}

/**
 * Create and start voices using a preset and a MIDI note on event.
 * @param synth FluidSynth instance
 * @param id Voice group ID to use (can be used with fluid_synth_stop()).
 * @param preset Preset to synthesize
 * @param audio_chan Unused currently, set to 0
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param key MIDI note number (0-127)
 * @param vel MIDI velocity number (1-127)
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 *
 * NOTE: Should only be called from within synthesis thread, which includes
 * SoundFont loader preset noteon method.
 */
int
fluid_synth_start(fluid_synth_t* synth, unsigned int id, fluid_preset_t* preset, 
		  int audio_chan, int chan, int key, int vel)
{
  int result;
  fluid_return_val_if_fail (preset != NULL, FLUID_FAILED);
  fluid_return_val_if_fail (key >= 0 && key <= 127, FLUID_FAILED);
  fluid_return_val_if_fail (vel >= 1 && vel <= 127, FLUID_FAILED);
  FLUID_API_ENTRY_CHAN(FLUID_FAILED);
  synth->storeid = id;
  result = fluid_preset_noteon (preset, synth, chan, key, vel);
  FLUID_API_RETURN(result);
}

/**
 * Stop notes for a given note event voice ID.
 * @param synth FluidSynth instance
 * @param id Voice note event ID
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 *
 * NOTE: In FluidSynth versions prior to 1.1.0 #FLUID_FAILED would be returned
 * if no matching voice note event ID was found.  Versions after 1.1.0 only
 * return #FLUID_FAILED if an error occurs.
 */
int
fluid_synth_stop(fluid_synth_t* synth, unsigned int id)
{
  int result;
  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_synth_api_enter(synth);
  fluid_synth_stop_LOCAL (synth, id);
  result = FLUID_OK;
  FLUID_API_RETURN(result);
}

/* Local synthesis thread variant of fluid_synth_stop */
static void
fluid_synth_stop_LOCAL (fluid_synth_t *synth, unsigned int id)
{
  fluid_voice_t* voice;
  int i;

  for (i = 0; i < synth->polyphony; i++) {
    voice = synth->voice[i];

    if (_ON(voice) && (fluid_voice_get_id (voice) == id))
      fluid_voice_noteoff(voice);
  }
}

/**
 * Offset the bank numbers of a loaded SoundFont.
 * @param synth FluidSynth instance
 * @param sfont_id ID of a loaded SoundFont
 * @param offset Bank offset value to apply to all instruments
 */
int
fluid_synth_set_bank_offset(fluid_synth_t* synth, int sfont_id, int offset)
{
  fluid_sfont_info_t *sfont_info;
  fluid_list_t *list;

  fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
  fluid_synth_api_enter(synth);
  
  for (list = synth->sfont_info; list; list = fluid_list_next(list)) {
    sfont_info = (fluid_sfont_info_t *)fluid_list_get (list);

    if (fluid_sfont_get_id (sfont_info->sfont) == (unsigned int)sfont_id)
    {
      sfont_info->bankofs = offset;
      break;
    }
  }

  if (!list)
  {
    FLUID_LOG (FLUID_ERR, "No SoundFont with id = %d", sfont_id);
    FLUID_API_RETURN(FLUID_FAILED);
  }

  FLUID_API_RETURN(FLUID_OK);
}

/**
 * Get bank offset of a loaded SoundFont.
 * @param synth FluidSynth instance
 * @param sfont_id ID of a loaded SoundFont
 * @return SoundFont bank offset value
 */
int
fluid_synth_get_bank_offset(fluid_synth_t* synth, int sfont_id)
{
  fluid_sfont_info_t *sfont_info;
  fluid_list_t *list;
  int offset = 0;

  fluid_return_val_if_fail (synth != NULL, 0);
  fluid_synth_api_enter(synth);

  for (list = synth->sfont_info; list; list = fluid_list_next(list)) {
    sfont_info = (fluid_sfont_info_t *)fluid_list_get (list);

    if (fluid_sfont_get_id (sfont_info->sfont) == (unsigned int)sfont_id)
    {
      offset = sfont_info->bankofs;
      break;
    }
  }

  if (!list)
  {
    FLUID_LOG (FLUID_ERR, "No SoundFont with id = %d", sfont_id);
    FLUID_API_RETURN(0);
  }

  FLUID_API_RETURN(offset);
}

void 
fluid_synth_api_enter(fluid_synth_t* synth)
{
  if (synth->use_mutex) {
    fluid_rec_mutex_lock(synth->mutex);
  }
  if (!synth->public_api_count) {
    fluid_synth_check_finished_voices(synth);
  }
  synth->public_api_count++;
}

void fluid_synth_api_exit(fluid_synth_t* synth)
{
  synth->public_api_count--;
  if (!synth->public_api_count) {
    fluid_rvoice_eventhandler_flush(synth->eventhandler);
  }

  if (synth->use_mutex) {
    fluid_rec_mutex_unlock(synth->mutex);
  }
  
}


/**
 * Set midi channel type 
 * @param synth FluidSynth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param type CHANNEL_TYPE_MELODIC, or CHANNEL_TYPE_DRUM
 * @return FLUID_OK on success, FLUID_FAILED otherwise
 * @since 1.1.4
 */
int fluid_synth_set_channel_type(fluid_synth_t* synth, int chan, int type)
{
  fluid_return_val_if_fail ((type >= CHANNEL_TYPE_MELODIC) && (type <= CHANNEL_TYPE_DRUM), FLUID_FAILED);
  FLUID_API_ENTRY_CHAN(FLUID_FAILED);
  
  synth->channel[chan]->channel_type = type;

  FLUID_API_RETURN(FLUID_OK);
}

