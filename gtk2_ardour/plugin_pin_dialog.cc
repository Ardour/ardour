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

#include <gtkmm/table.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"

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
	, _strict_io (_("Strict I/O"))
	, _automatic (_("Automatic"))
	, _add_plugin (_("+"))
	, _del_plugin (_("-"))
	, _add_output_audio (_("+"))
	, _del_output_audio (_("-"))
	, _add_output_midi (_("+"))
	, _del_output_midi (_("-"))
	, _pi (pi)
	, _pin_box_size (4)
	, _position_valid (false)
	, _ignore_updates (false)
{
	assert (pi->owner ()); // Route

	_pi->PluginIoReConfigure.connect (
			_plugin_connections, invalidator (*this), boost::bind (&PluginPinDialog::plugin_reconfigured, this), gui_context()
			);

	_pi->PluginMapChanged.connect (
			_plugin_connections, invalidator (*this), boost::bind (&PluginPinDialog::plugin_reconfigured, this), gui_context()
			);

	_pi->PluginConfigChanged.connect (
			_plugin_connections, invalidator (*this), boost::bind (&PluginPinDialog::plugin_reconfigured, this), gui_context()
			);

	// TODO min. width depending on # of pins.
	darea.set_size_request(600, 200);
	_strict_io.set_sensitive (false);

	Label* l;
	int r = 0;
	Table* t = manage (new Table (4, 3));
	t->set_border_width (0);
	t->set_spacings (4);

	l = manage (new Label (_("Track/Bus:"), ALIGN_END));
	t->attach (*l, 0, 1, r, r + 1);
	l = manage (new Label ());
	l->set_ellipsize (Pango::ELLIPSIZE_MIDDLE);
	l->set_width_chars (24);
	l->set_max_width_chars (24);
	l->set_text (_route()->name ());
	t->attach (*l, 1, 3, r, r + 1);
	++r;

	l = manage (new Label (_("Plugin:"), ALIGN_END));
	t->attach (*l, 0, 1, r, r + 1);
	l = manage (new Label ());
	l->set_ellipsize (Pango::ELLIPSIZE_MIDDLE);
	l->set_width_chars (24);
	l->set_max_width_chars (24);
	l->set_text (pi->name ());
	t->attach (*l, 1, 3, r, r + 1);
	++r;

	l = manage (new Label (_("Settings:"), ALIGN_END));
	t->attach (*l, 0, 1, r, r + 1);
	t->attach (_strict_io, 1, 2, r, r + 1, FILL, SHRINK);
	t->attach (_automatic, 2, 3, r, r + 1, FILL, SHRINK);
	++r;

	l = manage (new Label (_("Instances:"), ALIGN_END));
	t->attach (*l, 0, 1, r, r + 1);
	t->attach (_add_plugin, 1, 2, r, r + 1, SHRINK, SHRINK);
	t->attach (_del_plugin, 2, 3, r, r + 1, SHRINK, SHRINK);
	++r;

	l = manage (new Label (_("Audio Out:"), ALIGN_END));
	t->attach (*l, 0, 1, r, r + 1);
	t->attach (_add_output_audio, 1, 2, r, r + 1, SHRINK, SHRINK);
	t->attach (_del_output_audio, 2, 3, r, r + 1, SHRINK, SHRINK);
	++r;

	l = manage (new Label (_("Midi Out:"), ALIGN_END));
	t->attach (*l, 0, 1, r, r + 1);
	t->attach (_add_output_midi, 1, 2, r, r + 1, SHRINK, SHRINK);
	t->attach (_del_output_midi, 2, 3, r, r + 1, SHRINK, SHRINK);
	++r;

	HBox* hbox = manage (new HBox);
	hbox->pack_start (darea, true, true);
	hbox->pack_start (*t, false, true);

	VBox* vbox = manage (new VBox);
	vbox->pack_start (*hbox, true, true);
	add (*vbox);
	vbox->show_all();

	plugin_reconfigured ();

	darea.add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::POINTER_MOTION_MASK);
	darea.signal_size_allocate().connect (sigc::mem_fun (*this, &PluginPinDialog::darea_size_allocate));
	darea.signal_expose_event().connect (sigc::mem_fun (*this, &PluginPinDialog::darea_expose_event));
	darea.signal_button_press_event().connect (sigc::mem_fun (*this, &PluginPinDialog::darea_button_press_event));
	darea.signal_button_release_event().connect (sigc::mem_fun (*this, &PluginPinDialog::darea_button_release_event));
	darea.signal_motion_notify_event().connect (sigc::mem_fun (*this, &PluginPinDialog::darea_motion_notify_event));

	_automatic.signal_clicked.connect (sigc::mem_fun(*this, &PluginPinDialog::automatic_clicked));
	 _add_plugin.signal_clicked.connect (sigc::bind (sigc::mem_fun(*this, &PluginPinDialog::add_remove_plugin_clicked), true));
	 _del_plugin.signal_clicked.connect (sigc::bind (sigc::mem_fun(*this, &PluginPinDialog::add_remove_plugin_clicked), false));

	 _add_output_audio.signal_clicked.connect (sigc::bind (sigc::mem_fun(*this, &PluginPinDialog::add_remove_port_clicked), true, DataType::AUDIO));
	 _del_output_audio.signal_clicked.connect (sigc::bind (sigc::mem_fun(*this, &PluginPinDialog::add_remove_port_clicked), false, DataType::AUDIO));
	 _add_output_midi.signal_clicked.connect (sigc::bind (sigc::mem_fun(*this, &PluginPinDialog::add_remove_port_clicked), true, DataType::MIDI));
	 _del_output_midi.signal_clicked.connect (sigc::bind (sigc::mem_fun(*this, &PluginPinDialog::add_remove_port_clicked), false, DataType::MIDI));
}

