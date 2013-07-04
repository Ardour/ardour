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

#include <list>

#include <sigc++/bind.h>

#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/route_group.h"
#include "ardour/meter.h"

#include "ardour/audio_track.h"
#include "ardour/midi_track.h"

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/rgb_macros.h>

#include "ardour_ui.h"
#include "global_signals.h"
#include "logmeter.h"
#include "gui_thread.h"
#include "ardour_window.h"
#include "utils.h"

#include "meterbridge.h"
#include "meter_strip.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;

PBD::Signal1<void,MeterStrip*> MeterStrip::CatchDeletion;

MeterStrip::MetricPatterns MeterStrip::metric_patterns;
MeterStrip::TickPatterns MeterStrip::ticks_patterns;
int MeterStrip::max_pattern_metric_size = 1024;

MeterStrip::MeterStrip (int metricmode)
	: AxisView(0)
	, RouteUI(0)
{
	level_meter = 0;
	set_spacing(2);
	peakbx.set_size_request(-1, 14);
	btnbox.set_size_request(-1, 16);
	namebx.set_size_request(18, 52);

	_types.clear ();
	switch(metricmode) {
		case 1:
			meter_metric_area.set_name ("AudioBusMetrics");
			_types.push_back (DataType::AUDIO);
			break;
		case 2:
			meter_metric_area.set_name ("AudioTrackMetrics");
			_types.push_back (DataType::AUDIO);
			break;
		case 3:
			meter_metric_area.set_name ("MidiTrackMetrics");
			_types.push_back (DataType::MIDI);
			break;
		default:
			meter_metric_area.set_name ("AudioMidiTrackMetrics");
			_types.push_back (DataType::AUDIO);
			_types.push_back (DataType::MIDI);
			break;
	}
	//meter_metric_area.queue_draw ();

	set_size_request_to_display_given_text (meter_metric_area, "-8888", 1, 0);
	meter_metric_area.signal_expose_event().connect (
			sigc::mem_fun(*this, &MeterStrip::meter_metrics_expose));

	meterbox.pack_start(meter_metric_area, true, false);

	pack_start (peakbx, false, false);
	pack_start (meterbox, true, true);
	pack_start (btnbox, false, false);
	pack_start (namebx, false, false);

	peakbx.show();
	btnbox.show();
	meter_metric_area.show();
	meterbox.show();
	namebx.show();

	UI::instance()->theme_changed.connect (sigc::mem_fun(*this, &MeterStrip::on_theme_changed));
	ColorsChanged.connect (sigc::mem_fun (*this, &MeterStrip::on_theme_changed));
	DPIReset.connect (sigc::mem_fun (*this, &MeterStrip::on_theme_changed));
}

