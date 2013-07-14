/*
    Copyright (C) 2013 Paul Davis
    Author: Robin Gareus

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

#include <gtkmm2ext/cairo_widget.h>
#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/rgb_macros.h>

#include <ardour/rc_configuration.h>
#include "ardour_ui.h"
#include "utils.h"
#include "logmeter.h"
#include "meter_patterns.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;


static const int max_pattern_metric_size = 1026;

sigc::signal<void> ResetAllPeakDisplays;
sigc::signal<void,ARDOUR::Route*> ResetRoutePeakDisplays;
sigc::signal<void,ARDOUR::RouteGroup*> ResetGroupPeakDisplays;
sigc::signal<void> RedrawMetrics;

sigc::signal<void, int, ARDOUR::RouteGroup*, ARDOUR::MeterType> SetMeterTypeMulti;

cairo_pattern_t*
meter_render_ticks (Gtk::Widget& w, vector<ARDOUR::DataType> types)
{
	Glib::RefPtr<Gdk::Window> win (w.get_window());

	bool background;
	gint width, height;
	win->get_size (width, height);
	background = types.size() == 0
		|| w.get_name().substr(w.get_name().length() - 4) == "Left"
		|| w.get_name().substr(w.get_name().length() - 5) == "Right";

	cairo_surface_t* surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, width, height);
	cairo_t* cr = cairo_create (surface);

	cairo_move_to (cr, 0, 0);
	cairo_rectangle (cr, 0, 0, width, height);
	{
		Gdk::Color c = w.get_style()->get_bg (background ? Gtk::STATE_ACTIVE : Gtk::STATE_NORMAL);
		cairo_set_source_rgb (cr, c.get_red_p(), c.get_green_p(), c.get_blue_p());
	}
	cairo_fill (cr);

	height = min(max_pattern_metric_size, height);
	uint32_t peakcolor = ARDOUR_UI::config()->color_by_name ("meterbridge peaklabel");

	for (vector<DataType>::const_iterator i = types.begin(); i != types.end(); ++i) {

		Gdk::Color c;
		c = w.get_style()->get_fg (Gtk::STATE_NORMAL);

		if (types.size() > 1) {
			/* we're overlaying more than 1 set of marks, so use different colours */
			switch (*i) {
			case DataType::AUDIO:
				c = w.get_style()->get_fg (Gtk::STATE_NORMAL);
				cairo_set_source_rgb (cr, c.get_red_p(), c.get_green_p(), c.get_blue_p());
				break;
			case DataType::MIDI:
				c = w.get_style()->get_fg (Gtk::STATE_ACTIVE);
				cairo_set_source_rgb (cr, c.get_red_p(), c.get_green_p(), c.get_blue_p());
				break;
			}
		} else {
			c = w.get_style()->get_fg (Gtk::STATE_NORMAL);
			cairo_set_source_rgb (cr, c.get_red_p(), c.get_green_p(), c.get_blue_p());
		}

		std::map<int,float> points;

		switch (*i) {
		case DataType::AUDIO:
			points.insert (std::pair<int,float>(-60, 0.5));
			points.insert (std::pair<int,float>(-50, 0.5));
			points.insert (std::pair<int,float>(-40, 0.5));
			points.insert (std::pair<int,float>(-30, 0.5));
			if (Config->get_meter_line_up_level() == MeteringLineUp24) {
				points.insert (std::pair<int,float>(-24, 0.5));
			} else {
				points.insert (std::pair<int,float>(-25, 0.5));
			}
			points.insert (std::pair<int,float>(-20, 1.0));

			points.insert (std::pair<int,float>(-19, 0.5));
			points.insert (std::pair<int,float>(-18, 1.0));
			points.insert (std::pair<int,float>(-17, 0.5));
			points.insert (std::pair<int,float>(-16, 0.5));
			points.insert (std::pair<int,float>(-15, 1.0));

			points.insert (std::pair<int,float>(-14, 0.5));
			points.insert (std::pair<int,float>(-13, 0.5));
			points.insert (std::pair<int,float>(-12, 0.5));
			points.insert (std::pair<int,float>(-11, 0.5));
			points.insert (std::pair<int,float>(-10, 1.0));

			points.insert (std::pair<int,float>( -9, 0.5));
			points.insert (std::pair<int,float>( -8, 0.5));
			points.insert (std::pair<int,float>( -7, 0.5));
			points.insert (std::pair<int,float>( -6, 0.5));
			points.insert (std::pair<int,float>( -5, 1.0));
			points.insert (std::pair<int,float>( -4, 0.5));
			points.insert (std::pair<int,float>( -3, 0.5));
			points.insert (std::pair<int,float>( -2, 0.5));
			points.insert (std::pair<int,float>( -1, 0.5));

			points.insert (std::pair<int,float>(  0, 1.0));
			points.insert (std::pair<int,float>(  1, 0.5));
			points.insert (std::pair<int,float>(  2, 0.5));
			points.insert (std::pair<int,float>(  3, 0.5));
			points.insert (std::pair<int,float>(  4, 0.5));
			points.insert (std::pair<int,float>(  5, 0.5));
			break;

		case DataType::MIDI:
			points.insert (std::pair<int,float>(  0, 1.0));
			points.insert (std::pair<int,float>( 16, 0.5));
			points.insert (std::pair<int,float>( 32, 0.5));
			points.insert (std::pair<int,float>( 48, 0.5));
			points.insert (std::pair<int,float>( 64, 1.0));
			points.insert (std::pair<int,float>( 80, 0.5));
			points.insert (std::pair<int,float>( 96, 0.5));
			points.insert (std::pair<int,float>(100, 1.0));
			points.insert (std::pair<int,float>(112, 0.5));
			points.insert (std::pair<int,float>(127, 1.0));
			break;
		}

		for (std::map<int,float>::const_iterator j = points.begin(); j != points.end(); ++j) {
			cairo_set_line_width (cr, (j->second));

			float fraction = 0;
			gint pos;

			switch (*i) {
			case DataType::AUDIO:
				if (j->first >= 0 || j->first == -9) {
					cairo_set_source_rgb (cr,
							UINT_RGBA_R_FLT(peakcolor),
							UINT_RGBA_G_FLT(peakcolor),
							UINT_RGBA_B_FLT(peakcolor));
				} else {
					cairo_set_source_rgb (cr, c.get_red_p(), c.get_green_p(), c.get_blue_p());
				}
				fraction = log_meter (j->first);
				pos = height - (gint) floor (height * fraction);
				cairo_move_to(cr, 0, pos + .5);
				cairo_line_to(cr, 3, pos + .5);
				cairo_stroke (cr);
				break;
			case DataType::MIDI:
				fraction = (j->first) / 127.0;
				pos = 1 + height - (gint) floor (height * fraction);
				pos = min (pos, height);
				cairo_arc(cr, 1.5, pos + .5, 1.0, 0, 2 * M_PI);
				cairo_fill(cr);
				break;
			}
		}
	}

	cairo_pattern_t* pattern = cairo_pattern_create_for_surface (surface);

	cairo_destroy (cr);
	cairo_surface_destroy (surface);

	return pattern;
}


