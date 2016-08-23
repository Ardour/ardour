/*

  Freeverb

  Written by Jezar at Dreampoint, June 2000
  http://www.dreampoint.co.uk
  This code is public domain

  Translated to C by Peter Hanappe, Mai 2001
*/

#include "fluid_rev.h"

/***************************************************************
 *
 *                           REVERB
 */

/* Denormalising:
 *
 * According to music-dsp thread 'Denormalise', Pentium processors
 * have a hardware 'feature', that is of interest here, related to
 * numeric underflow.  We have a recursive filter. The output decays
 * exponentially, if the input stops.  So the numbers get smaller and
 * smaller... At some point, they reach 'denormal' level.  This will
 * lead to drastic spikes in the CPU load.  The effect was reproduced
 * with the reverb - sometimes the average load over 10 s doubles!!.
 *
 * The 'undenormalise' macro fixes the problem: As soon as the number
 * is close enough to denormal level, the macro forces the number to
 * 0.0f.  The original macro is:
 *
 * #define undenormalise(sample) if(((*(unsigned int*)&sample)&0x7f800000)==0) sample=0.0f
 *
 * This will zero out a number when it reaches the denormal level.
 * Advantage: Maximum dynamic range Disadvantage: We'll have to check
 * every sample, expensive.  The alternative macro comes from a later
 * mail from Jon Watte. It will zap a number before it reaches
 * denormal level. Jon suggests to run it once per block instead of
 * every sample.
 */

# if defined(WITH_FLOATX)
# define zap_almost_zero(sample) (((*(unsigned int*)&(sample))&0x7f800000) < 0x08000000)?0.0f:(sample)
# else
/* 1e-20 was chosen as an arbitrary (small) threshold. */
#define zap_almost_zero(sample) fabs(sample)<1e-10 ? 0 : sample;
#endif

/* Denormalising part II:
 *
 * Another method fixes the problem cheaper: Use a small DC-offset in
 * the filter calculations.  Now the signals converge not against 0,
 * but against the offset.  The constant offset is invisible from the
 * outside world (i.e. it does not appear at the output.  There is a
 * very small turn-on transient response, which should not cause
 * problems.
 */


//#define DC_OFFSET 0
#define DC_OFFSET 1e-8
//#define DC_OFFSET 0.001f
typedef struct _fluid_allpass fluid_allpass;
typedef struct _fluid_comb fluid_comb;

struct _fluid_allpass {
  fluid_real_t feedback;
  fluid_real_t *buffer;
  int bufsize;
  int bufidx;
};

void fluid_allpass_init(fluid_allpass* allpass);
void fluid_allpass_setfeedback(fluid_allpass* allpass, fluid_real_t val);
fluid_real_t fluid_allpass_getfeedback(fluid_allpass* allpass);

static void
fluid_allpass_setbuffer(fluid_allpass* allpass, int size)
{
  allpass->bufidx = 0;
  allpass->buffer = FLUID_ARRAY(fluid_real_t,size);
  allpass->bufsize = size;
}

static void
fluid_allpass_release(fluid_allpass* allpass)
{
  FLUID_FREE(allpass->buffer);
}

void
fluid_allpass_init(fluid_allpass* allpass)
{
  int i;
  int len = allpass->bufsize;
  fluid_real_t* buf = allpass->buffer;
  for (i = 0; i < len; i++) {
    buf[i] = DC_OFFSET; /* this is not 100 % correct. */
  }
}

void
fluid_allpass_setfeedback(fluid_allpass* allpass, fluid_real_t val)
{
  allpass->feedback = val;
}

fluid_real_t
fluid_allpass_getfeedback(fluid_allpass* allpass)
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

/*  fluid_real_t fluid_allpass_process(fluid_allpass* allpass, fluid_real_t input) */
/*  { */
/*    fluid_real_t output; */
/*    fluid_real_t bufout; */
/*    bufout = allpass->buffer[allpass->bufidx]; */
/*    undenormalise(bufout); */
/*    output = -input + bufout; */
/*    allpass->buffer[allpass->bufidx] = input + (bufout * allpass->feedback); */
/*    if (++allpass->bufidx >= allpass->bufsize) { */
/*      allpass->bufidx = 0; */
/*    } */
/*    return output; */
/*  } */

struct _fluid_comb {
  fluid_real_t feedback;
  fluid_real_t filterstore;
  fluid_real_t damp1;
  fluid_real_t damp2;
  fluid_real_t *buffer;
  int bufsize;
  int bufidx;
};

void fluid_comb_setbuffer(fluid_comb* comb, int size);
void fluid_comb_release(fluid_comb* comb);
void fluid_comb_init(fluid_comb* comb);
void fluid_comb_setdamp(fluid_comb* comb, fluid_real_t val);
fluid_real_t fluid_comb_getdamp(fluid_comb* comb);
void fluid_comb_setfeedback(fluid_comb* comb, fluid_real_t val);
fluid_real_t fluid_comb_getfeedback(fluid_comb* comb);

