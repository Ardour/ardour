/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_gtk_trigger_ui_h__
#define __ardour_gtk_trigger_ui_h__

#include "ardour/triggerbox.h"
#include "widgets/ardour_button.h"
#include "widgets/slider_controller.h"
#include "widgets/frame.h"

namespace ArdourWidgets {
	class ArdourButton;
	class HSliderController;
}

class TriggerPropertiesBox;
class RegionPropertiesBox;
class RegionOperationsBox;
class ClipEditorBox;

class TriggerUI : public Gtk::Table //, public sigc::trackable
{
  public:
	TriggerUI ();
	~TriggerUI ();

	void set_trigger (ARDOUR::TriggerReference);

	static std::string follow_action_to_string (ARDOUR::Trigger::FollowAction);
	static ARDOUR::Trigger::FollowAction  string_to_follow_action (std::string const &);
	static std::string quantize_length_to_string (Temporal::BBT_Offset const &);
	static std::string launch_style_to_string (ARDOUR::Trigger::LaunchStyle);

  private:

	/* name editing */
	bool namebox_button_press (GdkEventButton*);
	bool start_rename ();
	void end_rename (bool);
	void entry_changed ();
	void entry_activated ();
	bool entry_focus_in (GdkEventFocus*);
	bool entry_focus_out (GdkEventFocus*);
	bool entry_key_press (GdkEventKey*);
	bool entry_key_release (GdkEventKey*);
	bool entry_button_press (GdkEventButton*);
	void disconnect_entry_signals ();

	ARDOUR::TriggerReference tref;
	ARDOUR::TriggerPtr trigger() const;

	std::list<sigc::connection> _entry_connections;
	bool                        _renaming;
	Gtk::Entry                  _nameentry;
	Gtk::Label                  _name_label;
	Gtk::EventBox               _namebox;
	ArdourWidgets::Frame        _name_frame;

	sigc::connection            _file_chooser_connection;
	ArdourWidgets::ArdourButton        _follow_action_button;

	Gtk::Adjustment                    _velocity_adjustment;
	ArdourWidgets::HSliderController   _velocity_slider;

	Gtk::Label                    _left_probability_label;
	Gtk::Label                    _right_probability_label;
	Gtk::Adjustment                    _follow_probability_adjustment;
	ArdourWidgets::HSliderController   _follow_probability_slider;

	Gtk::Adjustment                    _follow_count_adjustment;
	Gtk::SpinButton                    _follow_count_spinner;

	ArdourWidgets::ArdourDropdown      _follow_left;
	ArdourWidgets::ArdourDropdown      _follow_right;

	ArdourWidgets::ArdourButton        _legato_button;

	ArdourWidgets::ArdourDropdown      _quantize_button;

	ArdourWidgets::ArdourDropdown      _launch_style_button;

	void set_quantize (Temporal::BBT_Offset);
	void set_launch_style (ARDOUR::Trigger::LaunchStyle);
	void set_follow_action (ARDOUR::Trigger::FollowAction, uint64_t);

	void trigger_changed (PBD::PropertyChange);

	bool follow_action_button_event (GdkEvent*);
	bool legato_button_event (GdkEvent*);
	void follow_count_event ();

	void probability_adjusted ();
	void velocity_adjusted ();

	PBD::ScopedConnectionList trigger_connections;
};

class TriggerWidget : public Gtk::VBox
{
  public:
	TriggerWidget ();
	void set_trigger (ARDOUR::TriggerReference tr) const { ui->set_trigger(tr); }

  private:
	TriggerUI* ui;
};

/* XXX probably for testing only */

class TriggerWindow : public Gtk::Window
{
    public:
	TriggerWindow (ARDOUR::TriggerReference);

	bool on_key_press_event (GdkEventKey*);
	bool on_key_release_event (GdkEventKey*);

	TriggerPropertiesBox *_trig_box;
	RegionOperationsBox *_ops_box;
	ClipEditorBox *_trim_box;
};

#endif /* __ardour_gtk_trigger_ui_h__ */
