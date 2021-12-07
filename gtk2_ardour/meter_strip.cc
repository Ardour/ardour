/*
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
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

#include <list>

#include <sigc++/bind.h>

#include "pbd/unwind.h"

#include "ardour/logmeter.h"
#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/route_group.h"
#include "ardour/meter.h"

#include "ardour/audio_track.h"
#include "ardour/midi_track.h"

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"

#include "widgets/tooltips.h"

#include "gui_thread.h"
#include "ardour_window.h"
#include "context_menu_helper.h"
#include "ui_config.h"
#include "utils.h"

#include "meterbridge.h"
#include "meter_strip.h"
#include "meter_patterns.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;
using namespace ArdourMeter;

PBD::Signal1<void,MeterStrip*> MeterStrip::CatchDeletion;
PBD::Signal0<void> MeterStrip::MetricChanged;
PBD::Signal0<void> MeterStrip::ConfigurationChanged;

#define PX_SCALE(pxmin, dflt) rint(std::max((double)pxmin, (double)dflt * UIConfiguration::instance().get_ui_scale()))

MeterStrip::MeterStrip (int metricmode, MeterType mt)
	: RouteUI ((Session*) 0)
	, metric_type (MeterPeak)
	, _clear_meters (true)
	, _meter_peaked (false)
	, _has_midi (false)
	, _tick_bar (0)
	, _strip_type (0)
	, _metricmode (-1)
	, level_meter (0)
	, _suspend_menu_callbacks (false)
{
	mtr_vbox.set_spacing (PX_SCALE(2, 2));
	nfo_vbox.set_spacing (PX_SCALE(2, 2));
	peakbx.set_size_request (-1, PX_SCALE(14, 14));
	namebx.set_size_request (PX_SCALE(16, 18), PX_SCALE(32, 52));
	spacer.set_size_request (-1,0);

	set_metric_mode(metricmode, mt);

	meter_metric_area.set_size_request (PX_SCALE(25, 25), 10);
	meter_metric_area.signal_expose_event().connect (
			sigc::mem_fun(*this, &MeterStrip::meter_metrics_expose));
	RedrawMetrics.connect (sigc::mem_fun(*this, &MeterStrip::redraw_metrics));

	meterbox.pack_start(meter_metric_area, true, false);

	mtr_vbox.pack_start (peakbx, false, false);
	mtr_vbox.pack_start (meterbox, true, true);
	mtr_vbox.pack_start (spacer, false, false);
	mtr_container.add(mtr_vbox);

	mtr_hsep.set_size_request (-1, 1);
	mtr_hsep.set_name("BlackSeparator");

	nfo_vbox.pack_start (mtr_hsep, false, false);
	nfo_vbox.pack_start (btnbox, false, false);
	nfo_vbox.pack_start (namebx, false, false);

	pack_start (mtr_container, true, true);
	pack_start (nfo_vbox, false, false);

	peakbx.show();
	btnbox.show();
	meter_metric_area.show();
	meterbox.show();
	spacer.show();
	mtr_vbox.show();
	mtr_container.show();
	mtr_hsep.show();
	nfo_vbox.show();

	UI::instance()->theme_changed.connect (sigc::mem_fun(*this, &MeterStrip::on_theme_changed));
	UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &MeterStrip::on_theme_changed));
	UIConfiguration::instance().DPIReset.connect (sigc::mem_fun (*this, &MeterStrip::on_theme_changed));
}

MeterStrip::MeterStrip (Session* sess, boost::shared_ptr<ARDOUR::Route> rt)
	: SessionHandlePtr (sess)
	, RouteUI ((Session*) 0)
	, _route (rt)
	, metric_type (MeterPeak)
	, _clear_meters (true)
	, _meter_peaked (false)
	, _has_midi (false)
	, _tick_bar (0)
	, _strip_type (0)
	, _metricmode (-1)
	, level_meter (0)
	, gain_control (ArdourKnob::default_elements, ArdourKnob::Detent)
	, _suspend_menu_callbacks (false)
{
	mtr_vbox.set_spacing (PX_SCALE(2, 2));
	nfo_vbox.set_spacing (PX_SCALE(2, 2));
	RouteUI::init ();
	RouteUI::set_route (rt);

	// note: level_meter->setup_meters() does the scaling
	int meter_width = 6;
	if (_route->shared_peak_meter()->input_streams().n_total() == 1) {
		meter_width = 12;
	}

	// level meter + ticks
	level_meter = new LevelMeterHBox(sess);
	level_meter->set_meter (_route->shared_peak_meter().get());
	level_meter->clear_meters();
	level_meter->setup_meters (220, meter_width, 6);
	level_meter->ButtonPress.connect_same_thread (level_meter_connection, boost::bind (&MeterStrip::level_meter_button_press, this, _1));
	_route->shared_peak_meter()->MeterTypeChanged.connect (meter_route_connections, invalidator (*this), boost::bind (&MeterStrip::meter_type_changed, this, _1), gui_context());

	meter_align.set(0.5, 0.5, 0.0, 1.0);
	meter_align.add(*level_meter);

	meterbox.pack_start(meter_ticks1_area, true, false);
	meterbox.pack_start(meter_align, true, true);
	meterbox.pack_start(meter_ticks2_area, true, false);

	// peak display
	peak_display.set_name ("meterbridge peakindicator");
	peak_display.set_elements((ArdourButton::Element) (ArdourButton::Edge|ArdourButton::Body));
	set_tooltip (peak_display, _("Reset Peak"));
	peak_display.unset_flags (Gtk::CAN_FOCUS);
	peak_display.set_size_request(PX_SCALE(12, 12), PX_SCALE(8, 8));
	peak_display.set_corner_radius(2); // ardour-button scales this

	peak_align.set(0.5, 1.0, 1.0, 0.8);
	peak_align.add(peak_display);
	peakbx.pack_start(peak_align, true, true, 2);
	peakbx.set_size_request(-1, PX_SCALE(14, 14));

	// add track-name & -number label
	number_label.set_text("-");
	number_label.set_size_request(PX_SCALE(18, 18), PX_SCALE(18, 18));

	name_changed();

	name_label.set_corner_radius(2); // ardour button scales radius
	name_label.set_elements((ArdourButton::Element)(ArdourButton::Edge|ArdourButton::Body|ArdourButton::Text|ArdourButton::Inactive));
	name_label.set_name("meterbridge label");
	name_label.set_angle(-90.0);
	name_label.set_text_ellipsize (Pango::ELLIPSIZE_END);
	name_label.set_layout_ellipsize_width(48 * PANGO_SCALE);
	name_label.set_size_request(PX_SCALE(18, 18), PX_SCALE(50, 50));
	name_label.set_alignment(-1.0, .5);
	set_tooltip (name_label, Gtkmm2ext::markup_escape_text (_route->name()));
	set_tooltip (*level_meter, Gtkmm2ext::markup_escape_text (_route->name()));

	number_label.set_corner_radius(2);
	number_label.set_elements((ArdourButton::Element)(ArdourButton::Edge|ArdourButton::Body|ArdourButton::Text|ArdourButton::Inactive));
	number_label.set_name("tracknumber label");
	number_label.set_angle(-90.0);
	number_label.set_layout_ellipsize_width(18 * PANGO_SCALE);
	number_label.set_alignment(.5, .5);

	namebx.set_size_request(PX_SCALE(18, 18), PX_SCALE(52, 52));
	namebx.pack_start(namenumberbx, true, false, 0);
	namenumberbx.pack_start(name_label, true, false, 0);
	namenumberbx.pack_start(number_label, false, false, 0);

	mon_in_box.pack_start(*monitor_input_button, true, false);
	btnbox.pack_start(mon_in_box, false, false, 1);
	mon_disk_box.pack_start(*monitor_disk_button, true, false);
	btnbox.pack_start(mon_disk_box, false, false, 1);

	recbox.pack_start(*rec_enable_button, true, false);
	btnbox.pack_start(recbox, false, false, 1);
	mutebox.pack_start(*mute_button, true, false);
	btnbox.pack_start(mutebox, false, false, 1);
	solobox.pack_start(*solo_button, true, false);
	btnbox.pack_start(solobox, false, false, 1);

	/* Fader/Gain */
	gain_control.set_size_request (PX_SCALE (18, 18), PX_SCALE (18, 18));
	gain_control.set_tooltip_prefix (_("Level: "));
	gain_control.set_name ("trim knob"); // XXX
	gain_control.StartGesture.connect (sigc::mem_fun (*this, &MeterStrip::gain_start_touch));
	gain_control.StopGesture.connect (sigc::mem_fun (*this, &MeterStrip::gain_end_touch));
	gain_control.set_controllable (_route->gain_control ());

	gain_box.pack_start(gain_control, true, false);
	btnbox.pack_start(gain_box, false, false, 1);

	rec_enable_button->set_corner_radius(2);
	rec_enable_button->set_size_request (PX_SCALE(18, 18), PX_SCALE(18, 18));

	mute_button->set_corner_radius(2);
	mute_button->set_size_request (PX_SCALE(18, 18), PX_SCALE(18, 18));

	solo_button->set_corner_radius(2);
	solo_button->set_size_request (PX_SCALE(18, 18), PX_SCALE(18, 18));

	monitor_input_button->set_corner_radius(2);
	monitor_input_button->set_size_request (PX_SCALE(18, 18), PX_SCALE(18, 18));

	monitor_disk_button->set_corner_radius(2);
	monitor_disk_button->set_size_request (PX_SCALE(18, 18), PX_SCALE(18, 18));

	mutebox.set_size_request (PX_SCALE(18, 18), PX_SCALE(18, 18));
	solobox.set_size_request (PX_SCALE(18, 18), PX_SCALE(18, 18));
	recbox.set_size_request (PX_SCALE(18, 18), PX_SCALE(18, 18));
	mon_in_box.set_size_request (PX_SCALE(18, 18), PX_SCALE(18, 18));
	mon_disk_box.set_size_request (PX_SCALE(18, 18), PX_SCALE(18, 18));
	gain_box.set_size_request (PX_SCALE(18, 18), PX_SCALE(18, 18));
	spacer.set_size_request(-1,0);

	update_button_box();
	update_name_box();
	update_background (_route->meter_type());

	mtr_vbox.pack_start (peakbx, false, false);
	mtr_vbox.pack_start (meterbox, true, true);
	mtr_vbox.pack_start (spacer, false, false);
	mtr_container.add(mtr_vbox);

	mtr_hsep.set_size_request(-1,1);
	mtr_hsep.set_name("BlackSeparator");

	nfo_vbox.pack_start (mtr_hsep, false, false);
	nfo_vbox.pack_start (btnbox, false, false);
	nfo_vbox.pack_start (namebx, false, false);

	pack_start (mtr_container, true, true);
	pack_start (nfo_vbox, false, false);

	name_label.show();
	peak_display.show();
	peakbx.show();
	gain_control.show ();
	meter_ticks1_area.show();
	meter_ticks2_area.show();
	meterbox.show();
	spacer.show();
	level_meter->show();
	meter_align.show();
	peak_align.show();
	btnbox.show();
	mtr_vbox.show();
	mtr_container.show();
	mtr_hsep.show();
	nfo_vbox.show();
	namenumberbx.show();

	if (boost::dynamic_pointer_cast<Track>(_route)) {
		monitor_input_button->show();
		monitor_disk_button->show();
	} else {
		monitor_input_button->hide();
		monitor_disk_button->hide();
	}

	_route->shared_peak_meter()->ConfigurationChanged.connect (
			meter_route_connections, invalidator (*this), boost::bind (&MeterStrip::meter_configuration_changed, this, _1), gui_context()
			);

	ResetAllPeakDisplays.connect (sigc::mem_fun(*this, &MeterStrip::reset_peak_display));
	ResetRoutePeakDisplays.connect (sigc::mem_fun(*this, &MeterStrip::reset_route_peak_display));
	ResetGroupPeakDisplays.connect (sigc::mem_fun(*this, &MeterStrip::reset_group_peak_display));
	RedrawMetrics.connect (sigc::mem_fun(*this, &MeterStrip::redraw_metrics));
	SetMeterTypeMulti.connect (sigc::mem_fun(*this, &MeterStrip::set_meter_type_multi));

	meter_configuration_changed (_route->shared_peak_meter()->input_streams ());

	meter_ticks1_area.set_size_request(PX_SCALE(3, 3), -1);
	meter_ticks2_area.set_size_request(PX_SCALE(3, 3), -1);
	meter_ticks1_area.signal_expose_event().connect (sigc::mem_fun(*this, &MeterStrip::meter_ticks1_expose));
	meter_ticks2_area.signal_expose_event().connect (sigc::mem_fun(*this, &MeterStrip::meter_ticks2_expose));

	_route->DropReferences.connect (meter_route_connections, invalidator (*this), boost::bind (&MeterStrip::self_delete, this), gui_context());

	peak_display.signal_button_release_event().connect (sigc::mem_fun(*this, &MeterStrip::peak_button_release), false);
	name_label.signal_button_release_event().connect (sigc::mem_fun(*this, &MeterStrip::name_label_button_release), false);

	UI::instance()->theme_changed.connect (sigc::mem_fun(*this, &MeterStrip::on_theme_changed));
	UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &MeterStrip::on_theme_changed));
	UIConfiguration::instance().DPIReset.connect (sigc::mem_fun (*this, &MeterStrip::on_theme_changed));
	Config->ParameterChanged.connect (*this, invalidator (*this), ui_bind (&MeterStrip::parameter_changed, this, _1), gui_context());
	sess->config.ParameterChanged.connect (*this, invalidator (*this), ui_bind (&MeterStrip::parameter_changed, this, _1), gui_context());

	if (_route->is_master()) {
		_strip_type = 4;
	}
	else if (boost::dynamic_pointer_cast<AudioTrack>(_route) == 0
			&& boost::dynamic_pointer_cast<MidiTrack>(_route) == 0) {
		/* non-master bus */
		_strip_type = 3;
	}
	else if (boost::dynamic_pointer_cast<MidiTrack>(_route)) {
		_strip_type = 2;
	}
	else {
		_strip_type = 1;
	}
}