void
fluid_comb_setbuffer(fluid_comb* comb, int size)
{
  comb->filterstore = 0;
  comb->bufidx = 0;
  comb->buffer = FLUID_ARRAY(fluid_real_t,size);
  comb->bufsize = size;
}

void
fluid_comb_release(fluid_comb* comb)
{
  FLUID_FREE(comb->buffer);
}

void
fluid_comb_init(fluid_comb* comb)
{
  int i;
  fluid_real_t* buf = comb->buffer;
  int len = comb->bufsize;
  for (i = 0; i < len; i++) {
    buf[i] = DC_OFFSET; /* This is not 100 % correct. */
  }
}

void
fluid_comb_setdamp(fluid_comb* comb, fluid_real_t val)
{
  comb->damp1 = val;
  comb->damp2 = 1 - val;
}

fluid_real_t
fluid_comb_getdamp(fluid_comb* comb)
{
  return comb->damp1;
}

void
fluid_comb_setfeedback(fluid_comb* comb, fluid_real_t val)
{
  comb->feedback = val;
}

fluid_real_t
fluid_comb_getfeedback(fluid_comb* comb)
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

/* fluid_real_t fluid_comb_process(fluid_comb* comb, fluid_real_t input) */
/* { */
/*    fluid_real_t output; */

/*    output = comb->buffer[comb->bufidx]; */
/*    undenormalise(output); */
/*    comb->filterstore = (output * comb->damp2) + (comb->filterstore * comb->damp1); */
/*    undenormalise(comb->filterstore); */
/*    comb->buffer[comb->bufidx] = input + (comb->filterstore * comb->feedback); */
/*    if (++comb->bufidx >= comb->bufsize) { */
/*      comb->bufidx = 0; */
/*    } */

/*    return output; */
/* } */

#define numcombs 8
#define numallpasses 4
#define	fixedgain 0.015f
#define scalewet 3.0f
#define scaledamp 1.0f
#define scaleroom 0.28f
#define offsetroom 0.7f
#define initialroom 0.5f
#define initialdamp 0.2f
#define initialwet 1
#define initialdry 0
#define initialwidth 1
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

