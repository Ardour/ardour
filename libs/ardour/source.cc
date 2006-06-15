/*
    Copyright (C) 2000 Paul Davis 

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

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <float.h>
#include <cerrno>
#include <ctime>
#include <cmath>
#include <iomanip>
#include <algorithm>

#include <glibmm/thread.h>
#include <pbd/xml++.h>
#include <pbd/pthread_utils.h>

#include <ardour/source.h>

#include "i18n.h"

using std::min;
using std::max;

using namespace ARDOUR;

Source::Source (string name)
{
	_name = name;
	_id = ARDOUR::new_id();
	_use_cnt = 0;
	_timestamp = 0;
}

Source::Source (const XMLNode& node) 
{
	_use_cnt = 0;
	_timestamp = 0;

	if (set_state (node)) {
		throw failed_constructor();
	}
}

Source::~Source ()
{
}

XMLNode&
Source::get_state ()
{
	XMLNode *node = new XMLNode ("Source");
	char buf[64];

	node->add_property ("name", _name);
	snprintf (buf, sizeof(buf)-1, "%" PRIu64, _id);
	node->add_property ("id", buf);

	if (_timestamp != 0) {
		snprintf (buf, sizeof (buf), "%ld", _timestamp);
		node->add_property ("timestamp", buf);
	}

	return *node;
}

int
Source::set_state (const XMLNode& node)
{
	const XMLProperty* prop;

	if ((prop = node.property ("name")) != 0) {
		_name = prop->value();
	} else {
		return -1;
	}
	
	if ((prop = node.property ("id")) != 0) {
		sscanf (prop->value().c_str(), "%" PRIu64, &_id);
	} else {
		return -1;
	}

	if ((prop = node.property ("timestamp")) != 0) {
		sscanf (prop->value().c_str(), "%ld", &_timestamp);
	}

	return 0;
}

void
Source::use ()
{
	_use_cnt++;
}

void
Source::release ()
{
	if (_use_cnt) --_use_cnt;
}

