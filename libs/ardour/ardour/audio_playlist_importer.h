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

#ifndef __ardour_audio_playlist_importer_h__
#define __ardour_audio_playlist_importer_h__

#include <list>

#include <boost/shared_ptr.hpp>

#include "pbd/xml++.h"
#include "pbd/id.h"

#include "ardour/element_importer.h"
#include "ardour/element_import_handler.h"

namespace ARDOUR {

class AudioRegionImportHandler;
class AudioRegionImporter;
class AudioPlaylistImporter;
class Session;

class LIBARDOUR_API AudioPlaylistImportHandler : public ElementImportHandler
{
  public:
	typedef boost::shared_ptr<AudioPlaylistImporter> PlaylistPtr;
	typedef std::list<PlaylistPtr> PlaylistList;

	AudioPlaylistImportHandler (XMLTree const & source, Session & session, AudioRegionImportHandler & region_handler, const char * nodename = "Playlists");
	virtual ~AudioPlaylistImportHandler () {}
	virtual std::string get_info () const;

	void get_regions (XMLNode const & node, ElementList & list) const;
	void update_region_id (XMLProperty* id_prop);

	void playlists_by_diskstream (PBD::ID const & id, PlaylistList & list) const;

  protected:
	AudioRegionImportHandler & region_handler;
};

class LIBARDOUR_API UnusedAudioPlaylistImportHandler : public AudioPlaylistImportHandler
{
  public:
	UnusedAudioPlaylistImportHandler (XMLTree const & source, Session & session, AudioRegionImportHandler & region_handler) :
		AudioPlaylistImportHandler (source, session, region_handler, "UnusedPlaylists") { }
	std::string get_info () const;
};

class LIBARDOUR_API AudioPlaylistImporter : public ElementImporter
{
  public:
	AudioPlaylistImporter (XMLTree const & source, Session & session, AudioPlaylistImportHandler & handler, XMLNode const & node);
	AudioPlaylistImporter (AudioPlaylistImporter const & other);
	~AudioPlaylistImporter ();

	std::string get_info () const;

	void set_diskstream (PBD::ID const & id);
	PBD::ID const & orig_diskstream () const { return orig_diskstream_id; }

  protected:
	bool _prepare_move ();
	void _cancel_move ();
	void _move ();

  private:
	typedef std::list<boost::shared_ptr<AudioRegionImporter> > RegionList;

	void populate_region_list ();

	AudioPlaylistImportHandler & handler;
	XMLNode const & orig_node;
	XMLNode xml_playlist;
	PBD::ID orig_diskstream_id;
	PBD::ID diskstream_id;
	RegionList regions;
};

} // namespace ARDOUR

#endif
