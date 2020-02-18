/*
 * Copyright (C) 2005-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ardour_gtk_about_h__
#define __ardour_gtk_about_h__

#include <gtkmm/aboutdialog.h>

#include "ardour/session_handle.h"

class ConfigInfoDialog;

class About : public Gtk::AboutDialog, public ARDOUR::SessionHandlePtr
{
public:
	About ();
	~About ();

private:
	ConfigInfoDialog* config_info;
	void show_config_info ();

	static void launch_homepage (AboutDialog& about, const Glib::ustring& website_url);
};

#endif /* __ardour_gtk_about_h__ */
