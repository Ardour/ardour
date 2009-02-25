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

#include "ardour/io.h"
#include "ardour/route.h"
#include "ardour/route_group.h"
#include "ardour/session.h"
#include "ardour/session_route.h"
#include "ardour/dB.h"

#include <gtkmm/style.h>
#include <gdkmm/color.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/fastmeter.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/barcontroller.h>
#include <gtkmm2ext/gtk_ui.h>
#include "midi++/manager.h"
#include "pbd/fastlog.h"
#include "pbd/stacktrace.h"

#include "ardour_ui.h"
#include "gain_meter.h"
#include "utils.h"
#include "logmeter.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "public_editor.h"

#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/meter.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace sigc;
using namespace std;

sigc::signal<void> GainMeterBase::ResetAllPeakDisplays;
sigc::signal<void,RouteGroup*> GainMeterBase::ResetGroupPeakDisplays;

map<string,Glib::RefPtr<Gdk::Pixmap> > GainMeter::metric_pixmaps;
Glib::RefPtr<Gdk::Pixbuf> GainMeter::slider;


void
GainMeter::setup_slider_pix ()
{
	if ((slider = ::get_icon ("fader_belt")) == 0) {
		throw failed_constructor();
	}
}

GainMeterBase::GainMeterBase (Session& s, 
			      const Glib::RefPtr<Gdk::Pixbuf>& pix,
			      bool horizontal)
	: _session (s),
	  // 0.781787 is the value needed for gain to be set to 0.
	  gain_adjustment (0.781787, 0.0, 1.0, 0.01, 0.1),
	  gain_automation_style_button (""),
	  gain_automation_state_button ("")
	
{
	using namespace Menu_Helpers;

	ignore_toggle = false;
	meter_menu = 0;
	next_release_selects = false;
	style_changed = true;
	_width = Wide;

	if (horizontal) {
		gain_slider = manage (new HSliderController (pix,
							     &gain_adjustment,
							     false));
	} else {
		gain_slider = manage (new VSliderController (pix,
							     &gain_adjustment,
							     false));
	}

	level_meter = new LevelMeter(_session);

	gain_slider->signal_button_press_event().connect (mem_fun(*this, &GainMeter::start_gain_touch));
	gain_slider->signal_button_release_event().connect (mem_fun(*this, &GainMeter::end_gain_touch));
	gain_slider->set_name ("GainFader");

	gain_display.set_name ("MixerStripGainDisplay");
	gain_display.set_has_frame (false);
	set_size_request_to_display_given_text (gain_display, "-80.g", 2, 6); /* note the descender */
	gain_display.signal_activate().connect (mem_fun (*this, &GainMeter::gain_activated));
	gain_display.signal_focus_in_event().connect (mem_fun (*this, &GainMeter::gain_focused), false);
	gain_display.signal_focus_out_event().connect (mem_fun (*this, &GainMeter::gain_focused), false);

	peak_display.set_name ("MixerStripPeakDisplay");
//	peak_display.set_has_frame (false);
//	peak_display.set_editable (false);
	set_size_request_to_display_given_text  (peak_display, "-80.g", 2, 6); /* note the descender */
	max_peak = minus_infinity();
	peak_display.set_label (_("-inf"));
	peak_display.unset_flags (Gtk::CAN_FOCUS);

	gain_automation_style_button.set_name ("MixerAutomationModeButton");
	gain_automation_state_button.set_name ("MixerAutomationPlaybackButton");

	ARDOUR_UI::instance()->tooltips().set_tip (gain_automation_state_button, _("Fader automation mode"));
	ARDOUR_UI::instance()->tooltips().set_tip (gain_automation_style_button, _("Fader automation type"));

	gain_automation_style_button.unset_flags (Gtk::CAN_FOCUS);
	gain_automation_state_button.unset_flags (Gtk::CAN_FOCUS);

	gain_automation_state_button.set_size_request(15, 15);
	gain_automation_style_button.set_size_request(15, 15);
  
	gain_astyle_menu.items().push_back (MenuElem (_("Trim")));
	gain_astyle_menu.items().push_back (MenuElem (_("Abs")));
	
	gain_astate_menu.set_name ("ArdourContextMenu");
	gain_astyle_menu.set_name ("ArdourContextMenu");

	gain_adjustment.signal_value_changed().connect (mem_fun(*this, &GainMeterBase::gain_adjusted));
	peak_display.signal_button_release_event().connect (mem_fun(*this, &GainMeterBase::peak_button_release), false);
	gain_display.signal_key_press_event().connect (mem_fun(*this, &GainMeterBase::gain_key_press), false);

	ResetAllPeakDisplays.connect (mem_fun(*this, &GainMeterBase::reset_peak_display));
	ResetGroupPeakDisplays.connect (mem_fun(*this, &GainMeterBase::reset_group_peak_display));

	UI::instance()->theme_changed.connect (mem_fun(*this, &GainMeterBase::on_theme_changed));
	ColorsChanged.connect (bind(mem_fun (*this, &GainMeterBase::color_handler), false));
	DPIReset.connect (bind(mem_fun (*this, &GainMeterBase::color_handler), true));
}

