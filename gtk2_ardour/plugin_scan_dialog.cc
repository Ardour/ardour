/*
 * Copyright (C) 2005-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
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

#include "pbd/basename.h"

#include "ardour/plugin_manager.h"

#include "widgets/tooltips.h"

#include "ardour_ui.h"
#include "debug.h"
#include "gui_thread.h"
#include "plugin_scan_dialog.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace std;

PluginScanDialog::PluginScanDialog (bool just_cached, bool v, Gtk::Window* parent)
	: ArdourDialog (_("Scanning for plugins"))
	, btn_timeout_enable (_("Auto skip unresponsive plugins"))
	, btn_cancel_all (_("Abort scanning (for all plugins)"))
	, btn_cancel_one (_("Skip this plugin"))
	, btn_size_group (SizeGroup::create (Gtk::SIZE_GROUP_HORIZONTAL))
	, cache_only (just_cached)
	, verbose (v)
	, delayed_close (false)
{
	message.set_alignment (0.0, 0.5);
	timeout_info.set_alignment (0.5, 0.5);
	timeout_info.set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Scan is taking a long time.\nPlease check for popup dialogs.")));
	timeout_info.set_justify (JUSTIFY_CENTER);

	pbar.set_orientation (Gtk::PROGRESS_RIGHT_TO_LEFT);
	pbar.set_pulse_step (0.1);

	if (cache_only) {
		pbar.set_no_show_all ();
		btn_timeout_enable.set_no_show_all ();
		btn_cancel_one.set_no_show_all ();
	}

	btn_size_group->add_widget (btn_timeout_enable);
	btn_size_group->add_widget (btn_cancel_all);

	int         row = 0;
	Gtk::Table* tbl = manage (new Table (4, 2, false));
	/* clang-format off */
	tbl->attach (message,            0, 2, row, row + 1, EXPAND | FILL, EXPAND | FILL, 0, 8); ++row;
	tbl->attach (timeout_info,       0, 2, row, row + 1, EXPAND | FILL, SHRINK,        0, 8); ++row;
	tbl->attach (pbar,               0, 1, row, row + 1, EXPAND | FILL, SHRINK,        4, 2);
	tbl->attach (btn_cancel_one,     1, 2, row, row + 1, FILL,          SHRINK,        4, 4); ++row;
	tbl->show_all ();
	/* clang-format on */

	tbl->set_border_width (8);

	format_frame.add (*tbl);
	format_frame.set_border_width (4);
	format_frame.set_shadow_type (Gtk::SHADOW_ETCHED_IN);

	Gtk::HBox* cancel_all_padder = manage (new HBox (true));
	cancel_all_padder->pack_start (btn_timeout_enable, true, true, 4);
	cancel_all_padder->pack_start (btn_cancel_all, true, true, 4);

	/* Top level packaging */
	VBox* vbox = get_vbox ();
	vbox->set_size_request (400, -1);

	vbox->pack_start (format_frame, true, true);
	vbox->pack_start (*cancel_all_padder, false, false);
	vbox->show_all ();

	/* connect to signals */
	ARDOUR::PluginScanMessage.connect (connections, MISSING_INVALIDATOR, boost::bind (&PluginScanDialog::message_handler, this, _1, _2, _3), gui_context ());
	ARDOUR::PluginScanTimeout.connect (connections, MISSING_INVALIDATOR, boost::bind (&PluginScanDialog::plugin_scan_timeout, this, _1), gui_context ());

	btn_cancel_all.signal_clicked.connect (sigc::mem_fun (*this, &PluginScanDialog::cancel_scan_all));
	btn_cancel_one.signal_clicked.connect (sigc::mem_fun (*this, &PluginScanDialog::cancel_scan_one));
	btn_timeout_enable.signal_clicked.connect (sigc::mem_fun (*this, &PluginScanDialog::enable_scan_timeout));

	/* set tooltips */
	ArdourWidgets::set_tooltip (btn_cancel_all, _("Cancel Scanning all plugins, and close this dialog.  Your plugin list might be incomplete."));
	ArdourWidgets::set_tooltip (btn_cancel_one, _("Cancel Scanning this plugin.  It will be Ignored in the plugin list."));
	ArdourWidgets::set_tooltip (btn_timeout_enable, _("When enabled, scan will ignore plugins that take a long time to scan."));

	/* window stacking */
	if (parent) {
		set_transient_for (*parent);
		set_position (Gtk::WIN_POS_CENTER_ON_PARENT);
		delayed_close = true;
	}
}

