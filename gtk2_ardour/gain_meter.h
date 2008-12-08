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

#ifndef __ardour_gtk_gain_meter_h__
#define __ardour_gtk_gain_meter_h__

#include <vector>
#include <map>

#include <gtkmm/box.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/frame.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/button.h>
#include <gtkmm/table.h>
#include <gtkmm/drawingarea.h>
#include <gdkmm/colormap.h>

#include <ardour/types.h>

#include <gtkmm2ext/click_box.h>
#include <gtkmm2ext/focus_entry.h>
#include <gtkmm2ext/slider_controller.h>

#include "enums.h"
#include "level_meter.h"

namespace ARDOUR {
	class IO;
	class Session;
	class Route;
	class RouteGroup;
}
namespace Gtkmm2ext {
	class FastMeter;
	class BarController;
}
namespace Gtk {
	class Menu;
}

class GainMeterBase : virtual public sigc::trackable
{
  public:
	GainMeterBase ( ARDOUR::Session&, const Glib::RefPtr<Gdk::Pixbuf>& pix,
		       bool horizontal);
	virtual ~GainMeterBase ();

	virtual void set_io (boost::shared_ptr<ARDOUR::IO>);

	void update_gain_sensitive ();
	void update_meters ();

	void effective_gain_display ();
	void set_width (Width, int len=0);
	void set_meter_strip_name (const char * name);
	void set_fader_name (const char * name);

	virtual void setup_meters (int len=0);

	boost::shared_ptr<PBD::Controllable> get_controllable() { return _io->gain_control(); }

	LevelMeter& get_level_meter() const { return *level_meter; }
	Gtkmm2ext::SliderController& get_gain_slider() const { return *gain_slider; }

  protected:

	friend class MixerStrip;
	boost::shared_ptr<ARDOUR::IO> _io;
	ARDOUR::Session& _session;
	std::vector<sigc::connection> connections;

	bool ignore_toggle;
	bool next_release_selects;

	Gtkmm2ext::SliderController *gain_slider;
	Gtk::Adjustment              gain_adjustment;
	Gtkmm2ext::FocusEntry        gain_display;
	Gtk::Button                  peak_display;
	Gtk::DrawingArea             meter_metric_area;
	LevelMeter		    *level_meter;

	sigc::connection gain_watching;

	Gtk::Button gain_automation_style_button;
	Gtk::ToggleButton gain_automation_state_button;

	Gtk::Menu gain_astate_menu;
	Gtk::Menu gain_astyle_menu;

	gint gain_automation_style_button_event (GdkEventButton *);
	gint gain_automation_state_button_event (GdkEventButton *);
	gint pan_automation_style_button_event (GdkEventButton *);
	gint pan_automation_state_button_event (GdkEventButton *);

	void gain_automation_state_changed();
	void gain_automation_style_changed();

	std::string astate_string (ARDOUR::AutoState);
	std::string short_astate_string (ARDOUR::AutoState);
	std::string _astate_string (ARDOUR::AutoState, bool);

	std::string astyle_string (ARDOUR::AutoStyle);
	std::string short_astyle_string (ARDOUR::AutoStyle);
	std::string _astyle_string (ARDOUR::AutoStyle, bool);

	Width _width;

	void show_gain ();
	void gain_activated ();
	bool gain_focused (GdkEventFocus*);

	float       max_peak;
	
	void gain_adjusted ();
	void gain_changed ();
	
	void meter_point_clicked ();
	void gain_unit_changed ();
	
	virtual void hide_all_meters ();

	gint meter_button_press (GdkEventButton*, uint32_t);

	bool peak_button_release (GdkEventButton*);
	bool gain_key_press (GdkEventKey*);
	
	Gtk::Menu* meter_menu;
	void popup_meter_menu (GdkEventButton*);

	gint start_gain_touch (GdkEventButton*);
	gint end_gain_touch (GdkEventButton*);

	void set_mix_group_meter_point (ARDOUR::Route&, ARDOUR::MeterPoint);
	void set_meter_point (ARDOUR::Route&, ARDOUR::MeterPoint);
	gint meter_release (GdkEventButton*);
	gint meter_press (GdkEventButton*);
	bool wait_for_release;
	ARDOUR::MeterPoint old_meter_point;

	void parameter_changed (const char*);

	void reset_peak_display ();
	void reset_group_peak_display (ARDOUR::RouteGroup*);

	static sigc::signal<void> ResetAllPeakDisplays;
	static sigc::signal<void,ARDOUR::RouteGroup*> ResetGroupPeakDisplays;

	void on_theme_changed ();
	bool style_changed;
	bool dpi_changed;
	bool color_changed;
	void color_handler(bool);
};

class GainMeter : public GainMeterBase, public Gtk::VBox
{
  public:
	GainMeter (ARDOUR::Session&);
	~GainMeter () {}

	void set_io (boost::shared_ptr<ARDOUR::IO>);

	int get_gm_width ();
	void setup_meters (int len=0);

	static void setup_slider_pix ();

  protected:
	void hide_all_meters ();

	gint meter_metrics_expose (GdkEventExpose *);

	static std::map<std::string,Glib::RefPtr<Gdk::Pixmap> > metric_pixmaps;
	static Glib::RefPtr<Gdk::Pixmap> render_metrics (Gtk::Widget&);

  private:
	Gtk::HBox  gain_display_box;
	Gtk::HBox  fader_box;
	Gtk::VBox* fader_vbox;
	Gtk::HBox  hbox;

	static Glib::RefPtr<Gdk::Pixbuf> slider;
};

#endif /* __ardour_gtk_gain_meter_h__ */