PluginPinDialog::~PluginPinDialog()
{
}

void
PluginPinDialog::plugin_reconfigured ()
{
	if (_ignore_updates) {
		return;
	}
	_n_plugins = _pi->get_count ();
	_pi->configured_io (_in, _out);
	_sinks = _pi->natural_input_streams ();
	_sources = _pi->natural_output_streams ();

	_del_plugin.set_sensitive (_n_plugins > 1);
	_del_output_audio.set_sensitive (_out.n_audio () > 0 && _out.n_total () > 1);
	_del_output_midi.set_sensitive (_out.n_midi () > 0 && _out.n_total () > 1);
	_strict_io.set_active (_pi->strict_io());

	update_elements ();
}

void
PluginPinDialog::update_elements ()
{
	_elements.clear ();
	_hover.reset();
	_actor.reset();
	_selection.reset();

	for (uint32_t i = 0; i < _in.n_total (); ++i) {
		int id = (i < _in.n_midi ()) ? i : i - _in.n_midi ();
		_elements.push_back (CtrlWidget (Input, (i < _in.n_midi () ? DataType::MIDI : DataType::AUDIO), id));
	}

	for (uint32_t i = 0; i < _out.n_total (); ++i) {
		int id = (i < _out.n_midi ()) ? i : i - _out.n_midi ();
		_elements.push_back (CtrlWidget (Output, (i < _out.n_midi () ? DataType::MIDI : DataType::AUDIO), id));
	}

	for (uint32_t n = 0; n < _n_plugins; ++n) {
		for (uint32_t i = 0; i < _sinks.n_total(); ++i) {
			_elements.push_back (CtrlWidget (Sink, (i < _sinks.n_midi () ? DataType::MIDI : DataType::AUDIO), i, n));
		}
		for (uint32_t i = 0; i < _sources.n_total(); ++i) {
			_elements.push_back (CtrlWidget (Source, (i < _sources.n_midi () ? DataType::MIDI : DataType::AUDIO), i, n));
		}
	}
	_position_valid = false;
	darea.queue_draw ();
}

void
PluginPinDialog::update_element_pos ()
{
	/* layout sizes */
	const double yc   = rint (_height * .5);
	const double bxh2 = 18;
	const double bxw  = rint ((_width * .9) / ((_n_plugins) + .2 * (_n_plugins - 1)));
	const double bxw2 = rint (bxw * .5);
	const double y_in = 40;
	const double y_out = _height - 40;

	_pin_box_size = rint (max (6., 8. * UIConfiguration::instance().get_ui_scale()));

	for (CtrlElemList::iterator i = _elements.begin(); i != _elements.end(); ++i) {
		switch (i->e->ct) {
			case Input:
				{
					uint32_t idx = i->e->id;
					if (i->e->dt == DataType::AUDIO) { idx += _in.n_midi (); }
					i->x = rint ((idx + 1) * _width / (1. + _in.n_total ())) - 5.5;
					i->y = y_in - 25;
					i->w = 10;
					i->h = 25;
				}
				break;
			case Output:
				{
					uint32_t idx = i->e->id;
					if (i->e->dt == DataType::AUDIO) { idx += _out.n_midi (); }
					i->x = rint ((idx + 1) * _width / (1. + _out.n_total ())) - 5.5;
					i->y = y_out;
					i->w = 10;
					i->h = 25;
				}
				break;
			case Sink:
				{
					const double x0 = rint ((i->e->ip + .5) * _width / (double)(_n_plugins)) - .5 - bxw2;
					i->x = rint (x0 + (i->e->id + 1) * bxw / (1. + _sinks.n_total ())) - .5 - _pin_box_size * .5;
					i->y = yc - bxh2 - _pin_box_size;
					i->w = _pin_box_size + 1;
					i->h = _pin_box_size;
				}
				break;
			case Source:
				{
					const double x0 = rint ((i->e->ip + .5) * _width / (double)(_n_plugins)) - .5 - bxw2;
					i->x = rint (x0 + (i->e->id + 1) * bxw / (1. + _sources.n_total ())) - .5 - _pin_box_size * .5;
					i->y = yc + bxh2;
					i->w = _pin_box_size + 1;
					i->h = _pin_box_size;
				}
				break;
		}
	}

}

