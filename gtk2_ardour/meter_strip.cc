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
#include <gtkmm2ext/keyboard.h>
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
#include "meter_patterns.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;
using namespace ArdourMeter;

PBD::Signal1<void,MeterStrip*> MeterStrip::CatchDeletion;
PBD::Signal0<void> MeterStrip::MetricChanged;
PBD::Signal0<void> MeterStrip::ConfigurationChanged;

MeterStrip::MeterStrip (int metricmode, MeterType mt)
	: AxisView(0)
	, RouteUI(0)
{
	level_meter = 0;
	_strip_type = 0;
	_tick_bar = 0;
	_metricmode = -1;
	metric_type = MeterPeak;
	mtr_vbox.set_spacing(2);
	nfo_vbox.set_spacing(2);
	peakbx.set_size_request(-1, 14);
	namebx.set_size_request(18, 52);
	spacer.set_size_request(-1,0);

	set_metric_mode(metricmode, mt);

	meter_metric_area.set_size_request(25, 10);
	meter_metric_area.signal_expose_event().connect (
			sigc::mem_fun(*this, &MeterStrip::meter_metrics_expose));
	RedrawMetrics.connect (sigc::mem_fun(*this, &MeterStrip::redraw_metrics));

	meterbox.pack_start(meter_metric_area, true, false);

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
	ColorsChanged.connect (sigc::mem_fun (*this, &MeterStrip::on_theme_changed));
	DPIReset.connect (sigc::mem_fun (*this, &MeterStrip::on_theme_changed));
}

