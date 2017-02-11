/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "ardour/audioengine.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/keyboard.h"

#include "canvas/colors.h"
#include "canvas/utils.h"

#include "ardour_ui.h"
#include "public_editor.h"
#include "main_clock.h"
#include "mini_timeline.h"
#include "timers.h"
#include "tooltips.h"
#include "ui_config.h"

#include "pbd/i18n.h"

#define PADDING 3
#define BBT_BAR_CHAR "|"

using namespace ARDOUR;

MiniTimeline::MiniTimeline ()
	: _last_update_frame (-1)
	, _clock_mode (AudioClock::Timecode)
	, _time_width (0)
	, _time_height (0)
	, _n_labels (0)
	, _px_per_sample (0)
	, _time_granularity (0)
	, _time_span_samples (0)
	, _marker_height (0)
	, _pointer_x (-1)
	, _pointer_y (-1)
	, _minitl_context_menu (0)
{
	add_events (Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::BUTTON_PRESS_MASK|Gdk::POINTER_MOTION_MASK|Gdk::SCROLL_MASK);

	_layout = Pango::Layout::create (get_pango_context());

	UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &MiniTimeline::set_colors));
	UIConfiguration::instance().DPIReset.connect (sigc::mem_fun (*this, &MiniTimeline::on_name_changed));
	UIConfiguration::instance().DPIReset.connect (sigc::mem_fun (*this, &MiniTimeline::on_name_changed));

	set_name ("minitimeline");

	Location::name_changed.connect (marker_connection, invalidator (*this), boost::bind (&MiniTimeline::update_minitimeline, this), gui_context ());
	Location::end_changed.connect (marker_connection, invalidator (*this), boost::bind (&MiniTimeline::update_minitimeline, this), gui_context ());
	Location::start_changed.connect (marker_connection, invalidator (*this), boost::bind (&MiniTimeline::update_minitimeline, this), gui_context ());
	Location::flags_changed.connect (marker_connection, invalidator (*this), boost::bind (&MiniTimeline::update_minitimeline, this), gui_context ());

	ARDOUR_UI_UTILS::set_tooltip (*this,
			string_compose (_("<b>Navigation Timeline</b>. Use left-click to locate to time position or marker; scroll-wheel to jump, hold %1 for fine gained and %2 + %3 for extra-fine grained control. Right-click to set display range. The display unit is defined by the primary clock."),
				Gtkmm2ext::Keyboard::primary_modifier_name(),
				Gtkmm2ext::Keyboard::primary_modifier_name (),
				Gtkmm2ext::Keyboard::secondary_modifier_name ()));
}

MiniTimeline::~MiniTimeline ()
{
	delete _minitl_context_menu;
	_minitl_context_menu = 0;
}

void
MiniTimeline::session_going_away ()
{
	super_rapid_connection.disconnect ();
	session_connection.drop_connections ();
	SessionHandlePtr::session_going_away ();
	_jumplist.clear ();
	delete _minitl_context_menu;
	_minitl_context_menu = 0;
}

void
MiniTimeline::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);
	if (!s) {
		return;
	}

	assert (!super_rapid_connection.connected ());
	super_rapid_connection = Timers::super_rapid_connect (
			sigc::mem_fun (*this, &MiniTimeline::super_rapid_update)
			);

	_session->config.ParameterChanged.connect (session_connection,
			invalidator (*this),
			boost::bind (&MiniTimeline::parameter_changed, this, _1), gui_context()
			);
	_session->locations()->added.connect (session_connection,
			invalidator (*this),
			boost::bind (&MiniTimeline::update_minitimeline, this), gui_context()
			);
	_session->locations()->removed.connect (session_connection,
			invalidator (*this),
			boost::bind (&MiniTimeline::update_minitimeline, this), gui_context()
			);
	_session->locations()->changed.connect (session_connection,
			invalidator (*this),
			boost::bind (&MiniTimeline::update_minitimeline, this), gui_context()
			);

	_jumplist.clear ();
	calculate_time_spacing ();
	update_minitimeline ();
}

void
MiniTimeline::on_style_changed (const Glib::RefPtr<Gtk::Style>& old_style)
{
	CairoWidget::on_style_changed (old_style);
	set_colors ();
	calculate_time_width ();
}

