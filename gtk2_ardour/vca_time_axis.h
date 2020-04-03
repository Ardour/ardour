/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_vca_time_axis_h__
#define __ardour_vca_time_axis_h__

#include "widgets/ardour_button.h"

#include "stripable_colorpicker.h"
#include "stripable_time_axis.h"
#include "gain_meter.h"

namespace ArdourCanvas {
	class Canvas;
}

namespace ARDOUR {
	class Session;
	class VCA;
}

class VCATimeAxisView : public StripableTimeAxisView
{
public:
	VCATimeAxisView (PublicEditor&, ARDOUR::Session*, ArdourCanvas::Canvas& canvas);
	virtual ~VCATimeAxisView ();

	boost::shared_ptr<ARDOUR::Stripable> stripable() const;
	ARDOUR::PresentationInfo const & presentation_info () const;

	void set_vca (boost::shared_ptr<ARDOUR::VCA>);
	boost::shared_ptr<ARDOUR::VCA> vca() const { return _vca; }

	std::string name() const;
	Gdk::Color color () const;
	std::string state_id() const;

	void set_height (uint32_t h, TrackHeightMode m = OnlySelf);

	bool marked_for_display () const;
	bool set_marked_for_display (bool);

	void show_all_automation (bool apply_to_selection = false);
	void show_existing_automation (bool apply_to_selection = false);
	void hide_all_automation (bool apply_to_selection = false);

protected:
	boost::shared_ptr<ARDOUR::VCA> _vca;
	ArdourWidgets::ArdourButton    solo_button;
	ArdourWidgets::ArdourButton    mute_button;
	ArdourWidgets::ArdourButton    automation_button;
	ArdourWidgets::ArdourButton    drop_button;
	ArdourWidgets::ArdourButton    number_label;
	GainMeterBase                  gain_meter;
	PBD::ScopedConnectionList      vca_connections;

	void create_gain_automation_child (const Evoral::Parameter &, bool);
	void create_trim_automation_child (const Evoral::Parameter &, bool) {}
	void create_mute_automation_child (const Evoral::Parameter &, bool);

	void create_automation_child (const Evoral::Parameter& param, bool show);
	virtual void build_automation_action_menu (bool);
	void         build_display_menu ();
	Gtk::Menu* automation_action_menu;

	bool name_entry_changed (std::string const&);

	void parameter_changed (std::string const& p);
	void vca_property_changed (PBD::PropertyChange const&);
	void update_vca_name ();
	void set_button_names ();
	void update_solo_display ();
	void update_mute_display ();
	void update_track_number_visibility ();
	bool solo_release (GdkEventButton*);
	bool mute_release (GdkEventButton*);
	bool automation_click (GdkEventButton*);
	bool drop_release (GdkEventButton*);
	void self_delete ();

	void drop_all_slaves ();
	void choose_color ();

private:
	StripableColorDialog _color_picker;
};

#endif /* __ardour_vca_time_axis_h__ */
