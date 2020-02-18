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

#ifndef __gtk2_ardour_ardour_knob_h__
#define __gtk2_ardour_ardour_knob_h__

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

class LIBWIDGETS_API KnobPersistentTooltip : public Gtkmm2ext::PersistentTooltip
{
public:
	KnobPersistentTooltip (Gtk::Widget* w);

	void start_drag ();
	void stop_drag ();
	bool dragging () const;

private:
	bool _dragging;
};


class LIBWIDGETS_API ArdourKnob : public CairoWidget , public Gtkmm2ext::Activatable
{
public:

	enum Element {
		Arc = 0x1,
		Bevel = 0x2,
		unused2 = 0x4,
		unused3 = 0x8,
		unused4 = 0x10,
		unused5 = 0x20,
	};

	enum Flags {
		NoFlags = 0,
		Detent = 0x1,
		ArcToZero = 0x2,
	};

	ArdourKnob (Element e = default_elements, Flags flags = NoFlags);
	virtual ~ArdourKnob ();

	void set_active_state (Gtkmm2ext::ActiveState);
	void set_visual_state (Gtkmm2ext::VisualState);

	Element elements() const { return _elements; }
	void set_elements (Element);
	void add_elements (Element);
	static Element default_elements;

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

  protected:
	void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);
	void on_size_request (Gtk::Requisition* req);
	void on_size_allocate (Gtk::Allocation&);
	void on_style_changed (const Glib::RefPtr<Gtk::Style>&);
	void on_name_changed ();
	bool on_enter_notify_event (GdkEventCrossing*);
	bool on_leave_notify_event (GdkEventCrossing*);
	bool on_focus_in_event (GdkEventFocus*);
	bool on_focus_out_event (GdkEventFocus*);

	void controllable_changed (bool force_update = false);
	PBD::ScopedConnection watch_connection;


  private:
	Element _elements;
	BindingProxy binding_proxy;

	bool _hovering;
	float _grabbed_x;
	float _grabbed_y;

	float _val; // current value [0..1]
	float _normal; // default value, arc
	float _dead_zone_delta;

	Flags _flags;

	void action_sensitivity_changed ();
	void action_visibility_changed ();
	void action_tooltip_changed ();

	std::string _tooltip_prefix;
	KnobPersistentTooltip _tooltip;
};

} /* namespace */

#endif /* __gtk2_ardour_ardour_knob_h__ */
