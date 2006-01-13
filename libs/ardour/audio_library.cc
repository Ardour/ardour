/*
    Copyright (C) 2003 Paul Davis 
    Author: Taybin Rutkin

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

    $Id$
*/

#include <cstdio> // Needed so that libraptor (included in lrdf) won't complain
#include <cerrno>
#include <iostream>
#include <sstream>
#include <cctype>

#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>

#include <lrdf.h>

#include <pbd/compose.h>

#include <ardour/ardour.h>
#include <ardour/configuration.h>
#include <ardour/audio_library.h>
#include <ardour/utils.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;

static char* SOUNDFILE = "http://ardour.org/ontology/Soundfile";

AudioLibrary::AudioLibrary ()
{
//	sfdb_paths.push_back("/Users/taybin/sounds");

	src = "file:" + Config->get_user_ardour_path() + "sfdb";

	// workaround for possible bug in raptor that crashes when saving to a
	// non-existant file.
	touch_file(Config->get_user_ardour_path() + "sfdb");

	lrdf_read_file(src.c_str());

	lrdf_statement pattern;

	pattern.subject = SOUNDFILE;
	pattern.predicate = RDF_TYPE;
	pattern.object = RDFS_CLASS;
	pattern.object_type = lrdf_uri;

	lrdf_statement* matches = lrdf_matches(&pattern);

	// if empty DB, create basic schema
	if (matches == 0) {
		initialize_db ();
		save_changes();
	} 

	lrdf_free_statements(matches);

	scan_paths();
}

AudioLibrary::~AudioLibrary ()
{
}

void
AudioLibrary::initialize_db ()
{
	// define ardour:Soundfile
	lrdf_add_triple(src.c_str(), SOUNDFILE, RDF_TYPE, RDFS_CLASS, lrdf_uri);

	// add intergral fields
	add_field(_("channels"));
	add_field(_("samplerate"));
	add_field(_("resolution"));
	add_field(_("format"));
}

void
AudioLibrary::save_changes ()
{
	if (lrdf_export_by_source(src.c_str(), src.substr(5).c_str())) {
		warning << string_compose(_("Could not open %1.  Audio Library not saved"), src) << endmsg;
	}
}

void
AudioLibrary::add_member (string member)
{
	string file_uri(string_compose("file:%1", member));

	lrdf_add_triple(src.c_str(), file_uri.c_str(), RDF_TYPE, 
			SOUNDFILE, lrdf_uri);
}

void
AudioLibrary::remove_member (string uri)
{
	lrdf_remove_uri_matches (uri.c_str());
}

