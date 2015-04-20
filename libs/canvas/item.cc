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

#include "pbd/compose.h"
#include "pbd/stacktrace.h"
#include "pbd/convert.h"

#include "ardour/utils.h"

#include "canvas/canvas.h"
#include "canvas/debug.h"
#include "canvas/item.h"
#include "canvas/scroll_group.h"

using namespace std;
using namespace PBD;
using namespace ArdourCanvas;

int Item::default_items_per_cell = 64;

Item::Item (Canvas* canvas)
	: Fill (*this)
	, Outline (*this)
	,  _canvas (canvas)
	, _parent (0)
	, _scroll_parent (0)
	, _visible (true)
	, _bounding_box_dirty (true)
	, _lut (0)
	, _ignore_events (false)
{
	DEBUG_TRACE (DEBUG::CanvasItems, string_compose ("new canvas item %1\n", this));
}	

Item::Item (Item* parent)
	: Fill (*this)
	, Outline (*this)
	,  _canvas (parent->canvas())
	, _parent (parent)
	, _scroll_parent (0)
	, _visible (true)
	, _bounding_box_dirty (true)
	, _lut (0)
	, _ignore_events (false)
{
	DEBUG_TRACE (DEBUG::CanvasItems, string_compose ("new canvas item %1\n", this));

	if (parent) {
		_parent->add (this);
	}

	find_scroll_parent ();
}	

Item::Item (Item* parent, Duple const& p)
	: Fill (*this)
	, Outline (*this)
	,  _canvas (parent->canvas())
	, _parent (parent)
	, _scroll_parent (0)
	, _position (p)
	, _visible (true)
	, _bounding_box_dirty (true)
	, _lut (0)
	, _ignore_events (false)
{
	DEBUG_TRACE (DEBUG::CanvasItems, string_compose ("new canvas item %1\n", this));

	if (parent) {
		_parent->add (this);
	}

	find_scroll_parent ();

}	

Item::~Item ()
{
	if (_parent) {
		_parent->remove (this);
	}

	if (_canvas) {
		_canvas->item_going_away (this, _bounding_box);
	}

	clear_items (true);
	delete _lut;
}

bool
Item::visible() const
{
	Item const * i = this;

	while (i) {
		if (!i->self_visible()) {
			return false;
		}
		i = i->parent();
	}

	return true;
}

Duple
Item::canvas_origin () const
{
	return item_to_canvas (Duple (0,0));
}

Duple
Item::window_origin () const 
{
	/* This is slightly subtle. Our _position is in the coordinate space of 
	   our parent. So to find out where that is in window coordinates, we
	   have to ask our parent.
	*/
	if (_parent) {
		return _parent->item_to_window (_position);
	} else {
		return _position;
	}
}

ArdourCanvas::Rect
Item::item_to_parent (ArdourCanvas::Rect const & r) const
{
	return r.translate (_position);
}

Duple
Item::scroll_offset () const
{
	if (_scroll_parent) {
		return _scroll_parent->scroll_offset();
	} 
	return Duple (0,0);
}

Duple
Item::position_offset() const
{
	Item const * i = this;
	Duple offset;

	while (i) {
		offset = offset.translate (i->position());
		i = i->parent();
	}

	return offset;
}

ArdourCanvas::Rect
Item::item_to_canvas (ArdourCanvas::Rect const & r) const
{
	return r.translate (position_offset());
}

ArdourCanvas::Duple
Item::item_to_canvas (ArdourCanvas::Duple const & d) const
{
	return d.translate (position_offset());
}

ArdourCanvas::Duple
Item::canvas_to_item (ArdourCanvas::Duple const & r) const
{
	return r.translate (-position_offset());
}

ArdourCanvas::Rect
Item::canvas_to_item (ArdourCanvas::Rect const & r) const
{
	return r.translate (-position_offset());
}

void
Item::item_to_canvas (Coord& x, Coord& y) const
{
	Duple d = item_to_canvas (Duple (x, y));
		
	x = d.x;
	y = d.y;
}

void
Item::canvas_to_item (Coord& x, Coord& y) const
{
	Duple d = canvas_to_item (Duple (x, y));

	x = d.x;
	y = d.y;
}


