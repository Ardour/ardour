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

#include "ardour/midi_automation_list_binder.h"
#include "midi_automation_line.h"

using namespace std;

MidiAutomationLine::MidiAutomationLine (
	const std::string&                                      name,
	TimeAxisView&                                           tav,
	ArdourCanvas::Group&                                    group,
	boost::shared_ptr<ARDOUR::AutomationList>               list,
	boost::shared_ptr<ARDOUR::MidiRegion>                   region,
	Evoral::Parameter                                       parameter,
	Evoral::TimeConverter<double, ARDOUR::framepos_t>*      converter)
	: AutomationLine (name, tav, group, list, converter)
	, _region (region)
	, _parameter (parameter)
{

}

MementoCommandBinder<ARDOUR::AutomationList>*
MidiAutomationLine::memento_command_binder ()
{
	return new ARDOUR::MidiAutomationListBinder (_region->midi_source(), _parameter);
}
