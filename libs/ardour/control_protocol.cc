/*
    Copyright (C) 2006 Paul Davis 

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

#include <ardour/control_protocol.h>

using namespace ARDOUR;
using namespace std;

sigc::signal<void> ControlProtocol::ZoomToSession;
sigc::signal<void> ControlProtocol::ZoomOut;
sigc::signal<void> ControlProtocol::ZoomIn;
sigc::signal<void> ControlProtocol::Enter;
sigc::signal<void,float> ControlProtocol::ScrollTimeline;

ControlProtocol::ControlProtocol (Session& s, string str)
	: BasicUI (s),
	  _name (str)
{
	_active = false;
}

ControlProtocol::~ControlProtocol ()
{
}

