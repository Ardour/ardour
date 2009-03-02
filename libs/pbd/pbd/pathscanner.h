/*
    Copyright (C) 2000-2007 Paul Davis 

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

#ifndef __libmisc_pathscanner_h__
#define __libmisc_pathscanner_h__

#include <vector>
#include <string>
#include <regex.h>

using std::string;
using std::vector;

class PathScanner

{
  public:
	vector<string *> *operator() (const string &dirpath,
				      bool (*filter)(const string &, void *arg),
				      void *arg, 
				      bool match_fullpath = true,
				      bool return_fullpath = true,
				      long limit = -1,
				      bool recurse = false) {
		return run_scan (dirpath,
				 (bool (PathScanner::*)(const string &)) 0, 
				 filter, 
				 arg,
				 match_fullpath,
				 return_fullpath, 
				 limit, recurse);
	}

	vector<string *> *operator() (const string &dirpath,
				      const string &regexp,
				      bool match_fullpath = true,
				      bool return_fullpath = true,
				      long limit = -1,
				      bool recurse = false);
    
	string *find_first (const string &dirpath,
			    const string &regexp,
			    bool match_fullpath = true,
			    bool return_fullpath = true);
	
	string *find_first (const string &dirpath,
			    bool (*filter)(const string &, void *),
			    void *arg,
			    bool match_fullpath = true,
			    bool return_fullpath = true);
	
  private:
	regex_t compiled_pattern;
	
	bool regexp_filter (const string &str) {
		return regexec (&compiled_pattern, str.c_str(), 0, 0, 0) == 0;
	}
	
	vector<string *> *run_scan (const string &dirpath,
				    bool (PathScanner::*mfilter) (const string &),
				    bool (*filter)(const string &, void *),
				    void *arg,
				    bool match_fullpath,
				    bool return_fullpath,
				    long limit,
				    bool recurse = false);

	vector<string *> *run_scan_internal (vector<string*>*, 
					     const string &dirpath,
				    bool (PathScanner::*mfilter) (const string &),
				    bool (*filter)(const string &, void *),
				    void *arg,
				    bool match_fullpath,
				    bool return_fullpath,
				    long limit,
				    bool recurse = false);
};

#endif // __libmisc_pathscanner_h__
