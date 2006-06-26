/*
    Copyright (C) 2006 Paul Davis
	Written by Dave Robillard, 2006

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

#include <pbd/xml++.h>
#include <pbd/pthread_utils.h>

#include <ardour/midi_source.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

sigc::signal<void,MidiSource *> MidiSource::MidiSourceCreated;

MidiSource::MidiSource (string name)
	: Source (name)
{
	_read_data_count = 0;
	_write_data_count = 0;
}

MidiSource::MidiSource (const XMLNode& node) 
	: Source (node)
{
	_read_data_count = 0;
	_write_data_count = 0;

	if (set_state (node)) {
		throw failed_constructor();
	}
}

MidiSource::~MidiSource ()
{
}

XMLNode&
MidiSource::get_state ()
{
	XMLNode& node (Source::get_state());

	if (_captured_for.length()) {
		node.add_property ("captured-for", _captured_for);
	}

	return node;
}

int
MidiSource::set_state (const XMLNode& node)
{
	const XMLProperty* prop;

	Source::set_state (node);

	if ((prop = node.property ("captured-for")) != 0) {
		_captured_for = prop->value();
	}

	return 0;
}

jack_nframes_t
MidiSource::read (unsigned char *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const
{
	//Glib::Mutex::Lock lm (_lock);
	//return read_unlocked (dst, start, cnt, workbuf);
	return 0;
}

jack_nframes_t
MidiSource::write (unsigned char *dst, jack_nframes_t cnt, char * workbuf)
{
	//Glib::Mutex::Lock lm (_lock);
	//return write_unlocked (dst, cnt, workbuf);
	return 0;
}


bool
MidiSource::file_changed (string path)
{
	struct stat stat_file;
	//struct stat stat_peak;

	int e1 = stat (path.c_str(), &stat_file);
	//int e2 = stat (peak_path(path).c_str(), &stat_peak);
	
	if (!e1){//&& !e2 && stat_file.st_mtime > stat_peak.st_mtime){
		return true;
	} else {
		return false;
	}
}


void
MidiSource::update_length (jack_nframes_t pos, jack_nframes_t cnt)
{
	if (pos + cnt > _length) {
		_length = pos+cnt;
	}
}

