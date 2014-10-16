/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#ifndef __ardour_export_preset_h__
#define __ardour_export_preset_h__

#include <string>

#include "pbd/uuid.h"
#include "pbd/xml++.h"

#include "ardour/libardour_visibility.h"

namespace ARDOUR
{

class Session;

class LIBARDOUR_API ExportPreset {
  public:
	ExportPreset (std::string filename, Session & s);
	~ExportPreset ();

	PBD::UUID const & id () const { return _id; }
	std::string name () const { return _name; }

	void set_name (std::string const & name);

	// Note: The set_..._state functions take ownership of the XMLNode
	void set_global_state (XMLNode & state);
	void set_local_state (XMLNode & state);

	XMLNode const * get_global_state () const { return global.root(); }
	XMLNode const * get_local_state () const { return local; }

	void save (std::string const & filename);
	void remove_local () const;

  private:

	void set_id (std::string const & id);

	XMLNode * get_instant_xml () const;
	void save_instant_xml () const;
	void remove_instant_xml () const;

	PBD::UUID  _id;
	std::string _name;

	Session &   session;
	XMLTree     global;
	XMLNode *   local;

};

} // namespace ARDOUR

#endif // __ardour_export_preset_h__
