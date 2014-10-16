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

class RenderFromLog : public Benchmark
{
public:
	RenderFromLog (string const & session) : Benchmark (session) {}

	void set_items_per_cell (int items)
	{
		_items_per_cell = items;
	}
	
	void do_run (ImageCanvas& canvas)
	{
		Group::default_items_per_cell = _items_per_cell;
		canvas.set_log_renders (false);

		list<Rect> const & renders = canvas.renders ();
		
		for (list<Rect>::const_iterator i = renders.begin(); i != renders.end(); ++i) {
			canvas.render_to_image (*i);
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

	RenderFromLog render_from_log (argv[1]);

//	int tests[] = { 16, 32, 64, 128, 256, 512, 1024, 1e4, 1e5, 1e6 };
	int tests[] = { 16 };

	for (unsigned int i = 0; i < sizeof (tests) / sizeof (int); ++i) {
		render_from_log.set_items_per_cell (tests[i]);
		cout << tests[i] << " " << render_from_log.run () << "\n";
	}

	return 0;
}

	