void
MiniTimeline::on_name_changed ()
{
	set_colors ();
	calculate_time_width ();

	if (is_realized()) {
		queue_resize ();
	}
}

void
MiniTimeline::set_colors ()
{
	// TODO  UIConfiguration::instance().color & font
}

void
MiniTimeline::parameter_changed (std::string const& p)
{
	if (p == "minitimeline-span") {
		calculate_time_spacing ();
		update_minitimeline ();
	}
}

void
MiniTimeline::on_size_request (Gtk::Requisition* req)
{
	req->width = req->height = 0;
	CairoWidget::on_size_request (req);

	req->width = std::max (req->width, 1);
	req->height = std::max (req->height, 20);
}

void
MiniTimeline::on_size_allocate (Gtk::Allocation& alloc)
{
	CairoWidget::on_size_allocate (alloc);
	calculate_time_spacing ();
}

void
MiniTimeline::set_span (framecnt_t ts)
{
	assert (_session);
	if (_session->config.get_minitimeline_span () == ts) {
		return;
	}

	_session->config.set_minitimeline_span (ts);
	calculate_time_spacing ();
	update_minitimeline ();
}

void
MiniTimeline::super_rapid_update ()
{
	if (!_session || !_session->engine().running() || !is_mapped ()) {
		return;
	}
	framepos_t const frame = PublicEditor::instance().playhead_cursor_sample ();
	AudioClock::Mode m = ARDOUR_UI::instance()->primary_clock->mode();

	bool change = false;
	if (fabs ((_last_update_frame - frame) * _px_per_sample) >= 1.0) {
		change = true;
	}

	if (m != _clock_mode) {
		_clock_mode = m;
		calculate_time_width ();
		change = true;
	}

	if (_clock_mode == AudioClock::BBT) {
		// TODO check if tempo-map changed
		change = true;
	}

	if (change) {
		_last_update_frame = frame;
		update_minitimeline ();
	}
}

void
MiniTimeline::update_minitimeline ()
{
	CairoWidget::set_dirty ();
}

void
MiniTimeline::calculate_time_width ()
{
	switch (_clock_mode) {
		case AudioClock::Timecode:
			_layout->set_text (" 88:88:88,888 ");
			break;
		case AudioClock::BBT:
			_layout->set_text ("888|88|8888");
			break;
		case AudioClock::MinSec:
			_layout->set_text ("88:88:88,88");
			break;
		case AudioClock::Frames:
			_layout->set_text ("8888888888");
			break;
	}
	_layout->get_pixel_size (_time_width, _time_height);
}

void
MiniTimeline::calculate_time_spacing ()
{
	_n_labels = floor (get_width () / (_time_width * 1.15));

	if (_n_labels == 0 || !_session) {
		return;
	}

	const framecnt_t time_span = _session->config.get_minitimeline_span () / 2;
	_time_span_samples = time_span * _session->nominal_frame_rate ();
	_time_granularity = _session->nominal_frame_rate () * ceil (2. * time_span / _n_labels);
	_px_per_sample = get_width () / (2. * _time_span_samples);
	//_px_per_sample = 1.0 / round (1.0 / _px_per_sample);
}

void
MiniTimeline::format_time (framepos_t when)
{
	switch (_clock_mode) {
		case AudioClock::Timecode:
			{
				Timecode::Time TC;
				_session->timecode_time (when, TC);
				// chop of leading space or minus.
				_layout->set_text (Timecode::timecode_format_time (TC).substr(1));
			}
			break;
		case AudioClock::BBT:
			{
				char buf[64];
				Timecode::BBT_Time BBT = _session->tempo_map().bbt_at_frame (when);
				snprintf (buf, sizeof (buf), "%03" PRIu32 BBT_BAR_CHAR "%02" PRIu32 BBT_BAR_CHAR "%04" PRIu32,
						BBT.bars, BBT.beats, BBT.ticks);
				_layout->set_text (buf);
			}
			break;
		case AudioClock::MinSec:
			{
				char buf[32];
				AudioClock::print_minsec (when, buf, sizeof (buf), _session->frame_rate());
				_layout->set_text (std::string(buf).substr(1));
			}
			break;
		case AudioClock::Frames:
			{
				char buf[32];
				snprintf (buf, sizeof (buf), "%" PRId64, when);
				_layout->set_text (buf);
			}
			break;
	}
}

