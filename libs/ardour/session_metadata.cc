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

#include "ardour/session_metadata.h"

#include <iostream>
#include <sstream>

using namespace std;
using namespace Glib;
using namespace ARDOUR;

SessionMetadata::SessionMetadata ()
{
	/*** General ***/
	map.insert (Property ("comment", ""));
	map.insert (Property ("copyright", ""));
	map.insert (Property ("isrc", ""));
	map.insert (Property ("year", ""));

	/*** Title and friends ***/
	map.insert (Property ("grouping", ""));
	map.insert (Property ("title", ""));
	map.insert (Property ("subtitle", ""));

	/*** People... ***/
	map.insert (Property ("artist", ""));
	map.insert (Property ("album_artist", ""));
	map.insert (Property ("lyricist", ""));
	map.insert (Property ("composer", ""));
	map.insert (Property ("conductor", ""));
	map.insert (Property ("remixer", ""));
	map.insert (Property ("arranger", ""));
	map.insert (Property ("engineer", ""));
	map.insert (Property ("producer", ""));
	map.insert (Property ("dj_mixer", ""));
	map.insert (Property ("mixer", ""));
	//map.insert (Property ("performers", "")); // Multiple values [instrument]

	/*** Album info ***/
	map.insert (Property ("album", ""));
	map.insert (Property ("compilation", ""));
	map.insert (Property ("disc_subtitle", ""));
	map.insert (Property ("disc_number", ""));
	map.insert (Property ("total_discs", ""));
	map.insert (Property ("track_number", ""));
	map.insert (Property ("total_tracks", ""));

	/*** Style ***/
	map.insert (Property ("genre", ""));
	//map.insert (Property ("mood", ""));
	//map.insert (Property ("bpm", ""));

	/*** Other ***/
	//map.insert (Property ("lyrics", ""));
	//map.insert (Property ("media", ""));
	//map.insert (Property ("label", ""));
	//map.insert (Property ("barcode", ""));
	//map.insert (Property ("encoded_by", ""));
	//map.insert (Property ("catalog_number", ""));

	/*** Sorting orders ***/
	//map.insert (Property ("album_sort", ""));
	//map.insert (Property ("album_artist_sort", ""));
	//map.insert (Property ("artist_sort", ""));
	//map.insert (Property ("title_sort", ""));
}

SessionMetadata::~SessionMetadata ()
{

}

XMLNode *
SessionMetadata::get_xml (const ustring & name)
{
	ustring value = get_value (name);
	if (value.empty()) {
		return 0;
	}

	XMLNode val ("value", value);
	XMLNode * node = new XMLNode (name);
	node->add_child_copy (val);

	return node;
}

ustring
SessionMetadata::get_value (const ustring & name) const
{
	PropertyMap::const_iterator it = map.find (name);
	if (it == map.end()) {
		// Should not be reached!
		std::cerr << "Programming error in SessionMetadata::get_value" << std::endl;
		return "";
	}

	return it->second;
}

uint32_t
SessionMetadata::get_uint_value (const ustring & name) const
{
	return atoi (get_value (name).c_str());
}

void
SessionMetadata::set_value (const ustring & name, const ustring & value)
{
	PropertyMap::iterator it = map.find (name);
	if (it == map.end()) {
		// Should not be reached!
		std::cerr << "Programming error in SessionMetadata::set_value" << std::endl;
		return;
	}

	it->second = value;
}

void
SessionMetadata::set_value (const ustring & name, uint32_t value)
{
	std::ostringstream oss;
	oss << value;
	if (oss.str().compare("0")) {
		set_value (name, oss.str());
	} else {
		set_value (name, "");
	}
}

/*** Serialization ***/
XMLNode &
SessionMetadata::get_state ()
{
	XMLNode * node = new XMLNode ("Metadata");
	XMLNode * prop;

	for (PropertyMap::const_iterator it = map.begin(); it != map.end(); ++it) {
		if ((prop = get_xml (it->first))) {
			node->add_child_nocopy (*prop);
		}
	}

	return *node;
}

int
SessionMetadata::set_state (const XMLNode & state, int version)
{
	const XMLNodeList & children = state.children();
	ustring name;
	ustring value;
	XMLNode * node;

	for (XMLNodeConstIterator it = children.begin(); it != children.end(); it++) {
		node = *it;
		if (node->children().empty()) {
			continue;
		}

		name = node->name();
		node = *node->children().begin();
		value = node->content();

		set_value (name, value);
	}

	return 0;
}

/*** Accessing ***/
ustring
SessionMetadata::comment () const
{
	return get_value("comment");
}

ustring
SessionMetadata::copyright () const
{
	return get_value("copyright");
}

ustring
SessionMetadata::isrc () const
{
	return get_value("isrc");
}

