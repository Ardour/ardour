#include <ostream>
#include <iostream>
#include <stdio.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include <pbd/id.h>
#include <string>

using namespace std;
using namespace PBD;

Glib::Mutex* ID::counter_lock = 0;
uint64_t ID::_counter = 0;

void
ID::init ()
{
	counter_lock = new Glib::Mutex;
}

ID::ID ()
{
	Glib::Mutex::Lock lm (*counter_lock);
	id = _counter++;
}

ID::ID (string str)
{
	string_assign (str);
}

int
ID::string_assign (string str)
{
	return sscanf (str.c_str(), "%" PRIu64, &id) != 0;
}

void
ID::print (char* buf) const
{
	/* XXX sizeof buf is unknown. bad API design */
	snprintf (buf, 16, "%" PRIu64, id);
}

string ID::to_s() const
{
    char buf[16]; // see print()
    print(buf);
    return string(buf);
}

ID&
ID::operator= (string str)
{
	string_assign (str);
	return *this;
}

ostream&
operator<< (ostream& ostr, const ID& id)
{
	char buf[32];
	id.print (buf);
	ostr << buf;
	return ostr;
}

