#include <iostream>
#include <gtkmm.h>
#include "canvas/canvas.h"
#include "canvas/rectangle.h"

using namespace std;
using namespace ArdourCanvas;

Rectangle* rectangle[2];

void
bigger_clicked ()
{
	Rect r = rectangle[0]->get ();
	r.x1 += 16;
	r.y1 += 16;
	rectangle[0]->set (r);
}

void
smaller_clicked ()
{
	Rect r = rectangle[0]->get ();
	r.x1 -= 16;
	r.y1 -= 16;
	rectangle[0]->set (r);
}

void
left_clicked ()
{
	Duple p = rectangle[0]->position ();
	p.x -= 16;
	rectangle[0]->set_position (p);
}

void
right_clicked ()
{
	Duple p = rectangle[0]->position ();
	p.x += 16;
	rectangle[0]->set_position (p);
}

int main (int argc, char* argv[])
{
	Gtk::Main kit (argc, argv);

	Gtk::Window window;
	window.set_title ("Hello world");
	window.set_size_request (512, 512);
	GtkCanvas canvas;
	canvas.set_size_request (2048, 2048);

	rectangle[0] = new Rectangle (canvas.root(), Rect (64, 64, 128, 128));
	rectangle[0]->set_outline_color (0xff0000aa);
	rectangle[1] = new Rectangle (canvas.root(), Rect (64, 64, 128, 128));
	rectangle[1]->set_position (Duple (256, 256));
	rectangle[1]->set_outline_width (4);
	rectangle[1]->set_outline_color (0x00ff00ff);
	rectangle[1]->set_fill (true);
	rectangle[1]->set_fill_color (0x00ffffff);
	rectangle[1]->set_outline_what ((Rectangle::What) (Rectangle::LEFT | Rectangle::RIGHT));

	Gtk::VBox overall_box;

	Gtk::ScrolledWindow scroller;
	scroller.add (canvas);
	overall_box.pack_start (scroller);

	Gtk::HBox button_box;
	
	Gtk::Button bigger ("Bigger");
	bigger.signal_clicked().connect (sigc::ptr_fun (&bigger_clicked));
	button_box.pack_start (bigger);

	Gtk::Button smaller ("Smaller");
	smaller.signal_clicked().connect (sigc::ptr_fun (&smaller_clicked));
	button_box.pack_start (smaller);

	Gtk::Button left ("Left");
	left.signal_clicked().connect (sigc::ptr_fun (&left_clicked));
	button_box.pack_start (left);

	Gtk::Button right ("Right");
	right.signal_clicked().connect (sigc::ptr_fun (&right_clicked));
	button_box.pack_start (right);
	
	overall_box.pack_start (button_box, false, false);
	
	window.add (overall_box);
	canvas.show ();
	window.show_all ();

	Gtk::Main::run (window);
	return 0;
}
