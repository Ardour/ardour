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

#include <pbd/xml++.h>
#include <pbd/id.h>

#include <ardour/element_importer.h>
#include <ardour/element_import_handler.h>
#include <ardour/types.h>

#include "i18n.h"

namespace ARDOUR {

class AudioRegionImportHandler;
class AudioRegionImporter;

class AudioPlaylistImportHandler : public ElementImportHandler
{
  public:
	AudioPlaylistImportHandler (XMLTree const & source, Session & session, AudioRegionImportHandler & region_handler, const char * nodename = "Playlists");
	virtual ~AudioPlaylistImportHandler () {}
	virtual string get_info () const;
	
	void get_regions (XMLNode const & node, ElementList & list);
	void update_region_id (XMLProperty* id_prop);
	
  protected:
	AudioRegionImportHandler & region_handler;
};

class UnusedAudioPlaylistImportHandler : public AudioPlaylistImportHandler
{
  public:
	UnusedAudioPlaylistImportHandler (XMLTree const & source, Session & session, AudioRegionImportHandler & region_handler) :
		AudioPlaylistImportHandler (source, session, region_handler, X_("UnusedPlaylists")) { }
	string get_info () const { return _("Audio Playlists (unused)"); }
};

class AudioPlaylistImporter : public ElementImporter
{
  public:
	AudioPlaylistImporter (XMLTree const & source, Session & session, AudioPlaylistImportHandler & handler, XMLNode const & node);

	string get_info () const;
	bool prepare_move ();
	void cancel_move ();
	void move ();
	
	void set_diskstream (PBD::ID const & id);

  private:
	typedef std::list<boost::shared_ptr<AudioRegionImporter> > RegionList;

	AudioPlaylistImportHandler & handler;
	XMLNode xml_playlist;
	PBD::ID diskstream_id;
	RegionList regions;
};

} // namespace ARDOUR

#endif
