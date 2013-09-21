#include <iostream>
#include <gtkmm.h>
#include "canvas/canvas.h"
#include "canvas/rectangle.h"
#include "canvas/pixbuf.h"

using namespace std;
using namespace ArdourCanvas;

Rectangle* rectangle = 0;

bool
event (GdkEvent* ev)
{
	static bool dragging = false;
	static Duple offset;
	
	if (ev->type == GDK_BUTTON_PRESS) {
		GdkEventButton* b = reinterpret_cast<GdkEventButton*> (ev);
		if (b->button == 1) {
			dragging = true;
			offset = Duple (b->x, b->y) - rectangle->position ();
			rectangle->grab ();
			cout << "Dragging offset=" << offset << "\n";
		}
	} else if (ev->type == GDK_BUTTON_RELEASE) {
		GdkEventButton* b = reinterpret_cast<GdkEventButton*> (ev);
		cout << "Release.\n";
		if (b->button == 1) {
			dragging = false;
			rectangle->ungrab ();
			cout << "Drag complete.\n";
		}
	} else if (ev->type == GDK_MOTION_NOTIFY) {
		GdkEventMotion* m = reinterpret_cast<GdkEventMotion*> (ev);
		if (dragging) {
			rectangle->set_position (Duple (m->x, m->y) - offset);
			cout << "Move to " << (Duple (m->x, m->y) - offset) << "\n";
		}
	}
			
	return true;
}

int main (int argc, char* argv[])
{
	Gtk::Main kit (argc, argv);

	Gtk::Window window;
	window.set_title ("Hello world");
	window.set_size_request (768, 768);

	Gtk::Adjustment hadj (0, 0, 1e3);
	Gtk::Adjustment vadj (0, 0, 1e3);
	GtkCanvasViewport viewport (hadj, vadj);
	GtkCanvas* canvas = viewport.canvas ();

	rectangle = new Rectangle (canvas->root(), Rect (64, 64, 128, 128));
	rectangle->set_outline_color (0xff0000aa);
	rectangle->Event.connect (sigc::ptr_fun (event));

	window.add (viewport);
	canvas->show ();
	window.show_all ();
	
	Gtk::Main::run (window);
	return 0;
}