MeterStrip::~MeterStrip ()
{
	if (level_meter) {
		delete level_meter;
		CatchDeletion (this);
	}
}

void
MeterStrip::self_delete ()
{
	delete this;
}

void
MeterStrip::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);
	if (!s) return;
	s->config.ParameterChanged.connect (*this, invalidator (*this), ui_bind (&MeterStrip::parameter_changed, this, _1), gui_context());
	update_button_box();
	update_name_box();
}

void
MeterStrip::blink_rec_display (bool onoff)
{
	RouteUI::blink_rec_display (onoff);
}

std::string
MeterStrip::state_id() const
{
	if (_route) {
		return string_compose ("mtrs %1", _route->id().to_s());
	} else {
		return string ();
	}
}

void
MeterStrip::set_button_names()
{
	mute_button->set_text (S_("Mute|M"));

	if (_route && _route->solo_safe_control()->solo_safe()) {
		solo_button->set_visual_state (Gtkmm2ext::VisualState (solo_button->visual_state() | Gtkmm2ext::Insensitive));
	} else {
		solo_button->set_visual_state (Gtkmm2ext::VisualState (solo_button->visual_state() & ~Gtkmm2ext::Insensitive));
	}
	if (!Config->get_solo_control_is_listen_control()) {
		solo_button->set_text (S_("Solo|S"));
	} else {
		switch (Config->get_listen_position()) {
		case AfterFaderListen:
			solo_button->set_text (S_("AfterFader|A"));
			break;
		case PreFaderListen:
			solo_button->set_text (S_("PreFader|P"));
			break;
		}
	}

	monitor_input_button->set_text (S_("MonitorInput|I"));
	monitor_disk_button->set_text (S_("MonitorDisk|D"));
}

