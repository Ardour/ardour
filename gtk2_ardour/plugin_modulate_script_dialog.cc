/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2011 Paul Davis
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "pbd/compose.h"

#include "gtkmm2ext/gui_thread.h"

#include "plugin_modulate_script_dialog.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;

PluginModulateScriptDialog::PluginModulateScriptDialog (boost::shared_ptr<ARDOUR::PluginInsert> pi)
	: ArdourWindow (string_compose (_("Modulate %1"), pi->name()))
	, _pi (pi)
	, _set_button (_("Set Script"))
	, _read_button (_("Read Active Script"))
	, _clear_button (_("Remove Script"))
{
	Gtk::ScrolledWindow *scrollin = manage (new Gtk::ScrolledWindow);
	scrollin->set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	scrollin->add (entry);

	Gtk::HBox *hbox = manage (new HBox());
	hbox->pack_start (_set_button, false, false, 2);
	hbox->pack_start (_read_button, false, false, 2);
	hbox->pack_start (_clear_button, false, false, 2);
	hbox->pack_end (_status, false, false, 2);

	vbox.pack_start (*scrollin, true, true, 0);
	vbox.pack_start (*hbox, false, false, 2);
	add (vbox);

	set_size_request (640, 480); // XXX

	_set_button.signal_clicked.connect (sigc::mem_fun(*this, &PluginModulateScriptDialog::set_script));
	_read_button.signal_clicked.connect (sigc::mem_fun(*this, &PluginModulateScriptDialog::read_script));
	_clear_button.signal_clicked.connect (sigc::mem_fun(*this, &PluginModulateScriptDialog::unload_script));


	_pi->ModulationScriptChanged.connect (
			_plugin_connection, invalidator (*this), boost::bind (&PluginModulateScriptDialog::script_changed, this), gui_context ()
			);

	read_script ();
	script_changed ();
}

PluginModulateScriptDialog::~PluginModulateScriptDialog ()
{
}

void
PluginModulateScriptDialog::script_changed ()
{
	if (_pi->modulation_script_loaded ()) {
		_read_button.set_sensitive (true);
		_clear_button.set_sensitive (true);
		_status.set_text (_("Status: running"));
	} else {
		_read_button.set_sensitive (false);
		_clear_button.set_sensitive (false);
		_status.set_text (_("Status: inactive"));
	}
}

void
PluginModulateScriptDialog::read_script ()
{
	Glib::RefPtr<Gtk::TextBuffer> tb (entry.get_buffer());
	tb->set_text (_pi->modulation_script ());
}

void
PluginModulateScriptDialog::set_script ()
{
	Glib::RefPtr<Gtk::TextBuffer> tb (entry.get_buffer());
	std::string script = tb->get_text();
	if (!_pi->load_modulation_script (script)) {
		Gtk::MessageDialog msg (*this, _("Loading the Script failed. Check syntax"));
		msg.run();
	}
}

void
PluginModulateScriptDialog::unload_script ()
{
	_pi->unload_modulation_script ();
}

/* ***************************************************************************/


PluginModulateScriptProxy::PluginModulateScriptProxy(std::string const &name, boost::weak_ptr<ARDOUR::PluginInsert> pi)
	: WM::ProxyBase (name, std::string())
	, _pi (pi)
{
	boost::shared_ptr<PluginInsert> p = _pi.lock ();
	if (!p) {
		return;
	}
	p->DropReferences.connect (going_away_connection, MISSING_INVALIDATOR, boost::bind (&PluginModulateScriptProxy::processor_going_away, this), gui_context());
}

PluginModulateScriptProxy::~PluginModulateScriptProxy()
{
	_window = 0;
}

ARDOUR::SessionHandlePtr*
PluginModulateScriptProxy::session_handle ()
{
	ArdourWindow* aw = dynamic_cast<ArdourWindow*> (_window);
	if (aw) { return aw; }
	return 0;
}

Gtk::Window*
PluginModulateScriptProxy::get (bool create)
{
	boost::shared_ptr<PluginInsert> pi = _pi.lock ();
	if (!pi) {
		return 0;
	}

	if (!_window) {
		if (!create) {
			return 0;
		}
		_window = new PluginModulateScriptDialog (pi);
		ArdourWindow* aw = dynamic_cast<ArdourWindow*> (_window);
		if (aw) {
			aw->set_session (_session);
		}
		_window->show_all ();
	}
	return _window;
}

void
PluginModulateScriptProxy::processor_going_away ()
{
	delete _window;
	_window = 0;
	WM::Manager::instance().remove (this);
	going_away_connection.disconnect();
	delete this;
}
