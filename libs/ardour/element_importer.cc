/*
 * Copyright (C) 2008 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ardour/element_importer.h"

#include <sstream>
#include <iomanip>

#include "pbd/string_convert.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

Signal2<std::pair<bool, string>,string, string> ElementImporter::Rename;
Signal1 <bool,string> ElementImporter::Prompt;

ElementImporter::ElementImporter (XMLTree const & source, ARDOUR::Session & session) :
  source (source),
  session(session),
  _queued (false),
  _broken (false)
{
	source.root()->get_property ("sample-rate", sample_rate);
}

ElementImporter::~ElementImporter ()
{
}

void
ElementImporter::move ()
{
	if (!_queued) { return; }
	_move ();
}

bool
ElementImporter::prepare_move ()
{
	if (_queued) {
		return true;
	}
	_queued = _prepare_move ();
	return _queued;
}

void
ElementImporter::cancel_move ()
{
	if (!_queued) { return; }
	_cancel_move ();
}

string
ElementImporter::timecode_to_string(Timecode::Time & time) const
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

samplecnt_t
ElementImporter::rate_convert_samples (samplecnt_t samples) const
{
	if (sample_rate == session.sample_rate()) {
		return samples;
	}

	// +0.5 for proper rounding
	return static_cast<samplecnt_t> (samples * (static_cast<double> (session.nominal_sample_rate()) / sample_rate) + 0.5);
}

string
ElementImporter::rate_convert_samples (string const & samples) const
{
	return to_string (rate_convert_samples (string_to<uint32_t>(samples)));
}
