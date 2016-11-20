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

#ifndef _FLUIDSYNTH_VOICE_H
#define _FLUIDSYNTH_VOICE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file voice.h
 * @brief Synthesis voice manipulation functions.
 *
 * The interface to the synthesizer's voices.
 * Examples on using them can be found in fluid_defsfont.c.
 * Most of these functions should only be called from within synthesis context,
 * such as the SoundFont loader's noteon method.
 */


FLUIDSYNTH_API void fluid_voice_update_param(fluid_voice_t* voice, int gen); 

/**
 * Enum used with fluid_voice_add_mod() to specify how to handle duplicate modulators.
 */
enum fluid_voice_add_mod {
  FLUID_VOICE_OVERWRITE,        /**< Overwrite any existing matching modulator */
  FLUID_VOICE_ADD,              /**< Add (sum) modulator amounts */
  FLUID_VOICE_DEFAULT           /**< For default modulators only, no need to check for duplicates */
};

FLUIDSYNTH_API void fluid_voice_add_mod(fluid_voice_t* voice, fluid_mod_t* mod, int mode);
FLUIDSYNTH_API void fluid_voice_gen_set(fluid_voice_t* voice, int gen, float val);
FLUIDSYNTH_API float fluid_voice_gen_get(fluid_voice_t* voice, int gen);
FLUIDSYNTH_API void fluid_voice_gen_incr(fluid_voice_t* voice, int gen, float val);

FLUIDSYNTH_API unsigned int fluid_voice_get_id(fluid_voice_t* voice);
FLUIDSYNTH_API int fluid_voice_is_playing(fluid_voice_t* voice);
FLUIDSYNTH_API int fluid_voice_optimize_sample(fluid_sample_t* s);       
    

#ifdef __cplusplus
}
#endif
#endif /* _FLUIDSYNTH_VOICE_H */