void
AudioLibrary::search_members_and (vector<string>& members, 
								  const map<string,string>& fields)
{
	lrdf_statement **head;
	lrdf_statement* pattern = 0;
	lrdf_statement* old = 0;
	head = &pattern;

	map<string,string>::const_iterator i;
	for (i = fields.begin(); i != fields.end(); ++i){
		pattern = new lrdf_statement;
		pattern->subject = "?";
		pattern->predicate = strdup(field_uri(i->first).c_str());
		pattern->object = strdup((i->second).c_str());
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

		compact_vector(members);
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

void
AudioLibrary::search_members_or (vector<string>& members, 
								 const map<string,string>& fields)
{
	map<string,string>::const_iterator i;

	lrdf_statement pattern;
	for (i = fields.begin(); i != fields.end(); ++i) {
		pattern.subject = 0;
		pattern.predicate = strdup(field_uri(i->first).c_str());
		pattern.object = strdup((i->second).c_str());
		pattern.object_type = lrdf_literal;

		lrdf_statement* matched = lrdf_matches(&pattern);

		lrdf_statement* old = matched;
		while(matched) {
//			printf ("OR: %s\n", matched->subject);
			members.push_back(matched->subject);
			matched = matched->next;
		}

		free(pattern.predicate);
		free(pattern.object);
		lrdf_free_statements (old);
	}

	compact_vector(members);
}

void
AudioLibrary::add_field (string name)
{
	string local_field = field_uri(name);
	lrdf_statement pattern;
	pattern.subject = strdup(local_field.c_str());
	pattern.predicate = RDF_TYPE;
	pattern.object = RDF_BASE "Property";
	pattern.object_type = lrdf_uri;

	if(lrdf_exists_match(&pattern)) {
		return;
	}

	// of type rdf:Property
	lrdf_add_triple(src.c_str(), local_field.c_str(), RDF_TYPE, 
			RDF_BASE "Property", lrdf_uri);
	// of range ardour:Soundfile
	lrdf_add_triple(src.c_str(), local_field.c_str(), RDFS_BASE "range",
			SOUNDFILE, lrdf_uri);
	// of domain rdf:Literal
	lrdf_add_triple(src.c_str(), local_field.c_str(), RDFS_BASE "domain", 
					RDF_BASE "Literal", lrdf_uri);

	set_label (local_field, name);
	
	fields_changed(); /* EMIT SIGNAL */
}

void
AudioLibrary::get_fields (vector<string>& fields)
{
	lrdf_statement pattern;

	pattern.subject = 0;
	pattern.predicate = RDFS_BASE "range";
	pattern.object = SOUNDFILE;
	pattern.object_type = lrdf_uri;

	lrdf_statement* matches = lrdf_matches(&pattern);

	lrdf_statement* current = matches;
	while (current != 0) {
		fields.push_back(get_label(current->subject));

		current = current->next;
	}

	lrdf_free_statements(matches);

	compact_vector(fields);
}

void
AudioLibrary::remove_field (string name)
{
	lrdf_remove_uri_matches(field_uri(name).c_str());
	fields_changed (); /* EMIT SIGNAL */
}

string 
AudioLibrary::get_field (string uri, string field)
{
	lrdf_statement pattern;

	pattern.subject = strdup(uri.c_str());

	pattern.predicate = strdup(field_uri(field).c_str());

	pattern.object = 0;
	pattern.object_type = lrdf_literal;

	lrdf_statement* matches = lrdf_matches(&pattern);
	free(pattern.subject);
	free(pattern.predicate);

	stringstream object;
	if (matches != 0){
		object << matches->object;
	}

	lrdf_free_statements(matches);
	return object.str();
}

void 
AudioLibrary::set_field (string uri, string field, string literal)
{
	lrdf_statement pattern;

	pattern.subject = strdup(uri.c_str());

	string local_field = field_uri(field);
	pattern.predicate = strdup(local_field.c_str());

	pattern.object = 0;
	pattern.object_type = lrdf_literal;

	lrdf_remove_matches(&pattern);
	free(pattern.subject);
	free(pattern.predicate);

	lrdf_add_triple(src.c_str(), uri.c_str(), local_field.c_str(), 
			literal.c_str(), lrdf_literal);

	 fields_changed(); /* EMIT SIGNAL */
}

string
AudioLibrary::field_uri (string name)
{
	stringstream local_field;
	local_field << "file:sfdb/fields/" << name;

	return local_field.str();
}

string
AudioLibrary::get_label (string uri)
{
	lrdf_statement pattern;
	pattern.subject = strdup(uri.c_str());
	pattern.predicate = RDFS_BASE "label";
	pattern.object = 0;
	pattern.object_type = lrdf_literal;

	lrdf_statement* matches = lrdf_matches (&pattern);
	free(pattern.subject);

	stringstream label;
	if (matches != 0){
		label << matches->object;
	}

	lrdf_free_statements(matches);

	return label.str();
}

void
AudioLibrary::set_label (string uri, string label)
{
	lrdf_statement pattern;
	pattern.subject = strdup(uri.c_str());
	pattern.predicate = RDFS_BASE "label";
	pattern.object = 0;
	pattern.object_type = lrdf_literal;

	lrdf_remove_matches(&pattern);
	free(pattern.subject);

	lrdf_add_triple(src.c_str(), uri.c_str(), RDFS_BASE "label", 
			label.c_str(), lrdf_literal);
}

void
AudioLibrary::compact_vector(vector<string>& vec)
{
    sort(vec.begin(), vec.end());
    unique(vec.begin(), vec.end());
}

void 
AudioLibrary::set_paths (vector<string> paths)
{
	sfdb_paths = paths;
}

vector<string> 
AudioLibrary::get_paths ()
{
	return sfdb_paths;
}

void
AudioLibrary::scan_paths ()
{
	if (sfdb_paths.size() < 1) {
		return;
	}

	vector<char *> pathv(sfdb_paths.size());
	unsigned int i;
	for (i = 0; i < sfdb_paths.size(); ++i) {
		pathv[i] = new char[sfdb_paths[i].length() +1];
		sfdb_paths[i].copy(pathv[i], string::npos);
		pathv[i][sfdb_paths[i].length()] = 0;
	}
	pathv[i] = 0;

	FTS* ft = fts_open(&pathv[0], FTS_LOGICAL|FTS_NOSTAT|FTS_PHYSICAL|FTS_XDEV, 0);
	if (errno) {
		error << strerror(errno) << endmsg;
		return;
	}

	lrdf_statement s;
	s.predicate = RDF_TYPE;
	s.object = SOUNDFILE;
	s.object_type = lrdf_uri;
	string filename;
	while (FTSENT* file = fts_read(ft)) {
		if ((file->fts_info & FTS_F) && (safe_file_extension(file->fts_name))) {
			filename = "file:";
			filename.append(file->fts_accpath);
			s.subject = strdup(filename.c_str());
			if (lrdf_exists_match(&s)) {
				continue;
			} else {
				add_member(file->fts_accpath);
				cout << file->fts_accpath << endl;
			}
			free(s.subject);
		}
	}
	fts_close(ft);

	for (i = 0; i < pathv.size(); ++i) {
		delete[] pathv[i];
	}

	save_changes();
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
        file.rfind(".voc") == string::npos);
}
