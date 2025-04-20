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

#include "ardour/route_group.h"
#include "ardour/session.h"

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/menu_elems.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/window_title.h"

#include "ardour_ui.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "rta_manager.h"
#include "rta_window.h"
#include "timers.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

RTAWindow::RTAWindow ()
	: ArdourWindow (_("Realtime Perceptual Analyzer"))
	, _pause (_("Freeze"), ArdourWidgets::ArdourButton::default_elements, true)
	, _visible (false)
	, _margin (24)
	, _min_dB (-60)
	, _max_dB (0)
	, _hovering_dB (false)
	, _dragging_dB (DragNone)
	, _cursor_x (-1)
	, _cursor_y (-1)
{
	_pause.signal_clicked.connect (mem_fun (*this, &RTAWindow::pause_toggled));
	_pause.set_name ("rta freeze button");

	_darea.add_events (Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::POINTER_MOTION_MASK | Gdk::LEAVE_NOTIFY_MASK);
	_darea.signal_size_request ().connect (sigc::mem_fun (*this, &RTAWindow::darea_size_request));
	_darea.signal_size_allocate ().connect (sigc::mem_fun (*this, &RTAWindow::darea_size_allocate));
	_darea.signal_expose_event ().connect (sigc::mem_fun (*this, &RTAWindow::darea_expose_event));
	_darea.signal_button_press_event ().connect (sigc::mem_fun (*this, &RTAWindow::darea_button_press_event));
	_darea.signal_button_release_event ().connect (sigc::mem_fun (*this, &RTAWindow::darea_button_release_event));
	_darea.signal_motion_notify_event ().connect (sigc::mem_fun (*this, &RTAWindow::darea_motion_notify_event));
	_darea.signal_scroll_event ().connect (sigc::mem_fun (*this, &RTAWindow::darea_scroll_event));
	_darea.signal_leave_notify_event ().connect (sigc::mem_fun (*this, &RTAWindow::darea_leave_notify_event), true);
	_darea.signal_grab_broken_event ().connect (sigc::mem_fun (*this, &RTAWindow::darea_grab_broken_event), true);
	_darea.signal_grab_notify ().connect (sigc::mem_fun (*this, &RTAWindow::darea_grab_notify), true);

	_speed_strings.push_back (_("Rapid"));
	_speed_strings.push_back (_("Fast"));
	_speed_strings.push_back (_("Moderate"));
	_speed_strings.push_back (_("Slow"));
	_speed_strings.push_back (_("Noise Measurement"));

	using namespace Gtkmm2ext;
	using PA = ARDOUR::DSP::PerceptualAnalyzer;
	_speed_dropdown.AddMenuElem (MenuElemNoMnemonic (_speed_strings[(int)PA::Rapid], sigc::bind (sigc::mem_fun (*this, &RTAWindow::set_rta_speed), PA::Rapid)));
	_speed_dropdown.AddMenuElem (MenuElemNoMnemonic (_speed_strings[(int)PA::Fast], sigc::bind (sigc::mem_fun (*this, &RTAWindow::set_rta_speed), PA::Fast)));
	_speed_dropdown.AddMenuElem (MenuElemNoMnemonic (_speed_strings[(int)PA::Moderate], sigc::bind (sigc::mem_fun (*this, &RTAWindow::set_rta_speed), PA::Moderate)));
	_speed_dropdown.AddMenuElem (MenuElemNoMnemonic (_speed_strings[(int)PA::Slow], sigc::bind (sigc::mem_fun (*this, &RTAWindow::set_rta_speed), PA::Slow)));
	_speed_dropdown.AddMenuElem (MenuElemNoMnemonic (_speed_strings[(int)PA::Noise], sigc::bind (sigc::mem_fun (*this, &RTAWindow::set_rta_speed), PA::Noise)));
	_speed_dropdown.set_sizing_texts (_speed_strings);
	_speed_dropdown.set_text (_speed_strings[(int)RTAManager::instance ()->rta_speed ()]);

	_warp_strings.push_back (_("Bark"));
	_warp_strings.push_back (_("Medium"));
	_warp_strings.push_back (_("High"));

	_warp_dropdown.AddMenuElem (MenuElemNoMnemonic (_warp_strings[(int)PA::Bark], sigc::bind (sigc::mem_fun (*this, &RTAWindow::set_rta_warp), PA::Bark)));
	_warp_dropdown.AddMenuElem (MenuElemNoMnemonic (_warp_strings[(int)PA::Medium], sigc::bind (sigc::mem_fun (*this, &RTAWindow::set_rta_warp), PA::Medium)));
	_warp_dropdown.AddMenuElem (MenuElemNoMnemonic (_warp_strings[(int)PA::High], sigc::bind (sigc::mem_fun (*this, &RTAWindow::set_rta_warp), PA::High)));
	_warp_dropdown.set_sizing_texts (_warp_strings);
	_warp_dropdown.set_text (_warp_strings[(int)RTAManager::instance ()->rta_warp ()]);

	_ctrlbox.set_spacing (4);
	_ctrlbox.pack_start (*manage (new Gtk::Label (_("Speed:"))), false, false);
	_ctrlbox.pack_start (_speed_dropdown, false, false);
	_ctrlbox.pack_start (*manage (new Gtk::Label (_("Warp:"))), false, false);
	_ctrlbox.pack_start (_warp_dropdown, false, false);
	_ctrlbox.pack_start (_pointer_info, false, false, 5);
	_ctrlbox.pack_end (_pause, false, false);

	_vpacker.pack_start (_darea, true, true);
	_vpacker.pack_start (_ctrlbox, false, false, 5);

	add (_vpacker);
	set_border_width (4);
	_vpacker.show_all ();

	Gtkmm2ext::UI::instance ()->theme_changed.connect (sigc::mem_fun (*this, &RTAWindow::on_theme_changed));
	UIConfiguration::instance ().ColorsChanged.connect (sigc::mem_fun (*this, &RTAWindow::on_theme_changed));
	UIConfiguration::instance ().DPIReset.connect (sigc::mem_fun (*this, &RTAWindow::on_theme_changed));

	on_theme_changed ();
}

