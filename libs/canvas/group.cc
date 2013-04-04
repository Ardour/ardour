#include <iostream>
#include <cairomm/context.h>
#include "pbd/stacktrace.h"
#include "pbd/xml++.h"
#include "canvas/group.h"
#include "canvas/types.h"
#include "canvas/debug.h"
#include "canvas/item_factory.h"

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

	for (vector<Item*>::const_iterator i = items.begin(); i != items.end(); ++i) {
		if (!(*i)->visible ()) {
			continue;
		}
		
		boost::optional<Rect> item_bbox = (*i)->bounding_box ();
		if (!item_bbox) {
			continue;
		}

		/* convert the render area to our child's coordinates */
		Rect const item_area = (*i)->parent_to_item (area);

		/* intersect the child's render area with the child's bounding box */
		boost::optional<Rect> r = item_bbox.get().intersection (item_area);

		if (r) {
			/* render the intersection */
			context->save ();
			context->translate ((*i)->position().x, (*i)->position().y);
			(*i)->render (r.get(), context);
			context->restore ();
			++render_count;
		}
	}
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
	_items.push_back (i);
	invalidate_lut ();
	_bounding_box_dirty = true;
	
	DEBUG_TRACE (PBD::DEBUG::CanvasItemsDirtied, "canvas item dirty: group add\n");
}

void
Group::remove (Item* i)
{
	_items.remove (i);
	invalidate_lut ();
	_bounding_box_dirty = true;
	
	DEBUG_TRACE (PBD::DEBUG::CanvasItemsDirtied, "canvas item dirty: group remove\n");
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
	boost::optional<Rect> const bbox = bounding_box ();

	if (!bbox || !bbox.get().contains (point)) {
		return;
	}

	Item::add_items_at_point (point, items);
	
	ensure_lut ();
	
	vector<Item*> our_items = _lut->items_at_point (point);
	for (vector<Item*>::iterator i = our_items.begin(); i != our_items.end(); ++i) {
		(*i)->add_items_at_point (point - (*i)->position(), items);
	}
}

XMLNode *
Group::get_state () const
{
	XMLNode* node = new XMLNode ("Group");
	for (list<Item*>::const_iterator i = _items.begin(); i != _items.end(); ++i) {
		node->add_child_nocopy (*(*i)->get_state ());
	}

	add_item_state (node);
	return node;
}

void
Group::set_state (XMLNode const * node)
{
	set_item_state (node);

	XMLNodeList const & children = node->children ();
	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
		/* this will create the item and add it to this group */
		create_item (this, *i);
	}
}