void
PluginPinDialog::set_color (cairo_t* cr, bool midi)
{
	// see also gtk2_ardour/processor_box.cc
	static const uint32_t audio_port_color = 0x4A8A0EFF; // Green
	static const uint32_t midi_port_color = 0x960909FF; //Red

	if (midi) {
		cairo_set_source_rgb (cr,
				UINT_RGBA_R_FLT(midi_port_color),
				UINT_RGBA_G_FLT(midi_port_color),
				UINT_RGBA_B_FLT(midi_port_color));
	} else {
		cairo_set_source_rgb (cr,
				UINT_RGBA_R_FLT(audio_port_color),
				UINT_RGBA_G_FLT(audio_port_color),
				UINT_RGBA_B_FLT(audio_port_color));
	}
}

void
PluginPinDialog::draw_io_pin (cairo_t* cr, const CtrlWidget& w)
{
	const double dir = (w.e->ct == Input) ? 1 : -1;

	cairo_move_to (cr, w.x + 5.0, w.y + ((w.e->ct == Input) ? 25 : 0));
	cairo_rel_line_to (cr, -5.,  -5. * dir);
	cairo_rel_line_to (cr,  0., -25. * dir);
	cairo_rel_line_to (cr, 10.,   0.);
	cairo_rel_line_to (cr,  0.,  25. * dir);
	cairo_close_path  (cr);

	cairo_set_line_width (cr, 1.0);
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_stroke_preserve (cr);

	set_color (cr, w.e->dt == DataType::MIDI);
	if (w.e == _selection || w.e == _actor) {
		cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
	} else if (w.prelight) {
		cairo_fill_preserve (cr);
		cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.5);
	}
	cairo_fill (cr);
}

void
PluginPinDialog::draw_plugin_pin (cairo_t* cr, const CtrlWidget& w)
{
	cairo_rectangle (cr, w.x, w.y, w.w, w.h);
	set_color (cr, w.e->dt == DataType::MIDI);
	if (w.e == _selection || w.e == _actor) {
		cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
	} else if (w.prelight) {
		cairo_fill_preserve (cr);
		cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.5);
	}
	cairo_fill (cr);
}

bool
PluginPinDialog::is_valid_port (uint32_t i, uint32_t n_total, uint32_t n_midi, bool midi)
{
	if (!midi) { i += n_midi; }
	if (i >= n_total) {
		return false;
	}
	return true;
}

double
PluginPinDialog::pin_x_pos (uint32_t i, double x0, double width, uint32_t n_total, uint32_t n_midi, bool midi)
{
	if (!midi) { i += n_midi; }
	return rint (x0 + (i + 1) * width / (1. + n_total)) - .5;
}

void
PluginPinDialog::draw_connection (cairo_t* cr, double x0, double x1, double y0, double y1, bool midi, bool dashed)
{
	const double bz = 2 * _pin_box_size;

	cairo_move_to (cr, x0, y0);
	cairo_curve_to (cr, x0, y0 + bz, x1, y1 - bz, x1, y1);
	cairo_set_line_width (cr, 3.0);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_source_rgb (cr, 1, 0, 0);
	if (dashed) {
		const double dashes[] = { 5, 7 };
		cairo_set_dash (cr, dashes, 2, 0);
	}
	set_color (cr, midi);
	cairo_stroke (cr);
	if (dashed) {
		cairo_set_dash (cr, 0, 0, 0);
	}
}

