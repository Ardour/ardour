#include "audiographer/routines.h"

namespace AudioGrapher
{
Routines::compute_peak_t Routines::_compute_peak = &Routines::default_compute_peak;
Routines::apply_gain_to_buffer_t Routines::_apply_gain_to_buffer = &Routines::default_apply_gain_to_buffer;
}
