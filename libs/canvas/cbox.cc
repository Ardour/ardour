/*
 * Copyright (C) 2020 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <iostream>

#include "pbd/unwind.h"

#include "canvas/canvas.h"
#include "canvas/cbox.h"
#include "canvas/constrained_item.h"

using namespace ArdourCanvas;
using namespace kiwi;
using std::cerr;
using std::endl;

cBox::cBox (Canvas* c, Orientation o)
	: ConstraintPacker (c)
	, orientation (o)
	, _spacing (0)
	, _top_padding (0)
	, _bottom_padding (0)
	, _left_padding (0)
	, _right_padding (0)
	, _top_margin (0)
	, _bottom_margin (0)
	, _left_margin (0)
	, _right_margin (0)
	, collapse_on_hide (false)
	, homogenous (true)
{
}

cBox::cBox (Item* i, Orientation o)
	: ConstraintPacker (i)
	, orientation (o)
	, _spacing (0)
	, _top_padding (0)
	, _bottom_padding (0)
	, _left_padding (0)
	, _right_padding (0)
	, _top_margin (0)
	, _bottom_margin (0)
	, _left_margin (0)
	, _right_margin (0)
	, collapse_on_hide (false)
	, homogenous (true)
{
}

void
cBox::set_spacing (double s)
{
	_spacing = s;
}

void
cBox::set_padding (double top, double right, double bottom, double left)
{
	double last = top;

	_top_padding = last;

	if (right >= 0) {
		last = right;
	}
	_right_padding = last;

	if (bottom >= 0) {
		last = bottom;
	}
	_bottom_padding = last;

	if (left >= 0) {
		last = left;
	}
	_left_padding = last;
}

void
cBox::set_margin (double top, double right, double bottom, double left)
{
	double last = top;

	_top_margin = last;

	if (right >= 0) {
		last = right;
	}
	_right_margin = last;

	if (bottom >= 0) {
		last = bottom;
	}
	_bottom_margin = last;

	if (left >= 0) {
		last = left;
	}
	_left_margin = last;
}

void
cBox::remove (Item* item)
{
	for (Order::iterator t = order.begin(); t != order.end(); ++t) {
		if (&(*t)->item() == item) {
			order.erase (t);
			break;
		}
	}

	ConstraintPacker::remove (item);
}

ConstrainedItem*
cBox::add_constrained (Item* item)
{
	return pack (item, PackOptions (0), PackOptions (PackExpand|PackFill));
}

BoxConstrainedItem*
cBox::pack_start (Item* item, PackOptions primary_axis_opts, PackOptions secondary_axis_opts)
{
	return pack (item, PackOptions (primary_axis_opts|PackFromStart), secondary_axis_opts);
}

BoxConstrainedItem*
cBox::pack_end (Item* item, PackOptions primary_axis_opts, PackOptions secondary_axis_opts)
{
	return pack (item, PackOptions (primary_axis_opts|PackFromEnd), secondary_axis_opts);
}

BoxConstrainedItem*
cBox::pack (Item* item, PackOptions primary_axis_opts, PackOptions secondary_axis_opts)
{
	BoxConstrainedItem* ci =  new BoxConstrainedItem (*item, primary_axis_opts, secondary_axis_opts);

	add_constrained_internal (item, ci);
	order.push_back (ci);

	return ci;
}

void
cBox::preferred_size (Duple& min, Duple& natural) const
{
	Order::size_type n_expanding = 0;
	Order::size_type n_nonexpanding = 0;
	Order::size_type total = 0;
	Distance non_expanding_used = 0;
	Distance largest = 0;
	Distance largest_opposite = 0;
	Duple i_min, i_natural;

	cerr << "cbox::prefsize (" << (orientation == Vertical ? " vert) " : " horiz) ") << endl;

	for (Order::const_iterator o = order.begin(); o != order.end(); ++o) {

		(*o)->item().preferred_size (i_min, i_natural);

		cerr << '\t' << (*o)->item().debug_name() << " min " << i_min << " nat " << i_natural << endl;

		if ((*o)->primary_axis_pack_options() & PackExpand) {
			n_expanding++;

			if (orientation == Vertical) {
				if (i_natural.height() > largest) {
					largest = i_natural.height();
				}
				if (i_natural.width() > largest) {
					largest_opposite = i_natural.width();
				}
			} else {
				if (i_natural.width() > largest) {
					largest = i_natural.width();
				}
				if (i_natural.height() > largest) {
					largest_opposite = i_natural.height();
				}
			}

		} else {
			n_nonexpanding++;

			if (orientation == Vertical) {
				if (i_natural.height() > 0) {
					non_expanding_used += i_natural.height();
				} else {
					non_expanding_used += i_min.height();
				}
			} else {
				if (i_natural.width() > 0) {
					non_expanding_used += i_natural.width();
				} else {
					non_expanding_used += i_min.width();
				}
			}
		}
		total++;
	}

	Duple r;

	if (orientation == Vertical) {
		cerr << "+++ vertical box, neu = " << non_expanding_used << " largest = " << largest << " opp " << largest_opposite << " total " << total << endl;
		min.x = non_expanding_used + (n_expanding * largest_opposite) + _left_margin + _right_margin + ((total - 1) * _spacing);
		min.y = non_expanding_used + (n_expanding * largest) + _top_margin + _bottom_margin + ((total - 1) * _spacing);
	} else {
		cerr << "+++ horiz box, neu = " << non_expanding_used << " largest = " << largest << " opp " << largest_opposite << " total " << total << endl;
		min.x = non_expanding_used + (n_expanding * largest) + _left_margin + _right_margin + ((total - 1) * _spacing);
		min.y = non_expanding_used + (n_expanding * largest_opposite) + _top_margin + _bottom_margin + ((total - 1) * _spacing);

	}

	cerr << "++++ " << debug_name() << " rpref " << min << endl;

	natural = min;
}

void
cBox::size_allocate (Rect const & r)
{
	PBD::Unwinder<bool> uw (in_alloc, true);

	Item::size_allocate (r);

	kiwi::Solver solver;

	double expanded_size;
	Order::size_type n_expanding = 0;
	Order::size_type n_nonexpanding = 0;
	Order::size_type total = 0;
	Distance non_expanding_used = 0;

	for (Order::iterator o = order.begin(); o != order.end(); ++o) {
		if ((*o)->primary_axis_pack_options() & PackExpand) {
			n_expanding++;
		} else {
			n_nonexpanding++;

			Duple min, natural;

			(*o)->item().preferred_size (min, natural);

			if (orientation == Vertical) {
				non_expanding_used += natural.height();
			} else {
				non_expanding_used += natural.width();
			}

		}
		total++;
	}

	if (orientation == Vertical) {
		expanded_size = (r.height() - _top_margin - _bottom_margin - ((total - 1) * _spacing) - non_expanding_used) / n_expanding;
	} else {
		expanded_size = (r.width() - _left_margin - _right_margin - ((total - 1) * _spacing) - non_expanding_used) / n_expanding;
	}

	cerr << "\n\n\n" << debug_name() << " SIZE-ALLOC " << r << " expanded items (" << n_expanding << ")will be " << expanded_size << " neu " << non_expanding_used << " t = " << total << " s " << _spacing << '\n';

	Order::size_type n = 0;
	Order::iterator prev = order.end();
	try {
		for (Order::iterator o = order.begin(); o != order.end(); ++o, ++n) {

			Duple min, natural;
			(*o)->item().preferred_size (min, natural);

			cerr << "\t" << (*o)->item().debug_name() << " min " << min << " nat " << natural << endl;

			/* setup center_{x,y} variables in case calling/using
			 * code wants to use them for additional constraints
			 */

			solver.addConstraint ((*o)->center_x() == (*o)->left() + ((*o)->width() / 2.));
			solver.addConstraint ((*o)->center_y() == (*o)->top() + ((*o)->height() / 2.));

			/* Add constraints that will size the item within this box */

			if (orientation == Vertical) {

				/* set up constraints for expand/fill options, done by
				 * adjusting height and margins of each item
				 */

				if ((*o)->primary_axis_pack_options() & PackExpand) {

					/* item will take up more than it's natural
					 * size, if space is available
					 */

					if ((*o)->primary_axis_pack_options() & PackFill) {

						/* item is expanding to fill all
						 * available space and wants that space
						 * for itself.
						 */

						solver.addConstraint ((*o)->height() == expanded_size | kiwi::strength::strong);
						solver.addConstraint ((*o)->top_padding() == 0. | kiwi::strength::strong);
						solver.addConstraint ((*o)->bottom_padding() == 0. | kiwi::strength::strong);

					} else {

						/* item is expanding to fill all
						 * available space and wants that space
						 * as padding
						 */

						solver.addConstraint ((*o)->height() == natural.height());
						solver.addConstraint ((*o)->top_padding() + (*o)->bottom_padding() + (*o)->height() == expanded_size | kiwi::strength::strong);
						solver.addConstraint ((*o)->bottom_padding() == (*o)->top_padding() | kiwi::strength::strong);
					}

				} else {

					/* item is not going to expand to fill
					 * available space. just give it's preferred
					 * height.
					 */

					cerr << (*o)->item().debug_name() << " will use natural height of " << natural.height() << endl;
					solver.addConstraint ((*o)->height() == natural.height());
					solver.addConstraint ((*o)->top_padding() == 0.);
					solver.addConstraint ((*o)->bottom_padding() == 0.);
				}


				/* now set upper left corner of the item */

				if (n == 0) {

					/* first item */

					solver.addConstraint ((*o)->top() == _top_margin + (*o)->top_padding() | kiwi::strength::strong);

				} else {
					/* subsequent items */

					solver.addConstraint ((*o)->top() == (*prev)->bottom() + (*prev)->bottom_padding() + (*o)->top_padding() + _spacing | kiwi::strength::strong);
				}

				/* set the side-effect variables and/or constants */

				solver.addConstraint ((*o)->left() + (*o)->width() == (*o)->right()| kiwi::strength::strong);
				solver.addConstraint ((*o)->bottom() == (*o)->top() + (*o)->height());
				solver.addConstraint ((*o)->left() == _left_margin + (*o)->left_padding() | kiwi::strength::strong);

				if (!((*o)->secondary_axis_pack_options() & PackExpand) && natural.width() > 0) {
					cerr << "\t\t also using  natural width of " << natural.width() << endl;
					solver.addConstraint ((*o)->width() == natural.width());
				} else {
					cerr << "\t\t also using container width of " << r.width() << endl;
					solver.addConstraint ((*o)->width() == r.width() - (_left_margin + _right_margin + (*o)->right_padding()) | kiwi::strength::strong);
				}


			} else {

				/* set up constraints for expand/fill options, done by
				 * adjusting width and margins of each item
				 */

				if ((*o)->primary_axis_pack_options() & PackExpand) {

					/* item will take up more than it's natural
					 * size, if space is available
					 */

					if ((*o)->primary_axis_pack_options() & PackFill) {

						/* item is expanding to fill all
						 * available space and wants that space
						 * for itself.
						 */

						solver.addConstraint ((*o)->width() == expanded_size | kiwi::strength::strong);
						solver.addConstraint ((*o)->left_padding() == 0. | kiwi::strength::strong);
						solver.addConstraint ((*o)->right_padding() == 0. | kiwi::strength::strong);

					} else {

						/* item is expanding to fill all
						 * available space and wants that space
						 * as padding
						 */

						solver.addConstraint ((*o)->width() == natural.width());
						solver.addConstraint ((*o)->left_padding() + (*o)->right_padding() + (*o)->width() == expanded_size | kiwi::strength::strong);
						solver.addConstraint ((*o)->left_padding() == (*o)->right_padding() | kiwi::strength::strong);
					}

				} else {

					/* item is not going to expand to fill
					 * available space. just give it's preferred
					 * width.
					 */

					solver.addConstraint ((*o)->width() == natural.width());
					solver.addConstraint ((*o)->left_padding() == 0.);
					solver.addConstraint ((*o)->right_padding() == 0.);
				}


				/* now set upper left corner of the item */

				if (n == 0) {

					/* first item */

					solver.addConstraint ((*o)->left() == _left_margin + (*o)->left_padding() | kiwi::strength::strong);

				} else {
					/* subsequent items */

					solver.addConstraint ((*o)->left() == (*prev)->right() + (*prev)->right_padding() + (*o)->left_padding() + _spacing | kiwi::strength::strong);
				}

				/* set the side-effect variables and/or constants */

				solver.addConstraint ((*o)->bottom() == (*o)->top() + (*o)->height());
				solver.addConstraint ((*o)->right() == (*o)->left() + (*o)->width());
				solver.addConstraint ((*o)->top() == _top_margin + (*o)->top_padding() | kiwi::strength::strong);

				if (!((*o)->secondary_axis_pack_options() & PackExpand) && natural.height() > 0) {
					cerr << "\t\tand natural height of " << natural.height() << endl;
					solver.addConstraint ((*o)->height() == natural.height());
				} else {
					cerr << "\t\tand container height of " << r.height() << endl;
					solver.addConstraint ((*o)->height() == r.height() - (_top_margin + _bottom_margin + (*o)->bottom_padding()) | kiwi::strength::strong);
				}
			}

			/* Add constraints that come with the item */

			std::vector<Constraint> const & constraints ((*o)->constraints());

			for (std::vector<Constraint>::const_iterator c = constraints.begin(); c != constraints.end(); ++c) {
				solver.addConstraint (*c);
			}

			prev = o;
		}

	} catch (std::exception& e) {
		cerr << "Setting up sovler failed: " << e.what() << endl;
		return;
	}


	solver.updateVariables ();
	//solver.dump (cerr);

	for (Order::iterator o = order.begin(); o != order.end(); ++o, ++n) {
		(*o)->dump (cerr);
	}

	apply (&solver);

	_bounding_box_dirty = true;

}

void
cBox::child_changed (bool bbox_changed)
{
}
