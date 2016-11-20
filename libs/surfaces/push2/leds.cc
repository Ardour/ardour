#include <algorithm>

#include "push2.h"

using namespace ArdourSurface;
using std::make_pair;
using std::max;
using std::min;

void
Push2::LED::set_color (uint8_t ci)
{
	_color_index = max (uint8_t(0), min (uint8_t(127), ci));
}

void
Push2::LED::set_state (LED::State s)
{
	_state = s;
}