uint32_t
SessionMetadata::year () const
{
	return get_uint_value("year");
}

ustring
SessionMetadata::grouping () const
{
	return get_value("grouping");
}

ustring
SessionMetadata::title () const
{
	return get_value("title");
}

ustring
SessionMetadata::subtitle () const
{
	return get_value("subtitle");
}

ustring
SessionMetadata::artist () const
{
	return get_value("artist");
}

ustring
SessionMetadata::album_artist () const
{
	return get_value("album_artist");
}

ustring
SessionMetadata::lyricist () const
{
	return get_value("lyricist");
}

ustring
SessionMetadata::composer () const
{
	return get_value("composer");
}

ustring
SessionMetadata::conductor () const
{
	return get_value("conductor");
}

ustring
SessionMetadata::remixer () const
{
	return get_value("remixer");
}

ustring
SessionMetadata::arranger () const
{
	return get_value("arranger");
}

ustring
SessionMetadata::engineer () const
{
	return get_value("engineer");
}

ustring
SessionMetadata::producer () const
{
	return get_value("producer");
}

ustring
SessionMetadata::dj_mixer () const
{
	return get_value("dj_mixer");
}

ustring
SessionMetadata::mixer () const
{
	return get_value("mixer");
}

ustring
SessionMetadata::album () const
{
	return get_value("album");
}

ustring
SessionMetadata::compilation () const
{
	return get_value("compilation");
}

ustring
SessionMetadata::disc_subtitle () const
{
	return get_value("disc_subtitle");
}

uint32_t
SessionMetadata::disc_number () const
{
	return get_uint_value("disc_number");
}

uint32_t
SessionMetadata::total_discs () const
{
	return get_uint_value("total_discs");
}

uint32_t
SessionMetadata::track_number () const
{
	return get_uint_value("track_number");
}

uint32_t
SessionMetadata::total_tracks () const
{
	return get_uint_value("total_tracks");
}

ustring
SessionMetadata::genre () const
{
	return get_value("genre");
}

/*** Editing ***/
void
SessionMetadata::set_comment (const ustring & v)
{
	set_value ("comment", v);
}

void
SessionMetadata::set_copyright (const ustring & v)
{
	set_value ("copyright", v);
}

void
SessionMetadata::set_isrc (const ustring & v)
{
	set_value ("isrc", v);
}

void
SessionMetadata::set_year (uint32_t v)
{
	set_value ("year", v);
}

void
SessionMetadata::set_grouping (const ustring & v)
{
	set_value ("grouping", v);
}

void
SessionMetadata::set_title (const ustring & v)
{
	set_value ("title", v);
}

void
SessionMetadata::set_subtitle (const ustring & v)
{
	set_value ("subtitle", v);
}

void
SessionMetadata::set_artist (const ustring & v)
{
	set_value ("artist", v);
}

void
SessionMetadata::set_album_artist (const ustring & v)
{
	set_value ("album_artist", v);
}

void
SessionMetadata::set_lyricist (const ustring & v)
{
	set_value ("lyricist", v);
}

void
SessionMetadata::set_composer (const ustring & v)
{
	set_value ("composer", v);
}

void
SessionMetadata::set_conductor (const ustring & v)
{
	set_value ("conductor", v);
}

void
SessionMetadata::set_remixer (const ustring & v)
{
	set_value ("remixer", v);
}

void
SessionMetadata::set_arranger (const ustring & v)
{
	set_value ("arranger", v);
}

void
SessionMetadata::set_engineer (const ustring & v)
{
	set_value ("engineer", v);
}

void
SessionMetadata::set_producer (const ustring & v)
{
	set_value ("producer", v);
}

void
SessionMetadata::set_dj_mixer (const ustring & v)
{
	set_value ("dj_mixer", v);
}

void
SessionMetadata::set_mixer (const ustring & v)
{
	set_value ("mixer", v);
}

void
SessionMetadata::set_album (const ustring & v)
{
	set_value ("album", v);
}

void
SessionMetadata::set_compilation (const ustring & v)
{
	set_value ("compilation", v);
}

void
SessionMetadata::set_disc_subtitle (const ustring & v)
{
	set_value ("disc_subtitle", v);
}

void
SessionMetadata::set_disc_number (uint32_t v)
{
	set_value ("disc_number", v);
}

void
SessionMetadata::set_total_discs (uint32_t v)
{
	set_value ("total_discs", v);
}

void
SessionMetadata::set_track_number (uint32_t v)
{
	set_value ("track_number", v);
}

void
SessionMetadata::set_total_tracks (uint32_t v)
{
	set_value ("total_tracks", v);
}

void
SessionMetadata::set_genre (const ustring & v)
{
	set_value ("genre", v);
}