void
MiniTimeline::draw_dots (cairo_t* cr, int left, int right, int y, ArdourCanvas::Color color)
{
	if (left + 1 >= right) {
		return;
	}
	cairo_move_to (cr, left + .5, y + .5);
	cairo_line_to (cr, right - .5, y + .5);
	ArdourCanvas::set_source_rgb_a(cr, color, 0.3);
	const double dashes[] = { 0, 1 };
	cairo_set_dash (cr, dashes, 2, 1);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_width (cr, 1.0);
	cairo_stroke (cr);
	cairo_set_dash (cr, 0, 0, 0);
}

int
MiniTimeline::draw_mark (cairo_t* cr, int x0, int x1, const std::string& label, bool& prelight)
{
	int h = _marker_height;
	/* ArdourMarker shape
	 * MH = 13
	 *
	 * Mark:
	 *
	 *  (0,0)   --  (6,0)
	 *    |           |
	 *    |           |
	 * (0,MH*.4)  (6,MH*.4)
	 *     \         /
	 *        (3,MH)
	 */

	const int y = PADDING;
	int w2 = (h - 1) / 4;
	double h0 = h * .4;
	double h1 = h - h0;

	int lw, lh;
	_layout->set_text (label);
	_layout->get_pixel_size (lw, lh);
	int rw = std::min (x1, x0 + w2 + lw + 2);

	if (_pointer_y >= 0 && _pointer_y <= y + h && _pointer_x >= x0 && _pointer_x <= rw) {
		prelight = true;
	}

	// TODO cache in set_colors()
	uint32_t color = UIConfiguration::instance().color (
			prelight ? "entered marker" : "location marker");

	double r, g, b, a;
	ArdourCanvas::color_to_rgba (color, r, g, b, a);

	if (rw < x0) {
		rw = x1;
	} else {
		cairo_save (cr);
		cairo_rectangle (cr, x0, y, rw - x0, h);
		cairo_set_source_rgba (cr, r, g, b, 0.5); // this should use a shaded color
		cairo_fill_preserve (cr);
		cairo_clip (cr);

		// marker label
		cairo_move_to (cr, x0 + w2, y + .5 * (h - lh));
		cairo_set_source_rgb (cr, 0, 0, 0);
		pango_cairo_show_layout (cr, _layout->gobj());
		cairo_restore (cr);
	}

	// draw marker on top
	cairo_move_to (cr, x0 - .5, y + .5);
	cairo_rel_line_to (cr, -w2 , 0 );
	cairo_rel_line_to (cr, 0, h0);
	cairo_rel_line_to (cr, w2, h1);
	cairo_rel_line_to (cr, w2, -h1);
	cairo_rel_line_to (cr, 0, -h0);
	cairo_close_path (cr);
	cairo_set_source_rgba (cr, r, g, b, 1.0);
	cairo_set_line_width (cr, 1.0);
	cairo_stroke_preserve (cr);
	cairo_fill (cr);

	return rw;
}

struct LocationMarker {
	LocationMarker (const std::string& l, framepos_t w)
		: label (l), when (w) {}
	std::string label;
	framepos_t  when;
};

struct LocationMarkerSort {
	bool operator() (const LocationMarker& a, const LocationMarker& b) {
		return (a.when < b.when);
	}
};

