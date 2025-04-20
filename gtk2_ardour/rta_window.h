/*
 * Copyright (C) 2025 Robin Gareus <robin@gareus.org>
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
#pragma once

#include <glibmm/thread.h>

#include <ytkmm/box.h>
#include <ytkmm/drawingarea.h>

#include "ardour/ardour.h"
#include "ardour/dsp_filter.h"
#include "ardour/session_handle.h"
#include "ardour/types.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"

#include "ardour_window.h"

class RTAWindow : public ArdourWindow
{
public:
	RTAWindow ();

	void     set_session (ARDOUR::Session*);
	XMLNode& get_state () const;

private:
	void on_map ();
	void on_unmap ();

	void session_going_away ();
	void update_title ();
	void on_theme_changed ();
	void rta_settings_changed ();

	void darea_size_request (Gtk::Requisition*);
	void darea_size_allocate (Gtk::Allocation&);
	bool darea_expose_event (GdkEventExpose*);

	bool darea_button_press_event (GdkEventButton*);
	bool darea_button_release_event (GdkEventButton*);
	bool darea_motion_notify_event (GdkEventMotion*);
	bool darea_scroll_event (GdkEventScroll*);
	bool darea_leave_notify_event (GdkEventCrossing*);
	bool darea_grab_broken_event (GdkEventGrabBroken*);
	void darea_grab_notify (bool);

	void set_rta_speed (ARDOUR::DSP::PerceptualAnalyzer::Speed);
	void set_rta_warp (ARDOUR::DSP::PerceptualAnalyzer::Warp);

	void pause_toggled ();

	enum DragStatus {
		DragNone,
		DragUpper,
		DragLower,
		DragRange
	};

	const float _dB_range = 86; // +6 .. -80 dB
	const float _dB_span  = 24;
	const float _dB_min   = -80;

	Gtk::VBox                          _vpacker;
	Gtk::HBox                          _ctrlbox;
	Gtk::DrawingArea                   _darea;
	Gtk::Label                         _pointer_info;
	ArdourWidgets::ArdourButton        _pause;
	ArdourWidgets::ArdourDropdown      _speed_dropdown;
	ArdourWidgets::ArdourDropdown      _warp_dropdown;
	Cairo::RefPtr<Cairo::ImageSurface> _grid;
	bool                               _visible;
	std::vector<std::string>           _speed_strings;
	std::vector<std::string>           _warp_strings;
	std::map<int, float>               _xpos;
	Gtkmm2ext::Color                   _basec;
	Gtkmm2ext::Color                   _gridc;
	Gtkmm2ext::Color                   _textc;
	int                                _margin;
	float                              _uiscale;
	int                                _min_dB;
	int                                _max_dB;
	bool                               _hovering_dB;
	DragStatus                         _dragging_dB;
	float                              _dragstart_y;
	float                              _dragstart_dB;
	int                                _cursor_x;
	int                                _cursor_y;

	PBD::ScopedConnectionList _rta_connections;
};
