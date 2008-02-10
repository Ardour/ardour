/*
    Copyright (C) 2007 Paul Davis
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

#include "lineset.h"
#include "rgb_macros.h"

#include <libgnomecanvas/libgnomecanvas.h>
#include <libgnomecanvasmm/group.h>
#include <libgnomecanvasmm/canvas.h>

#include <algorithm>
#include <cmath>
#include <iostream>

using namespace std;

namespace Gnome {
namespace Canvas {

LinesetClass Lineset::lineset_class;

static const char* overlap_error_str = "Lineset error: Line overlap";

Lineset::Line::Line(double c, double w, uint32_t color)
	: coord(c)
	, width(w) {
	UINT_TO_RGBA (color, &r, &g, &b, &a);
}

/* Constructor for dummy lines that are used only with the coordinate */
Lineset::Line::Line(double c)
	: coord(c) {
}

void
Lineset::Line::set_color(uint32_t color) {
	UINT_TO_RGBA (color, &r, &g, &b, &a);
}

const Glib::Class&
LinesetClass::init() {
	if(!gtype_) {
		class_init_func_ = &LinesetClass::class_init_function;
		register_derived_type(Item::get_type());
	}

	return *this;
}

void
LinesetClass::class_init_function(void* g_class, void* class_data) {
}

Lineset::Lineset(Group& parent, Orientation o)
	: Glib::ObjectBase("GnomeCanvasLineset")
	, Item(Glib::ConstructParams(lineset_class.init()))
	, cached_pos(lines.end())
	, orientation(o)
	, x1(*this, "x1", 0.0)
	, y1(*this, "y1", 0.0)
	, x2(*this, "x2", 0.0)
	, y2(*this, "y2", 0.0)
	, in_update(false)
	, update_region1(1.0)
	, update_region2(0.0)
	, bounds_changed(false)
	, covered1(1.0) // covered1 > covered2 ==> nothing's covered
	, covered2(0.0) {

	item_construct(parent);

	property_x1().signal_changed().connect(mem_fun(*this, &Lineset::bounds_need_update));
	property_y1().signal_changed().connect(mem_fun(*this, &Lineset::bounds_need_update));
	property_x2().signal_changed().connect(mem_fun(*this, &Lineset::bounds_need_update));
	property_y2().signal_changed().connect(mem_fun(*this, &Lineset::bounds_need_update));
}

Lineset::~Lineset() {
}

bool
Lineset::line_compare(const Line& a, const Line& b) {
	return a.coord < b.coord;
}

void
Lineset::print_lines() {
	for(Lines::iterator it = lines.begin(); it != lines.end(); ++it) {
		cerr << "   " << it->coord << " " << it->width << " " << (int)it->r << " " << (int)it->g << " " << (int)it->b << " " << (int)it->a << endl;
	}
}

void
Lineset::move_line(double coord, double dest) {
	if(coord == dest) {
		return;
	}

	Lines::iterator it = line_at(coord);

	if(it != lines.end()) {
		

		double width = it->width;
		it->coord = dest;

		Lines::iterator ins = lower_bound(lines.begin(), lines.end(), *it, line_compare);

		lines.insert(ins, *it);
		lines.erase(it);

		if(coord > dest) {
			region_needs_update(dest, coord + width);
		}
		else {
			region_needs_update(coord, dest + width);
		}
	}
}

void
Lineset::change_line_width(double coord, double width) {
	Lines::iterator it = line_at(coord);

	if(it != lines.end()) {
		Line& l = *it;
		++it;

		if(it != lines.end()) {
			if(l.coord + width > it->coord) {
				cerr << overlap_error_str << endl;
				return;
			}
		}

		l.width = width;
		region_needs_update(coord, coord + width);
	}
}

void
Lineset::change_line_color(double coord, uint32_t color) {
	Lines::iterator it = line_at(coord);

	if(it != lines.end()) {
		it->set_color(color);
		region_needs_update(it->coord, it->coord + it->width);
	}
}

void
Lineset::add_line(double coord, double width, uint32_t color) {
	Line l(coord, width, color);

	Lines::iterator it = std::lower_bound(lines.begin(), lines.end(), l, line_compare);
	
	/* overlap checking */
	if(it != lines.end()) {
		if(l.coord + l.width > it->coord) {
			cerr << overlap_error_str << endl;
			return;
		}
	}
	if(it != lines.begin()) {
		--it;
		if(l.coord < it->coord + it->width) {
			cerr << overlap_error_str << endl;
			return;
		}
		++it;
	}
	
	lines.insert(it, l);
	region_needs_update(coord, coord + width);
}