GainMeterBase::~GainMeterBase ()
{
	delete meter_menu;
	delete level_meter;
}

void
GainMeterBase::set_io (boost::shared_ptr<IO> io)
{
 	connections.clear ();
	
 	_io = io;
	
 	level_meter->set_io (_io);
 	gain_slider->set_controllable (_io->gain_control());

	boost::shared_ptr<Route> r;

	if ((r = boost::dynamic_pointer_cast<Route> (_io)) != 0) {

		if (!r->is_hidden()) {

			using namespace Menu_Helpers;
	
 			gain_astate_menu.items().clear ();

			gain_astate_menu.items().push_back (MenuElem (_("Manual"), 
								      bind (mem_fun (*_io, &IO::set_parameter_automation_state),
									    Evoral::Parameter(GainAutomation), (AutoState) Off)));
			gain_astate_menu.items().push_back (MenuElem (_("Play"),
								      bind (mem_fun (*_io, &IO::set_parameter_automation_state),
									    Evoral::Parameter(GainAutomation), (AutoState) Play)));
			gain_astate_menu.items().push_back (MenuElem (_("Write"),
								      bind (mem_fun (*_io, &IO::set_parameter_automation_state),
									    Evoral::Parameter(GainAutomation), (AutoState) Write)));
			gain_astate_menu.items().push_back (MenuElem (_("Touch"),
								      bind (mem_fun (*_io, &IO::set_parameter_automation_state),
									    Evoral::Parameter(GainAutomation), (AutoState) Touch)));
			
			connections.push_back (gain_automation_style_button.signal_button_press_event().connect (mem_fun(*this, &GainMeterBase::gain_automation_style_button_event), false));
			connections.push_back (gain_automation_state_button.signal_button_press_event().connect (mem_fun(*this, &GainMeterBase::gain_automation_state_button_event), false));
			
			connections.push_back (r->gain_control()->alist()->automation_state_changed.connect (mem_fun(*this, &GainMeter::gain_automation_state_changed)));
			connections.push_back (r->gain_control()->alist()->automation_style_changed.connect (mem_fun(*this, &GainMeter::gain_automation_style_changed)));

			gain_automation_state_changed ();
		}
	}

	//cerr << "Connect " << this << " to gain change for " << _io->name() << endl;

	connections.push_back (_io->gain_control()->Changed.connect (mem_fun(*this, &GainMeterBase::gain_changed)));

	gain_changed ();
	show_gain ();
	update_gain_sensitive ();
}

void
GainMeterBase::hide_all_meters ()
{
	level_meter->hide_meters();
}

void
GainMeter::hide_all_meters ()
{
	bool remove_metric_area = false;

	GainMeterBase::hide_all_meters ();

	if (remove_metric_area) {
		if (meter_metric_area.get_parent()) {
			level_meter->remove (meter_metric_area);
		}
	}
}

void
GainMeterBase::setup_meters (int len)
{
	level_meter->setup_meters(len, 5);
}