void
MiniTimeline::render (cairo_t* cr, cairo_rectangle_t*)
{
	// TODO cache, set_colors()
	ArdourCanvas::Color base = UIConfiguration::instance().color ("ruler base");
	ArdourCanvas::Color text = UIConfiguration::instance().color ("ruler text");

	if (_n_labels == 0) {
		return;
	}

	Gtkmm2ext::rounded_rectangle (cr, 0, 0, get_width(), get_height(), 4);
	ArdourCanvas::set_source_rgba(cr, base);
	cairo_fill (cr);

	Gtkmm2ext::rounded_rectangle (cr, PADDING, PADDING, get_width() - PADDING - PADDING, get_height() - PADDING - PADDING, 4);
	cairo_clip (cr);

	if (_session == 0) {
		return;
	}


	/* time */
	const framepos_t p = _last_update_frame;
	const framepos_t lower = (std::max ((framepos_t)0, (p - _time_span_samples)) / _time_granularity) * _time_granularity;

	int dot_left = get_width() * .5 + (lower - p) * _px_per_sample;
	for (int i = 0; i < 2 + _n_labels; ++i) {
		framepos_t when = lower + i * _time_granularity;
		double xpos = get_width() * .5 + (when - p) * _px_per_sample;

		// TODO round to nearest display TC in +/- 1px
		// prefer to display BBT |0  or .0

		int lw, lh;
		format_time (when);
		_layout->get_pixel_size (lw, lh);

		int x0 = xpos - lw / 2.0;
		int y0 = get_height() - PADDING - _time_height;

		draw_dots (cr, dot_left, x0, y0 + _time_height * .5, text);

		cairo_move_to (cr, x0, y0);
		ArdourCanvas::set_source_rgba(cr, text);
		pango_cairo_show_layout (cr, _layout->gobj());
		dot_left = x0 + lw;
	}
	draw_dots (cr, dot_left, get_width(), get_height() - PADDING - _time_height * .5, text);

	/* locations */
	framepos_t lmin = std::max ((framepos_t)0, (p - _time_span_samples));
	framepos_t lmax = p + _time_span_samples;

	int tw, th;
	_layout->set_text (X_("Marker@"));
	_layout->get_pixel_size (tw, th);

	_marker_height = th + 2;
	assert (_marker_height > 4);
	const int mw = (_marker_height - 1) / 4;

	lmin -= mw / _px_per_sample;
	lmax += mw / _px_per_sample;

	std::vector<LocationMarker> lm;

	const Locations::LocationList& ll (_session->locations ()->list ());
	for (Locations::LocationList::const_iterator l = ll.begin(); l != ll.end(); ++l) {
		if ((*l)->is_session_range ()) {
			framepos_t when = (*l)->start ();
			if (when >= lmin && when <= lmax) {
				lm.push_back (LocationMarker(_("start"), when));
			}
			when = (*l)->end ();
			if (when >= lmin && when <= lmax) {
				lm.push_back (LocationMarker(_("end"), when));
			}
			continue;
		}

		if (!(*l)->is_mark () || (*l)->name().substr (0, 4) == "xrun") {
			continue;
		}

		framepos_t when = (*l)->start ();
		if (when < lmin || when > lmax) {
			continue;
		}
		lm.push_back (LocationMarker((*l)->name(), when));
	}

	_jumplist.clear ();

	LocationMarkerSort location_marker_sort;
	std::sort (lm.begin(), lm.end(), location_marker_sort);

	int id = 0;
	for (std::vector<LocationMarker>::const_iterator l = lm.begin(); l != lm.end(); ++id) {
		framepos_t when = (*l).when;
		int x0 = floor (get_width() * .5 + (when - p) * _px_per_sample);
		int x1 = get_width();
		const std::string& label = (*l).label;
		if (++l != lm.end()) {
			x1 = floor (get_width() * .5 + ((*l).when - p) * _px_per_sample) - 1 - mw;
		}
		bool prelight = false;
		x1 = draw_mark (cr, x0, x1, label, prelight);
		_jumplist.push_back (JumpRange (x0 - mw, x1, when, prelight));
	}

	/* playhead on top */
	int xc = get_width () * 0.5f;
	cairo_set_line_width (cr, 1.0);
	cairo_set_source_rgb (cr, 1, 0, 0); // playhead color
	cairo_move_to (cr, xc - .5, 0);
	cairo_rel_line_to (cr, 0, get_height ());
	cairo_stroke (cr);
	cairo_move_to (cr, xc - .5, get_height ());
	cairo_rel_line_to (cr, -3,  0);
	cairo_rel_line_to (cr,  3, -4);
	cairo_rel_line_to (cr,  3,  4);
	cairo_close_path (cr);
	cairo_fill (cr);
}

