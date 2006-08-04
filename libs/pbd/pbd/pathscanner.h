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
				  long limit = -1) {
	    return run_scan (dirpath,
			     (bool (PathScanner::*)(const string &)) 0, 
			     filter, 
			     arg,
			     match_fullpath,
			     return_fullpath, 
			     limit);
    }

    vector<string *> *operator() (const string &dirpath,
				  const string &regexp,
				  bool match_fullpath = true,
				  bool return_fullpath = true,
				  long limit = -1);

    
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
				long limit);
    

};

#endif // __libmisc_pathscanner_h__
