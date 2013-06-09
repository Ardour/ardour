/*
    Copyright (C) 2009 Paul Davis 

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

#include "pbd/controllable_descriptor.h"
#include "pbd/strsplit.h"
#include "pbd/convert.h"

using namespace std;
using namespace PBD;

int
ControllableDescriptor::set (const std::string& str)
{
	string::size_type first_space = str.find_first_of (" ");

	if (first_space == string::npos) {
		return -1;
	}

	string front = str.substr (0, first_space);
	string back = str.substr (first_space);

	vector<string> path;	
	split (front, path, '/');

	if (path.size() < 2) {
		return -1;
	}

	vector<string> rest;	
	split (back, rest, ' ');

	if (rest.size() < 1) {
		return -1;
	}

	if (path[0] == "route" || path[0] == "rid") {

		_top_level_type = RemoteControlID;

		if (rest[0][0] == 'B') {
			_banked = true;
			_rid = atoi (rest[0].substr (1));
		} else if (isdigit (rest[0][0])) {
			_banked = false;
			_rid = atoi (rest[0]);
		} else {
			return -1;
		}

	} else if (path[0] == "bus" || path[0] == "track") {

		_top_level_type = NamedRoute;
		_top_level_name = rest[0];
	}

	if (path[1] == "gain") {
		_subtype = Gain;

	} else if (path[1] == "solo") {
		_subtype = Solo;

	} else if (path[1] == "mute") {
		_subtype = Mute;

	} else if (path[1] == "recenable") {
		_subtype = Recenable;

	} else if (path[1] == "balance") {
		_subtype = Balance;

	} else if (path[1] == "panwidth") {
		_subtype = PanWidth;

	} else if (path[1] == "pandirection") {
		_subtype = PanDirection;

	} else if (path[1] == "plugin") {
		if (path.size() == 3 && rest.size() == 3) {
			if (path[2] == "parameter") {
				_subtype = PluginParameter;
				_target.push_back (atoi (rest[1]));
				_target.push_back (atoi (rest[2]));
			} else {
				return -1;
			}
		} else {
			return -1;
		}
	} else if (path[1] == "send") {
		
		if (path.size() == 3 && rest.size() == 3) {
			if (path[2] == "gain") {
				_subtype = SendGain;
				_target.push_back (atoi (rest[1]));
				_target.push_back (atoi (rest[2]));
			} else {
				return -1;
			}
		} else {
			return -1;
		}
	}
	
	return 0;
}

uint32_t
ControllableDescriptor::rid() const
{
	if (banked()) {
		return _rid + _bank_offset;
	} 		

	return _rid;
}

uint32_t
ControllableDescriptor::target (uint32_t n) const
{
	if (n < _target.size()) {
		return _target[n];
	} 
	
	return 0;
}