void
MiniTimeline::build_minitl_context_menu ()
{
	using namespace Gtk;
	using namespace Gtk::Menu_Helpers;

	assert (_session);

	const framecnt_t time_span = _session->config.get_minitimeline_span ();

	_minitl_context_menu = new Gtk::Menu();
	MenuList& items = _minitl_context_menu->items();

	// ideally this would have a heading (or rather be a sub-menu to "Visible Time")
	std::map<framecnt_t, std::string> spans;
	spans[30]   = _("30 sec");
	spans[60]   = _("1 min");
	spans[120]  = _("2 mins");
	spans[300]  = _("5 mins");
	spans[600]  = _("10 mins");
	spans[1200] = _("20 mins");

	RadioMenuItem::Group span_group;
	for (std::map<framecnt_t, std::string>::const_iterator i = spans.begin (); i != spans.end (); ++i) {
		items.push_back (RadioMenuElem (span_group, i->second, sigc::bind (sigc::mem_fun (*this, &MiniTimeline::set_span), i->first)));
		if (time_span == i->first) {
			static_cast<RadioMenuItem*>(&items.back())->set_active ();
		}
	}
}

void
MiniTimeline::show_minitl_context_menu ()
{
	if (_minitl_context_menu == 0) {
		build_minitl_context_menu ();
	}
	_minitl_context_menu->popup (1, gtk_get_current_event_time());
}

bool
MiniTimeline::on_button_press_event (GdkEventButton *ev)
{
	if (Gtkmm2ext::Keyboard::is_context_menu_event (ev)) {
		if (_session) {
			show_minitl_context_menu ();
		}
		return true;
	}
	return true;
}

bool
MiniTimeline::on_button_release_event (GdkEventButton *ev)
{
	if (!_session) { return true; }
	if (_session->actively_recording ()) { return true; }
	if (ev->y < 0 || ev->y > get_height () || ev->x < 0 || ev->x > get_width ()) {
		return true;
	}

	if (ev->y <= PADDING + _marker_height) {
		for (JumpList::const_iterator i = _jumplist.begin (); i != _jumplist.end(); ++i) {
			if (i->left < ev->x && ev->x < i->right) {
				_session->request_locate (i->to, _session->transport_rolling ());
				return true;
			}
		}
	}

	if (ev->button == 1) {
		framepos_t when = _last_update_frame + (ev->x - get_width() * .5) / _px_per_sample;
		_session->request_locate (std::max ((framepos_t)0, when), _session->transport_rolling ());
	}

	return true;
}

bool
MiniTimeline::on_motion_notify_event (GdkEventMotion *ev)
{
	if (!_session) { return true; }
	if (_session->actively_recording ()) { return true; }

	_pointer_x = ev->x;
	_pointer_y = ev->y;

	bool need_expose = false;

	for (JumpList::const_iterator i = _jumplist.begin (); i != _jumplist.end(); ++i) {
		if (i->left < ev->x && ev->x < i->right && ev->y <= PADDING + _marker_height) {
			if (!(*i).prelight) {
				need_expose = true;
				break;
			}
		} else {
			if ((*i).prelight) {
				need_expose = true;
				break;
			}
		}
	}
	if (need_expose) {
		update_minitimeline ();
	}

	return true;
}

bool
MiniTimeline::on_leave_notify_event (GdkEventCrossing *ev)
{
	CairoWidget::on_leave_notify_event (ev);
	_pointer_x = _pointer_y = -1;
	for (JumpList::const_iterator i = _jumplist.begin (); i != _jumplist.end(); ++i) {
		if ((*i).prelight) {
			update_minitimeline ();
			break;
		}
	}
	return true;
}

bool
MiniTimeline::on_scroll_event (GdkEventScroll *ev)
{
	if (!_session) { return true; }
	if (_session->actively_recording ()) { return true; }
	const framecnt_t time_span = _session->config.get_minitimeline_span ();
	framepos_t when = _session->audible_frame ();

	double scale = time_span / 60.0;

	if (ev->state & Gtkmm2ext::Keyboard::GainFineScaleModifier) {
		if (ev->state & Gtkmm2ext::Keyboard::GainExtraFineScaleModifier) {
			scale = 0.1;
		} else {
			scale = 0.5;
		}
	}

	switch (ev->direction) {
		case GDK_SCROLL_UP:
		case GDK_SCROLL_RIGHT:
			when += scale * _session->nominal_frame_rate ();
			break;
		case GDK_SCROLL_DOWN:
		case GDK_SCROLL_LEFT:
			when -= scale * _session->nominal_frame_rate ();
			break;
		default:
			return true;
			break;
	}
	_session->request_locate (std::max ((framepos_t)0, when), _session->transport_rolling ());
	return true;
}
