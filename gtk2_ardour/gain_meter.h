/*
 * Copyright (C) 2005-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2007-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef __ardour_gtk_gain_meter_h__
#define __ardour_gtk_gain_meter_h__

#include <vector>
#include <map>

#include <gtkmm/adjustment.h>
#include <gtkmm/alignment.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/frame.h>
#include <gtkmm/table.h>

//#include <gdkmm/colormap.h>

#include "pbd/signals.h"

#include "ardour/chan_count.h"
#include "ardour/types.h"
#include "ardour/session_handle.h"

#include "widgets/ardour_button.h"
#include "widgets/focus_entry.h"
#include "widgets/slider_controller.h"

#include "enums.h"
#include "level_meter.h"

namespace ARDOUR {
	class IO;
	class GainControl;
	class Session;
	class Route;
	class RouteGroup;
	class PeakMeter;
	class Amp;
	class Automatable;
}

namespace Gtk {
	class Menu;
}

enum MeterPointChangeTarget {
	MeterPointChangeAll,
	MeterPointChangeGroup,
	MeterPointChangeSingle
};

class GainMeterBase : virtual public sigc::trackable, ARDOUR::SessionHandlePtr
{
public:
	GainMeterBase (ARDOUR::Session*, bool horizontal, int, int);
	virtual ~GainMeterBase ();

	virtual void set_controls (boost::shared_ptr<ARDOUR::Route> route,
	                           boost::shared_ptr<ARDOUR::PeakMeter> meter,
	                           boost::shared_ptr<ARDOUR::Amp> amp,
	                           boost::shared_ptr<ARDOUR::GainControl> control);

	void update_gain_sensitive ();
	void update_meters ();

	const ARDOUR::ChanCount meter_channels () const;

	void effective_gain_display ();
	void set_width (Width, int len=0);
	void set_meter_strip_name (const char * name);
	void set_fader_name (const char * name);

	virtual void setup_meters (int len=0);

	boost::shared_ptr<PBD::Controllable> get_controllable();

	LevelMeterHBox& get_level_meter() const { return *level_meter; }
	ArdourWidgets::SliderController& get_gain_slider() const { return *gain_slider; }

	/** Emitted in the GUI thread when a button is pressed over the level meter;
	 *  return true if the event is handled.
	 */
	PBD::Signal1<bool, GdkEventButton *> LevelMeterButtonPress;

	static std::string meterpt_string (ARDOUR::MeterPoint);
	static std::string astate_string (ARDOUR::AutoState);
	static std::string short_astate_string (ARDOUR::AutoState);
	static std::string _astate_string (ARDOUR::AutoState, bool);

protected:

	friend class MixerStrip;
	friend class MeterStrip;
	friend class RouteTimeAxisView;
	friend class VCAMasterStrip;
	boost::shared_ptr<ARDOUR::Route> _route;
	boost::shared_ptr<ARDOUR::PeakMeter> _meter;
	boost::shared_ptr<ARDOUR::Amp> _amp;
	boost::shared_ptr<ARDOUR::GainControl> _control;
	std::vector<sigc::connection> connections;
	PBD::ScopedConnectionList model_connections;

	bool ignore_toggle;
	bool next_release_selects;

	ArdourWidgets::SliderController* gain_slider;
	Gtk::Adjustment                  gain_adjustment;
	ArdourWidgets::FocusEntry        gain_display;
	ArdourWidgets::FocusEntry        peak_display;
	Gtk::DrawingArea                 meter_metric_area;
	Gtk::DrawingArea                 meter_ticks1_area;
	Gtk::DrawingArea                 meter_ticks2_area;
	LevelMeterHBox*                  level_meter;

	sigc::connection gain_watching;

	ArdourWidgets::ArdourButton gain_automation_state_button;

	Gtk::Menu gain_astate_menu;

	ArdourWidgets::ArdourButton meter_point_button;

	Gtk::Menu meter_point_menu;

	void set_gain_astate (ARDOUR::AutoState);
	bool gain_astate_propagate;
	static sigc::signal<void, ARDOUR::AutoState> ChangeGainAutomationState;

	gint gain_automation_state_button_event (GdkEventButton *);
	gint pan_automation_state_button_event (GdkEventButton *);

	void gain_automation_state_changed();

	void setup_gain_adjustment ();
	Width _width;

	void show_gain ();
	void gain_activated ();
	bool gain_focused (GdkEventFocus*);

	float max_peak;

	void fader_moved ();
	void gain_changed ();

	void meter_point_clicked (ARDOUR::MeterPoint);
	void gain_unit_changed ();

	virtual void hide_all_meters ();

	gint meter_button_press (GdkEventButton*, uint32_t);

	bool peak_button_press (GdkEventButton*);
	bool peak_button_release (GdkEventButton*);
	bool gain_key_press (GdkEventKey*);

	Gtk::Menu* meter_menu;
	void popup_meter_menu (GdkEventButton*);

	void amp_stop_touch ();
	void amp_start_touch ();

	void set_route_group_meter_point (ARDOUR::Route&, ARDOUR::MeterPoint);
	void set_meter_point (ARDOUR::Route&, ARDOUR::MeterPoint);
	gint meter_press (GdkEventButton*);
	ARDOUR::MeterPoint old_meter_point;

	MeterPointChangeTarget meter_point_change_target;

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

	bool _clear_meters;
	bool _meter_peaked;
};

class GainMeter : public GainMeterBase, public Gtk::VBox
{
  public:
         GainMeter (ARDOUR::Session*, int);
	virtual ~GainMeter ();

	virtual void set_controls (boost::shared_ptr<ARDOUR::Route> route,
	                           boost::shared_ptr<ARDOUR::PeakMeter> meter,
	                           boost::shared_ptr<ARDOUR::Amp> amp,
	                           boost::shared_ptr<ARDOUR::GainControl> control);

	int get_gm_width ();
	void setup_meters (int len=0);
	void route_active_changed ();

  protected:
	void hide_all_meters ();

	gint meter_metrics_expose (GdkEventExpose *);
	gint meter_ticks1_expose (GdkEventExpose *);
	gint meter_ticks2_expose (GdkEventExpose *);
	void on_style_changed (const Glib::RefPtr<Gtk::Style>&);
	void redraw_metrics ();

  private:

	void meter_configuration_changed (ARDOUR::ChanCount);

	Gtk::HBox  gain_display_box;
	Gtk::HBox  fader_box;
	Gtk::VBox  fader_vbox;
	Gtk::HBox  hbox;
	Gtk::HBox  meter_hbox;
	Gtk::Alignment fader_alignment;
	Gtk::Alignment meter_alignment;
	std::vector<ARDOUR::DataType> _types;
};

#endif /* __ardour_gtk_gain_meter_h__ */

