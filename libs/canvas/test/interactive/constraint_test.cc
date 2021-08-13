#include <iostream>

#include <gtkmm/adjustment.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>

#include "gtkmm2ext/colors.h"

#include "canvas/box.h"
#include "canvas/canvas.h"
#include "canvas/circle.h"
#include "canvas/constrained_item.h"
#include "canvas/constraint_packer.h"
#include "canvas/rectangle.h"
#include "canvas/text.h"

using namespace ArdourCanvas;
using namespace Gtk;
using std::cerr;
using std::endl;

int
main (int argc, char* argv[])
{
	Gtk::Main app (&argc, &argv);

	Gtk::Window win;
	Gtk::Adjustment hadj (0, 0, 1000, 1, 10);
	Gtk::Adjustment vadj (0, 0, 1000, 1, 10);
	GtkCanvasViewport cview (hadj, vadj);
	Canvas* c = cview.canvas ();

	c->set_background_color (0xffffffff);

	// cview.set_size_request (100, 100);

	win.add (cview);

	Rectangle* r1 = new Rectangle (c);
	Rectangle* r2 = new Rectangle (c);
	Rectangle* r3 = new Rectangle (c);

	r1->set_fill_color (Gtkmm2ext::random_color());
	r2->set_fill_color (Gtkmm2ext::random_color());
	r3->set_fill_color (Gtkmm2ext::random_color());

	r1->name = "r1";
	r2->name = "r2";
	r3->name = "r3";

	//r1->set_size_request (20, 20);
	//r2->set_size_request (30, 30);
	//r3->set_size_request (40, 40);

	ConstraintPacker* vbox = new ConstraintPacker (c->root(), Vertical);
	vbox->name = "vbox";
	vbox->set_fill (true);
	vbox->set_fill_color (0xff0000ff);
	vbox->set_margin (20);

	vbox->pack_start (r1,  PackOptions(PackExpand|PackFill));
	vbox->pack_start (r2, PackOptions(PackExpand|PackFill));
	vbox->pack_start (r3, PackOptions(PackExpand|PackFill));

	ConstraintPacker* hbox1 = new ConstraintPacker (c, Horizontal);
	hbox1->name = "hbox1";
	hbox1->set_fill (true);
	hbox1->set_fill_color (0x00ff00ff);

	hbox1->set_margin (10);

	Rectangle* r4 = new Rectangle (c);
	Rectangle* r5 = new Rectangle (c);
	Rectangle* r6 = new Rectangle (c);

	r4->set_fill_color (Gtkmm2ext::random_color());
	r5->set_fill_color (Gtkmm2ext::random_color());
	r6->set_fill_color (Gtkmm2ext::random_color());

	r4->name = "r4";
	r5->name = "r5";
	r6->name = "r6";

	ConstrainedItem* ci4 = hbox1->pack_start (r4, PackOptions(PackExpand|PackFill));
	hbox1->pack_start (r5, PackOptions(PackExpand|PackFill));
	hbox1->pack_start (r6, PackOptions(PackExpand|PackFill));

	BoxConstrainedItem* hb1;
	BoxConstrainedItem* ci;

	hb1 = vbox->pack_start (hbox1, PackOptions (PackExpand|PackFill));

	ci4->add_constraint (ci4->width() == hb1->width() / 2.);

	Circle* circle = new Circle (c);
	circle->name = "circle";
	//circle->set_radius (30);
	circle->set_fill_color (Gtkmm2ext::random_color());
	circle->set_outline_color (Gtkmm2ext::random_color());

	ci = vbox->pack_start (circle, PackOptions (PackExpand|PackFill));
	ci->add_constraint (ci->height() == 0.5 * hb1->height());
	ci->add_constraint (ci->center_x() == ci4->center_x());
	ci->add_constraint (ci->top_padding() == 10);
	ci->add_constraint (ci->bottom_padding() == 10);

	ConstraintPacker* hbox2 = new ConstraintPacker (c, Horizontal);
	hbox2->name = "hbox2";
	hbox2->set_fill (true);
	hbox2->set_fill_color (Gtkmm2ext::random_color());
	hbox2->set_outline (true);

	Text* txt = new Text (c);
	txt->name = "text";

	Pango::FontDescription font ("Sans");

	txt->set_font_description (font);
	txt->set ("hello world");

	ConstrainedItem* hb2 = vbox->pack_start (hbox2, PackOptions (PackExpand|PackFill));
	ConstrainedItem* ti = hbox2->pack_start (txt, PackOptions (PackExpand), PackOptions (0));

	ti->add_constraint (ti->center_x() == hb2->center_x());
	ti->add_constraint (ti->center_y() == hb2->center_y());

	win.show_all ();
	app.run ();

	return 0;
}
