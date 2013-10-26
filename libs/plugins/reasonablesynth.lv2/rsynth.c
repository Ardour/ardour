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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // needed for M_PI
#endif

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#ifndef BUFFER_SIZE_SAMPLES
#define BUFFER_SIZE_SAMPLES 64
#endif

#ifndef MIN
#define MIN(A, B) ( (A) < (B) ? (A) : (B) )
#endif

/* internal MIDI event abstraction */
enum RMIDI_EV_TYPE {
  INVALID=0,
  NOTE_ON,
  NOTE_OFF,
  PROGRAM_CHANGE,
  CONTROL_CHANGE,
};

struct rmidi_event_t {
  enum RMIDI_EV_TYPE type;
  uint8_t channel; /**< the MIDI channel number 0-15 */
  union {
    struct {
      uint8_t note;
      uint8_t velocity;
    } tone;
    struct {
      uint8_t param;
      uint8_t value;
    } control;
  } d;
};

typedef struct {
  uint32_t tme[3]; // attack, decay, release times [settings:ms || internal:samples]
  float    vol[2]; // attack, sustain volume [0..1]
  uint32_t off[3]; // internal use (added attack,decay,release times)
} ADSRcfg;

typedef struct _RSSynthChannel {
  uint32_t  keycomp;
  uint32_t  adsr_cnt[128];
  float     adsr_amp[128];
  float     phase[128];      // various use, zero'ed on note-on
  int8_t    miditable[128];  // internal, note-on/off velocity
  ADSRcfg   adsr;
  void      (*synthesize) (struct _RSSynthChannel* sc,
      const uint8_t note, const float vol, const float pc,
      const size_t n_samples, float* left, float* right);
} RSSynthChannel;

typedef void (*SynthFunction) (RSSynthChannel* sc,
    const uint8_t note, const float vol, const float pc,
    const size_t n_samples, float* left, float* right);

typedef struct {
  uint32_t       boffset;
  float          buf [2][BUFFER_SIZE_SAMPLES];
  RSSynthChannel sc[16];
  float          freqs[128];
  float          kcgain;
  float          kcfilt;
  double         rate;
} RSSynthesizer;


/* initialize ADSR values
 *
 * @param rate sample-rate
 * @param a attack time in seconds
 * @param d decay time in seconds
 * @param r release time in seconds
 * @param avol attack gain [0..1]
 * @param svol sustain volume level [0..1]
 */
static void init_adsr(ADSRcfg *adsr, const double rate,
    const uint32_t a, const uint32_t d, const uint32_t r,
    const float avol, const float svol) {

  adsr->vol[0] = avol;
  adsr->vol[1] = svol;
  adsr->tme[0] = a * rate / 1000.0;
  adsr->tme[1] = d * rate / 1000.0;
  adsr->tme[2] = r * rate / 1000.0;

  assert(adsr->tme[0] > 32);
  assert(adsr->tme[1] > 32);
  assert(adsr->tme[2] > 32);
  assert(adsr->vol[0] >=0 && adsr->vol[1] <= 1.0);
  assert(adsr->vol[1] >=0 && adsr->vol[1] <= 1.0);

  adsr->off[0] = adsr->tme[0];
  adsr->off[1] = adsr->tme[1] + adsr->off[0];
  adsr->off[2] = adsr->tme[2] + adsr->off[1];
}

/* calculate per-sample, per-key envelope */
static inline float adsr_env(RSSynthChannel *sc, const uint8_t note) {

  if (sc->adsr_cnt[note] < sc->adsr.off[0]) {
    // attack
    const uint32_t p = ++sc->adsr_cnt[note];
    if (p == sc->adsr.tme[0]) {
      sc->adsr_amp[note] = sc->adsr.vol[0];
      return sc->adsr.vol[0];
    } else {
      const float d = sc->adsr.vol[0] - sc->adsr_amp[note];
      return sc->adsr_amp[note] + (p / (float) sc->adsr.tme[0]) * d;
    }
  }
  else if (sc->adsr_cnt[note] < sc->adsr.off[1]) {
    // decay
    const uint32_t p = ++sc->adsr_cnt[note] - sc->adsr.off[0];
    if (p == sc->adsr.tme[1]) {
      sc->adsr_amp[note] = sc->adsr.vol[1];
      return sc->adsr.vol[1];
    } else {
      const float d = sc->adsr.vol[1] - sc->adsr_amp[note];
      return sc->adsr_amp[note] + (p / (float) sc->adsr.tme[1]) * d;
    }
  }
  else if (sc->adsr_cnt[note] == sc->adsr.off[1]) {
    // sustain
    return sc->adsr.vol[1];
  }
  else if (sc->adsr_cnt[note] < sc->adsr.off[2]) {
    // release
    const uint32_t p = ++sc->adsr_cnt[note] - sc->adsr.off[1];
    if (p == sc->adsr.tme[2]) {
      sc->adsr_amp[note] = 0;
      return 0;
    } else {
      const float d = 0 - sc->adsr_amp[note];
      return sc->adsr_amp[note] + (p / (float) sc->adsr.tme[2]) * d;
    }
  }
  else {
    sc->adsr_cnt[note] = 0;
    return 0;
  }
}


