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

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

AUPluginUI::AUPluginUI (boost::shared_ptr<PluginInsert> ap)
{
	if ((au = boost::dynamic_pointer_cast<AUPlugin> (ap->plugin())) == 0) {
		error << _("unknown type of editor-supplying plugin (note: no AudioUnit support in this version of ardour)") << endmsg;
		throw failed_constructor ();
	}

#if 0
	set_position (Gtk::WIN_POS_MOUSE);
	set_name ("PluginEditor");
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK|Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);

	signal_delete_event().connect (bind (sigc::ptr_fun (just_hide_it), reinterpret_cast<Window*> (this)));
	insert->GoingAway.connect (mem_fun(*this, &PluginUIWindow::plugin_going_away));

	if (scrollable) {
		gint h = _pluginui->get_preferred_height ();
		if (h > 600) h = 600;
		set_default_size (450, h); 
	}
#endif
	info << "AUPluginUI created" << endmsg;
}

AUPluginUI::~AUPluginUI ()
{
	// nothing to do here - plugin destructor destroys the GUI
}