cairo_pattern_t*
meter_render_metrics (Gtk::Widget& w, vector<DataType> types)
{
	Glib::RefPtr<Gdk::Window> win (w.get_window());

	bool tickleft;
	bool background;
	gint width, height;
	win->get_size (width, height);

	tickleft = w.get_name().substr(w.get_name().length() - 4) == "Left";
	background = types.size() == 0 || tickleft || w.get_name().substr(w.get_name().length() - 5) == "Right";

	cairo_surface_t* surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, width, height);
	cairo_t* cr = cairo_create (surface);
	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create(w.get_pango_context());

	Pango::AttrList audio_font_attributes;
	Pango::AttrList midi_font_attributes;
	Pango::AttrList unit_font_attributes;

	Pango::AttrFontDesc* font_attr;
	Pango::FontDescription font;

	font = Pango::FontDescription (""); // use defaults
	//font = get_font_for_style("gain-fader");
	//font = w.get_style()->get_font();

	font.set_weight (Pango::WEIGHT_NORMAL);
	font.set_size (9.0 * PANGO_SCALE);
	font_attr = new Pango::AttrFontDesc (Pango::Attribute::create_attr_font_desc (font));
	audio_font_attributes.change (*font_attr);
	delete font_attr;

	font.set_weight (Pango::WEIGHT_ULTRALIGHT);
	font.set_stretch (Pango::STRETCH_ULTRA_CONDENSED);
	font.set_size (7.5 * PANGO_SCALE);
	font_attr = new Pango::AttrFontDesc (Pango::Attribute::create_attr_font_desc (font));
	midi_font_attributes.change (*font_attr);
	delete font_attr;

	font.set_size (7.0 * PANGO_SCALE);
	font_attr = new Pango::AttrFontDesc (Pango::Attribute::create_attr_font_desc (font));
	unit_font_attributes.change (*font_attr);
	delete font_attr;

	cairo_move_to (cr, 0, 0);
	cairo_rectangle (cr, 0, 0, width, height);
	{
		Gdk::Color c = w.get_style()->get_bg (background ? Gtk::STATE_ACTIVE : Gtk::STATE_NORMAL);
		cairo_set_source_rgb (cr, c.get_red_p(), c.get_green_p(), c.get_blue_p());
	}
	cairo_fill (cr);

	height = min(max_pattern_metric_size, height);
	uint32_t peakcolor = ARDOUR_UI::config()->color_by_name ("meterbridge peaklabel");

	for (vector<DataType>::const_iterator i = types.begin(); i != types.end(); ++i) {

		Gdk::Color c;

		if (types.size() > 1) {
			/* we're overlaying more than 1 set of marks, so use different colours */
			Gdk::Color c;
			switch (*i) {
			case DataType::AUDIO:
				c = w.get_style()->get_fg (Gtk::STATE_NORMAL);
				cairo_set_source_rgb (cr, c.get_red_p(), c.get_green_p(), c.get_blue_p());
				break;
			case DataType::MIDI:
				c = w.get_style()->get_fg (Gtk::STATE_ACTIVE);
				cairo_set_source_rgb (cr, c.get_red_p(), c.get_green_p(), c.get_blue_p());
				break;
			}
		} else {
			c = w.get_style()->get_fg (Gtk::STATE_NORMAL);
			cairo_set_source_rgb (cr, c.get_red_p(), c.get_green_p(), c.get_blue_p());
		}

		std::map<int,float> points;

		switch (*i) {
		case DataType::AUDIO:
			layout->set_attributes (audio_font_attributes);
			points.insert (std::pair<int,float>(-50, 0.5));
			points.insert (std::pair<int,float>(-40, 0.5));
			points.insert (std::pair<int,float>(-30, 0.5));
			points.insert (std::pair<int,float>(-20, 1.0));
			if (types.size() == 1) {
				if (Config->get_meter_line_up_level() == MeteringLineUp24) {
					points.insert (std::pair<int,float>(-24, 0.5));
				} else {
					points.insert (std::pair<int,float>(-25, 0.5));
				}
				points.insert (std::pair<int,float>(-15, 1.0));
			}
			points.insert (std::pair<int,float>(-18, 1.0));
			points.insert (std::pair<int,float>(-10, 1.0));
			points.insert (std::pair<int,float>( -5, 1.0));
			points.insert (std::pair<int,float>( -3, 0.5));
			points.insert (std::pair<int,float>(  0, 1.0));
			points.insert (std::pair<int,float>(  3, 0.5));
			break;

		case DataType::MIDI:
			layout->set_attributes (midi_font_attributes);
			points.insert (std::pair<int,float>(  0, 1.0));
			if (types.size() == 1) {
				points.insert (std::pair<int,float>( 16, 0.5));
				points.insert (std::pair<int,float>( 32, 0.5));
				points.insert (std::pair<int,float>( 48, 0.5));
				points.insert (std::pair<int,float>( 64, 1.0));
				points.insert (std::pair<int,float>( 80, 0.5));
				points.insert (std::pair<int,float>( 96, 0.5));
				points.insert (std::pair<int,float>(100, 0.5));
				points.insert (std::pair<int,float>(112, 0.5));
			} else {
				/* labels that don't overlay with dB */
				points.insert (std::pair<int,float>( 24, 0.5));
				points.insert (std::pair<int,float>( 48, 0.5));
				points.insert (std::pair<int,float>( 72, 0.5));
			}
			points.insert (std::pair<int,float>(127, 1.0));
			break;
		}

		char buf[32];
		gint pos;

		for (std::map<int,float>::const_iterator j = points.begin(); j != points.end(); ++j) {

			float fraction = 0;
			switch (*i) {
			case DataType::AUDIO:
				cairo_set_line_width (cr, (j->second));
				if (j->first >= 0) {
					cairo_set_source_rgb (cr,
							UINT_RGBA_R_FLT(peakcolor),
							UINT_RGBA_G_FLT(peakcolor),
							UINT_RGBA_B_FLT(peakcolor));
				}
				fraction = log_meter (j->first);
				snprintf (buf, sizeof (buf), "%+2d", j->first);
				pos = height - (gint) floor (height * fraction);
				if (tickleft) {
					cairo_move_to(cr, width-2.5, pos + .5);
					cairo_line_to(cr, width, pos + .5);
				} else {
					cairo_move_to(cr, 0, pos + .5);
					cairo_line_to(cr, 2.5, pos + .5);
				}
				cairo_stroke (cr);
				break;
			case DataType::MIDI:
				cairo_set_line_width (cr, 1.0);
				fraction = (j->first) / 127.0;
				snprintf (buf, sizeof (buf), "%3d", j->first);
				pos = 1 + height - (gint) rintf (height * fraction);
				pos = min (pos, height);
				if (tickleft) {
					cairo_arc(cr, width - 2.0, pos + .5, 1.0, 0, 2 * M_PI);
				} else {
					cairo_arc(cr, 3, pos + .5, 1.0, 0, 2 * M_PI);
				}
				cairo_fill(cr);
				break;
			}

			layout->set_text(buf);

			/* we want logical extents, not ink extents here */

			int tw, th;
			layout->get_pixel_size(tw, th);

			int p = pos - (th / 2);
			p = min (p, height - th);
			p = max (p, 0);

			cairo_move_to (cr, width-4-tw, p);
			pango_cairo_show_layout (cr, layout->gobj());
		}
	}

	if (types.size() == 1) {
		int tw, th;
		layout->set_attributes (unit_font_attributes);
		switch (types.at(0)) {
			case DataType::AUDIO:
				layout->set_text("dBFS");
				layout->get_pixel_size(tw, th);
				break;
			case DataType::MIDI:
				layout->set_text("vel");
				layout->get_pixel_size(tw, th);
				break;
		}
		Gdk::Color c = w.get_style()->get_fg (Gtk::STATE_ACTIVE);
		cairo_set_source_rgb (cr, c.get_red_p(), c.get_green_p(), c.get_blue_p());
		cairo_move_to (cr, 2, height - th - 1.5);
		pango_cairo_show_layout (cr, layout->gobj());
	}

	cairo_pattern_t* pattern = cairo_pattern_create_for_surface (surface);

	cairo_destroy (cr);
	cairo_surface_destroy (surface);

	return pattern;
}


