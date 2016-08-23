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

#include "fluid_rvoice_event.h"
#include "fluid_rvoice.h"
#include "fluid_rvoice_mixer.h"
#include "fluid_iir_filter.h"
#include "fluid_lfo.h"
#include "fluid_adsr_env.h"

#define EVENTFUNC_0(proc, type) \
  if (event->method == proc) { \
    proc((type) event->object); \
    return; }

#define EVENTFUNC_R1(proc, type) \
  if (event->method == proc) { \
    if(event->intparam != 0) { FLUID_LOG(FLUID_DBG, "IR-mismatch"); }  \
    proc((type) event->object, event->realparams[0]); \
    return; }

#define EVENTFUNC_PTR(proc, type, type2) \
  if (event->method == proc) { \
    proc((type) event->object, (type2) event->ptr); \
    return; }

#define EVENTFUNC_I1(proc, type) \
  if (event->method == proc) { \
    if(event->realparams[0] != 0.0f) { FLUID_LOG(FLUID_DBG, "IR-mismatch"); }  \
    proc((type) event->object, event->intparam); \
    return; } 

#define EVENTFUNC_IR(proc, type) \
  if (event->method == proc) { \
    proc((type) event->object, event->intparam, event->realparams[0]); \
    return; } 
  
#define EVENTFUNC_ALL(proc, type) \
  if (event->method == proc) { \
    proc((type) event->object, event->intparam, event->realparams[0], \
      event->realparams[1], event->realparams[2], event->realparams[3], \
      event->realparams[4]); \
    return; }

#define EVENTFUNC_R4(proc, type) \
  if (event->method == proc) { \
    proc((type) event->object, event->intparam, event->realparams[0], \
      event->realparams[1], event->realparams[2], event->realparams[3]); \
    return; }

