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

#ifndef __ardour_gtk_track_meter_h__
#define __ardour_gtk_track_meter_h__

#include <vector>

#include <gtkmm/box.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/frame.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/button.h>
#include <gtkmm/table.h>
#include <gtkmm/drawingarea.h>

#include "ardour/types.h"
#include "ardour/chan_count.h"
#include "ardour/session_handle.h"

#include <gtkmm2ext/click_box.h>
#include <gtkmm2ext/focus_entry.h>
#include <gtkmm2ext/slider_controller.h>
#include <gtkmm2ext/fastmeter.h>

#include "enums.h"

namespace ARDOUR {
	class Session;
	class PeakMeter;
}
namespace Gtk {
	class Menu;
}

class LevelMeterBase : public ARDOUR::SessionHandlePtr, virtual public sigc::trackable
{
  public:
	LevelMeterBase (ARDOUR::Session*, PBD::EventLoop::InvalidationRecord* ir,
			Gtkmm2ext::FastMeter::Orientation o = Gtkmm2ext::FastMeter::Vertical);
	virtual ~LevelMeterBase ();

	virtual void set_meter (ARDOUR::PeakMeter* meter);

	void update_gain_sensitive ();

	float update_meters ();
	void update_meters_falloff ();
	void clear_meters (bool reset_highlight = true);
	void hide_meters ();
	void setup_meters (int len=0, int width=3, int thin=2);
	void set_max_audio_meter_count (uint32_t cnt = 0);

	void set_type (ARDOUR::MeterType);
	ARDOUR::MeterType get_type () { return meter_type; }

	/** Emitted in the GUI thread when a button is pressed over the meter */
	PBD::Signal1<bool, GdkEventButton *> ButtonPress;
	PBD::Signal1<bool, GdkEventButton *> ButtonRelease;
	PBD::Signal1<void, ARDOUR::MeterType> MeterTypeChanged;

	protected:
	virtual void mtr_pack(Gtk::Widget &w) = 0;
	virtual void mtr_remove(Gtk::Widget &w) = 0;

  private:
	PBD::EventLoop::InvalidationRecord* parent_invalidator;
	ARDOUR::PeakMeter* _meter;
	Gtkmm2ext::FastMeter::Orientation _meter_orientation;

	Width _width;

	struct MeterInfo {
	    Gtkmm2ext::FastMeter *meter;
	    gint16                width;
		int                   length;
	    bool                  packed;
	    float                 max_peak;

	    MeterInfo() {
		    meter = 0;
		    width = 0;
                    length = 0;
		    packed = false;
		    max_peak = -INFINITY;
	    }
	};

	guint16                regular_meter_width;
	int                    meter_length;
	guint16                thin_meter_width;
	std::vector<MeterInfo> meters;
	float                  max_peak;
	ARDOUR::MeterType      meter_type;
	ARDOUR::MeterType      visible_meter_type;
	uint32_t               midi_count;
	uint32_t               meter_count;
	uint32_t               max_visible_meters;

	PBD::ScopedConnection _configuration_connection;
	PBD::ScopedConnection _meter_type_connection;
	PBD::ScopedConnection _parameter_connection;

	void hide_all_meters ();
	bool meter_button_press (GdkEventButton *);
	bool meter_button_release (GdkEventButton *);

	void parameter_changed (std::string);
	void configuration_changed (ARDOUR::ChanCount in, ARDOUR::ChanCount out);
	void meter_type_changed (ARDOUR::MeterType);

	bool color_changed;
	void color_handler ();
};

class LevelMeterHBox : public LevelMeterBase, public Gtk::HBox
{
  public:
	LevelMeterHBox (ARDOUR::Session*);
	~LevelMeterHBox();

	protected:
	void mtr_pack(Gtk::Widget &w);
	void mtr_remove(Gtk::Widget &w);
};

class LevelMeterVBox : public LevelMeterBase, public Gtk::VBox
{
  public:
	LevelMeterVBox (ARDOUR::Session*);
	~LevelMeterVBox();

	protected:
	void mtr_pack(Gtk::Widget &w);
	void mtr_remove(Gtk::Widget &w);
};

#endif /* __ardour_gtk_track_meter_h__ */