bool
PluginPinDialog::darea_expose_event (GdkEventExpose* ev)
{
	Gtk::Allocation a = darea.get_allocation();
	double const width = a.get_width();
	double const height = a.get_height();

	if (!_position_valid) {
		_width = width;
		_height = height;
		update_element_pos ();
		_position_valid = true;
	}

	cairo_t* cr = gdk_cairo_create (darea.get_window()->gobj());
	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	Gdk::Color const bg = get_style()->get_bg (STATE_NORMAL);
	cairo_set_source_rgb (cr, bg.get_red_p (), bg.get_green_p (), bg.get_blue_p ());
	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	/* layout sizes  -- TODO consolidate w/ update_element_pos() */
	// i/o pins
	const double y_in = 40;
	const double y_out = height - 40;

	// plugin box(es)
	const double yc   = rint (height * .5);
	const double bxh2 = 18;
	const double bxw  = rint ((width * .9) / ((_n_plugins) + .2 * (_n_plugins - 1)));
	const double bxw2 = rint (bxw * .5);

	const uint32_t pc_in = _in.n_total();
	const uint32_t pc_in_midi = _in.n_midi();
	const uint32_t pc_out = _out.n_total();
	const uint32_t pc_out_midi = _out.n_midi();

	/* draw midi-bypass (behind) */
	if (_pi->has_midi_bypass ()) {
		double x0 = rint (width / (1. + pc_in)) - .5;
		double x1 = rint (width / (1. + pc_out)) - .5;
		draw_connection (cr, x0, x1, y_in, y_out, true, true);
	}

	/* plugins & connection wires */
	for (uint32_t i = 0; i < _n_plugins; ++i) {
		double x0 = rint ((i + .5) * width / (double)(_n_plugins)) - .5;

		/* plugin box */
		cairo_set_source_rgb (cr, .3, .3, .3);
		rounded_rectangle (cr, x0 - bxw2, yc - bxh2, bxw, 2 * bxh2, 7);
		cairo_fill (cr);

		const ChanMapping::Mappings in_map = _pi->input_map (i).mappings();
		const ChanMapping::Mappings out_map = _pi->output_map (i).mappings();

		for (ChanMapping::Mappings::const_iterator t = in_map.begin (); t != in_map.end (); ++t) {
			bool is_midi = t->first == DataType::MIDI;
			for (ChanMapping::TypeMapping::const_iterator c = (*t).second.begin (); c != (*t).second.end () ; ++c) {
				uint32_t pn = (*c).first; // pin
				uint32_t pb = (*c).second;
				if (!is_valid_port (pb, pc_in, pc_in_midi, is_midi)) {
					continue;
				}
				double c_x0 = pin_x_pos (pb, 0, width, pc_in, pc_in_midi, is_midi);
				double c_x1 = pin_x_pos (pn, x0 - bxw2, bxw, _sinks.n_total (), _sinks.n_midi (), is_midi);
				draw_connection (cr, c_x0, c_x1, y_in, yc - bxh2 - _pin_box_size, is_midi);
			}
		}

		for (ChanMapping::Mappings::const_iterator t = out_map.begin (); t != out_map.end (); ++t) {
			bool is_midi = t->first == DataType::MIDI;
			for (ChanMapping::TypeMapping::const_iterator c = (*t).second.begin (); c != (*t).second.end () ; ++c) {
				uint32_t pn = (*c).first; // pin
				uint32_t pb = (*c).second;
				if (!is_valid_port (pb, pc_out, pc_out_midi, is_midi)) {
					continue;
				}
				double c_x0 = pin_x_pos (pn, x0 - bxw2, bxw, _sources.n_total (), _sources.n_midi (), is_midi);
				double c_x1 = pin_x_pos (pb, 0, width, pc_out, pc_out_midi, is_midi);
				draw_connection (cr, c_x0, c_x1, yc + bxh2 + _pin_box_size, y_out, is_midi);
			}
		}
	}

	/* pins and ports */
	for (CtrlElemList::const_iterator i = _elements.begin(); i != _elements.end(); ++i) {
		switch (i->e->ct) {
			case Input:
			case Output:
				draw_io_pin (cr, *i);
				break;
			case Sink:
			case Source:
				draw_plugin_pin (cr, *i);
				break;
		}
	}

	cairo_destroy (cr);
	return true;
}

void
PluginPinDialog::darea_size_allocate (Gtk::Allocation&)
{
	_position_valid = false;
}

bool
PluginPinDialog::darea_motion_notify_event (GdkEventMotion* ev)
{
	bool changed = false;
	_hover.reset ();
	for (CtrlElemList::iterator i = _elements.begin(); i != _elements.end(); ++i) {
		if (ev->x >= i->x && ev->x <= i->x + i->w
				&& ev->y >= i->y && ev->y <= i->y + i->h)
		{
			if (!i->prelight) changed = true;
			i->prelight = true;
			_hover = i->e;
		} else {
			if (i->prelight) changed = true;
			i->prelight = false;
		}
	}
	if (changed) {
		darea.queue_draw ();
	}
	return true;
}