void
PluginScanDialog::start ()
{
	/* OK, this is extremely hard to understand on first reading, so please
	 * read this and think about it carefully if you are confused.
	 *
	 * Plugin discovery must take place in the main thread of the
	 * process. This is not true for all plugin APIs but it is true for
	 * VST.  For AU, although plugins themselves do not care, Apple decided
	 * that Cocoa must be "invoked" from the main thread. Since the plugin
	 * might show a "registration" GUI, discovery must be done
	 * in the main thread.
	 *
	 * This means that the PluginManager::refresh() call MUST be made from
	 * the main thread (typically the GUI thread, but certainly the thread
	 * running main()). Failure to do this will cause crashes, undefined
	 * behavior and other undesirable stuff (because plugin APIs failed to
	 * specify this aspect of the host behavior).
	 *
	 * The ::refresh call is likely to be slow, particularly in the case of
	 * VST(2) plugins where we are forced to load the shared object do
	 * discovery (there is no separate metadata as with LV2 for
	 * example). This means that it will block the GUI event loop where we
	 * are calling it from. This is a problem.
	 *
	 * Normally we would solve this by running it in a separate thread, but
	 * we cannot do this for reasons described above regarding plugin
	 * discovery.
	 *
	 * We "solve" this by making the PluginManager emit a signal as it
	 * examines every new plugin. Our handler for this signal checks the
	 * message, and then runs ARDOUR_UI::gui_idle_handler() which flushes
	 * the GUI event loop of pending events. This effectively handles
	 * redraws and event input and all the usual stuff, meaning that the
	 * GUI event loop appears to continue running during the ::refresh()
	 * call. In reality, it only runs at the start of each plugin
	 * discovery, so if the discovery process for a particular plugin takes
	 * a long time (e.g. because it displays a licensing window and sits
	 * waiting for input from the user), there's nothing we can do -
	 * control will not be returned to our GUI event loop until that is
	 * finished.
	 *
	 * This is a horrible design. Truly, really horrible. But it is caused
	 * by plugin APIs failing to mandate that discovery can happen from any
	 * thread and that plugins should NOT display a GUI or interact with
	 * the user during discovery/instantiation. Fundamentally, all plugin
	 * APIs should allow discovery without instantiation, like LV2 does
	 * (and to a very limited extent like AU does, if you play some games
	 * with the lower level APIs).
	 *
	 * For now (October 2019) it is the best we can come up with that does
	 * not break when some VST plugin decides to behave stupidly.
	 */

	DEBUG_TRACE (DEBUG::GuiStartup, "plugin refresh starting\n");
	PluginManager::instance ().refresh (cache_only);
	DEBUG_TRACE (DEBUG::GuiStartup, "plugin refresh complete\n");

	/* scan is done at this point, return full control to main event loop */
}

void
PluginScanDialog::cancel_scan_all ()
{
	PluginManager::instance ().cancel_scan_all ();
	btn_timeout_enable.set_sensitive (false);
}

void
PluginScanDialog::cancel_scan_one ()
{
	PluginManager::instance ().cancel_scan_one ();
	btn_cancel_one.set_sensitive (false);
}

void
PluginScanDialog::enable_scan_timeout ()
{
	PluginManager::instance ().enable_scan_timeout ();
	btn_timeout_enable.set_sensitive (false);
	pbar.show ();
}