void
RTAWindow::on_theme_changed ()
{
	_basec = UIConfiguration::instance ().color (X_("gtk_bases")); // gtk_darkest
	_gridc = UIConfiguration::instance ().color (X_("gtk_background"));
	_textc = UIConfiguration::instance ().color (X_("gtk_foreground"));

	_margin  = 2 * ceilf (12.f * UIConfiguration::instance ().get_ui_scale ());
	_uiscale = std::max<float> (1.f, sqrtf (UIConfiguration::instance ().get_ui_scale ()));

	_grid.clear ();
	_xpos.clear ();
	_darea.queue_resize ();
	_darea.queue_draw ();
}

XMLNode&
RTAWindow::get_state () const
{
	XMLNode* node = new XMLNode ("RTAWindow");
	node->set_property (X_("min-dB"), _min_dB);
	node->set_property (X_("max-dB"), _max_dB);
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

	XMLNode* node = _session->instant_xml (X_("RTAWindow"));
	if (node) {
		node->get_property ("min-dB", _min_dB);
		node->get_property ("max-dB", _max_dB);

		if (_max_dB > _dB_min + _dB_range) {
			_max_dB = _dB_min + _dB_range;
		}
		if (_min_dB < _dB_min) {
			_min_dB = _dB_min;
		}
		if (_max_dB - _min_dB < _dB_span) {
			_max_dB = 0;
			_min_dB = -60;
		}
	}

	update_title ();
	_session->DirtyChanged.connect (_session_connections, invalidator (*this), std::bind (&RTAWindow::update_title, this), gui_context ());

	_pause.set_active (false);

	RTAManager::instance ()->SignalReady.connect_same_thread (_rta_connections, [this] () { _darea.queue_draw (); });
	RTAManager::instance ()->SettingsChanged.connect_same_thread (_rta_connections, [this] () { rta_settings_changed (); });
}

