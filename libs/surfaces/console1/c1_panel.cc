/*
 * Copyright (C) 2026 Holger Dehnhardt <holger@dehnhardt.org>
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

#include <algorithm>

#include <ytkmm/drawingarea.h>

#include <cmath>
#include <fstream>
#include <glib/gstdio.h>
#include <glibmm/miscutils.h>

#include "pbd/i18n.h"
#include "pbd/debug.h"
#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"

#include "ardour/stripable.h"
#include "ardour/well_known_enum.h"
#include "gtkmm2ext/gui_thread.h"

#include "c1_panel.h"

using namespace ArdourSurface;
using namespace ARDOUR;
using namespace Gtkmm2ext;

namespace {

const C1LabelPos label_positions[] = {
	{ Console1::DISPLAY_ON, 70, 104, 46 },
	{ Console1::MODE, 118, 104, 40 },
	{ Console1::TRACK_GROUP, 826, 66, 44 },
	{ Console1::ORDER, 927, 91, 36 },
	{ Console1::EXTERNAL_SIDECHAIN, 992, 91, 34 },

	{ Console1::HIGH_CUT, 157, 190, 70 },
	{ Console1::LOW_CUT, 157, 267, 70 },
	{ Console1::GAIN, 85, 299, 70 },
	{ Console1::FILTER_TO_COMPRESSORS, 201, 293, 76 },
	{ Console1::PHASE_INV, 193, 319, 60 },
	{ Console1::PRESET, 190, 345, 60 },

	{ Console1::SHAPE, 275, 130, 70 },
	{ Console1::SHAPE_GATE, 265, 226, 70 },
	{ Console1::HARD_GATE, 267, 288, 54 },
	{ Console1::SHAPE_RELEASE, 338, 191, 70 },
	{ Console1::SHAPE_SUSTAIN, 338, 264, 70 },
	{ Console1::SHAPE_PUNCH, 338, 341, 70 },

	{ Console1::EQ, 440, 130, 70 },
	{ Console1::LOW_SHAPE, 446, 181, 36 },
	{ Console1::HIGH_SHAPE, 676, 181, 36 },
	{ Console1::LOW_MID_SHAPE, 502, 188, 56 },
	{ Console1::HIGH_MID_SHAPE, 577, 188, 56 },
	{ Console1::LOW_FREQ, 427, 267, 66 },
	{ Console1::LOW_MID_FREQ, 502, 267, 66 },
	{ Console1::HIGH_MID_FREQ, 577, 267, 66 },
	{ Console1::HIGH_FREQ, 652, 267, 66 },
	{ Console1::LOW_GAIN, 427, 341, 66 },
	{ Console1::LOW_MID_GAIN, 502, 341, 66 },
	{ Console1::HIGH_MID_GAIN, 577, 341, 66 },
	{ Console1::HIGH_GAIN, 652, 341, 66 },

	{ Console1::COMP, 765, 130, 80 },
	{ Console1::COMP_RATIO, 741, 228, 66 },
	{ Console1::COMP_PAR, 741, 309, 66 },
	{ Console1::COMP_ATTACK, 817, 189, 66 },
	{ Console1::COMP_RELEASE, 817, 265, 66 },
	{ Console1::COMP_THRESH, 817, 341, 66 },
	{ Console1::DRIVE, 905, 189, 66 },
	{ Console1::CHARACTER, 905, 265, 66 },
	{ Console1::PAN, 905, 341, 66 },
	{ Console1::SOLO, 957, 272, 36 },
	{ Console1::MUTE, 995, 272, 36 },
	{ Console1::VOLUME, 979, 347, 66 },
};

} // namespace

C1Panel::C1Panel (Console1& p, std::string const& image_path)
  : c1 (p)
  , edit_mode (false)
  , dragging (-1)
  , drag_dx (0)
  , drag_dy (0)
  , offset_x (0)
{
	add_events (Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::POINTER_MOTION_MASK | Gdk::SCROLL_MASK);
	positions.assign (std::begin (label_positions), std::end (label_positions));
	load_positions ();

	try {
		pixbuf = Gdk::Pixbuf::create_from_file (image_path);
	} catch (Glib::Error const&) {
	}
	c1.LabelsChanged.connect (_connections, invalidator (*this), std::bind (&C1Panel::labels_changed, this), gui_context ());
	c1.ShiftChange.connect (_connections, invalidator (*this), std::bind (&C1Panel::labels_changed, this), gui_context ());
	c1.PluginStateChange.connect (_connections, invalidator (*this), std::bind (&C1Panel::labels_changed, this), gui_context ());
}

std::string
C1Panel::positions_file () const
{
	return Glib::build_filename (ARDOUR::user_config_directory (), "c1mappings", "c1_label_positions.txt");
}

void
C1Panel::load_positions ()
{
	std::ifstream in (positions_file ().c_str ());
	int id;
	double x, y, w, h;
	while (in >> id >> x >> y >> w >> h) {
		for (auto& lp : positions) {
			if (lp.id == id) {
				lp.x = x;
				lp.y = y;
				lp.w = w;
				lp.h = h;
			}
		}
	}
}

void
C1Panel::save_positions () const
{
	g_mkdir_with_parents (Glib::path_get_dirname (positions_file ()).c_str (), 0755);
	std::ofstream out (positions_file ().c_str ());
	for (const auto& lp : positions) {
		out << int (lp.id) << ' ' << lp.x << ' ' << lp.y << ' ' << lp.w << ' ' << lp.h << '\n';
	}
	/* also written in readable form to the debug log so the defaults in c1_panel.cc can be updated */
	for (const auto& lp : positions) {
		DEBUG_TRACE (PBD::DEBUG::Console1, string_compose ("label position: { %1, %2, %3, %4, %5 }\n", int (lp.id), lp.x, lp.y, lp.w, lp.h));
	}
}

