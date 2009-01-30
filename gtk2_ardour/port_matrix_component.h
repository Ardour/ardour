/*
    Copyright (C) 2002-2009 Paul Davis 

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

#ifndef __gtk_ardour_port_matrix_component_h__
#define __gtk_ardour_port_matrix_component_h__

#include <gtkmm/eventbox.h>

class PortMatrix;
class PortMatrixBody;
class PortMatrixNode;

/** One component of the PortMatrix.  This is a cairo-rendered
 *  Pixmap.
 */
class PortMatrixComponent
{
public:
	PortMatrixComponent (PortMatrix *, PortMatrixBody *);
	virtual ~PortMatrixComponent ();

	virtual double component_to_parent_x (double x) const = 0;
	virtual double parent_to_component_x (double x) const = 0;
	virtual double component_to_parent_y (double y) const = 0;
	virtual double parent_to_component_y (double y) const = 0;
	virtual void mouseover_changed (PortMatrixNode const &) = 0;
	virtual void draw_extra (cairo_t *) = 0;

	void setup ();
	GdkPixmap* get_pixmap (GdkDrawable *);
	std::pair<uint32_t, uint32_t> dimensions ();

	void require_render () {
		_render_required = true;
	}
	
	void require_rebuild () {
		_dimension_computation_required = true;
		_render_required = true;
	}

	void set_parent_rectangle (Gdk::Rectangle const & r) {
		_parent_rectangle = r;
	}

	Gdk::Rectangle parent_rectangle () const {
		return _parent_rectangle;
	}

	/** @return width of columns in the grid */
	static uint32_t column_width () {
		return 32;
	}

	/** @return height of rows in the grid */
	static uint32_t row_height () {
		return 32;
	}

protected:

	/** @return width of borders drawn around labels */
	static uint32_t label_border_width () {
		return 1;
	}

	/** @return padding between a name and the nearest line */
	static uint32_t name_pad () {
		return 8;
	}

	/** @return width of thin lines in the grid */
	static uint32_t thin_grid_line_width () {
		return 1;
	}

	/** @return width of thick lines in the grid */
	static uint32_t thick_grid_line_width () {
		return 2;
	}

	/** @return space around the connection indicator */
	static uint32_t connection_indicator_pad () {
		return 8;
	}

	static uint32_t mouseover_line_width () {
		return 4;
	}

	/** @return angle of column labels, in radians */
	static double angle () {
		return M_PI / 4;
	}

	/* XXX I guess these colours should come from a theme, or something */

	/** @return background colour */
	static Gdk::Color background_colour () {
 		return Gdk::Color ("#000000");
	}

	/** @return text colour */
	static Gdk::Color text_colour () {
		return Gdk::Color ("#ffffff");
	}

	/** @return grid line colour */
	static Gdk::Color grid_colour () {
		return Gdk::Color ("#333333");
	}

	/** @return colour of association blobs */
	static Gdk::Color association_colour () {
		return Gdk::Color ("#00ff00");
	}

	/** @return colour to paint grid squares when they can't be associated */
	static Gdk::Color unknown_colour () {
		return Gdk::Color ("#cccccc");
	}

	/** @return colour to paint mouseover lines */
	static Gdk::Color mouseover_line_colour () {
		return Gdk::Color ("#ff0000");
	}

	/** @return colour to paint mouseover lines */
	static Gdk::Color mouseover_port_colour () {
		return Gdk::Color ("#777777");
	}

	/* XXX */
	static Gdk::Color get_a_bundle_colour (int x) {
		if ((x % 2) == 0) {
			return Gdk::Color ("#547027");
		} else {
			return Gdk::Color ("#3552a6");
		}
	}

	/* XXX */
	static Gdk::Color get_a_group_colour (int x) {
		if ((x % 2) == 0) {
			return Gdk::Color ("#222222");
		} else {
			return Gdk::Color ("#444444");
		}
	}
	
	void set_source_rgb (cairo_t *, Gdk::Color const &);
	void set_source_rgba (cairo_t *, Gdk::Color const &, double);
	std::pair<std::string, double> display_port_name (cairo_t*, std::string const &, double) const;

	/** Render the complete component to a cairo context. */
	virtual void render (cairo_t *) = 0;
	/** Compute any required dimensions.  This must set up
	 *  _width and _height.
	 */
	virtual void compute_dimensions () = 0;

	PortMatrix* _matrix;
	PortMatrixBody* _body; ///< the PortMatrixBody that we're in
	uint32_t _width; ///< full width of the contents
	uint32_t _height; ///< full height of the contents
	Gdk::Rectangle _parent_rectangle;

private:	
	GdkPixmap* _pixmap; ///< pixmap
	bool _render_required; ///< true if the rendered pixmap is out of date
	bool _dimension_computation_required; ///< true if the dimensions are out of date
};

#endif
