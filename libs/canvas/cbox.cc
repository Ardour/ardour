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
	_solver.addEditVariable (expanded_item_size, kiwi::strength::strong);
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
	_solver.addEditVariable (expanded_item_size, kiwi::strength::strong);
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

	for (Order::const_iterator o = order.begin(); o != order.end(); ++o) {

		(*o)->item().preferred_size (i_min, i_natural);

		// cerr << '\t' << (*o)->item().whoami() << " min " << i_min << " nat " << i_natural << endl;

		if ((*o)->primary_axis_pack_options() & PackExpand) {
			n_expanding++;

			if (orientation == Vertical) {
				if (i_natural.height() > largest) {
					largest = i_natural.height();
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
				non_expanding_used += i_natural.height();
			} else {
				non_expanding_used += i_natural.width();
			}
		}

		/* determine the maximum size for the opposite axis. All items
		 * will be this size or less on this axis
		 */

		if (orientation == Vertical) {
			if (i_natural.width() > largest_opposite) {
				largest_opposite = i_natural.width();
			}
		} else {
			if (i_natural.height() > largest_opposite) {
				largest_opposite = i_natural.height();
			}
		}

		total++;
	}

	Duple r;

	if (orientation == Vertical) {
		// cerr << "+++ vertical box, neu = " << non_expanding_used << " neuo " << non_expanding_used_opposite << " largest = " << largest << " opp " << largest_opposite << " total " << total << endl;
		min.y = non_expanding_used + (n_expanding * largest) + _top_margin + _bottom_margin + ((total - 1) * _spacing);
		min.x = largest_opposite + _left_margin + _right_margin;
	} else {
		// cerr << "+++ horiz box, neu = " << non_expanding_used << " neuo " << non_expanding_used_opposite << " largest = " << largest << " opp " << largest_opposite << " total " << total << endl;
		min.x = non_expanding_used + (n_expanding * largest) + _left_margin + _right_margin + ((total - 1) * _spacing);
		min.y = largest_opposite + _top_margin + _bottom_margin;

	}

	// cerr << whoami() << " preferred-size = " << min << endl;

	natural = min;
}

void
cBox::size_allocate (Rect const & r)
{
	PBD::Unwinder<bool> uw (in_alloc, true);

	Item::size_allocate (r);

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

	// cerr << "\n\n\n" << whoami() << " SIZE-ALLOC " << r << " NCU ? " << _need_constraint_update << " expanded items (" << n_expanding << ")will be " << expanded_size << " neu " << non_expanding_used << " t = " << total << " s " << _spacing
	// << " t " << _top_margin << " b " << _bottom_margin << " l " << _left_margin << " r " << _right_margin
	// << endl;


	if (_need_constraint_update) {
		update_constraints ();
	}

	_solver.suggestValue (width, r.width());
	_solver.suggestValue (height, r.height());
	_solver.suggestValue (expanded_item_size, expanded_size);

	_solver.updateVariables ();
	_solver.dump (cerr);

	for (ConstrainedItemMap::const_iterator o = constrained_map.begin(); o != constrained_map.end(); ++o) {
		o->second->dump (cerr);
	}

	apply (&_solver);

	_bounding_box_dirty = true;

}

void
cBox::update_constraints ()
{
	/* must totally override ConstraintPacker::update_constraints() */

	_solver.reset ();
	_solver.addEditVariable (width, kiwi::strength::strong);
	_solver.addEditVariable (height, kiwi::strength::strong);
	_solver.addEditVariable (expanded_item_size, kiwi::strength::strong);

	try {

		Order::iterator prev = order.end();

		for (Order::iterator o = order.begin(); o != order.end(); ++o) {

			Duple min, natural;

			(*o)->item().preferred_size (min, natural);

			if (orientation == Vertical) {
				add_vertical_box_constraints (_solver, *o, prev == order.end() ? 0 : *prev, natural.height(), natural.width(), width);
			} else {
				add_horizontal_box_constraints (_solver, *o, prev == order.end() ? 0 : *prev, natural.width(), natural.height(), height);
			}

			prev = o;
		}

		/* There maybe items that were not pack_start()'ed or
		 * pack_end()'ed into this box, but just added with
		 * constraints. Find all items in the box, and add any
		 * constraints that come with them.
		 */

		for (ConstrainedItemMap::const_iterator x = constrained_map.begin(); x != constrained_map.end(); ++x) {

			std::vector<Constraint> const & constraints (x->second->constraints());

			for (std::vector<Constraint>::const_iterator c = constraints.begin(); c != constraints.end(); ++c) {
				_solver.addConstraint (*c);
			}
		}


	} catch (std::exception& e) {
		cerr << "Setting up sovler failed: " << e.what() << endl;
		return;
	}

	_need_constraint_update = false;
}

void
cBox::child_changed (bool bbox_changed)
{
}

/* It would be nice to do this with templates or even by passing ptr-to-method,
 * but both of them interfere with the similarly meta-programming-ish nature of
 * the way that kiwi builds Constraint objects from expressions. So a macro it
 * is ...
 */