void 
GainMeter::setup_meters (int len)
{
	if (!meter_metric_area.get_parent()) {
		level_meter->pack_end (meter_metric_area, false, false);
		meter_metric_area.show_all ();
	}
	GainMeterBase::setup_meters (len);
}

bool
GainMeterBase::gain_key_press (GdkEventKey* ev)
{
	if (key_is_legal_for_numeric_entry (ev->keyval)) {
		/* drop through to normal handling */
		return false;
	}
	/* illegal key for gain entry */
	return true;
}

bool
GainMeterBase::peak_button_release (GdkEventButton* ev)
{
	/* reset peak label */

	if (ev->button == 1 && Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier|Keyboard::TertiaryModifier)) {
		ResetAllPeakDisplays ();
	} else if (ev->button == 1 && Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
		boost::shared_ptr<Route> r;

		if ((r = boost::dynamic_pointer_cast<Route> (_io)) != 0) {
			ResetGroupPeakDisplays (r->mix_group());
		}
	} else {
		reset_peak_display ();
	}

	return true;
}

void
GainMeterBase::reset_peak_display ()
{
	boost::shared_ptr<Route> r;

	if ((r = boost::dynamic_pointer_cast<Route> (_io)) != 0) {
		r->peak_meter().reset_max();
	}

	level_meter->clear_meters();
	max_peak = -INFINITY;
	peak_display.set_label (_("-Inf"));
	peak_display.set_name ("MixerStripPeakDisplay");
}

void
GainMeterBase::reset_group_peak_display (RouteGroup* group)
{
	boost::shared_ptr<Route> r;
	
	if ((r = boost::dynamic_pointer_cast<Route> (_io)) != 0) {
		if (group == r->mix_group()) {
			reset_peak_display ();
		}
	}
}

void
GainMeterBase::popup_meter_menu (GdkEventButton *ev)
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
GainMeterBase::gain_focused (GdkEventFocus* ev)
{
	if (ev->in) {
		gain_display.select_region (0, -1);
	} else {
		gain_display.select_region (0, 0);
	}
	return false;
}

void
GainMeterBase::gain_activated ()
{
	float f;

	if (sscanf (gain_display.get_text().c_str(), "%f", &f) == 1) {

		/* clamp to displayable values */

		f = min (f, 6.0f);

		_io->gain_control()->set_value (dB_to_coefficient(f));

		if (gain_display.has_focus()) {
			PublicEditor::instance().reset_focus();
		}
	}
}

void
GainMeterBase::show_gain ()
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
GainMeterBase::gain_adjusted ()
{
	//cerr << this << " for " << _io->name() << " GAIN ADJUSTED\n";
	if (!ignore_toggle) {
		//cerr << "Set GC\n";
		_io->gain_control()->set_value (slider_position_to_gain (gain_adjustment.get_value()));
		//cerr << "Set GC OUT\n";
	}
	show_gain ();
}

void
GainMeterBase::effective_gain_display ()
{
	gfloat value = gain_to_slider_position (_io->effective_gain());
	
	//cerr << this << " for " << _io->name() << " EGAIN = " << value
	//		<< " AGAIN = " << gain_adjustment.get_value () << endl;
	// stacktrace (cerr, 20);

	if (gain_adjustment.get_value() != value) {
		ignore_toggle = true; 
		gain_adjustment.set_value (value);
		ignore_toggle = false;
	}
}

void
GainMeterBase::gain_changed ()
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun(*this, &GainMeterBase::effective_gain_display));
}

void
GainMeterBase::set_meter_strip_name (const char * name)
{
	meter_metric_area.set_name (name);
}

void
GainMeterBase::set_fader_name (const char * name)
{
	gain_slider->set_name (name);
}

