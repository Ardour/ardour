#ifndef PORTAUDIO_BACKEND_DEBUG_H
#define PORTAUDIO_BACKEND_DEBUG_H

#include "pbd/debug.h"

using namespace PBD;

#define DEBUG_AUDIO(msg) DEBUG_TRACE (DEBUG::BackendAudio, msg);
#define DEBUG_MIDI(msg) DEBUG_TRACE (DEBUG::BackendMIDI, msg);
#define DEBUG_TIMING(msg) DEBUG_TRACE (DEBUG::BackendTiming, msg);
#define DEBUG_THREADS(msg) DEBUG_TRACE (DEBUG::BackendThreads, msg);

#endif // PORTAUDIO_BACKEND_DEBUG_H
