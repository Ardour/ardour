#include "audiographer/utils.h"

using namespace AudioGrapher;

char const * Utils::zeros = 0;
unsigned long Utils::num_zeros = 0;

void
Utils::free_resources()
{
	num_zeros = 0;
	delete [] zeros;
	zeros = 0;
}