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

#ifndef _FLUIDSYNTH_MOD_H
#define _FLUIDSYNTH_MOD_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file mod.h
 * @brief SoundFont modulator functions and constants.
 */

#define FLUID_NUM_MOD           64      /**< Maximum number of modulators in a voice */

/**
 * Modulator structure.  See SoundFont 2.04 PDF section 8.2.
 */
struct _fluid_mod_t
{
  unsigned char dest;           /**< Destination generator to control */
  unsigned char src1;           /**< Source controller 1 */
  unsigned char flags1;         /**< Source controller 1 flags */
  unsigned char src2;           /**< Source controller 2 */
  unsigned char flags2;         /**< Source controller 2 flags */
  double amount;                /**< Multiplier amount */
  /* The 'next' field allows to link modulators into a list.  It is
   * not used in fluid_voice.c, there each voice allocates memory for a
   * fixed number of modulators.  Since there may be a huge number of
   * different zones, this is more efficient.
   */
  fluid_mod_t * next;
};

/**
 * Flags defining the polarity, mapping function and type of a modulator source.
 * Compare with SoundFont 2.04 PDF section 8.2.
 *
 * Note: Bit values do not correspond to the SoundFont spec!  Also note that
 * #FLUID_MOD_GC and #FLUID_MOD_CC are in the flags field instead of the source field.
 */
enum fluid_mod_flags
{
  FLUID_MOD_POSITIVE = 0,       /**< Mapping function is positive */
  FLUID_MOD_NEGATIVE = 1,       /**< Mapping function is negative */
  FLUID_MOD_UNIPOLAR = 0,       /**< Mapping function is unipolar */
  FLUID_MOD_BIPOLAR = 2,        /**< Mapping function is bipolar */
  FLUID_MOD_LINEAR = 0,         /**< Linear mapping function */
  FLUID_MOD_CONCAVE = 4,        /**< Concave mapping function */
  FLUID_MOD_CONVEX = 8,         /**< Convex mapping function */
  FLUID_MOD_SWITCH = 12,        /**< Switch (on/off) mapping function */
  FLUID_MOD_GC = 0,             /**< General controller source type (#fluid_mod_src) */
  FLUID_MOD_CC = 16             /**< MIDI CC controller (source will be a MIDI CC number) */
};

/**
 * General controller (if #FLUID_MOD_GC in flags).  This
 * corresponds to SoundFont 2.04 PDF section 8.2.1
 */
enum fluid_mod_src
{
  FLUID_MOD_NONE = 0,                   /**< No source controller */
  FLUID_MOD_VELOCITY = 2,               /**< MIDI note-on velocity */
  FLUID_MOD_KEY = 3,                    /**< MIDI note-on note number */
  FLUID_MOD_KEYPRESSURE = 10,           /**< MIDI key pressure */
  FLUID_MOD_CHANNELPRESSURE = 13,       /**< MIDI channel pressure */
  FLUID_MOD_PITCHWHEEL = 14,            /**< Pitch wheel */
  FLUID_MOD_PITCHWHEELSENS = 16         /**< Pitch wheel sensitivity */
};

FLUIDSYNTH_API fluid_mod_t* fluid_mod_new(void);
FLUIDSYNTH_API void fluid_mod_delete(fluid_mod_t * mod);

FLUIDSYNTH_API void fluid_mod_set_source1(fluid_mod_t* mod, int src, int flags); 
FLUIDSYNTH_API void fluid_mod_set_source2(fluid_mod_t* mod, int src, int flags); 
FLUIDSYNTH_API void fluid_mod_set_dest(fluid_mod_t* mod, int dst); 
FLUIDSYNTH_API void fluid_mod_set_amount(fluid_mod_t* mod, double amount); 

FLUIDSYNTH_API int fluid_mod_get_source1(fluid_mod_t* mod);
FLUIDSYNTH_API int fluid_mod_get_flags1(fluid_mod_t* mod);
FLUIDSYNTH_API int fluid_mod_get_source2(fluid_mod_t* mod);
FLUIDSYNTH_API int fluid_mod_get_flags2(fluid_mod_t* mod);
FLUIDSYNTH_API int fluid_mod_get_dest(fluid_mod_t* mod);
FLUIDSYNTH_API double fluid_mod_get_amount(fluid_mod_t* mod);

FLUIDSYNTH_API int fluid_mod_test_identity(fluid_mod_t * mod1, fluid_mod_t * mod2);


#ifdef __cplusplus
}
#endif
#endif /* _FLUIDSYNTH_MOD_H */

