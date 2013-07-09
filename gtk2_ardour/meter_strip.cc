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

PBD::Signal1<void,MeterStrip*> MeterStrip::CatchDeletion;
PBD::Signal0<void> MeterStrip::MetricChanged;

MeterStrip::MeterStrip (int metricmode)
	: AxisView(0)
	, RouteUI(0)
{
	level_meter = 0;
	_strip_type = 0;
	set_spacing(2);
	peakbx.set_size_request(-1, 14);
	namebx.set_size_request(18, 52);
	numbx.set_size_request(18, 42);

	set_metric_mode(metricmode);

	meter_metric_area.set_size_request(25, 10);
	meter_metric_area.signal_expose_event().connect (
			sigc::mem_fun(*this, &MeterStrip::meter_metrics_expose));
	RedrawMetrics.connect (sigc::mem_fun(*this, &MeterStrip::redraw_metrics));

	meterbox.pack_start(meter_metric_area, true, false);

	pack_start (peakbx, false, false);
	pack_start (meterbox, true, true);
	pack_start (btnbox, false, false);
	pack_start (namebx, false, false);
	pack_start (numbx, false, false);

	peakbx.show();
	btnbox.show();
	meter_metric_area.show();
	meterbox.show();

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
	SessionHandlePtr::set_session (sess);

	_has_midi = false;

	int meter_width = 6;
	if (_route->shared_peak_meter()->input_streams().n_total() == 1) {
		meter_width = 12;
	}

	// level meter + ticks
	level_meter = new LevelMeter(sess);
	level_meter->set_meter (_route->shared_peak_meter().get());
	level_meter->clear_meters();
	level_meter->setup_meters (220, meter_width, 6);
	level_meter->set_type (_route->meter_type());
	level_meter->ButtonPress.connect_same_thread (level_meter_connection, boost::bind (&MeterStrip::level_meter_button_press, this, _1));
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

	number_label.set_alignment(1.0, .5);
	number_label.set_name("meterbridge numlabel");

	if (_route->unique_id() > 0) {
		char buf[12];
		snprintf(buf, 12, "%d", _route->unique_id());
		number_label.set_text(buf);
	} else {
		number_label.set_text("");
	}

	namebx.set_size_request(18, 52);
	namebx.pack_start(name_label, true, false, 3);

	numbx.set_size_request(18, 42);
	numbx.pack_start(number_label, true, false, 0);

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

	update_button_box();
	update_name_box();

	pack_start (peakbx, false, false);
	pack_start (meterbox, true, true);
	pack_start (btnbox, false, false);
	pack_start (namebx, false, false);
	pack_start (numbx, false, false);

	number_label.show();
	name_label.show();
	peak_display.show();
	peakbx.show();
	meter_ticks1_area.show();
	meter_ticks2_area.show();
	meterbox.show();
	level_meter->show();
	meter_align.show();
	peak_align.show();
	btnbox.show();

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
	solo_button->set_text (_("S"));
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
	ARDOUR_UI::instance()->set_tip (name_label, _route->name());
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
		meter_ticks1_area.set_name ("AudioBusMetricsLeft");
		meter_ticks2_area.set_name ("AudioBusMetricsRight");
		_has_midi = false;
	}
	else if (type == (1 << DataType::AUDIO)) {
		meter_ticks1_area.set_name ("AudioTrackMetricsLeft");
		meter_ticks2_area.set_name ("AudioTrackMetricsRight");
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

	if (old_has_midi != _has_midi) MetricChanged();
	on_theme_changed();
}

void
MeterStrip::on_size_request (Gtk::Requisition* r)
{
	meter_clear_pattern_cache();
	VBox::on_size_request(r);
}

void
MeterStrip::on_size_allocate (Gtk::Allocation& a)
{
	meter_clear_pattern_cache();
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

gint
MeterStrip::meter_metrics_expose (GdkEventExpose *ev)
{
	return meter_expose_metrics(ev, _types, &meter_metric_area);
}

void
MeterStrip::set_metric_mode (int metricmode)
{
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

	meter_metric_area.queue_draw ();
}

void
MeterStrip::set_pos (int pos)
{
	number_label.set_alignment(1.0, pos%2 ? 0.0 : 1.0 );
}

gint
MeterStrip::meter_ticks1_expose (GdkEventExpose *ev)
{
	return meter_expose_ticks(ev, _types, &meter_ticks1_area);
}

gint
MeterStrip::meter_ticks2_expose (GdkEventExpose *ev)
{
	return meter_expose_ticks(ev, _types, &meter_ticks2_area);
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
	if (_session->config.get_show_id_on_meterbridge()) {
		numbx.show();
	} else {
		numbx.hide();
	}
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
	else if (p == "show-id-on-meterbridge") {
		update_name_box();
	}
}


bool
MeterStrip::level_meter_button_press (GdkEventButton* ev)
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
	add_level_meter_item (items, group, _("Peak"), MeterPeak);
	add_level_meter_item (items, group, _("RMS + Peak"), MeterKrms);

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Change all in Group to Peak"), sigc::bind (SetMeterTypeMulti, -1, _route->route_group(), MeterPeak)));
	items.push_back (MenuElem (_("Change all in Group to RMS + Peak"), sigc::bind (SetMeterTypeMulti, -1, _route->route_group(), MeterKrms)));
	items.push_back (MenuElem (_("Change all to Peak"), sigc::bind (SetMeterTypeMulti, 0, _route->route_group(), MeterPeak)));
	items.push_back (MenuElem (_("Change all to RMS + Peak"), sigc::bind (SetMeterTypeMulti, 0, _route->route_group(), MeterKrms)));
	items.push_back (MenuElem (_("Change same track-type to Peak"), sigc::bind (SetMeterTypeMulti, _strip_type, _route->route_group(), MeterPeak)));
	items.push_back (MenuElem (_("Change same track-type to RMS + Peak"), sigc::bind (SetMeterTypeMulti, _strip_type, _route->route_group(), MeterKrms)));

	m->popup (ev->button, ev->time);
	_suspend_menu_callbacks = false;
}

void
MeterStrip::add_level_meter_item (Menu_Helpers::MenuList& items, RadioMenuItem::Group& group, string const & name, MeterType type)
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
	level_meter->set_type (type);
}

void
MeterStrip::meter_type_changed (MeterType type)
{
	_route->set_meter_type(type);
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
