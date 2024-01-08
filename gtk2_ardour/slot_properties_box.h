/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __slot_properties_box_h__
#define __slot_properties_box_h__

#include <map>

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include "gtkmm/sizegroup.h"
#include <gtkmm/table.h>

#include "ardour/ardour.h"
#include "ardour/session_handle.h"

#include "widgets/ardour_button.h"
#include "widgets/slider_controller.h"
#include "widgets/ardour_dropdown.h"

#include "gtkmm2ext/cairo_packer.h"

#include "patch_change_widget.h"
#include "trigger_ui.h"

namespace ARDOUR {
	class Session;
	class Location;
}

class TriggerPropertiesBox;
class RegionPropertiesBox;
class RegionOperationsBox;
class MidiCueEditor;
class ClipEditorBox;

class SlotPropertyTable : public TriggerUI, public Gtk::Table
{
  public:
	SlotPropertyTable ();
	~SlotPropertyTable ();

	virtual void on_trigger_set ();

	Glib::RefPtr<Gtk::SizeGroup> _follow_size_group;

	ArdourWidgets::ArdourButton   _color_button;
	Gtk::Label                    _color_label;

	ArdourWidgets::ArdourButton _load_button;

	Gtk::Adjustment                    _velocity_adjustment;
	ArdourWidgets::HSliderController   _velocity_slider;

	Gtk::Table                    _trigger_table;
	Gtk::Table                    _launch_table;
	Gtk::Table                    _follow_table;

	Gtk::Adjustment               _gain_adjustment;
	Gtk::SpinButton               _gain_spinner;
	Gtk::Label                    _gain_label;

	ArdourWidgets::ArdourButton   _patch_button;
	ArdourWidgets::ArdourButton   _allow_button;

	Gtk::Label                    _beat_label;
	Gtk::Label                    _follow_length_label;
	Gtk::Label                    _follow_count_label;

	Gtk::Label                    _left_probability_label;
	Gtk::Label                    _right_probability_label;
	Gtk::Adjustment                    _follow_probability_adjustment;
	ArdourWidgets::HSliderController   _follow_probability_slider;

	Gtk::Adjustment                    _follow_count_adjustment;
	Gtk::SpinButton                    _follow_count_spinner;

	ArdourWidgets::ArdourButton        _use_follow_length_button;
	Gtk::Adjustment                    _follow_length_adjustment;
	Gtk::SpinButton                    _follow_length_spinner;

	ArdourWidgets::ArdourDropdown      _follow_left;
	ArdourWidgets::ArdourDropdown      _follow_right;

	Gtk::Label                    _vel_sense_label;
	Gtk::Label                    _launch_style_label;
	Gtk::Label                    _launch_quant_label;
	Gtk::Label                    _legato_label;
	Gtk::Label                    _isolate_label;

	ArdourWidgets::ArdourButton        _legato_button;

	ArdourWidgets::ArdourButton        _isolate_button;

	ArdourWidgets::ArdourDropdown      _quantize_button;

	ArdourWidgets::ArdourDropdown      _launch_style_button;

	void set_quantize (Temporal::BBT_Offset);
	void set_launch_style (ARDOUR::Trigger::LaunchStyle);
	void set_follow_action (ARDOUR::FollowAction const &, uint64_t);

	void on_trigger_changed (PBD::PropertyChange const& );

	bool allow_button_event (GdkEvent*);
	bool legato_button_event (GdkEvent*);
	void follow_count_event ();

	bool isolate_button_event (GdkEvent*);

	void gain_change_event ();

	bool use_follow_length_event (GdkEvent*);
	void follow_length_event ();

	void probability_adjusted ();
	void velocity_adjusted ();

	void patch_button_event ();

private:
	bool     _ignore_changes;

	PatchChangeTriggerWindow _patch_change_window;
};

class SlotPropertyWidget : public Gtk::VBox
{
  public:
	SlotPropertyWidget ();
	void set_trigger (ARDOUR::TriggerReference tr) const { ui->set_trigger(tr); }

  private:
	SlotPropertyTable* ui;
};

class SlotPropertiesBox : public Gtk::VBox, public ARDOUR::SessionHandlePtr
{
public:
	SlotPropertiesBox ();
	~SlotPropertiesBox ();

	void set_session (ARDOUR::Session*);

	void set_slot (ARDOUR::TriggerReference);

private:
	Gtk::Table table;

	Gtk::Label _header_label;

	SlotPropertyWidget* _triggerwidget;
};

/* XXX probably for testing only */

class SlotPropertyWindow : public Gtk::Window
{
    public:
	SlotPropertyWindow (ARDOUR::TriggerReference);

	bool on_key_press_event (GdkEventKey*);
	bool on_key_release_event (GdkEventKey*);

	TriggerPropertiesBox *_trig_box;
	RegionOperationsBox *_ops_box;
	ClipEditorBox *_trim_box;
	MidiCueEditor* _midi_editor;
};
#endif /* __multi_region_properties_box_h__ */
