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

#include "fluid_chan.h"
#include "fluid_mod.h"
#include "fluid_synth.h"
#include "fluid_sfont.h"

/* Field shift amounts for sfont_bank_prog bit field integer */
#define PROG_SHIFTVAL   0
#define BANK_SHIFTVAL   8
#define SFONT_SHIFTVAL  22

/* Field mask values for sfont_bank_prog bit field integer */
#define PROG_MASKVAL    0x000000FF      /* Bit 7 is used to indicate unset state */
#define BANK_MASKVAL    0x003FFF00
#define BANKLSB_MASKVAL 0x00007F00
#define BANKMSB_MASKVAL 0x003F8000
#define SFONT_MASKVAL   0xFFC00000


static void fluid_channel_init(fluid_channel_t* chan);


fluid_channel_t*
new_fluid_channel(fluid_synth_t* synth, int num)
{
  fluid_channel_t* chan;

  chan = FLUID_NEW(fluid_channel_t);
  if (chan == NULL) {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }

  chan->synth = synth;
  chan->channum = num;
  chan->preset = NULL;
  chan->tuning = NULL;

  fluid_channel_init(chan);
  fluid_channel_init_ctrl(chan, 0);

  return chan;
}

static void
fluid_channel_init(fluid_channel_t* chan)
{
  fluid_preset_t *newpreset;
  int prognum, banknum;

  chan->sostenuto_orderid = 0;

  chan->channel_type = (chan->channum == 9) ? CHANNEL_TYPE_DRUM : CHANNEL_TYPE_MELODIC;
  prognum = 0;
  banknum = (chan->channel_type == CHANNEL_TYPE_DRUM) ? DRUM_INST_BANK : 0;

  chan->sfont_bank_prog = 0 << SFONT_SHIFTVAL | banknum << BANK_SHIFTVAL
    | prognum << PROG_SHIFTVAL;

  newpreset = fluid_synth_find_preset(chan->synth, banknum, prognum);
  fluid_channel_set_preset(chan, newpreset);

  chan->interp_method = FLUID_INTERP_DEFAULT;
  chan->tuning_bank = 0;
  chan->tuning_prog = 0;
  chan->nrpn_select = 0;
  chan->nrpn_active = 0;

  if (chan->tuning)
  {
    fluid_tuning_unref (chan->tuning, 1);
    chan->tuning = NULL;
  }
}

/*
  @param is_all_ctrl_off if nonzero, only resets some controllers, according to 
  http://www.midi.org/techspecs/rp15.php 
*/
void
fluid_channel_init_ctrl(fluid_channel_t* chan, int is_all_ctrl_off)
{
  int i;

  chan->key_pressure = 0;
  chan->channel_pressure = 0;
  chan->pitch_bend = 0x2000; /* Range is 0x4000, pitch bend wheel starts in centered position */

  for (i = 0; i < GEN_LAST; i++) {
    chan->gen[i] = 0.0f;
    chan->gen_abs[i] = 0;
  }

  if (is_all_ctrl_off) {
    for (i = 0; i < ALL_SOUND_OFF; i++) {
      if (i >= EFFECTS_DEPTH1 && i <= EFFECTS_DEPTH5) {
        continue;
      }
      if (i >= SOUND_CTRL1 && i <= SOUND_CTRL10) {
        continue;
      }
      if (i == BANK_SELECT_MSB || i == BANK_SELECT_LSB || i == VOLUME_MSB || 
          i == VOLUME_LSB || i == PAN_MSB || i == PAN_LSB) {
        continue;
      }

      fluid_channel_set_cc (chan, i, 0);
    }
  }
  else {
    for (i = 0; i < 128; i++) {
      fluid_channel_set_cc (chan, i, 0);
    }
  }

  /* Set RPN controllers to NULL state */
  fluid_channel_set_cc (chan, RPN_LSB, 127);
  fluid_channel_set_cc (chan, RPN_MSB, 127);

  /* Set NRPN controllers to NULL state */
  fluid_channel_set_cc (chan, NRPN_LSB, 127);
  fluid_channel_set_cc (chan, NRPN_MSB, 127);

  /* Expression (MSB & LSB) */
  fluid_channel_set_cc (chan, EXPRESSION_MSB, 127);
  fluid_channel_set_cc (chan, EXPRESSION_LSB, 127);

  if (!is_all_ctrl_off) {

    chan->pitch_wheel_sensitivity = 2; /* two semi-tones */

    /* Just like panning, a value of 64 indicates no change for sound ctrls */
    for (i = SOUND_CTRL1; i <= SOUND_CTRL10; i++) {
      fluid_channel_set_cc (chan, i, 64);
    }

    /* Volume / initial attenuation (MSB & LSB) */
    fluid_channel_set_cc (chan, VOLUME_MSB, 100);
    fluid_channel_set_cc (chan, VOLUME_LSB, 0);

    /* Pan (MSB & LSB) */
    fluid_channel_set_cc (chan, PAN_MSB, 64);
    fluid_channel_set_cc (chan, PAN_LSB, 0);

    /* Reverb */
    /* fluid_channel_set_cc (chan, EFFECTS_DEPTH1, 40); */
    /* Note: although XG standard specifies the default amount of reverb to 
       be 40, most people preferred having it at zero.
       See http://lists.gnu.org/archive/html/fluid-dev/2009-07/msg00016.html */
  }
}

