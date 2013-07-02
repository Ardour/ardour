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

#include "ardour_ui.h"
#include "global_signals.h"
#include "logmeter.h"
#include "gui_thread.h"
#include "ardour_window.h"

#include "meterbridge.h"
#include "meter_strip.h"

#include "i18n.h"

#define WITH_METRICS 1

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;

PBD::Signal1<void,MeterStrip*> MeterStrip::CatchDeletion;

MeterStrip::MetricPatterns MeterStrip::metric_patterns;
MeterStrip::TickPatterns MeterStrip::ticks_patterns;

MeterStrip::MeterStrip (Meterbridge& mtr, Session* sess, boost::shared_ptr<ARDOUR::Route> rt)
	: AxisView(sess)
	, RouteUI(sess)
	, _route(rt)
	, style_changed (false)
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
	level_meter->setup_meters (400, meter_width, 6);
#ifdef WITH_METRICSINMETER
	level_meter->pack_start (meter_metric_area, false, false);
#endif

	Gtk::Alignment *meter_align = Gtk::manage (new Gtk::Alignment());
	meter_align->set(0.5, 0.5, 0.0, 1.0);
	meter_align->add(*level_meter);

#ifdef WITH_METRICS
	meterbox.pack_start(meter_metric_area, true, false);
	meterbox.pack_start(meter_ticks1_area, true, false);
	meterbox.pack_start(*meter_align, true, true);
	meterbox.pack_start(meter_ticks2_area, true, false);
#endif

	// peak display
	peak_display.set_name ("MixerStripPeakDisplay");
	set_size_request_to_display_given_text (peak_display, "-80.g", 2, 6);
	max_peak = minus_infinity();
	peak_display.set_label (_("-inf"));
	peak_display.unset_flags (Gtk::CAN_FOCUS);

	peakbx.pack_start(peak_display, true, true);
	peakbx.set_size_request(-1, 16);

	// add track-name label -- TODO ellipsize
	label.set_text(_route->name().c_str());
	label.set_name("MeterbridgeLabel");
	label.set_angle(90.0);
	label.set_alignment(0.5, 1.0);
	label.set_size_request(12, 52);

	// rec-enable button
	btnbox.pack_start(*rec_enable_button, true, false);
	btnbox.set_size_request(-1, 16);

	pack_start (peakbx, false, false);
#ifdef WITH_METRICS
	pack_start (meterbox, true, true);
#else
	pack_start (*meter_align, true, true);
#endif
	pack_start (btnbox, false, false);
	pack_start (label, false, false, 2);

	peak_display.show();
	peakbx.show();
#ifdef WITH_METRICS
	meter_ticks1_area.show();
	meter_ticks2_area.show();
	meter_metric_area.hide();
	meterbox.show();
#endif
	level_meter->show();
	meter_align->show();
	btnbox.show();
	label.show();

	_route->shared_peak_meter()->ConfigurationChanged.connect (
			route_connections, invalidator (*this), boost::bind (&MeterStrip::meter_configuration_changed, this, _1), gui_context()
			);
	meter_configuration_changed (_route->shared_peak_meter()->input_streams ());

#ifdef WITH_METRICS
	set_size_request_to_display_given_text (meter_metric_area, "-8888", 1, 0);
	meter_metric_area.signal_expose_event().connect (
			sigc::mem_fun(*this, &MeterStrip::meter_metrics_expose));

	meter_ticks1_area.set_size_request(3,-1);
	meter_ticks2_area.set_size_request(3,-1);
	meter_ticks1_area.signal_expose_event().connect (sigc::mem_fun(*this, &MeterStrip::meter_ticks1_expose));
	meter_ticks2_area.signal_expose_event().connect (sigc::mem_fun(*this, &MeterStrip::meter_ticks2_expose));
#endif

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
	rec_enable_button->set_text (_("R"));
}

void
MeterStrip::strip_property_changed (const PropertyChange& what_changed)
{
	if (!what_changed.contains (ARDOUR::Properties::name)) {
		return;
	}
	ENSURE_GUI_THREAD (*this, &MeterStrip::strip_name_changed, what_changed)
	label.set_text(_route->name());
}

void
MeterStrip::fast_update ()
{
	char buf[32];
	float mpeak = level_meter->update_meters();
	if (mpeak > max_peak) {
		max_peak = mpeak;
		if (mpeak <= -200.0f) {
			peak_display.set_label (_("-inf"));
		} else {
			snprintf (buf, sizeof(buf), "%.1f", mpeak);
			peak_display.set_label (buf);
		}

		if (mpeak >= 0.0f) {
			peak_display.set_name ("MixerStripPeakDisplayPeak");
		}
	}
}

void
MeterStrip::display_metrics (bool show)
{
	if (show) {
		meter_metric_area.show();
	} else {
		meter_metric_area.hide();
	}
}

void
MeterStrip::on_theme_changed()
{
	style_changed = true;

	// TODO save meter_width as private var?!
	int meter_width = 6;
	if (_route->shared_peak_meter()->input_streams().n_total() == 1) {
		meter_width = 12;
	}
	level_meter->setup_meters (400, meter_width, 6);
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

#ifdef WITH_METRICS
	// TODO draw Inactive routes or busses with different styles
	if (boost::dynamic_pointer_cast<AudioTrack>(_route) == 0
			&& boost::dynamic_pointer_cast<MidiTrack>(_route) == 0
			) {
		meter_metric_area.set_name ("AudioBusMetrics");
		meter_ticks1_area.set_name ("AudioBusMetrics");
		meter_ticks2_area.set_name ("AudioBusMetrics");
	}
	else if (type == (1 << DataType::AUDIO)) {
		meter_metric_area.set_name ("AudioTrackMetrics");
		meter_ticks1_area.set_name ("AudioTrackMetrics");
		meter_ticks2_area.set_name ("AudioTrackMetrics");
	}
	else if (type == (1 << DataType::MIDI)) {
		meter_metric_area.set_name ("MidiTrackMetrics");
		meter_ticks1_area.set_name ("MidiTrackMetrics");
		meter_ticks2_area.set_name ("MidiTrackMetrics");
	} else {
		meter_metric_area.set_name ("AudioMidiTrackMetrics");
		meter_ticks1_area.set_name ("AudioMidiTrackMetrics");
		meter_ticks2_area.set_name ("AudioMidiTrackMetrics");
	}
	meter_metric_area.queue_draw ();
#endif
	style_changed = true;
}

