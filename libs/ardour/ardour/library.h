#ifndef __libardour_library_h__
#define __libardour_library_h__

#include <cstdio>
#include <cstdint>
#include <atomic>
#include <string>
#include <vector>
#include <thread>

#include <boost/function.hpp>

#include <curl/curl.h>

namespace ARDOUR {

class LibraryDescription
{
   public:
	LibraryDescription (std::string const & n, std::string const & a, std::string const & d, std::string const & u, std::string const & l, std::string const & td, std::string const & s)
		: _name (n), _author (a), _description (d), _url (u), _license (l), _toplevel_dir (td), _size (s), _installed (false) {}

	std::string const & name() const { return _name; }
	std::string const & description() const { return _description; }
	std::string const & author() const { return _author; }
	std::string const & url() const { return _url; }
	std::string const & license() const { return _license; }
	std::string const & toplevel_dir() const { return _toplevel_dir; }
	std::string const & size() const { return _size; }

	bool installed() const { return _installed; }
	void set_installed (bool yn) { _installed = yn; }

  private:
	std::string _name;
	std::string _author;
	std::string _description;
	std::string _url;
	std::string _license;
	std::string _toplevel_dir;
	std::string _size;
	bool _installed;
};

class Downloader {
  public:
	Downloader (std::string const & url, std::string const & destdir);

	int start ();
	void cleanup ();
	void cancel ();
	double progress() const;

	uint64_t download_size() const { return _download_size; }
	uint64_t downloaded () const { return _downloaded; }

	/* public so it can be called from a static C function */
	size_t write (void *contents, size_t size, size_t nmemb);

	int status() const { return _status; }
	std::string download_path() const;

  private:
	std::string url;
	std::string destdir;
	std::string file_path;
	FILE* file;
	CURL* curl;
	bool _cancel;
	std::atomic<uint64_t> _download_size; /* read-only from requestor thread */
	std::atomic<uint64_t> _downloaded; /* read-only from requestor thread */
	std::atomic<int> _status;
	std::thread thr;

	void download ();
};

class LibraryFetcher {
  public:
	LibraryFetcher() {}

	int add (std::string const & path);
	int get_descriptions ();
	void foreach_description (boost::function<void (LibraryDescription)> f) const;

	bool installed (LibraryDescription const & desc);
	void foreach_description (boost::function<void (LibraryDescription)> f);

  private:
	std::vector<LibraryDescription> _descriptions;
	std::string install_path_for (LibraryDescription const &);
};

} /* namespace */

#endif