void
Lineset::remove_line(double coord) {
	Lines::iterator it = line_at(coord);

	if(it != lines.end()) {
		double start = it->coord;
		double end = start + it->width;

		lines.erase(it);

		region_needs_update(start, end);
	}
}

void
Lineset::remove_lines(double c1, double c2) {
	if(!lines.empty()) {
		region_needs_update(c1, c2);
	}
}

void
Lineset::remove_until(double coord) {
	if(!lines.empty()) {
		double first = lines.front().coord;
		
		// code

		region_needs_update(first, coord);
	}
}
	
void
Lineset::remove_from(double coord) {
	if(!lines.empty()) {
		double last = lines.back().coord + lines.back().width;

		// code

		region_needs_update(coord, last);
	}
}

void
Lineset::clear() {
	if(!lines.empty()) {
		double coord1 = lines.front().coord;
		double coord2 = lines.back().coord + lines.back().width;
			
		lines.clear();
		region_needs_update(coord1, coord2);
	}
}

/*
 * this function is optimized to work faster if we access elements that are adjacent to each other.
 * so if a large number of lines are modified, it is wise to modify them in sorted order.
 */
Lineset::Lines::iterator
Lineset::line_at(double coord) {
	if(cached_pos != lines.end()) {
		if(coord < cached_pos->coord) {
			/* backward search */
			while(--cached_pos != lines.end()) {
				if(cached_pos->coord <= coord) {
					if(cached_pos->coord + cached_pos->width < coord) {
						/* coord is between two lines */
						return lines.end();
					}
					else {
						return cached_pos;
					}
				}
			}
		}
		else {
			/* forward search */
			while(cached_pos != lines.end()) {
				if(cached_pos->coord > coord) {
					/* we searched past the line that we want, so now see
					   if the previous line includes the coordinate */
					--cached_pos;
					if(cached_pos->coord + cached_pos->width >= coord) {
						return cached_pos;
					}
					else {
						return lines.end();
					}
				}
				++cached_pos;
			}
		}
	}
	else {
		/* initialize the cached position */
		Line dummy(coord);

		cached_pos = lower_bound(lines.begin(), lines.end(), dummy, line_compare);
		
		/* The iterator found should point to the element after the one we want. */
		--cached_pos;
		
		if(cached_pos != lines.end()) {
			if(cached_pos->coord <= coord) {
				if(cached_pos->coord + cached_pos->width >= coord) {
					return cached_pos;
				}
				else {
					return lines.end();
				}
			}
			else {
				return lines.end();
			}
		}
		else {
			return lines.end();
		}
	}

	return lines.end();
}

void
Lineset::redraw_request(ArtIRect& r) {
	get_canvas()->request_redraw(r.x0, r.y0, r.x1, r.y1);
}

void
Lineset::redraw_request(ArtDRect& r) {
	int x0, y0, x1, y1;
	Canvas& cv = *get_canvas();

	//cerr << "redraw request: " << r.x0 << " " << r.y0 << " " << r.x1 << " " << r.y1 << endl;

	cv.w2c(r.x0, r.y0, x0, y0);
	cv.w2c(r.x1, r.y1, x1, y1);
	cv.request_redraw(x0, y0, x1, y1);
}

void
Lineset::update_lines(bool need_redraw) {
	//cerr << "update_lines need_redraw=" << need_redraw << endl;
	if(!need_redraw) {
		update_region1 = 1.0;
		update_region2 = 0.0;
		return;
	}

	if(update_region2 > update_region1) {
		ArtDRect redraw;
		Lineset::bounds_vfunc(&redraw.x0, &redraw.y0, &redraw.x1, &redraw.y1);
		i2w(redraw.x0, redraw.y0);
		i2w(redraw.x1, redraw.y1);
		
		if(orientation == Vertical) {
			redraw.x1 = redraw.x0 + update_region2;
			redraw.x0 += update_region1;
		}
		else {
			redraw.y1 = redraw.y0 + update_region2;
			redraw.y0 += update_region1;
		}
		redraw_request(redraw);
		update_region1 = 1.0;
		update_region2 = 0.0;
	}

	// if we need to calculate what becomes visible, use some of this
	//cv.c2w (0, 0, world_v[X1], world_v[Y1]);
	//cv.c2w (cv.get_width(), cv.get_height(), world_v[X2], world_v[Y2]);
}