MeterStrip::MeterStrip (Session* sess, boost::shared_ptr<ARDOUR::Route> rt)
	: AxisView(sess)
	, RouteUI(sess)
	, _route(rt)
	, peak_display()
{
	set_spacing(2);
	RouteUI::set_route (rt);

	int meter_width = 6;
	if (_route->shared_peak_meter()->input_streams().n_total() == 1) {
		meter_width = 12;
	}

	// level meter + ticks
	level_meter = new LevelMeter(sess);
	level_meter->set_meter (_route->shared_peak_meter().get());
	level_meter->clear_meters();
	level_meter->setup_meters (220, meter_width, 6);

	meter_align.set(0.5, 0.5, 0.0, 1.0);
	meter_align.add(*level_meter);

	meterbox.pack_start(meter_ticks1_area, true, false);
	meterbox.pack_start(meter_align, true, true);
	meterbox.pack_start(meter_ticks2_area, true, false);

	// peak display
	peak_display.set_name ("meterbridge peakindicator");
	peak_display.set_elements((ArdourButton::Element) (ArdourButton::Edge|ArdourButton::Body));
	max_peak = minus_infinity();
	peak_display.unset_flags (Gtk::CAN_FOCUS);
	peak_display.set_size_request(12, 8);
	peak_display.set_corner_radius(2);

	peak_align.set(0.5, 1.0, 1.0, 0.8);
	peak_align.add(peak_display);
	peakbx.pack_start(peak_align, true, true, 3);
	peakbx.set_size_request(-1, 14);

	// add track-name label
	name_label.set_text(_route->name().c_str());
	name_label.set_corner_radius(2);
	name_label.set_name("solo isolate"); // XXX re-use 'very_small_text'
	name_label.set_angle(-90.0);
	name_label.layout()->set_ellipsize (Pango::ELLIPSIZE_END);
	name_label.layout()->set_width(48 * PANGO_SCALE);
	name_label.set_size_request(18, 50);

	namebx.set_size_request(18, 52);
	namebx.pack_start(name_label, true, false, 3);

	// rec-enable button
	btnbox.pack_start(*rec_enable_button, true, false);
	rec_enable_button->set_corner_radius(2);
	btnbox.set_size_request(-1, 16);

	pack_start (peakbx, false, false);
	pack_start (meterbox, true, true);
	pack_start (btnbox, false, false);
	pack_start (namebx, false, false);

	peak_display.show();
	peakbx.show();
	meter_ticks1_area.show();
	meter_ticks2_area.show();
	meterbox.show();
	level_meter->show();
	meter_align.show();
	peak_align.show();
	btnbox.show();
	name_label.show();
	namebx.show();

	_route->shared_peak_meter()->ConfigurationChanged.connect (
			route_connections, invalidator (*this), boost::bind (&MeterStrip::meter_configuration_changed, this, _1), gui_context()
			);
	meter_configuration_changed (_route->shared_peak_meter()->input_streams ());

	meter_ticks1_area.set_size_request(3,-1);
	meter_ticks2_area.set_size_request(3,-1);
	meter_ticks1_area.signal_expose_event().connect (sigc::mem_fun(*this, &MeterStrip::meter_ticks1_expose));
	meter_ticks2_area.signal_expose_event().connect (sigc::mem_fun(*this, &MeterStrip::meter_ticks2_expose));

	_route->DropReferences.connect (route_connections, invalidator (*this), boost::bind (&MeterStrip::self_delete, this), gui_context());
	_route->PropertyChanged.connect (route_connections, invalidator (*this), boost::bind (&MeterStrip::strip_property_changed, this, _1), gui_context());

	peak_display.signal_button_release_event().connect (sigc::mem_fun(*this, &MeterStrip::peak_button_release), false);

	UI::instance()->theme_changed.connect (sigc::mem_fun(*this, &MeterStrip::on_theme_changed));
	ColorsChanged.connect (sigc::mem_fun (*this, &MeterStrip::on_theme_changed));
	DPIReset.connect (sigc::mem_fun (*this, &MeterStrip::on_theme_changed));
}

MeterStrip::~MeterStrip ()
{
	delete level_meter;
	CatchDeletion (this);
}

void
MeterStrip::self_delete ()
{
	delete this;
}

void
MeterStrip::update_rec_display ()
{
	RouteUI::update_rec_display ();
}

std::string
MeterStrip::state_id() const
{
	return string_compose ("mtrs %1", _route->id().to_s());
}

void
MeterStrip::set_button_names()
{
	rec_enable_button->set_text ("");
	rec_enable_button->set_image (::get_icon (X_("record_normal_red")));
}

void
MeterStrip::strip_property_changed (const PropertyChange& what_changed)
{
	if (!what_changed.contains (ARDOUR::Properties::name)) {
		return;
	}
	ENSURE_GUI_THREAD (*this, &MeterStrip::strip_name_changed, what_changed)
	name_label.set_text(_route->name());
}

void
MeterStrip::fast_update ()
{
	float mpeak = level_meter->update_meters();
	if (mpeak > max_peak) {
		max_peak = mpeak;
		if (mpeak >= 0.0f) {
			peak_display.set_name ("meterbridge peakindicator on");
			peak_display.set_elements((ArdourButton::Element) (ArdourButton::Edge|ArdourButton::Body));
		}
	}
}

void
MeterStrip::on_theme_changed()
{
	metric_patterns.clear();
	ticks_patterns.clear();

	if (level_meter && _route) {
		int meter_width = 6;
		if (_route->shared_peak_meter()->input_streams().n_total() == 1) {
			meter_width = 12;
		}
		level_meter->setup_meters (220, meter_width, 6);
	}
	meter_metric_area.queue_draw();
	meter_ticks1_area.queue_draw();
	meter_ticks2_area.queue_draw();
}

