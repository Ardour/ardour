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
	Glib::ustring comment () const;
	Glib::ustring copyright () const;
	Glib::ustring isrc () const;
	uint32_t year () const;

	Glib::ustring grouping () const;
	Glib::ustring title () const;
	Glib::ustring subtitle () const;
	
	Glib::ustring artist () const;
	Glib::ustring album_artist () const;
	Glib::ustring lyricist () const;
	Glib::ustring composer () const;
	Glib::ustring conductor () const;
	Glib::ustring remixer () const;
	Glib::ustring arranger () const;
	Glib::ustring engineer () const;
	Glib::ustring producer () const;
	Glib::ustring dj_mixer () const;
	Glib::ustring mixer () const;
	
	Glib::ustring album () const;
	Glib::ustring compilation () const;
	Glib::ustring disc_subtitle () const;
	uint32_t disc_number () const;
	uint32_t total_discs () const;
	uint32_t track_number () const;
	uint32_t total_tracks () const;
	
	Glib::ustring genre () const;
	
	/*** Editing ***/
	void set_comment (const Glib::ustring &);
	void set_copyright (const Glib::ustring &);
	void set_isrc (const Glib::ustring &);
	void set_year (uint32_t);
	
	void set_grouping (const Glib::ustring &);
	void set_title (const Glib::ustring &);
	void set_subtitle (const Glib::ustring &);
	
	void set_artist (const Glib::ustring &);
	void set_album_artist (const Glib::ustring &);
	void set_lyricist (const Glib::ustring &);
	void set_composer (const Glib::ustring &);
	void set_conductor (const Glib::ustring &);
	void set_remixer (const Glib::ustring &);
	void set_arranger (const Glib::ustring &);
	void set_engineer (const Glib::ustring &);
	void set_producer (const Glib::ustring &);
	void set_dj_mixer (const Glib::ustring &);
	void set_mixer (const Glib::ustring &);
	
	void set_album (const Glib::ustring &);
	void set_compilation (const Glib::ustring &);
	void set_disc_subtitle (const Glib::ustring &);
	void set_disc_number (uint32_t);
	void set_total_discs (uint32_t);
	void set_track_number (uint32_t);
	void set_total_tracks (uint32_t);
	
	void set_genre (const Glib::ustring &);
	
	/*** Serialization ***/
	XMLNode & get_state ();
	int set_state (const XMLNode &);

  private:
	
	typedef std::pair<Glib::ustring, Glib::ustring> Property;
	typedef std::map<Glib::ustring, Glib::ustring> PropertyMap;
	PropertyMap map;

	XMLNode * get_xml (const Glib::ustring & name);
	
	Glib::ustring get_value (const Glib::ustring & name) const;
	uint32_t get_uint_value (const Glib::ustring & name) const;
	
	void set_value (const Glib::ustring & name, const Glib::ustring & value);
	void set_value (const Glib::ustring & name, uint32_t value);
};

} // namespace ARDOUR

#endif // __ardour_session_metadata_h__