void
MeterStrip::route_property_changed (const PropertyChange& what_changed)
{
	if (!what_changed.contains (ARDOUR::Properties::name)) {
		return;
	}
	ENSURE_GUI_THREAD (*this, &MeterStrip::strip_name_changed, what_changed);
	name_changed();
	set_tooltip (name_label, _route->name());
	if (level_meter) {
		set_tooltip (*level_meter, _route->name());
	}
}

void
MeterStrip::route_color_changed ()
{
	number_label.set_fixed_colors (gdk_color_to_rgba (color()), gdk_color_to_rgba (color()));
}


void
MeterStrip::fast_update ()
{
	if (_clear_meters) {
		level_meter->clear_meters();
		peak_display.set_active_state (Gtkmm2ext::Off);
		_clear_meters = false;
		_meter_peaked = false;
	}

	const float mpeak = level_meter->update_meters();
	const bool peaking = mpeak > UIConfiguration::instance().get_meter_peak();

	if (!_meter_peaked && peaking) {
		peak_display.set_active_state ( Gtkmm2ext::ExplicitActive );
		_meter_peaked = true;
	}
}

void
MeterStrip::on_theme_changed()
{
	if (level_meter && _route) {
		int meter_width = 6;
		if (_route->shared_peak_meter()->input_streams().n_total() == 1) {
			meter_width = 12;
		}
		level_meter->setup_meters (220, meter_width, 6);
	}
}

