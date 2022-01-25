/*
 * Copyright (C) 2005-2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_audio_time_axis_h__
#define __ardour_audio_time_axis_h__

#include <gtkmm/table.h>
#include <gtkmm/button.h>
#include <gtkmm/box.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/radiomenuitem.h>
#include <gtkmm/checkmenuitem.h>

#include <list>

#include "ardour/types.h"

#include "ardour_dialog.h"
#include "route_ui.h"
#include "enums.h"
#include "editing.h"
#include "route_time_axis.h"

namespace ARDOUR {
	class Session;
	class RouteGroup;
	class IOProcessor;
	class Processor;
	class Location;
	class AudioPlaylist;
}

class PublicEditor;
class AudioThing;
class AudioStreamView;
class Selection;
class Selectable;
class RegionView;
class AudioRegionView;
class AutomationLine;
class AutomationGainLine;
class AutomationPanLine;
class TimeSelection;
class AutomationTimeAxisView;

class AudioTimeAxisView : public RouteTimeAxisView
{
public:
	AudioTimeAxisView (PublicEditor&, ARDOUR::Session*, ArdourCanvas::Canvas& canvas);
	virtual ~AudioTimeAxisView ();

	void set_route (boost::shared_ptr<ARDOUR::Route>);

	AudioStreamView* audio_view();

	void set_show_waveforms_recording (bool yn);

	void create_automation_child (const Evoral::Parameter& param, bool show);

	void first_idle ();

	void show_all_automation (bool apply_to_selection = false);
	void show_existing_automation (bool apply_to_selection = false);
	void hide_all_automation (bool apply_to_selection = false);

private:
	friend class AudioStreamView;
	friend class AudioRegionView;

	void route_active_changed ();
	void parameter_changed (std::string const &);

	Gtk::Menu* build_mode_menu();
	void build_automation_action_menu (bool);

	void update_control_names ();
};

#endif /* __ardour_audio_time_axis_h__ */