void
GainMeterBase::update_gain_sensitive ()
{
	static_cast<Gtkmm2ext::SliderController*>(gain_slider)->set_sensitive (
			!(_io->gain_control()->alist()->automation_state() & Play));
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
GainMeterBase::meter_press(GdkEventButton* ev)
{
	boost::shared_ptr<Route> _route;

	wait_for_release = false;
	
	if ((_route = boost::dynamic_pointer_cast<Route>(_io)) == 0) {
		return FALSE;
	}

	if (!ignore_toggle) {

		if (Keyboard::is_context_menu_event (ev)) {
			
			// no menu at this time.

		} else {

			if (Keyboard::is_button2_event(ev)) {

				// Primary-button2 click is the midi binding click
				// button2-click is "momentary"
				
				if (!Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier))) {
					wait_for_release = true;
					old_meter_point = _route->meter_point ();
				}
			}

			if (ev->button == 1 || Keyboard::is_button2_event (ev)) {

				if (Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {

					/* Primary+Tertiary-click applies change to all routes */

					_session.begin_reversible_command (_("meter point change"));
                                        Session::GlobalMeteringStateCommand *cmd = new Session::GlobalMeteringStateCommand (_session, this);
					_session.foreach_route (this, &GainMeterBase::set_meter_point, next_meter_point (_route->meter_point()));
                                        cmd->mark();
					_session.add_command (cmd);
					_session.commit_reversible_command ();
					
					
				} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {

					/* Primary-click: solo mix group.
					   NOTE: Primary-button2 is MIDI learn.
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
GainMeterBase::meter_release(GdkEventButton* ev)
{
	if(!ignore_toggle){
		if (wait_for_release){
			wait_for_release = false;
			
			boost::shared_ptr<Route> r;
			
			if ((r = boost::dynamic_pointer_cast<Route>(_io)) != 0) {
				set_meter_point (*r, old_meter_point);
			}
		}
	}

	return true;
}

void
GainMeterBase::set_meter_point (Route& route, MeterPoint mp)
{
	route.set_meter_point (mp, this);
}

void
GainMeterBase::set_mix_group_meter_point (Route& route, MeterPoint mp)
{
	RouteGroup* mix_group;

	if((mix_group = route.mix_group()) != 0){
		mix_group->apply (&Route::set_meter_point, mp, this);
	} else {
		route.set_meter_point (mp, this);
	}
}

void
GainMeterBase::meter_point_clicked ()
{
	boost::shared_ptr<Route> r;

	if ((r = boost::dynamic_pointer_cast<Route> (_io)) != 0) {
		/* WHAT? */
	}
}

gint
GainMeterBase::start_gain_touch (GdkEventButton* ev)
{
	_io->gain_control()->start_touch ();
	return FALSE;
}

gint
GainMeterBase::end_gain_touch (GdkEventButton* ev)
{
	_io->gain_control()->stop_touch ();
	return FALSE;
}

gint
GainMeterBase::gain_automation_state_button_event (GdkEventButton *ev)
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
GainMeterBase::gain_automation_style_button_event (GdkEventButton *ev)
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
GainMeterBase::astate_string (AutoState state)
{
	return _astate_string (state, false);
}

string
GainMeterBase::short_astate_string (AutoState state)
{
	return _astate_string (state, true);
}

string
GainMeterBase::_astate_string (AutoState state, bool shrt)
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
GainMeterBase::astyle_string (AutoStyle style)
{
	return _astyle_string (style, false);
}

string
GainMeterBase::short_astyle_string (AutoStyle style)
{
	return _astyle_string (style, true);
}

string
GainMeterBase::_astyle_string (AutoStyle style, bool shrt)
{
	if (style & Trim) {
		return _("Trim");
	} else {
	        /* XXX it might different in different languages */

		return (shrt ? _("Abs") : _("Abs"));
	}
}

void
GainMeterBase::gain_automation_style_changed ()
{
	switch (_width) {
	case Wide:
		gain_automation_style_button.set_label (astyle_string(_io->gain_control()->alist()->automation_style()));
		break;
	case Narrow:
		gain_automation_style_button.set_label  (short_astyle_string(_io->gain_control()->alist()->automation_style()));
		break;
	}
}

void
GainMeterBase::gain_automation_state_changed ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &GainMeterBase::gain_automation_state_changed));
	
	bool x;

	switch (_width) {
	case Wide:
		gain_automation_state_button.set_label (astate_string(_io->gain_control()->alist()->automation_state()));
		break;
	case Narrow:
		gain_automation_state_button.set_label (short_astate_string(_io->gain_control()->alist()->automation_state()));
		break;
	}

	x = (_io->gain_control()->alist()->automation_state() != Off);
	
	if (gain_automation_state_button.get_active() != x) {
		ignore_toggle = true;
		gain_automation_state_button.set_active (x);
		ignore_toggle = false;
	}

	update_gain_sensitive ();
	
	/* start watching automation so that things move */
	
	gain_watching.disconnect();

	if (x) {
		gain_watching = ARDOUR_UI::RapidScreenUpdate.connect (mem_fun (*this, &GainMeterBase::effective_gain_display));
	}
}