void
RTAWindow::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &RTAWindow::session_going_away);

	_rta_connections.drop_connections ();

	ArdourWindow::session_going_away ();
	_session = 0;

	update_title ();
	_darea.queue_draw ();
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
RTAWindow::on_map ()
{
	_visible = true;
	RTAManager::instance ()->set_active (!_pause.get_active ());

	ArdourWindow::on_map ();
}

void
RTAWindow::on_unmap ()
{
	_visible = false;
	RTAManager::instance ()->set_active (false);

	ArdourWindow::on_unmap ();
}

void
RTAWindow::pause_toggled ()
{
	RTAManager::instance ()->set_active (_visible && !_pause.get_active ());
}

void
RTAWindow::rta_settings_changed ()
{
	_speed_dropdown.set_text (_speed_strings[(int)RTAManager::instance ()->rta_speed ()]);
	_warp_dropdown.set_text (_warp_strings[(int)RTAManager::instance ()->rta_warp ()]);
	_xpos.clear ();
	_darea.queue_draw ();
}

void
RTAWindow::set_rta_speed (DSP::PerceptualAnalyzer::Speed s)
{
	RTAManager::instance ()->set_rta_speed (s);
}

void
RTAWindow::set_rta_warp (DSP::PerceptualAnalyzer::Warp w)
{
	RTAManager::instance ()->set_rta_warp (w);
}

/* log scale grid  20Hz .. 20kHz */
static float
x_at_freq (const float f, const int width)
{
	return width * logf (f / 20.f) / logf (1000.f);
}

static float
freq_at_x (const int x, const int width)
{
	return 20.f * powf (1000.f, x / (float)width);
}

bool
RTAWindow::darea_button_press_event (GdkEventButton* ev)
{
	if (ev->button != 1 || ev->type != GDK_BUTTON_PRESS) {
		return false;
	}
	if (!_hovering_dB) {
		if (!_pause.get_active ()) {
			_pause.set_active_state (Gtkmm2ext::ImplicitActive);
			pause_toggled ();
		}
		return true;
	}

	assert (_dragging_dB == DragNone);

	Gtk::Allocation a      = _darea.get_allocation ();
	float const     height = a.get_height ();

	const float y0 = _margin;
	const float y1 = height - _margin;
	const float hh = y1 - y0;

	const float dBy0 = (y1 - hh * (_max_dB - _dB_min) / _dB_range);
	const float dBy1 = (y1 - hh * (_min_dB - _dB_min) / _dB_range);

	const float dByc = (dBy1 + dBy0) / 2;
	const float dByr = (dBy1 - dBy0) / 2;

	if (ev->y < dByc - dByr * .8) {
		_dragging_dB  = DragUpper;
		_dragstart_dB = _max_dB;
	} else if (ev->y > dByc + dByr * .8) {
		_dragging_dB  = DragLower;
		_dragstart_dB = _min_dB;
	} else {
		_dragging_dB  = DragRange;
		_dragstart_dB = _min_dB;
	}
	_dragstart_y = ev->y;

	_darea.add_modal_grab ();
	_darea.queue_draw ();

	return true;
}

bool
RTAWindow::darea_button_release_event (GdkEventButton* ev)
{
	if (ev->button != 1) {
		return false;
	}

	if (_pause.active_state () == Gtkmm2ext::ImplicitActive) {
		_pause.set_active_state (Gtkmm2ext::Off);
		pause_toggled ();
	}

	bool changed = false;

	if (_dragging_dB != DragNone) {
		_dragging_dB = DragNone;
		changed = true;
		_darea.remove_modal_grab ();
	}

	if (_hovering_dB) {
		_hovering_dB = false;
		changed = true;
	}

	if (changed) {
		_darea.queue_draw ();
	}

	return true;
}

