#include <gtkmm/adjustment.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>

#include "canvas/canvas.h"
#include "canvas/constraint_packer.h"

using namespace ArdourCanvas;
using namespace Gtk;

int
main (int argc, char* argv[])
{
	Gtk::Main app (&argc, &argv);

	Gtk::Window win;
	Gtk::Adjustment hadj (0, 0, 1000, 1, 10);
	Gtk::Adjustment vadj (0, 0, 1000, 1, 10);
	GtkCanvasViewport cview (hadj, vadj);

	win.add (cview);
	win.show_all ();

	app.run ();

	return 0;
}
