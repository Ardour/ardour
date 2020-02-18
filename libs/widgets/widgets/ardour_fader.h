/*
 * Copyright (C) 2006 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef _WIDGETS_ARDOUR_FADER_H_
#define _WIDGETS_ARDOUR_FADER_H_

#include <cmath>
#include <stdint.h>

#include <gdkmm.h>
#include <gtkmm/adjustment.h>

#include "gtkmm2ext/cairo_widget.h"
#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API ArdourFader : public CairoWidget
{
public:
	ArdourFader (Gtk::Adjustment& adjustment, int orientation, int span, int girth);
	virtual ~ArdourFader ();
	static void flush_pattern_cache();

	sigc::signal<void> StartGesture;
	sigc::signal<void> StopGesture;
	sigc::signal<void> OnExpose;

	void set_default_value (float);
	void set_text (const std::string&, bool centered = true, bool expose = true);

	enum Tweaks {
		NoShowUnityLine = 0x1,
		NoButtonForward = 0x2,
		NoVerticalScroll = 0x4,
	};

	Tweaks tweaks() const { return _tweaks; }
	void set_tweaks (Tweaks);

protected:
	void on_size_request (GtkRequisition*);
	void on_size_allocate (Gtk::Allocation& alloc);

	void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);
	bool on_grab_broken_event (GdkEventGrabBroken*);
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	bool on_motion_notify_event (GdkEventMotion*);
	bool on_scroll_event (GdkEventScroll* ev);
	bool on_enter_notify_event (GdkEventCrossing* ev);
	bool on_leave_notify_event (GdkEventCrossing* ev);

	void on_state_changed (Gtk::StateType);
	void on_style_changed (const Glib::RefPtr<Gtk::Style>&);

	enum Orientation {
		VERT,
		HORIZ,
	};

private:
	Glib::RefPtr<Pango::Layout> _layout;
	std::string                 _text;
	Tweaks                      _tweaks;
	Gtk::Adjustment&            _adjustment;
	int _text_width;
	int _text_height;

	int _span, _girth;
	int _min_span, _min_girth;
	int _orien;
	cairo_pattern_t* _pattern;
	bool _hovering;
	GdkWindow* _grab_window;
	double _grab_loc;
	double _grab_start;
	bool _dragging;
	float _default_value;
	int _unity_loc;
	bool _centered_text;

	sigc::connection _parent_style_change;
	Widget * _current_parent;
	Gdk::Color get_parent_bg ();

	void create_patterns();
	void adjustment_changed ();
	void set_adjustment_from_event (GdkEventButton *);
	void update_unity_position ();
	int  display_span ();

	struct FaderImage {
		cairo_pattern_t* pattern;
		double fr;
		double fg;
		double fb;
		double br;
		double bg;
		double bb;
		int width;
		int height;

		FaderImage (cairo_pattern_t* p,
				double afr, double afg, double afb,
				double abr, double abg, double abb,
				int w, int h)
			: pattern (p)
				, fr (afr)
				 , fg (afg)
				 , fb (afb)
				 , br (abr)
				 , bg (abg)
				 , bb (abb)
				 , width (w)
				 , height (h)
		{}

		bool matches (double afr, double afg, double afb,
				double abr, double abg, double abb,
				int w, int h) {
			return width == w &&
				height == h &&
				afr == fr &&
				afg == fg &&
				afb == fb &&
				abr == br &&
				abg == bg &&
				abb == bb;
		}
	};

	static std::list<FaderImage*> _patterns;
	static cairo_pattern_t* find_pattern (double afr, double afg, double afb,
			double abr, double abg, double abb,
			int w, int h);

};

} /* namespace */

#endif /* __gtkmm2ext_pixfader_h__ */
