/*
  Copyright (C) 2002 Paul Davis

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

  $Id$
*/

#include <limits.h>

#include <ardour/io.h>
#include <ardour/route.h>
#include <ardour/route_group.h>
#include <ardour/session.h>
#include <ardour/session_route.h>
#include <ardour/dB.h>

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/pix.h>
#include <gtkmm2ext/fastmeter.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/barcontroller.h>
#include <midi++/manager.h>
#include <pbd/fastlog.h>

#include "ardour_ui.h"
#include "gain_meter.h"
#include "utils.h"
#include "logmeter.h"
#include "gui_thread.h"

#include <ardour/session.h>
#include <ardour/route.h>

#include "i18n.h"
#include "misc_xpms"

using namespace ARDOUR;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace sigc;

Signal0<void> GainMeter::ResetAllPeakDisplays;
Signal1<void,RouteGroup*> GainMeter::ResetGroupPeakDisplays;
Pix* GainMeter::slider_pix = 0;

int
GainMeter::setup_slider_pix ()
{
	vector<const char **> xpms;

	xpms.push_back (vslider_rail_xpm);
	xpms.push_back (vslider_slider_xpm);

	if ((slider_pix = get_pix ("sliders", xpms, false)) == 0) {
		error << _("Cannot create slider pixmaps") << endmsg;
		return -1;
	}

	slider_pix->ref ();
	return 0;
}

GainMeter::GainMeter (IO& io, Session& s)
	: _io (io),
	  _session (s),
	  gain_slider (0),
	  // 0.781787 is the value needed for gain to be set to 0.
	  gain_adjustment (0.781787, 0.0, 1.0, 0.01, 0.1),
	  gain_display (&gain_adjustment, "MixerStripGainDisplay"),
	  gain_unit_label (_("dbFS")),
	  meter_point_label (_("pre")),
	  top_table (1, 2)
	