bool
RTAWindow::darea_leave_notify_event (GdkEventCrossing*)
{
	if (_hovering_dB) {
		_hovering_dB = false;
		_darea.queue_draw ();
	} else if (_cursor_x >= 0 || _cursor_y >= 0) {
		_darea.queue_draw ();
	}
	_pointer_info.set_text ("");
	_cursor_x = _cursor_y = -1;
	return false;
}

bool
RTAWindow::darea_grab_broken_event (GdkEventGrabBroken*)
{
	if (_dragging_dB != DragNone) {
		_darea.remove_modal_grab ();
		_dragging_dB = DragNone;
		_darea.queue_draw ();
		return true;
	}
	return false;
}

void
RTAWindow::darea_grab_notify (bool was_grabbed)
{
	if (!was_grabbed) {
		_darea.remove_modal_grab ();
		_dragging_dB = DragNone;
		_darea.queue_draw ();
	}
}

bool
RTAWindow::darea_motion_notify_event (GdkEventMotion* ev)
{
	Gtk::Allocation a      = _darea.get_allocation ();
	float const     width  = a.get_width ();
	float const     height = a.get_height ();

	if (_dragging_dB != DragNone) {
		const float hh      = height - 2 * _margin;
		const float dx      = _dragstart_y - ev->y;
		const float dBpx    = _dB_range / hh;
		const float dB      = dx * dBpx;
		float       new_dB  = _dragstart_dB + dB;
		bool        changed = false;

		if (_dragging_dB == DragUpper) {
			new_dB  = rintf (std::min (_dB_min + _dB_range, std::max (new_dB, _min_dB + _dB_span)));
			changed = _max_dB != new_dB;
			_max_dB = new_dB;
		} else if (_dragging_dB == DragLower) {
			new_dB  = rintf (std::max (_dB_min, std::min (new_dB, _max_dB - _dB_span)));
			changed = _min_dB != new_dB;
			_min_dB = new_dB;
		} else {
			float min_dB = rintf (std::max (_dB_min, std::min (new_dB, _max_dB - _dB_span)));
			float dbd    = (min_dB - _min_dB);
			float max_dB = rintf (std::min (_dB_min + _dB_range, std::max (_max_dB + dbd, _min_dB + _dB_span)));
			dbd          = std::min<float> (dbd, max_dB - _max_dB);

			changed = dbd != 0;
			_max_dB += dbd;
			_min_dB += dbd;
		}

		if (changed) {
			_grid.clear ();
			_darea.queue_draw ();
		}

		return true;
	}

	bool queue_draw = false;

	int twomargin = 2 * _margin;
	if (ev->x > _margin && ev->x < width - _margin && ev->y > _margin && ev->y < height - _margin) {
		float freq = freq_at_x (ev->x - _margin, width - twomargin);

		std::stringstream ss;
		ss << std::fixed;
		if (freq >= 10000) {
			ss << std::setprecision (1) << freq / 1000.0 << "kHz";
		} else if (freq >= 1000) {
			ss << std::setprecision (2) << freq / 1000.0 << "kHz";
		} else {
			ss << std::setprecision (0) << freq << "Hz";
		}

		float dB = _min_dB + (height - _margin - ev->y) * (_max_dB - _min_dB) / (height - twomargin);
		ss << " " << std::setw (6) << std::setprecision (1) << std::showpos << dB;
		ss << std::setw (0) << "dB";
		_pointer_info.set_text (ss.str ());

		if (ev->x != _cursor_x || ev->y != _cursor_y) {
			queue_draw = true;
		}
		_cursor_x = ev->x;
		_cursor_y = ev->y;

	} else {
		_pointer_info.set_text ("");
		if (_cursor_x >= 0 || _cursor_y >= 0) {
			queue_draw = true;
		}
		_cursor_x = _cursor_y = -1;
	}

	bool h = ev->x > (width - _margin);
	if (h == _hovering_dB && !queue_draw) {
		return true;
	}
	_hovering_dB = h;
	_darea.queue_draw ();

	return true;
}

