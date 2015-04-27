/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#ifndef __tracks_compact_meter_strip__
#define __tracks_compact_meter_strip__

#include "waves_ui.h"
#include "gtkmm2ext/persistent_tooltip.h"
#include "level_meter.h"
#include "ardour/ardour.h"

namespace ARDOUR {
	class Route;
	class Session;
}

class CompactMeterStrip : public Gtk::EventBox, public WavesUI
{
  public:
	CompactMeterStrip (ARDOUR::Session*, boost::shared_ptr<ARDOUR::Route>);
	~CompactMeterStrip ();

	size_t get_serial_number () { 	return _serial_number; }
	void set_serial_number ( size_t serial_number ) { _serial_number = serial_number; }
    void update_tooltip ();
    void set_persistant_tooltip_font (Pango::FontDescription font);
    
	void fast_update ();
	boost::shared_ptr<ARDOUR::Route> route() { return _route; }
	static PBD::Signal1<void,CompactMeterStrip*> CatchDeletion;
	

  protected:
	boost::shared_ptr<ARDOUR::Route> _route;
	void self_delete ();

  private:
	Gtk::Box&      _level_meter_home;
	LevelMeterHBox _level_meter;
	Gtk::EventBox& _record_indicator;
	PBD::ScopedConnectionList _route_connections;
	int _meter_width;
	int _thin_meter_width;
	size_t _serial_number;
    Gtkmm2ext::PersistentTooltip _tooltip;
	void meter_configuration_changed (ARDOUR::ChanCount);
	void update_rec_display ();
    void route_property_changed(const PBD::PropertyChange& what_changed);
    bool on_eventbox_button_press (GdkEventButton* ev);
};

#endif /* __tracks_compact_meter_strip__ */
