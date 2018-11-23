/*

  Freeverb

  Written by Jezar at Dreampoint, June 2000
  http://www.dreampoint.co.uk
  This code is public domain

  Translated to C by Peter Hanappe, Mai 2001
*/

#include "fluid_sys.h"
#include "fluid_rev.h"

/***************************************************************
 *
 *                           REVERB
 */

/* Denormalising:
 *
 * We have a recursive filter. The output decays exponentially, if the input
 * stops. So the numbers get smaller and smaller... At some point, they reach
 * 'denormal' level. On some platforms this will lead to drastic spikes in the
 * CPU load. This is especially noticable on some older Pentium (especially
 * Pentium 3) processors, but even more modern Intel Core processors still show
 * reduced performance with denormals. While there are compile-time switches to
 * treat denormals as zero for a lot of processors, those are not available or
 * effective on all platforms.
 *
 * The fix used here: Use a small DC-offset in the filter calculations.  Now
 * the signals converge not against 0, but against the offset.  The constant
 * offset is invisible from the outside world (i.e. it does not appear at the
 * output.  There is a very small turn-on transient response, which should not
 * cause problems.
 */
#define DC_OFFSET ((fluid_real_t)1e-8)

typedef struct _fluid_allpass fluid_allpass;
typedef struct _fluid_comb fluid_comb;

struct _fluid_allpass
{
    fluid_real_t feedback;
    fluid_real_t *buffer;
    int bufsize;
    int bufidx;
};

void fluid_allpass_init(fluid_allpass *allpass);
void fluid_allpass_setfeedback(fluid_allpass *allpass, fluid_real_t val);
fluid_real_t fluid_allpass_getfeedback(fluid_allpass *allpass);

static void
fluid_allpass_setbuffer(fluid_allpass *allpass, int size)
{
    allpass->bufidx = 0;
    allpass->buffer = FLUID_ARRAY(fluid_real_t, size);
    allpass->bufsize = size;
}

static void
fluid_allpass_release(fluid_allpass *allpass)
{
    FLUID_FREE(allpass->buffer);
}

void
fluid_allpass_init(fluid_allpass *allpass)
{
    int i;
    int len = allpass->bufsize;
    fluid_real_t *buf = allpass->buffer;

    for(i = 0; i < len; i++)
    {
        buf[i] = DC_OFFSET; /* this is not 100 % correct. */
    }
}

void
fluid_allpass_setfeedback(fluid_allpass *allpass, fluid_real_t val)
{
    allpass->feedback = val;
}

fluid_real_t
fluid_allpass_getfeedback(fluid_allpass *allpass)
{
    return allpass->feedback;
}

#define fluid_allpass_process(_allpass, _input) \
{ \
  fluid_real_t output; \
  fluid_real_t bufout; \
  bufout = _allpass.buffer[_allpass.bufidx]; \
  output = bufout-_input; \
  _allpass.buffer[_allpass.bufidx] = _input + (bufout * _allpass.feedback); \
  if (++_allpass.bufidx >= _allpass.bufsize) { \
    _allpass.bufidx = 0; \
  } \
  _input = output; \
}

struct _fluid_comb
{
    fluid_real_t feedback;
    fluid_real_t filterstore;
    fluid_real_t damp1;
    fluid_real_t damp2;
    fluid_real_t *buffer;
    int bufsize;
    int bufidx;
};

void fluid_comb_setbuffer(fluid_comb *comb, int size);
void fluid_comb_release(fluid_comb *comb);
void fluid_comb_init(fluid_comb *comb);
void fluid_comb_setdamp(fluid_comb *comb, fluid_real_t val);
fluid_real_t fluid_comb_getdamp(fluid_comb *comb);
void fluid_comb_setfeedback(fluid_comb *comb, fluid_real_t val);
fluid_real_t fluid_comb_getfeedback(fluid_comb *comb);

void
fluid_comb_setbuffer(fluid_comb *comb, int size)
{
    comb->filterstore = 0;
    comb->bufidx = 0;
    comb->buffer = FLUID_ARRAY(fluid_real_t, size);
    comb->bufsize = size;
}

void
fluid_comb_release(fluid_comb *comb)
{
    FLUID_FREE(comb->buffer);
}

void
fluid_comb_init(fluid_comb *comb)
{
    int i;
    fluid_real_t *buf = comb->buffer;
    int len = comb->bufsize;

    for(i = 0; i < len; i++)
    {
        buf[i] = DC_OFFSET; /* This is not 100 % correct. */
    }
}

void
fluid_comb_setdamp(fluid_comb *comb, fluid_real_t val)
{
    comb->damp1 = val;
    comb->damp2 = 1 - val;
}

fluid_real_t
fluid_comb_getdamp(fluid_comb *comb)
{
    return comb->damp1;
}

