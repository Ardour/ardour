/*
    Copyright (C) 2001-2011 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "option_editor.h"
#include "visibility_group.h"

/** @file rc_option_editor.h
 *  @brief Editing of options which are obtained from and written back to one of the .rc files.
 *
 *  This is subclassed from OptionEditor.  Simple options (e.g. boolean and simple choices)
 *  are expressed using subclasses of Option.  More complex UI elements are represented
 *  using individual classes subclassed from OptionEditorBox.
 */

/** Editor for options which are obtained from and written back to one of the .rc files. */
class RCOptionEditor : public OptionEditor
{
public:
	RCOptionEditor ();

	void populate_sync_options ();

private:
	void parameter_changed (std::string const &);
	void ltc_generator_volume_changed ();
	ARDOUR::RCConfiguration* _rc_config;
	BoolOption* _solo_control_is_listen_control;
	ComboOption<ARDOUR::ListenPosition>* _listen_position;
	VisibilityGroup _mixer_strip_visibility;
	ComboOption<ARDOUR::SyncSource>* _sync_source;
	BoolOption* _sync_framerate;
	BoolOption* _sync_genlock;
	ComboStringOption* _ltc_port;
	HSliderOption* _ltc_volume_slider;
	Gtk::Adjustment* _ltc_volume_adjustment;
	BoolOption* _ltc_send_continuously;

        PBD::ScopedConnection parameter_change_connection;
};
