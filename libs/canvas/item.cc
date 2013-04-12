#include "pbd/compose.h"
#include "pbd/stacktrace.h"
#include "pbd/xml++.h"
#include "pbd/convert.h"

#include "ardour/utils.h"

#include "canvas/group.h"
#include "canvas/item.h"
#include "canvas/canvas.h"
#include "canvas/debug.h"

using namespace std;
using namespace PBD;
using namespace ArdourCanvas;

Item::Item (Canvas* canvas)
	: _canvas (canvas)
	, _parent (0)
{
	init ();
}

Item::Item (Group* parent)
	: _canvas (parent->canvas ())
	, _parent (parent)
{
	init ();
}

Item::Item (Group* parent, Duple position)
	: _canvas (parent->canvas())
	, _parent (parent)
	, _position (position)
{
	init ();
}

void
Item::init ()
{
	_visible = true;
	_bounding_box_dirty = true;
	_ignore_events = false;
	
	if (_parent) {
		_parent->add (this);
	}

	DEBUG_TRACE (DEBUG::CanvasItems, string_compose ("new canvas item %1\n", this));
}	

Item::~Item ()
{
	if (_canvas) {
		_canvas->item_going_away (this, _bounding_box);
	}
	
	if (_parent) {
		_parent->remove (this);
	}
}

Rect
Item::item_to_parent (Rect const & r) const
{
	return r.translate (_position);
}

/** Set the position of this item in the parent's coordinates */
void
Item::set_position (Duple p)
{
	boost::optional<Rect> bbox = bounding_box ();
	boost::optional<Rect> pre_change_parent_bounding_box;

	if (bbox) {
		pre_change_parent_bounding_box = item_to_parent (bbox.get());
	}
	
	_position = p;

	_canvas->item_moved (this, pre_change_parent_bounding_box);

	if (_parent) {
		_parent->child_changed ();
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
	assert (_parent);
	_parent->raise_child_to_top (this);
}

void
Item::raise (int levels)
{
	assert (_parent);
	_parent->raise_child (this, levels);
}

void
Item::lower_to_bottom ()
{
	assert (_parent);
	_parent->lower_child_to_bottom (this);
}

void
Item::hide ()
{
	_visible = false;
	_canvas->item_shown_or_hidden (this);
}

void
Item::show ()
{
	_visible = true;
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

Rect
Item::parent_to_item (Rect const & d) const
{
	return d.translate (- _position);
}

void
Item::unparent ()
{
	_canvas = 0;
	_parent = 0;
}

void
Item::reparent (Group* new_parent)
{
	if (_parent) {
		_parent->remove (this);
	}

	assert (new_parent);

	_parent = new_parent;
	_canvas = _parent->canvas ();
	_parent->add (this);
}

void
Item::grab_focus ()
{
	/* XXX */
}

/** @return Bounding box in this item's coordinates */
boost::optional<Rect>
Item::bounding_box () const
{
	if (_bounding_box_dirty) {
		compute_bounding_box ();
	}

	assert (!_bounding_box_dirty);
	return _bounding_box;
}

Coord
Item::height () const 
{
	boost::optional<Rect> bb  = bounding_box();

	if (bb) {
		return bb->height ();
	}
	return 0;
}

Coord
Item::width () const 
{
	boost::optional<Rect> bb = bounding_box().get();

	if (bb) {
		return bb->width ();
	}

	return 0;
}

/* XXX may be called even if bbox is not changing ... bit grotty */
void
Item::begin_change ()
{
	_pre_change_bounding_box = bounding_box ();
}

void
Item::end_change ()
{
	_canvas->item_changed (this, _pre_change_bounding_box);
	
	if (_parent) {
		_parent->child_changed ();
	}
}

void
Item::move (Duple movement)
{
	set_position (position() + movement);
}

void
Item::add_item_state (XMLNode* node) const
{
	node->add_property ("x-position", string_compose ("%1", _position.x));
	node->add_property ("y-position", string_compose ("%1", _position.y));
	node->add_property ("visible", _visible ? "yes" : "no");
}

void
Item::set_item_state (XMLNode const * node)
{
	_position.x = atof (node->property("x-position")->value().c_str());
	_position.y = atof (node->property("y-position")->value().c_str());
	_visible = PBD::string_is_affirmative (node->property("visible")->value());
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
Item::item_to_canvas (Coord& x, Coord& y) const
{
	Duple d (x, y);
	Item const * i = this;
	
	while (i) {
		d = i->item_to_parent (d);
		i = i->_parent;
	}
		
	x = d.x;
	y = d.y;
}

void
Item::canvas_to_item (Coord& x, Coord& y) const
{
	Duple d (x, y);
	Item const * i = this;

	while (i) {
		d = i->parent_to_item (d);
		i = i->_parent;
	}

	x = d.x;
	y = d.y;
}

Rect
Item::item_to_canvas (Rect const & area) const
{
	Rect r = area;
	Item const * i = this;

	while (i) {
		r = i->item_to_parent (r);
		i = i->parent ();
	}

	return r;
}

void
Item::set_ignore_events (bool ignore)
{
	_ignore_events = ignore;
}

void
Item::dump (ostream& o) const
{
	boost::optional<Rect> bb = bounding_box();

	o << _canvas->indent() << whatami() << ' ' << this;
	
#ifdef CANVAS_DEBUG
	if (!name.empty()) {
		o << ' ' << name;
	}
#endif

	if (bb) {
		o << endl << _canvas->indent() << "\tbbox: " << bb.get();
	} else {
		o << " bbox unset";
	}

	o << endl;
}

std::string
Item::whatami () const 
{
	std::string type = demangle (typeid (*this).name());
	return type.substr (type.find_last_of (':') + 1);
}

ostream&
ArdourCanvas::operator<< (ostream& o, const Item& i)
{
	i.dump (o);
	return o;
}
