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

*/

#include <limits.h>

#include <ardour/io.h>
#include <ardour/route.h>
#include <ardour/route_group.h>
#include <ardour/session.h>
#include <ardour/session_route.h>
#include <ardour/dB.h>

#include <gtkmm2ext/utils.h>
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
#include "keyboard.h"
#include "public_editor.h"

#include <ardour/session.h>
#include <ardour/route.h>
#include <ardour/meter.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace sigc;
using namespace std;

sigc::signal<void> GainMeter::ResetAllPeakDisplays;
sigc::signal<void,RouteGroup*> GainMeter::ResetGroupPeakDisplays;
Glib::RefPtr<Gdk::Pixbuf> GainMeter::slider;
Glib::RefPtr<Gdk::Pixbuf> GainMeter::rail;
map<string,Glib::RefPtr<Gdk::Pixmap> > GainMeter::metric_pixmaps;

int
GainMeter::setup_slider_pix ()
{
	slider = ::get_icon ("fader_belt");
	return 0;
}

GainMeter::GainMeter (boost::shared_ptr<IO> io, Session& s)
	: _io (io),
	  _session (s),
	  gain_slider (0),
	  // 0.781787 is the value needed for gain to be set to 0.
	  gain_adjustment (0.781787, 0.0, 1.0, 0.01, 0.1),
	  gain_automation_style_button (""),
	  gain_automation_state_button ("")
	
{
	if (slider == 0) {
		setup_slider_pix ();
	}

	ignore_toggle = false;
	meter_menu = 0;
	next_release_selects = false;

	gain_slider = manage (new VSliderController (slider,
						     &gain_adjustment,
						     _io->gain_control(),
						     false));

	gain_slider->signal_button_press_event().connect (mem_fun(*this, &GainMeter::start_gain_touch));
	gain_slider->signal_button_release_event().connect (mem_fun(*this, &GainMeter::end_gain_touch));
	gain_slider->set_name ("GainFader");

	gain_display.set_name ("MixerStripGainDisplay");
	gain_display.set_has_frame (false);
	set_size_request_to_display_given_text (gain_display, "-80.g", 2, 6); /* note the descender */
	gain_display.signal_activate().connect (mem_fun (*this, &GainMeter::gain_activated));
	gain_display.signal_focus_in_event().connect (mem_fun (*this, &GainMeter::gain_focused), false);
	gain_display.signal_focus_out_event().connect (mem_fun (*this, &GainMeter::gain_focused), false);

	gain_display_box.set_homogeneous (true);
	gain_display_box.set_spacing (2);
	gain_display_box.pack_start (gain_display, true, true);

	peak_display.set_name ("MixerStripPeakDisplay");
//	peak_display.set_has_frame (false);
//	peak_display.set_editable (false);
	set_size_request_to_display_given_text  (peak_display, "-80.g", 2, 6); /* note the descender */
	max_peak = minus_infinity();
	peak_display.set_label (_("-inf"));
	peak_display.unset_flags (Gtk::CAN_FOCUS);

	meter_metric_area.set_name ("MeterMetricsStrip");
	set_size_request_to_display_given_text (meter_metric_area, "-50", 0, 0);

	meter_packer.set_spacing (2);

	gain_automation_style_button.set_name ("MixerAutomationModeButton");
	gain_automation_state_button.set_name ("MixerAutomationPlaybackButton");

	ARDOUR_UI::instance()->tooltips().set_tip (gain_automation_state_button, _("Fader automation mode"));
	ARDOUR_UI::instance()->tooltips().set_tip (gain_automation_style_button, _("Fader automation type"));

	gain_automation_style_button.unset_flags (Gtk::CAN_FOCUS);
	gain_automation_state_button.unset_flags (Gtk::CAN_FOCUS);

	gain_automation_state_button.set_size_request(15, 15);
	gain_automation_style_button.set_size_request(15, 15);

	HBox* fader_centering_box = manage (new HBox);
	fader_centering_box->pack_start (*gain_slider, true, false);

	fader_vbox = manage (new Gtk::VBox());
	fader_vbox->set_spacing (0);
	fader_vbox->pack_start (*fader_centering_box, false, false, 0);

	hbox.set_spacing (2);

	if (_io->default_type() == ARDOUR::DataType::AUDIO) {
		hbox.pack_start (*fader_vbox, true, true);
	}
	
	set_width (Narrow);

	Route* r;

	if ((r = dynamic_cast<Route*> (_io.get())) != 0) {

	        /* 
		   if we have a route (ie. we're not the click), 
		   pack some route-dependent stuff.
		*/

	        gain_display_box.pack_end (peak_display, true, true);

		hbox.pack_end (meter_packer, true, true);

		using namespace Menu_Helpers;
	
		gain_astate_menu.items().push_back (MenuElem (_("Manual"), 
						      bind (mem_fun (*_io, &IO::set_gain_automation_state), (AutoState) Off)));
		gain_astate_menu.items().push_back (MenuElem (_("Play"),
						      bind (mem_fun (*_io, &IO::set_gain_automation_state), (AutoState) Play)));
		gain_astate_menu.items().push_back (MenuElem (_("Write"),
						      bind (mem_fun (*_io, &IO::set_gain_automation_state), (AutoState) Write)));
		gain_astate_menu.items().push_back (MenuElem (_("Touch"),
						      bind (mem_fun (*_io, &IO::set_gain_automation_state), (AutoState) Touch)));
	
		gain_astyle_menu.items().push_back (MenuElem (_("Trim")));
		gain_astyle_menu.items().push_back (MenuElem (_("Abs")));
	
		gain_astate_menu.set_name ("ArdourContextMenu");
		gain_astyle_menu.set_name ("ArdourContextMenu");

		gain_automation_style_button.signal_button_press_event().connect (mem_fun(*this, &GainMeter::gain_automation_style_button_event), false);
		gain_automation_state_button.signal_button_press_event().connect (mem_fun(*this, &GainMeter::gain_automation_state_button_event), false);
		
		r->gain_automation_curve().automation_state_changed.connect (mem_fun(*this, &GainMeter::gain_automation_state_changed));
		r->gain_automation_curve().automation_style_changed.connect (mem_fun(*this, &GainMeter::gain_automation_style_changed));
		fader_vbox->pack_start (gain_automation_state_button, false, false, 0);

		gain_automation_state_changed ();
	}

	set_spacing (2);

	pack_start (gain_display_box, Gtk::PACK_SHRINK);
	pack_start (hbox, Gtk::PACK_SHRINK);

	_io->gain_changed.connect (mem_fun(*this, &GainMeter::gain_changed));

	meter_metric_area.signal_expose_event().connect (mem_fun(*this, &GainMeter::meter_metrics_expose));
	gain_adjustment.signal_value_changed().connect (mem_fun(*this, &GainMeter::gain_adjusted));
	peak_display.signal_button_release_event().connect (mem_fun(*this, &GainMeter::peak_button_release), false);
	gain_display.signal_key_press_event().connect (mem_fun(*this, &GainMeter::gain_key_press), false);

	Config->ParameterChanged.connect (mem_fun (*this, &GainMeter::parameter_changed));

	gain_changed (0);
	show_gain ();

	update_gain_sensitive ();
	
	ResetAllPeakDisplays.connect (mem_fun(*this, &GainMeter::reset_peak_display));
	ResetGroupPeakDisplays.connect (mem_fun(*this, &GainMeter::reset_group_peak_display));
}

