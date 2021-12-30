/*
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2021 Ben Loftis <ben@harrisonconsoles.com>
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

#include <algorithm>
#include "pbd/compose.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/actions.h"

#include "ardour/location.h"
#include "ardour/profile.h"
#include "ardour/session.h"

#include "audio_clock.h"
#include "automation_line.h"
#include "control_point.h"
#include "editor.h"
#include "region_view.h"
#include "trigger_ui.h"

#include "slot_properties_box.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using std::min;
using std::max;

SlotPropertiesBox::SlotPropertiesBox ()
{
	_header_label.set_text(_("Slot Properties:"));
	_header_label.set_alignment(0.0, 0.5);
	pack_start(_header_label, false, false, 6);

	_triggerwidget = manage (new SlotPropertyWidget ());
	_triggerwidget->show();

//	double w;
//	double h;

//	_triggerwidget->size_request (w, h);
//	_triggerwidget->set_size_request (w, h);


	pack_start (*_triggerwidget, true, true);
}

SlotPropertiesBox::~SlotPropertiesBox ()
{
}

void
SlotPropertiesBox::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);
}

void
SlotPropertiesBox::set_slot (TriggerReference tref)
{
	_triggerwidget->set_trigger (tref);
}
