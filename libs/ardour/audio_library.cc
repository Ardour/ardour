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
#include <iostream>
#include <sstream>
#include <cctype>

#include <lrdf.h>

#include <pbd/compose.h>

#include <ardour/ardour.h>
#include <ardour/configuration.h>
#include <ardour/audio_library.h>
#include <ardour/utils.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;

namespace std {
struct UriSorter {
	bool operator() (string a, string b) const { 
		return cmp_nocase(Library->get_label(a), Library->get_label(b)) == -1; 
	}
}; 
};

static char* GROUP = "http://ardour.org/ontology/Group";
static char* SOUNDFILE = "http://ardour.org/ontology/Soundfile";
static char* hasFile = "http://ardour.org/ontology/hasFile";
static char* memberOf = "http://ardour.org/ontology/memberOf";
static char* subGroupOf = "http://ardour.org/ontology/subGroupOf";

AudioLibrary::AudioLibrary ()
{
	src = "file:" + Config->get_user_ardour_path() + "sfdb";

	// workaround for possible bug in raptor that crashes when saving to a
	// non-existant file.
	touch_file(Config->get_user_ardour_path() + "sfdb");

	lrdf_read_file(src.c_str());

	lrdf_statement pattern;

	pattern.subject = GROUP;
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
}

AudioLibrary::~AudioLibrary ()
{
}

void
AudioLibrary::initialize_db ()
{
	// define ardour:Group
	lrdf_add_triple(src.c_str(), GROUP, RDF_TYPE, RDFS_CLASS, lrdf_uri);
	// define ardour:Soundfile
	lrdf_add_triple(src.c_str(), SOUNDFILE, RDF_TYPE, RDFS_CLASS, lrdf_uri);

	// add intergral fields
	add_field("channels");
	add_field("samplerate");
	add_field("resolution");
	add_field("format");
}

void
AudioLibrary::save_changes ()
{
	if (lrdf_export_by_source(src.c_str(), src.substr(5).c_str())) {
		warning << string_compose(_("Could not open %1.  Audio Library not saved"), src) << endmsg;
	}
}

string
AudioLibrary::add_group (string group, string parent_uri)
{
	string local_group(string_compose("file:sfbd/group/%1", get_uid()));

	lrdf_add_triple(src.c_str(), local_group.c_str(), 
					RDFS_BASE "label", group.c_str(), lrdf_literal);

	if (parent_uri == ""){
		lrdf_add_triple(src.c_str(), local_group.c_str(), 
						subGroupOf, GROUP, lrdf_uri);
	} else {
		lrdf_add_triple(src.c_str(), local_group.c_str(), 
						subGroupOf, parent_uri.c_str(), lrdf_uri);
	}

	 added_group(local_group, parent_uri); /* EMIT SIGNAL */

	return local_group;
}

void
AudioLibrary::remove_group (string uri)
{
	list<string> items;
	list<string>::iterator i;

	get_members(items, uri);
	for (i = items.begin(); i != items.end(); ++i) {
		remove_member(*i);
	}
	
	items.clear();
	
	get_groups(items, uri);
	for (i = items.begin(); i != items.end(); ++i) {
		remove_group(*i);
	}

	lrdf_remove_uri_matches(uri.c_str());
	save_changes ();

	 removed_group(uri); /* EMIT SIGNAL */
}

void
AudioLibrary::get_groups (list<string>& groups, string parent_uri)
{
	lrdf_statement pattern;

	pattern.subject = 0;
	pattern.predicate = subGroupOf;
	if (parent_uri == ""){
		pattern.object = strdup(GROUP);
	} else {
		pattern.object = strdup(parent_uri.c_str());
	}

	lrdf_statement* matches = lrdf_matches(&pattern);

	lrdf_statement* current = matches;
	while (current != 0) {
		groups.push_back(current->subject);
		current = current->next;
	}

	lrdf_free_statements(matches);
	free (pattern.object);

	UriSorter cmp;
	groups.sort(cmp);
	groups.unique();
}

string
AudioLibrary::add_member (string member, string parent_uri)
{
	string local_member(string_compose("file:sfdb/soundfile/%1", get_uid()));
	string file_uri(string_compose("file:%1", member));

	lrdf_add_triple(src.c_str(), local_member.c_str(), RDF_TYPE, 
			SOUNDFILE, lrdf_uri);
	lrdf_add_triple(src.c_str(), local_member.c_str(), hasFile,
					file_uri.c_str(), lrdf_uri);

	string::size_type size = member.find_last_of('/');
	string label = member.substr(++size);

	lrdf_add_triple(src.c_str(), local_member.c_str(), RDFS_BASE "label", 
			label.c_str(), lrdf_literal);

	if (parent_uri == ""){
		lrdf_add_triple(src.c_str(), local_member.c_str(), memberOf,
				GROUP, lrdf_uri);
	} else {
		lrdf_add_triple(src.c_str(), local_member.c_str(), memberOf, 
			parent_uri.c_str(), lrdf_uri);
	}

	save_changes ();

	 added_member (local_member, parent_uri); /* EMIT SIGNAL */

	return local_member;
}

void
AudioLibrary::remove_member (string uri)
{
	lrdf_remove_uri_matches (uri.c_str());

	save_changes ();

	 removed_member(uri); /* EMIT SIGNAL */
}

void
AudioLibrary::get_members (list<string>& members, string parent_uri)
{
	lrdf_statement pattern;

	pattern.subject = 0;
	pattern.predicate = memberOf;
	if (parent_uri == ""){
		pattern.object = strdup(GROUP);
	} else {
		pattern.object = strdup(parent_uri.c_str());
	}

	lrdf_statement* matches = lrdf_matches(&pattern);

	lrdf_statement* current = matches;
	while (current != 0) {
		members.push_back(current->subject);
		current = current->next;
	}

	lrdf_free_statements(matches);
	free (pattern.object);

	UriSorter cmp;
	members.sort(cmp);
	members.unique();
}

void
AudioLibrary::search_members_and (list<string>& members, 
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

		UriSorter cmp;
		members.sort(cmp);
		members.unique();
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
AudioLibrary::search_members_or (list<string>& members, 
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

	UriSorter cmp;
	members.sort(cmp);
	members.unique();
}

string
AudioLibrary::get_member_filename (string uri)
{
	lrdf_statement pattern;
	pattern.subject = strdup(uri.c_str());
	pattern.predicate = hasFile;
	pattern.object = 0;
	pattern.object_type = lrdf_uri;
	
	lrdf_statement* matches = lrdf_matches(&pattern);
	if (matches) {
		string file = matches->object;
		lrdf_free_statements(matches);

		string::size_type pos = file.find(":");
		return file.substr(++pos);
	} else {
		warning << _("Could not find member filename") << endmsg;
		return "-Unknown-";
	}
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
	
	save_changes();

	 fields_changed(); /* EMIT SIGNAL */
}

void
AudioLibrary::get_fields (list<string>& fields)
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

	fields.sort();
	fields.unique();
}

void
AudioLibrary::remove_field (string name)
{
	lrdf_remove_uri_matches(field_uri(name).c_str());
	save_changes();
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

	save_changes();

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