void
GainMeter::set_width (Width w)
{
	switch (w) {
	case Wide:
		peak_display.show();
		break;
	case Narrow:
		peak_display.hide();
		break;
	}

	_width = w;
	setup_meters ();
}

Glib::RefPtr<Gdk::Pixmap>
GainMeter::render_metrics (Gtk::Widget& w)
{
	Glib::RefPtr<Gdk::Window> win (w.get_window());
	Glib::RefPtr<Gdk::GC> fg_gc (w.get_style()->get_fg_gc (Gtk::STATE_NORMAL));
	Glib::RefPtr<Gdk::GC> bg_gc (w.get_style()->get_bg_gc (Gtk::STATE_NORMAL));
	gint width, height;
	int  db_points[] = { -50, -40, -20, -30, -10, -3, 0, 4 };
	char buf[32];

	win->get_size (width, height);
	
	Glib::RefPtr<Gdk::Pixmap> pixmap = Gdk::Pixmap::create (win, width, height);

	metric_pixmaps[w.get_name()] = pixmap;

	pixmap->draw_rectangle (bg_gc, true, 0, 0, width, height);

	Glib::RefPtr<Pango::Layout> layout = w.create_pango_layout("");

	for (uint32_t i = 0; i < sizeof (db_points)/sizeof (db_points[0]); ++i) {

		float fraction = log_meter (db_points[i]);
		gint pos = height - (gint) floor (height * fraction);

		snprintf (buf, sizeof (buf), "%d", abs (db_points[i]));

		layout->set_text (buf);

		/* we want logical extents, not ink extents here */

		int width, height;
		layout->get_pixel_size (width, height);

		pixmap->draw_line (fg_gc, 0, pos, 4, pos);
		pixmap->draw_layout (fg_gc, 6, pos - (height/2), layout);
	}

	return pixmap;
}