void
fluid_comb_setfeedback(fluid_comb *comb, fluid_real_t val)
{
    comb->feedback = val;
}

fluid_real_t
fluid_comb_getfeedback(fluid_comb *comb)
{
    return comb->feedback;
}

#define fluid_comb_process(_comb, _input, _output) \
{ \
  fluid_real_t _tmp = _comb.buffer[_comb.bufidx]; \
  _comb.filterstore = (_tmp * _comb.damp2) + (_comb.filterstore * _comb.damp1); \
  _comb.buffer[_comb.bufidx] = _input + (_comb.filterstore * _comb.feedback); \
  if (++_comb.bufidx >= _comb.bufsize) { \
    _comb.bufidx = 0; \
  } \
  _output += _tmp; \
}

#define numcombs 8
#define numallpasses 4
#define	fixedgain 0.015f
/* scale_wet_width is a compensation weight factor to get an output
   amplitude (wet) rather independent of the width setting.
    0: the output amplitude is fully dependant on the width setting.
   >0: the output amplitude is less dependant on the width setting.
   With a scale_wet_width of 0.2 the output amplitude is rather
   independent of width setting (see fluid_revmodel_update()).
 */
#define scale_wet_width 0.2f
#define scalewet 3.0f
#define scaledamp 1.0f
#define scaleroom 0.28f
#define offsetroom 0.7f
#define stereospread 23

/*
 These values assume 44.1KHz sample rate
 they will probably be OK for 48KHz sample rate
 but would need scaling for 96KHz (or other) sample rates.
 The values were obtained by listening tests.
*/
#define combtuningL1 1116
#define combtuningR1 (1116 + stereospread)
#define combtuningL2 1188
#define combtuningR2 (1188 + stereospread)
#define combtuningL3 1277
#define combtuningR3 (1277 + stereospread)
#define combtuningL4 1356
#define combtuningR4 (1356 + stereospread)
#define combtuningL5 1422
#define combtuningR5 (1422 + stereospread)
#define combtuningL6 1491
#define combtuningR6 (1491 + stereospread)
#define combtuningL7 1557
#define combtuningR7 (1557 + stereospread)
#define combtuningL8 1617
#define combtuningR8 (1617 + stereospread)
#define allpasstuningL1 556
#define allpasstuningR1 (556 + stereospread)
#define allpasstuningL2 441
#define allpasstuningR2 (441 + stereospread)
#define allpasstuningL3 341
#define allpasstuningR3 (341 + stereospread)
#define allpasstuningL4 225
#define allpasstuningR4 (225 + stereospread)

struct _fluid_revmodel_t
{
    fluid_real_t roomsize;
    fluid_real_t damp;
    fluid_real_t level, wet1, wet2;
    fluid_real_t width;
    fluid_real_t gain;
    /*
     The following are all declared inline
     to remove the need for dynamic allocation
     with its subsequent error-checking messiness
    */
    /* Comb filters */
    fluid_comb combL[numcombs];
    fluid_comb combR[numcombs];
    /* Allpass filters */
    fluid_allpass allpassL[numallpasses];
    fluid_allpass allpassR[numallpasses];
};

static void fluid_revmodel_update(fluid_revmodel_t *rev);
static void fluid_revmodel_init(fluid_revmodel_t *rev);
void fluid_set_revmodel_buffers(fluid_revmodel_t *rev, fluid_real_t sample_rate);

fluid_revmodel_t *
new_fluid_revmodel(fluid_real_t sample_rate)
{
    fluid_revmodel_t *rev;
    rev = FLUID_NEW(fluid_revmodel_t);

    if(rev == NULL)
    {
        return NULL;
    }

    fluid_set_revmodel_buffers(rev, sample_rate);

    /* Set default values */
    fluid_allpass_setfeedback(&rev->allpassL[0], 0.5f);
    fluid_allpass_setfeedback(&rev->allpassR[0], 0.5f);
    fluid_allpass_setfeedback(&rev->allpassL[1], 0.5f);
    fluid_allpass_setfeedback(&rev->allpassR[1], 0.5f);
    fluid_allpass_setfeedback(&rev->allpassL[2], 0.5f);
    fluid_allpass_setfeedback(&rev->allpassR[2], 0.5f);
    fluid_allpass_setfeedback(&rev->allpassL[3], 0.5f);
    fluid_allpass_setfeedback(&rev->allpassR[3], 0.5f);

    rev->gain = fixedgain;

    return rev;
}

void
delete_fluid_revmodel(fluid_revmodel_t *rev)
{
    int i;
    fluid_return_if_fail(rev != NULL);

    for(i = 0; i < numcombs; i++)
    {
        fluid_comb_release(&rev->combL[i]);
        fluid_comb_release(&rev->combR[i]);
    }

    for(i = 0; i < numallpasses; i++)
    {
        fluid_allpass_release(&rev->allpassL[i]);
        fluid_allpass_release(&rev->allpassR[i]);
    }

    FLUID_FREE(rev);
}