void
PluginScanDialog::disable_per_plugin_interaction ()
{
	pbar.set_sensitive (false);
	pbar.set_text ("");
	pbar.set_fraction (0);
	btn_cancel_one.set_sensitive (false);
}

static void
format_time (char* buf, size_t size, int timeout)
{
	if (timeout < 0) {
		snprintf (buf, size, "-");
	} else if (timeout < 100) {
		snprintf (buf, size, "%.1f%s", timeout / 10.f, S_("seconds|s"));
	} else if (timeout < 600) {
		snprintf (buf, size, "%.0f%s", timeout / 10.f, S_("seconds|s"));
	} else if (timeout < 36000) {
		int tsec = timeout / 10;
		snprintf (buf, size, "%d%s %02d%s", tsec / 60, S_("minutes|m"), tsec % 60, S_("seconds|s"));
	} else {
		int tsec = timeout / 10;
		int tmin = tsec / 60;
		int thrs = tmin / 60;
		snprintf (buf, size, "%d:%02d:%.02d", thrs, tmin % 60, tsec % 60);
	}
}

void
PluginScanDialog::plugin_scan_timeout (int timeout)
{
	if (!is_mapped ()) {
		return;
	}


	if (timeout > 0) {
		int scan_timeout = Config->get_plugin_scan_timeout ();
		pbar.set_sensitive (true);
		if (scan_timeout > 400 && (scan_timeout - timeout) > 300) {
			timeout_info.show ();
		}
		if (timeout < scan_timeout) {
			char buf[128];
			format_time (buf, sizeof (buf), timeout);
			pbar.set_text (string_compose (_("Scan timeout %1"), buf));
		} else {
			pbar.set_text (_("Scanning"));
			timeout_info.hide ();
		}
		pbar.set_sensitive (true);
		pbar.set_fraction ((float)timeout / (float)scan_timeout);
	} else if (timeout < 0) {
		char buf[128];
		format_time (buf, sizeof (buf), -timeout);
		pbar.set_sensitive (true);
		pbar.set_text (string_compose (_("Scanning since %1"), buf));
		pbar.pulse ();
		if (timeout <= -300) {
			timeout_info.show ();
		}
	} else {
		disable_per_plugin_interaction ();
		timeout_info.hide ();
	}

	ARDOUR_UI::instance ()->gui_idle_handler ();
}

void
PluginScanDialog::on_hide ()
{
	cancel_scan_all ();
	ArdourDialog::on_hide ();
}

void
PluginScanDialog::message_handler (std::string type, std::string plugin, bool can_cancel)
{
	DEBUG_TRACE (DEBUG::GuiStartup, string_compose (X_("plugin scan message: %1 cancel? %2\n"), type, can_cancel));

	timeout_info.hide ();

	if (type == X_("closeme") && !is_mapped ()) {
		return;
	}

	const bool cancelled = PluginManager::instance ().cancelled ();

	if (type != X_("closeme") && !UIConfiguration::instance ().get_show_plugin_scan_window () && !verbose) {
		if (is_mapped ()) {
			hide ();
			connections.drop_connections ();
			ARDOUR_UI::instance ()->gui_idle_handler ();
			return;
		}
		return;
	}

	if (type == X_("closeme")) {
		disable_per_plugin_interaction ();
		connections.drop_connections ();
		btn_cancel_all.set_sensitive (false);
		btn_timeout_enable.set_sensitive (false);
		queue_draw ();
		for (int i = 0; delayed_close && i < 30; ++i) { // 1.5 sec delay
			Glib::usleep (50000);
			ARDOUR_UI::instance ()->gui_idle_handler ();
		}
		hide ();
	} else {
		format_frame.set_label (type);
		message.set_text (_("Scanning: ") + PBD::basename_nosuffix (plugin));
		show ();
	}

	btn_cancel_one.set_sensitive (can_cancel && !cancelled);
	btn_cancel_all.set_sensitive (can_cancel && !cancelled);

	ARDOUR_UI::instance ()->gui_idle_handler ();
}
