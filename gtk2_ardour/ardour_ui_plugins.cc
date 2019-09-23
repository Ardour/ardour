/*
 * Copyright (C) 2005-2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2005-2017 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2005-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2010 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2015 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2013-2016 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2013-2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015 André Nusser <andre.nusser@googlemail.com>
 * Copyright (C) 2016-2018 Len Ovens <len@ovenwerks.net>
 * Copyright (C) 2017 Johannes Mueller <github@johannes-mueller.org>
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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#include "gtk2ardour-version.h"
#endif

#include <gtkmm/progressbar.h>

#include "pbd/i18n.h"

#include "ardour/plugin_manager.h"

#include "ardour_ui.h"
#include "ui_config.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace std;

/* TODO: this is getting elaborate enough to warrant being split into a dedicated class */
static MessageDialog *scan_dlg = NULL;
static ProgressBar   *scan_pbar = NULL;
static HBox          *scan_tbox = NULL;
static Gtk::Button   *scan_timeout_button;

void
ARDOUR_UI::cancel_plugin_scan ()
{
	PluginManager::instance().cancel_plugin_scan();
}

void
ARDOUR_UI::cancel_plugin_timeout ()
{
	PluginManager::instance().cancel_plugin_timeout();
	scan_timeout_button->set_sensitive (false);
}

void
ARDOUR_UI::plugin_scan_timeout (int timeout)
{
	if (!scan_dlg || !scan_dlg->is_mapped() || !scan_pbar) {
		return;
	}
	if (timeout > 0) {
		scan_pbar->set_sensitive (false);
		scan_timeout_button->set_sensitive (true);
		scan_pbar->set_fraction ((float) timeout / (float) Config->get_vst_scan_timeout());
		scan_tbox->show();
	} else {
		scan_pbar->set_sensitive (false);
		scan_timeout_button->set_sensitive (false);
	}
	gui_idle_handler();
}

void
ARDOUR_UI::plugin_scan_dialog (std::string type, std::string plugin, bool can_cancel)
{
	if (type == X_("closeme") && !(scan_dlg && scan_dlg->is_mapped())) {
		return;
	}

	const bool cancelled = PluginManager::instance().cancelled();
	if (type != X_("closeme") && (!UIConfiguration::instance().get_show_plugin_scan_window()) && !_initial_verbose_plugin_scan) {
		if (cancelled && scan_dlg->is_mapped()) {
			scan_dlg->hide();
			gui_idle_handler();
			return;
		}
		if (cancelled || !can_cancel) {
			return;
		}
	}

	static Gtk::Button *cancel_button;
	if (!scan_dlg) {
		scan_dlg = new MessageDialog("", false, MESSAGE_INFO, BUTTONS_NONE); // TODO manage
		VBox* vbox = scan_dlg->get_vbox();
		vbox->set_size_request(400,-1);
		scan_dlg->set_title (_("Scanning for plugins"));

		cancel_button = manage(new Gtk::Button(_("Cancel plugin scan")));
		cancel_button->set_name ("EditorGTKButton");
		cancel_button->signal_clicked().connect ( mem_fun (*this, &ARDOUR_UI::cancel_plugin_scan) );
		cancel_button->show();

		scan_dlg->get_vbox()->pack_start ( *cancel_button, PACK_SHRINK);

		scan_tbox = manage( new HBox() );

		scan_timeout_button = manage(new Gtk::Button(_("Stop Timeout")));
		scan_timeout_button->set_name ("EditorGTKButton");
		scan_timeout_button->signal_clicked().connect ( mem_fun (*this, &ARDOUR_UI::cancel_plugin_timeout) );
		scan_timeout_button->show();

		scan_pbar = manage(new ProgressBar());
		scan_pbar->set_orientation(Gtk::PROGRESS_RIGHT_TO_LEFT);
		scan_pbar->set_text(_("Scan Timeout"));
		scan_pbar->show();

		scan_tbox->pack_start (*scan_pbar, PACK_EXPAND_WIDGET, 4);
		scan_tbox->pack_start (*scan_timeout_button, PACK_SHRINK, 4);

		scan_dlg->get_vbox()->pack_start (*scan_tbox, PACK_SHRINK, 4);
	}

	assert(scan_dlg && scan_tbox && cancel_button);

	if (type == X_("closeme")) {
		scan_tbox->hide();
		scan_dlg->hide();
	} else {
		scan_dlg->set_message(type + ": " + Glib::path_get_basename(plugin));
		scan_dlg->show();
	}
	if (!can_cancel || !cancelled) {
		scan_timeout_button->set_sensitive(false);
	}
	cancel_button->set_sensitive(can_cancel && !cancelled);

	gui_idle_handler();
}
