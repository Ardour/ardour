/*
 * Copyright (C) 2005-2024 Paul Davis <paul@linuxaudiosystems.com>
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

#include "canvas/rectangle.h"

#include "editing_context.h"
#include "editor_drag.h"
#include "keyboard.h"
#include "pianoroll_automation_line.h"

PianorollAutomationLine::PianorollAutomationLine (const std::string&                      name,
                                              EditingContext&                         ec,
                                              ArdourCanvas::Item&                     parent,
                                              ArdourCanvas::Rectangle*                drag_base,
                                              std::shared_ptr<ARDOUR::AutomationList> al,
                                              const ARDOUR::ParameterDescriptor&      desc)
	: AutomationLine (name, ec, parent, drag_base, al, desc)
{
	_drag_base->set_data ("line", this);
	_drag_base->Event.connect (sigc::mem_fun (*this, &PianorollAutomationLine::base_event_handler));
}

bool
PianorollAutomationLine::base_event_handler (GdkEvent* ev)
{
	if (!sensitive()) {
		return false;
	}
	return _editing_context.typed_event  (_drag_base, ev, AutomationTrackItem);
}

bool
PianorollAutomationLine::event_handler (GdkEvent* ev)
{
	return _editing_context.typed_event (line, ev, EditorAutomationLineItem);
}