void
MeterStrip::meter_configuration_changed (ChanCount c)
{
	int type = 0;
	_types.clear ();
	bool old_has_midi = _has_midi;

	for (DataType::iterator i = DataType::begin(); i != DataType::end(); ++i) {
		if (c.get (*i) > 0) {
			_types.push_back (*i);
			type |= 1 << (*i);
		}
	}

	if (boost::dynamic_pointer_cast<AudioTrack>(_route) == 0
			&& boost::dynamic_pointer_cast<MidiTrack>(_route) == 0
			) {
		meter_ticks1_area.set_name ("MyAudioBusMetricsLeft");
		meter_ticks2_area.set_name ("MyAudioBusMetricsRight");
		_has_midi = false;
	}
	else if (type == (1 << DataType::AUDIO)) {
		meter_ticks1_area.set_name ("MyAudioTrackMetricsLeft");
		meter_ticks2_area.set_name ("MyAudioTrackMetricsRight");
		_has_midi = false;
	}
	else if (type == (1 << DataType::MIDI)) {
		meter_ticks1_area.set_name ("MidiTrackMetricsLeft");
		meter_ticks2_area.set_name ("MidiTrackMetricsRight");
		_has_midi = true;
	} else {
		meter_ticks1_area.set_name ("AudioMidiTrackMetricsLeft");
		meter_ticks2_area.set_name ("AudioMidiTrackMetricsRight");
		_has_midi = true;
	}
	set_tick_bar(_tick_bar);

	on_theme_changed();
	if (old_has_midi != _has_midi) {
		MetricChanged(); /* EMIT SIGNAL */
	}
	else ConfigurationChanged();
}

