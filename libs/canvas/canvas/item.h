/*
    Copyright (C) 2011-2013 Paul Davis
    Original Author: Carl Hetherington <cth@carlh.net>

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

#ifndef __CANVAS_ITEM_H__
#define __CANVAS_ITEM_H__

#include <stdint.h>

#include <gdk/gdk.h>

#include <cairomm/context.h>

#include "pbd/signals.h"

#include "canvas/types.h"

namespace ArdourCanvas
{

class Canvas;
class Group;
class Rect;	

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
	
class Item
{
public:
	Item (Canvas *);
	Item (Group *);
	Item (Group *, Duple);
	virtual ~Item ();

	/** Render this item to a Cairo context.
	 *  @param area Area to draw in this item's coordinates.
	 */
	virtual void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const = 0;

	virtual void add_items_at_point (Duple, std::vector<Item const *>& items) const {
		items.push_back (this);
	}

	/** Update _bounding_box and _bounding_box_dirty */
	virtual void compute_bounding_box () const = 0;

	void grab ();
	void ungrab ();
	
	void unparent ();
	void reparent (Group *);

	/** @return Parent group, or 0 if this is the root group */
	Group* parent () const {
		return _parent;
	}

	void set_position (Duple);
	void set_x_position (Coord);
	void set_y_position (Coord);
	void move (Duple);

	/** @return Position of this item in the parent's coordinates */
	Duple position () const {
		return _position;
	}

	boost::optional<Rect> bounding_box () const;
        Coord height() const;
        Coord width() const;

	Duple item_to_parent (Duple const &) const;
	Rect item_to_parent (Rect const &) const;
	Duple parent_to_item (Duple const &) const;
	Rect parent_to_item (Rect const &) const;
	/* XXX: it's a pity these aren't the same form as item_to_parent etc.,
	   but it makes a bit of a mess in the rest of the code if they are not.
	*/

        void canvas_to_item (Coord &, Coord &) const;
        Duple canvas_to_item (Duple const &) const;
	void item_to_canvas (Coord &, Coord &) const;
	Rect item_to_canvas (Rect const &) const;
        Duple item_to_canvas (Duple const &) const;

	void raise_to_top ();
	void raise (int);
	void lower_to_bottom ();

        void hide ();
	void show ();

	/** @return true if this item is visible (ie it will be rendered),
	 *  otherwise false
	 */
	bool visible () const {
		return _visible;
	}

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
#endif
	
#ifdef CANVAS_COMPATIBILITY
	void grab_focus ();
#endif	

        virtual void dump (std::ostream&) const;
        std::string whatami() const;

protected:

	void begin_change ();
	void end_change ();

	Canvas* _canvas;
	/** parent group; may be 0 if we are the root group or if we have been unparent()ed */
	Group* _parent;
	/** position of this item in parent coordinates */
	Duple _position;
	/** true if this item is visible (ie to be drawn), otherwise false */
	bool _visible;
	/** our bounding box before any change that is currently in progress */
	boost::optional<Rect> _pre_change_bounding_box;

	/** our bounding box; may be out of date if _bounding_box_dirty is true */
	mutable boost::optional<Rect> _bounding_box;
	/** true if _bounding_box might be out of date, false if its definitely not */
	mutable bool _bounding_box_dirty;

	/* XXX: this is a bit grubby */
	std::map<std::string, void *> _data;

private:
	void init ();

	bool _ignore_events;
};

extern std::ostream& operator<< (std::ostream&, const ArdourCanvas::Item&);

}


#endif