/*
 * return false if a full redraw request has been made.
 * return true if nothing or only parts of the rect area has been requested for redraw
 */
bool
Lineset::update_bounds() {
	GnomeCanvasItem* item = GNOME_CANVAS_ITEM(gobj());
	ArtDRect old_b;
	ArtDRect new_b;
	ArtDRect redraw;
	Canvas& cv = *get_canvas();

	/* store the old bounding box */
	old_b.x0 = item->x1;
	old_b.y0 = item->y1;
	old_b.x1 = item->x2;
	old_b.y1 = item->y2;
	Lineset::bounds_vfunc(&new_b.x0, &new_b.y0, &new_b.x1, &new_b.y1);

	i2w(new_b.x0, new_b.y0);
	i2w(new_b.x1, new_b.y1);

	item->x1 = new_b.x0;
	item->y1 = new_b.y0;
	item->x2 = new_b.x1;
	item->y2 = new_b.y1;
	
	/* Update bounding box used in rendering function */
	cv.w2c(new_b.x0, new_b.y0, bbox.x0, bbox.y0);
	cv.w2c(new_b.x1, new_b.y1, bbox.x1, bbox.y1);

	/*
	 * if the first primary axis property (x1 for Vertical, y1 for Horizontal) changed, we must redraw everything,
	 * because lines are positioned relative to this coordinate. Please excuse the confusion resulting from
	 * gnome canvas coordinate numbering (1, 2) and libart's (0, 1).
	 */
	if(orientation == Vertical) {
		if(new_b.x0 == old_b.x0) {
			/* No need to update everything */
			if(new_b.y0 != old_b.y0) {
				redraw.x0 = old_b.x0;
				redraw.y0 = min(old_b.y0, new_b.y0);
				redraw.x1 = old_b.x1;
				redraw.y1 = max(old_b.y0, new_b.y0);
				redraw_request(redraw);
			}
			if(new_b.y1 != old_b.y1) {
				redraw.x0 = old_b.x0;
				redraw.y0 = min(old_b.y1, new_b.y1);
				redraw.x1 = old_b.x1;
				redraw.y1 = max(old_b.y1, new_b.y1);
				redraw_request(redraw);
			}
			
			if(new_b.x1 > old_b.x1) {
				// we have a larger area ==> possibly more lines
				request_lines(old_b.x1, new_b.x1);
				redraw.x0 = old_b.x1;
				redraw.y0 = min(old_b.y0, new_b.y0);
				redraw.x1 = new_b.x1;
				redraw.y1 = max(old_b.y1, new_b.y1);
				redraw_request(redraw);
			}
			else if(new_b.x1 < old_b.x1) {
				remove_lines(new_b.x1, old_b.x1);
				redraw.x0 = new_b.x1;
				redraw.y0 = min(old_b.y0, new_b.y0);
				redraw.x1 = old_b.x1;
				redraw.y1 = max(old_b.y1, new_b.y1);
				redraw_request(redraw);
			}
			return true;
		}
		else {
			/* update everything */
			//cerr << "update everything" << endl;
			art_drect_union(&redraw, &old_b, &new_b);
			redraw_request(redraw);
			return false;
		}
	}
	else {
		if(new_b.y0 == old_b.y0) {
			/* No need to update everything */
			if(new_b.x0 != old_b.x0) {
				redraw.y0 = old_b.y0;
				redraw.x0 = min(old_b.x0, new_b.x0);
				redraw.y1 = old_b.y1;
				redraw.x1 = max(old_b.x0, new_b.x0);
				redraw_request(redraw);
			}
			if(new_b.x1 != old_b.x1) {
				redraw.y0 = old_b.y0;
				redraw.x0 = min(old_b.x1, new_b.x1);
				redraw.y1 = old_b.y1;
				redraw.x1 = max(old_b.x1, new_b.x1);
				redraw_request(redraw);
			}
			
			if(new_b.y1 > old_b.y1) {
				// we have a larger area ==> possibly more lines
				request_lines(old_b.y1, new_b.y1);
				redraw.y0 = old_b.y1;
				redraw.x0 = min(old_b.x0, new_b.x0);
				redraw.y1 = new_b.y1;
				redraw.x1 = max(old_b.x1, new_b.x1);
				redraw_request(redraw);
			}
			else if(new_b.y1 < old_b.y1) {
				remove_lines(new_b.y1, old_b.y1);
				redraw.y0 = new_b.y1;
				redraw.x0 = min(old_b.x0, new_b.x0);
				redraw.y1 = old_b.y1;
				redraw.x1 = max(old_b.x1, new_b.x1);
				redraw_request(redraw);
			}
			return true;
		}
		else {
			/* update everything */
			art_drect_union(&redraw, &old_b, &new_b);
			redraw_request(redraw);
			return false;
		}
	}
}

