/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>

#include "ardour/parameter_descriptor.h"
#include "ardour/parameter_types.h"
#include "ardour/stripable.h"

#include "public_editor.h"
#include "stripable_time_axis.h"
#include "automation_line.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace ARDOUR;
using namespace Gtk;

StripableTimeAxisView::StripableTimeAxisView (PublicEditor& ed, ARDOUR::Session* s, ArdourCanvas::Canvas& canvas)
	: TimeAxisView(s, ed, (TimeAxisView*) 0, canvas)
	, gain_automation_item(NULL)
	, trim_automation_item(NULL)
	, mute_automation_item(NULL)
	, parent_canvas (canvas)
	, no_redraw (false)
{
}

StripableTimeAxisView::~StripableTimeAxisView ()
{
}

void
StripableTimeAxisView::set_stripable (boost::shared_ptr<ARDOUR::Stripable> s)
{
	_stripable = s;
	_editor.ZoomChanged.connect (sigc::mem_fun(*this, &StripableTimeAxisView::reset_samples_per_pixel));
}

void
StripableTimeAxisView::reset_samples_per_pixel ()
{
	set_samples_per_pixel (_editor.get_current_zoom());
}

void
StripableTimeAxisView::set_samples_per_pixel (double fpp)
{
	TimeAxisView::set_samples_per_pixel (fpp);
}


void
StripableTimeAxisView::add_automation_child (Evoral::Parameter param, boost::shared_ptr<AutomationTimeAxisView> track, bool show)
{
	using namespace Menu_Helpers;

	add_child (track);

	if (param.type() != PluginAutomation) {
		/* PluginAutomation is handled by
		 * - RouteTimeAxisView::processor_automation_track_hidden
		 * - RouteTimeAxisView::processor_automation
		 */
		track->Hiding.connect (sigc::bind (sigc::mem_fun (*this, &StripableTimeAxisView::automation_track_hidden), param));
		_automation_tracks[param] = track;
	}

	/* existing state overrides "show" argument */
	bool visible;
	if (track->get_gui_property ("visible", visible)) {
		show = visible;
	}

	/* this might or might not change the visibility status, so don't rely on it */
	track->set_marked_for_display (show);

	if (show && !no_redraw) {
		request_redraw ();
	}
}

void
StripableTimeAxisView::update_gain_track_visibility ()
{
	bool const showit = gain_automation_item->get_active();

	bool visible;
	if (gain_track->get_gui_property ("visible", visible) && visible != showit) {
		gain_track->set_marked_for_display (showit);

		/* now trigger a redisplay */

		if (!no_redraw) {
			 _stripable->gui_changed (X_("visible_tracks"), (void *) 0); /* EMIT_SIGNAL */
		}
	}
}

void
StripableTimeAxisView::update_trim_track_visibility ()
{
	bool const showit = trim_automation_item->get_active();

	bool visible;
	if (trim_track->get_gui_property ("visible", visible) && visible != showit) {
		trim_track->set_marked_for_display (showit);

		/* now trigger a redisplay */

		if (!no_redraw) {
			 _stripable->gui_changed (X_("visible_tracks"), (void *) 0); /* EMIT_SIGNAL */
		}
	}
}

void
StripableTimeAxisView::update_mute_track_visibility ()
{
	bool const showit = mute_automation_item->get_active();

	bool visible;
	if (mute_track->get_gui_property ("visible", visible) && visible != showit) {
		mute_track->set_marked_for_display (showit);

		/* now trigger a redisplay */

		if (!no_redraw) {
			 _stripable->gui_changed (X_("visible_tracks"), (void *) 0); /* EMIT_SIGNAL */
		}
	}
}

Gtk::CheckMenuItem*
StripableTimeAxisView::automation_child_menu_item (Evoral::Parameter param)
{
	assert (param.type() != PluginAutomation);
	ParameterMenuMap::iterator i = _main_automation_menu_map.find (param);
	if (i != _main_automation_menu_map.end()) {
		return i->second;
	}

	return 0;
}

void
StripableTimeAxisView::automation_track_hidden (Evoral::Parameter param)
{
	boost::shared_ptr<AutomationTimeAxisView> track = automation_child (param);

	if (!track) {
		return;
	}

	Gtk::CheckMenuItem* menu = automation_child_menu_item (param);

	if (menu && !_hidden && menu->get_active()) {
		menu->set_active (false);
	}

	if (_stripable && !no_redraw) {
		request_redraw ();
	}
}

boost::shared_ptr<AutomationTimeAxisView>
StripableTimeAxisView::automation_child(Evoral::Parameter param, PBD::ID)
{
	assert (param.type() != PluginAutomation);
	AutomationTracks::iterator i = _automation_tracks.find(param);
	if (i != _automation_tracks.end()) {
		return i->second;
	} else {
		return boost::shared_ptr<AutomationTimeAxisView>();
	}
}

boost::shared_ptr<AutomationLine>
StripableTimeAxisView::automation_child_by_alist_id (PBD::ID alist_id)
{
	for (AutomationTracks::iterator i = _automation_tracks.begin(); i != _automation_tracks.end(); ++i) {
		boost::shared_ptr<AutomationTimeAxisView> atv (i->second);
		std::list<boost::shared_ptr<AutomationLine> > lines = atv->lines();
		for (std::list<boost::shared_ptr<AutomationLine> >::const_iterator li = lines.begin(); li != lines.end(); ++li) {
			if ((*li)->the_list()->id() == alist_id) {
				return *li;
			}
		}
	}
	return boost::shared_ptr<AutomationLine> ();
}

void
StripableTimeAxisView::request_redraw ()
{
	if (_stripable) {
		_stripable->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
	}
}

void
StripableTimeAxisView::show_all_automation (bool apply_to_selection)
{
	/* this protected member should not be called directly */
	assert (!apply_to_selection);
	assert (no_redraw);

	for (AutomationTracks::iterator i = _automation_tracks.begin(); i != _automation_tracks.end(); ++i) {
		i->second->set_marked_for_display (true);

		Gtk::CheckMenuItem* menu = automation_child_menu_item (i->first);

		if (menu) {
			menu->set_active(true);
		}
	}
}

void
StripableTimeAxisView::show_existing_automation (bool apply_to_selection)
{
	/* this protected member should not be called directly */
	assert (!apply_to_selection);
	assert (no_redraw);

	for (AutomationTracks::iterator i = _automation_tracks.begin(); i != _automation_tracks.end(); ++i) {
		if (i->second->has_automation()) {
			i->second->set_marked_for_display (true);

			Gtk::CheckMenuItem* menu = automation_child_menu_item (i->first);
			if (menu) {
				menu->set_active(true);
			}
		}
	}
}

void
StripableTimeAxisView::hide_all_automation (bool apply_to_selection)
{
	/* this protected member should not be called directly */
	assert (!apply_to_selection);
	assert (no_redraw);

	for (AutomationTracks::iterator i = _automation_tracks.begin(); i != _automation_tracks.end(); ++i) {
		i->second->set_marked_for_display (false);

		Gtk::CheckMenuItem* menu = automation_child_menu_item (i->first);

		if (menu) {
			menu->set_active (false);
		}
	}
}
