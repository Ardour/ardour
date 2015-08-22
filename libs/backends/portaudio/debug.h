#ifndef PORTAUDIO_BACKEND_DEBUG_H
#define PORTAUDIO_BACKEND_DEBUG_H

#include "ardour/debug.h"

#define DEBUG_AUDIO(msg) DEBUG_TRACE (PBD::DEBUG::BackendAudio, msg);
#define DEBUG_MIDI(msg) DEBUG_TRACE (PBD::DEBUG::BackendMIDI, msg);
#define DEBUG_TIMING(msg) DEBUG_TRACE (PBD::DEBUG::BackendTiming, msg);
#define DEBUG_THREADS(msg) DEBUG_TRACE (PBD::DEBUG::BackendThreads, msg);
#define DEBUG_PORTS(msg) DEBUG_TRACE (PBD::DEBUG::BackendPorts, msg);

#endif // PORTAUDIO_BACKEND_DEBUG_H
