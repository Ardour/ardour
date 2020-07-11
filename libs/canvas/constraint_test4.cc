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

	srandom (time ((time_t) 0));

	win.add (cview);

	/* Make some items */

	Rectangle* r1 = new Rectangle (c);
	Rectangle* r2 = new Rectangle (c);
	Rectangle* r3 = new Rectangle (c);

	r1->set_fill_color (Gtkmm2ext::random_color());
	r2->set_fill_color (Gtkmm2ext::random_color());
	r3->set_fill_color (Gtkmm2ext::random_color());

	r1->name = "L";
	r2->name = "R";
	r3->name = "C";

	r1->set_intrinsic_size (20, 20);
	r2->set_intrinsic_size (30, 30);
	r3->set_intrinsic_size (40, 40);

	Text* txt = new Text (c);
	txt->name = "text";
	Pango::FontDescription font ("Sans");
	txt->set_font_description (font);
	txt->set ("hello world");

	Rectangle* bb = new Rectangle (c);
	bb->set_fill_color (Gtkmm2ext::random_color());

	Circle* circ = new Circle (c);
	circ->name = "circle";
	circ->set_fill_color (Gtkmm2ext::random_color());
	circ->set_outline_color (Gtkmm2ext::random_color());

	/* create a container */

	ConstraintPacker* packer = new ConstraintPacker (c->root());

	/* give it a minimum size */

	packer->set_intrinsic_size (100, 100);

	/* add stuff */

	ConstrainedItem* left = packer->add_constrained (r1);
	ConstrainedItem* right = packer->add_constrained (r2);
	ConstrainedItem* center = packer->add_constrained (r3);
	ConstrainedItem* text = packer->add_constrained (txt);
	ConstrainedItem* bens_box = packer->add_constrained (bb);
	ConstrainedItem* circle = packer->add_constrained (circ);

	/* first, constraints that connect an item dimension to the container dimensions or a constant */
	packer->constrain (left->left() == 0);
	packer->constrain (left->height() == packer->height);
	packer->constrain (left->top() == 0);
	packer->constrain (left->width() == 0.5 * packer->width);
	packer->constrain (right->right() == packer->width);
	packer->constrain (center->height() == 0.5 * packer->height);

	/* second, constraints that connect an item dimension to other items */
	center->right_of (*left, 50);
	right->right_of (*center);
	center->same_width_as (*right);
	right->same_width_as (*center);
	right->same_height_as (*left);
	center->top_aligned_with (*left);
	right->top_aligned_with (*center);

	/* XXX this needs to somehow move into ConstraintPacker but I currently
	 * see no way to build a constraint from a container of
	 * ConstrainedItems
	 */

	packer->constrain (left->width() + right->width() + center->width() +
	                   left->left_padding() + left->right_padding() +
	                   center->left_padding() + center->right_padding() +
	                   right->left_padding() + right->right_padding()
	                   == packer->width);

	/* Text at a fixed position */
	text->at (Duple (150, 50));
	/* Rectangle of fixed position and size */
	bens_box->box (Rect (40, 40, 80, 80));

	/* a circle sized and centered */
	circle->size (Duple (30, 30));
	circle->centered_on (*center);

	win.show_all ();
	app.run ();

	return 0;
}
