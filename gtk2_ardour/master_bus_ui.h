/*
    Copyright (C) 2006 Paul Davis

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

#ifndef __tracks_master_bus_ui_h__
#define __tracks_master_bus_ui_h__

#include <list>
#include <set>

#include <gtkmm/table.h>
#include <gtkmm/button.h>
#include <gtkmm/box.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/radiomenuitem.h>
#include <gtkmm/checkmenuitem.h>
#include <gtkmm/adjustment.h>

#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/slider_controller.h>

#include "ardour/playlist.h"
#include "ardour/types.h"

#include "route_ui.h"
#include "enums.h"
#include "gain_meter.h"

namespace ARDOUR {
	class Session;
	class Region;
	class RouteGroup;
	class IOProcessor;
	class Processor;
	class Location;
	class Playlist;
}


class PublicEditor;
class RegionView;
class StreamView;
class Selection;
class RegionSelection;
class Selectable;
class AutomationTimeAxisView;
class AutomationLine;
class ProcessorAutomationLine;
class TimeSelection;
class RouteGroupMenu;

class MasterBusUI : public RouteUI
{
public:
 	MasterBusUI (PublicEditor&, 
				 ARDOUR::Session*);
 	virtual ~MasterBusUI ();

	void set_route (boost::shared_ptr<ARDOUR::Route>);
	virtual void set_button_names ();
	virtual std::string state_id () const;

private:
	Gtk::Box& gain_meter_home;
	GainMeter gm;
};

#endif /* __tracks_master_bus_ui_h__ */

