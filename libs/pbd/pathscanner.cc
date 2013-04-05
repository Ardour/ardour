/*
    Copyright (C) 1998-99 Paul Barton-Davis 

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

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <glibmm/miscutils.h>

#include "pbd/error.h"
#include "pbd/pathexpand.h"
#include "pbd/pathscanner.h"
#include "pbd/stl_delete.h"

using namespace std;
using namespace PBD;

vector<string *> *
PathScanner::operator() (const string &dirpath, const string &regexp,
			 bool match_fullpath, bool return_fullpath, 
			 long limit, bool recurse)

{
	int err;
	char msg[256];

	if ((err = regcomp (&compiled_pattern, regexp.c_str(),
			    REG_EXTENDED|REG_NOSUB))) {
		
		regerror (err, &compiled_pattern,
			  msg, sizeof (msg));
		
		error << "Cannot compile soundfile regexp for use (" 
		      << msg 
		      << ")" 
		      << endmsg;
		
		return 0;
	}
	
	return run_scan (dirpath, &PathScanner::regexp_filter, 
			 (bool (*)(const string &, void *)) 0,
			 0,
			 match_fullpath,
			 return_fullpath,
			 limit, recurse);
}	

vector<string *> *
PathScanner::run_scan (const string &dirpath, 
		       bool (PathScanner::*memberfilter)(const string &),
		       bool (*filter)(const string &, void *),
		       void *arg,
		       bool match_fullpath, bool return_fullpath,
		       long limit,
		       bool recurse)
{
	return run_scan_internal ((vector<string*>*) 0, dirpath, memberfilter, filter, arg, match_fullpath, return_fullpath, limit, recurse);
}
	
vector<string *> *
PathScanner::run_scan_internal (vector<string *> *result,
				const string &dirpath, 
				bool (PathScanner::*memberfilter)(const string &),
				bool (*filter)(const string &, void *),
				void *arg,
				bool match_fullpath, bool return_fullpath,
				long limit,
				bool recurse)
{
	DIR *dir;
	struct dirent *finfo;
	char *pathcopy = strdup (search_path_expand (dirpath).c_str());
	char *thisdir;
	string fullpath;
	string search_str;
	string *newstr;
	long nfound = 0;

	if ((thisdir = strtok (pathcopy, ":")) == 0 ||
	    strlen (thisdir) == 0) {
		free (pathcopy);
		return 0;
	}

	if (result == 0) {
		result = new vector<string *>;
	}

	do {

		if ((dir = opendir (thisdir)) == 0) {
			continue;
		}
		
		while ((finfo = readdir (dir)) != 0) {

			if ((finfo->d_name[0] == '.' && finfo->d_name[1] == '\0') ||
			    (finfo->d_name[0] == '.' && finfo->d_name[1] == '.' && finfo->d_name[2] == '\0')) {
				continue;
			}
                        
                        fullpath = Glib::build_filename (thisdir, finfo->d_name);

			struct stat statbuf;
			if (stat (fullpath.c_str(), &statbuf) < 0) {
				continue;
			}

			if (statbuf.st_mode & S_IFDIR && recurse) {
				run_scan_internal (result, fullpath, memberfilter, filter, arg, match_fullpath, return_fullpath, limit, recurse);
			} else {
				
				if (match_fullpath) {
					search_str = fullpath;
				} else {
					search_str = finfo->d_name;
				}
				
				/* handle either type of function ptr */
				
				if (memberfilter) {
					if (!(this->*memberfilter)(search_str)) {
						continue;
					} 
				} else {
					if (!filter(search_str, arg)) {
						continue;
					}
				}

				if (return_fullpath) {
					newstr = new string (fullpath);
				} else {
					newstr = new string (finfo->d_name);
				} 
				
				result->push_back (newstr);
				nfound++;
			}
		}
		closedir (dir);
		
	} while ((limit < 0 || (nfound < limit)) && (thisdir = strtok (0, ":")));

	free (pathcopy);
	return result;
}

string *
PathScanner::find_first (const string &dirpath,
			 const string &regexp,
			 bool match_fullpath,
			 bool return_fullpath)
{
	vector<string *> *res;
	string *ret;
	int err;
	char msg[256];

	if ((err = regcomp (&compiled_pattern, regexp.c_str(),
			    REG_EXTENDED|REG_NOSUB))) {
		
		regerror (err, &compiled_pattern,
			  msg, sizeof (msg));
		
		error << "Cannot compile soundfile regexp for use (" << msg << ")" << endmsg;

		
		return 0;
	}
	
	res = run_scan (dirpath, 
			&PathScanner::regexp_filter,
			(bool (*)(const string &, void *)) 0,
			0,
			match_fullpath,
			return_fullpath, 
			1);
	
	if (res->size() == 0) {
		ret = 0;
	} else {
		ret = res->front();
	}
	vector_delete (res);
	delete res;
	return ret;
}

string *
PathScanner::find_first (const string &dirpath,
			 bool (*filter)(const string &, void *),
			 void * /*arg*/,
			 bool match_fullpath,
			 bool return_fullpath)
{
	vector<string *> *res;
	string *ret;

	res = run_scan (dirpath, 
			(bool (PathScanner::*)(const string &)) 0,
			filter,
			0,
			match_fullpath,
			return_fullpath, 1);
	
	if (res->size() == 0) {
		ret = 0;
	} else {
		ret = res->front();
	}
	vector_delete (res);
	delete res;
	return ret;
}
