#ifndef __libardour_library_h__
#define __libardour_library_h__

#include <string>
#include <vector>
#include <thread>

namespace ARDOUR {

class LibraryDescription
{
   public:
	LibraryDescription (std::string const & n, std::string const & d, std::string const & f, std::string const & l)
		: _name (n), _description (d), _file (f), _license (l) {}

	std::string const & name() const { return _name; }
	std::string const & description() const { return _description; }
	std::string const & file() const { return _file; }
	std::string const & license() const { return _license; }

  private:
	std::string _name;
	std::string _description;
	std::string _file;
	std::string _license;
};

class LibraryFetcher {
  public:
	int add (std::string const & url);
	int get_descriptions ();

  private:
	std::string url;
	std::thread thr;
	std::vector<LibraryDescription> _descriptions;
};

} /* namespace */

#endif
