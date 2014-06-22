/*
    Copyright (C) 2011-2014 Paul Davis
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

#include "canvas/canvas.h"
#include "canvas/debug.h"
#include "canvas/layout.h"

using namespace std;
using namespace PBD;
using namespace ArdourCanvas;

Layout::Layout (Canvas* canvas) 
	: Container (canvas)
{
}

Layout::Layout (Item* parent) 
	: Container (parent)
{
}

Layout::Layout (Item* parent, Duple const & p) 
	: Container (parent, p)
{
}

/** @param area Area to draw in window coordinates.
 *  @param context Context, set up with its origin at this layout's position.
 */
void
Layout::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	ensure_lut ();
	std::vector<Item*> items = _lut->get (area);

#ifdef CANVAS_DEBUG
	if (DEBUG_ENABLED(PBD::DEBUG::CanvasRender)) {
		cerr << string_compose ("%1GROUP %2 @ %7 render %5 @ %6 %3 items out of %4\n", 
					_canvas->render_indent(), (name.empty() ? string ("[unnamed]") : name), items.size(), _items.size(), area, _position, this);
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
		
		Rect item = (*i)->item_to_window (item_bbox.get());
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

