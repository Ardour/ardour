/*
 * Copyright (C) 2005-2007 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2007-2010 David Robillard <d@drobilla.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <sstream>

#include <libxml/uri.h>

#ifdef HAVE_LRDF
#include <lrdf.h>
#endif

#include <glibmm/miscutils.h>

#include <glibmm/convert.h>

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/file_utils.h"

#include "ardour/audio_library.h"
#include "ardour/filesystem_paths.h"

#include "pbd/i18n.h"

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

#ifdef HAVE_LRDF
	lrdf_read_file(src.c_str());
#endif
}

AudioLibrary::~AudioLibrary ()
{
}

void
AudioLibrary::save_changes ()
{
#ifdef HAVE_LRDF
#ifdef PLATFORM_WINDOWS
	string path = Glib::locale_from_utf8 (Glib::filename_from_uri(src));
#else
	string path = Glib::filename_from_uri(src);
#endif
	if (lrdf_export_by_source(src.c_str(), path.c_str())) {
		PBD::warning << string_compose(_("Could not open %1.  Audio Library not saved"), path) << endmsg;
	}
#endif
}

void
AudioLibrary::set_tags (string member, vector<string> tags)
{
#ifdef HAVE_LRDF
	sort (tags.begin(), tags.end());
	tags.erase (unique(tags.begin(), tags.end()), tags.end());

	const string file_uri(Glib::filename_to_uri (member));

	lrdf_remove_uri_matches (file_uri.c_str());

	for (vector<string>::iterator i = tags.begin(); i != tags.end(); ++i) {
		lrdf_add_triple (src.c_str(), file_uri.c_str(), TAG, (*i).c_str(), lrdf_literal);
	}
#endif
}

vector<string>
AudioLibrary::get_tags (string member)
{
	vector<string> tags;
#ifdef HAVE_LRDF
	char * uri = strdup(Glib::filename_to_uri(member).c_str());

	lrdf_statement pattern;
	pattern.subject = uri;
	pattern.predicate = const_cast<char*>(TAG);
	pattern.object = 0;
	pattern.object_type = lrdf_literal;

	lrdf_statement* matches = lrdf_matches (&pattern);

	lrdf_statement* current = matches;
	while (current != 0) {
		tags.push_back (current->object);

		current = current->next;
	}

	lrdf_free_statements (matches);

	sort (tags.begin(), tags.end());
	free (uri);
#endif
	return tags;
}

void
AudioLibrary::search_members_and (vector<string>& members, const vector<string>& tags)
{
#ifdef HAVE_LRDF
	lrdf_statement **head;
	lrdf_statement* pattern = 0;
	lrdf_statement* old = 0;
	head = &pattern;

	vector<string>::const_iterator i;
	for (i = tags.begin(); i != tags.end(); ++i){
		pattern = new lrdf_statement;
		pattern->subject = const_cast<char*>("?");
		pattern->predicate = const_cast<char*>(TAG);
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

	    sort (members.begin(), members.end());
	    members.erase (unique (members.begin(), members.end()), members.end());
	}

	// memory clean up
	pattern = *head;
	while(pattern){
		free(pattern->object);
		old = pattern;
		pattern = pattern->next;
		delete old;
	}
#endif
}