{
	if (slider_pix == 0) {
		setup_slider_pix ();
	}

	ignore_toggle = false;
	meter_menu = 0;
	
	gain_slider = manage (new VSliderController (slider_pix,
						     &gain_adjustment,
						     & _io.midi_gain_control(),
						     false));

	gain_slider->signal_button_press_event.connect (mem_fun(*this, &GainMeter::start_gain_touch));
	gain_slider->signal_button_release_event.connect (mem_fun(*this, &GainMeter::end_gain_touch));
	gain_slider->set_name ("MixerGainMeter");

	if (_session.midi_port()) {
		_io.set_midi_to_gain_function (slider_position_to_gain);
		_io.set_gain_to_midi_function (gain_to_slider_position);
	}

	gain_display.set_print_func (_gain_printer, this);
	set_size_request_to_display_given_text (gain_display, "-86.0", 2, 2);

	gain_unit_button.add (gain_unit_label);
	gain_unit_button.set_name ("MixerStripGainUnitButton");
	gain_unit_label.set_name ("MixerStripGainUnitButton");

	top_table.set_col_spacings (2);
	top_table.set_homogeneous (true);
	top_table.attach (gain_unit_button, 0, 1, 0, 1);

	Route* r;

	if ((r = dynamic_cast<Route*> (&_io)) != 0) {
		r->meter_change.connect (mem_fun(*this, &GainMeter::meter_changed));
		meter_point_button.add (meter_point_label);
		meter_point_button.set_name ("MixerStripMeterPreButton");
		meter_point_label.set_name ("MixerStripMeterPreButton");
		
		switch (r->meter_point()) {
		case MeterInput:
			meter_point_label.set_text (_("input"));
			break;
			
		case MeterPreFader:
			meter_point_label.set_text (_("pre"));
			break;
			
		case MeterPostFader:
			meter_point_label.set_text (_("post"));
			break;
		}
		
		/* TRANSLATORS: this string should be longest of the strings
		   used to describe meter points. In english, its "input".
		*/
		
		set_size_request_to_display_given_text (meter_point_button, _("tupni"), 2, 2);

		meter_point_button.signal_button_press_event.connect (mem_fun(*this, &GainMeter::meter_press));
		meter_point_button.signal_button_release_event.connect (mem_fun(*this, &GainMeter::meter_release));

		top_table.attach (meter_point_button, 1, 2, 0, 1);
	}

	gain_display_box.set_spacing (2);
	gain_display_frame.set_shadow_type (Gtk::SHADOW_IN);
	gain_display_frame.set_name ("BaseFrame");
	gain_display_frame.add (gain_display);
	gain_display_box.pack_start (gain_display_frame, false, false);

	peak_display.set_name ("MixerStripPeakDisplay");
	set_size_request_to_display_given_text (peak_display, "-86.0", 2, 2);
	peak_display.add (peak_display_label);
	peak_display_frame.set_shadow_type (Gtk::SHADOW_IN);
	peak_display_frame.set_name ("BaseFrame");
	peak_display_frame.add (peak_display);
	max_peak = minus_infinity();
	peak_display_label.set_text (_("-inf"));
	
	gain_display_box.pack_start (peak_display_frame, false, false);


	meter_metric_area.set_size_request (18, -1);
	meter_metric_area.set_name ("MeterMetricsStrip");

	meter_packer.show ();
	gain_slider->show_all ();

	meter_packer.set_spacing (2);
	fader_box.set_spacing (2);

	fader_box.pack_start (*gain_slider, false, false);

	hbox.set_spacing (4);
	hbox.pack_start (fader_box, false, false);
	hbox.pack_start (meter_packer, false, false);

	set_spacing (4);
	pack_start (top_table, false, false);
	pack_start (gain_display_box, false, false);
	pack_start (hbox, false, false);

	show_all ();

	_io.gain_changed.connect (mem_fun(*this, &GainMeter::gain_changed));

	meter_metric_area.signal_expose_event.connect (mem_fun(*this, &GainMeter::meter_metrics_expose));
	gain_adjustment.value_changed.connect (mem_fun(*this, &GainMeter::gain_adjusted));
	peak_display.signal_button_release_event.connect (mem_fun(*this, &GainMeter::peak_button_release));

	_session.MeterHoldChanged.connect (mem_fun(*this, &GainMeter::meter_hold_changed));
	
	gain_changed (0);
	update_gain_sensitive ();

	ResetAllPeakDisplays.connect (mem_fun(*this, &GainMeter::reset_peak_display));
	ResetGroupPeakDisplays.connect (mem_fun(*this, &GainMeter::reset_group_peak_display));
}

void
GainMeter::set_width (Width w)
{
	switch (w) {
	case Wide:
		peak_display_frame.show_all();
		break;
	case Narrow:
		peak_display_frame.hide_all();
		break;
	}

	_width = w;
	setup_meters ();
}

gint
GainMeter::meter_metrics_expose (GdkEventExpose *ev)
{
	/* XXX optimize this so that it doesn't do it all everytime */

	double fraction;

	Gdk_Window win (meter_metric_area.get_window());
	Gdk_GC fg_gc (meter_metric_area.get_style()->get_fg_gc (Gtk::STATE_NORMAL));
	Gdk_GC bg_gc (meter_metric_area.get_style()->get_bg_gc (Gtk::STATE_NORMAL));
	Gdk_Font font (meter_metric_area.get_style()->get_font());
	gint x, y, width, height, depth;
	gint pos;
	int  db_points[] = { -50, -10, -3, 0, 6 };
	uint32_t i;
	char buf[32];
	GdkRectangle base_rect;
	GdkRectangle draw_rect;

	win.get_geometry (x, y, width, height, depth);
	
	base_rect.width = width;
	base_rect.height = height;
	base_rect.x = 0;
	base_rect.y = 0;

	gdk_rectangle_intersect (&ev->area, &base_rect, &draw_rect);
	win.draw_rectangle (bg_gc, true, draw_rect.x, draw_rect.y, draw_rect.width, draw_rect.height);

	for (i = 0; i < sizeof (db_points)/sizeof (db_points[0]); ++i) {
		fraction = log_meter (db_points[i]);
		pos = height - (gint) floor (height * fraction);

		snprintf (buf, sizeof (buf), "%d", db_points[i]);

		gint twidth;
		gint lbearing;
		gint rbearing;
		gint ascent;
		gint descent;

		gdk_string_extents (font,
				    buf,
				    &lbearing,
				    &rbearing,
				    &twidth,
				    &ascent,
				    &descent);

		win.draw_text (font, fg_gc, width - twidth, pos + ascent, buf, strlen (buf));
	}

	return TRUE;
}