void
MeterStrip::set_tick_bar (int m)
{
	std::string n;
	_tick_bar = m;
	if (_tick_bar & 1) {
		n = meter_ticks1_area.get_name();
		if (n.substr(0,3) != "Bar") {
			meter_ticks1_area.set_name("Bar" + n);
		}
	} else {
		n = meter_ticks1_area.get_name();
		if (n.substr(0,3) == "Bar") {
			meter_ticks1_area.set_name (n.substr (3));
		}
	}
	if (_tick_bar & 2) {
		n = meter_ticks2_area.get_name();
		if (n.substr(0,3) != "Bar") {
			meter_ticks2_area.set_name("Bar" + n);
		}
	} else {
		n = meter_ticks2_area.get_name();
		if (n.substr(0,3) == "Bar") {
			meter_ticks2_area.set_name (n.substr (3));
		}
	}
}

void
MeterStrip::on_size_request (Gtk::Requisition* r)
{
	VBox::on_size_request(r);
}

void
MeterStrip::on_size_allocate (Gtk::Allocation& a)
{
	const int wh = a.get_height();
	int nh;
	int mh = 0;
	if (_session) {
		mh = _session->config.get_meterbridge_label_height();
	}
	switch (mh) {
		default:
		case 0:
			nh = ceilf(wh * .12f);
			if (nh < 52) nh = 52;
			if (nh > 148) nh = 148;
			break;
		case 1:
			nh = 52;
			break;
		case 2:
			nh = 88;
			break;
		case 3:
			nh = 106;
			break;
		case 4:
			nh = 148;
			break;
	}
	int tnh = 0;
	if (_session && _session->config.get_track_name_number()) {
		// NB numbers are rotated 90deg. on the meterbridge
		tnh = 4 + std::max(2u, _session->track_number_decimals()) * 8; // TODO 8 = max_with_of_digit_0_to_9()
	}

	nh *= UIConfiguration::instance().get_ui_scale();
	tnh *= UIConfiguration::instance().get_ui_scale();

	int prev_height, ignored;
	bool need_relayout = false;

	namebx.get_size_request (ignored, prev_height);
	namebx.set_size_request (PX_SCALE(18, 18), nh + tnh);

	if (prev_height != nh + tnh) {
		need_relayout = true;
	}

	namenumberbx.get_size_request (ignored, prev_height);
	namenumberbx.set_size_request (PX_SCALE(18, 18), nh + tnh);

	if (prev_height != nh + tnh) {
		need_relayout = true;
	}

	if (_route) {
		int nlh = nh + (_route->is_master() ? tnh : -1);
		name_label.get_size_request(ignored, prev_height);
		name_label.set_size_request (PX_SCALE(18, 18), nlh);
		name_label.set_layout_ellipsize_width ((nh - 4 + (_route->is_master() ? tnh : 0)) * PANGO_SCALE); // XXX
		if (prev_height != nlh) {
			need_relayout = true;
		}
	}

	VBox::on_size_allocate(a);

	if (need_relayout) {
		queue_resize();
		/* force re-layout, parent on_scroll(), queue_resize() */
		MetricChanged(); /* EMIT SIGNAL */
	}
}

