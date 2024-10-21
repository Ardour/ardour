/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006 Hans Fugal <hans@fugal.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2016 Nick Mainsbridge <mainsbridge@gmail.com>
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

#include "editor_automation_line.h"
#include "public_editor.h"
#include "time_axis_view.h"

using namespace std;
using namespace ARDOUR;

/** @param converter A TimeConverter whose origin_b is the start time of the AutomationList in session samples.
 *  This will not be deleted by EditorAutomationLine.
 */
EditorAutomationLine::EditorAutomationLine (const string&                              name,
                                TimeAxisView&                              tv,
                                ArdourCanvas::Item&                        parent,
                                std::shared_ptr<AutomationList>            al,
                                const ParameterDescriptor&                 desc)
	: AutomationLine (name, tv.editor(), parent, nullptr, al, desc)
	, trackview (tv)
{
	line->set_data ("trackview", &trackview);
}

EditorAutomationLine::~EditorAutomationLine ()
{
}

bool
EditorAutomationLine::event_handler (GdkEvent* event)
{
	return trackview.editor().canvas_line_event (event, line, this);
}