/*****************************************************************************/
/* piano like sound w/slight stereo phase */
static void synthesize_sineP (RSSynthChannel* sc,
    const uint8_t note, const float vol, const float fq,
    const size_t n_samples, float* left, float* right) {

  size_t i;
  float phase = sc->phase[note];

  for (i=0; i < n_samples; ++i) {
    float env = adsr_env(sc, note);
    if (sc->adsr_cnt[note] == 0) break;
    const float amp = vol * env;

    left[i]  += amp * sinf(2.0 * M_PI * phase);
    left[i]  += .300 * amp * sinf(2.0 * M_PI * phase * 2.0);
    left[i]  += .150 * amp * sinf(2.0 * M_PI * phase * 3.0);
    left[i]  += .080 * amp * sinf(2.0 * M_PI * phase * 4.0);
  //left[i]  -= .007 * amp * sinf(2.0 * M_PI * phase * 5.0);
  //left[i]  += .010 * amp * sinf(2.0 * M_PI * phase * 6.0);
    left[i]  += .020 * amp * sinf(2.0 * M_PI * phase * 7.0);
    phase += fq;
    right[i] += amp * sinf(2.0 * M_PI * phase);
    right[i] += .300 * amp * sinf(2.0 * M_PI * phase * 2.0);
    right[i] += .150 * amp * sinf(2.0 * M_PI * phase * 3.0);
    right[i] -= .080 * amp * sinf(2.0 * M_PI * phase * 4.0);
  //right[i] += .007 * amp * sinf(2.0 * M_PI * phase * 5.0);
  //right[i] += .010 * amp * sinf(2.0 * M_PI * phase * 6.0);
    right[i] -= .020 * amp * sinf(2.0 * M_PI * phase * 7.0);
    if (phase > 1.0) phase -= 2.0;
  }
  sc->phase[note] = phase;
}

static const ADSRcfg piano_adsr = {{   5, 800,  100}, { 1.0,  0.0}, {0,0,0}};

/*****************************************************************************/


/* process note - move through ADSR states, count active keys,.. */
static void process_key (void *synth,
    const uint8_t chn, const uint8_t note,
    const size_t n_samples, float *left, float *right)
{
  RSSynthesizer*  rs = (RSSynthesizer*)synth;
  RSSynthChannel* sc = &rs->sc[chn];
  const int8_t vel = sc->miditable[note];
  const float vol = /* master_volume */ 0.25 * fabsf(vel) / 127.0;
  const float phase = sc->phase[note];

  if (phase == -10 && vel > 0) {
    // new note on
    assert(sc->adsr_cnt[note] == 0);
    sc->adsr_amp[note] = 0;
    sc->adsr_cnt[note] = 0;
    sc->phase[note] = 0;
    sc->keycomp++;
    //printf("[On] Now %d keys active on chn %d\n", sc->keycomp, chn);
  }
  else if (phase >= -1.0 && phase <= 1.0 && vel > 0) {
    // sustain note or re-start note while adsr in progress:
    if (sc->adsr_cnt[note] > sc->adsr.off[1]) {
      // x-fade to attack
      sc->adsr_amp[note] = adsr_env(sc, note);
      sc->adsr_cnt[note] = 0;
    }
  }
  else if (phase >= -1.0 && phase <= 1.0 && vel < 0) {
    // note off
    if (sc->adsr_cnt[note] <= sc->adsr.off[1]) {
      if (sc->adsr_cnt[note] != sc->adsr.off[1]) {
	// x-fade to release
	sc->adsr_amp[note] = adsr_env(sc, note);
      }
      sc->adsr_cnt[note] = sc->adsr.off[1] + 1;
    }
  }
  else {
    /* note-on + off in same cycle */
    sc->miditable[note] = 0;
    sc->adsr_cnt[note] = 0;
    sc->phase[note] = -10;
    return;
  }

  // synthesize actual sound
  sc->synthesize(sc, note, vol, rs->freqs[note], n_samples, left, right);

  if (sc->adsr_cnt[note] == 0) {
    //printf("Note %d,%d released\n", chn, note);
    sc->miditable[note] = 0;
    sc->adsr_amp[note] = 0;
    sc->phase[note] = -10;
    sc->keycomp--;
    //printf("[off] Now %d keys active on chn %d\n", sc->keycomp, chn);
  }
}

