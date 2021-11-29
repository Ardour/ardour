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

#include "multi_region_properties_box.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using std::min;
using std::max;

MultiRegionPropertiesBox::MultiRegionPropertiesBox ()
{
	pack_start (table, false, false);

	table.set_homogeneous (false);
	table.set_spacings (4);
	table.set_border_width (8);

	mute_regions_label.set_text (_("Some regions are muted"));

	mute_regions_button.set_text ("Mute All");
	mute_regions_button.set_name ("generic button");
	mute_regions_button.signal_clicked.connect (sigc::mem_fun (*this, &MultiRegionPropertiesBox::mute_selected_regions));

	unmute_regions_button.set_text ("Un-Mute All");
	unmute_regions_button.set_name ("generic button");
	unmute_regions_button.signal_clicked.connect (sigc::mem_fun (*this, &MultiRegionPropertiesBox::unmute_selected_regions));

	int row = 0;
	table.attach(mute_regions_label, 0, 1, row, row+1, Gtk::SHRINK, Gtk::SHRINK );
	table.attach(mute_regions_button, 1, 2, row, row+1, Gtk::SHRINK, Gtk::SHRINK );
	table.attach(unmute_regions_button, 2, 3, row, row+1, Gtk::SHRINK, Gtk::SHRINK );

	Editor::instance().get_selection().RegionsChanged.connect (sigc::mem_fun (*this, &MultiRegionPropertiesBox::region_selection_changed));
}

MultiRegionPropertiesBox::~MultiRegionPropertiesBox ()
{
}

void
MultiRegionPropertiesBox::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);
}

void
MultiRegionPropertiesBox::region_selection_changed ()
{
	timepos_t s, e;
	Selection& selection (Editor::instance().get_selection());
}


void
MultiRegionPropertiesBox::mute_selected_regions ()
{
	Selection& selection (Editor::instance().get_selection());
	for (RegionSelection::iterator s = selection.regions.begin(); s != selection.regions.end(); ++s) {
		ARDOUR::Region* region = (*s)->region().get();
		region->set_muted(true);
	}
}

void
MultiRegionPropertiesBox::unmute_selected_regions ()
{
	Selection& selection (Editor::instance().get_selection());
	for (RegionSelection::iterator s = selection.regions.begin(); s != selection.regions.end(); ++s) {
		ARDOUR::Region* region = (*s)->region().get();
		region->set_muted(false);
	}
}