gint
MeterStrip::meter_metrics_expose (GdkEventExpose *ev)
{
	if (_route) {
		return meter_expose_metrics(ev, _route->meter_type(), _types, &meter_metric_area);
	} else {
		return meter_expose_metrics(ev, metric_type, _types, &meter_metric_area);
	}
}

void
MeterStrip::set_metric_mode (int metricmode, ARDOUR::MeterType mt)
{
	if (metric_type == mt && _metricmode == metricmode) {
		return;
	}
	metric_type = mt;
	_metricmode = metricmode;

	_types.clear ();
	switch(metricmode) {
		case 0:
			meter_metric_area.set_name ("MidiTrackMetricsLeft");
			_types.push_back (DataType::MIDI);
			break;
		case 1:
			meter_metric_area.set_name ("AudioTrackMetricsLeft");
			_types.push_back (DataType::AUDIO);
			break;
		case 2:
			meter_metric_area.set_name ("MidiTrackMetricsRight");
			_types.push_back (DataType::MIDI);
			break;
		case 3:
		default:
			meter_metric_area.set_name ("AudioTrackMetricsRight");
			_types.push_back (DataType::AUDIO);
			break;
	}
	update_background (mt);
	meter_metric_area.queue_draw ();
}

void
MeterStrip::update_background(MeterType type)
{
	switch(type) {
		case MeterIEC1DIN:
		case MeterIEC1NOR:
		case MeterIEC2BBC:
		case MeterIEC2EBU:
		case MeterK12:
		case MeterK14:
		case MeterK20:
			mtr_container.set_name ("meterstripPPM");
			break;
		case MeterVU:
			mtr_container.set_name ("meterstripVU");
			break;
		default:
			mtr_container.set_name ("meterstripDPM");
	}
}

MeterType
MeterStrip::meter_type()
{
	assert((!_route && _strip_type == 0) || (_route && _strip_type != 0));
	if (!_route) return metric_type;
	return _route->meter_type();
}

gint
MeterStrip::meter_ticks1_expose (GdkEventExpose *ev)
{
	assert(_route);
	return meter_expose_ticks(ev, _route->meter_type(), _types, &meter_ticks1_area);
}

gint
MeterStrip::meter_ticks2_expose (GdkEventExpose *ev)
{
	assert(_route);
	return meter_expose_ticks(ev, _route->meter_type(), _types, &meter_ticks2_area);
}

void
MeterStrip::reset_route_peak_display (Route* route)
{
	if (_route && _route.get() == route) {
		reset_peak_display ();
	}
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
	_clear_meters = true;
}

bool
MeterStrip::peak_button_release (GdkEventButton* ev)
{
	if (ev->button == 1 && Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier|Keyboard::TertiaryModifier)) {
		ResetAllPeakDisplays ();
	} else if (ev->button == 1 && Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
		if (_route) {
			ResetGroupPeakDisplays (_route->route_group());
		}
	} else {
		ResetRoutePeakDisplays (_route.get());
	}
	return false;
}

void
MeterStrip::redraw_metrics ()
{
	meter_metric_area.queue_draw();
	meter_ticks1_area.queue_draw();
	meter_ticks2_area.queue_draw();
}