void
fluid_rvoice_event_dispatch(fluid_rvoice_event_t* event)
{
  EVENTFUNC_PTR(fluid_rvoice_mixer_add_voice, fluid_rvoice_mixer_t*, fluid_rvoice_t*);
  EVENTFUNC_I1(fluid_rvoice_noteoff, fluid_rvoice_t*);
  EVENTFUNC_0(fluid_rvoice_voiceoff, fluid_rvoice_t*);
  EVENTFUNC_0(fluid_rvoice_reset, fluid_rvoice_t*);

  EVENTFUNC_ALL(fluid_adsr_env_set_data, fluid_adsr_env_t*);

  EVENTFUNC_I1(fluid_lfo_set_delay, fluid_lfo_t*);
  EVENTFUNC_R1(fluid_lfo_set_incr, fluid_lfo_t*);

  EVENTFUNC_R1(fluid_iir_filter_set_fres, fluid_iir_filter_t*);
  EVENTFUNC_R1(fluid_iir_filter_set_q_dB, fluid_iir_filter_t*);

  EVENTFUNC_IR(fluid_rvoice_buffers_set_mapping, fluid_rvoice_buffers_t*);
  EVENTFUNC_IR(fluid_rvoice_buffers_set_amp, fluid_rvoice_buffers_t*);

  EVENTFUNC_R1(fluid_rvoice_set_modenv_to_pitch, fluid_rvoice_t*);
  EVENTFUNC_R1(fluid_rvoice_set_output_rate, fluid_rvoice_t*);
  EVENTFUNC_R1(fluid_rvoice_set_root_pitch_hz, fluid_rvoice_t*);
  EVENTFUNC_R1(fluid_rvoice_set_synth_gain, fluid_rvoice_t*);
  EVENTFUNC_R1(fluid_rvoice_set_pitch, fluid_rvoice_t*);
  EVENTFUNC_R1(fluid_rvoice_set_attenuation, fluid_rvoice_t*);
  EVENTFUNC_R1(fluid_rvoice_set_min_attenuation_cB, fluid_rvoice_t*);
  EVENTFUNC_R1(fluid_rvoice_set_viblfo_to_pitch, fluid_rvoice_t*);
  EVENTFUNC_R1(fluid_rvoice_set_modlfo_to_pitch, fluid_rvoice_t*);
  EVENTFUNC_R1(fluid_rvoice_set_modlfo_to_vol, fluid_rvoice_t*);
  EVENTFUNC_R1(fluid_rvoice_set_modlfo_to_fc, fluid_rvoice_t*);
  EVENTFUNC_R1(fluid_rvoice_set_modenv_to_fc, fluid_rvoice_t*);
  EVENTFUNC_R1(fluid_rvoice_set_modenv_to_pitch, fluid_rvoice_t*);
  EVENTFUNC_I1(fluid_rvoice_set_interp_method, fluid_rvoice_t*);
  EVENTFUNC_I1(fluid_rvoice_set_start, fluid_rvoice_t*);
  EVENTFUNC_I1(fluid_rvoice_set_end, fluid_rvoice_t*);
  EVENTFUNC_I1(fluid_rvoice_set_loopstart, fluid_rvoice_t*);
  EVENTFUNC_I1(fluid_rvoice_set_loopend, fluid_rvoice_t*);
  EVENTFUNC_I1(fluid_rvoice_set_samplemode, fluid_rvoice_t*);
  EVENTFUNC_PTR(fluid_rvoice_set_sample, fluid_rvoice_t*, fluid_sample_t*);

  EVENTFUNC_R1(fluid_rvoice_mixer_set_samplerate, fluid_rvoice_mixer_t*);
  EVENTFUNC_I1(fluid_rvoice_mixer_set_polyphony, fluid_rvoice_mixer_t*);
  EVENTFUNC_I1(fluid_rvoice_mixer_set_reverb_enabled, fluid_rvoice_mixer_t*);
  EVENTFUNC_I1(fluid_rvoice_mixer_set_chorus_enabled, fluid_rvoice_mixer_t*);
  EVENTFUNC_I1(fluid_rvoice_mixer_set_mix_fx, fluid_rvoice_mixer_t*);
  EVENTFUNC_0(fluid_rvoice_mixer_reset_fx, fluid_rvoice_mixer_t*);
  EVENTFUNC_0(fluid_rvoice_mixer_reset_reverb, fluid_rvoice_mixer_t*);
  EVENTFUNC_0(fluid_rvoice_mixer_reset_chorus, fluid_rvoice_mixer_t*);
  EVENTFUNC_IR(fluid_rvoice_mixer_set_threads, fluid_rvoice_mixer_t*);
 
  EVENTFUNC_ALL(fluid_rvoice_mixer_set_chorus_params, fluid_rvoice_mixer_t*);
  EVENTFUNC_R4(fluid_rvoice_mixer_set_reverb_params, fluid_rvoice_mixer_t*);

  FLUID_LOG(FLUID_ERR, "fluid_rvoice_event_dispatch: Unknown method %p to dispatch!", event->method);
}


/**
 * In order to be able to push more than one event atomically,
 * use push for all events, then use flush to commit them to the 
 * queue. If threadsafe is false, all events are processed immediately. */
int
fluid_rvoice_eventhandler_push(fluid_rvoice_eventhandler_t* handler, 
                                void* method, void* object, int intparam, 
                                fluid_real_t realparam)
{
  fluid_rvoice_event_t* event;
  fluid_rvoice_event_t local_event;
  event = handler->is_threadsafe ? 
    fluid_ringbuffer_get_inptr(handler->queue, handler->queue_stored) : &local_event;

  if (event == NULL) {
    FLUID_LOG(FLUID_WARN, "Ringbuffer full, try increasing polyphony!");
    return FLUID_FAILED; // Buffer full...
  }

  event->method = method;
  event->object = object;
  event->intparam = intparam;
  event->realparams[0] = realparam;
  if (handler->is_threadsafe)
    handler->queue_stored++;
  else
    fluid_rvoice_event_dispatch(event);
  return FLUID_OK;
}


int 
fluid_rvoice_eventhandler_push_ptr(fluid_rvoice_eventhandler_t* handler, 
                                   void* method, void* object, void* ptr)
{
  fluid_rvoice_event_t* event;
  fluid_rvoice_event_t local_event;
  event = handler->is_threadsafe ? 
    fluid_ringbuffer_get_inptr(handler->queue, handler->queue_stored) : &local_event;

  if (event == NULL) {
    FLUID_LOG(FLUID_WARN, "Ringbuffer full, try increasing polyphony!");
    return FLUID_FAILED; // Buffer full...
  }

  event->method = method;
  event->object = object;
  event->ptr = ptr;
  if (handler->is_threadsafe)
    handler->queue_stored++;
  else
    fluid_rvoice_event_dispatch(event);
  return FLUID_OK;
}


