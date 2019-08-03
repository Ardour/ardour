/*
 * Copyright (C) 2008 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
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

#ifndef __ardour_tempo_map_importer_h__
#define __ardour_tempo_map_importer_h__

#include <boost/shared_ptr.hpp>

#include "pbd/xml++.h"

#include "ardour/element_importer.h"
#include "ardour/element_import_handler.h"

namespace ARDOUR {

class Session;

class LIBARDOUR_API TempoMapImportHandler : public ElementImportHandler
{
  public:
	TempoMapImportHandler (XMLTree const & source, Session & session);
	std::string get_info () const;
};

class LIBARDOUR_API TempoMapImporter : public ElementImporter
{
  private:
	typedef boost::shared_ptr<XMLNode> XMLNodePtr;
  public:
	TempoMapImporter (XMLTree const & source, Session & session, XMLNode const & node);

	virtual std::string get_info () const;

  protected:
	bool _prepare_move ();
	void _cancel_move ();
	void _move ();

  private:
	XMLNode xml_tempo_map;
};

} // namespace ARDOUR

#endif
