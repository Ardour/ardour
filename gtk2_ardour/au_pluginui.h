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

#ifndef __au_plugin_ui_h__
#define __au_plugin_ui_h__

#include <boost/shared_ptr.hpp>

#include <Carbon/Carbon.h>
#include <AudioUnit/AudioUnit.h>

namespace ARDOUR {
	class AUPlugin;
	class PluginInsert;
	class IOProcessor;
}

class AUPluginUI
{
  public:
	AUPluginUI (boost::shared_ptr<ARDOUR::PluginInsert>);
	~AUPluginUI ();
	
  private:
	WindowRef wr;
	boost::shared_ptr<ARDOUR::AUPlugin> au;

	void plugin_going_away (ARDOUR::IOProcessor*);
	Component get_carbon_view_component(OSType subtype);
};

#endif // __au_plugin_ui_h__