Duple
Item::item_to_window (ArdourCanvas::Duple const & d, bool rounded) const
{
	Duple ret = item_to_canvas (d).translate (-scroll_offset());

	if (rounded) {
		ret.x = round (ret.x);
		ret.y = round (ret.y);
	}

	return ret;
}

Duple
Item::window_to_item (ArdourCanvas::Duple const & d) const
{
	return canvas_to_item (d.translate (scroll_offset()));
}

ArdourCanvas::Rect
Item::item_to_window (ArdourCanvas::Rect const & r, bool rounded) const
{
	Rect ret = item_to_canvas (r).translate (-scroll_offset());

	if (rounded) {
		ret.x0 = round (ret.x0);
		ret.x1 = round (ret.x1);
		ret.y0 = round (ret.y0);
		ret.y1 = round (ret.y1);
	}

	return ret;
}

ArdourCanvas::Rect
Item::window_to_item (ArdourCanvas::Rect const & r) const
{
	return canvas_to_item (r.translate (scroll_offset()));
}

/** Set the position of this item in the parent's coordinates */
void
Item::set_position (Duple p)
{
	if (p == _position) {
		return;
	}

	boost::optional<ArdourCanvas::Rect> bbox = bounding_box ();
	boost::optional<ArdourCanvas::Rect> pre_change_parent_bounding_box;

	if (bbox) {
		/* see the comment in Canvas::item_moved() to understand
		 * why we use the parent's bounding box here.
		 */
		pre_change_parent_bounding_box = item_to_parent (bbox.get());
	}
	
	_position = p;

	/* only update canvas and parent if visible. Otherwise, this
	   will be done when ::show() is called.
	*/
	
	if (visible()) {
		_canvas->item_moved (this, pre_change_parent_bounding_box);
		

		if (_parent) {
			_parent->child_changed ();
		}
	}
}

void
Item::set_x_position (Coord x)
{
	set_position (Duple (x, _position.y));
}

void
Item::set_y_position (Coord y)
{
	set_position (Duple (_position.x, y));
}

void
Item::raise_to_top ()
{
	if (_parent) {
		_parent->raise_child_to_top (this);
	}
}

void
Item::raise (int levels)
{
	if (_parent) {
		_parent->raise_child (this, levels);
	}
}

void
Item::lower_to_bottom ()
{
	if (_parent) {
		_parent->lower_child_to_bottom (this);
	}
}

void
Item::hide ()
{
	if (_visible) {
		_visible = false;

		/* children are all hidden because we are hidden, no need
		   to propagate change because our bounding box necessarily
		   includes them all already. thus our being hidden results
		   in (a) a redraw of the entire bounding box (b) no children
		   will be drawn.

		   BUT ... current item in canvas might be one of our children,
		   which is now hidden. So propagate away.
		*/

		for (list<Item*>::iterator i = _items.begin(); i != _items.end(); ++i) {

			if ((*i)->self_visible()) {
				/* item was visible but is now hidden because
				   we (its parent) are hidden
				*/
				(*i)->propagate_show_hide ();
			}
		}

		
		propagate_show_hide ();
	}
}

void
Item::show ()
{
	if (!_visible) {

		_visible = true;

		for (list<Item*>::iterator i = _items.begin(); i != _items.end(); ++i) {
			if ((*i)->self_visible()) {
				/* item used to be hidden by us (its parent),
				   but is now visible
				*/
				(*i)->propagate_show_hide ();
			}
		}

		propagate_show_hide ();
	}
}

void
Item::propagate_show_hide ()
{
	/* bounding box may have changed while we were hidden */
	
	if (_parent) {
		_parent->child_changed ();
	}
	
	_canvas->item_shown_or_hidden (this);
}

Duple
Item::item_to_parent (Duple const & d) const
{
	return d.translate (_position);
}

Duple
Item::parent_to_item (Duple const & d) const
{
	return d.translate (- _position);
}

ArdourCanvas::Rect
Item::parent_to_item (ArdourCanvas::Rect const & d) const
{
	return d.translate (- _position);
}

void
Item::unparent ()
{
	_parent = 0;
	_scroll_parent = 0;
}

