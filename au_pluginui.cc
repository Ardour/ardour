/*
    Copyright (C) 2006 Paul Davis 
	Written by Taybin Rutkin

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

#include <ardour/insert.h>
#include <ardour/audio_unit.h>

#include "plugin_ui.h"

using namespace ARDOUR;
using namespace PBD;

AUPluginUI::AUPluginUI (boost::shared_ptr<PluginInsert> pi, boost::shared_ptr<AUPlugin> ap)
	: PlugUIBase (pi),
	  au (ap)
{
	info << "AUPluginUI created" << endmsg;
}

AUPluginUI::~AUPluginUI ()
{
	// nothing to do here - plugin destructor destroys the GUI
}

int
AUPluginUI::get_preferred_height ()
{
	return -1;
}