void
C1Panel::set_edit_mode (bool e)
{
	edit_mode = e;
	dragging = -1;
	queue_draw ();
}

void
C1Panel::reset_positions ()
{
	positions.assign (std::begin (label_positions), std::end (label_positions));
	::g_unlink (positions_file ().c_str ());
	queue_draw ();
}

int
C1Panel::hit_test (double x, double y) const
{
	for (int i = int (positions.size ()) - 1; i >= 0; --i) {
		const C1LabelPos& lp = positions[i];
		if (std::fabs (x - lp.x) <= lp.w / 2 && std::fabs (y - lp.y) <= lp.h / 2) {
			return i;
		}
	}
	return -1;
}

bool
C1Panel::on_button_press_event (GdkEventButton* ev)
{
	if (!edit_mode || ev->button != 1) {
		return false;
	}
	dragging = hit_test (ev->x - offset_x, ev->y);
	if (dragging >= 0) {
		drag_dx = positions[dragging].x - (ev->x - offset_x);
		drag_dy = positions[dragging].y - ev->y;
	}
	return true;
}

bool
C1Panel::on_motion_notify_event (GdkEventMotion* ev)
{
	if (!edit_mode || dragging < 0) {
		return false;
	}
	positions[dragging].x = std::round (ev->x - offset_x + drag_dx);
	positions[dragging].y = std::round (ev->y + drag_dy);
	queue_draw ();
	return true;
}

bool
C1Panel::on_button_release_event (GdkEventButton*)
{
	if (!edit_mode || dragging < 0) {
		return false;
	}
	dragging = -1;
	save_positions ();
	return true;
}

bool
C1Panel::on_scroll_event (GdkEventScroll* ev)
{
	if (!edit_mode) {
		return false;
	}
	int i = hit_test (ev->x - offset_x, ev->y);
	if (i < 0) {
		return false;
	}
	const double step = ev->direction == GDK_SCROLL_UP ? 2 : -2;
	if (ev->state & GDK_SHIFT_MASK) {
		positions[i].h = std::max (6.0, positions[i].h + step);
	} else {
		positions[i].w = std::max (10.0, positions[i].w + step * 2);
	}
	save_positions ();
	queue_draw ();
	return true;
}