void
fluid_set_revmodel_buffers(fluid_revmodel_t *rev, fluid_real_t sample_rate)
{

    float srfactor = sample_rate / 44100.0f;

    fluid_comb_setbuffer(&rev->combL[0], combtuningL1 * srfactor);
    fluid_comb_setbuffer(&rev->combR[0], combtuningR1 * srfactor);
    fluid_comb_setbuffer(&rev->combL[1], combtuningL2 * srfactor);
    fluid_comb_setbuffer(&rev->combR[1], combtuningR2 * srfactor);
    fluid_comb_setbuffer(&rev->combL[2], combtuningL3 * srfactor);
    fluid_comb_setbuffer(&rev->combR[2], combtuningR3 * srfactor);
    fluid_comb_setbuffer(&rev->combL[3], combtuningL4 * srfactor);
    fluid_comb_setbuffer(&rev->combR[3], combtuningR4 * srfactor);
    fluid_comb_setbuffer(&rev->combL[4], combtuningL5 * srfactor);
    fluid_comb_setbuffer(&rev->combR[4], combtuningR5 * srfactor);
    fluid_comb_setbuffer(&rev->combL[5], combtuningL6 * srfactor);
    fluid_comb_setbuffer(&rev->combR[5], combtuningR6 * srfactor);
    fluid_comb_setbuffer(&rev->combL[6], combtuningL7 * srfactor);
    fluid_comb_setbuffer(&rev->combR[6], combtuningR7 * srfactor);
    fluid_comb_setbuffer(&rev->combL[7], combtuningL8 * srfactor);
    fluid_comb_setbuffer(&rev->combR[7], combtuningR8 * srfactor);
    fluid_allpass_setbuffer(&rev->allpassL[0], allpasstuningL1 * srfactor);
    fluid_allpass_setbuffer(&rev->allpassR[0], allpasstuningR1 * srfactor);
    fluid_allpass_setbuffer(&rev->allpassL[1], allpasstuningL2 * srfactor);
    fluid_allpass_setbuffer(&rev->allpassR[1], allpasstuningR2 * srfactor);
    fluid_allpass_setbuffer(&rev->allpassL[2], allpasstuningL3 * srfactor);
    fluid_allpass_setbuffer(&rev->allpassR[2], allpasstuningR3 * srfactor);
    fluid_allpass_setbuffer(&rev->allpassL[3], allpasstuningL4 * srfactor);
    fluid_allpass_setbuffer(&rev->allpassR[3], allpasstuningR4 * srfactor);

    /* Clear all buffers */
    fluid_revmodel_init(rev);
}


static void
fluid_revmodel_init(fluid_revmodel_t *rev)
{
    int i;

    for(i = 0; i < numcombs; i++)
    {
        fluid_comb_init(&rev->combL[i]);
        fluid_comb_init(&rev->combR[i]);
    }

    for(i = 0; i < numallpasses; i++)
    {
        fluid_allpass_init(&rev->allpassL[i]);
        fluid_allpass_init(&rev->allpassR[i]);
    }
}

void
fluid_revmodel_reset(fluid_revmodel_t *rev)
{
    fluid_revmodel_init(rev);
}

void
fluid_revmodel_processreplace(fluid_revmodel_t *rev, const fluid_real_t *in,
                              fluid_real_t *left_out, fluid_real_t *right_out)
{
    int i, k = 0;
    fluid_real_t outL, outR, input;

    for(k = 0; k < FLUID_BUFSIZE; k++)
    {

        outL = outR = 0;

        /* The original Freeverb code expects a stereo signal and 'input'
         * is set to the sum of the left and right input sample. Since
         * this code works on a mono signal, 'input' is set to twice the
         * input sample. */
        input = (2.0f * in[k] + DC_OFFSET) * rev->gain;

        /* Accumulate comb filters in parallel */
        for(i = 0; i < numcombs; i++)
        {
            fluid_comb_process(rev->combL[i], input, outL);
            fluid_comb_process(rev->combR[i], input, outR);
        }

        /* Feed through allpasses in series */
        for(i = 0; i < numallpasses; i++)
        {
            fluid_allpass_process(rev->allpassL[i], outL);
            fluid_allpass_process(rev->allpassR[i], outR);
        }

        /* Remove the DC offset */
        outL -= DC_OFFSET;
        outR -= DC_OFFSET;

        /* Calculate output REPLACING anything already there */
        left_out[k] = outL * rev->wet1 + outR * rev->wet2;
        right_out[k] = outR * rev->wet1 + outL * rev->wet2;
    }
}

