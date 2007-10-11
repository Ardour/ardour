/*
    Copyright (C) 2000-2007 Paul Davis 

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

#include <pbd/enumwriter.h>

#include "audio_clock.h"
#include "editing.h"
#include "enums.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace Editing;

void
setup_gtk_ardour_enums ()
{
	EnumWriter& enum_writer (EnumWriter::instance());
	vector<int> i;
	vector<string> s;

	AudioClock::Mode clock_mode;
	Width width;
	ImportMode import_mode;

#define REGISTER(e) enum_writer.register_distinct (typeid(e).name(), i, s); i.clear(); s.clear()
#define REGISTER_BITS(e) enum_writer.register_bits (typeid(e).name(), i, s); i.clear(); s.clear()
#define REGISTER_ENUM(e) i.push_back (e); s.push_back (#e)
#define REGISTER_CLASS_ENUM(t,e) i.push_back (t::e); s.push_back (#e)

	REGISTER_CLASS_ENUM (AudioClock, SMPTE);
	REGISTER_CLASS_ENUM (AudioClock, BBT);
	REGISTER_CLASS_ENUM (AudioClock, MinSec);
	REGISTER_CLASS_ENUM (AudioClock, Frames);
	REGISTER_CLASS_ENUM (AudioClock, Off);
	REGISTER (clock_mode);

	REGISTER_ENUM (Wide);
	REGISTER_ENUM (Narrow);
	REGISTER (width);

	REGISTER_ENUM (ImportAsTrack);
	REGISTER_ENUM (ImportToTrack);
	REGISTER_ENUM (ImportAsRegion);
	REGISTER_ENUM (ImportAsTapeTrack);
	REGISTER (import_mode);
}
