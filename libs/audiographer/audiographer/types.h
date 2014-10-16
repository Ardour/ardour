#ifndef AUDIOGRAPHER_TYPES_H
#define AUDIOGRAPHER_TYPES_H

#include <stdint.h>

#include "audiographer/visibility.h"

namespace AudioGrapher {

/* XXX: copied from libardour */	
typedef int64_t framecnt_t;
	
typedef uint8_t ChannelCount;

typedef float DefaultSampleType;

} // namespace

#endif // AUDIOGRAPHER_TYPES_H
