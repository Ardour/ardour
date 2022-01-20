/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
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

#ifndef __CANVAS_ITEM_H__
#define __CANVAS_ITEM_H__

#include <stdint.h>

#include <gdk/gdk.h>

#include <cairomm/context.h>

#include "pbd/signals.h"

#include "canvas/fill.h"
#include "canvas/lookup_table.h"
#include "canvas/outline.h"
#include "canvas/types.h"
#include "canvas/visibility.h"

namespace ArdourCanvas
{

class Canvas;
class ScrollGroup;
class ConstrainedItem;

/** The parent class for anything that goes on the canvas.
 *
 *  Items have a position, which is expressed in the coordinates of the parent.
 *  They also have a bounding box, which describes the area in which they have
 *  drawable content, which is expressed in their own coordinates (whose origin
 *  is at the item position).
 *
 *  Any item that is being displayed on a canvas has a pointer to that canvas,
 *  and all except the `root group' have a pointer to their parent group.
 */

class LIBCANVAS_API Item : public Fill, public Outline
{
public:
	Item (Canvas *);
	Item (Item *);
	Item (Item *, Duple const& p);
	virtual ~Item ();

	void redraw () const;

	/** Render this item to a Cairo context.
	 *  @param area Area to draw, in **window** coordinates
	 *
	 *  Items must convert their own coordinates into window coordinates
	 *  because Cairo is limited to a fixed point coordinate space that
	 *  does not extend as far as the Ardour timeline. All rendering must
	 *  be done using coordinates that do not exceed the (rough) limits
	 *  of the canvas' window, to avoid odd errors within Cairo as it
	 *  converts doubles into its fixed point format and then tesselates
	 *  the results.
	 */
	virtual void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const = 0;

	/** Item has changed will be rendered in next render pass so give item a
	 * chance to perhaps schedule work in another thread etc.
	 *
	 *  @param area Area to draw, in **window** coordinates
	 */
	virtual void prepare_for_render (Rect const & area) const { }

	/** Adds one or more items to the vector \p items based on their
	 * covering \p point which is in window coordinates
	 *
	 * Note that Item::add_items_at_window_point() is only intended to be
	 * called on items already looked up in a LookupTable (i.e. by a
	 * parent) and thus known to cover \p point already.
	 *
	 * Derived classes may add more items than themselves (e.g. containers).
	 */
	virtual void add_items_at_point (Duple point, std::vector<Item const *>& items) const;

	/** Test if point is inside the item
	 *
	 * @param point The point is in window coordinates
	 * @return true if the item covers \p point , false otherwise.
	 */
	virtual bool covers (Duple const& point) const;

	/** Update _bounding_box and _bounding_box_dirty */
	virtual void compute_bounding_box () const = 0;

	void grab ();
	void ungrab ();

	void unparent ();
	void reparent (Item *, bool already_added = false);

	/** @return Parent group, or 0 if this is the root group */
	Item* parent () const {
		return _parent;
	}

	uint32_t depth() const;
	const Item* closest_ancestor_with (const Item& other) const;
	bool common_ancestor_within (uint32_t, const Item& other) const;

	/** returns true if this item is an ancestor of \p candidate ,
	 * and false otherwise.
	 */
	bool is_ancestor_of (const Item& candidate) const {
		return candidate.is_descendant_of (*this);
	}

	/** returns true if this Item is a descendant of \p candidate ,
	 * and false otherwise.
	 */
	bool is_descendant_of (const Item& candidate) const;

	void set_position (Duple);
	void set_x_position (Coord);
	void set_y_position (Coord);
	void move (Duple);

	/** @return Position of this item in the parent's coordinates */
	Duple position () const {
		return _position;
	}

	Duple window_origin() const;
	Duple canvas_origin() const;

	ScrollGroup* scroll_parent() const { return _scroll_parent; }

	/* layout-related methods */

	virtual void size_request (double& w, double& h) const;
	void set_size_request (double w, double h);
	void set_size_request_to_display_given_text (const std::vector<std::string>& strings, gint hpadding, gint vpadding);

	void size_allocate (Rect const&);
	virtual void _size_allocate (Rect const&);
	virtual void size_allocate_children (Rect const & r);
	Rect allocation() const { return _allocation; }
	void set_layout_sensitive (bool);
	bool layout_sensitive () const { return _layout_sensitive; }

	/** bounding box is the public API to get the area covered by the item
	 * (which may differ from its allocation). The returned Rect is in item
	 * coordinates (i.e. x0,y0 = 0,0 mean that the upper left corner of the
	 * bounding box is at the item's _position).
	 */
	Rect bounding_box () const;

	Coord height() const;
	Coord width() const;

	Duple item_to_parent (Duple const &) const;
	Rect item_to_parent (Rect const &) const;
	Duple parent_to_item (Duple const &) const;
	Rect parent_to_item (Rect const &) const;

	/* XXX: it's a pity these two aren't the same form as item_to_parent etc.,
	 * but it makes a bit of a mess in the rest of the code if they are not.
	 */
	void canvas_to_item (Coord &, Coord &) const;
	void item_to_canvas (Coord &, Coord &) const;

	Duple canvas_to_item (Duple const&) const;
	Rect item_to_canvas (Rect const&) const;
	Duple item_to_canvas (Duple const&) const;
	Rect canvas_to_item (Rect const&) const;

