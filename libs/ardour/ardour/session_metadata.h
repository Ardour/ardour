/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_session_metadata_h__
#define __ardour_session_metadata_h__

#include <string>

#include <map>
#include <utility>

#include "pbd/statefuldestructible.h"
#include "pbd/xml++.h"

namespace ARDOUR {

/** Represents metadata associated to a Session
 * Metadata can be accessed and edited via this class.
 * Exported files can also be tagged with this data.
 */
class SessionMetadata : public PBD::StatefulDestructible
{
  public:
	SessionMetadata ();
	~SessionMetadata ();

	/*** Accessing ***/
	std::string comment () const;
	std::string copyright () const;
	std::string isrc () const;
	uint32_t year () const;

	std::string grouping () const;
	std::string title () const;
	std::string subtitle () const;

	std::string artist () const;
	std::string album_artist () const;
	std::string lyricist () const;
	std::string composer () const;
	std::string conductor () const;
	std::string remixer () const;
	std::string arranger () const;
	std::string engineer () const;
	std::string producer () const;
	std::string dj_mixer () const;
	std::string mixer () const;

	std::string album () const;
	std::string compilation () const;
	std::string disc_subtitle () const;
	uint32_t disc_number () const;
	uint32_t total_discs () const;
	uint32_t track_number () const;
	uint32_t total_tracks () const;

	std::string genre () const;

	/*** Editing ***/
	void set_comment (const std::string &);
	void set_copyright (const std::string &);
	void set_isrc (const std::string &);
	void set_year (uint32_t);

	void set_grouping (const std::string &);
	void set_title (const std::string &);
	void set_subtitle (const std::string &);

	void set_artist (const std::string &);
	void set_album_artist (const std::string &);
	void set_lyricist (const std::string &);
	void set_composer (const std::string &);
	void set_conductor (const std::string &);
	void set_remixer (const std::string &);
	void set_arranger (const std::string &);
	void set_engineer (const std::string &);
	void set_producer (const std::string &);
	void set_dj_mixer (const std::string &);
	void set_mixer (const std::string &);

	void set_album (const std::string &);
	void set_compilation (const std::string &);
	void set_disc_subtitle (const std::string &);
	void set_disc_number (uint32_t);
	void set_total_discs (uint32_t);
	void set_track_number (uint32_t);
	void set_total_tracks (uint32_t);

	void set_genre (const std::string &);

	/*** Serialization ***/
	XMLNode & get_state ();
	int set_state (const XMLNode &, int version);

  private:

	typedef std::pair<std::string, std::string> Property;
	typedef std::map<std::string, std::string> PropertyMap;
	PropertyMap map;

	XMLNode * get_xml (const std::string & name);

	std::string get_value (const std::string & name) const;
	uint32_t get_uint_value (const std::string & name) const;

	void set_value (const std::string & name, const std::string & value);
	void set_value (const std::string & name, uint32_t value);
};

} // namespace ARDOUR

#endif // __ardour_session_metadata_h__
