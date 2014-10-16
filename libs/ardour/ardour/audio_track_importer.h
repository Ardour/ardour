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

#ifndef __ardour_audio_track_importer_h__
#define __ardour_audio_track_importer_h__

#include <list>

#include "pbd/xml++.h"
#include "pbd/id.h"

#include "ardour/element_importer.h"
#include "ardour/element_import_handler.h"

namespace ARDOUR {

class AudioPlaylistImportHandler;
class AudioPlaylistImporter;

class LIBARDOUR_API AudioTrackImportHandler : public ElementImportHandler
{
  public:
	AudioTrackImportHandler (XMLTree const & source, Session & session, AudioPlaylistImportHandler & pl_handler);
	virtual ~AudioTrackImportHandler () {}
	virtual std::string get_info () const;

  private:
	AudioPlaylistImportHandler & pl_handler;
};


class LIBARDOUR_API AudioTrackImporter : public ElementImporter
{
  public:
	AudioTrackImporter (XMLTree const & source,
	                    Session & session,
	                    AudioTrackImportHandler & track_handler,
	                    XMLNode const & node,
	                    AudioPlaylistImportHandler & pl_handler);
	~AudioTrackImporter ();

	std::string get_info () const;

  protected:
	bool _prepare_move ();
	void _cancel_move ();
	void _move ();

  private:

	typedef boost::shared_ptr<AudioPlaylistImporter> PlaylistPtr;
	typedef std::list<PlaylistPtr> PlaylistList;

	bool parse_route_xml ();
	bool parse_io ();

	bool parse_processor (XMLNode & node);
	bool parse_controllable (XMLNode & node);
	bool parse_automation (XMLNode & node);
	bool rate_convert_events (XMLNode & node);

	AudioTrackImportHandler & track_handler;
	XMLNode xml_track;

	PBD::ID old_ds_id;
	PBD::ID new_ds_id;

	PlaylistList playlists;
	AudioPlaylistImportHandler & pl_handler;
};

} // namespace ARDOUR

#endif
