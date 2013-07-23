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
using namespace ArdourMeter;

static const int max_pattern_metric_size = 1026;

/* signals used by meters */

sigc::signal<void> ArdourMeter::ResetAllPeakDisplays;
sigc::signal<void,ARDOUR::Route*> ArdourMeter::ResetRoutePeakDisplays;
sigc::signal<void,ARDOUR::RouteGroup*> ArdourMeter::ResetGroupPeakDisplays;
sigc::signal<void> ArdourMeter::RedrawMetrics;

sigc::signal<void, int, ARDOUR::RouteGroup*, ARDOUR::MeterType> ArdourMeter::SetMeterTypeMulti;


/* pattern cache */

struct MeterMatricsMapKey {
	MeterMatricsMapKey (std::string n, MeterType t)
		: _n(n)
		, _t(t)
	{}
	inline bool operator<(const MeterMatricsMapKey& rhs) const {
		return (_n < rhs._n) || (_n == rhs._n && _t < rhs._t);
	}
	std::string _n;
	MeterType _t;
};

namespace ArdourMeter {
	typedef std::map<MeterMatricsMapKey, cairo_pattern_t*> MetricPatternMap;
}

static ArdourMeter::MetricPatternMap ticks_patterns;
static ArdourMeter::MetricPatternMap metric_patterns;


const std::string
ArdourMeter::meter_type_string (ARDOUR::MeterType mt)
{
	switch (mt) {
		case MeterPeak:
			return _("Peak");
			break;
		case MeterKrms:
			return _("RMS + Peak");
			break;
		case MeterIEC1DIN:
			return _("IEC1/DIN");
			break;
		case MeterIEC1NOR:
			return _("IEC1/Nordic");
			break;
		case MeterIEC2BBC:
			return _("IEC2/BBC");
			break;
		case MeterIEC2EBU:
			return _("IEC2/EBU");
			break;
		case MeterK20:
			return _("K20");
			break;
		case MeterK14:
			return _("K14");
			break;
		case MeterVU:
			return _("VU");
			break;
		default:
			return _("???");
			break;
	}
}

static inline float mtr_col_and_fract(
		cairo_t* cr, Gdk::Color const * const c, const uint32_t peakcolor, const MeterType mt, const float val)
{
	float fraction = 0;

	switch (mt) {
		default:
		case MeterKrms:
		case MeterPeak:
			fraction = log_meter (val);
			if (val >= 0 || val == -9) {
				cairo_set_source_rgb (cr,
						UINT_RGBA_R_FLT(peakcolor),
						UINT_RGBA_G_FLT(peakcolor),
						UINT_RGBA_B_FLT(peakcolor));
			} else {
				cairo_set_source_rgb (cr, c->get_red_p(), c->get_green_p(), c->get_blue_p());
			}
			break;
		case MeterIEC2BBC:
		case MeterIEC2EBU:
			fraction = meter_deflect_ppm(val);
			if (val >= -10.0) {
				cairo_set_source_rgb (cr,
						UINT_RGBA_R_FLT(peakcolor),
						UINT_RGBA_G_FLT(peakcolor),
						UINT_RGBA_B_FLT(peakcolor));
			} else {
				cairo_set_source_rgb (cr, c->get_red_p(), c->get_green_p(), c->get_blue_p());
			}
			break;
		case MeterIEC1NOR:
			fraction = meter_deflect_nordic(val);
			if (val >= -12.0) {
				cairo_set_source_rgb (cr,
						UINT_RGBA_R_FLT(peakcolor),
						UINT_RGBA_G_FLT(peakcolor),
						UINT_RGBA_B_FLT(peakcolor));
			} else {
				cairo_set_source_rgb (cr, c->get_red_p(), c->get_green_p(), c->get_blue_p());
			}
			break;
		case MeterIEC1DIN:
			fraction = meter_deflect_din(val);
			if (val >= -9.0) {
				cairo_set_source_rgb (cr,
						UINT_RGBA_R_FLT(peakcolor),
						UINT_RGBA_G_FLT(peakcolor),
						UINT_RGBA_B_FLT(peakcolor));
			} else {
				cairo_set_source_rgb (cr, c->get_red_p(), c->get_green_p(), c->get_blue_p());
			}
			break;
		case MeterVU:
			fraction = meter_deflect_vu(val);
			if (val >= -20.0) {
				cairo_set_source_rgb (cr,
						UINT_RGBA_R_FLT(peakcolor),
						UINT_RGBA_G_FLT(peakcolor),
						UINT_RGBA_B_FLT(peakcolor));
			} else {
				cairo_set_source_rgb (cr, c->get_red_p(), c->get_green_p(), c->get_blue_p());
			}
			break;
		case MeterK20:
			fraction = meter_deflect_k (val, 20);
			if (val >= -16.0) {
				cairo_set_source_rgb (cr, 1.0, 0.0, 0.0); // red
			} else if (val >= -20.0) {
				cairo_set_source_rgb (cr, 0.8, 0.8, 0.0); // yellow
			} else {
				cairo_set_source_rgb (cr, 0.0, 1.0, 0.0); // green
			}
			break;
		case MeterK14:
			if (val >= -10.0) {
				cairo_set_source_rgb (cr, 1.0, 0.0, 0.0); // red
			} else if (val >= -14.0) {
				cairo_set_source_rgb (cr, 0.8, 0.8, 0.0); // yellow
			} else {
				cairo_set_source_rgb (cr, 0.0, 1.0, 0.0); // green
			}
			fraction = meter_deflect_k (val, 14);
			break;
	}
	return fraction;
}


