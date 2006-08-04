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

#include <string>
#include <map>
#include <vector>

#include <sigc++/signal.h>

#include <pbd/stateful.h>

using std::vector;
using std::string;
using std::map;

namespace ARDOUR {

class AudioLibrary : public Stateful
{
  public:
	AudioLibrary ();
	~AudioLibrary ();

	static string state_node_name;
	
	XMLNode& get_state (void);
	int set_state (const XMLNode&);

	void set_paths (vector<string> paths);
	vector<string> get_paths ();
	void scan_paths ();

	void add_member (string member);
	void remove_member (string uri);

	void search_members_and (vector<string>& results, 
							 const map<string,string>& fields);
	void search_members_or (vector<string>& results, 
							const map<string,string>& fields);

	void add_field (string field);
	void get_fields (vector<string>& fields);
	void remove_field (string field);
	string get_field (string uri, string field);
	void set_field (string uri, string field, string literal);
	string get_label (string uri);
	void set_label (string uri, string name);

	void save_changes();

	sigc::signal<void> fields_changed;
	
  private:
	vector<string> sfdb_paths;

	string field_uri (string name);

	bool is_rdf_type (string uri, string type);
	void remove_uri (string uri);

	string src;
	
	void initialize_db();
	void compact_vector (vector<string>& vec);
	bool safe_file_extension (string);
};

extern AudioLibrary* Library;

} // ARDOUR namespace

#endif // __ardour_audio_library_h__
