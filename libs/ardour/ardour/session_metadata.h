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
#include <glibmm/ustring.h>

#include <map>
#include <utility>

#include "pbd/statefuldestructible.h"
#include "pbd/xml++.h"

namespace ARDOUR {

using std::string;
using Glib::ustring;

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
	ustring comment () const;
	ustring copyright () const;
	ustring isrc () const;
	uint32_t year () const;

	ustring grouping () const;
	ustring title () const;
	ustring subtitle () const;
	
	ustring artist () const;
	ustring album_artist () const;
	ustring lyricist () const;
	ustring composer () const;
	ustring conductor () const;
	ustring remixer () const;
	ustring arranger () const;
	ustring engineer () const;
	ustring producer () const;
	ustring dj_mixer () const;
	ustring mixer () const;
	
	ustring album () const;
	ustring compilation () const;
	ustring disc_subtitle () const;
	uint32_t disc_number () const;
	uint32_t total_discs () const;
	uint32_t track_number () const;
	uint32_t total_tracks () const;
	
	ustring genre () const;
	
	/*** Editing ***/
	void set_comment (const ustring &);
	void set_copyright (const ustring &);
	void set_isrc (const ustring &);
	void set_year (uint32_t);
	
	void set_grouping (const ustring &);
	void set_title (const ustring &);
	void set_subtitle (const ustring &);
	
	void set_artist (const ustring &);
	void set_album_artist (const ustring &);
	void set_lyricist (const ustring &);
	void set_composer (const ustring &);
	void set_conductor (const ustring &);
	void set_remixer (const ustring &);
	void set_arranger (const ustring &);
	void set_engineer (const ustring &);
	void set_producer (const ustring &);
	void set_dj_mixer (const ustring &);
	void set_mixer (const ustring &);
	
	void set_album (const ustring &);
	void set_compilation (const ustring &);
	void set_disc_subtitle (const ustring &);
	void set_disc_number (uint32_t);
	void set_total_discs (uint32_t);
	void set_track_number (uint32_t);
	void set_total_tracks (uint32_t);
	
	void set_genre (const ustring &);
	
	/*** Serialization ***/
	XMLNode & get_state ();
	int set_state (const XMLNode &);

  private:
	
	typedef std::pair<ustring, ustring> Property;
	typedef std::map<ustring, ustring> PropertyMap;
	PropertyMap map;

	XMLNode * get_xml (const ustring & name);
	
	ustring get_value (const ustring & name) const;
	uint32_t get_uint_value (const ustring & name) const;
	
	void set_value (const ustring & name, const ustring & value);
	void set_value (const ustring & name, uint32_t value);
};

} // namespace ARDOUR

#endif // __ardour_session_metadata_h__
