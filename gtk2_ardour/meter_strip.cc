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

#include <list>

#include <sigc++/bind.h>

#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/route_group.h"

#include <gtkmm2ext/gtk_ui.h>

#include "ardour_ui.h"
#include "gui_thread.h"
#include "ardour_window.h"

#include "meterbridge.h"
#include "meter_strip.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;

PBD::Signal1<void,MeterStrip*> MeterStrip::CatchDeletion;

MeterStrip::MeterStrip (Meterbridge& mtr, Session* sess, boost::shared_ptr<ARDOUR::Route> rt)
	: _meterbridge(mtr)
{
	_route = rt;

	level_meter = new LevelMeter(sess);
	level_meter->set_meter (rt->shared_peak_meter().get());
	level_meter->clear_meters();
	level_meter->setup_meters (350, 6);

	rt->DropReferences.connect (route_connections, invalidator (*this), boost::bind (&MeterStrip::self_delete, this), gui_context());
	rt->PropertyChanged.connect (route_connections, invalidator (*this), boost::bind (&MeterStrip::strip_property_changed, this, _1), gui_context());


	pack_start (*level_meter, true, true);
	level_meter->show();

	label = manage(new Gtk::Label(rt->name().c_str()));
	pack_start (*label, true, true);
	label->show();
}

void
MeterStrip::fast_update ()
{
	float mpeak = level_meter->update_meters();
}

MeterStrip::~MeterStrip ()
{
	delete level_meter;
	CatchDeletion (this);
}

void
MeterStrip::self_delete ()
{
	delete this;
}

void
MeterStrip::strip_property_changed (const PropertyChange& what_changed)
{
	if (!what_changed.contains (ARDOUR::Properties::name)) {
		return;
	}
	ENSURE_GUI_THREAD (*this, &MeterStrip::strip_name_changed, what_changed)
	label->set_text(_route->name());
}
