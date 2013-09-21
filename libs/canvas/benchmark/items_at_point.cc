#include <sys/time.h>
#include "canvas/group.h"
#include "canvas/canvas.h"
#include "canvas/root_group.h"
#include "canvas/rectangle.h"
#include "benchmark.h"

using namespace std;
using namespace ArdourCanvas;

static void
test (int items_per_cell)
{
	Group::default_items_per_cell = items_per_cell;
	
	int const n_rectangles = 10000;
	int const n_tests = 1000;
	double const rough_size = 1000;
	srand (1);

	ImageCanvas canvas;

	list<Item*> rectangles;

	for (int i = 0; i < n_rectangles; ++i) {
		rectangles.push_back (new Rectangle (canvas.root(), rect_random (rough_size)));
	}

	for (int i = 0; i < n_tests; ++i) {
		Duple test (double_random() * rough_size, double_random() * rough_size);

		/* ask the group what's at this point */
		vector<Item const *> items;
		canvas.root()->add_items_at_point (test, items);
	}
}

int main ()
{
	int tests[] = { 1, 2, 4, 8, 16, 32, 64, 128, 256 };

	for (unsigned int i = 0; i < sizeof (tests) / sizeof (int); ++i) {
		timeval start;
		timeval stop;
		
		gettimeofday (&start, 0);
		test (tests[i]);
		gettimeofday (&stop, 0);

		int sec = stop.tv_sec - start.tv_sec;
		int usec = stop.tv_usec - start.tv_usec;
		if (usec < 0) {
			--sec;
			usec += 1e6;
		}

		double seconds = sec + ((double) usec / 1e6);

		cout << "Test " << tests[i] << ": " << seconds << "\n";
	}
}
	