void
Item::reparent (Item* new_parent)
{
	if (new_parent == _parent) {
		return;
	}

	assert (_canvas == new_parent->canvas());

	if (_parent) {
		_parent->remove (this);
	}

	assert (new_parent);

	_parent = new_parent;
	_canvas = _parent->canvas ();

	find_scroll_parent ();

	_parent->add (this);
}

void
Item::find_scroll_parent ()
{
	Item const * i = this;
	ScrollGroup const * last_scroll_group = 0;

	/* Don't allow a scroll group to find itself as its own scroll parent
	 */

	i = i->parent ();

	while (i) {
		ScrollGroup const * sg = dynamic_cast<ScrollGroup const *> (i);
		if (sg) {
			last_scroll_group = sg;
		}
		i = i->parent();
	}
	
	_scroll_parent = const_cast<ScrollGroup*> (last_scroll_group);
}

bool
Item::common_ancestor_within (uint32_t limit, const Item& other) const
{
	uint32_t d1 = depth();
	uint32_t d2 = other.depth();
	const Item* i1 = this;
	const Item* i2 = &other;
	
	/* move towards root until we are at the same level
	   for both items
	*/

	while (d1 != d2) {
		if (d1 > d2) {
			if (!i1) {
				return false;
			}
			i1 = i1->parent();
			d1--;
			limit--;
		} else {
			if (!i2) {
				return false;
			}
			i2 = i2->parent();
			d2--;
			limit--;
		}
		if (limit == 0) {
			return false;
		}
	}

	/* now see if there is a common parent */

	while (i1 != i2) {
		if (i1) {
			i1 = i1->parent();
		}
		if (i2) {
			i2 = i2->parent ();
		}

		limit--;
		if (limit == 0) {
			return false;
		}
	}
	
	return true;
}

const Item*
Item::closest_ancestor_with (const Item& other) const
{
	uint32_t d1 = depth();
	uint32_t d2 = other.depth();
	const Item* i1 = this;
	const Item* i2 = &other;

	/* move towards root until we are at the same level
	   for both items
	*/

	while (d1 != d2) {
		if (d1 > d2) {
			if (!i1) {
				return 0;
			}
			i1 = i1->parent();
			d1--;
		} else {
			if (!i2) {
				return 0;
			}
			i2 = i2->parent();
			d2--;
		}
	}

	/* now see if there is a common parent */

	while (i1 != i2) {
		if (i1) {
			i1 = i1->parent();
		}
		if (i2) {
			i2 = i2->parent ();
		}
	}
	
	return i1;
}

bool
Item::is_descendant_of (const Item& candidate) const
{
	Item const * i = _parent;

	while (i) {
		if (i == &candidate) {
			return true;
		}
		i = i->parent();
	}

	return false;
}

void
Item::grab_focus ()
{
	/* XXX */
}

/** @return Bounding box in this item's coordinates */
boost::optional<ArdourCanvas::Rect>
Item::bounding_box () const
{
	if (_bounding_box_dirty) {
		compute_bounding_box ();
		assert (!_bounding_box_dirty);
		add_child_bounding_boxes ();
	}

	return _bounding_box;
}

Coord
Item::height () const 
{
	boost::optional<ArdourCanvas::Rect> bb  = bounding_box();

	if (bb) {
		return bb->height ();
	}
	return 0;
}

Coord
Item::width () const 
{
	boost::optional<ArdourCanvas::Rect> bb = bounding_box();

	if (bb) {
		return bb->width ();
	}

	return 0;
}

void
Item::redraw () const
{
	if (visible() && _bounding_box && _canvas) {
		_canvas->request_redraw (item_to_window (_bounding_box.get()));
	}
}	

void
Item::begin_change ()
{
	_pre_change_bounding_box = bounding_box ();
}

void
Item::end_change ()
{
	if (visible()) {
		_canvas->item_changed (this, _pre_change_bounding_box);
		
		if (_parent) {
			_parent->child_changed ();
		}
	}
}

void
Item::begin_visual_change ()
{
}

void
Item::end_visual_change ()
{
	if (visible()) {
		_canvas->item_visual_property_changed (this);
	}
}

void
Item::move (Duple movement)
{
	set_position (position() + movement);
}

void
Item::grab ()
{
	assert (_canvas);
	_canvas->grab (this);
}