void
GainMeterBase::update_meters()
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

void GainMeterBase::color_handler(bool dpi)
{
	color_changed = true;
	dpi_changed = (dpi) ? true : false;
	setup_meters();
}

void
GainMeterBase::set_width (Width w, int len)
{
	_width = w;
	level_meter->setup_meters (len);
}


void
GainMeterBase::on_theme_changed()
{
	style_changed = true;
}

GainMeter::GainMeter (Session& s)
	: GainMeterBase (s, slider, false)
{
	gain_display_box.set_homogeneous (true);
	gain_display_box.set_spacing (2);
	gain_display_box.pack_start (gain_display, true, true);

	meter_metric_area.set_name ("AudioTrackMetrics");
	set_size_request_to_display_given_text (meter_metric_area, "-50", 0, 0);

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
	hbox.pack_start (*fader_vbox, true, true);

	set_spacing (2);

	pack_start (gain_display_box, Gtk::PACK_SHRINK);
	pack_start (hbox, Gtk::PACK_SHRINK);

	meter_metric_area.signal_expose_event().connect (mem_fun(*this, &GainMeter::meter_metrics_expose));
}

void
GainMeter::set_io (boost::shared_ptr<IO> io)
{
	if (level_meter->get_parent()) {
		hbox.remove (*level_meter);
	}

	if (peak_display.get_parent()) {
		gain_display_box.remove (peak_display);
	}

	if (gain_automation_state_button.get_parent()) {
		fader_vbox->remove (gain_automation_state_button);
	}

	GainMeterBase::set_io (io);

	boost::shared_ptr<Route> r;

	if ((r = boost::dynamic_pointer_cast<Route> (_io)) != 0) {
		
		/* 
		   if we have a non-hidden route (ie. we're not the click or the auditioner), 
		   pack some route-dependent stuff.
		*/

		gain_display_box.pack_end (peak_display, true, true);
		hbox.pack_end (*level_meter, true, true);

		if (!r->is_hidden()) {
			fader_vbox->pack_start (gain_automation_state_button, false, false, 0);
		}
	}
}

int
GainMeter::get_gm_width ()
{
	Gtk::Requisition sz;
	hbox.size_request (sz);
	return sz.width;
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
	static Glib::RefPtr<Gtk::Style> meter_style;
	if (style_changed) {
		meter_style = meter_metric_area.get_style();
	}
	Glib::RefPtr<Gdk::Window> win (meter_metric_area.get_window());
	Glib::RefPtr<Gdk::GC> bg_gc (meter_style->get_bg_gc (Gtk::STATE_INSENSITIVE));
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

	if (i == metric_pixmaps.end() || style_changed || dpi_changed) {
		pixmap = render_metrics (meter_metric_area);
	} else {
		pixmap = i->second;
	}

	gdk_rectangle_intersect (&ev->area, &base_rect, &draw_rect);
	win->draw_drawable (bg_gc, pixmap, draw_rect.x, draw_rect.y, draw_rect.x, draw_rect.y, draw_rect.width, draw_rect.height);
	style_changed = false;
	return true;
}

boost::shared_ptr<PBD::Controllable>
GainMeterBase::get_controllable()
{
	return _io->gain_control();
}


