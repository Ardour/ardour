/*
    Copyright (C) 2011-2013 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <iostream>
#include <cairomm/context.h>

#include "pbd/stacktrace.h"
#include "pbd/compose.h"

#include "canvas/group.h"
#include "canvas/types.h"
#include "canvas/debug.h"
#include "canvas/item.h"
#include "canvas/canvas.h"

using namespace std;
using namespace ArdourCanvas;

int Group::default_items_per_cell = 64;


Group::Group (Canvas* canvas)
	: Item (canvas)
	, _lut (0)
{
	
}

Group::Group (Group* parent)
	: Item (parent)
	, _lut (0)
{
	
}

Group::Group (Group* parent, Duple position)
	: Item (parent, position)
	, _lut (0)
{
	
}

Group::~Group ()
{
	for (list<Item*>::iterator i = _items.begin(); i != _items.end(); ++i) {
		(*i)->unparent ();
	}

	_items.clear ();
}

/** @param area Area to draw in this group's coordinates.
 *  @param context Context, set up with its origin at this group's position.
 */
void
Group::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	ensure_lut ();
	vector<Item*> items = _lut->get (area);

	++render_depth;
		
#ifdef CANVAS_DEBUG
	if (DEBUG_ENABLED(PBD::DEBUG::CanvasRender)) {
		cerr << string_compose ("%1GROUP %2 render %5 %3 items out of %4\n", 
					_canvas->render_indent(), (name.empty() ? string ("[unnamed]") : name), items.size(), _items.size(), area);
	}
#endif

	for (vector<Item*>::const_iterator i = items.begin(); i != items.end(); ++i) {

		if (!(*i)->visible ()) {
#ifdef CANVAS_DEBUG
			if (DEBUG_ENABLED(PBD::DEBUG::CanvasRender)) {
				cerr << _canvas->render_indent() << "Item " << (*i)->whatami() << " [" << (*i)->name << "] invisible - skipped\n";
			}
#endif
			continue;
		}
		
		boost::optional<Rect> item_bbox = (*i)->bounding_box ();

		if (!item_bbox) {
#ifdef CANVAS_DEBUG
			if (DEBUG_ENABLED(PBD::DEBUG::CanvasRender)) {
				// cerr << _canvas->render_indent() << "Item " << (*i)->whatami() << " [" << (*i)->name << "] empty - skipped\n";
			}
#endif
			continue;
		}
		
		Rect item = (*i)->item_to_window (item_bbox.get());
		item.expand (0.5);
		boost::optional<Rect> draw = item.intersection (area);
		
		if (draw) {
#ifdef CANVAS_DEBUG
			if (DEBUG_ENABLED(PBD::DEBUG::CanvasRender)) {
				cerr << _canvas->render_indent() << " render "
				     << ' ' 
				     << (*i)->whatami()
				     << ' '
				     << (*i)->name
				     << " item = " 
				     << item
				     << " intersect = "
				     << draw.get()
				     << endl;
			}
#endif

			(*i)->render (area, context);
			++render_count;

		} else {
#ifdef CANVAS_DEBUG
			if (DEBUG_ENABLED(PBD::DEBUG::CanvasRender)) {
				//cerr << string_compose ("%1skip render of %2 %3, no intersection\n", _canvas->render_indent(), (*i)->whatami(),
				// (*i)->name);
			}
#endif
		}
	}

	--render_depth;
}

void
Group::compute_bounding_box () const
{
	Rect bbox;
	bool have_one = false;

	for (list<Item*>::const_iterator i = _items.begin(); i != _items.end(); ++i) {

		boost::optional<Rect> item_bbox = (*i)->bounding_box ();

		if (!item_bbox) {
			continue;
		}

		Rect group_bbox = (*i)->item_to_parent (item_bbox.get ());
		if (have_one) {
			bbox = bbox.extend (group_bbox);
		} else {
			bbox = group_bbox;
			have_one = true;
		}
	}

	if (!have_one) {
		_bounding_box = boost::optional<Rect> ();
	} else {
		_bounding_box = bbox;
	}

	_bounding_box_dirty = false;
}

void
Group::add (Item* i)
{
	/* XXX should really notify canvas about this */

	_items.push_back (i);
	invalidate_lut ();
	_bounding_box_dirty = true;

	
}

void
Group::remove (Item* i)
{

	if (i->parent() != this) {
		return;
	}

	begin_change ();

	i->unparent ();
	_items.remove (i);
	invalidate_lut ();
	_bounding_box_dirty = true;
	
	end_change ();
}

void
Group::clear (bool with_delete)
{
	begin_change ();

	for (list<Item*>::iterator i = _items.begin(); i != _items.end(); ++i) {
		if (with_delete) {
			delete *i;
		} else {
			(*i)->unparent ();
		}
	}

	_items.clear ();

	invalidate_lut ();
	_bounding_box_dirty = true;

	end_change ();
}

void
Group::raise_child_to_top (Item* i)
{
	_items.remove (i);
	_items.push_back (i);
	invalidate_lut ();
}

void
Group::raise_child (Item* i, int levels)
{
	list<Item*>::iterator j = find (_items.begin(), _items.end(), i);
	assert (j != _items.end ());

	++j;
	_items.remove (i);

	while (levels > 0 && j != _items.end ()) {
		++j;
		--levels;
	}

	_items.insert (j, i);
	invalidate_lut ();
}

void
Group::lower_child_to_bottom (Item* i)
{
	_items.remove (i);
	_items.push_front (i);
	invalidate_lut ();
}

void
Group::ensure_lut () const
{
	if (!_lut) {
		_lut = new DumbLookupTable (*this);
	}
}

void
Group::invalidate_lut () const
{
	delete _lut;
	_lut = 0;
}

void
Group::child_changed ()
{
	invalidate_lut ();
	_bounding_box_dirty = true;

	if (_parent) {
		_parent->child_changed ();
	}
}

void
Group::add_items_at_point (Duple const point, vector<Item const *>& items) const
{
	/* Point is in parent coordinate system */

	boost::optional<Rect> const bbox = bounding_box ();

	if (!bbox || !bbox.get().contains (point)) {
		return;
	}

	/* this adds this group itself to the list of items at point */
	Item::add_items_at_point (point, items);
	

	/* now recurse and add any items within our group that contain point */

	ensure_lut ();
	vector<Item*> our_items = _lut->items_at_point (point);

	for (vector<Item*>::iterator i = our_items.begin(); i != our_items.end(); ++i) {
		(*i)->add_items_at_point (point - (*i)->position(), items);
	}
}

void
Group::dump (ostream& o) const
{
#ifdef CANVAS_DEBUG
	o << _canvas->indent();
	o << "Group " << this << " [" << name << ']';
	o << " @ " << position();
	o << " Items: " << _items.size();
	o << " Visible ? " << _visible;

	boost::optional<Rect> bb = bounding_box();

	if (bb) {
		o << endl << _canvas->indent() << "  bbox: " << bb.get();
		o << endl << _canvas->indent() << "  CANVAS bbox: " << item_to_canvas (bb.get());
	} else {
		o << "  bbox unset";
	}

	o << endl;
#endif

	ArdourCanvas::dump_depth++;

	for (list<Item*>::const_iterator i = _items.begin(); i != _items.end(); ++i) {
		o << **i;
	}

	ArdourCanvas::dump_depth--;
}
