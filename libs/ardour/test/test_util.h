#include <string>
#include <list>

class XMLNode;

namespace ARDOUR {
	class Session;
}

extern void check_xml (XMLNode *, std::string, std::list<std::string> const &);
extern bool write_ref (XMLNode *, std::string);
extern ARDOUR::Session* load_session (std::string, std::string);
