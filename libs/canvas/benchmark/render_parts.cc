#include <sys/time.h>
#include <pangomm/init.h>
#include "pbd/compose.h"
#include "pbd/xml++.h"
#include "canvas/group.h"
#include "canvas/canvas.h"
#include "canvas/root_group.h"
#include "canvas/rectangle.h"
#include "benchmark.h"

using namespace std;
using namespace ArdourCanvas;

class RenderParts : public Benchmark
{
public:
	RenderParts (string const & session) : Benchmark (session) {}

	void set_items_per_cell (int items)
	{
		_items_per_cell = items;
	}
	
	void do_run (ImageCanvas& canvas)
	{
		Group::default_items_per_cell = _items_per_cell;
	
		for (int i = 0; i < 1e4; i += 50) {
			canvas.render_to_image (Rect (i, 0, i + 50, 1024));
		}
	}

private:
	int _items_per_cell;
};

int main (int argc, char* argv[])
{
	if (argc < 2) {
		cerr << "Syntax: render_parts <session>\n";
		exit (EXIT_FAILURE);
	}

	Pango::init ();

	RenderParts render_parts (argv[1]);

	int tests[] = { 16, 32, 64, 128, 256, 512, 1024, 1e4, 1e5, 1e6 };

	for (unsigned int i = 0; i < sizeof (tests) / sizeof (int); ++i) {
		render_parts.set_items_per_cell (tests[i]);
		cout << tests[i] << " " << render_parts.run () << "\n";
	}

	return 0;
}

	
