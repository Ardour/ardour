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


#ifndef _FLUID_RVOICE_MIXER_H
#define _FLUID_RVOICE_MIXER_H

#include "fluidsynth_priv.h"
#include "fluid_rvoice.h"
//#include "fluid_ladspa.h"

typedef struct _fluid_rvoice_mixer_t fluid_rvoice_mixer_t;

#define FLUID_MIXER_MAX_BUFFERS_DEFAULT (8192/FLUID_BUFSIZE) 


void fluid_rvoice_mixer_set_finished_voices_callback(
  fluid_rvoice_mixer_t* mixer,
  void (*func)(void*, fluid_rvoice_t*),
  void* userdata);


int fluid_rvoice_mixer_render(fluid_rvoice_mixer_t* mixer, int blockcount);
int fluid_rvoice_mixer_get_bufs(fluid_rvoice_mixer_t* mixer, 
				  fluid_real_t*** left, fluid_real_t*** right);

fluid_rvoice_mixer_t* new_fluid_rvoice_mixer(int buf_count, int fx_buf_count, 
					     fluid_real_t sample_rate);

void delete_fluid_rvoice_mixer(fluid_rvoice_mixer_t*);

void fluid_rvoice_mixer_set_samplerate(fluid_rvoice_mixer_t* mixer, fluid_real_t samplerate);
void fluid_rvoice_mixer_set_reverb_enabled(fluid_rvoice_mixer_t* mixer, int on);
void fluid_rvoice_mixer_set_chorus_enabled(fluid_rvoice_mixer_t* mixer, int on);
void fluid_rvoice_mixer_set_mix_fx(fluid_rvoice_mixer_t* mixer, int on);
int fluid_rvoice_mixer_set_polyphony(fluid_rvoice_mixer_t* handler, int value);
int fluid_rvoice_mixer_add_voice(fluid_rvoice_mixer_t* mixer, fluid_rvoice_t* voice);
void fluid_rvoice_mixer_set_chorus_params(fluid_rvoice_mixer_t* mixer, int set, 
				         int nr, double level, double speed, 
				         double depth_ms, int type);
void fluid_rvoice_mixer_set_reverb_params(fluid_rvoice_mixer_t* mixer, int set, 
					 double roomsize, double damping, 
					 double width, double level);
void fluid_rvoice_mixer_reset_fx(fluid_rvoice_mixer_t* mixer);
void fluid_rvoice_mixer_reset_reverb(fluid_rvoice_mixer_t* mixer);
void fluid_rvoice_mixer_reset_chorus(fluid_rvoice_mixer_t* mixer);

void fluid_rvoice_mixer_set_threads(fluid_rvoice_mixer_t* mixer, int thread_count, 
				    int prio_level);
				    
#ifdef LADSPA				    
void fluid_rvoice_mixer_set_ladspa(fluid_rvoice_mixer_t* mixer, 
				   fluid_LADSPA_FxUnit_t* ladspa);
#endif

#endif

