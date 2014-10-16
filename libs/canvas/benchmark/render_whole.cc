#include <sys/time.h>
#include <pangomm/init.h>
#include "pbd/xml++.h"
#include "pbd/compose.h"
#include "canvas/canvas.h"
#include "canvas/types.h"
#include "benchmark.h"

using namespace std;
using namespace ArdourCanvas;

class RenderWhole : public Benchmark
{
public:
	RenderWhole (string const & session) : Benchmark (session) {}

	void do_run (ImageCanvas& canvas)
	{
		canvas.render_to_image (Rect (0, 0, 4096, 1024));
	}

	void finish (ImageCanvas& canvas)
	{
		canvas.write_to_png ("session.png");
	}
};

int main (int argc, char* argv[])
{
	if (argc < 2) {
		cerr << "Syntax: render_whole <session-name> [<number-of-iterations>]\n";
		exit (EXIT_FAILURE);
	}

	Pango::init ();

	RenderWhole render_whole (argv[1]);

	if (argc > 2) {
		render_whole.set_iterations (atoi (argv[2]));
	}
	
	cout << render_whole.run () << "\n";
	
	return 0;
}
