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

#ifdef PLATFORM_WINDOWS
#define srandom() srand()
#endif

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

	srandom (time ((time_t *) 0));

	cview.set_size_request (100, 100);

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

	r1->set_size_request (20, 20);
	r2->set_size_request (30, 30);
	r3->set_size_request (40, 40);

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

	win.show_all ();
	app.run ();

	return 0;
}
