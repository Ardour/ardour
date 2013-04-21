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
*/

#include "ardour/freeze_operation.h"
#include "ardour/track.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"

using namespace ARDOUR;
using namespace std;

FreezeOperation* FreezeOperation::_instance = NULL;

FreezeOperation::FreezeOperation(){}
FreezeOperation::~FreezeOperation(){ delete _instance; }

FreezeOperation*
FreezeOperation::get_instance()
{
  return (_instance == NULL) ? _instance = new FreezeOperation() : _instance;
}

void 
FreezeOperation::apply(AudioTrack* track){}

void 
FreezeOperation::apply(MidiTrack* track){}

void 
FreezeOperation::disapply(AudioTrack* track){ track->unfreeze(); }

void 
FreezeOperation::disapply(MidiTrack* track){ track->unfreeze(); }
