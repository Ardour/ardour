/*
    Copyright (C) 2016 Paul Davis

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

#include "ardour/vca.h"

#include "vca_master_strip.h"

using namespace ARDOUR;
using std::string;

VCAMasterStrip::VCAMasterStrip (Session* s, boost::shared_ptr<VCA> v)
	: AxisView (s)
	, _vca (v)
	, gain_meter (s, 250)
{
	gain_meter.set_controls (boost::shared_ptr<Route>(),
	                         boost::shared_ptr<PeakMeter>(),
	                         boost::shared_ptr<Amp>(),
	                         _vca->control());

	name_button.set_text (_vca->name());
	active_button.set_text ("active");

	pack_start (active_button, false, false);
	pack_start (name_button, false, false);
	pack_start (gain_meter, true, true);

	active_button.show_all ();
	name_button.show_all ();
	gain_meter.show_all ();
}

string
VCAMasterStrip::name() const
{
	return _vca->name();
}
