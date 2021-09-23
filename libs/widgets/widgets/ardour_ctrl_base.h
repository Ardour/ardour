/*
 * Copyright (C) 2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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

#ifndef _WIDGETS_ARDOUR_CTRL_BASE_H_
#define _WIDGETS_ARDOUR_CTRL_BASE_H_

#include <list>
#include <stdint.h>

#include <gtkmm/action.h>

#include "pbd/signals.h"

#include "gtkmm2ext/activatable.h"
#include "gtkmm2ext/cairo_widget.h"
#include "gtkmm2ext/persistent_tooltip.h"

#include "widgets/binding_proxy.h"
#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API CtrlPersistentTooltip : public Gtkmm2ext::PersistentTooltip
{
public:
	CtrlPersistentTooltip (Gtk::Widget* w);

	void start_drag ();
	void stop_drag ();
	bool dragging () const;

private:
	bool _dragging;
};


class LIBWIDGETS_API ArdourCtrlBase : public CairoWidget , public Gtkmm2ext::Activatable
{
public:
	enum Flags {
		NoFlags      = 0x0,
		Detent       = 0x1,
		ArcToZero    = 0x2,
		NoHorizontal = 0x4,
		NoVertical   = 0x8,
		Reverse      = 0x10,
	};

	ArdourCtrlBase (Flags flags = NoFlags);
	virtual ~ArdourCtrlBase ();

	void set_active_state (Gtkmm2ext::ActiveState);
	void set_visual_state (Gtkmm2ext::VisualState);

	void set_tooltip_prefix (std::string pfx) { _tooltip_prefix = pfx; controllable_changed (true); }

	boost::shared_ptr<PBD::Controllable> get_controllable() { return binding_proxy.get_controllable(); }
	void set_controllable (boost::shared_ptr<PBD::Controllable> c);

	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	bool on_scroll_event (GdkEventScroll* ev);
	bool on_motion_notify_event (GdkEventMotion *ev) ;

	void color_handler ();

	sigc::signal<void> StartGesture;
	sigc::signal<void> StopGesture;

	void set_size_request (int, int);

protected:
	virtual void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*) = 0;

	void on_size_request (Gtk::Requisition* req);
	void on_style_changed (const Glib::RefPtr<Gtk::Style>&);
	void on_name_changed ();
	bool on_enter_notify_event (GdkEventCrossing*);
	bool on_leave_notify_event (GdkEventCrossing*);
	bool on_focus_in_event (GdkEventFocus*);
	bool on_focus_out_event (GdkEventFocus*);

	int _req_width;
	int _req_height;

	bool _hovering;

	float _val; // current value [0..1]
	float _normal; // default value, arc

	Flags _flags;
	CtrlPersistentTooltip _tooltip;

private:
	void controllable_changed (bool force_update = false);
	void action_sensitivity_changed ();
	void action_visibility_changed ();
	void action_tooltip_changed ();

	PBD::ScopedConnection watch_connection;

	BindingProxy binding_proxy;

	float _grabbed_x;
	float _grabbed_y;
	float _dead_zone_delta;

	std::string _tooltip_prefix;
};

} /* namespace */

#endif /* __gtk2_ardour_ardour_knob_h__ */
