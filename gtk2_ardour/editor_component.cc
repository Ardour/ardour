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

#include "ardour/session.h"
#include "editor_component.h"

using namespace std;
using namespace sigc;
using namespace ARDOUR;

EditorComponent::EditorComponent (Editor* e)
	: _editor (e),
	  _session (0)
{

}
	
void
EditorComponent::connect_to_session (Session* s)
{
	_session = s;

	_session_connections.push_back (_session->GoingAway.connect (mem_fun (*this, &EditorComponent::session_going_away)));
}

void
EditorComponent::session_going_away ()
{
	for (list<connection>::iterator i = _session_connections.begin(); i != _session_connections.end(); ++i) {
		i->disconnect ();
	}
}
