/*
  Copyright (C) 2011 Paul Davis

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

#ifndef __libgtmm2ext_cairocell_h__
#define __libgtmm2ext_cairocell_h__

#include <map>

#include <stdint.h>
#include <cairomm/cairomm.h>
#include <gtkmm.h>

class CairoCell
{
  public:
	CairoCell(int32_t id);
	virtual ~CairoCell() {}
	
	int32_t id() const { return _id; }

	virtual void render (Cairo::RefPtr<Cairo::Context>&) = 0;

	double x() const { return bbox.x; }
	double y() const { return bbox.y; }
	double width() const { return bbox.width; }
	double height() const { return bbox.height; }

	void set_position (double x, double y) {
		bbox.x = x;
		bbox.y = y;
	}

	bool intersects (GdkRectangle& r) const {
		return gdk_rectangle_intersect (&r, &bbox, 0);
	}

	bool covers (double x, double y) const {
		return bbox.x <= x && bbox.x + bbox.width > x &&
			bbox.y <= y && bbox.y + bbox.height > y;
	}

	double xpad() const { return _xpad; }
	void   set_xpad (double x) { _xpad = x; }

	void set_visible (bool yn) { _visible = yn; }
	bool visible() const { return _visible; }
	virtual void set_size (Glib::RefPtr<Pango::Context>&,
	                       const Pango::FontDescription&) {}

  protected:
	int32_t _id;
	GdkRectangle bbox;
	bool _visible;
	uint32_t _xpad;
};

class CairoBarCell : public CairoCell
{
  public:
        CairoBarCell(int32_t id) : CairoCell (id) {};

	void render (Cairo::RefPtr<Cairo::Context>& context) {
		if (bbox.height > 4) {
			context->move_to (bbox.x, bbox.y + 2);
			context->set_line_width (bbox.width);
			context->rel_line_to (0, bbox.height - 2);
			context->stroke ();
		}
	}

	void set_size (Glib::RefPtr<Pango::Context>& context,
	               const Pango::FontDescription& font) {
		Pango::FontMetrics metrics = context->get_metrics (font);
		bbox.width = std::max (1.5, (0.1 * metrics.get_approximate_digit_width() / PANGO_SCALE));
		bbox.height = (metrics.get_ascent() + metrics.get_descent()) / PANGO_SCALE;
	}

  private:
};

class CairoColonCell : public CairoCell
{
  public:
	CairoColonCell (int32_t id) : CairoCell (id) {};

	void render (Cairo::RefPtr<Cairo::Context>& context);
	void set_size (Glib::RefPtr<Pango::Context>& context,
	               const Pango::FontDescription& font);
};

class CairoTextCell : public CairoCell
{
  public:
	CairoTextCell (int32_t id, double  width_chars);
	void set_size (Glib::RefPtr<Pango::Context>&, const Pango::FontDescription&);

	std::string get_text() const {
		return layout->get_text ();
	}
	double width_chars() const { return _width_chars; }

	void render (Cairo::RefPtr<Cairo::Context>&);

  protected:
	friend class CairoEditableText;
	void set_width_chars (double wc) { _width_chars = wc; }
	void set_text (const std::string& txt);

  private:
	double _width_chars;
	Glib::RefPtr<Pango::Layout> layout;
};

class CairoEditableText : public Gtk::Misc
{
public:
	CairoEditableText ();
	~CairoEditableText ();

	void add_cell (CairoCell*);
	void clear_cells ();

	void start_editing (CairoCell*);
	void stop_editing ();

	void set_text (CairoTextCell* cell, const std::string&);
	void set_width_chars (CairoTextCell* cell, uint32_t);

	void set_draw_background (bool yn) { _draw_bg = yn; }
	
	void set_colors (double cr, double cg, double cb, double ca) {
		r = cr;
		g = cg;
		b = cb;
		a = ca;
		queue_draw ();
	}

	void set_edit_colors (double cr, double cg, double cb, double ca) {
		edit_r = cr;
		edit_g = cg;
		edit_b = cb;
		edit_a = ca;
		queue_draw ();
	}

	void set_bg (double r, double g, double b, double a) {
		bg_r = r;
		bg_g = g;
		bg_b = b;
		bg_a = a;
		queue_draw ();
	}

	void set_font (const std::string& str);
	void set_font (const Pango::FontDescription&);

	double xpad() const { return _xpad; }
	void set_xpad (double x) { _xpad = x; queue_resize(); }
	double ypad() const { return _ypad; }
	void set_ypad (double y) { _ypad = y; queue_resize(); }
	
	double corner_radius() const { return _corner_radius; }
	void set_corner_radius (double r) { _corner_radius = r; queue_draw (); }
	
	sigc::signal<bool,GdkEventScroll*,CairoCell*> scroll;
	sigc::signal<bool,GdkEventButton*,CairoCell*> button_press;
	sigc::signal<bool,GdkEventButton*,CairoCell*> button_release;

protected:
	bool on_expose_event (GdkEventExpose*);
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	void on_size_request (GtkRequisition*);
	void on_size_allocate (Gtk::Allocation&);
	bool on_focus_in_event (GdkEventFocus*);
	bool on_focus_out_event (GdkEventFocus*);
	bool on_scroll_event (GdkEventScroll*);

private:
	typedef std::vector<CairoCell*> CellMap;

	CellMap cells;
	Pango::FontDescription _font;
	CairoCell* editing_cell;
	bool _draw_bg;
	double width;
	double max_cell_height;
	double height;
	double _corner_radius;
	double _xpad;
	double _ypad;
	double r;
	double g;
	double b;
	double a;
	double edit_r;
	double edit_g;
	double edit_b;
	double edit_a;
	double bg_r;
	double bg_g;
	double bg_b;
	double bg_a;

	CairoCell* find_cell (uint32_t x, uint32_t y);
	void queue_draw_cell (CairoCell* target);
};

#endif /* __libgtmm2ext_cairocell_h__ */
