/*
    Copyright (C) 2010 Paul Davis

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

#include "ardour/speaker.h"
#include "ardour/speakers.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

Speaker::Speaker (int i, const AngularVector& position)
        : id (i)
{
        move (position);
}

void
Speaker::move (const AngularVector& new_position)
{
        _angles = new_position;
        _angles.cartesian (_coords);
}

Speakers::Speakers ()
{
}

Speakers::~Speakers ()
{
}

void
Speakers::dump_speakers (ostream& o)
{
        for (vector<Speaker>::iterator i = _speakers.begin(); i != _speakers.end(); ++i) {
                o << "Speaker " << (*i).id << " @ " 
                  << (*i).coords().x << ", " << (*i).coords().y << ", " << (*i).coords().z
                  << " azimuth " << (*i).angles().azi
                  << " elevation " << (*i).angles().ele
                  << " distance " << (*i).angles().length
                  << endl;
        }
}

void
Speakers::clear_speakers ()
{
        _speakers.clear ();
        update ();
}

int 
Speakers::add_speaker (const AngularVector& position)
{
        int id = _speakers.size();

        cerr << "Added speaker " << id << " at " << position.azi << " /= " << position.ele << endl;

        _speakers.push_back (Speaker (id, position));
        update ();

        dump_speakers (cerr);
        Changed ();

        return id;
}        

void
Speakers::remove_speaker (int id)
{
        for (vector<Speaker>::iterator i = _speakers.begin(); i != _speakers.end(); ) {
                if ((*i).id == id) {
                        i = _speakers.erase (i);
                        update ();
                        break;
                } 
        }
}

void
Speakers::move_speaker (int id, const AngularVector& new_position)
{
        for (vector<Speaker>::iterator i = _speakers.begin(); i != _speakers.end(); ++i) {
                if ((*i).id == id) {
                        (*i).move (new_position);
                        update ();
                        break;
                }
        }
}



