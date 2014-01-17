/* reasonable simple synth
 *
 * Copyright (C) 2013 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

/* LV2 */
#include "lv2/lv2plug.in/ns/lv2core/lv2.h"
#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"
#include "lv2/lv2plug.in/ns/ext/midi/midi.h"

#define RSY_URI "https://community.ardour.org/node/7596"

/* the synth interface */
static void *   synth_alloc      (void);
static void     synth_init       (void *, double rate);
static void     synth_free       (void *);
static void     synth_parse_midi (void *, const uint8_t *data, const size_t size);
static uint32_t synth_sound      (void *, uint32_t written, uint32_t nframes, float **out);

#include "rsynth.c"

typedef enum {
  RSY_MIDIIN = 0,
  RSY_OUTL,
  RSY_OUTR
} PortIndex;

typedef struct {
  const LV2_Atom_Sequence* midiin;
  float* outL;
  float* outR;

  LV2_URID_Map* map;
  LV2_URID midi_MidiEvent;

  double SampleRateD;
  void *synth;
  bool xmas;
} RSynth;

/* main LV2 */

static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               bundle_path,
            const LV2_Feature* const* features)
{
  (void) descriptor; /* unused variable */
  (void) bundle_path; /* unused variable */

  if (rate < 8000) {
    fprintf(stderr, "RSynth.lv2 error: unsupported sample-rate (must be > 8k)\n");
    return NULL;
  }
  RSynth* self = (RSynth*)calloc(1, sizeof(RSynth));
  if(!self) {
    return NULL;
  }

  self->SampleRateD = rate;

  int i;
  for (i=0; features[i]; ++i) {
    if (!strcmp(features[i]->URI, LV2_URID__map)) {
      self->map = (LV2_URID_Map*)features[i]->data;
    }
  }

  if (!self->map) {
    fprintf(stderr, "RSynth.lv2 error: Host does not support urid:map\n");
    free(self);
    return NULL;
  }

  self->midi_MidiEvent = self->map->map(self->map->handle, LV2_MIDI__MidiEvent);

  self->synth = synth_alloc();
  synth_init(self->synth, rate);


  struct tm date;
  time_t now;
  time(&now);
  localtime_r(&now, &date);
  if (getenv("ITSXMAS") || (date.tm_mon == 11 /*dec*/ && date.tm_mday == 25)) {
    printf("reasonable synth.lv2 says: happy holidays!\n");
    self->xmas = true;
  }

  return (LV2_Handle)self;
}

static void
connect_port(LV2_Handle handle,
             uint32_t   port,
             void*      data)
{
  RSynth* self = (RSynth*)handle;

  switch ((PortIndex)port) {
    case RSY_MIDIIN:
      self->midiin = (const LV2_Atom_Sequence*)data;
      break;
    case RSY_OUTL:
      self->outL = (float*)data;
      break;
    case RSY_OUTR:
      self->outR = (float*)data;
      break;
  }
}

static void
run(LV2_Handle handle, uint32_t n_samples)
{
  RSynth* self = (RSynth*)handle;
  float* audio[2];

  audio[0] = self->outL;
  audio[1] = self->outR;

  uint32_t written = 0;

  /* Process incoming MIDI events */
  if (self->midiin) {
    LV2_Atom_Event const* ev = (LV2_Atom_Event const*)((&(self->midiin)->body) + 1); // lv2_atom_sequence_begin
    while( // !lv2_atom_sequence_is_end
        (const uint8_t*)ev < ((const uint8_t*) &(self->midiin)->body + (self->midiin)->atom.size)
        )
    {
      if (ev->body.type == self->midi_MidiEvent) {
        if (written + BUFFER_SIZE_SAMPLES < ev->time.frames
            && ev->time.frames < n_samples) {
          /* first synthesize sound up until the message timestamp */
          written = synth_sound(self->synth, written, ev->time.frames, audio);
        }
        /* send midi message to synth */
        if (self->xmas) {
          synth_parse_xmas(self->synth, (const uint8_t*)(ev+1), ev->body.size);
        } else {
          synth_parse_midi(self->synth, (const uint8_t*)(ev+1), ev->body.size);
        }
      }
      ev = (LV2_Atom_Event const*) // lv2_atom_sequence_next()
        ((const uint8_t*)ev + sizeof(LV2_Atom_Event) + ((ev->body.size + 7) & ~7));
    }
  }

  /* synthesize [remaining] sound */
  synth_sound(self->synth, written, n_samples, audio);
}

static void
cleanup(LV2_Handle handle)
{
  RSynth* self = (RSynth*)handle;
  synth_free(self->synth);
  free(handle);
}

static const void*
extension_data(const char* uri)
{
  (void) uri; /* unused variable */
  return NULL;
}

static const LV2_Descriptor descriptor = {
  RSY_URI,
  instantiate,
  connect_port,
  NULL,
  run,
  NULL,
  cleanup,
  extension_data
};

#if defined(COMPILER_MSVC)
__declspec(dllexport)
#else
__attribute__ ((visibility ("default")))
#endif
const LV2_Descriptor*
lv2_descriptor(uint32_t idx)
{
  switch (idx) {
  case 0:
    return &descriptor;
  default:
    return NULL;
  }
}

/* vi:set ts=8 sts=2 sw=2 et: */
