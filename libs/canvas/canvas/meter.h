/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __push2_meter_h__
#define __push2_meter_h__

#include <map>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

#include <cairomm/pattern.h>
#include <cairomm/region.h>

#include "canvas/item.h"

namespace ArdourCanvas {

class Canvas;

class Meter : public Item {
  public:
	enum Orientation {
		Horizontal,
		Vertical
	};

	Meter (Item* parent,
	       long hold_cnt, unsigned long width, Orientation, int len=0,
	       int clr0=0x008800ff, int clr1=0x008800ff,
	       int clr2=0x00ff00ff, int clr3=0x00ff00ff,
	       int clr4=0xffaa00ff, int clr5=0xffaa00ff,
	       int clr6=0xffff00ff, int clr7=0xffff00ff,
	       int clr8=0xff0000ff, int clr9=0xff0000ff,
	       int bgc0=0x333333ff, int bgc1=0x444444ff,
	       int bgh0=0x991122ff, int bgh1=0x551111ff,
	       float stp0 = 55.0, // log_meter(-18);
	       float stp1 = 77.5, // log_meter(-9);
	       float stp2 = 92.5, // log_meter(-3); // 95.0, // log_meter(-2);
	       float stp3 = 100.0,
	       int styleflags = 3
		);

	Meter (ArdourCanvas::Canvas* canvas,
	       long hold_cnt, unsigned long width, Orientation, int len=0,
	       int clr0=0x008800ff, int clr1=0x008800ff,
	       int clr2=0x00ff00ff, int clr3=0x00ff00ff,
	       int clr4=0xffaa00ff, int clr5=0xffaa00ff,
	       int clr6=0xffff00ff, int clr7=0xffff00ff,
	       int clr8=0xff0000ff, int clr9=0xff0000ff,
	       int bgc0=0x333333ff, int bgc1=0x444444ff,
	       int bgh0=0x991122ff, int bgh1=0x551111ff,
	       float stp0 = 55.0, // log_meter(-18);
	       float stp1 = 77.5, // log_meter(-9);
	       float stp2 = 92.5, // log_meter(-3); // 95.0, // log_meter(-2);
	       float stp3 = 100.0,
	       int styleflags = 3
		);
	virtual ~Meter ();
	static void flush_pattern_cache();

	void set (float level, float peak = -1);
	void clear ();

	float get_level() { return current_level; }
	float get_user_level() { return current_user_level; }
	float get_peak() { return current_peak; }

	long hold_count() { return hold_cnt; }
	void set_hold_count (long);
	void set_highlight (bool);
	bool get_highlight () { return highlight; }

	void render (ArdourCanvas::Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box() const;

  private:

	Cairo::RefPtr<Cairo::Pattern> fgpattern;
	Cairo::RefPtr<Cairo::Pattern> bgpattern;
	gint pixheight;
	gint pixwidth;

	float _stp[4];
	int _clr[10];
	int _bgc[2];
	int _bgh[2];
	int _styleflags;

	Orientation orientation;
	mutable Cairo::RectangleInt pixrect;
	mutable Cairo::RectangleInt last_peak_rect;
	unsigned long hold_cnt;
	unsigned long hold_state;
	bool bright_hold;
	float current_level;
	float current_peak;
	float current_user_level;
	bool highlight;

	void init (int clr0, int clr1, int clr2, int clr3,
	           int clr4, int clr5, int clr6, int clr7,
	           int clr8, int clr9,
	           int bgc0, int bgc1,
	           int bgh0, int bgh1,
	           float stp0, float stp1,
	           float stp2, float stp3,
	           int dimen, int len);

	void vertical_expose (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> context) const;
	void queue_vertical_redraw (float old_level);

	void horizontal_expose (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> context) const;
	void queue_horizontal_redraw (float old_level);

	static bool no_rgba_overlay;

	static Cairo::RefPtr<Cairo::Pattern> generate_meter_pattern (int, int, int *, float *, int, bool);
	static Cairo::RefPtr<Cairo::Pattern> vertical_meter_pattern (int, int, int *, float *, int);
	static Cairo::RefPtr<Cairo::Pattern> horizontal_meter_pattern (int, int, int *, float *, int);

	static Cairo::RefPtr<Cairo::Pattern> generate_meter_background (int, int, int *, bool, bool);
	static Cairo::RefPtr<Cairo::Pattern> vertical_background (int, int, int *, bool);
	static Cairo::RefPtr<Cairo::Pattern> horizontal_background (int, int, int *, bool);

	struct Pattern10MapKey {
		Pattern10MapKey (
				int w, int h,
				float stp0, float stp1, float stp2, float stp3,
				int c0, int c1, int c2, int c3,
				int c4, int c5, int c6, int c7,
				int c8, int c9, int st
				)
			: dim(w, h)
			, stp(stp0, stp1, stp2, stp3)
			, cols(c0, c1, c2, c3, c4, c5, c6, c7, c8, c9)
			, style(st)
		{}
		inline bool operator<(const Pattern10MapKey& rhs) const {
			return (dim < rhs.dim)
				|| (dim == rhs.dim && stp < rhs.stp)
				|| (dim == rhs.dim && stp == rhs.stp && cols < rhs.cols)
				|| (dim == rhs.dim && stp == rhs.stp && cols == rhs.cols && style < rhs.style);
		}
		boost::tuple<int, int> dim;
		boost::tuple<float, float, float, float> stp;
		boost::tuple<int, int, int, int, int, int, int, int, int, int> cols;
		int style;
	};
	typedef std::map<Pattern10MapKey, Cairo::RefPtr<Cairo::Pattern> > Pattern10Map;

	struct PatternBgMapKey {
		PatternBgMapKey (int w, int h, int c0, int c1, bool shade)
			: dim(w, h)
			, cols(c0, c1)
			, sh(shade)
		{}
		inline bool operator<(const PatternBgMapKey& rhs) const {
			return (dim < rhs.dim) || (dim == rhs.dim && cols < rhs.cols) || (dim == rhs.dim && cols == rhs.cols && (sh && !rhs.sh));
		}
		boost::tuple<int, int> dim;
		boost::tuple<int, int> cols;
		bool sh;
	};
	typedef std::map<PatternBgMapKey, Cairo::RefPtr<Cairo::Pattern> > PatternBgMap;

	static Pattern10Map vm_pattern_cache;
	static PatternBgMap vb_pattern_cache;
	static Pattern10Map hm_pattern_cache;
	static PatternBgMap hb_pattern_cache;
	static int min_pattern_metric_size; // min dimension for axis that displays the meter level
	static int max_pattern_metric_size; // max dimension for axis that displays the meter level
};

} /* namespace */

#endif /* __push2_meter_h__ */