void
MeterStrip::update_button_box ()
{
	if (!_session) return;
	int height = 0;
	if (_session->config.get_show_mute_on_meterbridge()) {
		height += PX_SCALE(18, 18) + PX_SCALE(2, 2);
		mutebox.show();
	} else {
		mutebox.hide();
	}
	if (_session->config.get_show_solo_on_meterbridge()) {
		height += PX_SCALE(18, 18) + PX_SCALE(2, 2);
		solobox.show();
	} else {
		solobox.hide();
	}
	if (_session->config.get_show_rec_on_meterbridge()) {
		height += PX_SCALE(18, 18) + PX_SCALE(2, 2);
		recbox.show();
	} else {
		recbox.hide();
	}
	if (_session->config.get_show_monitor_on_meterbridge()) {
		height += PX_SCALE(18, 18) + PX_SCALE(2, 2);
		height += PX_SCALE(18, 18) + PX_SCALE(2, 2);
		mon_in_box.show();
		mon_disk_box.show();
	} else {
		mon_in_box.hide();
		mon_disk_box.hide();
	}
	if (_session->config.get_show_fader_on_meterbridge ()) {
		height += PX_SCALE(18, 18) + PX_SCALE(2, 2);
		gain_box.show ();
	} else {
		gain_box.hide ();
	}
	btnbox.set_size_request(PX_SCALE(18, 18), height);
	check_resize();
}

void
MeterStrip::update_name_box ()
{
	if (!_session) return;
	if (_session->config.get_show_name_on_meterbridge()) {
		namebx.show();
	} else {
		namebx.hide();
	}
}

void
MeterStrip::parameter_changed (std::string const & p)
{
	if (p == "meter-peak") {
		_clear_meters = true;
	}
	else if (p == "show-rec-on-meterbridge") {
		update_button_box();
	}
	else if (p == "show-mute-on-meterbridge") {
		update_button_box();
	}
	else if (p == "show-solo-on-meterbridge") {
		update_button_box();
	}
	else if (p == "show-name-on-meterbridge") {
		update_name_box();
	}
	else if (p == "show-monitor-on-meterbridge") {
		update_button_box();
	}
	else if (p == "show-fader-on-meterbridge") {
		update_button_box();
	}
	else if (p == "meterbridge-label-height") {
		queue_resize();
	}
	else if (p == "track-name-number") {
		name_changed();
		queue_resize();
	}
}

void
MeterStrip::name_changed () {
	if (!_route) {
		return;
	}
	name_label.set_text(_route->name ());
	if (_session && _session->config.get_track_name_number()) {
		const uint64_t track_number = _route->track_number();
		if (track_number == 0) {
			number_label.set_text("-");
			number_label.hide();
		} else {
			number_label.set_text (PBD::to_string (track_number));
			number_label.show();
		}
		const int tnh = 4 + std::max(2u, _session->track_number_decimals()) * 8; // TODO 8 = max_width_of_digit_0_to_9()
		// NB numbers are rotated 90deg. on the meterbridge -> use height
		number_label.set_size_request(PX_SCALE(18, 18), tnh * UIConfiguration::instance().get_ui_scale());
	} else {
		number_label.hide();
	}
}

bool
MeterStrip::level_meter_button_press (GdkEventButton* ev)
{
	if (ev->button == 3) {
		if (_route && _route->shared_peak_meter()->input_streams ().n_audio() > 0) {
			popup_level_meter_menu (ev);
		}
		return true;
	}

	return false;
}

