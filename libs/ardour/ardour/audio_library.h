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

#ifndef __ardour_audio_library_h__
#define __ardour_audio_library_h__

#include <list>
#include <string>
#include <map>

#include <sigc++/signal.h>

using std::list;
using std::string;
using std::map;

namespace ARDOUR {

class AudioLibrary 
{
  public:
	AudioLibrary ();
	~AudioLibrary ();
  
	// add_group returns the URI of the created group
	string add_group (string group, string parent_uri = "");
	void remove_group (string uri);
	void get_groups (list<string>& groups, string parent_uri = "");

	// add_member returns the URI of the created group
	string add_member (string member, string parent_uri = "");
	void remove_member (string uri);
	void get_members (list<string>& members, string parent_uri = "");
	string get_member_filename (string uri);

	void search_members_and (list<string>& results, 
							 const map<string,string>& fields);
	void search_members_or (list<string>& results, 
							const map<string,string>& fields);

	void add_field (string field);
	void get_fields (list<string>& fields);
	void remove_field (string field);
	string get_field (string uri, string field);
	void set_field (string uri, string field, string literal);

	string get_label (string uri);
	void set_label (string uri, string label);

	sigc::signal<void, string, string> added_group; // group, parent
	sigc::signal<void, string, string> added_member;// member, parent
	sigc::signal<void, string> removed_group;
	sigc::signal<void, string> removed_member;
	sigc::signal<void> fields_changed;
	
  private:
	void save_changes ();
	string field_uri (string name);

	bool is_rdf_type (string uri, string type);
	void remove_uri (string uri);

	string  src;
	
	void initialize_db();
};

extern AudioLibrary* Library;

} // ARDOUR namespace

#endif // __ardour_audio_library_h__
