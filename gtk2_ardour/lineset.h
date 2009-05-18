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

#ifndef __gnome_canvas_lineset_h__
#define __gnome_canvas_lineset_h__

#include <stdint.h>
#include <libgnomecanvasmm/item.h>

namespace Gnome {
namespace Canvas {

class LineSetClass : public Glib::Class {
public:
	const Glib::Class& init();
	static void class_init_function(void* g_class, void* class_data);
};

/** A canvas item that displays a set of vertical or horizontal lines,
 * spanning the entire size of the item.
 */
class LineSet : public Item {
public:
	enum Orientation {
		Vertical,
		Horizontal
	};

	LineSet(Group& parent, Orientation);
	virtual ~LineSet();

	Glib::PropertyProxy<double> property_x1() { return x1.get_proxy(); }
	Glib::PropertyProxy<double> property_y1() { return y1.get_proxy(); }
	Glib::PropertyProxy<double> property_x2() { return x2.get_proxy(); }
	Glib::PropertyProxy<double> property_y2() { return y2.get_proxy(); }

	/* Note: every line operation takes a coord parameter, as an index to
	 * the line it modifies. The index will identify a line if it is between
	 * line.coord and line.coord + line.width.
	 */

	/** Move a line to a new position.
	 * For this to work (to move the desired line) it is important that
	 * lines have unique coordinates. This also applies to every line
	 * accessing functions below
	 */
	void move_line(double coord, double dest);

	/** Change the width of a line.
	 * Only allow if the new width doesn't overlap the next line (see below)
	 */
	void change_line_width(double coord, double width);

	/** Change the color of a line.
	 */
	void change_line_color(double coord, uint32_t color);

	/** Add a line to draw.
	 * width is an offset, so that coord + width specifies the end of the line.
	 * lines should not overlap, as no layering information is provided.
	 * however, line_coord[i] + line_width[i] == line_coord[i+1] is
	 * be legal, as the coordinates are real numbers and represents
	 * real world coordinates. Two real world object sharing coordinates for start
	 * and end are not overlapping.
	 */
	void add_line(double coord, double width, uint32_t color);

	/** Remove the line at coord
	 */
	void remove_line(double coord);
	
	/** Remove all lines in a coordinate range
	 */
	void remove_lines(double c1, double c2);

	/** Remove all lines with a coordinate lower than coord
	 */
	void remove_until(double coord);
	
	/** Remove all lines with a coordinate equal to or higher than coord.
	 */
	void remove_from(double coord);

	/** Remove all lines.
	 */
	void clear();

	/** Add a set of lines in the given range.
	 * For every line visible in the provided coordinate range, call add_line().
	 * This is called when the area between c1 and c2 becomes visible, when
	 * previously outside any possible view.
	 * The number of calls to this function should be kept at a minimum.
	 */
	virtual void request_lines(double c1, double c2);

	/** Instead of overriding the update_lines function one can connect to this
	 * and add lines externally instead.
	 * If add_lines() is overrided, this signal will not be emitted.
	 */
	sigc::signal<void, LineSet&, double, double> signal_request_lines;

	/* overridden from Gnome::Canvas::Item */
	void update_vfunc(double* affine, ArtSVP* clip_path, int flags);
	void realize_vfunc();
	void unrealize_vfunc();
	void map_vfunc();
	void unmap_vfunc();
	void draw_vfunc(const Glib::RefPtr<Gdk::Drawable>& drawable, int x, int y, int width, int height);
	void render_vfunc(GnomeCanvasBuf* buf);
	double point_vfunc(double x, double y, int cx, int cy, GnomeCanvasItem** actual_item);
	void bounds_vfunc(double* x1, double* y1, double* x2, double* y2);
	bool on_event(GdkEvent* p1);

	/* debug */
	void print_lines();
	
protected:
	struct Line {
		Line(double c, double w, uint32_t color);
		Line(double c);

		void set_color(uint32_t color);

		double coord;
		double width;
		unsigned char r;
		unsigned char g;
		unsigned char b;
		unsigned char a;
	};

	static inline void paint_vert(GnomeCanvasBuf* buf, LineSet::Line& line, int x1, int y1, int x2, int y2);
	static inline void paint_horiz(GnomeCanvasBuf* buf, LineSet::Line& line, int x1, int y1, int x2, int y2);

	static bool line_compare(const Line& a, const Line& b);

	typedef std::list<Line> Lines;
	void bounds_need_update();
	void region_needs_update(double coord1, double coord2);
	bool update_bounds();
	void update_lines(bool need_redraw);
	void redraw_request(ArtIRect&);
	void redraw_request(ArtDRect&);

	Lines::iterator line_at(double coord);

	/** Stores last accessed line so adjacent lines are found faster */
	Lines::iterator cached_pos;

	static LineSetClass lineset_class;
	Orientation orientation;
	Lines lines;

	/* properties */
	Glib::Property<double> x1;
	Glib::Property<double> y1;
	Glib::Property<double> x2;
	Glib::Property<double> y2;

	/** Cached bounding box in canvas coordinates */
	ArtIRect bbox;

private:
	LineSet();
	LineSet(const LineSet&);

	bool in_update;

	/* a range that needs update update1 > update2 ==> no update needed */
	double update_region1;
	double update_region2;
	bool bounds_changed;

	double covered1;
	double covered2;
};

} /* namespace Canvas */
} /* namespace Gnome */

#endif /* __gnome_canvas_lineset_h__ */
