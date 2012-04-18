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

SessionMetadata *SessionMetadata::_metadata = NULL;  //singleton instance

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

	/*** Education... ***/
	map.insert (Property ("instructor", ""));
	map.insert (Property ("course", ""));

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
	//map.insert (Property ("title_sort", ""));\
	
	/*** Globals ***/
	user_map.insert (Property ("user_name", ""));
	user_map.insert (Property ("user_email", ""));
	user_map.insert (Property ("user_web", ""));
	user_map.insert (Property ("user_organization", ""));
	user_map.insert (Property ("user_country", ""));
}

SessionMetadata::~SessionMetadata ()
{

}

XMLNode *
SessionMetadata::get_xml (const string & name)
{
	string value = get_value (name);
	if (value.empty()) {
		return 0;
	}

	XMLNode val ("value", value);
	XMLNode * node = new XMLNode (name);
	node->add_child_copy (val);

	return node;
}

string
SessionMetadata::get_value (const string & name) const
{
	PropertyMap::const_iterator it = map.find (name);
	if (it == map.end()) {
		it = user_map.find (name);
		if (it == user_map.end()) {
			// Should not be reached!
			std::cerr << "Programming error in SessionMetadata::get_value" << std::endl;
			return "";
		}
	}

	return it->second;
}

uint32_t
SessionMetadata::get_uint_value (const string & name) const
{
	return atoi (get_value (name).c_str());
}

void
SessionMetadata::set_value (const string & name, const string & value)
{
	PropertyMap::iterator it = map.find (name);
	if (it == map.end()) {
		it = user_map.find (name);
		if (it == user_map.end()) {
			// Should not be reached!
			std::cerr << "Programming error in SessionMetadata::set_value" << std::endl;
			return;
		}
	}

	it->second = value;
}

void
SessionMetadata::set_value (const string & name, uint32_t value)
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
SessionMetadata::set_state (const XMLNode & state, int version_num)
{
	const XMLNodeList & children = state.children();
	string name;
	string value;
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


XMLNode &
SessionMetadata::get_user_state ()
{
	XMLNode * node = new XMLNode ("Metadata");
	XMLNode * prop;

	for (PropertyMap::const_iterator it = user_map.begin(); it != user_map.end(); ++it) {
		if ((prop = get_xml (it->first))) {
			node->add_child_nocopy (*prop);
		}
	}

	return *node;
}

/*** Accessing ***/
string
SessionMetadata::comment () const
{
	return get_value("comment");
}

string
SessionMetadata::copyright () const
{
	return get_value("copyright");
}

string
SessionMetadata::isrc () const
{
	return get_value("isrc");
}

uint32_t
SessionMetadata::year () const
{
	return get_uint_value("year");
}

string
SessionMetadata::grouping () const
{
	return get_value("grouping");
}

string
SessionMetadata::title () const
{
	return get_value("title");
}

string
SessionMetadata::subtitle () const
{
	return get_value("subtitle");
}

string
SessionMetadata::artist () const
{
	return get_value("artist");
}

string
SessionMetadata::album_artist () const
{
	return get_value("album_artist");
}

string
SessionMetadata::lyricist () const
{
	return get_value("lyricist");
}

string
SessionMetadata::composer () const
{
	return get_value("composer");
}

string
SessionMetadata::conductor () const
{
	return get_value("conductor");
}

string
SessionMetadata::remixer () const
{
	return get_value("remixer");
}

string
SessionMetadata::arranger () const
{
	return get_value("arranger");
}

string
SessionMetadata::engineer () const
{
	return get_value("engineer");
}

string
SessionMetadata::producer () const
{
	return get_value("producer");
}

string
SessionMetadata::dj_mixer () const
{
	return get_value("dj_mixer");
}

string
SessionMetadata::mixer () const
{
	return get_value("mixer");
}

string
SessionMetadata::album () const
{
	return get_value("album");
}

string
SessionMetadata::compilation () const
{
	return get_value("compilation");
}

string
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

string
SessionMetadata::genre () const
{
	return get_value("genre");
}

string
SessionMetadata::instructor () const
{
	return get_value("instructor");
}

string
SessionMetadata::course () const
{
	return get_value("course");
}


string
SessionMetadata::user_name () const
{
	return get_value("user_name");
}

string
SessionMetadata::user_email () const
{
	return get_value("user_email");
}

string
SessionMetadata::user_web () const
{
	return get_value("user_web");
}

string
SessionMetadata::organization () const
{
	return get_value("user_organization");
}

string
SessionMetadata::country () const
{
	return get_value("user_country");
}



/*** Editing ***/
void
SessionMetadata::set_comment (const string & v)
{
	set_value ("comment", v);
}

void
SessionMetadata::set_copyright (const string & v)
{
	set_value ("copyright", v);
}

void
SessionMetadata::set_isrc (const string & v)
{
	set_value ("isrc", v);
}

void
SessionMetadata::set_year (uint32_t v)
{
	set_value ("year", v);
}

void
SessionMetadata::set_grouping (const string & v)
{
	set_value ("grouping", v);
}

void
SessionMetadata::set_title (const string & v)
{
	set_value ("title", v);
}

void
SessionMetadata::set_subtitle (const string & v)
{
	set_value ("subtitle", v);
}

void
SessionMetadata::set_artist (const string & v)
{
	set_value ("artist", v);
}

void
SessionMetadata::set_album_artist (const string & v)
{
	set_value ("album_artist", v);
}

void
SessionMetadata::set_lyricist (const string & v)
{
	set_value ("lyricist", v);
}

void
SessionMetadata::set_composer (const string & v)
{
	set_value ("composer", v);
}

void
SessionMetadata::set_conductor (const string & v)
{
	set_value ("conductor", v);
}

void
SessionMetadata::set_remixer (const string & v)
{
	set_value ("remixer", v);
}

void
SessionMetadata::set_arranger (const string & v)
{
	set_value ("arranger", v);
}

void
SessionMetadata::set_engineer (const string & v)
{
	set_value ("engineer", v);
}

void
SessionMetadata::set_producer (const string & v)
{
	set_value ("producer", v);
}

void
SessionMetadata::set_dj_mixer (const string & v)
{
	set_value ("dj_mixer", v);
}

void
SessionMetadata::set_mixer (const string & v)
{
	set_value ("mixer", v);
}

void
SessionMetadata::set_album (const string & v)
{
	set_value ("album", v);
}

void
SessionMetadata::set_compilation (const string & v)
{
	set_value ("compilation", v);
}

void
SessionMetadata::set_disc_subtitle (const string & v)
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
SessionMetadata::set_genre (const string & v)
{
	set_value ("genre", v);
}

void
SessionMetadata::set_instructor (const string & v)
{
	set_value ("instructor", v);
}

void
SessionMetadata::set_course (const string & v)
{
	set_value ("course", v);
}

void
SessionMetadata::set_user_name (const string & v)
{
	set_value ("user_name", v);
}

void
SessionMetadata::set_user_email (const string & v)
{
	set_value ("user_email", v);
}

void
SessionMetadata::set_user_web (const string & v)
{
	set_value ("user_web", v);
}

void
SessionMetadata::set_organization (const string & v)
{
	set_value ("user_organization", v);
}
void
SessionMetadata::set_country (const string & v)
{
	set_value ("user_country", v);
}