void
MeterStrip::on_size_request (Gtk::Requisition* r)
{
	style_changed = true;
	VBox::on_size_request(r);
}

void
MeterStrip::on_size_allocate (Gtk::Allocation& a)
{
	style_changed = true;
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
	font.set_size (10.0 * PANGO_SCALE);
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

	height = min(1024, height); // XXX see FastMeter::max_pattern_metric_size

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

		vector<int> points;

		switch (*i) {
		case DataType::AUDIO:
			layout->set_attributes (audio_font_attributes);
			points.push_back (-50);
			points.push_back (-40);
			points.push_back (-30);
			points.push_back (-20);
			points.push_back (-18);
			points.push_back (-10);
			points.push_back (-6);
			points.push_back (-3);
			points.push_back (0);
			points.push_back (3);
			break;

		case DataType::MIDI:
			layout->set_attributes (midi_font_attributes);
			points.push_back (0);
			if (types.size() == 1) {
				points.push_back (32);
			} else {
				/* tweak so as not to overlay the -30dB mark */
				points.push_back (48);
			}
			if (types.size() == 1) {
				points.push_back (64); // very close to -18dB
				points.push_back (96); // overlays with -6dB mark
			} else {
				points.push_back (72);
				points.push_back (88);
			}
			points.push_back (127);
			break;
		}

		char buf[32];

		for (vector<int>::const_iterator j = points.begin(); j != points.end(); ++j) {

			float fraction = 0;
			switch (*i) {
			case DataType::AUDIO:
				fraction = log_meter (*j);
				snprintf (buf, sizeof (buf), "%+2d", *j);
				break;
			case DataType::MIDI:
				fraction = *j / 127.0;
				snprintf (buf, sizeof (buf), "%3d", *j);
				break;
			}

			gint const pos = height - (gint) floor (height * fraction);
			layout->set_text(buf);

			/* we want logical extents, not ink extents here */

			int tw, th;
			layout->get_pixel_size(tw, th);

			int p = pos - (th / 2);
			p = min (p, height - th);
			p = max (p, 0);

			cairo_move_to (cr, width-2-tw, p + .5);
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
		cairo_move_to (cr, 1, height - th);
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

	if (i == metric_patterns.end() || style_changed) {
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

	style_changed = false;

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

	height = min(1024, height); // XXX see FastMeter::max_pattern_metric_size

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
			points.insert (std::pair<int,float>(-50, 0.5));
			points.insert (std::pair<int,float>(-40, 0.5));
			points.insert (std::pair<int,float>(-30, 0.5));
			points.insert (std::pair<int,float>(-20, 0.5));
			points.insert (std::pair<int,float>(-18, 1.0));
			points.insert (std::pair<int,float>(-15, 0.5));
			points.insert (std::pair<int,float>(-10, 1.0));
			points.insert (std::pair<int,float>( -9, 0.5));
			points.insert (std::pair<int,float>( -8, 0.5));
			points.insert (std::pair<int,float>( -7, 0.5));
			points.insert (std::pair<int,float>( -6, 0.5));
			points.insert (std::pair<int,float>( -5, 1.0));
			points.insert (std::pair<int,float>( -4, 0.5));
			points.insert (std::pair<int,float>( -3, 1.0));
			points.insert (std::pair<int,float>( -2, 0.5));
			points.insert (std::pair<int,float>( -1, 0.5));
			points.insert (std::pair<int,float>(  0, 1.0));
			points.insert (std::pair<int,float>(  1, 0.5));
			points.insert (std::pair<int,float>(  2, 0.5));
			points.insert (std::pair<int,float>(  3, 0.5));
			points.insert (std::pair<int,float>(  4, 0.5));
			points.insert (std::pair<int,float>(  5, 0.5));
			points.insert (std::pair<int,float>(  6, 0.5));
			break;

		case DataType::MIDI:
			points.insert (std::pair<int,float>(  0, 1.0));
			points.insert (std::pair<int,float>( 16, 0.5));
			points.insert (std::pair<int,float>( 32, 0.5));
			points.insert (std::pair<int,float>( 48, 0.5));
			points.insert (std::pair<int,float>( 64, 1.0));
			points.insert (std::pair<int,float>( 72, 0.5));
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
				if (j->first >= 0) {
					cairo_set_source_rgb (cr, 1.0, 0.0, 0.0);
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
				cairo_arc(cr, 1.5, pos, (j->second), 0, 2 * M_PI);
				cairo_fill_preserve(cr);
				cairo_stroke (cr);
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

	if (i == ticks_patterns.end() || style_changed) {
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

	style_changed = false;

	cairo_destroy (cr);

	return true;
}

void
MeterStrip::reset_group_peak_display (RouteGroup* group)
{
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
	peak_display.set_label (_("-inf"));
	peak_display.set_name ("MixerStripPeakDisplay");
}

bool
MeterStrip::peak_button_release (GdkEventButton* ev)
{
	reset_peak_display ();
	return true;
}