void
MeterStrip::meter_configuration_changed (ChanCount c)
{
	int type = 0;
	_types.clear ();

	for (DataType::iterator i = DataType::begin(); i != DataType::end(); ++i) {
		if (c.get (*i) > 0) {
			_types.push_back (*i);
			type |= 1 << (*i);
		}
	}

	// TODO draw Inactive routes or busses with different styles
	if (boost::dynamic_pointer_cast<AudioTrack>(_route) == 0
			&& boost::dynamic_pointer_cast<MidiTrack>(_route) == 0
			) {
		meter_ticks1_area.set_name ("AudioBusMetrics");
		meter_ticks2_area.set_name ("AudioBusMetrics");
	}
	else if (type == (1 << DataType::AUDIO)) {
		meter_ticks1_area.set_name ("AudioTrackMetrics");
		meter_ticks2_area.set_name ("AudioTrackMetrics");
	}
	else if (type == (1 << DataType::MIDI)) {
		meter_ticks1_area.set_name ("MidiTrackMetrics");
		meter_ticks2_area.set_name ("MidiTrackMetrics");
	} else {
		meter_ticks1_area.set_name ("AudioMidiTrackMetrics");
		meter_ticks2_area.set_name ("AudioMidiTrackMetrics");
	}

	on_theme_changed();
}

void
MeterStrip::on_size_request (Gtk::Requisition* r)
{
	metric_patterns.clear();
	ticks_patterns.clear();
	VBox::on_size_request(r);
}

void
MeterStrip::on_size_allocate (Gtk::Allocation& a)
{
	metric_patterns.clear();
	ticks_patterns.clear();
	const int wh = a.get_height();
	int nh = ceilf(wh * .11f);
	if (nh < 52) nh = 52;
	if (nh > 148) nh = 148;
	namebx.set_size_request(18, nh);
	if (_route) {
		name_label.set_size_request(18, nh-2);
		name_label.layout()->set_width((nh-4) * PANGO_SCALE);
	}
	VBox::on_size_allocate(a);
}

/* XXX code-copy from gain_meter.cc -- TODO consolidate
 *
 * slightly different:
 *  - ticks & label positions are swapped
 *  - more ticks for audio (longer meter by default)
 *  - right-aligned lables, unit-legend
 *  - height limitation of FastMeter::max_pattern_metric_size is included here
 */
cairo_pattern_t*
MeterStrip::render_metrics (Gtk::Widget& w, vector<DataType> types)
{
	Glib::RefPtr<Gdk::Window> win (w.get_window());

	gint width, height;
	win->get_size (width, height);

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
		Gdk::Color c = w.get_style()->get_bg (Gtk::STATE_NORMAL);
		cairo_set_source_rgb (cr, c.get_red_p(), c.get_green_p(), c.get_blue_p());
	}
	cairo_fill (cr);

	if (height > max_pattern_metric_size) {
		cairo_move_to (cr, 0, max_pattern_metric_size);
		cairo_rectangle (cr, 0, max_pattern_metric_size, width, height);
		Gdk::Color c = w.get_style()->get_bg (Gtk::STATE_ACTIVE);
		cairo_set_source_rgb (cr, c.get_red_p(), c.get_green_p(), c.get_blue_p());
		cairo_fill (cr);
	}

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
			points.insert (std::pair<int,float>(-25, 0.5));
			points.insert (std::pair<int,float>(-20, 1.0));
			points.insert (std::pair<int,float>(-18, 1.0));
			points.insert (std::pair<int,float>(-15, 1.0));
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
				points.insert (std::pair<int,float>( 48, 0.5));
				points.insert (std::pair<int,float>( 72, 0.5));
				points.insert (std::pair<int,float>( 88, 0.5));
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
				cairo_move_to(cr, width-2.5, pos + .5);
				cairo_line_to(cr, width, pos + .5);
				cairo_stroke (cr);
				break;
			case DataType::MIDI:
				cairo_set_line_width (cr, 1.0);
				fraction = (j->first) / 127.0;
				snprintf (buf, sizeof (buf), "%3d", j->first);
				pos = height - (gint) rintf (height * fraction);
				cairo_arc(cr, 3, pos, 1.0, 0, 2 * M_PI);
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
		cairo_move_to (cr, 1, height - th - 1.5);
		pango_cairo_show_layout (cr, layout->gobj());
	}

	cairo_pattern_t* pattern = cairo_pattern_create_for_surface (surface);
	MetricPatterns::iterator p;

	if ((p = metric_patterns.find (w.get_name())) != metric_patterns.end()) {
		cairo_pattern_destroy (p->second);
	}

	metric_patterns[w.get_name()] = pattern;

	cairo_destroy (cr);
	cairo_surface_destroy (surface);

	return pattern;
}

