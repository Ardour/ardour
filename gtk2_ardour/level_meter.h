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

#include <ardour/types.h>

#include <gtkmm2ext/click_box.h>
#include <gtkmm2ext/focus_entry.h>
#include <gtkmm2ext/slider_controller.h>

#include "enums.h"

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

class LevelMeter : public Gtk::HBox
{
  public:
	LevelMeter (ARDOUR::Session&);
	~LevelMeter ();

	virtual void set_io (boost::shared_ptr<ARDOUR::IO> io);

	void update_gain_sensitive ();

	float update_meters ();
	void update_meters_falloff ();
	void clear_meters ();
	void hide_meters ();
	void setup_meters (int len=0, int width=3);

  private:

	//friend class MixerStrip;
	boost::shared_ptr<ARDOUR::IO> _io;
	ARDOUR::Session& _session;

	Width _width;

	struct MeterInfo {
	    Gtkmm2ext::FastMeter *meter;
	    gint16          width;
		int				length;   
	    bool            packed;
	    
	    MeterInfo() { 
		    meter = 0;
		    width = 0;
			length = 0;
		    packed = false;
	    }
	};

	guint16 regular_meter_width;
	static const guint16 thin_meter_width = 2;
	std::vector<MeterInfo>    meters;
	float       max_peak;
	
	void hide_all_meters ();
	gint meter_button_release (GdkEventButton*, uint32_t);

	void parameter_changed (const char*);

	void on_theme_changed ();
	bool style_changed;
	bool color_changed;
	void color_handler ();
};

#endif /* __ardour_gtk_track_meter_h__ */