GainMeter::~GainMeter ()
{

	if (meter_menu) {
		delete meter_menu;
	}

	for (vector<MeterInfo>::iterator i = meters.begin(); i != meters.end(); i++) {
		if ((*i).meter) {
			delete (*i).meter;
		}
	}
}

void
GainMeter::update_meters ()
{
	vector<MeterInfo>::iterator i;
	uint32_t n;
	float peak;
	char buf[32];

	
	for (n = 0, i = meters.begin(); i != meters.end(); ++i, ++n) {
                if ((*i).packed) {
                        peak = _io.peak_input_power (n);

			if (_session.meter_falloff() == 0.0f || peak > (*i).meter->get_user_level()) {
				(*i).meter->set (log_meter (peak), peak);
			}

                        if (peak > max_peak) {
                                max_peak = peak;
                                /* set peak display */
                                snprintf (buf, sizeof(buf), "%.1f", max_peak);
                                peak_display_label.set_text (buf);

                                if (max_peak >= 0.0f) {
                                        peak_display.set_name ("MixerStripPeakDisplayPeak");
                                }
                        }
                }
        }

}

void
GainMeter::update_meters_falloff ()
{
	vector<MeterInfo>::iterator i;
	uint32_t n;
	float dbpeak;
	
	for (n = 0, i = meters.begin(); i != meters.end(); ++i, ++n) {
		if ((*i).packed) {
			// just do falloff
			//peak = (*i).meter->get_level() * _falloff_rate;
			dbpeak = (*i).meter->get_user_level() - _session.meter_falloff();

			dbpeak = std::max(dbpeak, -200.0f);
			
			// cerr << "tmplevel: " << tmplevel << endl;
			(*i).meter->set (log_meter (dbpeak), dbpeak);
		}
	}
	
}


void
GainMeter::meter_hold_changed()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &GainMeter::meter_hold_changed));
	
	vector<MeterInfo>::iterator i;
	uint32_t n;
	
	for (n = 0, i = meters.begin(); i != meters.end(); ++i, ++n) {
		
		(*i).meter->set_hold_count ((uint32_t) floor(_session.meter_hold()));
	}
}

void
GainMeter::hide_all_meters ()
{
	bool remove_metric_area = false;

	for (vector<MeterInfo>::iterator i = meters.begin(); i != meters.end(); ++i) {
		if ((*i).packed) {
			remove_metric_area = true;
			meter_packer.remove (*((*i).meter));
			(*i).packed = false;
		}
	}

	if (remove_metric_area) {
		if (meter_metric_area.get_parent()) {
			meter_packer.remove (meter_metric_area);
		}
	}
}

void
GainMeter::setup_meters ()
{
	uint32_t nmeters = _io.n_outputs();
	guint16 width;

	hide_all_meters ();

	Route* r;

	if ((r = dynamic_cast<Route*> (&_io)) != 0) {

		switch (r->meter_point()) {
		case MeterPreFader:
		case MeterInput:
			nmeters = r->n_inputs();
			break;
		case MeterPostFader:
			nmeters = r->n_outputs();
			break;
		}

	} else {

		nmeters = _io.n_outputs();

	}

	if (nmeters == 0) {
		return;
	}

	if (_width == Wide) {
		meter_packer.pack_start (meter_metric_area, false, false);
		meter_metric_area.show_all ();
	}

	if (nmeters <= 2) {
		width = regular_meter_width;
	} else {
		width = thin_meter_width;
	}

	while (meters.size() < nmeters) {
		meters.push_back (MeterInfo());
	}

	for (uint32_t n = 0; n < nmeters; ++n) {
		if (meters[n].width != width) {
			delete meters[n].meter;
			meters[n].meter = new FastMeter ((uint32_t) floor (_session.meter_hold()), width, FastMeter::Vertical);
			meters[n].width = width;

			meters[n].meter->signal_add_events (Gdk::BUTTON_RELEASE_MASK);
			meters[n].meter->signal_button_release_event.connect
				(bind (mem_fun(*this, &GainMeter::meter_button_release), n));
			meters[n].meter->signal_button_release_event.connect_after (ptr_fun (do_not_propagate));
		}

		meter_packer.pack_start (*meters[n].meter, false, false);
		meters[n].meter->show_all ();
		meters[n].packed = true;
	}
}	

