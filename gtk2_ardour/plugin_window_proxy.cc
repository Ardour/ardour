/*
 * Copyright (C) 2022 Robin Gareus <robin@gareus.org>
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

#include "ardour/plug_insert_base.h"
#include "ardour/plugin_manager.h"

#include "gui_thread.h"
#include "plugin_ui.h"
#include "plugin_window_proxy.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace Gtkmm2ext;

PluginWindowProxy::PluginWindowProxy (std::string const& name, std::string const& title, std::weak_ptr<PlugInsertBase> plugin)
	: WM::ProxyBase (name, std::string ())
	, _pib (plugin)
	, _title (title)
	, _is_custom (true)
	, _want_custom (true)
{
	std::shared_ptr<PlugInsertBase> p = _pib.lock ();
	if (!p) {
		return;
	}
	p->DropReferences.connect (*this, MISSING_INVALIDATOR, boost::bind (&PluginWindowProxy::plugin_going_away, this), gui_context ());
}

PluginWindowProxy::~PluginWindowProxy ()
{
	_window = 0;
}

Gtk::Window*
PluginWindowProxy::get (bool create)
{
	std::shared_ptr<PlugInsertBase> p = _pib.lock ();
	if (!p) {
		return 0;
	}

	if (_window && (_is_custom != _want_custom)) {
		set_state_mask (WindowProxy::StateMask (state_mask () & ~WindowProxy::Size));
		drop_window ();
	}

	if (!_window) {
		if (!create) {
			return 0;
		}

		_is_custom = _want_custom;
		_window    = new PluginUIWindow (p, false, _is_custom);

		if (_window) {
			_window->set_title (generate_processor_title (p));
			setup ();
			_window->show_all ();
		}
	}
	return _window;
}

void
PluginWindowProxy::show_the_right_window ()
{
	if (_window && (_is_custom != _want_custom)) {
		set_state_mask (WindowProxy::StateMask (state_mask () & ~WindowProxy::Size));
		drop_window ();
	}

	if (_window) {
		_window->unset_transient_for ();
	}
	toggle ();
}

int
PluginWindowProxy::set_state (const XMLNode& node, int)
{
	XMLNodeList                 children = node.children ();
	XMLNodeList::const_iterator i        = children.begin ();
	while (i != children.end ()) {
		std::string name;
		if ((*i)->name () == X_("Window") && (*i)->get_property (X_("name"), name) && name == _name) {
			break;
		}
		++i;
	}

	if (i != children.end ()) {
		(*i)->get_property (X_("custom-ui"), _want_custom);
	}

	return ProxyBase::set_state (node, 0);
}

XMLNode&
PluginWindowProxy::get_state () const
{
	XMLNode* node;
	node = &ProxyBase::get_state ();
	node->set_property (X_("custom-ui"), _is_custom);
	return *node;
}

void
PluginWindowProxy::plugin_going_away ()
{
	delete _window;
	_window = 0;
	WM::Manager::instance ().remove (this);
	drop_connections ();
	delete this;
}

std::string
PluginWindowProxy::generate_processor_title (std::shared_ptr<PlugInsertBase> p)
{
	std::string maker = p->plugin()->maker() ? p->plugin()->maker() : "";
	std::string::size_type email_pos;

	if ((email_pos = maker.find_first_of ('<')) != std::string::npos) {
		maker = maker.substr (0, email_pos - 1);
	}

	if (maker.length() > 32) {
		maker = maker.substr (0, 32);
		maker += " ...";
	}

	std::string type = PluginManager::plugin_type_name (p->type ());
	auto so = std::dynamic_pointer_cast<SessionObject> (p);
	assert (so);

	return string_compose(_("%1: %2 (by %3) [%4]"), _title, so->name(), maker, type);
}
