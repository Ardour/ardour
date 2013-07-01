/*
    Copyright (C) 2003 Paul Davis

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

#ifndef __gtkmm2ext_fastmeter_h__
#define __gtkmm2ext_fastmeter_h__

#include <map>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <cairomm/pattern.h>
#include <gtkmm/drawingarea.h>
#include <gdkmm/pixbuf.h>

namespace Gtkmm2ext {

class FastMeter : public Gtk::DrawingArea {
  public:
	enum Orientation {
		Horizontal,
		Vertical
	};

	FastMeter (long hold_cnt, unsigned long width, Orientation, int len=0, int clrb0=0x00ff00, int clr1=0xffff00, int clr2=0xffaa00, int clr3=0xff0000);
	virtual ~FastMeter ();

	void set (float level);
	void clear ();

	float get_level() { return current_level; }
	float get_user_level() { return current_user_level; }
	float get_peak() { return current_peak; }

	long hold_count() { return hold_cnt; }
	void set_hold_count (long);

protected:
	bool on_expose_event (GdkEventExpose*);
	void on_size_request (GtkRequisition*);
	void on_size_allocate (Gtk::Allocation&);

private:

	Cairo::RefPtr<Cairo::Pattern> fgpattern;
	Cairo::RefPtr<Cairo::Pattern> bgpattern;
	gint pixheight;
	gint pixwidth;
	int _clr0, _clr1, _clr2, _clr3;
	int _bgc0, _bgc1, _bgc2, _bgc3;

	Orientation orientation;
	GdkRectangle pixrect;
	GdkRectangle last_peak_rect;
	gint request_width;
	gint request_height;
	unsigned long hold_cnt;
	unsigned long hold_state;
	float current_level;
	float current_peak;
	float current_user_level;
	bool resized;

	bool vertical_expose (GdkEventExpose*);
	bool horizontal_expose (GdkEventExpose*);
	void queue_vertical_redraw (const Glib::RefPtr<Gdk::Window>&, float);
	void queue_horizontal_redraw (const Glib::RefPtr<Gdk::Window>&, float);

	static Cairo::RefPtr<Cairo::Pattern> generate_meter_pattern (
		int w, int h, int clr0, int clr1, int clr2, int clr3);
	static Cairo::RefPtr<Cairo::Pattern> request_vertical_meter (
		int w, int h, int clr0, int clr1, int clr2, int clr3);
	static Cairo::RefPtr<Cairo::Pattern> request_horizontal_meter (
		int w, int h, int clr0, int clr1, int clr2, int clr3);

	struct PatternMapKey {
		PatternMapKey (int w, int h, int c0, int c1, int c2, int c3)
			: dim(w, h)
			, cols(c0, c1, c2, c3)
		{}
		inline bool operator<(const PatternMapKey& rhs) const {
			return (dim < rhs.dim) || (dim == rhs.dim && cols < rhs.cols);
		}
		boost::tuple<int, int>           dim;  // width, height
		boost::tuple<int, int, int, int> cols; // c0, c1, c2, c3
	};
	typedef std::map<PatternMapKey, Cairo::RefPtr<Cairo::Pattern> > PatternMap;

	static PatternMap v_pattern_cache;
	static PatternMap h_pattern_cache;
	static int min_pattern_metric_size; // min dimension for axis that displays the meter level
	static int max_pattern_metric_size; // max dimension for axis that displays the meter level
};


} /* namespace */

 #endif /* __gtkmm2ext_fastmeter_h__ */
