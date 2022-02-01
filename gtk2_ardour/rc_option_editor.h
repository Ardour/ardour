/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2012-2018 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __gtk_ardour_rc_option_editor_h__
#define __gtk_ardour_rc_option_editor_h__

#include "widgets/tabbable.h"

#include "option_editor.h"
#include "visibility_group.h"
#include "transport_masters_dialog.h"

/** @file rc_option_editor.h
 *  @brief Editing of options which are obtained from and written back to one of the .rc files.
 *
 *  This is subclassed from OptionEditor.  Simple options (e.g. boolean and simple choices)
 *  are expressed using subclasses of Option.  More complex UI elements are represented
 *  using individual classes subclassed from OptionEditorBox.
 */

/** Editor for options which are obtained from and written back to one of the .rc files. */
class RCOptionEditor : public OptionEditorContainer, public ARDOUR::SessionHandlePtr, public ArdourWidgets::Tabbable
{
public:
	RCOptionEditor ();

	void set_session (ARDOUR::Session*);

	Gtk::Window* use_own_window (bool and_fill_it);
	XMLNode& get_state ();

	bool on_key_release_event (GdkEventKey*);

private:
	void parameter_changed (std::string const &);
	void ltc_generator_volume_changed ();
	ARDOUR::RCConfiguration* _rc_config;
	BoolOption* _solo_control_is_listen_control;
	ComboOption<ARDOUR::ListenPosition>* _listen_position;
	VisibilityGroup _mixer_strip_visibility;
	BoolOption* _sync_framerate;
	HSliderOption* _ltc_volume_slider;
	Gtk::Adjustment* _ltc_volume_adjustment;
	BoolOption* _ltc_send_continuously;
	BoolOption* _plugin_prefer_inline;
	TransportMastersWidget _transport_masters_widget;

	PBD::ScopedConnection parameter_change_connection;
	PBD::ScopedConnection engine_started_connection;

	void show_audio_setup ();
	void show_transport_masters ();

	void reset_clip_library_dir ();

	/* plugin actions */
	void plugin_scan_refresh ();
	void plugin_reset_stats ();
	void clear_vst2_cache ();
	void clear_vst2_blacklist ();
	void clear_vst3_cache ();
	void clear_vst3_blacklist ();
	void clear_au_cache ();
	void clear_au_blacklist ();
	void edit_vst_path (std::string const&, std::string const&, sigc::slot<std::string>, sigc::slot<bool, std::string>);
};

#endif /* __gtk_ardour_rc_option_editor_h__ */
