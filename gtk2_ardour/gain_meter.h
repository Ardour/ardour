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

#include "pbd/signals.h"

#include "ardour/chan_count.h"
#include "ardour/types.h"
#include "ardour/session_handle.h"

#include "ardour_button.h"

#include "gtkmm2ext/click_box.h"
#include "gtkmm2ext/focus_entry.h"
#include "gtkmm2ext/fader.h"

#include "waves_ui.h"
#include "enums.h"
#include "level_meter.h"

namespace ARDOUR {
	class IO;
	class Session;
	class Route;
	class RouteGroup;
	class PeakMeter;
	class Amp;
	class Automatable;
}
namespace Gtkmm2ext {
	class FastMeter;
	class BarController;
}
namespace Gtk {
	class Menu;
}

class GainMeter : virtual public sigc::trackable, ARDOUR::SessionHandlePtr, public Gtk::VBox, public WavesUI
{
  public:
    GainMeter (ARDOUR::Session*, const std::string& layout_script_file);
	virtual ~GainMeter ();

	virtual void set_controls (boost::shared_ptr<ARDOUR::Route> route,
				   boost::shared_ptr<ARDOUR::PeakMeter> meter,
				   boost::shared_ptr<ARDOUR::Amp> amp);

    void set_affected_by_selection (bool yn) {affected_by_selection = yn; }

	void update_gain_sensitive ();
	void update_meters ();

	void effective_gain_display ();
	void set_width (Width, int len=0);
	void set_fader_name (const char * name);

	virtual void setup_meters ();
	virtual void set_type (ARDOUR::MeterType);

	boost::shared_ptr<PBD::Controllable> get_controllable();

	LevelMeterHBox& get_level_meter() { return level_meter; }
	Gtkmm2ext::Fader& get_gain_slider() { return gain_slider; }
	WavesButton& get_gain_display_button () { return gain_display_button; }
	WavesButton& get_peak_display_button () { return peak_display_button; }

	/** Emitted in the GUI thread when a button is pressed over the level meter;
	 *  return true if the event is handled.
	 */
	PBD::Signal1<bool, GdkEventButton *> LevelMeterButtonPress;

	//
	int get_gm_width ();
	void route_active_changed ();


  protected:

	friend class MixerStrip;
	friend class RouteTimeAxisView;
	boost::shared_ptr<ARDOUR::Route> _route;
	boost::shared_ptr<ARDOUR::PeakMeter> _meter;
	boost::shared_ptr<ARDOUR::Amp> _amp;
	std::vector<sigc::connection> connections;
	PBD::ScopedConnectionList model_connections;

	bool ignore_toggle;
	bool next_release_selects;
    bool affected_by_selection;
    
	Gtkmm2ext::Fader&      gain_slider;
	Gtk::Adjustment&       gain_adjustment;
    Gtk::Entry&            gain_display_entry;
	WavesButton&           gain_display_button;
	WavesButton&           peak_display_button;
	Gtk::Box&              level_meter_home;
	LevelMeterHBox         level_meter;

	sigc::connection gain_watching;


	Gtk::Menu gain_astate_menu;
	Gtk::Menu gain_astyle_menu;

	void setup_gain_adjustment ();
    
    void adjust_gain_relatively(ARDOUR::gain_t val, const ARDOUR::RouteList& routes, void* src);
    ARDOUR::gain_t get_relative_gain_factor (ARDOUR::gain_t val, const ARDOUR::RouteList& routes);
    
	std::string astate_string (ARDOUR::AutoState);
	std::string short_astate_string (ARDOUR::AutoState);
	std::string _astate_string (ARDOUR::AutoState, bool);

	std::string astyle_string (ARDOUR::AutoStyle);
	std::string short_astyle_string (ARDOUR::AutoStyle);
	std::string _astyle_string (ARDOUR::AutoStyle, bool);

	void show_gain ();
	void gain_activated ();
	bool gain_focus_in (GdkEventFocus*);
	bool gain_focus_out (GdkEventFocus*);
	void start_gain_level_editing ();

	float max_peak;

	void gain_adjusted ();
	void gain_changed ();

	void meter_point_clicked ();
	void gain_unit_changed ();

	virtual void hide_all_meters ();

	gint meter_button_press (GdkEventButton*, uint32_t);

	bool peak_button_release (GdkEventButton*);
	bool gain_key_press (GdkEventKey*);
    bool gain_display_button_press (GdkEventButton*);
    bool gain_display_entry_press (GdkEventButton*);

	Gtk::Menu* meter_menu;
	void popup_meter_menu (GdkEventButton*);

	bool gain_slider_button_press (GdkEventButton *);
	bool gain_slider_button_release (GdkEventButton *);

	void set_route_group_meter_point (ARDOUR::Route&, ARDOUR::MeterPoint);
	void set_meter_point (ARDOUR::Route&, ARDOUR::MeterPoint);
	gint meter_release (GdkEventButton*);
	gint meter_press (GdkEventButton*);
	bool wait_for_release;
	ARDOUR::MeterPoint old_meter_point;

	void parameter_changed (const char*);

	void reset_peak_display ();
	void reset_route_peak_display (ARDOUR::Route*);
	void reset_group_peak_display (ARDOUR::RouteGroup*);

	void redraw_metrics ();
	void on_theme_changed ();
	void color_handler(bool);
	ARDOUR::DataType _data_type;
	ARDOUR::ChanCount _previous_amp_output_streams;

private:

	bool level_meter_button_press (GdkEventButton *);
	PBD::ScopedConnection _level_meter_connection;

	void meter_configuration_changed (ARDOUR::ChanCount);
	void meter_type_changed (ARDOUR::MeterType);

	std::vector<ARDOUR::DataType> _types;

	int _meter_width;
	int _thin_meter_width;
    bool _gain_slider_double_clicked;
};

#endif /* __ardour_gtk_gain_meter_h__ */

