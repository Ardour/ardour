/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006 Hans Fugal <hans@fugal.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#pragma once

#include <vector>
#include <list>
#include <string>
#include <sys/types.h>

#include <sigc++/signal.h>

#include "pbd/undo.h"
#include "pbd/statefuldestructible.h"
#include "pbd/memento_command.h"

#include "ardour/automation_list.h"
#include "ardour/parameter_descriptor.h"
#include "ardour/types.h"

#include "canvas/types.h"
#include "canvas/container.h"
#include "canvas/poly_line.h"

#include "automation_line.h"

class AutomationLine;
class ControlPoint;
class PointSelection;
class TimeAxisView;
class AutomationTimeAxisView;
class Selectable;
class Selection;
class PublicEditor;


/** A GUI representation of an ARDOUR::AutomationList within the main editor
 * (i.e. in a TimeAxisView
 */

class EditorAutomationLine : public AutomationLine
{
public:
	EditorAutomationLine (const std::string&                name,
	                TimeAxisView&                           tv,
	                ArdourCanvas::Item&                     parent,
	                std::shared_ptr<ARDOUR::AutomationList> al,
	                const ARDOUR::ParameterDescriptor&      desc);


	virtual ~EditorAutomationLine ();

	TimeAxisView& trackview;

   protected:
	virtual bool event_handler (GdkEvent*);
};


