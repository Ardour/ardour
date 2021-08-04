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

#include "pbd/i18n.h"
#include "pbd/unwind.h"
#include "pbd/stacktrace.h"

#include "kiwi/kiwi.h"

#include "canvas/canvas.h"
#include "canvas/constraint_packer.h"
#include "canvas/constrained_item.h"
#include "canvas/item.h"
#include "canvas/rectangle.h"

using namespace ArdourCanvas;

using std::cerr;
using std::endl;
using std::vector;
using kiwi::Constraint;
using namespace kiwi;

ConstraintPacker::ConstraintPacker (Canvas* canvas, Orientation o)
	: Container (canvas)
	, width (X_("packer width"))
	, height (X_("packer height"))
	, _orientation (o)
	, _spacing (0)
	, _top_padding (0)
	, _bottom_padding (0)
	, _left_padding (0)
	, _right_padding (0)
	, _top_margin (0)
	, _bottom_margin (0)
	, _left_margin (0)
	, _right_margin (0)
	, in_alloc (false)
	, _need_constraint_update (false)
{
	set_fill (false);
	set_outline (false);
	set_layout_sensitive (true);

	_solver.addEditVariable (width, kiwi::strength::strong);
	_solver.addEditVariable (height, kiwi::strength::strong);
}

ConstraintPacker::ConstraintPacker (Item* parent, Orientation o)
	: Container (parent)
	, width (X_("packer width"))
	, height (X_("packer height"))
	, _orientation (o)
	, _spacing (0)
	, _top_padding (0)
	, _bottom_padding (0)
	, _left_padding (0)
	, _right_padding (0)
	, _top_margin (0)
	, _bottom_margin (0)
	, _left_margin (0)
	, _right_margin (0)
	, in_alloc (false)
	, _need_constraint_update (false)
{
	set_fill (false);
	set_outline (false);
	set_layout_sensitive (true);

	_solver.addEditVariable (width, kiwi::strength::strong);
	_solver.addEditVariable (height, kiwi::strength::strong);
}

void
ConstraintPacker::compute_bounding_box () const
{
	_bounding_box = _allocation;
	bb_clean ();
}

void
ConstraintPacker::child_changed (bool bbox_changed)
{
	Item::child_changed (bbox_changed);

	if (in_alloc || !bbox_changed) {
		return;
	}
#if 0
	cerr << "CP, child bbox changed\n";

	for (ConstrainedItemMap::iterator x = constrained_map.begin(); x != constrained_map.end(); ++x) {

		Duple i;
		x->first->size_request (i.x, i.y);

		if (r) {

			// cerr << x->first->whatami() << '/' << x->first->name << " has instrinsic size " << r << endl;

			kiwi::Variable& w (x->second->intrinsic_width());
			if (!r.width()) {
				if (_solver.hasEditVariable (w)) {
					_solver.removeEditVariable (w);
					cerr << "\tremoved inttrinsic-width edit var\n";
				}
			} else {
				if (!_solver.hasEditVariable (w))  {
					cerr << "\tadding intrinsic width constraints\n";
					_solver.addEditVariable (w, kiwi::strength::strong);
					_solver.addConstraint (Constraint {x->second->width() >= w } | kiwi::strength::strong);
					_solver.addConstraint (Constraint (x->second->width() <= w) | kiwi::strength::weak);
				}
			}

			kiwi::Variable& h (x->second->intrinsic_height());
			if (!r.height()) {
				if (_solver.hasEditVariable (h)) {
					_solver.removeEditVariable (h);
					cerr << "\tremoved inttrinsic-height edit var\n";
				}
			} else {
				if (!_solver.hasEditVariable (h))  {
					cerr << "\tadding intrinsic height constraints\n";
					_solver.addEditVariable (h, kiwi::strength::strong);
					_solver.addConstraint (Constraint {x->second->height() >= h } | kiwi::strength::strong);
					_solver.addConstraint (Constraint (x->second->height() <= h) | kiwi::strength::weak);
				}
			}
		}
	}
#endif
}

void
ConstraintPacker::constrain (kiwi::Constraint const &c)
{
	constraint_list.push_back (c);
	_need_constraint_update = true;
}

