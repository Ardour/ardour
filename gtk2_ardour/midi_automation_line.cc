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
#include "midi++/midnam_patch.h"
#include "midi_automation_line.h"
#include "midi_time_axis.h"

#include "i18n.h"

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

string
MidiAutomationLine::get_verbose_cursor_string (double fraction) const
{
	using namespace MIDI::Name;

	if (_parameter.type() != ARDOUR::MidiCCAutomation) {
		return AutomationLine::get_verbose_cursor_string(fraction);
	}

	MidiTimeAxisView* const mtv = dynamic_cast<MidiTimeAxisView*>(trackview.get_parent());
	if (!mtv) {
		return AutomationLine::get_verbose_cursor_string(fraction);
	}

	boost::shared_ptr<MasterDeviceNames> device_names(mtv->get_device_names());
	if (!device_names) {
		return AutomationLine::get_verbose_cursor_string(fraction);
	}

	const std::string& device_mode = mtv->gui_property(X_("midnam-custom-device-mode"));
	const uint8_t      channel     = mtv->get_channel_for_add();

	boost::shared_ptr<const ValueNameList> value_names = device_names->value_name_list_by_control(
		device_mode, channel, _parameter.id());
	if (!value_names) {
		return AutomationLine::get_verbose_cursor_string(fraction);
	}

	const uint16_t cc_value = floor(std::max(std::min(fraction * 127.0, 127.0), 0.0));

	boost::shared_ptr<const Value> value = value_names->max_value_below(cc_value);
	if (!value) {
		return AutomationLine::get_verbose_cursor_string(fraction);
	}

	return value->name();
}

