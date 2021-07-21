/*
 * Copyright (C) 2005-2019 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __gtk2_ardour_plugin_scan_dialog_h__
#define __gtk2_ardour_plugin_scan_dialog_h__

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>

#include "ardour_dialog.h"

class PluginScanDialog : public ArdourDialog
{
public:
	PluginScanDialog (bool cache_only, bool verbose, Gtk::Window* parent = NULL);
	void start ();

private:
	Gtk::Label       message;
	Gtk::Label       timeout_info;
	Gtk::ProgressBar pbar;
	Gtk::Button      btn_timeout_one;
	Gtk::Button      btn_timeout_all;
	Gtk::Button      btn_cancel_all;
	Gtk::Button      btn_cancel_one;
	bool             cache_only;
	bool             verbose;
	bool             delayed_close;

	void on_hide ();

	void cancel_scan_all ();
	void cancel_scan_one ();
	void cancel_scan_timeout_all ();
	void cancel_scan_timeout_one ();

	void show_interactive_ctrls (bool show = true);

	void plugin_scan_timeout (int timeout);
	void message_handler (std::string type, std::string plugin, bool can_cancel);

	PBD::ScopedConnectionList connections;
};

#endif /* __gtk2_ardour_plugin_scan_dialog_h__ */