bool
PluginPinDialog::darea_button_press_event (GdkEventButton* ev)
{
	if (ev->type != GDK_BUTTON_PRESS) {
		return false;
	}

	switch (ev->button) {
		case 1:
			if (!_selection || (_selection && !_hover)) {
				_selection = _hover;
				_actor.reset ();
				darea.queue_draw ();
			} else if (_selection && _hover && _selection != _hover) {
				if (_selection->dt != _hover->dt) { _actor.reset (); }
				else if (_selection->ct == Input && _hover->ct == Sink) { _actor = _hover; }
				else if (_selection->ct == Sink && _hover->ct == Input) { _actor = _hover; }
				else if (_selection->ct == Output && _hover->ct == Source) { _actor = _hover; }
				else if (_selection->ct == Source && _hover->ct == Output) { _actor = _hover; }
				if (!_actor) {
				_selection = _hover;
				}
				darea.queue_draw ();
			}
		case 3:
			if (_hover) {
			}
			break;
		default:
			break;
	}

	return true;
}

bool
PluginPinDialog::darea_button_release_event (GdkEventButton* ev)
{
	if (_hover == _actor && _actor) {
		assert (_selection);
		assert (_selection->dt == _actor->dt);
		if      (_selection->ct == Input && _actor->ct == Sink) {
			handle_input_action (_actor, _selection);
		}
		else if (_selection->ct == Sink && _actor->ct == Input) {
			handle_input_action (_selection, _actor);
		}
		else if (_selection->ct == Output && _actor->ct == Source) {
			handle_output_action (_actor, _selection);
		}
		else if (_selection->ct == Source && _actor->ct == Output) {
			handle_output_action (_selection, _actor);
		}
		_selection.reset ();
	}
	_actor.reset ();
	darea.queue_draw ();
	return true;
}

void
PluginPinDialog::handle_input_action (const CtrlElem &s, const CtrlElem &i)
{
	const int pc = s->ip;
	bool valid;
	ChanMapping in_map (_pi->input_map (pc));
	uint32_t idx = in_map.get (s->dt, s->id, &valid);

	if (valid && idx == i->id) {
		// disconnect
		in_map.unset (s->dt, s->id);
		_pi->set_input_map (pc, in_map);
	}
	else if (!valid) {
		// connect
		in_map.set (s->dt, s->id, i->id);
		_pi->set_input_map (pc, in_map);
	}
	else {
		// reconnect
		in_map.unset (s->dt, s->id);
		in_map.set (s->dt, s->id, i->id);
		_pi->set_input_map (pc, in_map);
	}
}

void
PluginPinDialog::handle_output_action (const CtrlElem &s, const CtrlElem &o)
{
	const int pc = s->ip;
	bool valid;
	ChanMapping out_map (_pi->output_map (pc));
	uint32_t idx = out_map.get (s->dt, s->id, &valid);

	if (valid && idx == o->id) {
		// disconnect
		out_map.unset (s->dt, s->id);
		_pi->set_output_map (pc, out_map);
	}
	else {
		// disconnect source
		if (valid) {
			out_map.unset (s->dt, s->id);
		}
		// disconnect other outputs
		_ignore_updates = true;
		for (uint32_t n = 0; n < _n_plugins; ++n) {
			if (n == pc) {
				continue;
			}
			ChanMapping n_out_map (_pi->output_map (n));
			idx = n_out_map.get_src (s->dt, o->id, &valid);
			if (valid) {
				n_out_map.unset (s->dt, idx);
				_pi->set_output_map (n, n_out_map);
			}
		}
		_ignore_updates = false;
		idx = out_map.get_src (s->dt, o->id, &valid);
		if (valid) {
			out_map.unset (s->dt, idx);
		}
		// connect
		out_map.set (s->dt, s->id, o->id);
		_pi->set_output_map (pc, out_map);
	}
}

void
PluginPinDialog::automatic_clicked ()
{
	_route()->reset_plugin_insert (_pi);
}

void
PluginPinDialog::add_remove_plugin_clicked (bool add)
{
	ChanCount out = _out;
	assert (add || _n_plugins > 0);
	_route()->customize_plugin_insert (_pi, _n_plugins + (add ? 1 : -1),  out);
}

void
PluginPinDialog::add_remove_port_clicked (bool add, ARDOUR::DataType dt)
{
	ChanCount out = _out;
	assert (add || out.get (dt) > 0);
	out.set (dt, out.get (dt) + (add ? 1 : -1));
	_route()->customize_plugin_insert (_pi, _n_plugins, out);
}
