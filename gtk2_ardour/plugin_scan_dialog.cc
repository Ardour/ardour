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

PluginScanDialog::PluginScanDialog (bool just_cached, bool v)
	: ArdourDialog (_("Scanning for plugins"))
	, timeout_button (_("Stop Timeout"))
	, cancel_button (_("Cancel Plugin Scan"))
	, cache_only (just_cached)
	, verbose (v)
{
	VBox* vbox = get_vbox();
	vbox->set_size_request(400,-1);

	message.set_padding (12, 12);
	vbox->pack_start (message);

	cancel_button.set_name ("EditorGTKButton");
	cancel_button.signal_clicked().connect (sigc::mem_fun (*this, &PluginScanDialog::cancel_plugin_scan));
	cancel_button.show();

	vbox->pack_start (cancel_button, PACK_SHRINK);

	timeout_button.set_name ("EditorGTKButton");
	timeout_button.signal_clicked().connect (sigc::mem_fun (*this, &PluginScanDialog::cancel_plugin_timeout));
	timeout_button.show();

	pbar.set_orientation(Gtk::PROGRESS_RIGHT_TO_LEFT);
	pbar.set_pulse_step (0.1);
	pbar.set_text(_("Scan Timeout"));
	pbar.show();

	tbox.pack_start (pbar, PACK_EXPAND_WIDGET, 4);
	tbox.pack_start (timeout_button, PACK_SHRINK, 4);

	vbox->pack_start (tbox, PACK_SHRINK, 4);

	ARDOUR::PluginScanMessage.connect (connections, MISSING_INVALIDATOR, boost::bind(&PluginScanDialog::message_handler, this, _1, _2, _3), gui_context());
	ARDOUR::PluginScanTimeout.connect (connections, MISSING_INVALIDATOR, boost::bind(&PluginScanDialog::plugin_scan_timeout, this, _1), gui_context());

	vbox->show_all ();
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
	PluginManager::instance().refresh (cache_only);
	DEBUG_TRACE (DEBUG::GuiStartup, "plugin refresh complete\n");

	/* scan is done at this point, return full control to main event loop */
}

void
PluginScanDialog::cancel_plugin_scan ()
{
	PluginManager::instance().cancel_plugin_scan();
}

void
PluginScanDialog::cancel_plugin_timeout ()
{
	//PluginManager::instance().cancel_plugin_scan_timeout();
	timeout_button.set_sensitive (false);
}

void
PluginScanDialog::plugin_scan_timeout (int timeout)
{
	if (!is_mapped()) {
		return;
	}

	if (timeout > 0) {
		pbar.set_sensitive (true);
		timeout_button.set_sensitive (true);
		pbar.set_fraction ((float) timeout / (float) Config->get_vst_scan_timeout());
		tbox.show();
	} else if (timeout < 0) {
		pbar.set_sensitive (true);
		pbar.pulse ();
		timeout_button.set_sensitive (false);
		tbox.show();
	} else {
		pbar.set_sensitive (false);
		timeout_button.set_sensitive (false);
		tbox.hide();
	}

	ARDOUR_UI::instance()->gui_idle_handler ();
}

void
PluginScanDialog::message_handler (std::string type, std::string plugin, bool can_cancel)
{
	DEBUG_TRACE (DEBUG::GuiStartup, string_compose (X_("plugin scan message: %1 cancel? %2\n"), type, can_cancel));

	if (type == X_("closeme") && !is_mapped()) {
		return;
	}

	const bool cancelled = PluginManager::instance().cancelled();

	if (type != X_("closeme") && (!UIConfiguration::instance().get_show_plugin_scan_window()) && !verbose) {

		if (cancelled && is_mapped()) {
			hide();
			connections.drop_connections();
			ARDOUR_UI::instance()->gui_idle_handler ();
			return;
		}
		if (cancelled || !can_cancel) {
			return;
		}
	}

	if (type == X_("closeme")) {
		tbox.hide();
		hide();
		connections.drop_connections ();
	} else {
		message.set_text (type + ": " + PBD::basename_nosuffix (plugin));
		show();
	}

	if (!can_cancel || !cancelled) {
		timeout_button.set_sensitive(false);
	}

	cancel_button.set_sensitive(can_cancel && !cancelled);

	ARDOUR_UI::instance()->gui_idle_handler ();
}
