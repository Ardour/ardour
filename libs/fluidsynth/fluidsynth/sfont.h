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

#ifndef _FLUIDSYNTH_SFONT_H
#define _FLUIDSYNTH_SFONT_H

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file sfont.h
 * @brief SoundFont plugins
 *
 * It is possible to add new SoundFont loaders to the
 * synthesizer. The API uses a couple of "interfaces" (structures
 * with callback functions): #fluid_sfloader_t, #fluid_sfont_t, and
 * #fluid_preset_t.  This API allows for virtual SoundFont files to be loaded
 * and synthesized, which may not actually be SoundFont files, as long as they
 * can be represented by the SoundFont synthesis model.
 *
 * To add a new SoundFont loader to the synthesizer, call
 * fluid_synth_add_sfloader() and pass a pointer to an
 * fluid_sfloader_t structure. The important callback function in
 * this structure is "load", which should try to load a file and
 * returns a #fluid_sfont_t structure, or NULL if it fails.
 *
 * The #fluid_sfont_t structure contains a callback to obtain the
 * name of the SoundFont. It contains two functions to iterate
 * though the contained presets, and one function to obtain a
 * preset corresponding to a bank and preset number. This
 * function should return a #fluid_preset_t structure.
 *
 * The #fluid_preset_t structure contains some functions to obtain
 * information from the preset (name, bank, number). The most
 * important callback is the noteon function. The noteon function
 * should call fluid_synth_alloc_voice() for every sample that has
 * to be played. fluid_synth_alloc_voice() expects a pointer to a
 * #fluid_sample_t structure and returns a pointer to the opaque
 * #fluid_voice_t structure. To set or increment the values of a
 * generator, use fluid_voice_gen_set() or fluid_voice_gen_incr(). When you are
 * finished initializing the voice call fluid_voice_start() to
 * start playing the synthesis voice.
 */

/**
 * Some notification enums for presets and samples.
 */
enum {
  FLUID_PRESET_SELECTED,                /**< Preset selected notify */
  FLUID_PRESET_UNSELECTED,              /**< Preset unselected notify */
  FLUID_SAMPLE_DONE                     /**< Sample no longer needed notify */
};


/**
 * SoundFont loader structure.
 */
struct _fluid_sfloader_t {
  void* data;           /**< User defined data pointer */

  /**
   * The free method should free the memory allocated for the loader in
   * addition to any private data.
   * @param loader SoundFont loader
   * @return Should return 0 if no error occured, non-zero otherwise
   */
  int (*free)(fluid_sfloader_t* loader);

  /**
   * Method to load an instrument file (does not actually need to be a real file name,
   * could be another type of string identifier that the \a loader understands).
   * @param loader SoundFont loader
   * @param filename File name or other string identifier
   * @return The loaded instrument file (SoundFont) or NULL if an error occured.
   */
  fluid_sfont_t* (*load)(fluid_sfloader_t* loader, const char* filename);
};

/**
 * Virtual SoundFont instance structure.
 */
struct _fluid_sfont_t {
  void* data;           /**< User defined data */
  unsigned int id;      /**< SoundFont ID */

  /**
   * Method to free a virtual SoundFont bank.
   * @param sfont Virtual SoundFont to free.
   * @return Should return 0 when it was able to free all resources or non-zero
   *   if some of the samples could not be freed because they are still in use,
   *   in which case the free will be tried again later, until success.
   */
  int (*free)(fluid_sfont_t* sfont);

  /**
   * Method to return the name of a virtual SoundFont.
   * @param sfont Virtual SoundFont
   * @return The name of the virtual SoundFont.
   */
  char* (*get_name)(fluid_sfont_t* sfont);

  /**
   * Get a virtual SoundFont preset by bank and program numbers.
   * @param sfont Virtual SoundFont
   * @param bank MIDI bank number (0-16384)
   * @param prenum MIDI preset number (0-127)
   * @return Should return an allocated virtual preset or NULL if it could not
   *   be found.
   */
  fluid_preset_t* (*get_preset)(fluid_sfont_t* sfont, unsigned int bank, unsigned int prenum);

  /**
   * Start virtual SoundFont preset iteration method.
   * @param sfont Virtual SoundFont
   *
   * Starts/re-starts virtual preset iteration in a SoundFont.
   */
  void (*iteration_start)(fluid_sfont_t* sfont);

  /**
   * Virtual SoundFont preset iteration function.
   * @param sfont Virtual SoundFont
   * @param preset Caller supplied preset to fill in with current preset information
   * @return 0 when no more presets are available, 1 otherwise
   *
   * Should store preset information to the caller supplied \a preset structure
   * and advance the internal iteration state to the next preset for subsequent
   * calls.
   */
  int (*iteration_next)(fluid_sfont_t* sfont, fluid_preset_t* preset);
};

#define fluid_sfont_get_id(_sf) ((_sf)->id)

/**
 * Virtual SoundFont preset.
 */
struct _fluid_preset_t {
  void* data;                                   /**< User supplied data */
  fluid_sfont_t* sfont;                         /**< Parent virtual SoundFont */

  /**
   * Method to free a virtual SoundFont preset.
   * @param preset Virtual SoundFont preset
   * @return Should return 0
   */
  int (*free)(fluid_preset_t* preset);