/* synthesize a BUFFER_SIZE_SAMPLES's of audio-data */
static void synth_fragment (void *synth, const size_t n_samples, float *left, float *right) {
  RSSynthesizer* rs = (RSSynthesizer*)synth;
  memset (left, 0, n_samples * sizeof(float));
  memset (right, 0, n_samples * sizeof(float));
  uint8_t keycomp = 0;
  int c,k;
  size_t i;

  for (c=0; c < 16; ++c) {
    for (k=0; k < 128; ++k) {
      if (rs->sc[c].miditable[k] == 0) continue;
      process_key(synth, c, k, n_samples, left, right);
    }
    keycomp += rs->sc[c].keycomp;
  }

#if 1 // key-compression
  float kctgt = 8.0 / (float)(keycomp + 7.0);
  if (kctgt < .5) kctgt = .5;
  if (kctgt > 1.0) kctgt = 1.0;
  const float _w = rs->kcfilt;
  for (i=0; i < n_samples; ++i) {
    rs->kcgain += _w * (kctgt - rs->kcgain);
    left[i]  *= rs->kcgain;
    right[i] *= rs->kcgain;
  }
  rs->kcgain += 1e-12;
#endif
}

static void synth_reset_channel(RSSynthChannel* sc) {
  int k;
  for (k=0; k < 128; ++k) {
    sc->adsr_cnt[k]  = 0;
    sc->adsr_amp[k]  = 0;
    sc->phase[k]     = -10;
    sc->miditable[k] = 0;
  }
  sc->keycomp = 0;
}

static void synth_reset(void *synth) {
  RSSynthesizer* rs = (RSSynthesizer*)synth;
  int c;
  for (c=0; c < 16; ++c) {
    synth_reset_channel(&(rs->sc[c]));
  }
  rs->kcgain = 0;
}

static void synth_load(RSSynthChannel *sc, const double rate,
    SynthFunction synthesize,
    ADSRcfg const * const adsr) {
  synth_reset_channel(sc);
  init_adsr(&sc->adsr, rate,
      adsr->tme[0], adsr->tme[1], adsr->tme[2],
      adsr->vol[0], adsr->vol[1]);
  sc->synthesize = synthesize;
}


/**
 * internal abstraction of MIDI data handling
 */
static void synth_process_midi_event(void *synth, struct rmidi_event_t *ev) {
  RSSynthesizer* rs = (RSSynthesizer*)synth;
  switch(ev->type) {
    case NOTE_ON:
      if (rs->sc[ev->channel].miditable[ev->d.tone.note] <= 0)
	rs->sc[ev->channel].miditable[ev->d.tone.note] = ev->d.tone.velocity;
      break;
    case NOTE_OFF:
      if (rs->sc[ev->channel].miditable[ev->d.tone.note] > 0)
	rs->sc[ev->channel].miditable[ev->d.tone.note] *= -1.0;
      break;
    case PROGRAM_CHANGE:
      break;
    case CONTROL_CHANGE:
      if (ev->d.control.param == 0x00 || ev->d.control.param == 0x20) {
	/*  0x00 and 0x20 are used for BANK select */
	break;
      } else
      if (ev->d.control.param == 121) {
	/* reset all controllers */
	break;
      } else
      if (ev->d.control.param == 120 || ev->d.control.param == 123) {
	/* Midi panic: 120: all sound off, 123: all notes off*/
	synth_reset_channel(&(rs->sc[ev->channel]));
	break;
      } else
      if (ev->d.control.param >= 120) {
	/* params 122-127 are reserved - skip them. */
	break;
      }
      break;
    default:
      break;
  }
}

