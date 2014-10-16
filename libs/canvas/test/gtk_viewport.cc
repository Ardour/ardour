#include <iostream>
#include <gtkmm.h>
#include "canvas/canvas.h"
#include "canvas/rectangle.h"

using namespace std;
using namespace ArdourCanvas;

Gtk::Adjustment* hadj;
Gtk::Adjustment* vadj;

void
left_clicked ()
{
	hadj->set_value (hadj->get_value() - 64);
}

void
right_clicked ()
{
	hadj->set_value (hadj->get_value() + 64);
}

int main (int argc, char* argv[])
{
	Gtk::Main kit (argc, argv);

	Gtk::Window window;
	window.set_title ("Hello world");
	GtkCanvas canvas;

	Rectangle a (canvas.root(), Rect (64, 64, 128, 128));
	a.set_outline_color (0xff0000aa);
	Rectangle b (canvas.root(), Rect (64, 64, 128, 128));
	b.set_position (Duple (256, 256));
	b.set_outline_width (4);
	b.set_outline_color (0x00ff00ff);

	Gtk::HBox button_box;
	
	Gtk::Button left ("Left");
	left.signal_clicked().connect (sigc::ptr_fun (&left_clicked));
	button_box.pack_start (left);

	Gtk::Button right ("Right");
	right.signal_clicked().connect (sigc::ptr_fun (&right_clicked));
	button_box.pack_start (right);

	hadj = new Gtk::Adjustment (0, 0, 1e3);
	vadj = new Gtk::Adjustment (0, 0, 1e3);
	
	Gtk::Viewport viewport (*hadj, *vadj);
	viewport.add (canvas);

	Gtk::VBox overall_box;
	overall_box.pack_start (viewport);
	overall_box.pack_start (button_box, false, false);
	
	window.add (overall_box);
	canvas.show ();
	window.show_all ();
	
	Gtk::Main::run (window);
	return 0;
}