bool
RTAWindow::darea_scroll_event (GdkEventScroll* ev)
{
	if (_dragging_dB != DragNone || !_hovering_dB) {
		return true;
	}

	float delta = 0;
	switch (ev->direction) {
		case GDK_SCROLL_UP:
			delta = 1;
			break;
		case GDK_SCROLL_DOWN:
			delta = -1;
			break;
		default:
			return true;
	}

	using Gtkmm2ext::Keyboard;

	if (Keyboard::modifier_state_equals (ev->state, Keyboard::ScrollHorizontalModifier)) {
		_min_dB = rintf (std::max (_dB_min, std::min (_min_dB - delta, _max_dB - _dB_span)));
		_max_dB = rintf (std::min (_dB_min + _dB_range, std::max (_max_dB + delta, _min_dB + _dB_span)));
	} else {
		float new_dB = _min_dB + delta;
		/* compare to DragRange */
		float min_dB = rintf (std::max (_dB_min, std::min (new_dB, _max_dB - _dB_span)));
		float dbd    = (min_dB - _min_dB);
		float max_dB = rintf (std::min (_dB_min + _dB_range, std::max (_max_dB + dbd, _min_dB + _dB_span)));
		dbd          = std::min<float> (dbd, max_dB - _max_dB);
		_max_dB += dbd;
		_min_dB += dbd;
	}

	_grid.clear ();
	_darea.queue_draw ();
	return true;
}

void
RTAWindow::darea_size_allocate (Gtk::Allocation&)
{
	_grid.clear ();
	_xpos.clear ();
}

void
RTAWindow::darea_size_request (Gtk::Requisition* req)
{
	req->width  = 512 *_uiscale + 2 * _margin;
	req->height = req->width * 9 / 17;
}

