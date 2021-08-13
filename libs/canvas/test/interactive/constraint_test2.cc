#include <iostream>

#include <gtkmm/adjustment.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>

#include "pbd/compose.h"

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

#define SQUARED 16

 struct Column {
	Column (Canvas* c, uint32_t num) : number (num) {
		box = new ConstraintPacker (c, Vertical);
		box->name = string_compose ("col%1", num);
		box->set_spacing (12);

		Pango::FontDescription font ("Sans");

		for (int i = 0; i < SQUARED; ++i) {
			rects[i] = new Rectangle (c);
			rects[i]->name = string_compose ("r%1-%2", number, i);
			rects[i]->set_size_request (8, 12);
			rects[i]->set_outline_color (0xff0000ff);
			rects[i]->set_fill_color (Gtkmm2ext::random_color());

			BoxConstrainedItem* b = box->pack_start (rects[i], PackOptions (PackExpand|PackFill));

			labels[i] = new Text (c);
			labels[i]->name = string_compose ("t%1-%2", number, i);
			labels[i]->set_font_description (font);
			labels[i]->set (labels[i]->name);
			labels[i]->set_fill_color (0x000000ff);

			ConstrainedItem* l = box->add_constrained (labels[i]);

			/* Note: the use of labels[i].width() here is
			 * equivalent to using a constant. This is not the same
			 * as l->width(), which is a constraint-solved
			 * variable. labels[i].width() is the pixel width of
			 * the current text contents of labels[i].
			 */

			l->centered_on (*b);
			l->add_constraint (l->width() == labels[i]->width());
			l->add_constraint (l->height() == labels[i]->height());

#if 0
			l->add_constraint (l->left() == b->center_x() - (labels[i]->width() / 2.));
			l->add_constraint (l->width() == labels[i]->width());
			l->add_constraint (l->right() == l->left() + labels[i]->width());

			l->add_constraint (l->top() == b->center_y() - (labels[i]->height() / 2));
			l->add_constraint (l->height() == labels[i]->height());
			l->add_constraint (l->bottom() == l->top() + l->height());
#endif
		}
	}

	ConstraintPacker* box;
	Rectangle* rects[SQUARED];
	Text* labels[SQUARED];
	uint32_t number;
};

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

	ConstraintPacker* main_hbox = new ConstraintPacker (c->root(), Horizontal);
	main_hbox->name = "main";
	main_hbox->set_spacing (12);
	main_hbox->set_margin (24);

	Column* cols[SQUARED];

	for (size_t i = 0; i < SQUARED; ++i) {
		cols[i] = new Column (c, i);
		main_hbox->pack_start (cols[i]->box, PackOptions (PackExpand|PackFill));
	}


#if 0
	Circle* circle = new Circle (c);
	circle->name = "circle";
	//circle->set_radius (30);
	circle->set_fill_color (Gtkmm2ext::random_color());
	circle->set_outline_color (Gtkmm2ext::random_color());

	ci = vbox->pack_start (circle, PackOptions (PackExpand|PackFill));
	ci->add_constraint (ci->height() == 0.5 * hb1->height());

	ConstraintPacker* hbox2 = new ConstraintPacker (c, Horizontal);
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



#if 0
/* code test arbitrary constraint layout */

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
#endif
