#include <iostream>

#include <gtkmm/adjustment.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>

#include "gtkmm2ext/colors.h"

#include "canvas/box.h"
#include "canvas/canvas.h"
#include "canvas/cbox.h"
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

	srandom (time ((time_t) 0));

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

	//r1->set_intrinsic_size (20, 20);
	//r2->set_intrinsic_size (30, 30);
	//r3->set_intrinsic_size (40, 40);

//#define FULL_PACKER
#define CBOX_PACKER

#ifdef FULL_PACKER
	ConstraintPacker* packer = new ConstraintPacker (c->root());

	ConstrainedItem* left = packer->add_constrained (r1);
	ConstrainedItem* right = packer->add_constrained (r2);
	ConstrainedItem* center = packer->add_constrained (r3);

	/* x-axis */

	packer->constrain (left->left() == 0);
	packer->constrain (center->left() == left->right());
	packer->constrain (right->left() == center->right());

	packer->constrain (left->width() == packer->width * 0.4);
	packer->constrain (center->width() == packer->width * 0.1);
	packer->constrain (left->width() + right->width() + center->width() == packer->width);

	packer->constrain (left->right() == left->left() + left->width());
	packer->constrain (right->right() == right->left() + right->width());
	packer->constrain (center->right() == center->left() + center->width());

	/* y-axis */

	packer->constrain (left->top() == 0);
	packer->constrain (right->top() == left->top());
	packer->constrain (center->top() == left->top());

	packer->constrain (left->height() == packer->height);
	packer->constrain (right->height() == left->height());
	packer->constrain (center->height() == left->height());

	packer->constrain (left->bottom() == left->top() + left->height());
	packer->constrain (center->bottom() == center->top() + center->height());
	packer->constrain (right->bottom() == right->top() + right->height());

#elif defined(CBOX_PACKER)

	cBox* vbox = new cBox (c->root(), Vertical);
	vbox->name = "vbox";

	vbox->set_margin (10, 20, 30, 40);

	vbox->pack_start (r1,  PackOptions(PackExpand|PackFill));
	vbox->pack_start (r2, PackOptions(PackExpand|PackFill));
	vbox->pack_start (r3, PackOptions(PackExpand|PackFill));

	cBox* hbox1 = new cBox (c, Horizontal);
	hbox1->name = "hbox1";

	hbox1->set_margin (10, 10, 10, 10);

	Rectangle* r4 = new Rectangle (c);
	Rectangle* r5 = new Rectangle (c);
	Rectangle* r6 = new Rectangle (c);

	r4->set_fill_color (Gtkmm2ext::random_color());
	r5->set_fill_color (Gtkmm2ext::random_color());
	r6->set_fill_color (Gtkmm2ext::random_color());

	r4->name = "r4";
	r5->name = "r5";
	r6->name = "r6";
	hbox1->pack_start (r4, PackOptions(PackExpand|PackFill));
	hbox1->pack_start (r5, PackOptions(PackExpand|PackFill));
	hbox1->pack_start (r6, PackOptions(PackExpand|PackFill));

	BoxConstrainedItem* hb1;
	ConstrainedItem* ci;

	hb1 = vbox->pack_start (hbox1, PackOptions (PackExpand|PackFill));

	Circle* circle = new Circle (c);
	circle->name = "circle";
	//circle->set_radius (30);
	circle->set_fill_color (Gtkmm2ext::random_color());
	circle->set_outline_color (Gtkmm2ext::random_color());

	ci = vbox->pack_start (circle, PackOptions (PackExpand|PackFill));
	ci->add_constraint (ci->height() == 0.5 * hb1->height());

	cBox* hbox2 = new cBox (c, Horizontal);
	hbox2->name = "hbox2";
	hbox2->set_fill (true);
	hbox2->set_fill_color (Gtkmm2ext::random_color());

	Text* txt = new Text (c);
	txt->name = "text";

	Pango::FontDescription font ("Sans");

	txt->set_font_description (font);
	txt->set ("hello, world");

	ConstrainedItem* ti = hbox2->pack_start (txt, PackExpand);
	ti->add_constraint (ti->left() == 25);

	vbox->pack_start (hbox2, PackOptions (PackExpand|PackFill));

#endif

	win.show_all ();
	app.run ();

	return 0;
}