MeterStrip::MeterStrip (Session* sess, boost::shared_ptr<ARDOUR::Route> rt)
	: AxisView(sess)
	, RouteUI(sess)
	, _route(rt)
	, peak_display()
{
	mtr_vbox.set_spacing(2);
	nfo_vbox.set_spacing(2);
	RouteUI::set_route (rt);
	SessionHandlePtr::set_session (sess);

	_has_midi = false;
	_tick_bar = 0;
	_metricmode = -1;
	metric_type = MeterPeak;

	int meter_width = 6;
	if (_route->shared_peak_meter()->input_streams().n_total() == 1) {
		meter_width = 12;
	}

	// level meter + ticks
	level_meter = new LevelMeterHBox(sess);
	level_meter->set_meter (_route->shared_peak_meter().get());
	level_meter->clear_meters();
	level_meter->set_type (_route->meter_type());
	level_meter->setup_meters (220, meter_width, 6);
	level_meter->ButtonRelease.connect_same_thread (level_meter_connection, boost::bind (&MeterStrip::level_meter_button_release, this, _1));
	level_meter->MeterTypeChanged.connect_same_thread (level_meter_connection, boost::bind (&MeterStrip::meter_type_changed, this, _1));

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
	name_label.set_text(_route->name());
	name_label.set_corner_radius(2);
	name_label.set_name("meterbridge label");
	name_label.set_angle(-90.0);
	name_label.layout()->set_ellipsize (Pango::ELLIPSIZE_END);
	name_label.layout()->set_width(48 * PANGO_SCALE);
	name_label.set_size_request(18, 50);
	name_label.set_alignment(-1.0, .5);
	ARDOUR_UI::instance()->set_tip (name_label, _route->name());
	ARDOUR_UI::instance()->set_tip (*level_meter, _route->name());

	namebx.set_size_request(18, 52);
	namebx.pack_start(name_label, true, false, 3);

	recbox.pack_start(*rec_enable_button, true, false);
	btnbox.pack_start(recbox, false, false, 1);
	mutebox.pack_start(*mute_button, true, false);
	btnbox.pack_start(mutebox, false, false, 1);
	solobox.pack_start(*solo_button, true, false);
	btnbox.pack_start(solobox, false, false, 1);

	rec_enable_button->set_corner_radius(2);
	rec_enable_button->set_size_request(16, 16);

	mute_button->set_corner_radius(2);
	mute_button->set_size_request(16, 16);

	solo_button->set_corner_radius(2);
	solo_button->set_size_request(16, 16);

	mutebox.set_size_request(16, 16);
	solobox.set_size_request(16, 16);
	recbox.set_size_request(16, 16);
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

	_route->shared_peak_meter()->ConfigurationChanged.connect (
			route_connections, invalidator (*this), boost::bind (&MeterStrip::meter_configuration_changed, this, _1), gui_context()
			);

	ResetAllPeakDisplays.connect (sigc::mem_fun(*this, &MeterStrip::reset_peak_display));
	ResetRoutePeakDisplays.connect (sigc::mem_fun(*this, &MeterStrip::reset_route_peak_display));
	ResetGroupPeakDisplays.connect (sigc::mem_fun(*this, &MeterStrip::reset_group_peak_display));
	RedrawMetrics.connect (sigc::mem_fun(*this, &MeterStrip::redraw_metrics));
	SetMeterTypeMulti.connect (sigc::mem_fun(*this, &MeterStrip::set_meter_type_multi));

	meter_configuration_changed (_route->shared_peak_meter()->input_streams ());

	meter_ticks1_area.set_size_request(3,-1);
	meter_ticks2_area.set_size_request(3,-1);
	meter_ticks1_area.signal_expose_event().connect (sigc::mem_fun(*this, &MeterStrip::meter_ticks1_expose));
	meter_ticks2_area.signal_expose_event().connect (sigc::mem_fun(*this, &MeterStrip::meter_ticks2_expose));

	_route->DropReferences.connect (route_connections, invalidator (*this), boost::bind (&MeterStrip::self_delete, this), gui_context());
	_route->PropertyChanged.connect (route_connections, invalidator (*this), boost::bind (&MeterStrip::strip_property_changed, this, _1), gui_context());

	peak_display.signal_button_release_event().connect (sigc::mem_fun(*this, &MeterStrip::peak_button_release), false);
	name_label.signal_button_release_event().connect (sigc::mem_fun(*this, &MeterStrip::name_label_button_release), false);

	UI::instance()->theme_changed.connect (sigc::mem_fun(*this, &MeterStrip::on_theme_changed));
	ColorsChanged.connect (sigc::mem_fun (*this, &MeterStrip::on_theme_changed));
	DPIReset.connect (sigc::mem_fun (*this, &MeterStrip::on_theme_changed));
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
	delete level_meter;
	CatchDeletion (this);
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
	mute_button->set_text (_("M"));
	rec_enable_button->set_text ("");
	rec_enable_button->set_image (::get_icon (X_("record_normal_red")));

	if (_route && _route->solo_safe()) {
		solo_button->set_visual_state (Gtkmm2ext::VisualState (solo_button->visual_state() | Gtkmm2ext::Insensitive));
	} else {
		solo_button->set_visual_state (Gtkmm2ext::VisualState (solo_button->visual_state() & ~Gtkmm2ext::Insensitive));
	}
	if (!Config->get_solo_control_is_listen_control()) {
		solo_button->set_text (_("S"));
	} else {
		switch (Config->get_listen_position()) {
		case AfterFaderListen:
			solo_button->set_text (_("A"));
			break;
		case PreFaderListen:
			solo_button->set_text (_("P"));
			break;
		}
	}

}

void
MeterStrip::strip_property_changed (const PropertyChange& what_changed)
{
	if (!what_changed.contains (ARDOUR::Properties::name)) {
		return;
	}
	ENSURE_GUI_THREAD (*this, &MeterStrip::strip_name_changed, what_changed)
	name_label.set_text(_route->name());
	ARDOUR_UI::instance()->set_tip (name_label, _route->name());
	if (level_meter) {
		ARDOUR_UI::instance()->set_tip (*level_meter, _route->name());
	}
}

