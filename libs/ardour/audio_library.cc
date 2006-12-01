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

#include <pbd/compose.h>

#include <ardour/audio_library.h>
#include <ardour/utils.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;

static char* TAG = "http://ardour.org/ontology/Tag";

AudioLibrary::AudioLibrary ()
{
	src = "file:" + get_user_ardour_path() + "sfdb";

	// workaround for possible bug in raptor that crashes when saving to a
	// non-existant file.
	touch_file(get_user_ardour_path() + "sfdb");

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

string
AudioLibrary::path2uri (string path)
{
	xmlURI temp;
	memset(&temp, 0, sizeof(temp));
	
	xmlChar *cal = xmlCanonicPath((xmlChar*) path.c_str());
	temp.path = (char *) cal;
	xmlChar *ret = xmlSaveUri(&temp);
	xmlFree(cal);
	
	stringstream uri;
	uri << "file:" << (const char*) ret;
	
	xmlFree (ret);
	
	return uri.str();
}

void
AudioLibrary::set_tags (string member, vector<string> tags)
{
	sort (tags.begin(), tags.end());
	tags.erase (unique(tags.begin(), tags.end()), tags.end());
	
	string file_uri(path2uri(member));
	
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
	pattern.subject = strdup(path2uri(member).c_str());
	pattern.predicate = TAG;
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
AudioLibrary::search_members_and (vector<string>& members, const vector<string> tags)
{
	lrdf_statement **head;
	lrdf_statement* pattern = 0;
	lrdf_statement* old = 0;
	head = &pattern;

	vector<string>::const_iterator i;
	for (i = tags.begin(); i != tags.end(); ++i){
		pattern = new lrdf_statement;
		pattern->subject = "?";
		pattern->predicate = TAG;
		pattern->object = strdup((*i).c_str());
		pattern->next = old;

		old = pattern;
	}

	if (*head != 0) {
		lrdf_uris* ulist = lrdf_match_multi(*head);
		for (uint32_t j = 0; ulist && j < ulist->count; ++j) {
//			printf("AND: %s\n", ulist->items[j]);
			members.push_back(ulist->items[j]);
		}
		lrdf_free_uris(ulist);

	    sort(members.begin(), members.end());
	    unique(members.begin(), members.end());
	}

	// memory clean up
	pattern = *head;
	while(pattern){
		free(pattern->predicate);
		free(pattern->object);
		old = pattern;
		pattern = pattern->next;
		delete old;
	}
}

bool
AudioLibrary::safe_file_extension(string file)
{
	return !(file.rfind(".wav") == string::npos &&
		file.rfind(".aiff")== string::npos &&
		file.rfind(".aif") == string::npos &&
		file.rfind(".snd") == string::npos &&
		file.rfind(".au")  == string::npos &&
		file.rfind(".raw") == string::npos &&
		file.rfind(".sf")  == string::npos &&
		file.rfind(".cdr") == string::npos &&
		file.rfind(".smp") == string::npos &&
		file.rfind(".maud")== string::npos &&
		file.rfind(".vwe") == string::npos &&
		file.rfind(".paf") == string::npos &&
#ifdef HAVE_COREAUDIO
		file.rfind(".mp3") == string::npos &&
		file.rfind(".aac") == string::npos &&
		file.rfind(".mp4") == string::npos &&
#endif // HAVE_COREAUDIO
		file.rfind(".voc") == string::npos);
}
