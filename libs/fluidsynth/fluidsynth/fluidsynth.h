#ifndef _FLUIDSYNTH_H
#define _FLUIDSYNTH_H

#include <stdio.h>

#if defined(COMPILER_MSVC)
#  define FLUIDSYNTH_API
#else
#  define FLUIDSYNTH_API  __attribute__ ((visibility ("hidden")))
#endif

#ifdef __cplusplus
extern "C" {
#endif


FLUIDSYNTH_API void fluid_version(int *major, int *minor, int *micro);
FLUIDSYNTH_API const char* fluid_version_str(void);


#include "types.h"
#include "settings.h"
#include "synth.h"
#include "sfont.h"
#include "event.h"
#include "midi.h"
#include "log.h"
#include "misc.h"
#include "mod.h"
#include "gen.h"
#include "voice.h"


#ifdef __cplusplus
}
#endif

#endif /* _FLUIDSYNTH_H */