/******************************************************************************
 * PUBLIC API (used by lv2.c)
 */

/**
 * align LV2 and internal synth buffers
 * call synth_fragment as often as needed for the given LV2 buffer size
 *
 * @param synth synth-handle
 * @param written samples written so far (offset in \ref out)
 * @param nframes total samples to synthesize and write to the \out buffer
 * @param out pointer to stereo output buffers
 * @return end of buffer (written + nframes)
 */
static uint32_t synth_sound (void *synth, uint32_t written, const uint32_t nframes, float **out) {
  RSSynthesizer* rs = (RSSynthesizer*)synth;

  while (written < nframes) {
    uint32_t nremain = nframes - written;

    if (rs->boffset >= BUFFER_SIZE_SAMPLES)  {
      rs->boffset = 0;
      synth_fragment(rs, BUFFER_SIZE_SAMPLES, rs->buf[0], rs->buf[1]);
    }

    uint32_t nread = MIN(nremain, (BUFFER_SIZE_SAMPLES - rs->boffset));

    memcpy(&out[0][written], &rs->buf[0][rs->boffset], nread*sizeof(float));
    memcpy(&out[1][written], &rs->buf[1][rs->boffset], nread*sizeof(float));

    written += nread;
    rs->boffset += nread;
  }
  return written;
}

/**
 * parse raw midi-data.
 *
 * @param synth synth-handle
 * @param data 8bit midi message
 * @param size number of bytes in the midi-message
 */
static void synth_parse_midi(void *synth, const uint8_t *data, const size_t size) {
  if (size < 2 || size > 3) return;
  // All messages need to be 3 bytes; except program-changes: 2bytes.
  if (size == 2 && (data[0] & 0xf0)  != 0xC0) return;

  struct rmidi_event_t ev;

  ev.channel = data[0]&0x0f;
  switch (data[0] & 0xf0) {
    case 0x80:
      ev.type=NOTE_OFF;
      ev.d.tone.note=data[1]&0x7f;
      ev.d.tone.velocity=data[2]&0x7f;
      break;
    case 0x90:
      ev.type=NOTE_ON;
      ev.d.tone.note=data[1]&0x7f;
      ev.d.tone.velocity=data[2]&0x7f;
      break;
    case 0xB0:
      ev.type=CONTROL_CHANGE;
      ev.d.control.param=data[1]&0x7f;
      ev.d.control.value=data[2]&0x7f;
      break;
    case 0xC0:
      ev.type=PROGRAM_CHANGE;
      ev.d.control.value=data[1]&0x7f;
      break;
    default:
      return;
  }
  synth_process_midi_event(synth, &ev);
}

/**
 * initialize the synth
 * This should be called after synth_alloc()
 * as soon as the sample-rate is known
 *
 * @param synth synth-handle
 * @param rate sample-rate
 */
static void synth_init(void *synth, double rate) {
  RSSynthesizer* rs = (RSSynthesizer*)synth;
  rs->rate = rate;
  rs->boffset = BUFFER_SIZE_SAMPLES;
  const float tuning = 440;
  int c,k;
  for (k=0; k < 128; k++) {
    rs->freqs[k] = (2.0 * tuning / 32.0f) * powf(2, (k - 9.0) / 12.0) / rate;
    assert(rs->freqs[k] < M_PI/2); // otherwise spatialization may phase out..
  }
  rs->kcfilt = 12.0 / rate;
  synth_reset(synth);

  for (c=0; c < 16; c++) {
    synth_load(&rs->sc[c], rate, &synthesize_sineP, &piano_adsr);
  }
}

/**
 * Allocate data-structure, create a handle for all other synth_* functions.
 *
 * This data should be freeded with \ref synth_free when the synth is no
 * longer needed.
 *
 * The synth can only be used after calling \rev synth_init as well.
 *
 * @return synth-handle
 */
static void * synth_alloc(void) {
  return calloc(1, sizeof(RSSynthesizer));
}

/**
 * release synth data structure
 * @param synth synth-handle
 */
static void synth_free(void *synth) {
  free(synth);
}
/* vi:set ts=8 sts=2 sw=2: */