void
ConstraintPacker::box_size_request (Distance& w, Distance& h) const
{
	BoxPackedItems::size_type n_expanding = 0;
	BoxPackedItems::size_type n_nonexpanding = 0;
	BoxPackedItems::size_type total = 0;
	Distance non_expanding_used = 0;
	Distance largest = 0;
	Distance largest_opposite = 0;
	Distance width;
	Distance height;

	for (BoxPackedItems::const_iterator o = packed.begin(); o != packed.end(); ++o) {

		(*o)->item().size_request (width, height);

		// cerr << '\t' << (*o)->item().whoami() << " min " << i_min << " nat " << i_natural << endl;

		if ((*o)->primary_axis_pack_options() & PackExpand) {
			n_expanding++;

			if (_orientation == Vertical) {
				if (height > largest) {
					largest = height;
				}
			} else {
				if (width > largest) {
					largest = width;;
				}
				if (height > largest) {
					largest_opposite = height;
				}
			}

		} else {
			n_nonexpanding++;

			if (_orientation == Vertical) {
				non_expanding_used += height;
			} else {
				non_expanding_used += width;
			}
		}

		/* determine the maximum size for the opposite axis. All items
		 * will be this size or less on this axis
		 */

		if (_orientation == Vertical) {
			if (width > largest_opposite) {
				largest_opposite = width;
			}
		} else {
			if (height > largest_opposite) {
				largest_opposite = height;
			}
		}

		total++;
	}

	Duple r;

	if (_orientation == Vertical) {
		// cerr << "+++ vertical box, neu = " << non_expanding_used << " neuo " << non_expanding_used_opposite << " largest = " << largest << " opp " << largest_opposite << " total " << total << endl;
		height = non_expanding_used + (n_expanding * largest) + _top_margin + _bottom_margin + ((total - 1) * _spacing);
		width= largest_opposite + _left_margin + _right_margin;
	} else {
		// cerr << "+++ horiz box, neu = " << non_expanding_used << " neuo " << non_expanding_used_opposite << " largest = " << largest << " opp " << largest_opposite << " total " << total << endl;
		width = non_expanding_used + (n_expanding * largest) + _left_margin + _right_margin + ((total - 1) * _spacing);
		height = largest_opposite + _top_margin + _bottom_margin;

	}
}

void
ConstraintPacker::size_request (Distance& w, Distance& h) const
{
	const_cast<ConstraintPacker*>(this)->non_const_size_request (w, h);
}

void
ConstraintPacker::non_const_size_request (Distance& w, Distance& h)
{
	/* our parent wants to know how big we are.

	   We may have some intrinsic size (i.e. "everything in this constraint
	   layout should fit into WxH". Just add two constraints on our width
	   and height, and solve.

	   We may have one intrinsic dimension (i.e. "everything in this
	   constraint layout should fit into this (width|height). Ask all of
	   our children for the size-given-(W|H). Add constraints to represent
	   those values, and solve.

	   We may have no intrinsic dimensions at all. This is the tricky one.
	*/

	if (packed.size() == constrained_map.size()) {
		/* All child items were packed using ::pack() */
		box_size_request (w, h);
		return;
	}

	if (_requested_width < 0 && _requested_height < 0) {
		w = 100;
		h = 100;
		return;
	}

	if (_need_constraint_update) {
		const_cast<ConstraintPacker*>(this)->update_constraints ();
	}

	if (_requested_width > 0) {
		_solver.suggestValue (width, _requested_width);
	} else if (_requested_height > 0) {
		_solver.suggestValue (height, _requested_height);
	}

	_solver.updateVariables ();
	apply (0);

	Rect bb (bounding_box());

	w = std::max (bb.width(), _requested_width);
	h = std::max (bb.height(), _requested_width);

	/* put solver back to default state */

	_solver.reset ();
	_need_constraint_update = true;
}