void
Item::ungrab ()
{
	assert (_canvas);
	_canvas->ungrab ();
}

void
Item::set_data (string const & key, void* data)
{
	_data[key] = data;
}

void *
Item::get_data (string const & key) const
{
	map<string, void*>::const_iterator i = _data.find (key);
	if (i == _data.end ()) {
		return 0;
	}
	
	return i->second;
}

void
Item::set_ignore_events (bool ignore)
{
	_ignore_events = ignore;
}

std::string
Item::whatami () const 
{
	std::string type = demangle (typeid (*this).name());
	return type.substr (type.find_last_of (':') + 1);
}

uint32_t
Item::depth () const
{
	Item* i = _parent;
	int d = 0;
	while (i) {
		++d;
		i = i->parent();
	}
	return d;
}

bool
Item::covers (Duple const & point) const
{
	Duple p = window_to_item (point);

	if (_bounding_box_dirty) {
		compute_bounding_box ();
	}

	boost::optional<Rect> r = bounding_box();

	if (!r) {
		return false;
	}

	return r.get().contains (p);
}

/* nesting/grouping API */

void
Item::render_children (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (_items.empty()) {
		return;
	}

	ensure_lut ();
	std::vector<Item*> items = _lut->get (area);

#ifdef CANVAS_DEBUG
	if (DEBUG_ENABLED(PBD::DEBUG::CanvasRender)) {
		cerr << string_compose ("%1%7 %2 @ %7 render %5 @ %6 %3 items out of %4\n", 
					_canvas->render_indent(), (name.empty() ? string ("[unnamed]") : name), items.size(), _items.size(), area, _position, this,
					whatami());
	}
#endif

	++render_depth;
		
	for (std::vector<Item*>::const_iterator i = items.begin(); i != items.end(); ++i) {

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
				cerr << _canvas->render_indent() << "Item " << (*i)->whatami() << " [" << (*i)->name << "] empty - skipped\n";
			}
#endif
			continue;
		}
		
		Rect item = (*i)->item_to_window (item_bbox.get(), false);
		boost::optional<Rect> d = item.intersection (area);
		
		if (d) {
			Rect draw = d.get();
			if (draw.width() && draw.height()) {
#ifdef CANVAS_DEBUG
				if (DEBUG_ENABLED(PBD::DEBUG::CanvasRender)) {
					if (dynamic_cast<Container*>(*i) == 0) {
						cerr << _canvas->render_indent() << "render "
						     << ' ' 
						     << (*i)
						     << ' '
						     << (*i)->whatami()
						     << ' '
						     << (*i)->name
						     << " item "
						     << item_bbox.get()
						     << " window = " 
						     << item
						     << " intersect = "
						     << draw
						     << " @ " 
						     << _position
						     << endl;
					}
				}
#endif

				(*i)->render (area, context);
				++render_count;
			}

		} else {

#ifdef CANVAS_DEBUG
			if (DEBUG_ENABLED(PBD::DEBUG::CanvasRender)) {
				cerr << string_compose ("%1skip render of %2 %3, no intersection between %4 and %5\n", _canvas->render_indent(), (*i)->whatami(),
							(*i)->name, item, area);
			}
#endif

		}
	}

	--render_depth;
}

