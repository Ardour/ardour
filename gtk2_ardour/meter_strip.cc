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
#include "logmeter.h"
#include "gui_thread.h"
#include "ardour_window.h"

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
MeterStrip::TickPatterns MeterStrip::tick_patterns;

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

	// add level meter
	level_meter = new LevelMeter(sess);
	level_meter->set_meter (_route->shared_peak_meter().get());
	level_meter->clear_meters();
	level_meter->setup_meters (350, meter_width, 6);
	level_meter->pack_start (meter_metric_area, false, false);

	Gtk::Alignment *meter_align = Gtk::manage (new Gtk::Alignment());
	meter_align->set(0.5, 0.5, 0.0, 1.0);
	meter_align->add(*level_meter);

	// add track-name label
	// TODO
	// * fixed-height labels (or table layout)
	// * print lables at angle (allow longer text)
	label.set_text(_route->name().c_str());
	label.set_name("MeterbridgeLabel");
#if 0
	label.set_ellipsize(Pango::ELLIPSIZE_MIDDLE);
	label.set_max_width_chars(7);
	label.set_width_chars(7);
	label.set_alignment(0.5, 0.5);
#else //ellipsize & angle are incompatible :(
	label.set_angle(90.0);
	label.set_alignment(0.5, 0.0);
#endif
	label.set_size_request(12, 36);

	Gtk::HBox* btnbox = Gtk::manage (new Gtk::HBox());
	btnbox->pack_start(*rec_enable_button, true, false);
	btnbox->set_size_request(-1, 16);

	pack_start(*meter_align, true, true);
	pack_start (*btnbox, false, false);
	pack_start (label, false, false);

	meter_metric_area.show();
	level_meter->show();
	meter_align->show();
	btnbox->show();
	label.show();

	_route->shared_peak_meter()->ConfigurationChanged.connect (
			route_connections, invalidator (*this), boost::bind (&MeterStrip::meter_configuration_changed, this, _1), gui_context()
			);
	meter_configuration_changed (_route->shared_peak_meter()->input_streams ());

	set_size_request_to_display_given_text (meter_metric_area, "-8888", 1, 0);
	meter_metric_area.signal_expose_event().connect (
			sigc::mem_fun(*this, &MeterStrip::meter_metrics_expose));

	_route->DropReferences.connect (route_connections, invalidator (*this), boost::bind (&MeterStrip::self_delete, this), gui_context());
	_route->PropertyChanged.connect (route_connections, invalidator (*this), boost::bind (&MeterStrip::strip_property_changed, this, _1), gui_context());

	UI::instance()->theme_changed.connect (sigc::mem_fun(*this, &MeterStrip::on_theme_changed));
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
	const float mpeak = level_meter->update_meters();
	// TODO peak indicator if mpeak > 0
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
		meter_metric_area.set_name ("AudioBusMetrics");
	}
	else if (type == (1 << DataType::AUDIO)) {
		meter_metric_area.set_name ("AudioTrackMetrics");
	}
	else if (type == (1 << DataType::MIDI)) {
		meter_metric_area.set_name ("MidiTrackMetrics");
	} else {
		meter_metric_area.set_name ("AudioMidiTrackMetrics");
	}
	style_changed = true;
	meter_metric_area.queue_draw ();
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
			points.push_back (4);
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

			cairo_set_line_width (cr, 1.0);
			cairo_move_to (cr, width-3.5, pos);
			cairo_line_to (cr, width, pos);
			cairo_stroke (cr);

			layout->set_text(buf);

			/* we want logical extents, not ink extents here */

			int tw, th;
			layout->get_pixel_size(tw, th);

			int p = pos - (th / 2);
			p = min (p, height - th);
			p = max (p, 0);

			cairo_move_to (cr, width-5-tw, p);
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