void
fluid_revmodel_processmix(fluid_revmodel_t *rev, const fluid_real_t *in,
                          fluid_real_t *left_out, fluid_real_t *right_out)
{
    int i, k = 0;
    fluid_real_t outL, outR, input;

    for(k = 0; k < FLUID_BUFSIZE; k++)
    {

        outL = outR = 0;

        /* The original Freeverb code expects a stereo signal and 'input'
         * is set to the sum of the left and right input sample. Since
         * this code works on a mono signal, 'input' is set to twice the
         * input sample. */
        input = (2.0f * in[k] + DC_OFFSET) * rev->gain;

        /* Accumulate comb filters in parallel */
        for(i = 0; i < numcombs; i++)
        {
            fluid_comb_process(rev->combL[i], input, outL);
            fluid_comb_process(rev->combR[i], input, outR);
        }

        /* Feed through allpasses in series */
        for(i = 0; i < numallpasses; i++)
        {
            fluid_allpass_process(rev->allpassL[i], outL);
            fluid_allpass_process(rev->allpassR[i], outR);
        }

        /* Remove the DC offset */
        outL -= DC_OFFSET;
        outR -= DC_OFFSET;

        /* Calculate output MIXING with anything already there */
        left_out[k] += outL * rev->wet1 + outR * rev->wet2;
        right_out[k] += outR * rev->wet1 + outL * rev->wet2;
    }
}

static void
fluid_revmodel_update(fluid_revmodel_t *rev)
{
    /* Recalculate internal values after parameter change */
    int i;

    /* The stereo amplitude equation (wet1 and wet2 below) have a
    tendency to produce high amplitude with high width values ( 1 < width < 100).
    This results in an unwanted noisy output clipped by the audio card.
    To avoid this dependency, we divide by (1 + rev->width * scale_wet_width)
    Actually, with a scale_wet_width of 0.2, (regardless of level setting),
    the output amplitude (wet) seems rather independent of width setting */
    fluid_real_t wet = (rev->level * scalewet) /
                       (1.0f + rev->width * scale_wet_width);

    /* wet1 and wet2 are used by the stereo effect controled by the width setting
    for producing a stereo ouptput from a monophonic reverb signal.
    Please see the note above about a side effect tendency */
    rev->wet1 = wet * (rev->width / 2.0f + 0.5f);
    rev->wet2 = wet * ((1.0f - rev->width) / 2.0f);

    for(i = 0; i < numcombs; i++)
    {
        fluid_comb_setfeedback(&rev->combL[i], rev->roomsize);
        fluid_comb_setfeedback(&rev->combR[i], rev->roomsize);
    }

    for(i = 0; i < numcombs; i++)
    {
        fluid_comb_setdamp(&rev->combL[i], rev->damp);
        fluid_comb_setdamp(&rev->combR[i], rev->damp);
    }
}

/**
 * Set one or more reverb parameters.
 * @param rev Reverb instance
 * @param set One or more flags from #fluid_revmodel_set_t indicating what
 *   parameters to set (#FLUID_REVMODEL_SET_ALL to set all parameters)
 * @param roomsize Reverb room size
 * @param damping Reverb damping
 * @param width Reverb width
 * @param level Reverb level
 */
void
fluid_revmodel_set(fluid_revmodel_t *rev, int set, fluid_real_t roomsize,
                   fluid_real_t damping, fluid_real_t width, fluid_real_t level)
{
    if(set & FLUID_REVMODEL_SET_ROOMSIZE)
    {
        /* With upper limit above 1.07, the output amplitude will grow
        exponentially. So, keeping this upper limit to 1.0 seems sufficient
        as it produces yet a long reverb time */
        fluid_clip(roomsize, 0.0f, 1.0f);
        rev->roomsize = (roomsize * scaleroom) + offsetroom;
    }

    if(set & FLUID_REVMODEL_SET_DAMPING)
    {
        rev->damp = damping * scaledamp;
    }

    if(set & FLUID_REVMODEL_SET_WIDTH)
    {
        rev->width = width;
    }

    if(set & FLUID_REVMODEL_SET_LEVEL)
    {
        fluid_clip(level, 0.0f, 1.0f);
        rev->level = level;
    }

    fluid_revmodel_update(rev);
}

void
fluid_revmodel_samplerate_change(fluid_revmodel_t *rev, fluid_real_t sample_rate)
{
    int i;

    for(i = 0; i < numcombs; i++)
    {
        fluid_comb_release(&rev->combL[i]);
        fluid_comb_release(&rev->combR[i]);
    }

    for(i = 0; i < numallpasses; i++)
    {
        fluid_allpass_release(&rev->allpassL[i]);
        fluid_allpass_release(&rev->allpassR[i]);
    }

    fluid_set_revmodel_buffers(rev, sample_rate);
}