gint
GainMeter::meter_metrics_expose (GdkEventExpose *ev)
{
	Glib::RefPtr<Gdk::Window> win (meter_metric_area.get_window());
	Glib::RefPtr<Gdk::GC> fg_gc (meter_metric_area.get_style()->get_fg_gc (Gtk::STATE_NORMAL));
	Glib::RefPtr<Gdk::GC> bg_gc (meter_metric_area.get_style()->get_bg_gc (Gtk::STATE_NORMAL));
	GdkRectangle base_rect;
	GdkRectangle draw_rect;
	gint width, height;

	win->get_size (width, height);
	
	base_rect.width = width;
	base_rect.height = height;
	base_rect.x = 0;
	base_rect.y = 0;

	Glib::RefPtr<Gdk::Pixmap> pixmap;
	std::map<string,Glib::RefPtr<Gdk::Pixmap> >::iterator i = metric_pixmaps.find (meter_metric_area.get_name());

	if (i == metric_pixmaps.end()) {
		pixmap = render_metrics (meter_metric_area);
	} else {
		pixmap = i->second;
	}

	gdk_rectangle_intersect (&ev->area, &base_rect, &draw_rect);
	win->draw_rectangle (bg_gc, true, draw_rect.x, draw_rect.y, draw_rect.width, draw_rect.height);
	win->draw_drawable (bg_gc, pixmap, draw_rect.x, draw_rect.y, draw_rect.x, draw_rect.y, draw_rect.width, draw_rect.height);
	
	return true;
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
	float peak, mpeak;
	char buf[32];
	
	for (n = 0, i = meters.begin(); i != meters.end(); ++i, ++n) {
		if ((*i).packed) {
			peak = _io->peak_meter().peak_power (n);

			(*i).meter->set (log_meter (peak));

			mpeak = _io->peak_meter().max_peak_power(n);
			
			if (mpeak > max_peak) {
				max_peak = mpeak;
				/* set peak display */
				if (max_peak <= -200.0f) {
					peak_display.set_label (_("-inf"));
				} else {
					snprintf (buf, sizeof(buf), "%.1f", max_peak);
					peak_display.set_label (buf);
				}

				if (max_peak >= 0.0f) {
					peak_display.set_name ("MixerStripPeakDisplayPeak");
				}
			}
		}
	}
}

void
GainMeter::parameter_changed(const char* parameter_name)
{
#define PARAM_IS(x) (!strcmp (parameter_name, (x)))

	ENSURE_GUI_THREAD (bind (mem_fun(*this, &GainMeter::parameter_changed), parameter_name));

	if (PARAM_IS ("meter-hold")) {
	
		vector<MeterInfo>::iterator i;
		uint32_t n;
		
		for (n = 0, i = meters.begin(); i != meters.end(); ++i, ++n) {
			
			(*i).meter->set_hold_count ((uint32_t) floor(Config->get_meter_hold()));
		}
	}

#undef PARAM_IS
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
	uint32_t nmeters = _io->n_outputs().n_audio();
	guint16 width;

	hide_all_meters ();

	Route* r;

	if ((r = dynamic_cast<Route*> (_io.get())) != 0) {

		switch (r->meter_point()) {
		case MeterPreFader:
		case MeterInput:
			nmeters = r->n_inputs().n_audio();
			break;
		case MeterPostFader:
			nmeters = r->n_outputs().n_audio();
			break;
		}

	} else {

		nmeters = _io->n_outputs().n_audio();

	}

	if (nmeters == 0) {
		return;
	}

	if (nmeters <= 2) {
		width = regular_meter_width;
	} else {
		width = thin_meter_width;
	}

	while (meters.size() < nmeters) {
		meters.push_back (MeterInfo());
	}

	/* pack them backwards */

	if (_width == Wide) {
	        meter_packer.pack_end (meter_metric_area, false, false);
		meter_metric_area.show_all ();
	}

	for (int32_t n = nmeters-1; nmeters && n >= 0 ; --n) {
		if (meters[n].width != width) {
			delete meters[n].meter;
			meters[n].meter = new FastMeter ((uint32_t) floor (Config->get_meter_hold()), width, FastMeter::Vertical);
			meters[n].width = width;

			meters[n].meter->add_events (Gdk::BUTTON_RELEASE_MASK);
			meters[n].meter->signal_button_release_event().connect (bind (mem_fun(*this, &GainMeter::meter_button_release), n));
		}

		meter_packer.pack_end (*meters[n].meter, false, false);
		meters[n].meter->show_all ();
		meters[n].packed = true;
	}
}	

