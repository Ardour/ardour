#ifndef __jack_vst_h__
#define __jack_vst_h__

#include <sys/types.h>
#include <sys/time.h>
#include <jack/jack.h>
#include <jack/ringbuffer.h>
#include <fst.h>
#ifdef HAVE_ALSA
#include <alsa/asoundlib.h>
#endif

typedef struct _JackVST JackVST;

struct _JackVST {
    jack_client_t *client;
    VSTHandle *    handle;
    VSTState *     fst;
    float        **ins;
    float        **outs;
    jack_port_t  *midi_port;
    jack_port_t  **inports;
    jack_port_t  **outports;
    void*          userdata;
    int            bypassed;
    int            muted;
    int		   current_program;

    /* For VST/i support */

    int want_midi;
    pthread_t          midi_thread;
#ifdef HAVE_ALSA
    snd_seq_t*         seq;
#endif
    int                midiquit;
    jack_ringbuffer_t* event_queue;
    struct VstEvents*  events;
};

#define MIDI_EVENT_MAX 1024

#endif /* __jack_vst_h__ */