int 
fluid_rvoice_eventhandler_push5(fluid_rvoice_eventhandler_t* handler, 
                                void* method, void* object, int intparam, 
                                fluid_real_t r1, fluid_real_t r2, 
                                fluid_real_t r3, fluid_real_t r4, fluid_real_t r5)
{
  fluid_rvoice_event_t* event;
  fluid_rvoice_event_t local_event;
  event = handler->is_threadsafe ? 
    fluid_ringbuffer_get_inptr(handler->queue, handler->queue_stored) : &local_event;

  if (event == NULL) {
    FLUID_LOG(FLUID_WARN, "Ringbuffer full, try increasing polyphony!");
    return FLUID_FAILED; // Buffer full...
  }

  event->method = method;
  event->object = object;
  event->intparam = intparam;
  event->realparams[0] = r1;
  event->realparams[1] = r2;
  event->realparams[2] = r3;
  event->realparams[3] = r4;
  event->realparams[4] = r5;
  if (handler->is_threadsafe)
    handler->queue_stored++;
  else
    fluid_rvoice_event_dispatch(event);
  return FLUID_OK;
}


static void 
finished_voice_callback(void* userdata, fluid_rvoice_t* rvoice)
{
  fluid_rvoice_eventhandler_t* eventhandler = userdata;
  fluid_rvoice_t** vptr = fluid_ringbuffer_get_inptr(eventhandler->finished_voices, 0);
  if (vptr == NULL)
    return; // Buffer full
  *vptr = rvoice;
  fluid_ringbuffer_next_inptr(eventhandler->finished_voices, 1);
}

fluid_rvoice_eventhandler_t* 
new_fluid_rvoice_eventhandler(int is_threadsafe, int queuesize, 
  int finished_voices_size, int bufs, int fx_bufs, fluid_real_t sample_rate)
{
  fluid_rvoice_eventhandler_t* eventhandler = FLUID_NEW(fluid_rvoice_eventhandler_t);
  if (eventhandler == NULL) {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }
  eventhandler->mixer = NULL;
  eventhandler->queue = NULL;
  eventhandler->finished_voices = NULL;
  eventhandler->is_threadsafe = is_threadsafe;
  eventhandler->queue_stored = 0;
  
  eventhandler->finished_voices = new_fluid_ringbuffer(finished_voices_size,
                                                       sizeof(fluid_rvoice_t*));
  if (eventhandler->finished_voices == NULL)
    goto error_recovery;

  eventhandler->queue = new_fluid_ringbuffer(queuesize, sizeof(fluid_rvoice_event_t));
  if (eventhandler->queue == NULL)
    goto error_recovery;

  eventhandler->mixer = new_fluid_rvoice_mixer(bufs, fx_bufs, sample_rate); 
  if (eventhandler->mixer == NULL)
    goto error_recovery;
  fluid_rvoice_mixer_set_finished_voices_callback(eventhandler->mixer, 
                                        finished_voice_callback, eventhandler);
  return eventhandler;
  
error_recovery:
  delete_fluid_rvoice_eventhandler(eventhandler);
  return NULL;
}

int 
fluid_rvoice_eventhandler_dispatch_count(fluid_rvoice_eventhandler_t* handler)
{
  return fluid_ringbuffer_get_count(handler->queue);
}


/**
 * Call fluid_rvoice_event_dispatch for all events in queue
 * @return number of events dispatched
 */
int 
fluid_rvoice_eventhandler_dispatch_all(fluid_rvoice_eventhandler_t* handler)
{
  fluid_rvoice_event_t* event;
  int result = 0;
  while (NULL != (event = fluid_ringbuffer_get_outptr(handler->queue))) {
    fluid_rvoice_event_dispatch(event);
    result++;
    fluid_ringbuffer_next_outptr(handler->queue);   
  }
  return result;
}


void 
delete_fluid_rvoice_eventhandler(fluid_rvoice_eventhandler_t* handler)
{
  if (handler == NULL) return;
  delete_fluid_rvoice_mixer(handler->mixer);
  delete_fluid_ringbuffer(handler->queue);
  delete_fluid_ringbuffer(handler->finished_voices);
  FLUID_FREE(handler);
}