void
ConstraintPacker::_size_allocate (Rect const & r)
{
	PBD::Unwinder<bool> uw (in_alloc, true);
	double expanded_size;

	if (_layout_sensitive) {
		_position = Duple (r.x0, r.y0);
		_allocation = r;
	}

	if (!packed.empty()) {

		BoxPackedItems::size_type n_expanding = 0;
		BoxPackedItems::size_type n_nonexpanding = 0;
		BoxPackedItems::size_type total = 0;
		Distance non_expanding_used = 0;

		for (BoxPackedItems::iterator o = packed.begin(); o != packed.end(); ++o) {
			if ((*o)->primary_axis_pack_options() & PackExpand) {
				n_expanding++;
			} else {
				n_nonexpanding++;

				Distance w, h;

				(*o)->item().size_request (w, h);

				if (_orientation == Vertical) {
					non_expanding_used += h;
				} else {
					non_expanding_used += w;
				}

			}
			total++;
		}

		if (_orientation == Vertical) {
			expanded_size = (r.height() - _top_margin - _bottom_margin - ((total - 1) * _spacing) - non_expanding_used) / n_expanding;
		} else {
			expanded_size = (r.width() - _left_margin - _right_margin - ((total - 1) * _spacing) - non_expanding_used) / n_expanding;
		}
	}

	if (_need_constraint_update) {
		update_constraints ();
	}

	_solver.suggestValue (width, r.width());
	_solver.suggestValue (height, r.height());

	if (!packed.empty()) {
		_solver.suggestValue (expanded_item_size, expanded_size);
	}

	_solver.updateVariables ();

#if 0
	// PBD::stacktrace (cerr, 100);
	_canvas->dump (cerr);
	_solver.dump (cerr);

	for (ConstrainedItemMap::const_iterator o = constrained_map.begin(); o != constrained_map.end(); ++o) {
		o->second->dump (cerr);
	}
#endif

	apply (0);

	_bounding_box_dirty = true;
}

void
ConstraintPacker::add (Item* item)
{
	(void) add_constrained (item);
}

void
ConstraintPacker::add_front (Item* item)
{
	(void) add_constrained (item);
}

void
ConstraintPacker::add_constraints (Solver& s, ConstrainedItem* ci) const
{
	/* add any constraints inherent to this item */

	vector<Constraint> const & vc (ci->constraints());

	for (vector<Constraint>::const_iterator x = vc.begin(); x != vc.end(); ++x) {
		s.addConstraint (*x);
	}
}

ConstrainedItem*
ConstraintPacker::add_constrained (Item* item)
{
	ConstrainedItem* ci =  new ConstrainedItem (*item);
	add_constrained_internal (item, ci);
	return ci;
}

void
ConstraintPacker::add_constrained_internal (Item* item, ConstrainedItem* ci)
{
	Item::add (item);
	item->set_layout_sensitive (true);
	constrained_map.insert (std::make_pair (item, ci));
	_need_constraint_update = true;
	child_changed (true);
}

void
ConstraintPacker::remove (Item* item)
{
	Item::remove (item);

	for (ConstrainedItemMap::iterator x = constrained_map.begin(); x != constrained_map.end(); ++x) {

		if (x->first == item) {

			/* remove any non-builtin constraints for this item */

			for (ConstraintList::iterator c = constraint_list.begin(); c != constraint_list.end(); ++c) {
				if (x->second->involved (*c)) {
					constraint_list.erase (c);
				}
			}

			item->set_layout_sensitive (false);

			/* clean up */

			delete x->second;
			constrained_map.erase (x);
			break;
		}

	}

	for (BoxPackedItems::iterator t = packed.begin(); t != packed.end(); ++t) {
		if (&(*t)->item() == item) {
			packed.erase (t);
			break;
		}
	}

	_need_constraint_update = true;
}

void
ConstraintPacker::apply (Solver* s)
{
	for (ConstrainedItemMap::iterator x = constrained_map.begin(); x != constrained_map.end(); ++x) {
		x->second->constrained (*this);
	}
}

