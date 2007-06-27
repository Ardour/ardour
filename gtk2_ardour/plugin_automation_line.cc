/*
    Copyright (C) 2002-2003 Paul Davis 

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

#include "public_editor.h"
#include "plugin_automation_line.h"
#include "audio_time_axis.h"
#include "utils.h"

#include <ardour/ladspa_plugin.h>
#include <ardour/plugin_insert.h>
#include <ardour/curve.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

ProcessorAutomationLine::ProcessorAutomationLine (const string & name, Processor& proc, ParamID param, 
						TimeAxisView& tv, ArdourCanvas::Group& parent, AutomationList& l)
  
	: AutomationLine (name, tv, parent, l),
	_processor(proc),
	_param(param)
{
	set_verbose_cursor_uses_gain_mapping (false);

	PluginInsert *pi;
	Plugin::ParameterDescriptor desc;

	if ((pi  = dynamic_cast<PluginInsert*>(&_processor)) == 0) {
		fatal << _("insert automation created for non-plugin") << endmsg;
		/*NOTREACHED*/
	}

	pi->plugin()->get_parameter_descriptor (_param, desc);

	_upper = desc.upper;
	_lower = desc.lower;

	if (desc.toggled) {
		no_draw = true;
		return;
	}

	no_draw = false;
	_range = _upper - _lower;

	/* XXX set min/max for underlying curve ??? */
}

string
ProcessorAutomationLine::get_verbose_cursor_string (float fraction)
{
	char buf[32];

	snprintf (buf, sizeof (buf), "%.2f", _lower + (fraction * _range));
	return buf;
}

void
ProcessorAutomationLine::view_to_model_y (double& y)
{
	y = _lower + (y * _range);
}

void
ProcessorAutomationLine::model_to_view_y (double& y)
{
	y = (y - _lower) / _range;
	y = max (0.0, y);
	y = min (y, 1.0);
}

