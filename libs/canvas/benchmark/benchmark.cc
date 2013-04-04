#include <sys/time.h>
#include "pbd/compose.h"
#include "canvas/types.h"
#include "canvas/canvas.h"
#include "benchmark.h"

using namespace std;
using namespace ArdourCanvas;

double
double_random ()
{
	return ((double) rand() / RAND_MAX);
}

ArdourCanvas::Rect
rect_random (double rough_size)
{
	double const x = double_random () * rough_size / 2;
	double const y = double_random () * rough_size / 2;
	double const w = double_random () * rough_size / 2;
	double const h = double_random () * rough_size / 2;
	return Rect (x, y, x + w, y + h);
}

Benchmark::Benchmark (string const & session)
	: _iterations (1)
{
	string path = string_compose ("../../libs/canvas/benchmark/sessions/%1.xml", session);
	_canvas = new ImageCanvas (new XMLTree (path), Duple (4096, 4096));
}

void
Benchmark::set_iterations (int n)
{
	_iterations = n;
}

/** @return wallclock time in seconds */
double
Benchmark::run ()
{
	timeval start;
	gettimeofday (&start, 0);

	for (int i = 0; i < _iterations; ++i) {
		do_run (*_canvas);
	}

	timeval stop;
	gettimeofday (&stop, 0);

	finish (*_canvas);

	int sec = stop.tv_sec - start.tv_sec;
	int usec = stop.tv_usec - start.tv_usec;
	if (usec < 0) {
		--sec;
		usec += 1e6;
	}

	return sec + ((double) usec / 1e6);
}