void
C1Panel::labels_changed ()
{
	queue_draw ();
}

void
C1Panel::on_size_request (Gtk::Requisition* r)
{
	r->width  = pixbuf ? pixbuf->get_width () : 0;
	r->height = pixbuf ? pixbuf->get_height () : 0;
}

bool
C1Panel::on_expose_event (GdkEventExpose*)
{
	if (!pixbuf) {
		return true;
	}
	Cairo::RefPtr<Cairo::Context> cr = get_window ()->create_cairo_context ();

	/* centre the picture if the widget is larger than the picture */
	const double ox = std::max (0, (get_width () - pixbuf->get_width ()) / 2);
	offset_x = ox;
	cr->translate (ox, 0);
	Gdk::Cairo::set_source_pixbuf (cr, pixbuf, 0, 0);
	cr->paint ();

	cr->select_font_face ("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);

	for (const C1LabelPos& lp : positions) {
		std::string text = c1.control_label (lp.id);
		if (edit_mode && text.empty ()) {
			text = c1.findControllerNameById (lp.id);
		}

		/* cover the printed name */
		cr->set_source_rgb (0.10, 0.10, 0.11);
		cr->rectangle (lp.x - lp.w / 2, lp.y - lp.h / 2, lp.w, lp.h);
		cr->fill ();

		if (edit_mode) {
			cr->set_source_rgb (1.0, 0.2, 0.2);
			cr->set_line_width (1);
			cr->rectangle (lp.x - lp.w / 2 + 0.5, lp.y - lp.h / 2 + 0.5, lp.w, lp.h);
			cr->stroke ();
		}

		if (text.empty ()) {
			if (!edit_mode) {
				continue;
			}
		}

		if (c1.is_in_plugin_state ()) {
			if (c1.is_shifted ()) {
				cr->set_source_rgb (0.2, 0.85, 0.3);
			} else {
				cr->set_source_rgb (0.45, 0.78, 1.0);
			}
		} else if (c1.is_shifted ()) {
			cr->set_source_rgb (1.0, 0.9, 0.2);
		} else {
			cr->set_source_rgb (1.0, 1.0, 1.0);
		}

		double size = 10;
		Cairo::TextExtents ext;
		do {
			cr->set_font_size (size);
			cr->get_text_extents (text, ext);
			size -= 1;
		} while ((ext.width > lp.w - 2 || ext.height > lp.h - 2) && size > 5);

		cr->move_to (lp.x - ext.width / 2 - ext.x_bearing, lp.y - ext.height / 2 - ext.y_bearing);
		cr->show_text (text);
	}
	return true;
}

std::string
Console1::control_label (ControllerID id) const
{
	const bool shifted = shift_state;
	std::shared_ptr<ARDOUR::Stripable> st = _current_stripable;

	if (id >= ControllerID::FOCUS1 && id <= ControllerID::FOCUS20) {
		return std::string ();
	}

	if (id == ControllerID::PRESET) {
		return _ ("Shift");
	}
	if (id == ControllerID::TRACK_GROUP) {
		return _ ("Plugins");
	}

	if (in_plugin_state) {
		if (id == ControllerID::MUTE) {
			return _ ("Bypass");
		}
		auto l = plugin_labels.find (id);
		return l == plugin_labels.end () ? std::string () : l->second;
	}

	auto has = [&st] (ARDOUR::WellKnownCtrl c) { return st && st->mapped_control (c); };
	const bool harrison_gate = has (Gate_Attack);
	const bool harrison_comp = has (Comp_Attack);

	auto pick = [shifted] (const char* normal, const char* shift) -> std::string {
		return shifted && shift ? std::string (shift) : std::string (normal);
	};

	switch (id) {
		case ControllerID::DISPLAY_ON:
			return _ ("Rude Solo");
		case ControllerID::MODE:
			return _ ("Zoom");
		case ControllerID::EXTERNAL_SIDECHAIN:
			return _ ("Window");
		case ControllerID::ORDER:
			return _ ("Comp Mode");
		case ControllerID::SOLO:
			return swap_solo_mute ? _ ("Mute") : _ ("Solo");
		case ControllerID::MUTE:
			return swap_solo_mute ? _ ("Solo") : _ ("Mute");
		case ControllerID::PHASE_INV:
			return _ ("Phase");
		case ControllerID::GAIN:
			return _ ("Trim");
		case ControllerID::VOLUME:
			return _ ("Gain");
		case ControllerID::PAN:
			return _ ("Pan");
		case ControllerID::FILTER_TO_COMPRESSORS:
			return _ ("Filter");
		case ControllerID::LOW_CUT:
			return _ ("Low Cut");
		case ControllerID::HIGH_CUT:
			return _ ("High Cut");
		case ControllerID::SHAPE:
			return _ ("Gate");
		case ControllerID::HARD_GATE:
			if (harrison_gate) {
				return pick (_ ("Key Filter"), _ ("Key Listen"));
			}
			return pick (_ ("Fast Attack"), _ ("Expand"));
		case ControllerID::SHAPE_GATE:
			return _ ("Threshold");
		case ControllerID::SHAPE_RELEASE:
			return pick (_ ("Release"), _ ("Hysteresis"));
		case ControllerID::SHAPE_SUSTAIN:
			if (harrison_gate) {
				return pick (_ ("Attack"), _ ("Hold"));
			}
			return _ ("Hold");
		case ControllerID::SHAPE_PUNCH:
			return pick (_ ("Depth"), _ ("Key Freq"));
		case ControllerID::EQ:
			return _ ("EQ");
		case ControllerID::LOW_SHAPE:
		case ControllerID::HIGH_SHAPE:
			return _ ("Shape");
		case ControllerID::CHARACTER:
			return _ ("Drive");
		case ControllerID::COMP:
			return _ ("Comp");
		case ControllerID::COMP_THRESH:
			return _ ("Threshold");
		case ControllerID::COMP_ATTACK:
			return harrison_comp ? _ ("Attack") : _ ("Fast/Peak");
		case ControllerID::COMP_RELEASE:
			return _ ("Release");
		case ControllerID::COMP_RATIO:
			return _ ("Ratio");
		case ControllerID::COMP_PAR:
			return has (Comp_Makeup) ? _ ("Makeup") : _ ("Mix");
		case ControllerID::DRIVE:
			return _ ("Key Filter");
		default:
			break;
	}

	/* EQ bands; the shifted functions are mixbus sends */
	const ControllerID freqs[4] = { LOW_FREQ, LOW_MID_FREQ, HIGH_MID_FREQ, HIGH_FREQ };
	const ControllerID gains[4] = { LOW_GAIN, LOW_MID_GAIN, HIGH_MID_GAIN, HIGH_GAIN };
	for (int i = 0; i < 4; ++i) {
		if (id == freqs[i]) {
			return shifted ? string_compose (_ ("Send %1"), i + 1) : std::string (_ ("Freq"));
		}
		if (id == gains[i]) {
			static const char* gain_names[4] = { N_ ("Low Gain"), N_ ("Low Mid Gain"), N_ ("High Mid Gain"), N_ ("High Gain") };
			return shifted ? string_compose (_ ("Send %1"), i + 5) : std::string (_ (gain_names[i]));
		}
	}
	if (id == ControllerID::LOW_MID_SHAPE || id == ControllerID::HIGH_MID_SHAPE) {
		const bool low = (id == ControllerID::LOW_MID_SHAPE);
		if (shifted) {
			return string_compose (_ ("Send %1"), low ? 9 : 10);
		}
		if (switch_eq_q_dials) {
			return string_compose (_ ("Send %1"), low ? 11 : 12);
		}
		return _ ("Q");
	}

	return std::string ();
}
