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

#include "pbd/ringbuffer.h"

#include "ardour/ardour.h"
#include "ardour/dsp_filter.h"
#include "ardour/session_handle.h"
#include "ardour/types.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"

#include "ardour_window.h"

class RTAWindow
	: public ArdourWindow
	, public PBD::ScopedConnectionList
{
public:
	static RTAWindow* instance ();
	~RTAWindow ();

	void set_session (ARDOUR::Session*);
	XMLNode& get_state () const;

	/* TODO: consider moving this out of the RTAWindow class,
	 * allow to share std::list<RTA> to be used in different
	 * contexts (mixer-strip, RTAWindow, ...)
	 */
	void attach (std::shared_ptr<ARDOUR::Route>);
	void remove (std::shared_ptr<ARDOUR::Route>);
	bool attached (std::shared_ptr<ARDOUR::Route>) const;

	class RTA {
	public:
		RTA (std::shared_ptr<ARDOUR::Route>);
		~RTA ();

		RTA (RTA const&) = delete;

		using RTARingBuffer    = PBD::RingBuffer<ARDOUR::Sample>;
		using RTARingBufferPtr = std::shared_ptr<RTARingBuffer>;

		std::shared_ptr<ARDOUR::Route>  route;
		ARDOUR::samplecnt_t             rate;
		size_t                          blocksize;
		size_t                          stepsize;
		size_t                          offset;
		ARDOUR::DSP::PerceptualAnalyzer analyzer;
		RTARingBufferPtr                ringbuffer;
	};

private:
	RTAWindow ();
	static RTAWindow* _instance;

	void on_map ();
	void on_unmap ();
	bool hide_window (GdkEventAny*);

	void session_going_away ();
	void update_title ();
	void on_theme_changed ();
	void route_removed (std::weak_ptr<ARDOUR::Route>);

	void darea_size_request (Gtk::Requisition*);
	void darea_size_allocate (Gtk::Allocation&);
	bool darea_expose_event (GdkEventExpose*);

	bool darea_button_press_event (GdkEventButton*);
	bool darea_button_release_event (GdkEventButton*);
	bool darea_motion_notify_event (GdkEventMotion*);
	bool darea_leave_notify_event (GdkEventCrossing*);

	void set_rta_speed (ARDOUR::DSP::PerceptualAnalyzer::Speed);
	void set_rta_warp (ARDOUR::DSP::PerceptualAnalyzer::Warp);

	void fast_update ();
	void pause_toggled ();
	void set_active (bool);
	bool run_rta ();

	enum DragStatus {
		DragNone,
		DragUpper,
		DragLower,
		DragRange
	};

	const float _dB_range = 86; // +6 .. -80 dB
	const float _dB_min   = -80;

	Gtk::VBox                              _vpacker;
	Gtk::HBox                              _ctrlbox;
	Gtk::DrawingArea                       _darea;
	Gtk::Label                             _pointer_info;
	ArdourWidgets::ArdourButton            _pause;
	ArdourWidgets::ArdourDropdown          _speed_dropdown;
	ArdourWidgets::ArdourDropdown          _warp_dropdown;
	Cairo::RefPtr<Cairo::ImageSurface>     _grid;
	bool                                   _visible;
	bool                                   _active;
	std::list<RTA>                         _rta;
	ARDOUR::DSP::PerceptualAnalyzer::Speed _speed;
	ARDOUR::DSP::PerceptualAnalyzer::Warp  _warp;
	std::vector<std::string>               _speed_strings;
	std::vector<std::string>               _warp_strings;
	std::map<int, float>                   _xpos;
	Gtkmm2ext::Color                       _basec;
	Gtkmm2ext::Color                       _gridc;
	Gtkmm2ext::Color                       _textc;
	int                                    _margin;
	int                                    _min_dB;
	int                                    _max_dB;
	bool                                   _hovering_dB;
	DragStatus                             _dragging_dB;
	float                                  _dragstart_y;
	float                                  _dragstart_dB;

	sigc::connection _update_connection;
};
