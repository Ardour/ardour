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

sigc::signal<void,Source*> Source::SourceCreated;


Source::Source (string name, DataType type)
	: _type(type)
{
	assert(_name.find("/") == string::npos);

	_name = name;
	_use_cnt = 0;
	_timestamp = 0;
}

Source::Source (const XMLNode& node) 
	: _type(DataType::AUDIO)
{
	_use_cnt = 0;
	_timestamp = 0;

	if (set_state (node) || _type == DataType::NIL) {
		throw failed_constructor();
	}
	assert(_name.find("/") == string::npos);
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
	node->add_property ("type", _type.to_string());
	_id.print (buf);
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
		_id = prop->value ();
	} else {
		return -1;
	}

	if ((prop = node.property ("type")) != 0) {
		_type = DataType(prop->value());
	}

	if ((prop = node.property ("timestamp")) != 0) {
		sscanf (prop->value().c_str(), "%ld", &_timestamp);
	}
	assert(_name.find("/") == string::npos);

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

void
Source::update_length (jack_nframes_t pos, jack_nframes_t cnt)
{
	if (pos + cnt > _length) {
		_length = pos+cnt;
	}
}