void
ConstraintPacker::update_constraints ()
{
	_solver.reset ();
	_solver.addEditVariable (width, kiwi::strength::strong);
	_solver.addEditVariable (height, kiwi::strength::strong);

	if (!packed.empty()) {
		_solver.addEditVariable (expanded_item_size, kiwi::strength::strong);
	}

	try {

		/* First handle box-packed items */

		BoxPackedItems::iterator prev = packed.end();

		for (BoxPackedItems::iterator o = packed.begin(); o != packed.end(); ++o) {

			Distance w, h;

			(*o)->item().size_request (w,h);

			if (_orientation == Vertical) {
				add_vertical_box_constraints (_solver, *o, prev == packed.end() ? 0 : *prev, h, w, width);
			} else {
				add_horizontal_box_constraints (_solver, *o, prev == packed.end() ? 0 : *prev, w, h, height);
			}

			prev = o;
		}

		/* Now handle all other items (exclude those already dealt with */

		for (ConstrainedItemMap::iterator x = constrained_map.begin(); x != constrained_map.end(); ++x) {

			if (std::find (packed.begin(), packed.end(), x->second) != packed.end()) {
				add_constraints (_solver, x->second);
				continue;
			}

			Distance w, h;
			ConstrainedItem* ci = x->second;

			x->first->size_request (w, h);

			_solver.addConstraint ((ci->width() == w) | kiwi::strength::medium);
			_solver.addConstraint ((ci->height() == h) | kiwi::strength::medium);

			add_constraints (_solver, ci);
		}

		/* Now add packer-level constraints */

		for (ConstraintList::const_iterator c = constraint_list.begin(); c != constraint_list.end(); ++c) {
			_solver.addConstraint (*c);
		}

		_need_constraint_update = false;

	} catch (std::exception& e) {
		cerr << "Setting up sovler failed: " << e.what() << endl;
	}
}

BoxConstrainedItem*
ConstraintPacker::pack_start (Item* item, PackOptions primary_axis_opts, PackOptions secondary_axis_opts)
{
	return pack (item, PackOptions (primary_axis_opts|PackFromStart), secondary_axis_opts);
}

BoxConstrainedItem*
ConstraintPacker::pack_end (Item* item, PackOptions primary_axis_opts, PackOptions secondary_axis_opts)
{
	return pack (item, PackOptions (primary_axis_opts|PackFromEnd), secondary_axis_opts);
}

BoxConstrainedItem*
ConstraintPacker::pack (Item* item, PackOptions primary_axis_opts, PackOptions secondary_axis_opts)
{
	BoxConstrainedItem* ci =  new BoxConstrainedItem (*item, primary_axis_opts, secondary_axis_opts);

	add_constrained_internal (item, ci);
	packed.push_back (ci);

	return ci;
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
ConstraintPacker::add_vertical_box_constraints (kiwi::Solver& solver, BoxConstrainedItem* ci, BoxConstrainedItem* prev, double main_dimension, double second_dimension, kiwi::Variable & alloc_var)
{
	add_box_constraints (solver, ci, prev, main_dimension, second_dimension, alloc_var,
	                     height, width,
	                     top, bottom, top_padding, bottom_padding,
	                     left, right, left_padding, right_padding,
	                     _top_margin, _bottom_margin, _left_margin, _right_margin);

}

void
ConstraintPacker::add_horizontal_box_constraints (kiwi::Solver& solver, BoxConstrainedItem* ci, BoxConstrainedItem* prev, double main_dimension, double second_dimension, kiwi::Variable& alloc_var)
{
	add_box_constraints (solver, ci, prev, main_dimension, second_dimension, alloc_var,
	                     width, height,
	                     left, right, left_padding, right_padding,
	                     top, bottom, top_padding, bottom_padding,
	                     _left_margin, _right_margin, _top_margin, _bottom_margin);
}

void
ConstraintPacker::set_spacing (double s)
{
	_spacing = s;
}

void
ConstraintPacker::set_padding (double top, double right, double bottom, double left)
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
ConstraintPacker::set_margin (double top, double right, double bottom, double left)
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
ConstraintPacker::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
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

			setup_fill_context (context);
			context->rectangle (draw.x0, draw.y0, draw.width(), draw.height());
			if (outline()) {
				context->fill_preserve ();
			} else {
				context->fill ();
			}
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