	Duple item_to_window (Duple const&, bool rounded = true) const;
	Duple window_to_item (Duple const&) const;
	Rect item_to_window (Rect const&, bool rounded = true) const;
	Rect window_to_item (Rect const&) const;

	void raise_to_top ();
	void raise (int);
	void lower_to_bottom ();

	virtual void hide ();
	virtual void show ();

	/** @return true if this item is visible (ie it will be rendered),
	 *  otherwise false
	 */
	bool self_visible () const {
		return _visible;
	}

	bool visible () const;

	/** @return Our canvas, or 0 if we are not attached to one */
	Canvas* canvas () const {
		return _canvas;
	}

	void set_ignore_events (bool);
	bool ignore_events () const {
		return _ignore_events;
	}

	void set_data (std::string const &, void *);
	void* get_data (std::string const &) const;

	/* nested item ("grouping") API */
	virtual void add (Item *);
	virtual void add_front (Item *);
	virtual void remove (Item *);
	/* XXX this should become virtual also */
	void clear (bool with_delete = false);

	std::list<Item*> const & items () const {
		return _items;
	}

	void raise_child_to_top (Item *);
	void raise_child (Item *, int);
	void lower_child_to_bottom (Item *);
	virtual void child_changed (bool bbox_changed);

	PackOptions pack_options () const { return _pack_options; }
	void set_pack_options (PackOptions);

	static int default_items_per_cell;


	/* This is a sigc++ signal because it is solely
		 concerned with GUI stuff and is thus single-threaded
		 */

	template <class T>
		struct EventAccumulator {
			typedef T result_type;
			template <class U>
				result_type operator () (U first, U last) {
					while (first != last) {
						if (*first) {
							return true;
						}
						++first;
					}
					return false;
				}
		};

	sigc::signal1<bool, GdkEvent*, EventAccumulator<bool> > Event;

#ifdef CANVAS_DEBUG
	std::string name;
	std::string whoami() const { return whatami() + '/' + name; }
#else
	std::string whoami() const { return whatami(); }
#endif

#ifdef CANVAS_COMPATIBILITY
	void grab_focus ();
#endif

	const std::string& tooltip () const { return _tooltip; }
	void set_tooltip (const std::string&);

	void start_tooltip_timeout ();
	void stop_tooltip_timeout ();

	virtual void dump (std::ostream&) const;
	std::string whatami() const;

        bool resize_queued() const { return _resize_queued; }
        void queue_resize();

	bool scroll_translation() const { return _scroll_translation; }
	void disable_scroll_translation ();

        /* only derived containers need to implement this, but this
           is where they compute the sizes and position and their
           children. A fixed-layout container (i.e. one where every child
           has just had its position fixed via ::set_position()) does not
           need to do anything here. Only box/table/grid style containers,
           where the position of one child depends on the position and size of
           other children, need to provide an implementation.
        */
        virtual void layout();

  protected:
	friend class Fill;
	friend class Outline;

	/** To be called at the beginning of any property change that
	 *  may alter the bounding box of this item
	 */
	void begin_change ();
	/** To be called at the endof any property change that
	 *  may alter the bounding box of this item
	 */
	void end_change ();
	/** To be called at the beginning of any property change that
	 *  does NOT alter the bounding box of this item
	 */
	void begin_visual_change ();
	/** To be called at the endof any property change that
	 *  does NOT alter the bounding box of this item
	 */
	void end_visual_change ();

	Canvas* _canvas;
	/** parent group; may be 0 if we are the root group or if we have been unparent()ed */
	Item* _parent;
	/** scroll parent group; may be 0 if we are the root group or if we have been unparent()ed */
	ScrollGroup* _scroll_parent;
	/** position of this item in parent coordinates */
	Duple _position;
	/** true if this item is visible (ie to be drawn), otherwise false */
	bool _visible;
	/** our bounding box before any change that is currently in progress */
	Rect _pre_change_bounding_box;

	/** our bounding box; may be out of date if _bounding_box_dirty is true */
	mutable Rect _bounding_box;
	/** true if _bounding_box might be out of date, false if its definitely not */
	mutable bool _bounding_box_dirty;
	PackOptions _pack_options;

	void bb_clean () const;

	Rect _allocation;
	bool _layout_sensitive;

	/* XXX: this is a bit grubby */
	std::map<std::string, void *> _data;

	/* nesting ("grouping") API */

	void invalidate_lut () const;
	void clear_items (bool with_delete);

	void ensure_lut () const;
	mutable LookupTable* _lut;
	/* our items, from lowest to highest in the stack */
	std::list<Item*> _items;

	void add_child_bounding_boxes (bool include_hidden = false) const;
	void render_children (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const;
	void prepare_for_render_children (Rect const & area) const;

	Duple scroll_offset() const;
  public:
	Duple position_offset() const;

	bool _resize_queued;
	double _requested_width;
	double _requested_height;

private:
	void init ();

	std::string _tooltip;
	bool _ignore_events;
	bool _scroll_translation;
	
	void find_scroll_parent ();
	void propagate_show_hide ();
};

extern LIBCANVAS_API std::ostream& operator<< (std::ostream&, const ArdourCanvas::Item&);

}

#endif