struct _fluid_revmodel_t {
  fluid_real_t roomsize;
  fluid_real_t damp;
  fluid_real_t wet, wet1, wet2;
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

static void fluid_revmodel_update(fluid_revmodel_t* rev);
static void fluid_revmodel_init(fluid_revmodel_t* rev);
void fluid_set_revmodel_buffers(fluid_revmodel_t* rev, fluid_real_t sample_rate);

fluid_revmodel_t*
new_fluid_revmodel(fluid_real_t sample_rate)
{
  fluid_revmodel_t* rev;
  rev = FLUID_NEW(fluid_revmodel_t);
  if (rev == NULL) {
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
  fluid_revmodel_set(rev,FLUID_REVMODEL_SET_ALL,initialroom,initialdamp,initialwidth,initialwet);

  return rev;
}

void
delete_fluid_revmodel(fluid_revmodel_t* rev)
{
  int i;
  for (i = 0; i < numcombs;i++) {
    fluid_comb_release(&rev->combL[i]);
    fluid_comb_release(&rev->combR[i]);
  }
  for (i = 0; i < numallpasses; i++) {
    fluid_allpass_release(&rev->allpassL[i]);
    fluid_allpass_release(&rev->allpassR[i]);
  }

  FLUID_FREE(rev);
}

void
fluid_set_revmodel_buffers(fluid_revmodel_t* rev, fluid_real_t sample_rate) {

  float srfactor = sample_rate/44100.0f;

  fluid_comb_setbuffer(&rev->combL[0], combtuningL1*srfactor);
  fluid_comb_setbuffer(&rev->combR[0], combtuningR1*srfactor);
  fluid_comb_setbuffer(&rev->combL[1], combtuningL2*srfactor);
  fluid_comb_setbuffer(&rev->combR[1], combtuningR2*srfactor);
  fluid_comb_setbuffer(&rev->combL[2], combtuningL3*srfactor);
  fluid_comb_setbuffer(&rev->combR[2], combtuningR3*srfactor);
  fluid_comb_setbuffer(&rev->combL[3], combtuningL4*srfactor);
  fluid_comb_setbuffer(&rev->combR[3], combtuningR4*srfactor);
  fluid_comb_setbuffer(&rev->combL[4], combtuningL5*srfactor);
  fluid_comb_setbuffer(&rev->combR[4], combtuningR5*srfactor);
  fluid_comb_setbuffer(&rev->combL[5], combtuningL6*srfactor);
  fluid_comb_setbuffer(&rev->combR[5], combtuningR6*srfactor);
  fluid_comb_setbuffer(&rev->combL[6], combtuningL7*srfactor);
  fluid_comb_setbuffer(&rev->combR[6], combtuningR7*srfactor);
  fluid_comb_setbuffer(&rev->combL[7], combtuningL8*srfactor);
  fluid_comb_setbuffer(&rev->combR[7], combtuningR8*srfactor);
  fluid_allpass_setbuffer(&rev->allpassL[0], allpasstuningL1*srfactor);
  fluid_allpass_setbuffer(&rev->allpassR[0], allpasstuningR1*srfactor);
  fluid_allpass_setbuffer(&rev->allpassL[1], allpasstuningL2*srfactor);
  fluid_allpass_setbuffer(&rev->allpassR[1], allpasstuningR2*srfactor);
  fluid_allpass_setbuffer(&rev->allpassL[2], allpasstuningL3*srfactor);
  fluid_allpass_setbuffer(&rev->allpassR[2], allpasstuningR3*srfactor);
  fluid_allpass_setbuffer(&rev->allpassL[3], allpasstuningL4*srfactor);
  fluid_allpass_setbuffer(&rev->allpassR[3], allpasstuningR4*srfactor);

  /* Clear all buffers */
  fluid_revmodel_init(rev);
}


static void
fluid_revmodel_init(fluid_revmodel_t* rev)
{
  int i;
  for (i = 0; i < numcombs;i++) {
    fluid_comb_init(&rev->combL[i]);
    fluid_comb_init(&rev->combR[i]);
  }
  for (i = 0; i < numallpasses; i++) {
    fluid_allpass_init(&rev->allpassL[i]);
    fluid_allpass_init(&rev->allpassR[i]);
  }
}

void
fluid_revmodel_reset(fluid_revmodel_t* rev)
{
  fluid_revmodel_init(rev);
}

void
fluid_revmodel_processreplace(fluid_revmodel_t* rev, fluid_real_t *in,
			     fluid_real_t *left_out, fluid_real_t *right_out)
{
  int i, k = 0;
  fluid_real_t outL, outR, input;

  for (k = 0; k < FLUID_BUFSIZE; k++) {

    outL = outR = 0;

    /* The original Freeverb code expects a stereo signal and 'input'
     * is set to the sum of the left and right input sample. Since
     * this code works on a mono signal, 'input' is set to twice the
     * input sample. */
    input = (2.0f * in[k] + DC_OFFSET) * rev->gain;

    /* Accumulate comb filters in parallel */
    for (i = 0; i < numcombs; i++) {
      fluid_comb_process(rev->combL[i], input, outL);
      fluid_comb_process(rev->combR[i], input, outR);
    }
    /* Feed through allpasses in series */
    for (i = 0; i < numallpasses; i++) {
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
fluid_revmodel_processmix(fluid_revmodel_t* rev, fluid_real_t *in,
			 fluid_real_t *left_out, fluid_real_t *right_out)
{
  int i, k = 0;
  fluid_real_t outL, outR, input;

  for (k = 0; k < FLUID_BUFSIZE; k++) {

    outL = outR = 0;

    /* The original Freeverb code expects a stereo signal and 'input'
     * is set to the sum of the left and right input sample. Since
     * this code works on a mono signal, 'input' is set to twice the
     * input sample. */
    input = (2.0f * in[k] + DC_OFFSET) * rev->gain;

    /* Accumulate comb filters in parallel */
    for (i = 0; i < numcombs; i++) {
	    fluid_comb_process(rev->combL[i], input, outL);
	    fluid_comb_process(rev->combR[i], input, outR);
    }
    /* Feed through allpasses in series */
    for (i = 0; i < numallpasses; i++) {
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
fluid_revmodel_update(fluid_revmodel_t* rev)
{
  /* Recalculate internal values after parameter change */
  int i;

  rev->wet1 = rev->wet * (rev->width / 2.0f + 0.5f);
  rev->wet2 = rev->wet * ((1.0f - rev->width) / 2.0f);

  for (i = 0; i < numcombs; i++) {
    fluid_comb_setfeedback(&rev->combL[i], rev->roomsize);
    fluid_comb_setfeedback(&rev->combR[i], rev->roomsize);
  }

  for (i = 0; i < numcombs; i++) {
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
fluid_revmodel_set(fluid_revmodel_t* rev, int set, float roomsize,
                   float damping, float width, float level)
{
  if (set & FLUID_REVMODEL_SET_ROOMSIZE)
    rev->roomsize = (roomsize * scaleroom) + offsetroom;

  if (set & FLUID_REVMODEL_SET_DAMPING)
    rev->damp = damping * scaledamp;

  if (set & FLUID_REVMODEL_SET_WIDTH)
    rev->width = width;

  if (set & FLUID_REVMODEL_SET_LEVEL)
  {
    fluid_clip(level, 0.0f, 1.0f);
    rev->wet = level * scalewet;
  }

  fluid_revmodel_update (rev);
}

void
fluid_revmodel_samplerate_change(fluid_revmodel_t* rev, fluid_real_t sample_rate) {
  int i;
  for (i = 0; i < numcombs;i++) {
    fluid_comb_release(&rev->combL[i]);
    fluid_comb_release(&rev->combR[i]);
  }
  for (i = 0; i < numallpasses; i++) {
    fluid_allpass_release(&rev->allpassL[i]);
    fluid_allpass_release(&rev->allpassR[i]);
  }
  fluid_set_revmodel_buffers(rev, sample_rate);
}