int
GainMeter::get_gm_width ()
{
	Gtk::Requisition sz = hbox.size_request ();
	return sz.width;
}

bool
GainMeter::gain_key_press (GdkEventKey* ev)
{
	if (key_is_legal_for_numeric_entry (ev->keyval)) {
		/* drop through to normal handling */
		return false;
	}
	/* illegal key for gain entry */
	return true;
}

bool
GainMeter::peak_button_release (GdkEventButton* ev)
{
	/* reset peak label */

	if (ev->button == 1 && Keyboard::modifier_state_equals (ev->state, Keyboard::Control|Keyboard::Shift)) {
		ResetAllPeakDisplays ();
	} else if (ev->button == 1 && Keyboard::modifier_state_equals (ev->state, Keyboard::Control)) {
		Route* r;
		if ((r = dynamic_cast<Route*> (_io.get())) != 0) {
			ResetGroupPeakDisplays (r->mix_group());
		}
	} else {
		reset_peak_display ();
	}

	return true;
}

void
GainMeter::reset_peak_display ()
{
	Route * r;
	if ((r = dynamic_cast<Route*> (_io.get())) != 0) {
		r->peak_meter().reset_max();
	}

	max_peak = -INFINITY;
	peak_display.set_label (_("-Inf"));
	peak_display.set_name ("MixerStripPeakDisplay");
}

void
GainMeter::reset_group_peak_display (RouteGroup* group)
{
	Route* r;
	if ((r = dynamic_cast<Route*> (_io.get())) != 0) {
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
		peak_display.set_label (_("-inf"));
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

bool
GainMeter::gain_focused (GdkEventFocus* ev)
{
	if (ev->in) {
		gain_display.select_region (0, -1);
	} else {
		gain_display.select_region (0, 0);
	}
	return false;
}

void
GainMeter::gain_activated ()
{
	float f;

	if (sscanf (gain_display.get_text().c_str(), "%f", &f) == 1) {

		/* clamp to displayable values */

		f = min (f, 6.0f);

		_io->set_gain (dB_to_coefficient(f), this);

		if (gain_display.has_focus()) {
			PublicEditor::instance().reset_focus();
		}
	}
}

void
GainMeter::show_gain ()
{
	char buf[32];

	float v = gain_adjustment.get_value();
	
	if (v == 0.0) {
		strcpy (buf, _("-inf"));
	} else {
		snprintf (buf, 32, "%.1f", coefficient_to_dB (slider_position_to_gain (v)));
	}
	
	gain_display.set_text (buf);
}

void
GainMeter::gain_adjusted ()
{
	if (!ignore_toggle) {
		_io->set_gain (slider_position_to_gain (gain_adjustment.get_value()), this);
	}
	show_gain ();
}

void
GainMeter::effective_gain_display ()
{
	gfloat value = gain_to_slider_position (_io->effective_gain());
	
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

void
GainMeter::set_meter_strip_name (const char * name)
{
	meter_metric_area.set_name (name);
}

void
GainMeter::set_fader_name (const char * name)
{
	gain_slider->set_name (name);
}

void
GainMeter::update_gain_sensitive ()
{
	static_cast<Gtkmm2ext::SliderController*>(gain_slider)->set_sensitive (!(_io->gain_automation_state() & Play));
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

	if ((_route = dynamic_cast<Route*>(_io.get())) == 0) {
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

					_session.begin_reversible_command (_("meter point change"));
                                        Session::GlobalMeteringStateCommand *cmd = new Session::GlobalMeteringStateCommand (_session, this);
					_session.foreach_route (this, &GainMeter::set_meter_point, next_meter_point (_route->meter_point()));
                                        cmd->mark();
					_session.add_command (cmd);
					_session.commit_reversible_command ();
					
					
				} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::Control)) {

					/* ctrl-click: solo mix group.
					   ctrl-button2 is MIDI learn.
					*/
					
					if (ev->button == 1) {
						_session.begin_reversible_command (_("meter point change"));
						Session::GlobalMeteringStateCommand *cmd = new Session::GlobalMeteringStateCommand (_session, this);
						set_mix_group_meter_point (*_route, next_meter_point (_route->meter_point()));
						cmd->mark();
						_session.add_command (cmd);
						_session.commit_reversible_command ();
					}
					
				} else {
					
					/* click: change just this route */

					// XXX no undo yet
					
					_route->set_meter_point (next_meter_point (_route->meter_point()), this);
				}
			}
		}
	}

	return true;

}