/* Only called by delete_fluid_synth(), so no need to queue a preset free event */
int
delete_fluid_channel(fluid_channel_t* chan)
{
  if (chan->preset) delete_fluid_preset (chan->preset);
  FLUID_FREE(chan);
  return FLUID_OK;
}

/* FIXME - Calls fluid_channel_init() potentially in synthesis context */
void
fluid_channel_reset(fluid_channel_t* chan)
{
  fluid_channel_init(chan);
  fluid_channel_init_ctrl(chan, 0);
}

/* Should only be called from synthesis context */
int
fluid_channel_set_preset(fluid_channel_t* chan, fluid_preset_t* preset)
{

  fluid_preset_notify (chan->preset, FLUID_PRESET_UNSELECTED, chan->channum);

  if (chan->preset) {
    fluid_sfont_t *sfont;
    sfont = chan->preset->sfont;
    delete_fluid_preset (chan->preset);
    fluid_synth_sfont_unref (chan->synth, sfont); /* -- unref preset's SoundFont */
  }
  
  chan->preset = preset;

  fluid_preset_notify (preset, FLUID_PRESET_SELECTED, chan->channum);

  return FLUID_OK;
}

/* Set SoundFont ID, MIDI bank and/or program.  Use -1 to use current value. */
void
fluid_channel_set_sfont_bank_prog(fluid_channel_t* chan, int sfontnum,
                                  int banknum, int prognum)
{
  int oldval, newval, oldmask;

  newval = ((sfontnum != -1) ? sfontnum << SFONT_SHIFTVAL : 0)
    | ((banknum != -1) ? banknum << BANK_SHIFTVAL : 0)
    | ((prognum != -1) ? prognum << PROG_SHIFTVAL : 0);

  oldmask = ((sfontnum != -1) ? 0 : SFONT_MASKVAL)
    | ((banknum != -1) ? 0 : BANK_MASKVAL)
    | ((prognum != -1) ? 0 : PROG_MASKVAL);

  oldval = chan->sfont_bank_prog;
  newval = (newval & ~oldmask) | (oldval & oldmask);
  chan->sfont_bank_prog = newval;
}

/* Set bank LSB 7 bits */
void
fluid_channel_set_bank_lsb(fluid_channel_t* chan, int banklsb)
{
  int oldval, newval, style;

  style = chan->synth->bank_select;
  if (style == FLUID_BANK_STYLE_GM ||
      style == FLUID_BANK_STYLE_GS)
      return; /* ignored */

  oldval = chan->sfont_bank_prog;
  if (style == FLUID_BANK_STYLE_XG)
      newval = (oldval & ~BANK_MASKVAL) | (banklsb << BANK_SHIFTVAL);
  else /* style == FLUID_BANK_STYLE_MMA */
      newval = (oldval & ~BANKLSB_MASKVAL) | (banklsb << BANK_SHIFTVAL);
  chan->sfont_bank_prog = newval;
}

/* Set bank MSB 7 bits */
void
fluid_channel_set_bank_msb(fluid_channel_t* chan, int bankmsb)
{
  int oldval, newval, style;

  style = chan->synth->bank_select;

  if (style == FLUID_BANK_STYLE_XG)
  {
    /* XG bank, do drum-channel auto-switch */
    /* The number "120" was based on several keyboards having drums at 120 - 127, 
       reference: http://lists.nongnu.org/archive/html/fluid-dev/2011-02/msg00003.html */
    chan->channel_type = (120 <= bankmsb) ? CHANNEL_TYPE_DRUM : CHANNEL_TYPE_MELODIC;
    return;
  }

  if (style == FLUID_BANK_STYLE_GM ||
      chan->channel_type == CHANNEL_TYPE_DRUM)
      return; /* ignored */

  oldval = chan->sfont_bank_prog;
  if (style == FLUID_BANK_STYLE_GS)
      newval = (oldval & ~BANK_MASKVAL) | (bankmsb << BANK_SHIFTVAL);
  else /* style == FLUID_BANK_STYLE_MMA */
      newval = (oldval & ~BANKMSB_MASKVAL) | (bankmsb << (BANK_SHIFTVAL + 7));
  chan->sfont_bank_prog = newval;

}

/* Get SoundFont ID, MIDI bank and/or program.  Use NULL to ignore a value. */
void
fluid_channel_get_sfont_bank_prog(fluid_channel_t* chan, int *sfont,
                                  int *bank, int *prog)
{
  int sfont_bank_prog;

  sfont_bank_prog = chan->sfont_bank_prog;

  if (sfont) *sfont = (sfont_bank_prog & SFONT_MASKVAL) >> SFONT_SHIFTVAL;
  if (bank) *bank = (sfont_bank_prog & BANK_MASKVAL) >> BANK_SHIFTVAL;
  if (prog) *prog = (sfont_bank_prog & PROG_MASKVAL) >> PROG_SHIFTVAL;
}
