/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2011 Paul Davis
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

#include "gtkmm2ext/utils.h"

#include "plugin_pin_dialog.h"
#include "gui_thread.h"
#include "ui_config.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;


PluginPinDialog::PluginPinDialog (boost::shared_ptr<ARDOUR::PluginInsert> pi)
	: ArdourWindow (string_compose (_("Pin Configuration: %1"), pi->name ()))
	, _pi (pi)
{
	assert (pi->owner ()); // Route

	_pi->PluginIoReConfigure.connect (
			_plugin_connections, invalidator (*this), boost::bind (&PluginPinDialog::plugin_reconfigured, this), gui_context()
			);

	_pi->PluginMapChanged.connect (
			_plugin_connections, invalidator (*this), boost::bind (&PluginPinDialog::plugin_reconfigured, this), gui_context()
			);

	darea.signal_expose_event().connect (sigc::mem_fun (*this, &PluginPinDialog::darea_expose_event));

	// TODO min. width depending on # of pins.
	darea.set_size_request(600, 200);

	VBox* vbox = manage (new VBox);
	vbox->pack_start (darea, true, true);
	add (*vbox);
}

PluginPinDialog::~PluginPinDialog()
{
}

void
PluginPinDialog::plugin_reconfigured ()
{
	darea.queue_draw ();
}

void
PluginPinDialog::draw_io_pins (cairo_t* cr, double y0, double width, uint32_t n_total, uint32_t n_midi, bool input)
{
	double dir = input ? 1. : -1.;

	for (uint32_t i = 0; i < n_total; ++i) {
		double x0 = rint ((i + 1) * width / (1. + n_total)) - .5;
		cairo_move_to (cr, x0, y0);
		cairo_rel_line_to (cr, -5.,  -5. * dir);
		cairo_rel_line_to (cr,  0., -25. * dir);
		cairo_rel_line_to (cr, 10.,   0.);
		cairo_rel_line_to (cr,  0.,  25. * dir);
		cairo_close_path  (cr);

		cairo_set_source_rgb (cr, 0, 0, 0);
		cairo_stroke_preserve (cr);

		if (i < n_midi) {
			cairo_set_source_rgb (cr, 1, 0, 0);
		} else {
			cairo_set_source_rgb (cr, 0, 1, 0);
		}
		cairo_fill (cr);
	}
}

double
PluginPinDialog::pin_x_pos (uint32_t i, double x0, double width, uint32_t n_total, uint32_t n_midi, bool midi)
{
	if (!midi) { i += n_midi; }
	return rint (x0 + (i + 1) * width / (1. + n_total)) - .5;
}

void
PluginPinDialog::draw_plugin_pins (cairo_t* cr, double x0, double y0, double width, uint32_t n_total, uint32_t n_midi, bool input)
{
	// see also ProcessorEntry::PortIcon::on_expose_event
	const double dxy = rint (max (6., 8. * UIConfiguration::instance().get_ui_scale()));

	for (uint32_t i = 0; i < n_total; ++i) {
		double x = rint (x0 + (i + 1) * width / (1. + n_total)) - .5;
		cairo_rectangle (cr, x - dxy * .5, input ? y0 - dxy : y0, 1 + dxy, dxy);

		if (i < n_midi) {
			cairo_set_source_rgb (cr, 1, 0, 0);
		} else {
			cairo_set_source_rgb (cr, 0, 1, 0);
		}

		cairo_fill(cr);
	}
}

void
PluginPinDialog::draw_connection (cairo_t* cr, double x0, double x1, double y0, double y1, bool midi)
{
	cairo_move_to (cr, x0, y0);
	cairo_curve_to (cr, x0, y0 + 10, x1, y1 - 10, x1, y1);
	cairo_set_line_width  (cr, 3.0);
	cairo_set_source_rgb (cr, 1, 0, 0);
	if (midi) {
		cairo_set_source_rgb (cr, 1, 0, 0);
	} else {
		cairo_set_source_rgb (cr, 0, 1, 0);
	}
	cairo_stroke (cr);
}