typedef std::map<std::string,cairo_pattern_t*> TickPatterns;
static  TickPatterns ticks_patterns;

gint meter_expose_ticks (GdkEventExpose *ev, std::vector<ARDOUR::DataType> types, Gtk::DrawingArea *mta)
{
	Glib::RefPtr<Gdk::Window> win (mta->get_window());
	cairo_t* cr;

	cr = gdk_cairo_create (win->gobj());

	/* clip to expose area */

	gdk_cairo_rectangle (cr, &ev->area);
	cairo_clip (cr);

	cairo_pattern_t* pattern;
	TickPatterns::iterator i = ticks_patterns.find (mta->get_name());

	if (i == ticks_patterns.end()) {
		pattern = meter_render_ticks (*mta, types);
		ticks_patterns[mta->get_name()] = pattern;
	} else {
		pattern = i->second;
	}

	cairo_move_to (cr, 0, 0);
	cairo_set_source (cr, pattern);

	gint width, height;
	win->get_size (width, height);

	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	cairo_destroy (cr);

	return true;
}

typedef std::map<std::string,cairo_pattern_t*> MetricPatterns;
static  MetricPatterns metric_patterns;

gint meter_expose_metrics (GdkEventExpose *ev, std::vector<ARDOUR::DataType> types, Gtk::DrawingArea *mma)
{
	Glib::RefPtr<Gdk::Window> win (mma->get_window());
	cairo_t* cr;

	cr = gdk_cairo_create (win->gobj());

	/* clip to expose area */

	gdk_cairo_rectangle (cr, &ev->area);
	cairo_clip (cr);

	cairo_pattern_t* pattern;
	MetricPatterns::iterator i = metric_patterns.find (mma->get_name());

	if (i == metric_patterns.end()) {
		pattern = meter_render_metrics (*mma, types);
		metric_patterns[mma->get_name()] = pattern;
	} else {
		pattern = i->second;
	}

	cairo_move_to (cr, 0, 0);
	cairo_set_source (cr, pattern);

	gint width, height;
	win->get_size (width, height);

	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	cairo_destroy (cr);

	return true;
}

void meter_clear_pattern_cache(int which) {
	MetricPatterns::iterator i = metric_patterns.begin();
	TickPatterns::iterator j = ticks_patterns.begin();

	while (i != metric_patterns.end()) {
		int m = 4;
		std::string n = i->first;
		if (n.substr(n.length() - 4) == "Left")  { m = 1; }
		if (n.substr(n.length() - 5) == "Right") { m = 2; }
		if (which & m) {
			cairo_pattern_destroy(i->second);
			metric_patterns.erase(i++);
		} else {
			++i;
		}
	}

	while (j != ticks_patterns.end()) {
		int m = 4;
		std::string n = j->first;
		if (n.substr(n.length() - 4) == "Left")  { m = 1; }
		if (n.substr(n.length() - 5) == "Right") { m = 2; }
		if (which & m) {
			cairo_pattern_destroy(j->second);
			ticks_patterns.erase(j++);
		} else {
			++j;
		}
	}
	RedrawMetrics();
}
