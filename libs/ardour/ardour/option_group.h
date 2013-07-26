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

#ifndef __option_group_h__
#define __option_group_h__

#include <glibmm/optioncontext.h>

namespace ARDOUR {

/**
 * This class could be called CommandLineOptions or something similar
 * but I think that only really makes sense if these options are not
 * going to change during the execution of the program, otherwise these
 * are just a group of options that just happen to also be accessible
 * via the command line. Although as a class name OptionGroup isn't
 * very explanatory whereas CommandLineOptions is.
 */
class OptionGroup : public Glib::OptionGroup
{
public:
	OptionGroup();

	virtual bool on_post_parse (Glib::OptionContext&, Glib::OptionGroup&);

	bool m_arg_novst;
	bool m_arg_no_hw_optimizations;
	bool m_disable_plugins;

	bool m_parsed;
};

} // namespace ARDOUR

#endif
