/*
 * Copyright (C) 2019 Johannes Mueller <github@johannes-mueller.org>
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

#ifndef ardour_contourdesign_jump_distance_widget_h
#define ardour_contourdesign_jump_distance_widget_h

#include <gtkmm/comboboxtext.h>
#include <gtkmm/box.h>
#include <gtkmm/adjustment.h>

#include "pbd/signals.h"

#include "contourdesign.h"

namespace ArdourSurface
{

class JumpDistanceWidget : public Gtk::HBox
{
public:
	JumpDistanceWidget (JumpDistance dist);
	~JumpDistanceWidget () {}

	JumpDistance get_distance () const { return _distance; }
	void set_distance (JumpDistance dist);

	sigc::signal<void> Changed;

private:
	JumpDistance _distance;

	void update_value ();
	void update_unit ();

	Gtk::Adjustment _value_adj;
	Gtk::ComboBoxText _unit_cb;
};

} /* namespace */

#endif  /* ardour_contourdesign_jump_distance_widget_h */
