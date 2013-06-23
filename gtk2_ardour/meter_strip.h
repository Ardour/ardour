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

#ifndef __ardour_meter_strip__
#define __ardour_meter_strip__

#include <vector>

#include <cmath>

#include "pbd/stateful.h"

#include "ardour/types.h"
#include "ardour/ardour.h"

#include "level_meter.h"

namespace ARDOUR {
	class Route;
	class Session;
}
namespace Gtk {
	class Window;
	class Style;
}

class Meterbridge;

class MeterStrip : public Gtk::VBox
{
  public:
	MeterStrip (Meterbridge&, ARDOUR::Session*, boost::shared_ptr<ARDOUR::Route>);
	~MeterStrip ();

	void fast_update ();

	static PBD::Signal1<void,MeterStrip*> CatchDeletion;

  protected:
	boost::shared_ptr<ARDOUR::Route> _route;
	PBD::ScopedConnectionList route_connections;
	void self_delete ();

  private:
	Meterbridge& _meterbridge;
	Gtk::Label *label;

	LevelMeter   *level_meter;
	void meter_changed ();

	PBD::ScopedConnection _config_connection;
	void strip_property_changed (const PBD::PropertyChange&);

};

#endif /* __ardour_mixer_strip__ */