bool
RTAWindow::darea_expose_event (GdkEventExpose* ev)
{
	Gtk::Allocation a      = _darea.get_allocation ();
	float const     width  = a.get_width ();
	float const     height = a.get_height ();

	const float min_dB = _min_dB;
	const float max_dB = _max_dB;

	const float x0 = _margin;
	const float x1 = width - _margin;
	const float ww = x1 - x0;

	const float y0 = _margin;
	const float y1 = height - _margin;
	const float hh = y1 - y0;

	if (!_grid) {
		_grid = Cairo::ImageSurface::create (Cairo::FORMAT_RGB24, width, height);

		Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create (_grid);
		Gtkmm2ext::set_source_rgb_a (cr, _basec, 1);

		cr->paint ();
		cr->set_line_width (1.0);

		std::map<float, std::string> grid;
		grid[20]    = "20";
		grid[25]    = "";
		grid[31.5]  = "";
		grid[40]    = "40";
		grid[50]    = "";
		grid[63]    = "";
		grid[80]    = "80";
		grid[100]   = "";
		grid[125]   = "";
		grid[160]   = "160";
		grid[200]   = "";
		grid[250]   = "";
		grid[315]   = "315";
		grid[400]   = "";
		grid[500]   = "";
		grid[630]   = "630";
		grid[800]   = "";
		grid[1000]  = "";
		grid[1250]  = "1K25";
		grid[1600]  = "";
		grid[2000]  = "";
		grid[2500]  = "2K5";
		grid[3150]  = "";
		grid[4000]  = "";
		grid[5000]  = "5K";
		grid[6300]  = "";
		grid[8000]  = "";
		grid[10000] = "10K";
		grid[12500] = "";
		grid[16000] = "";
		grid[20000] = "20K";

		Gtkmm2ext::set_source_rgb_a (cr, _gridc, 1);
		Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (cr);
		layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());

		for (const auto& [f, txt] : grid) {
			const float xx = rintf (x0 + x_at_freq (f, ww)) + .5f;

			if (txt.empty ()) {
				cr->move_to (xx, y1);
				cr->line_to (xx, y1 + 4);
				cr->stroke ();
				continue;
			}

			cr->move_to (xx, y0);
			cr->line_to (xx, y1 + 5);
			cr->stroke ();

			cr->save ();
			layout->set_text (txt);
			layout->set_alignment (Pango::ALIGN_CENTER);
			int tw, th;
			layout->get_pixel_size (tw, th);
			cr->move_to (xx - tw / 2, y1 + 5);
			Gtkmm2ext::set_source_rgb_a (cr, _textc, 0.75);
			layout->show_in_cairo_context (cr);
			cr->restore ();
		}

		/* dB grid */

		std::vector<double> dashes35;
		dashes35.push_back (3.0);
		dashes35.push_back (5.0);

		std::vector<double> dashes2;
		dashes2.push_back (2.0);

		for (int dB = min_dB; dB <= max_dB; ++dB) {
			bool lbl = 0 == (dB % 12) || dB == max_dB;
			if (dB % 6 != 0) {
				continue;
			}
			float y = rintf (y1 - hh * (dB - min_dB) / (max_dB - min_dB)) + .5;

			cr->save ();
			cr->set_line_cap (Cairo::LINE_CAP_ROUND);
			if (!lbl) {
				cr->set_dash (dashes35, 0);
			} else {
				cr->set_dash (dashes2, 0);
			}
			cr->move_to (x0 - (lbl ? 5 : 0), y);
			cr->line_to (x1 + (lbl ? 5 : 0), y);
			cr->stroke ();
			cr->restore ();

			if (!lbl) {
				continue;
			}

			cr->save ();
			Gtkmm2ext::set_source_rgb_a (cr, _textc, 0.75);
			layout->set_text (string_compose ("%1", abs (dB) >= 10 ? abs(dB) : dB));
			layout->set_alignment (Pango::ALIGN_LEFT);
			int tw, th;
			layout->get_pixel_size (tw, th);
			cr->move_to (x1 + 5, y - th / 2);
			layout->show_in_cairo_context (cr);

			layout->set_alignment (Pango::ALIGN_RIGHT);
			cr->move_to (x0 - 5 - tw, y - th / 2);
			layout->show_in_cairo_context (cr);
			cr->restore ();
		}

		/* top/bottom border */
		cr->move_to (x0, y0 + .5);
		cr->line_to (x1, y0 + .5);
		cr->stroke ();

		cr->move_to (x0, y1 + .5);
		cr->line_to (x1, y1 + .5);
		cr->stroke ();
	}

	std::list<RTAManager::RTA> const& rta = RTAManager::instance ()->rta ();

	/* cache x-axis deflection */
	if (_xpos.empty () && !rta.empty ()) {
		auto const& r      = rta.front ();
		auto const& a      = r.analyzers ().front ();
		const int   n_bins = a->fftlen ();
		for (int i = 0; i <= n_bins; ++i) {
			float f = a->freq_at_bin (i);
			if (f < 15 || f > 22000) {
				_xpos[i] = -1;
			} else {
				_xpos[i] = x0 + x_at_freq (f, ww) + 0.5;
			}
		}
	}

	Cairo::RefPtr<Cairo::Context> cr = _darea.get_window ()->create_cairo_context ();

	cr->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cr->clip ();

	cr->set_source (_grid, 0, 0);
	cr->paint ();

	cr->save ();
	cr->rectangle (x0 + 1, y0, ww - 1, hh);
	cr->clip ();

	std::vector<std::pair<std::string, color_t>> legend;

	cr->set_line_width (1.5);
	for (auto const& r : rta) {
		const int n_bins = r.analyzers ().front ()->fftlen ();
		color_t   color;

		RouteGroup* group = r.route ()->route_group ();
		if (r.route ()->is_singleton ()) {
			color = 0xff | _textc;
		} else if (group && group->is_color ()) {
			color = group->rgba ();
		} else {
			color = r.route ()->presentation_info ().color ();
		}

		legend.emplace_back (r.route ()->name (), color);

		float red = UINT_RGBA_R_FLT (color);
		float grn = UINT_RGBA_G_FLT (color);
		float blu = UINT_RGBA_B_FLT (color);
		cr->set_source_rgba (red, grn, blu, 1);

		float last_x = -1;

		for (int i = 0; i <= n_bins; ++i) {
			float x = _xpos[i];
			if (x < 0) {
				continue;
			}
			auto  a  = r.analyzers ().begin ();
			float dB = (*a)->power_at_bin (i, 1.0, true);
			for (++a; a != r.analyzers ().end (); ++a) {
				dB = std::max (dB, (*a)->power_at_bin (i, 1.0, true));
			}
			float y = y1 - hh * (dB - min_dB) / (max_dB - min_dB);
			x       = std::max (std::min (x, x1), x0);
			y       = std::max (std::min (y, y1 + 1), y0 - 1);
			if (last_x < 0) {
				cr->move_to (x, y1 + 1);
			}
			cr->line_to (x, y);
			last_x = x;
		}
		cr->stroke_preserve ();
		assert (last_x > 0);
		cr->line_to (ceil (last_x) + 0.5, y1 + 1);
		cr->close_path ();
		cr->set_source_rgba (red, grn, blu, 0.35);
		cr->fill ();
	}

	cr->restore ();

	if (_hovering_dB || _dragging_dB != DragNone) {
		float m2 = _margin / 2.0;
		float m4 = _margin / 4.0;
		float m8 = _margin / 8.0;

		cr->rectangle (x1 + m2, 0, m2, height);
		Gtkmm2ext::set_source_rgb_a (cr, _textc, 0.3);
		cr->fill ();

		float dBy0 = rintf (y1 - hh * (max_dB - _dB_min) / _dB_range) + .5;
		float dBy1 = rintf (y1 - hh * (min_dB - _dB_min) / _dB_range) + .5;

		Gtkmm2ext::rounded_rectangle (cr, x1 + m2 + m8, dBy0, m4, (dBy1 - dBy0), m8);
		Gtkmm2ext::set_source_rgb_a (cr, _textc, 0.5);
		cr->fill ();
	}

	if (_cursor_x > 0 && _cursor_y > 0) {
		Gtkmm2ext::set_source_rgb_a (cr, _textc, 0.75);
		cr->set_line_width (1.0);
		cr->move_to (_cursor_x + .5, y0);
		cr->line_to (_cursor_x + .5, y1);
		cr->stroke ();
		cr->move_to (x0, _cursor_y + .5);
		cr->line_to (x1, _cursor_y + .5);
		cr->stroke ();
	}

	if (legend.empty ()) {
		return true;
	}

	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (cr);
	layout->set_font_description (UIConfiguration::instance ().get_SmallFont ());
	layout->set_alignment (Pango::ALIGN_LEFT);
	layout->set_text ("8|gGTrackorBusName");

	int tw, th;
	layout->get_pixel_size (tw, th);

	layout->set_ellipsize (Pango::ELLIPSIZE_END);
	layout->set_width (tw * PANGO_SCALE);

	float lw = tw + 10;
	float lh = 5 + (th + 5) * legend.size ();

	float lx = x1 - _margin / 2 - lw;
	float ly = y0 - _margin / 2;

	Gtkmm2ext::rounded_rectangle (cr, lx, ly, lw, lh);
	cr->set_line_width (1.0);
	Gtkmm2ext::set_source_rgb_a (cr, _textc, 0.8);
	cr->stroke_preserve ();
	cr->set_source_rgba (0, 0, 0, 0.5);
	cr->fill_preserve ();
	cr->clip ();

	ly += 5;

	for (const auto& [name, color] : legend) {
		Gtkmm2ext::set_source_rgb_a (cr, color, 1);
		layout->set_text (name);
		cr->move_to (lx + 5, ly);
		layout->show_in_cairo_context (cr);
		ly += 5 + th;
	}

	return true;
}