bool
PluginPinDialog::darea_expose_event (GdkEventExpose* ev)
{
	Gtk::Allocation a = darea.get_allocation();
	double const width = a.get_width();
	double const height = a.get_height();

	cairo_t* cr = gdk_cairo_create (darea.get_window()->gobj());

	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	Gdk::Color const bg = get_style()->get_bg (STATE_NORMAL);
	cairo_set_source_rgb (cr, bg.get_red_p (), bg.get_green_p (), bg.get_blue_p ());
	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	ChanCount in, out; // actual configured i/o
	_pi->configured_io (in, out);

	ChanCount sinks = _pi->natural_input_streams ();
	ChanCount sources = _pi->natural_output_streams ();
	uint32_t plugins = _pi->get_count ();

	/* layout sizes */
	// i/o pins
	double y_in = 40;
	double y_out = height - 40;

	// plugin box(es)
	double yc   = rint (height * .5);
	double bxh2 = 18;
	double bxw  = rint ((width * .9) / ((plugins) + .2 * (plugins - 1)));
	double bxw2 = rint (bxw * .5);

	// i/o pins
	const uint32_t pc_in = in.n_total();
	const uint32_t pc_in_midi = in.n_midi();
	const uint32_t pc_out = out.n_total();
	const uint32_t pc_out_midi = out.n_midi();

	cairo_set_line_width  (cr, 1.0);
	draw_io_pins (cr, y_in, width, pc_in, pc_in_midi, true);
	draw_io_pins (cr, y_out, width, pc_out, pc_out_midi, false);

	// draw midi-bypass (behind)
	if (sources.n_midi() == 0 && pc_in_midi > 0 && pc_out_midi > 0) {
		double x0 = rint (width / (1. + pc_in)) - .5;
		double x1 = rint (width / (1. + pc_out)) - .5;
		draw_connection (cr, x0, x1, y_in, y_out, true);
	}

	for (uint32_t i = 0; i < plugins; ++i) {
		double x0 = rint ((i + .5) * width / (double)(plugins)) - .5;

		draw_plugin_pins (cr, x0 - bxw2, yc - bxh2, bxw, sinks.n_total (), sinks.n_midi (), true);
		draw_plugin_pins (cr, x0 - bxw2, yc + bxh2, bxw, sources.n_total (), sources.n_midi (), false);

		cairo_set_source_rgb (cr, .3, .3, .3);
		rounded_rectangle (cr, x0 - bxw2, yc - bxh2, bxw, 2 * bxh2, 7);
		cairo_fill (cr);

		const ChanMapping::Mappings in_map =  _pi->input_map (i).mappings();
		const ChanMapping::Mappings out_map =  _pi->output_map (i).mappings();

		for (ChanMapping::Mappings::const_iterator t = in_map.begin (); t != in_map.end (); ++t) {
			bool is_midi = t->first == DataType::MIDI;
			for (ChanMapping::TypeMapping::const_iterator c = (*t).second.begin (); c != (*t).second.end () ; ++c) {
				uint32_t pn = (*c).first; // pin
				uint32_t pb = (*c).second;
				double c_x0 = pin_x_pos (pb, 0, width, pc_in, pc_in_midi, is_midi);
				double c_x1 = pin_x_pos (pn, x0 - bxw2, bxw, sinks.n_total (), sinks.n_midi (), is_midi);
				draw_connection (cr, c_x0, c_x1, y_in, yc - bxh2, is_midi);
			}
		}

		for (ChanMapping::Mappings::const_iterator t = out_map.begin (); t != out_map.end (); ++t) {
			bool is_midi = t->first == DataType::MIDI;
			for (ChanMapping::TypeMapping::const_iterator c = (*t).second.begin (); c != (*t).second.end () ; ++c) {
				uint32_t pn = (*c).first;  // pin
				uint32_t pb = (*c).second;
				double c_x0 = pin_x_pos (pn, x0 - bxw2, bxw, sources.n_total (), sources.n_midi (), is_midi);
				double c_x1 = pin_x_pos (pb, 0, width, pc_out, pc_out_midi, is_midi);
				draw_connection (cr, c_x0, c_x1, yc + bxh2, y_out, is_midi);
			}
		}
	}

	cairo_destroy (cr);
	return true;
}