void
MeterStrip::fast_update ()
{
	float mpeak = level_meter->update_meters();
	if (mpeak > max_peak) {
		max_peak = mpeak;
		if (mpeak >= Config->get_meter_peak()) {
			peak_display.set_name ("meterbridge peakindicator on");
			peak_display.set_elements((ArdourButton::Element) (ArdourButton::Edge|ArdourButton::Body));
		}
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
	if (old_has_midi != _has_midi) MetricChanged();
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
			meter_ticks1_area.set_name(n.substr(3,-1));
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
			meter_ticks2_area.set_name(n.substr(3,-1));
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
	namebx.set_size_request(18, nh);
	if (_route) {
		name_label.set_size_request(18, nh-2);
		name_label.layout()->set_width((nh-4) * PANGO_SCALE);
	}
	VBox::on_size_allocate(a);
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
	level_meter->clear_meters();
	max_peak = -INFINITY;
	peak_display.set_name ("meterbridge peakindicator");
	peak_display.set_elements((ArdourButton::Element) (ArdourButton::Edge|ArdourButton::Body));
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
	return true;
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
		height += 18;
		mutebox.show();
	} else {
		mutebox.hide();
	}
	if (_session->config.get_show_solo_on_meterbridge()) {
		height += 18;
		solobox.show();
	} else {
		solobox.hide();
	}
	if (_session->config.get_show_rec_on_meterbridge()) {
		height += 18;
		recbox.show();
	} else {
		recbox.hide();
	}
	btnbox.set_size_request(16, height);
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
		max_peak = -INFINITY;
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
	else if (p == "meterbridge-label-height") {
		queue_resize();
	}
}

bool
MeterStrip::level_meter_button_release (GdkEventButton* ev)
{
	if (ev->button == 3) {
		popup_level_meter_menu (ev);
		return true;
	}

	return false;
}

void
MeterStrip::popup_level_meter_menu (GdkEventButton* ev)
{
	using namespace Gtk::Menu_Helpers;

	Gtk::Menu* m = manage (new Menu);
	MenuList& items = m->items ();

	RadioMenuItem::Group group;

	_suspend_menu_callbacks = true;
	add_level_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterPeak), MeterPeak);
	add_level_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterKrms),  MeterKrms);
	add_level_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterIEC1DIN), MeterIEC1DIN);
	add_level_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterIEC1NOR), MeterIEC1NOR);
	add_level_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterIEC2BBC), MeterIEC2BBC);
	add_level_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterIEC2EBU), MeterIEC2EBU);
	add_level_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterK20), MeterK20);
	add_level_meter_type_item (items, group, ArdourMeter::meter_type_string(MeterK14), MeterK14);
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
	_suspend_menu_callbacks = false;
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

	Gtk::Menu* m = manage (new Menu);
	MenuList& items = m->items ();

	RadioMenuItem::Group group;

	_suspend_menu_callbacks = true;
	add_label_height_item (items, group, _("Variable height"), 0);
	add_label_height_item (items, group, _("Short"), 1);
	add_label_height_item (items, group, _("Tall"), 2);
	add_label_height_item (items, group, _("Grande"), 3);
	add_label_height_item (items, group, _("Venti"), 4);

	m->popup (ev->button, ev->time);
	_suspend_menu_callbacks = false;
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
	if (_route->meter_type() == type) return;

	level_meter->set_type (type);
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
	if (_route->meter_type() != type) {
		_route->set_meter_type(type);
	}
	update_background (type);
	MetricChanged();
}

void
MeterStrip::set_meter_type_multi (int what, RouteGroup* group, MeterType type)
{
	switch (what) {
		case -1:
			if (_route && group == _route->route_group()) {
				level_meter->set_type (type);
			}
			break;
		case 0:
			level_meter->set_type (type);
		default:
			if (what == _strip_type) {
				level_meter->set_type (type);
			}
			break;
	}
}
