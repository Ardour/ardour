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

#ifndef __ardour_audio_region_importer_h__
#define __ardour_audio_region_importer_h__

#include <list>
#include <map>
#include <utility>

#include <boost/shared_ptr.hpp>

#include "pbd/xml++.h"
#include "pbd/id.h"
#include "ardour/element_importer.h"
#include "ardour/element_import_handler.h"
#include "ardour/import_status.h"

namespace ARDOUR {

class Region;
class Session;
class Source;

class LIBARDOUR_API AudioRegionImportHandler : public ElementImportHandler
{
  public:
	// Inerface implementation
	AudioRegionImportHandler (XMLTree const & source, Session & session);
	std::string get_info () const;

	void create_regions_from_children (XMLNode const & node, ElementList & list);

	// Source management
	bool check_source (std::string const & filename) const;
	void add_source (std::string const & filename, boost::shared_ptr<Source> const & source);
	boost::shared_ptr<Source> const & get_source (std::string const & filename) const;

	// Id management
	void register_id (PBD::ID & old_id, PBD::ID & new_id);
	PBD::ID const & get_new_id (PBD::ID & old_id) const;

  private:
	// Source management
	typedef std::map<std::string, boost::shared_ptr<Source> > SourceMap;
	typedef std::pair<std::string, boost::shared_ptr<Source> > SourcePair;
	SourceMap sources;

	// Id management
	typedef std::map<PBD::ID, PBD::ID> IdMap;
	typedef std::pair<PBD::ID, PBD::ID> IdPair;
	IdMap id_map;
};

class LIBARDOUR_API AudioRegionImporter : public ElementImporter
{
  public:
	AudioRegionImporter (XMLTree const & source, Session & session, AudioRegionImportHandler & handler, XMLNode const & node);
	~AudioRegionImporter ();

	// Interface implementation
	std::string get_info () const;
	ImportStatus * get_import_status () { return &status; }

	// other stuff
	void add_sources_to_session ();
	XMLNode const & get_xml ();

  protected:
	bool _prepare_move ();
	void _cancel_move ();
	void _move ();

  private:

	XMLNode xml_region;
	AudioRegionImportHandler & handler;
	PBD::ID old_id;
	PBD::ID id;
	std::list<std::string> filenames;
	ImportStatus status;

	bool parse_xml_region ();
	bool parse_source_xml ();
	std::string get_sound_dir (XMLTree const & tree);

	void prepare_region ();
	void prepare_sources ();
	std::vector<boost::shared_ptr<Region> > region;
	bool region_prepared;
	bool sources_prepared;
};

} // namespace ARDOUR

#endif
