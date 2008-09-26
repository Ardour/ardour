/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#include <ardour/element_importer.h>

#include <sstream>
#include <iomanip>

#include <pbd/convert.h>
#include <ardour/session.h>

#include "i18n.h"

using namespace PBD;
using namespace ARDOUR;

sigc::signal <std::pair<bool, string>, string, string> ElementImporter::Rename;
sigc::signal <bool, string> ElementImporter::Prompt;

ElementImporter::ElementImporter (XMLTree const & source, ARDOUR::Session & session) : 
  source (source),
  session(session),
  queued (false),
  _broken (false)
{
	// Get samplerate
	XMLProperty *prop;
	prop = source.root()->property ("sample-rate");
	if (prop) {
		std::istringstream iss (prop->value());
		iss >> sample_rate;
	}
}

string
ElementImporter::smpte_to_string(SMPTE::Time & time) const
{
	std::ostringstream oss;
	oss << std::setfill('0') << std::right <<
	  std::setw(2) <<
	  time.hours << ":" <<
	  std::setw(2) <<
	  time.minutes << ":" <<
	  std::setw(2) <<
	  time.seconds << ":" <<
	  std::setw(2) <<
	  time.frames;
	
	return oss.str();
}

nframes_t
ElementImporter::rate_convert_samples (nframes_t samples) const
{
	if (sample_rate == session.frame_rate()) {
		return samples;
	}
	
	// +0.5 for proper rounding
	return static_cast<nframes_t> (samples * (static_cast<double> (session.nominal_frame_rate()) / sample_rate) + 0.5);
}

string
ElementImporter::rate_convert_samples (string const & samples) const
{
	return to_string (rate_convert_samples (atoi (samples)), std::dec);
}
