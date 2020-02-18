/*
 * Copyright (C) 2013-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2020 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk_ardour_missing_filesource_dialog_h__
#define __gtk_ardour_missing_filesource_dialog_h__

#include <list>
#include <string>

#include "ardour/data_type.h"

#include "ardour_dialog.h"

namespace ARDOUR {
	class Session;
}

class MissingFileSourceDialog : public ArdourDialog
{
public:
	MissingFileSourceDialog (ARDOUR::Session *, std::list<std::string> const &, ARDOUR::DataType);
};

#endif
