/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2017-2018 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef __ardour_vca_master_strip__
#define __ardour_vca_master_strip__

#include <boost/shared_ptr.hpp>

#include <gtkmm/box.h>
#include <gtkmm/colorselection.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/messagedialog.h>

#include "widgets/ardour_button.h"

#include "axis_view.h"
#include "control_slave_ui.h"
#include "gain_meter.h"
#include "stripable_colorpicker.h"

namespace ARDOUR {
	class GainControl;
	class VCA;
}

class FloatingTextEntry;

class VCAMasterStrip : public AxisView, public Gtk::EventBox
{
public:
	VCAMasterStrip (ARDOUR::Session*, boost::shared_ptr<ARDOUR::VCA>);
	~VCAMasterStrip ();

	boost::shared_ptr<ARDOUR::Stripable> stripable() const;
	ARDOUR::PresentationInfo const & presentation_info () const;

	std::string name() const;
	Gdk::Color color () const;
	std::string state_id() const;
	boost::shared_ptr<ARDOUR::VCA> vca() const { return _vca; }

	static PBD::Signal1<void,VCAMasterStrip*> CatchDeletion;

	bool marked_for_display () const;
	bool set_marked_for_display (bool);

private:
	boost::shared_ptr<ARDOUR::VCA> _vca;
	GainMeter    gain_meter;

	Gtk::Frame                  global_frame;
	Gtk::VBox                   global_vpacker;
	Gtk::HBox                   bottom_padding;
	Gtk::HBox                   solo_mute_box;
	ArdourWidgets::ArdourButton width_button;
	ArdourWidgets::ArdourButton color_button;
	ArdourWidgets::ArdourButton hide_button;
	ArdourWidgets::ArdourButton number_label;
	ArdourWidgets::ArdourButton solo_button;
	ArdourWidgets::ArdourButton mute_button;
	Gtk::Menu*                  context_menu;
	Gtk::MessageDialog*         delete_dialog;
	ArdourWidgets::ArdourButton vertical_button;
	ControlSlaveUI              control_slave_ui;
	PBD::ScopedConnectionList   vca_connections;

	void spill ();
	void spill_change (boost::shared_ptr<ARDOUR::Stripable>);
	void hide_clicked();
	bool width_button_pressed (GdkEventButton *);
	void set_selected (bool);
	bool solo_release (GdkEventButton*);
	bool mute_release (GdkEventButton*);
	void set_width (bool wide);
	void set_solo_text ();
	void solo_changed ();
	void mute_changed ();
	void unassign ();
	void start_name_edit ();
	void finish_name_edit (std::string, int);
	bool vertical_button_press (GdkEventButton*);
	bool number_button_press (GdkEventButton*);
	void vca_property_changed (PBD::PropertyChange const & what_changed);
	void update_vca_name ();
	void build_context_menu ();
	void hide_confirmation (int);
	void self_delete ();
	void remove ();
	void drop_all_slaves ();
	void assign_all_selected ();
	void unassign_all_selected ();

	void parameter_changed (std::string const& p);
	void set_button_names ();
	void update_bottom_padding ();

	void start_color_edit ();
	StripableColorDialog _color_picker;
};


#endif /* __ardour_vca_master_strip__ */
