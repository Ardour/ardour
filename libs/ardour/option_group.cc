/*
    Copyright (C) 2013 Paul Davis

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

#include <glibmm/optioncontext.h>

#include "ardour/option_group.h"
#include "ardour/session.h"

#include "i18n.h"

namespace ARDOUR {

OptionGroup::OptionGroup()
	: Glib::OptionGroup("libardour", _("libardour options"), _("Command-line options for libardour"))
	, m_arg_novst(false)
	, m_arg_no_hw_optimizations(false)
	, m_disable_plugins(false)
	, m_parsed(false)
{
	Glib::OptionEntry entry;

#if defined(WINDOWS_VST_SUPPORT) || defined(LXVST_SUPPORT)
	entry.set_long_name("novst");
	entry.set_short_name('V');
	entry.set_description(_("Do not use VST support."));
	add_entry(entry, m_arg_novst);
#endif

	entry.set_long_name("no-hw-optimizations");
	entry.set_short_name('O');
	entry.set_description(_("Disable h/w specific optimizations."));
	add_entry(entry, m_arg_no_hw_optimizations);

	entry.set_long_name("disable-plugins");
	entry.set_short_name('d');
	entry.set_description(_("Disable all plugins in an existing session"));
	add_entry(entry, m_disable_plugins);
}

bool
OptionGroup::on_post_parse (Glib::OptionContext&, Glib::OptionGroup&)
{
	if (m_disable_plugins) {
		ARDOUR::Session::set_disable_all_loaded_plugins (true);
	}

	m_parsed = true;

	return true;
}

} // namespace ARDOUR
