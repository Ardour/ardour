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

#include <pbd/xml++.h>

#include <ardour/element_importer.h>
#include <ardour/element_import_handler.h>

namespace ARDOUR {


class AudioTrackImportHandler : public ElementImportHandler
{
  public:
	AudioTrackImportHandler (XMLTree const & source, Session & session);
	virtual ~AudioTrackImportHandler () {}
	virtual string get_info () const;
};


class AudioTrackImporter : public ElementImporter
{
  public:
	AudioTrackImporter (XMLTree const & source, Session & session, AudioTrackImportHandler & handler, XMLNode const & node);

	string get_info () const;
	bool prepare_move ();
	void cancel_move ();
	void move ();

  private:

	bool parse_io (XMLNode const & node);
	bool parse_processor (XMLNode const & node);
	
	bool parse_controllable (XMLNode const & node, XMLNode & dest_parent);
	
	XMLNode xml_track;
	
};

} // namespace ARDOUR

#endif
