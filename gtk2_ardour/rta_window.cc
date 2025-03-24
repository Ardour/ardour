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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include "ardour/session.h"

#include "gtkmm2ext/window_title.h"

#include "gui_thread.h"
#include "rta_window.h"
#include "timers.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

RTAWindow::RTAWindow ()
	: ArdourWindow (_("Realtime Perceptual Analyzer"))
{

	_darea.add_events (Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::POINTER_MOTION_MASK);
	_darea.signal_size_request ().connect (sigc::mem_fun (*this, &RTAWindow::darea_size_request));
	_darea.signal_size_allocate ().connect (sigc::mem_fun (*this, &RTAWindow::darea_size_allocate));
	_darea.signal_expose_event ().connect (sigc::mem_fun (*this, &RTAWindow::darea_expose_event));
#if 0
	_darea.signal_button_press_event ().connect (sigc::mem_fun (*this, &RTAWindow::darea_button_press_event));
	_darea.signal_button_release_event ().connect (sigc::mem_fun (*this, &RTAWindow::darea_button_release_event));
	_darea.signal_motion_notify_event ().connect (sigc::mem_fun (*this, &RTAWindow::darea_motion_notify_event));
#endif

	_vpacker.pack_start (_darea, true, true);

	add (_vpacker);
	set_border_width (4);
	_vpacker.show_all ();
}

XMLNode&
RTAWindow::get_state () const
{
	XMLNode* node = new XMLNode ("RTAWindow");
	return *node;
}

void
RTAWindow::set_session (ARDOUR::Session* s)
{
	if (!s) {
		return;
	}
	/* only call SessionHandlePtr::set_session if session is not NULL,
	 * otherwise RTAWindow::session_going_away will never be invoked.
	 */
	ArdourWindow::set_session (s);

	update_title ();
	_session->DirtyChanged.connect (_session_connections, invalidator (*this), std::bind (&RTAWindow::update_title, this), gui_context ());
}

void
RTAWindow::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &RTAWindow::session_going_away);

	ArdourWindow::session_going_away ();
	_session = 0;
	update_title ();
}

void
RTAWindow::update_title ()
{
	if (_session) {
		std::string n;

		if (_session->snap_name () != _session->name ()) {
			n = _session->snap_name ();
		} else {
			n = _session->name ();
		}

		if (_session->dirty ()) {
			n = "*" + n;
		}

		Gtkmm2ext::WindowTitle title (n);
		title += _("Realtime Perceptual Analyzer");
		title += Glib::get_application_name ();
		set_title (title.get_string ());

	} else {
		Gtkmm2ext::WindowTitle title (_("Realtime Perceptual Analyzer"));
		title += Glib::get_application_name ();
		set_title (title.get_string ());
	}
}

void
RTAWindow::darea_size_allocate (Gtk::Allocation&)
{
}

void
RTAWindow::darea_size_request (Gtk::Requisition* req)
{
	req->width = 300;
	req->height = 300;
}

bool
RTAWindow::darea_expose_event (GdkEventExpose* ev)
{
	Gtk::Allocation a = _darea.get_allocation ();
	double const width = a.get_width ();
	double const height = a.get_height ();
	Cairo::RefPtr<Cairo::Context> cr = _darea.get_window()->create_cairo_context ();

	cr->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cr->clip  ();

	return true;
}