gint
GainMeter::peak_button_release (GdkEventButton* ev)
{
	/* reset peak label */

	if (ev->button == 1 && Keyboard::modifier_state_equals (ev->state, Keyboard::Control|Keyboard::Shift)) {
		ResetAllPeakDisplays ();
	} else if (ev->button == 1 && Keyboard::modifier_state_equals (ev->state, Keyboard::Control)) {
		Route* r;
		if ((r = dynamic_cast<Route*> (&_io)) != 0) {
			ResetGroupPeakDisplays (r->mix_group());
		}
	} else {
		reset_peak_display ();
	}
	return TRUE;
}

void
GainMeter::reset_peak_display ()
{
	max_peak = minus_infinity();
	peak_display_label.set_text (_("-inf"));
	peak_display.set_name ("MixerStripPeakDisplay");
}

void
GainMeter::reset_group_peak_display (RouteGroup* group)
{
	Route* r;
	if ((r = dynamic_cast<Route*> (&_io)) != 0) {
		if (group == r->mix_group()) {
			reset_peak_display ();
		}
	}
}

gint
GainMeter::meter_button_release (GdkEventButton* ev, uint32_t which)
{
	switch (ev->button) {
	case 1:
		meters[which].meter->clear();
		max_peak = minus_infinity();
		peak_display_label.set_text (_("-inf"));
		peak_display.set_name ("MixerStripPeakDisplay");
		break;

	case 3:
		// popup_meter_menu (ev);
		break;
	};

	return TRUE;
}

void
GainMeter::popup_meter_menu (GdkEventButton *ev)
{
	using namespace Menu_Helpers;

	if (meter_menu == 0) {
		meter_menu = new Gtk::Menu;
		MenuList& items = meter_menu->items();

		items.push_back (MenuElem ("-inf .. +0dBFS"));
		items.push_back (MenuElem ("-10dB .. +0dBFS"));
		items.push_back (MenuElem ("-4 .. +0dBFS"));
		items.push_back (SeparatorElem());
		items.push_back (MenuElem ("-inf .. -2dBFS"));
		items.push_back (MenuElem ("-10dB .. -2dBFS"));
		items.push_back (MenuElem ("-4 .. -2dBFS"));
	}

	meter_menu->popup (1, ev->time);
}

void
GainMeter::_gain_printer (char buf[32], Gtk::Adjustment& adj, void *arg)
{
	static_cast<GainMeter *>(arg)->gain_printer (buf, adj);
}

void
GainMeter::gain_printer (char buf[32], Gtk::Adjustment& adj)
{
	float v = adj.get_value();

	if (v == 0.0) {
		strcpy (buf, _("-inf"));
	} else {
		snprintf (buf, 32, "%.1f", coefficient_to_dB (slider_position_to_gain (v)));
	}
}

void
GainMeter::gain_adjusted ()
{
	if (!ignore_toggle) {
		_io.set_gain (slider_position_to_gain (gain_adjustment.get_value()), this);
	}
}

void
GainMeter::effective_gain_display ()
{
	gfloat value = gain_to_slider_position (_io.effective_gain());
	
	if (gain_adjustment.get_value() != value) {
		ignore_toggle = true;
		gain_adjustment.set_value (value);
		ignore_toggle = false;
	}
}

void
GainMeter::gain_changed (void *src)
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun(*this, &GainMeter::effective_gain_display));
}

