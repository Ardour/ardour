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

#include "automation_line.h"

namespace ARDOUR {
	class MidiRegion;
}

/** Stub class so that lines for MIDI AutomationRegionViews can use the correct
 *  MementoCommandBinder.
 */
class MidiAutomationLine : public AutomationLine
{
public:
	MidiAutomationLine (const std::string&, TimeAxisView&, ArdourCanvas::Item&,
	                    boost::shared_ptr<ARDOUR::AutomationList>,
	                    boost::shared_ptr<ARDOUR::MidiRegion>,
	                    Evoral::Parameter,
	                    Evoral::TimeConverter<double, ARDOUR::samplepos_t>* converter = 0);

	MementoCommandBinder<ARDOUR::AutomationList>* memento_command_binder ();

	virtual std::string get_verbose_cursor_string (double) const;

private:
	boost::shared_ptr<ARDOUR::MidiRegion> _region;
	Evoral::Parameter _parameter;
};