/* XXX code-copy from gain_meter.cc -- TODO consolidate */
gint
MeterStrip::meter_metrics_expose (GdkEventExpose *ev)
{
	Glib::RefPtr<Gdk::Window> win (meter_metric_area.get_window());
	cairo_t* cr;

	cr = gdk_cairo_create (win->gobj());

	/* clip to expose area */

	gdk_cairo_rectangle (cr, &ev->area);
	cairo_clip (cr);

	cairo_pattern_t* pattern;
	MetricPatterns::iterator i = metric_patterns.find (meter_metric_area.get_name());

	if (i == metric_patterns.end()) {
		pattern = render_metrics (meter_metric_area, _types);
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

cairo_pattern_t*
MeterStrip::render_ticks (Gtk::Widget& w, vector<DataType> types)
{
	Glib::RefPtr<Gdk::Window> win (w.get_window());

	gint width, height;
	win->get_size (width, height);

	cairo_surface_t* surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, width, height);
	cairo_t* cr = cairo_create (surface);

	cairo_move_to (cr, 0, 0);
	cairo_rectangle (cr, 0, 0, width, height);
	{
		Gdk::Color c = w.get_style()->get_bg (Gtk::STATE_NORMAL);
		cairo_set_source_rgb (cr, c.get_red_p(), c.get_green_p(), c.get_blue_p());
	}
	cairo_fill (cr);

	if (height > max_pattern_metric_size) {
		cairo_move_to (cr, 0, max_pattern_metric_size);
		cairo_rectangle (cr, 0, max_pattern_metric_size, width, height);
		Gdk::Color c = w.get_style()->get_bg (Gtk::STATE_ACTIVE);
		cairo_set_source_rgb (cr, c.get_red_p(), c.get_green_p(), c.get_blue_p());
		cairo_fill (cr);
	}

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
			points.insert (std::pair<int,float>(-25, 0.5));
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
				pos = height - (gint) floor (height * fraction);
				cairo_arc(cr, 1.5, pos, 1.0, 0, 2 * M_PI);
				cairo_fill(cr);
				break;
			}
		}
	}

	cairo_pattern_t* pattern = cairo_pattern_create_for_surface (surface);
	TickPatterns::iterator p;

	if ((p = ticks_patterns.find (w.get_name())) != metric_patterns.end()) {
		cairo_pattern_destroy (p->second);
	}

	ticks_patterns[w.get_name()] = pattern;

	cairo_destroy (cr);
	cairo_surface_destroy (surface);

	return pattern;
}

gint
MeterStrip::meter_ticks1_expose (GdkEventExpose *ev)
{
	return meter_ticks_expose(ev, &meter_ticks1_area);
}

gint
MeterStrip::meter_ticks2_expose (GdkEventExpose *ev)
{
	return meter_ticks_expose(ev, &meter_ticks2_area);
}

gint
MeterStrip::meter_ticks_expose (GdkEventExpose *ev, Gtk::DrawingArea *mta)
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
		pattern = render_ticks (*mta, _types);
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
MeterStrip::reset_group_peak_display (RouteGroup* group)
{
	/* UNUSED -- need connection w/mixer || other meters */
	if (_route && group == _route->route_group()) {
		reset_peak_display ();
	}
}

void
MeterStrip::reset_peak_display ()
{
	_route->shared_peak_meter()->reset_max();
	level_meter->clear_meters();
	max_peak = -INFINITY;
	peak_display.set_name ("meterbridge peakindicator");
	peak_display.set_elements((ArdourButton::Element) (ArdourButton::Edge|ArdourButton::Body));
}

bool
MeterStrip::peak_button_release (GdkEventButton* ev)
{
	reset_peak_display ();
	return true;
}