gint
GainMeter::entry_focus_event (GdkEventFocus* ev)
{
	if (ev->in) {
		ARDOUR_UI::instance()->allow_focus (true);
	} else {
		ARDOUR_UI::instance()->allow_focus (false);
	}
	return TRUE;
}


void
GainMeter::set_meter_strip_name (string name)
{
	meter_metric_area.set_name (name);
}

void
GainMeter::set_fader_name (string name)
{
	gain_slider->set_name (name);
}

void
GainMeter::update_gain_sensitive ()
{
	static_cast<Gtkmm2ext::SliderController*>(gain_slider)->set_sensitive (!(_io.gain_automation_state() & Play));
}


static MeterPoint
next_meter_point (MeterPoint mp)
{
	switch (mp) {
	case MeterInput:
		return MeterPreFader;
		break;
		
	case MeterPreFader:
		return MeterPostFader;
		break;
		
	case MeterPostFader:
		return MeterInput;
		break;
	}
	/*NOTREACHED*/
	return MeterInput;
}

gint
GainMeter::meter_press(GdkEventButton* ev)
{
	Route* _route;

	wait_for_release = false;

	if ((_route = dynamic_cast<Route*>(&_io)) == 0) {
		return FALSE;
	}

	if (!ignore_toggle) {

		if (Keyboard::is_context_menu_event (ev)) {
			
			// no menu at this time.

		} else {

			if (ev->button == 2) {

				// ctrl-button2 click is the midi binding click
				// button2-click is "momentary"
				
				if (!Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::Control))) {
					wait_for_release = true;
					old_meter_point = _route->meter_point ();
				}
			}

			if (ev->button == 1 || ev->button == 2) {

				if (Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::Control|Keyboard::Shift))) {

					/* ctrl-shift-click applies change to all routes */

					_session.foreach_route (this, &GainMeter::set_meter_point, next_meter_point (_route->meter_point()));
					
				} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::Control)) {

					/* ctrl-click: solo mix group.
					   ctrl-button2 is MIDI learn.
					*/
					
					if (ev->button == 1) {
						set_mix_group_meter_point (*_route, next_meter_point (_route->meter_point()));
					}
					
				} else {
					
					/* click: solo this route */
					
					_route->set_meter_point (next_meter_point (_route->meter_point()), this);
				}
			}
		}
	}

	return stop_signal (meter_point_button, "button-press-event");

}

gint
GainMeter::meter_release(GdkEventButton* ev)
{
	if(!ignore_toggle){
		if (wait_for_release){
			wait_for_release = false;
			set_meter_point (*(dynamic_cast<Route*>(&_io)), old_meter_point);
			stop_signal (meter_point_button, "button-release-event");
		}
	}
	return TRUE;
}

void
GainMeter::set_meter_point (Route& route, MeterPoint mp)
{
	route.set_meter_point (mp, this);
}

void
GainMeter::set_mix_group_meter_point (Route& route, MeterPoint mp)
{
	RouteGroup* mix_group;

	if((mix_group = route.mix_group()) != 0){
		mix_group->apply (&Route::set_meter_point, mp, this);
	} else {
		route.set_meter_point (mp, this);
	}
}

void
GainMeter::meter_changed (void *src)
{
	Route* r;

	ENSURE_GUI_THREAD (bind (mem_fun(*this, &GainMeter::meter_changed), src));

	if ((r = dynamic_cast<Route*> (&_io)) != 0) {

		switch (r->meter_point()) {
		case MeterInput:
			meter_point_label.set_text (_("input"));
			break;
			
		case MeterPreFader:
			meter_point_label.set_text (_("pre"));
			break;
			
		case MeterPostFader:
			meter_point_label.set_text (_("post"));
			break;
		}

		setup_meters ();
	}
}

void
GainMeter::meter_point_clicked ()
{
	Route* r;

	if ((r = dynamic_cast<Route*> (&_io)) != 0) {

	}
}

gint
GainMeter::start_gain_touch (GdkEventButton* ev)
{
	_io.start_gain_touch ();
	return FALSE;
}

gint
GainMeter::end_gain_touch (GdkEventButton* ev)
{
	_io.end_gain_touch ();
	return FALSE;
}