#define add_box_constraints(\
	solver, \
	bci, \
	prev, \
	natural_main_dimension, \
	natural_second_dimension, \
	alloc_var, \
	m_main_dimension, \
	m_second_dimension, \
	m_trailing, \
	m_leading, \
	m_trailing_padding, \
	m_leading_padding, \
	m_second_trailing, \
	m_second_leading, \
	m_second_trailing_padding, \
	m_second_leading_padding, \
	m_trailing_margin, \
	m_leading_margin, \
	m_second_trailing_margin, \
	m_second_leading_margin) \
 \
	/* Add constraints that will size the item within this box */ \
 \
	/* set up constraints for expand/fill options, done by \
	 * adjusting height and margins of each item \
	 */ \
 \
	if (bci->primary_axis_pack_options() & PackExpand) { \
 \
		/* item will take up more than it's natural \
		 * size, if space is available \
		 */ \
 \
		if (bci->primary_axis_pack_options() & PackFill) { \
 \
			/* item is expanding to fill all \
			 * available space and wants that space \
			 * for itself. \
			 */ \
 \
			solver.addConstraint ({(bci->m_main_dimension() == expanded_item_size) | kiwi::strength::strong}); \
			solver.addConstraint ({(bci->m_trailing_padding() == 0. ) | kiwi::strength::strong}); \
			solver.addConstraint ({(bci->m_leading_padding() == 0. ) | kiwi::strength::strong}); \
 \
		} else { \
 \
			/* item is expanding to fill all \
			 * available space and wants that space \
			 * as padding \
			 */ \
 \
			solver.addConstraint ({bci->m_main_dimension() == natural_main_dimension}); \
			solver.addConstraint ({(bci->m_trailing_padding() + bci->m_leading_padding() + bci->m_main_dimension() == expanded_item_size) | kiwi::strength::strong}); \
			solver.addConstraint ({(bci->m_leading_padding() == bci->m_trailing_padding()) | kiwi::strength::strong}); \
		} \
 \
	} else { \
 \
		/* item is not going to expand to fill \
		 * available space. just give it's preferred \
		 * height. \
		 */ \
 \
		/* cerr << bci->item().whoami() << " will usenatural height of " << natural.height() << endl; */ \
 \
		solver.addConstraint ({bci->m_main_dimension() == natural_main_dimension}); \
		solver.addConstraint ({bci->m_trailing_padding() == 0.}); \
		solver.addConstraint ({bci->m_leading_padding() == 0.}); \
	} \
 \
	/* now set upper upper edge of the item */ \
 \
	if (prev == 0) { \
 \
		/* first item */ \
 \
		solver.addConstraint ({(bci->m_trailing() == m_trailing_margin + bci->m_trailing_padding()) | kiwi::strength::strong}); \
 \
	} else { \
		/* subsequent items */ \
 \
		solver.addConstraint ({(bci->m_trailing() == prev->m_leading() + prev->m_leading_padding() + bci->m_trailing_padding() + _spacing) | kiwi::strength::strong}); \
	} \
 \
	solver.addConstraint ({bci->m_leading() == bci->m_trailing() + bci->m_main_dimension()}); \
 \
	/* set the side-effect variables and/or constants */ \
 \
	solver.addConstraint ({(bci->m_second_trailing_padding() == 0) | kiwi::strength::weak}); \
	solver.addConstraint ({(bci->m_second_leading_padding() == 0) | kiwi::strength::weak}); \
 \
	solver.addConstraint ({bci->m_second_trailing() + bci->m_second_dimension() == bci->m_second_leading()}); \
	solver.addConstraint ({(bci->m_second_trailing() == m_second_trailing_margin + bci->m_second_trailing_padding()) | kiwi::strength::strong}); \
 \
	if (!(bci->secondary_axis_pack_options() & PackExpand) && natural_second_dimension > 0) { \
		solver.addConstraint ({bci->m_second_dimension() == natural_second_dimension}); \
	} else { \
		solver.addConstraint ({(bci->m_second_dimension() == alloc_var - (m_second_trailing_margin + m_second_leading_margin + bci->m_second_leading_padding())) | kiwi::strength::strong}); \
	}


void
cBox::add_vertical_box_constraints (kiwi::Solver& solver, BoxConstrainedItem* ci, BoxConstrainedItem* prev, double main_dimension, double second_dimension, kiwi::Variable & alloc_var)
{
	add_box_constraints (solver, ci, prev, main_dimension, second_dimension, alloc_var,
	                     height, width,
	                     top, bottom, top_padding, bottom_padding,
	                     left, right, left_padding, right_padding,
	                     _top_margin, _bottom_margin, _left_margin, _right_margin);

}

void
cBox::add_horizontal_box_constraints (kiwi::Solver& solver, BoxConstrainedItem* ci, BoxConstrainedItem* prev, double main_dimension, double second_dimension, kiwi::Variable& alloc_var)
{
	add_box_constraints (solver, ci, prev, main_dimension, second_dimension, alloc_var,
	                     width, height,
	                     left, right, left_padding, right_padding,
	                     top, bottom, top_padding, bottom_padding,
	                     _left_margin, _right_margin, _top_margin, _bottom_margin);
}

void
cBox::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if ((fill() || outline()) && _allocation) {

		Rect contents = _allocation;

		/* allocation will have been left with (x0,y0) as given by the
		 * parent, but _position is set to the same value and will
		 * be taken into account by item_to_window()
		 */

		double width = contents.width() - (_left_margin + _top_margin);
		double height = contents.height() - (_top_margin + _bottom_margin);

		contents.x0 = _left_margin;
		contents.y0 = _top_margin;

		contents.x1 = contents.x0 + width;
		contents.y1 = contents.y0 + height;

		Rect self (item_to_window (contents, false));
		const Rect draw = self.intersection (area);

		if (fill()) {

			cerr << whoami() << " setting fill context with 0x" << std::hex << _fill_color << std::dec << " draw " << draw << endl;

			setup_fill_context (context);
			context->rectangle (draw.x0, draw.y0, draw.width(), draw.height());
			context->fill_preserve ();
		}

		if (outline()) {
			if (!fill()) {
				context->rectangle (draw.x0, draw.y0, draw.width(), draw.height());
			}
			setup_outline_context (context);
			context->stroke ();
		}
	}

	Item::render_children (area, context);
}
