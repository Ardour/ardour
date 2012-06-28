/*
    Copyright (C) 2003-2006 Paul Davis

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

#include <sstream>

#include <libxml/uri.h>

#include <lrdf.h>
#include <glibmm/miscutils.h>

#include <glibmm/convert.h>

#include "pbd/compose.h"

#include "ardour/audio_library.h"
#include "ardour/utils.h"
#include "ardour/filesystem_paths.h"

#include "i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

namespace {
	const char* const sfdb_file_name = "sfdb";
} // anonymous namespace

static const char* TAG = "http://ardour.org/ontology/Tag";

AudioLibrary::AudioLibrary ()
{
	std::string sfdb_file_path(user_config_directory ());

	sfdb_file_path = Glib::build_filename (sfdb_file_path, sfdb_file_name);

	src = Glib::filename_to_uri (sfdb_file_path);

	// workaround for possible bug in raptor that crashes when saving to a
	// non-existant file.

	touch_file(sfdb_file_path);

	lrdf_read_file(src.c_str());
}

AudioLibrary::~AudioLibrary ()
{
}

void
AudioLibrary::save_changes ()
{
	if (lrdf_export_by_source(src.c_str(), src.substr(5).c_str())) {
		PBD::warning << string_compose(_("Could not open %1.  Audio Library not saved"), src) << endmsg;
	}
}

void
AudioLibrary::set_tags (string member, vector<string> tags)
{
	sort (tags.begin(), tags.end());
	tags.erase (unique(tags.begin(), tags.end()), tags.end());

	const string file_uri(Glib::filename_to_uri (member));

	lrdf_remove_uri_matches (file_uri.c_str());

	for (vector<string>::iterator i = tags.begin(); i != tags.end(); ++i) {
		lrdf_add_triple (src.c_str(), file_uri.c_str(), TAG, (*i).c_str(), lrdf_literal);
	}
}

vector<string>
AudioLibrary::get_tags (string member)
{
	vector<string> tags;

	lrdf_statement pattern;
	pattern.subject = strdup(Glib::filename_to_uri(member).c_str());
	pattern.predicate = (char*)TAG;
	pattern.object = 0;
	pattern.object_type = lrdf_literal;

	lrdf_statement* matches = lrdf_matches (&pattern);
	free (pattern.subject);

	lrdf_statement* current = matches;
	while (current != 0) {
		tags.push_back (current->object);

		current = current->next;
	}

	lrdf_free_statements (matches);

	sort (tags.begin(), tags.end());

	return tags;
}

void
AudioLibrary::search_members_and (vector<string>& members, const vector<string>& tags)
{
	lrdf_statement **head;
	lrdf_statement* pattern = 0;
	lrdf_statement* old = 0;
	head = &pattern;

	vector<string>::const_iterator i;
	for (i = tags.begin(); i != tags.end(); ++i){
		pattern = new lrdf_statement;
		pattern->subject = (char*)"?";
		pattern->predicate = (char*)TAG;
		pattern->object = strdup((*i).c_str());
		pattern->next = old;

		old = pattern;
	}

	if (*head != 0) {
		lrdf_uris* ulist = lrdf_match_multi(*head);
		for (uint32_t j = 0; ulist && j < ulist->count; ++j) {
			members.push_back(Glib::filename_from_uri(ulist->items[j]));
		}
		lrdf_free_uris(ulist);

	    sort(members.begin(), members.end());
	    unique(members.begin(), members.end());
	}

	// memory clean up
	pattern = *head;
	while(pattern){
		free(pattern->object);
		old = pattern;
		pattern = pattern->next;
		delete old;
	}
}