  /**
   * Method to get a virtual SoundFont preset name.
   * @param preset Virtual SoundFont preset
   * @return Should return the name of the preset.  The returned string must be
   *   valid for the duration of the virtual preset (or the duration of the
   *   SoundFont, in the case of preset iteration).
   */
  char* (*get_name)(fluid_preset_t* preset);

  /**
   * Method to get a virtual SoundFont preset MIDI bank number.
   * @param preset Virtual SoundFont preset
   * @param return The bank number of the preset
   */
  int (*get_banknum)(fluid_preset_t* preset);

  /**
   * Method to get a virtual SoundFont preset MIDI program number.
   * @param preset Virtual SoundFont preset
   * @param return The program number of the preset
   */
  int (*get_num)(fluid_preset_t* preset);

  /**
   * Method to handle a noteon event (synthesize the instrument).
   * @param preset Virtual SoundFont preset
   * @param synth Synthesizer instance
   * @param chan MIDI channel number of the note on event
   * @param key MIDI note number (0-127)
   * @param vel MIDI velocity (0-127)
   * @return #FLUID_OK on success (0) or #FLUID_FAILED (-1) otherwise
   *
   * This method may be called from within synthesis context and therefore
   * should be as efficient as possible and not perform any operations considered
   * bad for realtime audio output (memory allocations and other OS calls).
   *
   * Call fluid_synth_alloc_voice() for every sample that has
   * to be played. fluid_synth_alloc_voice() expects a pointer to a
   * #fluid_sample_t structure and returns a pointer to the opaque
   * #fluid_voice_t structure. To set or increment the values of a
   * generator, use fluid_voice_gen_set() or fluid_voice_gen_incr(). When you are
   * finished initializing the voice call fluid_voice_start() to
   * start playing the synthesis voice.  Starting with FluidSynth 1.1.0 all voices
   * created will be started at the same time.
   */
  int (*noteon)(fluid_preset_t* preset, fluid_synth_t* synth, int chan, int key, int vel);

  /**
   * Virtual SoundFont preset notify method.
   * @param preset Virtual SoundFont preset
   * @param reason #FLUID_PRESET_SELECTED or #FLUID_PRESET_UNSELECTED
   * @param chan MIDI channel number
   * @return Should return #FLUID_OK
   *
   * Implement this optional method if the preset needs to be notified about
   * preset select and unselect events.
   *
   * This method may be called from within synthesis context and therefore
   * should be as efficient as possible and not perform any operations considered
   * bad for realtime audio output (memory allocations and other OS calls).
   */
  int (*notify)(fluid_preset_t* preset, int reason, int chan);
};

/**
 * Virtual SoundFont sample.
 */
struct _fluid_sample_t
{
  char name[21];                /**< Sample name */
  unsigned int start;           /**< Start index */
  unsigned int end;	        /**< End index, index of last valid sample point (contrary to SF spec) */
  unsigned int loopstart;       /**< Loop start index */
  unsigned int loopend;         /**< Loop end index, first point following the loop (superimposed on loopstart) */
  unsigned int samplerate;      /**< Sample rate */
  int origpitch;                /**< Original pitch (MIDI note number, 0-127) */
  int pitchadj;                 /**< Fine pitch adjustment (+/- 99 cents) */
  int sampletype;               /**< Values: #FLUID_SAMPLETYPE_MONO, FLUID_SAMPLETYPE_RIGHT, FLUID_SAMPLETYPE_LEFT, FLUID_SAMPLETYPE_ROM */
  int valid;                    /**< Should be TRUE if sample data is valid, FALSE otherwise (in which case it will not be synthesized) */
  short* data;                  /**< Pointer to the sample's data */

  int amplitude_that_reaches_noise_floor_is_valid;      /**< Indicates if \a amplitude_that_reaches_noise_floor is valid (TRUE), set to FALSE initially to calculate. */
  double amplitude_that_reaches_noise_floor;            /**< The amplitude at which the sample's loop will be below the noise floor.  For voice off optimization, calculated automatically. */

  unsigned int refcount;        /**< Count of voices using this sample (use #fluid_sample_refcount to access this field) */

  /**
   * Implement this function to receive notification when sample is no longer used.
   * @param sample Virtual SoundFont sample
   * @param reason #FLUID_SAMPLE_DONE only currently
   * @return Should return #FLUID_OK
   */
  int (*notify)(fluid_sample_t* sample, int reason);

  void* userdata;       /**< User defined data */
};


#define fluid_sample_refcount(_sample) ((_sample)->refcount)    /**< Get the reference count of a sample.  Should only be called from within synthesis context (noteon method for example) */


#define FLUID_SAMPLETYPE_MONO	1       /**< Flag for #fluid_sample_t \a sampletype field for mono samples */
#define FLUID_SAMPLETYPE_RIGHT	2       /**< Flag for #fluid_sample_t \a sampletype field for right samples of a stereo pair */
#define FLUID_SAMPLETYPE_LEFT	4       /**< Flag for #fluid_sample_t \a sampletype field for left samples of a stereo pair */
#define FLUID_SAMPLETYPE_LINKED	8       /**< Flag for #fluid_sample_t \a sampletype field, not used currently */
#define FLUID_SAMPLETYPE_ROM	0x8000  /**< Flag for #fluid_sample_t \a sampletype field, ROM sample, causes sample to be ignored */



#ifdef __cplusplus
}
#endif

#endif /* _FLUIDSYNTH_SFONT_H */
