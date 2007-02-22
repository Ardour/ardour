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
#include "redirect_automation_line.h"
#include "audio_time_axis.h"
#include "utils.h"

#include <ardour/session.h>
#include <ardour/ladspa_plugin.h>
#include <ardour/insert.h>
#include <ardour/curve.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

RedirectAutomationLine::RedirectAutomationLine (const string & name, Redirect& rd, uint32_t port, Session& s,
						
						TimeAxisView& tv, ArdourCanvas::Group& parent,
						
						AutomationList& l)
  
        : AutomationLine (name, tv, parent, l),
	  session (s),
	  _redirect (rd),
	  _port (port)
{
        set_verbose_cursor_uses_gain_mapping (false);

	PluginInsert *pi;
	Plugin::ParameterDescriptor desc;

	if ((pi  = dynamic_cast<PluginInsert*>(&_redirect)) == 0) {
		fatal << _("redirect automation created for non-plugin") << endmsg;
		/*NOTREACHED*/
	}

	pi->plugin()->get_parameter_descriptor (_port, desc);

	upper = desc.upper;
	lower = desc.lower;

	if (desc.toggled) {
		no_draw = true;
		return;
	}

	no_draw = false;
	range = upper - lower;

	/* XXX set min/max for underlying curve ??? */
}

string
RedirectAutomationLine::get_verbose_cursor_string (float fraction)
{
	char buf[32];

	snprintf (buf, sizeof (buf), "%.2f", lower + (fraction * range));
	return buf;
}

void
RedirectAutomationLine::view_to_model_y (double& y)
{
	y = lower + (y * range);
}

void
RedirectAutomationLine::model_to_view_y (double& y)
{
	y = (y - lower) / range;
	y = max (0.0, y);
	y = min (y, 1.0);
}

