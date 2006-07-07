#include <ostream>
#include <iostream>

#include <string.h>
#include <uuid/uuid.h>

#include <pbd/id.h>

using namespace std;
using namespace PBD;

ID::ID ()
{
	uuid_generate (id);
}

ID::ID (string str)
{
	string_assign (str);
}

int
ID::string_assign (string str)
{
	/* first check for old-style all-numeric ID's */

	if (strcspn (str.c_str(), "0123456789") == 0) {
		/* all chars are numeric. just render the existing ID into the space in 
		   which we would otherwise store a UUID.
		*/

		memset (id, ' ', sizeof (id));
		snprintf ((char*) id, sizeof (id), str.c_str());

	} else {

		/* OK, its UUID, probably */

		if (uuid_parse (str.c_str(), id)) {
			/* XXX error */
			return -1;
		}
	}

	return 0;
}

void
ID::print (char* buf) const
{
	uuid_unparse (id, buf);
}

ID&
ID::operator= (string str)
{
	string_assign (str);
	return *this;
}

bool
ID::operator== (const ID& other) const
{
	return memcmp (id, other.id, sizeof (id)) == 0;
}

ostream&
operator<< (ostream& ostr, const ID& id)
{
	char buf[37];
	id.print (buf);
	ostr << buf;
	return ostr;
}

