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

#ifndef __ardour_stripable_time_axis_h__
#define __ardour_stripable_time_axis_h__

#include "automation_time_axis.h"
#include "time_axis_view.h"

class StripableTimeAxisView : public TimeAxisView
{
public:
	StripableTimeAxisView (PublicEditor&, ARDOUR::Session*, ArdourCanvas::Canvas& canvas);
	virtual ~StripableTimeAxisView ();

	void set_stripable (boost::shared_ptr<ARDOUR::Stripable>);
	boost::shared_ptr<ARDOUR::Stripable> stripable() const { return _stripable; }

	typedef std::map<Evoral::Parameter, boost::shared_ptr<AutomationTimeAxisView> > AutomationTracks;
	const AutomationTracks& automation_tracks() const { return _automation_tracks; }
	virtual Gtk::CheckMenuItem* automation_child_menu_item (Evoral::Parameter);

	virtual void create_automation_child (const Evoral::Parameter& param, bool show) = 0;
	virtual boost::shared_ptr<AutomationTimeAxisView> automation_child (Evoral::Parameter param, PBD::ID ctrl_id = PBD::ID(0));

	virtual boost::shared_ptr<AutomationLine> automation_child_by_alist_id (PBD::ID);

	void request_redraw ();

	virtual void show_all_automation (bool apply_to_selection = false);
	virtual void show_existing_automation (bool apply_to_selection = false);
	virtual void hide_all_automation (bool apply_to_selection = false);

protected:
	void reset_samples_per_pixel ();
	virtual void set_samples_per_pixel (double);
	void add_automation_child(Evoral::Parameter param, boost::shared_ptr<AutomationTimeAxisView> track, bool show=true);

	virtual void create_gain_automation_child (const Evoral::Parameter &, bool) = 0;
	virtual void create_trim_automation_child (const Evoral::Parameter &, bool) = 0;
	virtual void create_mute_automation_child (const Evoral::Parameter &, bool) = 0;

	void automation_track_hidden (Evoral::Parameter param);

	void update_gain_track_visibility ();
	void update_trim_track_visibility ();
	void update_mute_track_visibility ();

	boost::shared_ptr<ARDOUR::Stripable> _stripable;

	boost::shared_ptr<AutomationTimeAxisView> gain_track;
	boost::shared_ptr<AutomationTimeAxisView> trim_track;
	boost::shared_ptr<AutomationTimeAxisView> mute_track;

	typedef std::map<Evoral::Parameter, Gtk::CheckMenuItem*> ParameterMenuMap;
	/** parameter -> menu item map for the main automation menu */
	ParameterMenuMap _main_automation_menu_map;

	Gtk::CheckMenuItem* gain_automation_item;
	Gtk::CheckMenuItem* trim_automation_item;
	Gtk::CheckMenuItem* mute_automation_item;

	AutomationTracks _automation_tracks;

	ArdourCanvas::Canvas& parent_canvas;
	bool                  no_redraw;
};

#endif /* __ardour_stripable_time_axis_h__ */