/*
 * what to do here?
 * 1. find out if any line data has been modified since last update.
 * N. find out if the item moved. if it moved, the old bbox and the new bbox need to be updated.
 */
void
Lineset::update_vfunc(double* affine, ArtSVP* clip_path, int flags) {
	GnomeCanvasItem* item = GNOME_CANVAS_ITEM(gobj());
	bool lines_need_redraw = true;

	/*
	 * need to call gnome_canvas_item_update here, to unset the need_update flag.
	 * but a call to Gnome::Canvas::Item::update_vfunc results in infinite recursion.
	 * that function is declared in gnome_canvas.c so no way to call it directly:
	 * Item::update_vfunc(affine, clip_path, flags);
	 * So just copy the code from that function. This has to be a bug or
	 * something I haven't figured out.
	 */
	GTK_OBJECT_UNSET_FLAGS (item, GNOME_CANVAS_ITEM_NEED_UPDATE);
	GTK_OBJECT_UNSET_FLAGS (item, GNOME_CANVAS_ITEM_NEED_AFFINE);
	GTK_OBJECT_UNSET_FLAGS (item, GNOME_CANVAS_ITEM_NEED_CLIP);
	GTK_OBJECT_UNSET_FLAGS (item, GNOME_CANVAS_ITEM_NEED_VIS);

	//cerr << "update {" << endl;
	in_update = true;

	// ahh. We must update bounds no matter what. If the group position changed,
	// there is no way that we are notified of that.

	//if(bounds_changed) {
	lines_need_redraw = update_bounds();
	bounds_changed = false;
		//}

	update_lines(lines_need_redraw);

	in_update = false;
	//cerr << "}" << endl;
}

void
Lineset::draw_vfunc(const Glib::RefPtr<Gdk::Drawable>& drawable, int x, int y, int width, int height) {
	cerr << "please don't use the GnomeCanvasLineset item in a non-aa Canvas" << endl;
	abort();
}

inline void
Lineset::paint_vert(GnomeCanvasBuf* buf, Lineset::Line& line, int x1, int y1, int x2, int y2) {
	if(line.width == 1.0) {
		PAINT_VERTA(buf, line.r, line.g, line.b, line.a, x1, y1, y2);
	}
	else {
		PAINT_BOX(buf, line.r, line.g, line.b, line.a, x1, y1, x2, y2);
	}
}

inline void
Lineset::paint_horiz(GnomeCanvasBuf* buf, Lineset::Line& line, int x1, int y1, int x2, int y2) {
	if(line.width == 1.0) {
		PAINT_HORIZA(buf, line.r, line.g, line.b, line.a, x1, x2, y1);
	}
	else {
		PAINT_BOX(buf, line.r, line.g, line.b, line.a, x1, y1, x2, y2);
	}
}