static cairo_pattern_t*
meter_render_ticks (Gtk::Widget& w, MeterType type, vector<ARDOUR::DataType> types)
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

		// tick-maker position in dBFS, line-thickness
		std::map<float,float> points;

		switch (*i) {
		case DataType::AUDIO:

			switch (type) {
				case MeterK14:
					points.insert (std::pair<float,float>(-54.0f, 1.0));
					points.insert (std::pair<float,float>(-44.0f, 1.0));
					points.insert (std::pair<float,float>(-34.0f, 1.0));
					points.insert (std::pair<float,float>(-24.0f, 1.0));
					points.insert (std::pair<float,float>(-20.0f, 1.0));
					points.insert (std::pair<float,float>(-17.0f, 1.0));
					points.insert (std::pair<float,float>(-14.0f, 1.0));
					points.insert (std::pair<float,float>(-11.0f, 1.0));
					points.insert (std::pair<float,float>( -8.0f, 1.0));
					points.insert (std::pair<float,float>( -4.0f, 1.0));
					points.insert (std::pair<float,float>(  0.0f, 1.0));
					break;
				case MeterK20:
					points.insert (std::pair<float,float>(-60.0f, 1.0));
					points.insert (std::pair<float,float>(-50.0f, 1.0));
					points.insert (std::pair<float,float>(-40.0f, 1.0));
					points.insert (std::pair<float,float>(-30.0f, 1.0));
					points.insert (std::pair<float,float>(-26.0f, 1.0));
					points.insert (std::pair<float,float>(-23.0f, 1.0));
					points.insert (std::pair<float,float>(-20.0f, 1.0));
					points.insert (std::pair<float,float>(-17.0f, 1.0));
					points.insert (std::pair<float,float>(-14.0f, 1.0));
					points.insert (std::pair<float,float>(-10.0f, 1.0));
					points.insert (std::pair<float,float>( -5.0f, 1.0));
					points.insert (std::pair<float,float>(  0.0f, 1.0));
					break;
				case MeterIEC2EBU:
					points.insert (std::pair<float,float>(-30.0f, 1.0));
					points.insert (std::pair<float,float>(-28.0f, 0.5));
					points.insert (std::pair<float,float>(-26.0f, 1.0));
					points.insert (std::pair<float,float>(-24.0f, 0.5));
					points.insert (std::pair<float,float>(-22.0f, 1.0));
					points.insert (std::pair<float,float>(-20.0f, 0.5));
					points.insert (std::pair<float,float>(-18.0f, 1.0));
					points.insert (std::pair<float,float>(-16.0f, 0.5));
					points.insert (std::pair<float,float>(-14.0f, 1.0));
					points.insert (std::pair<float,float>(-12.0f, 0.5));
					points.insert (std::pair<float,float>(-10.0f, 1.0));
					points.insert (std::pair<float,float>( -9.0f, 0.5));
					points.insert (std::pair<float,float>( -8.0f, 0.5));
					points.insert (std::pair<float,float>( -6.0f, 1.0));
					break;
				case MeterIEC2BBC:
					points.insert (std::pair<float,float>(-30.0f, 1.0));
					points.insert (std::pair<float,float>(-26.0f, 1.0));
					points.insert (std::pair<float,float>(-22.0f, 1.0));
					points.insert (std::pair<float,float>(-18.0f, 1.0));
					points.insert (std::pair<float,float>(-14.0f, 1.0));
					points.insert (std::pair<float,float>(-10.0f, 1.0));
					points.insert (std::pair<float,float>( -6.0f, 1.0));
					break;
				case MeterIEC1NOR:
					points.insert (std::pair<float,float>(-60.0f, 1.0)); // -42
					points.insert (std::pair<float,float>(-57.0f, 0.5));
					points.insert (std::pair<float,float>(-54.0f, 1.0));
					points.insert (std::pair<float,float>(-51.0f, 0.5));
					points.insert (std::pair<float,float>(-48.0f, 1.0));
					points.insert (std::pair<float,float>(-45.0f, 0.5));
					points.insert (std::pair<float,float>(-42.0f, 1.0));
					points.insert (std::pair<float,float>(-39.0f, 0.5));
					points.insert (std::pair<float,float>(-36.0f, 1.0));

					points.insert (std::pair<float,float>(-33.0f, 1.0));
					points.insert (std::pair<float,float>(-30.0f, 1.0));
					points.insert (std::pair<float,float>(-27.0f, 1.0));
					points.insert (std::pair<float,float>(-24.0f, 1.0));
					points.insert (std::pair<float,float>(-21.0f, 1.0));

					points.insert (std::pair<float,float>(-18.0f, 1.0));
					points.insert (std::pair<float,float>(-15.0f, 1.0));
					points.insert (std::pair<float,float>(-12.0f, 1.0));
					points.insert (std::pair<float,float>( -9.0f, 1.0));
					points.insert (std::pair<float,float>( -6.0f, 1.0));
					break;
				case MeterIEC1DIN:
					points.insert (std::pair<float,float>( -3.0f, 0.5)); // "200%"
					points.insert (std::pair<float,float>( -4.0f, 1.0)); // "100%"
					points.insert (std::pair<float,float>( -9.0f, 1.0));
					points.insert (std::pair<float,float>(-14.0f, 1.0));
					points.insert (std::pair<float,float>(-15.0f, 0.5)); // "50%"
					points.insert (std::pair<float,float>(-18.0f, 0.5)); // "-9"
					points.insert (std::pair<float,float>(-19.0f, 1.0)); // "30%"
					points.insert (std::pair<float,float>(-29.0f, 1.0)); // "10%"
					points.insert (std::pair<float,float>(-35.0f, 0.5)); // "5%" " -20"
					points.insert (std::pair<float,float>(-39.0f, 1.0)); // "3%"
					points.insert (std::pair<float,float>(-49.0f, 0.5)); // "1%"
					points.insert (std::pair<float,float>(-59.0f, 1.0));
					break;
				case MeterVU:
					points.insert (std::pair<float,float>(-17.0f, 1.0));
					points.insert (std::pair<float,float>(-18.0f, 1.0));
					points.insert (std::pair<float,float>(-19.0f, 1.0));
					points.insert (std::pair<float,float>(-20.0f, 1.0));
					points.insert (std::pair<float,float>(-21.0f, 1.0));
					points.insert (std::pair<float,float>(-22.0f, 1.0));
					points.insert (std::pair<float,float>(-23.0f, 1.0));
					points.insert (std::pair<float,float>(-25.0f, 1.0));
					points.insert (std::pair<float,float>(-27.0f, 1.0));
					points.insert (std::pair<float,float>(-30.0f, 1.0));
					points.insert (std::pair<float,float>(-40.0f, 1.0));
					break;

				default:
					points.insert (std::pair<float,float>(-60, 0.5));
					points.insert (std::pair<float,float>(-50, 1.0));
					points.insert (std::pair<float,float>(-40, 1.0));
					points.insert (std::pair<float,float>(-30, 1.0));
					if (Config->get_meter_line_up_level() == MeteringLineUp24) {
						points.insert (std::pair<float,float>(-24, 1.0));
					} else {
						points.insert (std::pair<float,float>(-25, 1.0));
					}
					points.insert (std::pair<float,float>(-20, 1.0));

					points.insert (std::pair<float,float>(-19, 0.5));
					points.insert (std::pair<float,float>(-18, 1.0));
					points.insert (std::pair<float,float>(-17, 0.5));
					points.insert (std::pair<float,float>(-16, 0.5));
					points.insert (std::pair<float,float>(-15, 1.0));

					points.insert (std::pair<float,float>(-14, 0.5));
					points.insert (std::pair<float,float>(-13, 0.5));
					points.insert (std::pair<float,float>(-12, 0.5));
					points.insert (std::pair<float,float>(-11, 0.5));
					points.insert (std::pair<float,float>(-10, 1.0));

					points.insert (std::pair<float,float>( -9, 1.0));
					points.insert (std::pair<float,float>( -8, 0.5));
					points.insert (std::pair<float,float>( -7, 0.5));
					points.insert (std::pair<float,float>( -6, 0.5));
					points.insert (std::pair<float,float>( -5, 1.0));
					points.insert (std::pair<float,float>( -4, 0.5));
					points.insert (std::pair<float,float>( -3, 1.0));
					points.insert (std::pair<float,float>( -2, 0.5));
					points.insert (std::pair<float,float>( -1, 0.5));

					points.insert (std::pair<float,float>(  0, 1.0));
					points.insert (std::pair<float,float>(  1, 0.5));
					points.insert (std::pair<float,float>(  2, 0.5));
					points.insert (std::pair<float,float>(  3, 1.0));
					points.insert (std::pair<float,float>(  4, 0.5));
					points.insert (std::pair<float,float>(  5, 0.5));
					break;
			}
			break;

		case DataType::MIDI:
			points.insert (std::pair<float,float>(  0, 1.0));
			points.insert (std::pair<float,float>( 16, 0.5));
			points.insert (std::pair<float,float>( 32, 0.5));
			points.insert (std::pair<float,float>( 48, 0.5));
			points.insert (std::pair<float,float>( 64, 1.0));
			points.insert (std::pair<float,float>( 80, 0.5));
			points.insert (std::pair<float,float>( 96, 0.5));
			points.insert (std::pair<float,float>(100, 1.0));
			points.insert (std::pair<float,float>(112, 0.5));
			points.insert (std::pair<float,float>(127, 1.0));
			break;
		}

		for (std::map<float,float>::const_iterator j = points.begin(); j != points.end(); ++j) {
			cairo_set_line_width (cr, (j->second));

			float fraction = 0;
			gint pos;

			switch (*i) {
			case DataType::AUDIO:
				fraction = mtr_col_and_fract(cr, &c, peakcolor, type, j->first);

				pos = height - (gint) floor (height * fraction);
				pos = max (pos, 1);
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

static cairo_pattern_t*
meter_render_metrics (Gtk::Widget& w, MeterType type, vector<DataType> types)
{
	Glib::RefPtr<Gdk::Window> win (w.get_window());

	bool tickleft, tickright;
	bool background;
	gint width, height;
	win->get_size (width, height);

	tickleft = w.get_name().substr(w.get_name().length() - 4) == "Left";
	tickright = w.get_name().substr(w.get_name().length() - 5) == "Right";
	background = types.size() == 0 || tickleft || tickright;

	if (!tickleft && !tickright) {
		tickright = true;
	}

	cairo_surface_t* surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, width, height);
	cairo_t* cr = cairo_create (surface);
	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create(w.get_pango_context());

	Pango::AttrList audio_font_attributes;
	Pango::AttrList midi_font_attributes;
	Pango::AttrList unit_font_attributes;

	Pango::AttrFontDesc* font_attr;
	Pango::FontDescription font;

	font = Pango::FontDescription ("ArdourMono");
	double fixfontsize = 81920.0 / (double) ARDOUR::Config->get_font_scale();

	font.set_weight (Pango::WEIGHT_NORMAL);
	font.set_size (9.0 * PANGO_SCALE * fixfontsize);
	font_attr = new Pango::AttrFontDesc (Pango::Attribute::create_attr_font_desc (font));
	audio_font_attributes.change (*font_attr);
	delete font_attr;

	font.set_weight (Pango::WEIGHT_ULTRALIGHT);
	font.set_stretch (Pango::STRETCH_ULTRA_CONDENSED);
	font.set_size (8.0 * PANGO_SCALE * fixfontsize);
	font_attr = new Pango::AttrFontDesc (Pango::Attribute::create_attr_font_desc (font));
	midi_font_attributes.change (*font_attr);
	delete font_attr;

	font.set_size (6.0 * PANGO_SCALE * fixfontsize);
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

	cairo_set_line_width (cr, 1.0);

	height = min(max_pattern_metric_size, height);
	uint32_t peakcolor = ARDOUR_UI::config()->color_by_name ("meterbridge peaklabel");

	for (vector<DataType>::const_iterator i = types.begin(); i != types.end(); ++i) {

		Gdk::Color c;
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


		// label-pos in dBFS, label-text
		std::map<float,string> points;

		switch (*i) {
		case DataType::AUDIO:
			layout->set_attributes (audio_font_attributes);
			switch (type) {
				case MeterK14:
					points.insert (std::pair<float,string>(-54.0f, "-40"));
					points.insert (std::pair<float,string>(-44.0f, "-30"));
					points.insert (std::pair<float,string>(-34.0f, "-20"));
					points.insert (std::pair<float,string>(-24.0f, "-10"));
					points.insert (std::pair<float,string>(-20.0f,  "-6"));
					points.insert (std::pair<float,string>(-17.0f,  "-3"));
					points.insert (std::pair<float,string>(-14.0f,   "0"));
					points.insert (std::pair<float,string>(-11.0f,  "+3"));
					points.insert (std::pair<float,string>( -8.0f,  "+6"));
					points.insert (std::pair<float,string>( -4.0f, "+10"));
					points.insert (std::pair<float,string>(  0.0f, "+14"));
					break;
				case MeterK20:
					points.insert (std::pair<float,string>(-60.0f, "-40"));
					points.insert (std::pair<float,string>(-50.0f, "-30"));
					points.insert (std::pair<float,string>(-40.0f, "-20"));
					points.insert (std::pair<float,string>(-30.0f, "-10"));
					points.insert (std::pair<float,string>(-26.0f,  "-6"));
					points.insert (std::pair<float,string>(-23.0f,  "-3"));
					points.insert (std::pair<float,string>(-20.0f,   "0"));
					points.insert (std::pair<float,string>(-17.0f,  "+3"));
					points.insert (std::pair<float,string>(-14.0f,  "+6"));
					points.insert (std::pair<float,string>(-10.0f, "+10"));
					points.insert (std::pair<float,string>( -5.0f, "+15"));
					points.insert (std::pair<float,string>(  0.0f, "+20"));
					break;
				default:
				case MeterPeak:
				case MeterKrms:
					points.insert (std::pair<float,string>(-50.0f, "-50"));
					points.insert (std::pair<float,string>(-40.0f, "-40"));
					points.insert (std::pair<float,string>(-30.0f, "-30"));
					points.insert (std::pair<float,string>(-20.0f, "-20"));
					if (types.size() == 1) {
						if (Config->get_meter_line_up_level() == MeteringLineUp24) {
							points.insert (std::pair<float,string>(-24.0f, "-24"));
						} else {
							points.insert (std::pair<float,string>(-25.0f, "-25"));
						}
						points.insert (std::pair<float,string>(-15.0f, "-15"));
					}
					points.insert (std::pair<float,string>(-18.0f, "-18"));
					points.insert (std::pair<float,string>(-10.0f, "-10"));
					points.insert (std::pair<float,string>( -5.0f, "-5"));
					points.insert (std::pair<float,string>( -3.0f, "-3"));
					points.insert (std::pair<float,string>(  0.0f, "+0"));
					points.insert (std::pair<float,string>(  3.0f, "+3"));
					break;

				case MeterIEC2EBU:
					points.insert (std::pair<float,string>(-30.0f, "-12"));
					points.insert (std::pair<float,string>(-26.0f, "-8"));
					points.insert (std::pair<float,string>(-22.0f, "-4"));
					points.insert (std::pair<float,string>(-18.0f, "TST"));
					points.insert (std::pair<float,string>(-14.0f, "+4"));
					points.insert (std::pair<float,string>(-10.0f, "+8"));
					points.insert (std::pair<float,string>( -6.0f, "+12"));
					break;

				case MeterIEC2BBC:
					points.insert (std::pair<float,string>(-30.0f, " 1 "));
					points.insert (std::pair<float,string>(-26.0f, " 2 "));
					points.insert (std::pair<float,string>(-22.0f, " 3 "));
					points.insert (std::pair<float,string>(-18.0f, " 4 "));
					points.insert (std::pair<float,string>(-14.0f, " 5 "));
					points.insert (std::pair<float,string>(-10.0f, " 6 "));
					points.insert (std::pair<float,string>( -6.0f, " 7 "));
					break;

				case MeterIEC1NOR:
					//points.insert (std::pair<float,string>(-60.0f, "-42"));
					points.insert (std::pair<float,string>(-54.0f, "-36"));
					points.insert (std::pair<float,string>(-48.0f, "-30"));
					points.insert (std::pair<float,string>(-42.0f, "-24"));
					points.insert (std::pair<float,string>(-36.0f, "-18"));

					points.insert (std::pair<float,string>(-33.0f, "-15"));
					points.insert (std::pair<float,string>(-30.0f, "-12"));
					points.insert (std::pair<float,string>(-27.0f, "-9"));
					points.insert (std::pair<float,string>(-24.0f, "-6"));
					points.insert (std::pair<float,string>(-21.0f, "-3"));

					points.insert (std::pair<float,string>(-18.0f, "TST"));
					points.insert (std::pair<float,string>(-15.0f, "+3"));
					points.insert (std::pair<float,string>(-12.0f, "+6"));
					points.insert (std::pair<float,string>( -9.0f, "+9"));
					points.insert (std::pair<float,string>( -6.0f, "+12"));
					break;

				case MeterIEC1DIN:
					//points.insert (std::pair<float,string>( -3.0f, "200%"));
					points.insert (std::pair<float,string>( -4.0f, "+5")); // "100%"
					points.insert (std::pair<float,string>( -9.0f, "0"));
					points.insert (std::pair<float,string>(-14.0f, "-5"));
					//points.insert (std::pair<float,string>(-15.0f, "50%"));
					//points.insert (std::pair<float,string>(-18.0f, "-9"));
					points.insert (std::pair<float,string>(-19.0f, "-10")); // "30%"
					points.insert (std::pair<float,string>(-29.0f, "-20")); // "10%"
					//points.insert (std::pair<float,string>(-35.0f, "-20")); // "5%"
					points.insert (std::pair<float,string>(-39.0f, "-30")); // "3%"
					//points.insert (std::pair<float,string>(-49.0f, "1%"));
					points.insert (std::pair<float,string>(-59.0f, "-50"));
					break;

				case MeterVU:
					points.insert (std::pair<float,string>(-17.0f, "+3"));
					points.insert (std::pair<float,string>(-18.0f, "+2"));
					points.insert (std::pair<float,string>(-19.0f, "+1"));
					points.insert (std::pair<float,string>(-20.0f, "0"));
					points.insert (std::pair<float,string>(-21.0f, "-1"));
					points.insert (std::pair<float,string>(-22.0f, "-2"));
					points.insert (std::pair<float,string>(-23.0f, "-3"));
					points.insert (std::pair<float,string>(-25.0f, "-5"));
					points.insert (std::pair<float,string>(-27.0f, "-7"));
					points.insert (std::pair<float,string>(-30.0f, "-10"));
					points.insert (std::pair<float,string>(-40.0f, "-20"));
					break;
			}
			break;
		case DataType::MIDI:
			layout->set_attributes (midi_font_attributes);
			points.insert (std::pair<float,string>(  0, "0"));
			if (types.size() == 1) {
				points.insert (std::pair<float,string>( 16,  "16"));
				points.insert (std::pair<float,string>( 32,  "32"));
				points.insert (std::pair<float,string>( 48,  "48"));
				points.insert (std::pair<float,string>( 64,  "64"));
				points.insert (std::pair<float,string>( 80,  "80"));
				points.insert (std::pair<float,string>( 96,  "96"));
				points.insert (std::pair<float,string>(100, "100"));
				points.insert (std::pair<float,string>(112, "112"));
			} else {
				/* labels that don't overlay with dB */
				points.insert (std::pair<float,string>( 24, "24"));
				points.insert (std::pair<float,string>( 48, "48"));
				points.insert (std::pair<float,string>( 72, "74"));
			}
			points.insert (std::pair<float,string>(127, "127"));
			break;
		}

		gint pos;

		for (std::map<float,string>::const_iterator j = points.begin(); j != points.end(); ++j) {
			float fraction = 0;
			bool align_center = background; // this is true for meterbridge meters w/ fixed background
			switch (*i) {
				case DataType::AUDIO:
					fraction = mtr_col_and_fract(cr, &c, peakcolor, type, j->first);

					pos = height - (gint) floor (height * fraction);
					pos = max (pos, 1);
					if (tickleft) {
						cairo_move_to(cr, width-1.5, pos + .5);
						cairo_line_to(cr, width, pos + .5);
						cairo_stroke (cr);
					} else if (tickright) {
						cairo_move_to(cr, 0, pos + .5);
						cairo_line_to(cr, 1.5, pos + .5);
						cairo_stroke (cr);
					}
					break;

				case DataType::MIDI:
					align_center = false; // don't bleed into legend
					fraction = (j->first) / 127.0;
					pos = 1 + height - (gint) rintf (height * fraction);
					pos = min (pos, height);
					if (tickleft) {
						cairo_arc(cr, width - 2.0, pos + .5, 1.0, 0, 2 * M_PI);
						cairo_fill(cr);
					} else if (tickright) {
						cairo_arc(cr, 3, pos + .5, 1.0, 0, 2 * M_PI);
						cairo_fill(cr);
					}
					break;
			}

			layout->set_text(j->second.c_str());

			int tw, th;
			layout->get_pixel_size(tw, th);

			int p = pos - (th / 2) - 1;
			p = min (p, height - th);
			p = max (p, 0);

			if (align_center) {
				cairo_move_to (cr, (width-tw)/2.0, p);
			} else {
				cairo_move_to (cr, width-3-tw, p);
			}
			pango_cairo_show_layout (cr, layout->gobj());
		}
	}

	// add legend
	if (types.size() == 1) {
		int tw, th;
		layout->set_attributes (unit_font_attributes);
		switch (types.at(0)) {
			case DataType::AUDIO:
				switch (type) {
					case MeterK20:
						layout->set_text("K20");
						break;
					case MeterK14:
						layout->set_text("K14");
						break;
					default:
					case MeterPeak:
					case MeterKrms:
						layout->set_text("dBFS");
						break;
					case MeterIEC2EBU:
						layout->set_text("EBU");
						break;
					case MeterIEC2BBC:
						layout->set_text("BBC");
						break;
					case MeterIEC1DIN:
						layout->set_text("DIN");
						break;
					case MeterIEC1NOR:
						layout->set_text("NOR");
						break;
					case MeterVU:
						layout->set_text("VU");
						break;
				}
				layout->get_pixel_size(tw, th);
				break;
			case DataType::MIDI:
				layout->set_text("mid");
				layout->get_pixel_size(tw, th);
				break;
		}
		Gdk::Color c = w.get_style()->get_fg (Gtk::STATE_ACTIVE);
		cairo_set_source_rgb (cr, c.get_red_p(), c.get_green_p(), c.get_blue_p());
		if (tickleft) {
			cairo_move_to (cr, width - 2 - tw, height - th - 0.5);
		} else {
			cairo_move_to (cr, 2, height - th - 0.5);
		}
		pango_cairo_show_layout (cr, layout->gobj());
	}

	cairo_pattern_t* pattern = cairo_pattern_create_for_surface (surface);

	cairo_destroy (cr);
	cairo_surface_destroy (surface);

	return pattern;
}

gint
ArdourMeter::meter_expose_ticks (GdkEventExpose *ev, MeterType type, std::vector<ARDOUR::DataType> types, Gtk::DrawingArea *mta)
{
	Glib::RefPtr<Gdk::Window> win (mta->get_window());
	cairo_t* cr;

	cr = gdk_cairo_create (win->gobj());

	/* clip to expose area */

	gdk_cairo_rectangle (cr, &ev->area);
	cairo_clip (cr);

	cairo_pattern_t* pattern;
	const MeterMatricsMapKey key (mta->get_name(), type);
	MetricPatternMap::iterator i = ticks_patterns.find (key);

	if (i == ticks_patterns.end()) {
		pattern = meter_render_ticks (*mta, type, types);
		ticks_patterns[key] = pattern;
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

gint
ArdourMeter::meter_expose_metrics (GdkEventExpose *ev, MeterType type, std::vector<ARDOUR::DataType> types, Gtk::DrawingArea *mma)
{
	Glib::RefPtr<Gdk::Window> win (mma->get_window());
	cairo_t* cr;

	cr = gdk_cairo_create (win->gobj());

	/* clip to expose area */

	gdk_cairo_rectangle (cr, &ev->area);
	cairo_clip (cr);

	cairo_pattern_t* pattern;
	const MeterMatricsMapKey key (mma->get_name(), type);
	MetricPatternMap::iterator i = metric_patterns.find (key);

	if (i == metric_patterns.end()) {
		pattern = meter_render_metrics (*mma, type, types);
		metric_patterns[key] = pattern;
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

void
ArdourMeter::meter_clear_pattern_cache(int which) {
	MetricPatternMap::iterator i = metric_patterns.begin();
	MetricPatternMap::iterator j = ticks_patterns.begin();

	while (i != metric_patterns.end()) {
		int m = 4;
		MeterMatricsMapKey const * const key = &(i->first);
		std::string n = key->_n;
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
		MeterMatricsMapKey const * const key = &(j->first);
		std::string n = key->_n;
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