void
MeterStrip::popup_level_meter_menu (GdkEventButton* ev)
{
	using namespace Gtk::Menu_Helpers;

	Gtk::Menu* m = ARDOUR_UI_UTILS::shared_popup_menu ();
	MenuList& items = m->items ();

	RadioMenuItem::Group group;

	PBD::Unwinder<bool> uw (_suspend_menu_callbacks, true);
	add_level_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterPeak), MeterPeak);
	add_level_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterPeak0dB), MeterPeak0dB);
	add_level_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterKrms),  MeterKrms);
	add_level_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterIEC1DIN), MeterIEC1DIN);
	add_level_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterIEC1NOR), MeterIEC1NOR);
	add_level_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterIEC2BBC), MeterIEC2BBC);
	add_level_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterIEC2EBU), MeterIEC2EBU);
	add_level_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterK20), MeterK20);
	add_level_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterK14), MeterK14);
	add_level_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterK12), MeterK12);
	add_level_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterVU),  MeterVU);

	MeterType cmt = _route->meter_type();
	const std::string cmn = ArdourMeter::meter_type_string(cmt);

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (string_compose(_("Change all in Group to %1"), cmn),
				sigc::bind (SetMeterTypeMulti, -1, _route->route_group(), cmt)));
	items.push_back (MenuElem (string_compose(_("Change all to %1"), cmn),
				sigc::bind (SetMeterTypeMulti, 0, _route->route_group(), cmt)));
	items.push_back (MenuElem (string_compose(_("Change same track-type to %1"), cmn),
				sigc::bind (SetMeterTypeMulti, _strip_type, _route->route_group(), cmt)));

	m->popup (ev->button, ev->time);
}

bool
MeterStrip::name_label_button_release (GdkEventButton* ev)
{
	if (!_session) return true;
	if (!_session->config.get_show_name_on_meterbridge()) return true;

	if (ev->button == 3) {
		popup_name_label_menu (ev);
		return true;
	}

	return false;
}

void
MeterStrip::popup_name_label_menu (GdkEventButton* ev)
{
	using namespace Gtk::Menu_Helpers;

	Gtk::Menu* m = ARDOUR_UI_UTILS::shared_popup_menu ();
	MenuList& items = m->items ();

	RadioMenuItem::Group group;

	PBD::Unwinder<bool> uw (_suspend_menu_callbacks, true);
	add_label_height_item (items, group, _("Variable height"), 0);
	add_label_height_item (items, group, _("Short"), 1);
	add_label_height_item (items, group, _("Tall"), 2);
	add_label_height_item (items, group, _("Grande"), 3);
	add_label_height_item (items, group, _("Venti"), 4);

	m->popup (ev->button, ev->time);
}

void
MeterStrip::add_label_height_item (Menu_Helpers::MenuList& items, RadioMenuItem::Group& group, string const & name, uint32_t h)
{
	using namespace Menu_Helpers;

	items.push_back (RadioMenuElem (group, name, sigc::bind (sigc::mem_fun (*this, &MeterStrip::set_label_height), h)));
	RadioMenuItem* i = dynamic_cast<RadioMenuItem *> (&items.back ());
	i->set_active (_session && _session->config.get_meterbridge_label_height() == h);
}

void
MeterStrip::add_level_meter_type_item (Menu_Helpers::MenuList& items, RadioMenuItem::Group& group, string const & name, MeterType type)
{
	using namespace Menu_Helpers;

	items.push_back (RadioMenuElem (group, name, sigc::bind (sigc::mem_fun (*this, &MeterStrip::set_meter_type), type)));
	RadioMenuItem* i = dynamic_cast<RadioMenuItem *> (&items.back ());
	i->set_active (_route->meter_type() == type);
}

void
MeterStrip::set_meter_type (MeterType type)
{
	if (_suspend_menu_callbacks) return;
	_route->set_meter_type (type);
}

void
MeterStrip::set_label_height (uint32_t h)
{
	if (_suspend_menu_callbacks) return;
	_session->config.set_meterbridge_label_height(h);
}

void
MeterStrip::meter_type_changed (MeterType type)
{
	update_background (type);
	MetricChanged(); /* EMIT SIGNAL */
}

void
MeterStrip::set_meter_type_multi (int what, RouteGroup* group, MeterType type)
{
	switch (what) {
		case -1:
			if (_route && group == _route->route_group()) {
				_route->set_meter_type (type);
			}
			break;
		case 0:
			_route->set_meter_type (type);
			break;
		default:
			if (what == _strip_type) {
				_route->set_meter_type (type);
			}
			break;
	}
}

string
MeterStrip::name () const
{
	return _route->name();
}

Gdk::Color
MeterStrip::color () const
{
	return RouteUI::route_color ();
}

void
MeterStrip::gain_start_touch ()
{
	_route->gain_control ()->start_touch (timepos_t (_session->transport_sample ()));
}

void
MeterStrip::gain_end_touch ()
{
	_route->gain_control ()->stop_touch (timepos_t (_session->transport_sample ()));
}