gint
GainMeter::meter_release(GdkEventButton* ev)
{

	if(!ignore_toggle){
		if (wait_for_release){
			wait_for_release = false;
			set_meter_point (*(dynamic_cast<Route*>(_io.get())), old_meter_point);
		}
	}
	return true;
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
GainMeter::meter_point_clicked ()
{
	Route* r;

	if ((r = dynamic_cast<Route*> (_io.get())) != 0) {

	}
}

gint
GainMeter::start_gain_touch (GdkEventButton* ev)
{
	_io->start_gain_touch ();
	return FALSE;
}

gint
GainMeter::end_gain_touch (GdkEventButton* ev)
{
	_io->end_gain_touch ();
	return FALSE;
}

gint
GainMeter::gain_automation_state_button_event (GdkEventButton *ev)
{
	if (ev->type == GDK_BUTTON_RELEASE) {
		return TRUE;
	}
	
	switch (ev->button) {
		case 1:
			gain_astate_menu.popup (1, ev->time);
			break;
		default:
			break;
	}

	return TRUE;
}

gint
GainMeter::gain_automation_style_button_event (GdkEventButton *ev)
{
	if (ev->type == GDK_BUTTON_RELEASE) {
		return TRUE;
	}

	switch (ev->button) {
	case 1:
		gain_astyle_menu.popup (1, ev->time);
		break;
	default:
		break;
	}
	return TRUE;
}

string
GainMeter::astate_string (AutoState state)
{
	return _astate_string (state, false);
}

string
GainMeter::short_astate_string (AutoState state)
{
	return _astate_string (state, true);
}

string
GainMeter::_astate_string (AutoState state, bool shrt)
{
	string sstr;

	switch (state) {
	case Off:
		sstr = (shrt ? "M" : _("M"));
		break;
	case Play:
		sstr = (shrt ? "P" : _("P"));
		break;
	case Touch:
		sstr = (shrt ? "T" : _("T"));
		break;
	case Write:
		sstr = (shrt ? "W" : _("W"));
		break;
	}

	return sstr;
}

string
GainMeter::astyle_string (AutoStyle style)
{
	return _astyle_string (style, false);
}

string
GainMeter::short_astyle_string (AutoStyle style)
{
	return _astyle_string (style, true);
}

string
GainMeter::_astyle_string (AutoStyle style, bool shrt)
{
	if (style & Trim) {
		return _("Trim");
	} else {
	        /* XXX it might different in different languages */

		return (shrt ? _("Abs") : _("Abs"));
	}
}

void
GainMeter::gain_automation_style_changed ()
{
  // Route* _route = dynamic_cast<Route*>(&_io);
	switch (_width) {
	case Wide:
	        gain_automation_style_button.set_label (astyle_string(_io->gain_automation_curve().automation_style()));
		break;
	case Narrow:
		gain_automation_style_button.set_label  (short_astyle_string(_io->gain_automation_curve().automation_style()));
		break;
	}
}

void
GainMeter::gain_automation_state_changed ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &GainMeter::gain_automation_state_changed));
	//Route* _route = dynamic_cast<Route*>(&_io);
	
	bool x;

	switch (_width) {
	case Wide:
		gain_automation_state_button.set_label (astate_string(_io->gain_automation_curve().automation_state()));
		break;
	case Narrow:
		gain_automation_state_button.set_label (short_astate_string(_io->gain_automation_curve().automation_state()));
		break;
	}

	x = (_io->gain_automation_state() != Off);
	
	if (gain_automation_state_button.get_active() != x) {
		ignore_toggle = true;
		gain_automation_state_button.set_active (x);
		ignore_toggle = false;
	}

	update_gain_sensitive ();
	
	/* start watching automation so that things move */
	
	gain_watching.disconnect();

	if (x) {
		gain_watching = ARDOUR_UI::RapidScreenUpdate.connect (mem_fun (*this, &GainMeter::effective_gain_display));
	}
}