void
Item::add_child_bounding_boxes() const
{
	boost::optional<Rect> self;
	Rect bbox;
	bool have_one = false;

	if (_bounding_box) {
		bbox = _bounding_box.get();
		have_one = true;
	}

	for (list<Item*>::const_iterator i = _items.begin(); i != _items.end(); ++i) {

		if (!(*i)->visible()) {
			continue;
		}

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
}

void
Item::add (Item* i)
{
	/* XXX should really notify canvas about this */

	_items.push_back (i);
	i->reparent (this);
	invalidate_lut ();
	_bounding_box_dirty = true;
}

void
Item::remove (Item* i)
{

	if (i->parent() != this) {
		return;
	}

	/* we cannot call bounding_box() here because that will iterate over
	   _items, one of which (the argument, i) may be in the middle of
	   deletion, making it impossible to call compute_bounding_box()
	   on it.
	*/

	if (_bounding_box) {
		_pre_change_bounding_box = _bounding_box;
	} else {
		_pre_change_bounding_box = Rect();
	}

	i->unparent ();
	_items.remove (i);
	invalidate_lut ();
	_bounding_box_dirty = true;
	
	end_change ();
}

void
Item::clear (bool with_delete)
{
	begin_change ();

	clear_items (with_delete);

	invalidate_lut ();
	_bounding_box_dirty = true;

	end_change ();
}

void
Item::clear_items (bool with_delete)
{
	for (list<Item*>::iterator i = _items.begin(); i != _items.end(); ) {

		list<Item*>::iterator tmp = i;
		Item *item = *i;

		++tmp;

		/* remove from list before doing anything else, because we
		 * don't want to find the item in _items during any activity
		 * driven by unparent-ing or deletion.
		 */

		_items.erase (i);
		item->unparent ();
		
		if (with_delete) {
			delete item;
		}

		i = tmp;
	}
}

void
Item::raise_child_to_top (Item* i)
{
	if (!_items.empty()) {
		if (_items.back() == i) {
			return;
		}
	}

	_items.remove (i);
	_items.push_back (i);

	invalidate_lut ();
        redraw ();
}

void
Item::raise_child (Item* i, int levels)
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
        redraw ();
}

void
Item::lower_child_to_bottom (Item* i)
{
	if (!_items.empty()) {
		if (_items.front() == i) {
			return;
		}
	}
	_items.remove (i);
	_items.push_front (i);
	invalidate_lut ();
        redraw ();
}

void
Item::ensure_lut () const
{
	if (!_lut) {
		_lut = new DumbLookupTable (*this);
	}
}

void
Item::invalidate_lut () const
{
	delete _lut;
	_lut = 0;
}

void
Item::child_changed ()
{
	invalidate_lut ();
	_bounding_box_dirty = true;

	if (_parent) {
		_parent->child_changed ();
	}
}

void
Item::add_items_at_point (Duple const point, vector<Item const *>& items) const
{
	boost::optional<Rect> const bbox = bounding_box ();

	/* Point is in window coordinate system */

	if (!bbox || !item_to_window (bbox.get()).contains (point)) {
		return;
	}

	/* recurse and add any items within our group that contain point.
	   Our children are only considered visible if we are, and similarly
	   only if we do not ignore events.
	*/

	vector<Item*> our_items;

	if (!_items.empty() && visible() && !_ignore_events) {
		ensure_lut ();
		our_items = _lut->items_at_point (point);
	}

	if (!our_items.empty() || covers (point)) {
		/* this adds this item itself to the list of items at point */
		items.push_back (this);
	}

	for (vector<Item*>::iterator i = our_items.begin(); i != our_items.end(); ++i) {
		(*i)->add_items_at_point (point, items);
	}
}

void
Item::set_tooltip (const std::string& s)
{
	_tooltip = s;
}

void
Item::start_tooltip_timeout ()
{
	if (!_tooltip.empty()) {
		_canvas->start_tooltip_timeout (this);
	}
}

void
Item::stop_tooltip_timeout ()
{
	_canvas->stop_tooltip_timeout ();
}

void
Item::dump (ostream& o) const
{
	boost::optional<ArdourCanvas::Rect> bb = bounding_box();

	o << _canvas->indent() << whatami() << ' ' << this << " self-Visible ? " << self_visible() << " visible ? " << visible();
	o << " @ " << position();
	
#ifdef CANVAS_DEBUG
	if (!name.empty()) {
		o << ' ' << name;
	}
#endif

	if (bb) {
		o << endl << _canvas->indent() << "\tbbox: " << bb.get();
		o << endl << _canvas->indent() << "\tCANVAS bbox: " << item_to_canvas (bb.get());
	} else {
		o << " bbox unset";
	}

	o << endl;

	if (!_items.empty()) {

#ifdef CANVAS_DEBUG
		o << _canvas->indent();
		o << " @ " << position();
		o << " Items: " << _items.size();
		o << " Self-Visible ? " << self_visible();
		o << " Visible ? " << visible();
		
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
}

ostream&
ArdourCanvas::operator<< (ostream& o, const Item& i)
{
	i.dump (o);
	return o;
}