void
Lineset::render_vfunc(GnomeCanvasBuf* buf) {
	ArtIRect rect;
	int pos0, pos1, offset;

	if (buf->is_bg) {
		gnome_canvas_buf_ensure_buf (buf);
		buf->is_bg = FALSE;
	}

	/* get the rect that we are rendering to */
	art_irect_intersect(&rect, &bbox, &buf->rect);

#if 0
	/* DEBUG render bounding box for this region. should result in the full
	   bounding box when all rendering regions are finished */
	PAINT_BOX(buf, 0xaa, 0xaa, 0xff, 0xbb, rect.x0, rect.y0, rect.x1, rect.y1);
#endif

#if 0
	/* harlequin debugging, shows the rect that is actually drawn, distinct from
	   rects from other render cycles */
	gint r, g, b, a;
	r = random() % 0xff;
	g = random() % 0xff;
	b = random() % 0xff;
	PAINT_BOX(buf, r, g, b, 0x33, rect.x0, rect.y0, rect.x1, rect.y1);
#endif

	if(lines.empty()) {
		return;
	}

	Lines::iterator it = lines.begin();
	Lines::iterator end = --lines.end();

	/**
	 * The first and the last line in this render have to be handled separately from those in between, because those lines
	 * may be cut off at the ends. 
	 */

	if(orientation == Vertical) {
		offset = bbox.x0;

		// skip parts of lines that are to the right of the buffer, and paint the last line visible
		for(; end != lines.end(); --end) {
			pos0 = ((int) floor(end->coord)) + offset;

			if(pos0 < rect.x1) {
				pos1 = min((pos0 + (int) floor(end->width)), rect.x1);
				if(pos0 < rect.x0 && pos1 < rect.x0) {
					return;
				}

				paint_vert(buf, *end, pos0, rect.y0, pos1, rect.y1);
				break;
			}
		}

		if(end == lines.end()) {
			return;
		}

		// skip parts of lines that are to the left of the buffer
		for(; it != end; ++it) {
			pos0 = ((int) floor(it->coord)) + offset;
			pos1 = pos0 + ((int) floor(it->width));
			
			if(pos1 > rect.x0) {
				pos0 = max(pos0, rect.x0);
				paint_vert(buf, *it, pos0, rect.y0, pos1, rect.y1);
				++it;
				break;
			}
		}
		
		// render what's between the first and last lines
		for(; it != end; ++it) {
			pos0 = ((int) floor(it->coord)) + offset;
			pos1 = pos0 + ((int) floor(it->width));

			paint_vert(buf, *it, pos0, rect.y0, pos1, rect.y1);
		}
	}
	else {
		offset = bbox.y0;

		// skip parts of lines that are to the right of the buffer, and paint the last line visible
		for(; end != lines.end(); --end) {
			pos0 = ((int) floor(end->coord)) + offset;

			if(pos0 < rect.y1) {
				pos1 = min((pos0 + (int) floor(end->width)), rect.y1);
				if(pos0 < rect.y0 && pos1 < rect.y0) {
					return;
				}

				paint_horiz(buf, *end, rect.x0, pos0, rect.x1, pos1);
				break;
			}
		}

		if(end == lines.end()) {
			return;
		}

		// skip parts of lines that are to the left of the buffer
		for(; it != end; ++it) {
			pos0 = ((int) floor(it->coord)) + offset;
			pos1 = pos0 + ((int) floor(it->width));
			
			if(pos1 > rect.y0) {
				pos0 = max(pos0, rect.y0);
				paint_horiz(buf, *it, rect.x0, pos0, rect.x1, pos1);
				++it;
				break;
			}
		}
		
		// render what's between the first and last lines
		for(; it != end; ++it) {
			pos0 = ((int) floor(it->coord)) + offset;
			pos1 = pos0 + ((int) floor(it->width));
			paint_horiz(buf, *it, rect.x0, pos0, rect.x1, pos1);
		}
	}
}

void
Lineset::bounds_vfunc(double* _x1, double* _y1, double* _x2, double* _y2) {
	*_x1 = x1;
	*_y1 = y1;
	*_x2 = x2 + 1;
	*_y2 = y2 + 1;
}


double
Lineset::point_vfunc(double x, double y, int cx, int cy, GnomeCanvasItem** actual_item) {
	double x1, y1, x2, y2;
	double dx, dy;

	Lineset::bounds_vfunc(&x1, &y1, &x2, &y2);

	*actual_item = gobj();

	if (x < x1) {
		dx = x1 - x;
	}
	else if(x > x2) {
		dx = x - x2;
	}
	else {
		dx = 0.0;
	}

	if (y < y1) {
		dy = y1 - y;
	}
	else if(y > y2) {
		dy = y - y2;
	}
	else {
		if (dx == 0.0) {
			// point is inside
			return 0.0;
		}
		else {
			dy = 0.0;
		}
	}

	return sqrt (dx * dx + dy * dy);
}

/* If not overrided emit the signal */
void
Lineset::request_lines(double c1, double c2) {
	signal_request_lines(*this, c1, c2);
}

void
Lineset::bounds_need_update() {
	bounds_changed = true;

	if(!in_update) {
		request_update();
	}
}

void
Lineset::region_needs_update(double coord1, double coord2) {
	if(update_region1 > update_region2) {
		update_region1 = coord1;
		update_region2 = coord2;
	}
	else {
		update_region1 = min(update_region1, coord1);
		update_region2 = max(update_region2, coord2);
	}

	if(!in_update) {
		request_update();
	}
}

/*
 * These have been defined to avoid endless recursion with gnomecanvasmm.
 * Don't know why this happens
 */
bool Lineset::on_event(GdkEvent* p1) { 
	return false;
}
void Lineset::realize_vfunc() { }
void Lineset::unrealize_vfunc() { }
void Lineset::map_vfunc() { }
void Lineset::unmap_vfunc() { }

} /* namespace Canvas */
} /* namespace Gnome */
