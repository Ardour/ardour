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

#ifndef __command_line_options_h__
#define __command_line_options_h__

#include <glibmm/optioncontext.h>

namespace ARDOUR {

/**
 * These options are not supposed to be modified during the execution of
 * the application.
 */
class CommandLineOptions : public Glib::OptionGroup
{
public:
	CommandLineOptions();

	virtual bool on_post_parse (Glib::OptionContext&, Glib::OptionGroup&);

	bool m_arg_novst;
	bool m_arg_no_hw_optimizations;
	bool m_disable_plugins;

	bool m_parsed;
};

} // namespace ARDOUR

#endif
