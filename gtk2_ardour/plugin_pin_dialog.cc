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

#include "ardour/audioengine.h"
#include "ardour/plugin.h"
#include "ardour/port.h"
#include "ardour/session.h"

#include "plugin_pin_dialog.h"
#include "gui_thread.h"
#include "tooltips.h"
#include "ui_config.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

PluginPinDialog::PluginPinDialog (boost::shared_ptr<ARDOUR::PluginInsert> pi)
	: ArdourWindow (string_compose (_("Pin Configuration: %1"), pi->name ()))
	, _rst_config (_("Reset"))
	, _rst_mapping (_("Reset"))
	, _tgl_sidechain (_("Side Chain"))
	, _add_plugin (_("+"))
	, _del_plugin (_("-"))
	, _add_output_audio (_("+"))
	, _del_output_audio (_("-"))
	, _add_output_midi (_("+"))
	, _del_output_midi (_("-"))
	, _add_sc_audio (_("A+"))
	, _add_sc_midi (_("M+"))
	, _pi (pi)
	, _pin_box_size (10)
	, _width (0)
	, _height (0)
	, _innerwidth (0)
	, _margin_x (28)
	, _margin_y (40)
	, _min_width (300)
	, _min_height (200)
	, _n_inputs (0)
	, _n_sidechains (0)
	, _position_valid (false)
	, _ignore_updates (false)
	, _sidechain_selector (0)
{
	assert (pi->owner ()); // Route

	_pi->PluginIoReConfigure.connect (
			_plugin_connections, invalidator (*this), boost::bind (&PluginPinDialog::plugin_reconfigured, this), gui_context ()
			);

	_pi->PluginMapChanged.connect (
			_plugin_connections, invalidator (*this), boost::bind (&PluginPinDialog::plugin_reconfigured, this), gui_context ()
			);

	_pi->PluginConfigChanged.connect (
			_plugin_connections, invalidator (*this), boost::bind (&PluginPinDialog::plugin_reconfigured, this), gui_context ()
			);

	_pin_box_size = 2 * ceil (max (8., 10. * UIConfiguration::instance ().get_ui_scale ()) * .5);
	_margin_x = 2 * ceil (max (24., 28. * UIConfiguration::instance ().get_ui_scale ()) * .5);
	_margin_y = 2 * ceil (max (36., 40. * UIConfiguration::instance ().get_ui_scale ()) * .5);

	_tgl_sidechain.set_name ("pinrouting sidechain");

	_pm_size_group  = SizeGroup::create (SIZE_GROUP_BOTH);
	_add_plugin.set_tweaks (ArdourButton::Square);
	_del_plugin.set_tweaks (ArdourButton::Square);
	_pm_size_group->add_widget (_add_plugin);
	_pm_size_group->add_widget (_del_plugin);
	_pm_size_group->add_widget (_add_output_audio);
	_pm_size_group->add_widget (_del_output_audio);
	_pm_size_group->add_widget (_add_output_midi);
	_pm_size_group->add_widget (_del_output_midi);

	_sc_size_group  = SizeGroup::create (SIZE_GROUP_BOTH);
	_sc_size_group->add_widget (_add_sc_audio);
	_sc_size_group->add_widget (_add_sc_midi);

	Label* l;
	Gtk::Separator *sep;
	int r = 0;
	Table* tl = manage (new Table (9, 2));
	tl->set_border_width (0);
	tl->set_spacings (2);

	Table* tr = manage (new Table (4, 3));
	tr->set_border_width (0);
	tr->set_spacings (2);

	/* left side table */
	l = manage (new Label (_("<b>Config</b>"), ALIGN_CENTER));
	l->set_use_markup ();
	tl->attach (*l, 0, 2, r, r + 1, FILL, SHRINK);
	++r;
	tl->attach (_rst_config, 0, 2, r, r + 1, FILL, SHRINK);
	++r;

	sep = manage (new HSeparator ());
	tl->attach (*sep, 0, 2, r, r + 1, FILL|EXPAND, FILL|EXPAND, 0, 4);
	++r;

	l = manage (new Label (_("Instances"), ALIGN_CENTER));
	tl->attach (*l, 0, 2, r, r + 1, FILL, SHRINK);
	++r;
	tl->attach (_add_plugin, 0, 1, r, r + 1, SHRINK, SHRINK);
	tl->attach (_del_plugin, 1, 2, r, r + 1, SHRINK, SHRINK);
	++r;

	l = manage (new Label (_("Audio Out"), ALIGN_CENTER));
	tl->attach (*l, 0, 2, r, r + 1, FILL, SHRINK);
	++r;
	tl->attach (_add_output_audio, 0, 1, r, r + 1, SHRINK, SHRINK);
	tl->attach (_del_output_audio, 1, 2, r, r + 1, SHRINK, SHRINK);
	++r;

	l = manage (new Label (_("Midi Out"), ALIGN_CENTER));
	tl->attach (*l, 0, 2, r, r + 1, FILL, SHRINK);
	++r;
	tl->attach (_add_output_midi, 0, 1, r, r + 1, SHRINK, SHRINK);
	tl->attach (_del_output_midi, 1, 2, r, r + 1, SHRINK, SHRINK);
	++r;

	/* right side table */
	r = 0;
	l = manage (new Label (_("<b>Connections</b>"), ALIGN_CENTER));
	l->set_use_markup ();
	tr->attach (*l, 0, 2, r, r + 1, FILL, SHRINK);
	++r;
	tr->attach (_rst_mapping, 0, 2, r, r + 1, FILL, SHRINK);
	++r;

	sep = manage (new HSeparator ());
	tr->attach (*sep, 0, 2, r, r + 1, FILL|EXPAND, SHRINK, 0, 4);
	++r;

	tr->attach (_tgl_sidechain, 0, 2, r, r + 1, FILL, SHRINK);
	++r;

	_sidechain_tbl = manage (new Gtk::Table ());
	_sidechain_tbl->set_spacings (2);
	tr->attach (*_sidechain_tbl, 0, 2, r, r + 1, EXPAND|FILL, EXPAND|FILL, 0, 2);
	++r;

	tr->attach (_add_sc_audio, 0, 1, r, r + 1, FILL, SHRINK);
	tr->attach (_add_sc_midi, 1, 2, r, r + 1, FILL, SHRINK);
	++r;

	HBox* hbox = manage (new HBox);
	hbox->set_spacing (4);
	hbox->pack_start (*tl, false, false);
	hbox->pack_start (darea, true, true);
	hbox->pack_start (*tr, false, false);

	VBox* vbox = manage (new VBox);
	vbox->pack_start (*hbox, true, true);
	set_border_width (4);
	add (*vbox);
	vbox->show_all ();

	plugin_reconfigured ();

	darea.add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::POINTER_MOTION_MASK);
	darea.signal_size_request ().connect (sigc::mem_fun (*this, &PluginPinDialog::darea_size_request));
	darea.signal_size_allocate ().connect (sigc::mem_fun (*this, &PluginPinDialog::darea_size_allocate));
	darea.signal_expose_event ().connect (sigc::mem_fun (*this, &PluginPinDialog::darea_expose_event));
	darea.signal_button_press_event ().connect (sigc::mem_fun (*this, &PluginPinDialog::darea_button_press_event));
	darea.signal_button_release_event ().connect (sigc::mem_fun (*this, &PluginPinDialog::darea_button_release_event));
	darea.signal_motion_notify_event ().connect (sigc::mem_fun (*this, &PluginPinDialog::darea_motion_notify_event));

	_tgl_sidechain.signal_clicked.connect (sigc::mem_fun (*this, &PluginPinDialog::toggle_sidechain));

	_rst_mapping.signal_clicked.connect (sigc::mem_fun (*this, &PluginPinDialog::reset_mapping));
	_rst_config.signal_clicked.connect (sigc::mem_fun (*this, &PluginPinDialog::reset_configuration));
	_add_plugin.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::add_remove_plugin_clicked), true));
	_del_plugin.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::add_remove_plugin_clicked), false));

	_add_output_audio.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::add_remove_port_clicked), true, DataType::AUDIO));
	_del_output_audio.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::add_remove_port_clicked), false, DataType::AUDIO));
	_add_output_midi.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::add_remove_port_clicked), true, DataType::MIDI));
	_del_output_midi.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::add_remove_port_clicked), false, DataType::MIDI));
	_add_sc_audio.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::add_sidechain_port), DataType::AUDIO));
	_add_sc_midi.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::add_sidechain_port), DataType::MIDI));
}

PluginPinDialog::~PluginPinDialog ()
{
	delete _sidechain_selector;
}

void
PluginPinDialog::plugin_reconfigured ()
{
	ENSURE_GUI_THREAD (*this, &PluginPinDialog::plugin_reconfigured);
	if (_ignore_updates) {
		return;
	}
	_n_plugins = _pi->get_count ();
	_pi->configured_io (_in, _out);
	_ins = _pi->internal_streams (); // with sidechain
	_sinks = _pi->natural_input_streams ();
	_sources = _pi->natural_output_streams ();

	_del_plugin.set_sensitive (_n_plugins > 1);
	_del_output_audio.set_sensitive (_out.n_audio () > 0 && _out.n_total () > 1);
	_del_output_midi.set_sensitive (_out.n_midi () > 0 && _out.n_total () > 1);
	_tgl_sidechain.set_active (_pi->has_sidechain ());
	_add_sc_audio.set_sensitive (_pi->has_sidechain ());
	_add_sc_midi.set_sensitive (_pi->has_sidechain ());

	if (_pi->custom_cfg ()) {
		_rst_config.set_name ("pinrouting custom");
	} else {
		_rst_config.set_name ("generic button");
	}

	if (!_pi->has_sidechain () && _sidechain_selector) {
		delete _sidechain_selector;
		_sidechain_selector = 0;
	}

	_io_connection.disconnect ();
	refill_sidechain_table ();

	/* update elements */

	_elements.clear ();
	_hover.reset ();
	_actor.reset ();
	_selection.reset ();

	_n_inputs = _n_sidechains = 0;

	for (uint32_t i = 0; i < _ins.n_total (); ++i) {
		DataType dt = i < _ins.n_midi () ? DataType::MIDI : DataType::AUDIO;
		uint32_t id = dt == DataType::MIDI ? i : i - _ins.n_midi ();
		bool sidechain = id >= _in.get (dt) ? true : false;
		if (sidechain) {
			++_n_sidechains;
		} else {
			++_n_inputs;
		}

		CtrlWidget cw (CtrlWidget (Input, dt, id, 0, sidechain));
		_elements.push_back (cw);
	}

	for (uint32_t i = 0; i < _out.n_total (); ++i) {
		int id = (i < _out.n_midi ()) ? i : i - _out.n_midi ();
		_elements.push_back (CtrlWidget (Output, (i < _out.n_midi () ? DataType::MIDI : DataType::AUDIO), id));
	}

	for (uint32_t n = 0; n < _n_plugins; ++n) {
		boost::shared_ptr<Plugin> plugin = _pi->plugin (n);
		for (uint32_t i = 0; i < _sinks.n_total (); ++i) {
			DataType dt (_sinks.n_midi () ? DataType::MIDI : DataType::AUDIO);
			int idx = (i < _sinks.n_midi ()) ? i : i - _sinks.n_midi ();
			const Plugin::IOPortDescription& iod (plugin->describe_io_port (dt, true, idx));
			CtrlWidget cw (CtrlWidget (Sink, dt, i, n, iod.is_sidechain));
			_elements.push_back (cw);
		}
		for (uint32_t i = 0; i < _sources.n_total (); ++i) {
			_elements.push_back (CtrlWidget (Source, (i < _sources.n_midi () ? DataType::MIDI : DataType::AUDIO), i, n));
		}
	}

	/* calc minimum size */
	const uint32_t max_ports = std::max (_ins.n_total (), _out.n_total ());
	const uint32_t max_pins = std::max ((_sinks * _n_plugins).n_total (), (_sources * _n_plugins).n_total ());
	uint32_t min_width = std::max (25 * max_ports, (uint32_t)(20 + _pin_box_size) * max_pins);
	min_width = std::max (min_width, (uint32_t)ceilf (_margin_y * .45 * _n_plugins * 16. / 9.)); // 16 : 9 aspect
	min_width = std::max ((uint32_t)300, min_width);

	min_width = 50 + 10 * ceilf (min_width / 10.f);

	uint32_t min_height = 3.5 * _margin_y + 2 * (_n_sidechains + 1) * _pin_box_size;
	min_height = std::max ((uint32_t)200, min_height);
	min_height = 4 * ceilf (min_height / 4.f);

	if (min_width != _min_width || min_height != _min_height) {
		_min_width = min_width;
		_min_height = min_height;
		darea.queue_resize ();
	}

	_position_valid = false;
	darea.queue_draw ();
}

void
PluginPinDialog::refill_sidechain_table ()
{
	Table_Helpers::TableList& kids = _sidechain_tbl->children ();
	for (Table_Helpers::TableList::iterator i = kids.begin (); i != kids.end ();) {
		i = kids.erase (i);
	}
	_sidechain_tbl->resize (1, 1);
	if (!_pi->has_sidechain () && _sidechain_selector) {
		return;
	}
	boost::shared_ptr<IO> io = _pi->sidechain_input ();
	if (!io) {
		return;
	}

	io->changed.connect (
			_io_connection, invalidator (*this), boost::bind (&PluginPinDialog::plugin_reconfigured, this), gui_context ()
			);

	uint32_t r = 0;
	PortSet& p (io->ports ());
	bool can_remove = p.num_ports () > 1;
	for (PortSet::iterator i = p.begin (DataType::MIDI); i != p.end (DataType::MIDI); ++i, ++r) {
		add_port_to_table (*i, r, can_remove);
	}
	for (PortSet::iterator i = p.begin (DataType::AUDIO); i != p.end (DataType::AUDIO); ++i, ++r) {
		add_port_to_table (*i, r, can_remove);
	}
	_sidechain_tbl->show_all ();
}

void
PluginPinDialog::add_port_to_table (boost::shared_ptr<Port> p, uint32_t r, bool can_remove)
{
	std::string lbl;
	std::string tip = p->name ();
	std::vector<std::string> cns;
	p->get_connections (cns);

	// TODO proper labels, see MixerStrip::update_io_button()
	if (cns.size () == 0) {
		lbl = "-";
	} else if (cns.size () > 1) {
		lbl = "...";
		tip += " &gt;- ";
	} else {
		lbl = cns[0];
		tip += " &gt;- ";
		if (lbl.find ("system:") == 0) {
			lbl = AudioEngine::instance ()->get_pretty_name_by_name (lbl);
			if (lbl.empty ()) {
				lbl = cns[0].substr (7);
			}
		}
	}
	for (std::vector<std::string>::const_iterator i = cns.begin(); i != cns.end(); ++i) {
		tip += *i;
		tip += " ";
	}

	ArdourButton *pb = manage (new ArdourButton (lbl));
	pb->set_text_ellipsize (Pango::ELLIPSIZE_MIDDLE);
	pb->set_layout_ellipsize_width (56 * PANGO_SCALE);
	ARDOUR_UI_UTILS::set_tooltip (*pb, tip);
	_sidechain_tbl->attach (*pb, 0, 1, r, r +1 , EXPAND|FILL, SHRINK);

	pb->signal_button_press_event ().connect (sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::sc_input_press), boost::weak_ptr<Port> (p)), false);
	pb->signal_button_release_event ().connect (sigc::mem_fun (*this, &PluginPinDialog::sc_input_release), false);

	pb = manage (new ArdourButton ("-"));
	_sidechain_tbl->attach (*pb, 1, 2, r, r +1 , FILL, SHRINK);
	if (can_remove) {
		pb->signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::remove_port), boost::weak_ptr<Port> (p)));
	} else {
		pb->set_sensitive (false);
	}
}

void
PluginPinDialog::update_element_pos ()
{
	/* layout sizes */
	_innerwidth = _width - 2. * _margin_x;

	const double yc   = rint (_height * .5);
	const double bxh2 = rint (_margin_y * .45); // TODO grow?
	const double bxw  = rint ((_innerwidth * .95) / ((_n_plugins) + .2 * (_n_plugins - 1)));
	const double bxw2 = rint (bxw * .5);
	const double y_in = _margin_y;
	const double y_out = _height - _margin_y;

	_bxw2 = bxw2;
	_bxh2 = bxh2;

	const double dx = _pin_box_size * .5;

	uint32_t sc_cnt = 0;
	for (CtrlElemList::iterator i = _elements.begin (); i != _elements.end (); ++i) {
		switch (i->e->ct) {
			case Input:
				if (i->e->sc) {
					i->x = _innerwidth + _margin_x - dx;
					i->y = y_in + (sc_cnt + .5) * _pin_box_size;
					i->h = _pin_box_size;
					i->w = 1.5 * _pin_box_size;
					++ sc_cnt;
				} else {
					uint32_t idx = i->e->id;
					if (i->e->dt == DataType::AUDIO) { idx += _in.n_midi (); }
					i->x = rint ((idx + 1) * _width / (1. + _n_inputs)) - 0.5 - dx;
					i->w = _pin_box_size;
					i->h = 1.5 * _pin_box_size;
					i->y = y_in - i->h;
				}
				break;
			case Output:
				{
					uint32_t idx = i->e->id;
					if (i->e->dt == DataType::AUDIO) { idx += _out.n_midi (); }
					i->x = rint ((idx + 1) * _width / (1. + _out.n_total ())) - 0.5 - dx;
					i->y = y_out;
					i->w = _pin_box_size;
					i->h = 1.5 * _pin_box_size;
				}
				break;
			case Sink:
				{
					const double x0 = rint ((i->e->ip + .5) * _innerwidth / (double)(_n_plugins)) - .5 - bxw2;
					i->x = _margin_x + rint (x0 + (i->e->id + 1) * bxw / (1. + _sinks.n_total ())) - .5 - dx;
					i->y = yc - bxh2 - dx;
					i->w = _pin_box_size;
					i->h = _pin_box_size;
				}
				break;
			case Source:
				{
					const double x0 = rint ((i->e->ip + .5) * _innerwidth / (double)(_n_plugins)) - .5 - bxw2;
					i->x = _margin_x + rint (x0 + (i->e->id + 1) * bxw / (1. + _sources.n_total ())) - .5 - dx;
					i->y = yc + bxh2 - dx;
					i->w = _pin_box_size;
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
				UINT_RGBA_R_FLT (midi_port_color),
				UINT_RGBA_G_FLT (midi_port_color),
				UINT_RGBA_B_FLT (midi_port_color));
	} else {
		cairo_set_source_rgb (cr,
				UINT_RGBA_R_FLT (audio_port_color),
				UINT_RGBA_G_FLT (audio_port_color),
				UINT_RGBA_B_FLT (audio_port_color));
	}
}

void
PluginPinDialog::draw_io_pin (cairo_t* cr, const CtrlWidget& w)
{

	if (w.e->sc) {
		const double dy = w.h * .5;
		const double dx = w.w - dy;
		cairo_move_to (cr, w.x, w.y + dy);
		cairo_rel_line_to (cr,  dy, -dy);
		cairo_rel_line_to (cr,  dx,  0);
		cairo_rel_line_to (cr,   0,  w.h);
		cairo_rel_line_to (cr, -dx,  0);
	} else {
		const double dir = (w.e->ct == Input) ? 1 : -1;
		const double dx = w.w * .5;
		const double dy = w.h - dx;

		cairo_move_to (cr, w.x + dx, w.y + ((w.e->ct == Input) ? w.h : 0));
		cairo_rel_line_to (cr,     -dx, -dx * dir);
		cairo_rel_line_to (cr,      0., -dy * dir);
		cairo_rel_line_to (cr, 2. * dx,        0.);
		cairo_rel_line_to (cr,      0.,  dy * dir);
	}
	cairo_close_path  (cr);

	cairo_set_line_width (cr, 1.0);
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_stroke_preserve (cr);

	set_color (cr, w.e->dt == DataType::MIDI);

	if (w.e->sc) {
		assert (w.e->ct == Input);
		cairo_fill_preserve (cr);
		cairo_set_source_rgba (cr, 0.0, 0.0, 1.0, 0.4);
	}

	if (w.e == _selection || w.e == _actor) {
		cairo_fill_preserve (cr);
		cairo_set_source_rgba (cr, 0.9, 0.9, 1.0, 0.6);
	} else if (w.prelight) {
		cairo_fill_preserve (cr);
		cairo_set_source_rgba (cr, 0.9, 0.9, 0.9, 0.3);
	}
	cairo_fill (cr);
}

void
PluginPinDialog::draw_plugin_pin (cairo_t* cr, const CtrlWidget& w)
{
	const double dx = w.w * .5;
	const double dy = w.h * .5;

	cairo_move_to (cr, w.x + dx, w.y);
	cairo_rel_line_to (cr, -dx,  dy);
	cairo_rel_line_to (cr,  dx,  dy);
	cairo_rel_line_to (cr,  dx, -dy);
	cairo_close_path  (cr);

	cairo_set_line_width (cr, 1.0);
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_stroke_preserve (cr);

	set_color (cr, w.e->dt == DataType::MIDI);

	if (w.e->sc) {
		assert (w.e->ct == Sink);
		cairo_fill_preserve (cr);
		cairo_set_source_rgba (cr, 0.0, 0.0, 1.0, 0.4);
	}

	if (w.e == _selection || w.e == _actor) {
		cairo_fill_preserve (cr);
		cairo_set_source_rgba (cr, 0.9, 0.9, 1.0, 0.6);
	} else if (w.prelight) {
		cairo_fill_preserve (cr);
		cairo_set_source_rgba (cr, 0.9, 0.9, 0.9, 0.3);
	}
	cairo_fill (cr);
}

double
PluginPinDialog::pin_x_pos (uint32_t i, double x0, double width, uint32_t n_total, uint32_t n_midi, bool midi)
{
	if (!midi) { i += n_midi; }
	return rint (x0 + (i + 1) * width / (1. + n_total)) - .5;
}

const PluginPinDialog::CtrlWidget&
PluginPinDialog::get_io_ctrl (CtrlType ct, DataType dt, uint32_t id, uint32_t ip) const
{
	for (CtrlElemList::const_iterator i = _elements.begin (); i != _elements.end (); ++i) {
		if (i->e->ct == ct && i->e->dt == dt && i->e->id == id && i->e->ip == ip) {
			return *i;
		}
	}
	fatal << string_compose (_("programming error: %1"),
			X_("Invalid Plugin I/O Port."))
		<< endmsg;
	abort (); /*NOTREACHED*/
	static CtrlWidget screw_old_compilers (Input, DataType::NIL, 0);
	return screw_old_compilers;
}

void
PluginPinDialog::edge_coordinates (const CtrlWidget& w, double &x, double &y)
{
	switch (w.e->ct) {
		case Input:
			if (w.e->sc) {
				x = w.x;
				y = w.y + w.h * .5;
			} else {
				x = w.x + w.w * .5;
				y = w.y + w.h;
			}
			break;
		case Output:
			x = w.x + w.w * .5;
			y = w.y;
			break;
		case Sink:
			x = w.x + w.w * .5;
			y = w.y;
			break;
		case Source:
			x = w.x + w.w * .5;
			y = w.y + w.h;
			break;
	}
}

void
PluginPinDialog::draw_connection (cairo_t* cr, double x0, double x1, double y0, double y1, bool midi, bool horiz, bool dashed)
{
	const double bz = 2 * _pin_box_size;
	const double bc = (dashed && x0 == x1) ? 1.25 * _pin_box_size : 0;

	cairo_move_to (cr, x0, y0);
	if (horiz) {
		cairo_curve_to (cr, x0 - bz, y0 + bc, x1 - bc, y1 - bz, x1, y1);
	} else {
		cairo_curve_to (cr, x0 - bc, y0 + bz, x1 - bc, y1 - bz, x1, y1);
	}
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

void
PluginPinDialog::draw_connection (cairo_t* cr, const CtrlWidget& w0, const CtrlWidget& w1, bool dashed)
{
	double x0, x1, y0, y1;
	edge_coordinates (w0, x0, y0);
	edge_coordinates (w1, x1, y1);
	assert (w0.e->dt == w1.e->dt);
	draw_connection (cr, x0, x1, y0, y1, w0.e->dt == DataType::MIDI, w0.e->sc, dashed);
}


bool
PluginPinDialog::darea_expose_event (GdkEventExpose* ev)
{
	Gtk::Allocation a = darea.get_allocation ();
	double const width = a.get_width ();
	double const height = a.get_height ();

	if (!_position_valid) {
		_width = width;
		_height = height;
		update_element_pos ();
		_position_valid = true;
	}

	cairo_t* cr = gdk_cairo_create (darea.get_window ()->gobj ());
	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	Gdk::Color const bg = get_style ()->get_bg (STATE_NORMAL);
	cairo_set_source_rgb (cr, bg.get_red_p (), bg.get_green_p (), bg.get_blue_p ());
	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	const double yc = rint (_height * .5);

	/* processor box */
	rounded_rectangle (cr, _margin_x, _margin_y - _pin_box_size * .5, _innerwidth, _height - 2 * _margin_y + _pin_box_size, 7);
	cairo_set_line_width (cr, 1.0);
	cairo_set_source_rgb (cr, .1, .1, .3);
	cairo_stroke_preserve (cr);
	cairo_set_source_rgb (cr, .3, .3, .3);
	cairo_fill (cr);

	/* draw midi-bypass (behind) */
	if (_pi->has_midi_bypass ()) {
		const CtrlWidget& cw0 = get_io_ctrl (Input, DataType::MIDI, 0);
		const CtrlWidget& cw1 = get_io_ctrl (Output, DataType::MIDI, 0);
		draw_connection (cr, cw0, cw1, true);
	}

	/* labels */
	Glib::RefPtr<Pango::Layout> layout;
	layout = Pango::Layout::create (get_pango_context ());

	layout->set_ellipsize (Pango::ELLIPSIZE_MIDDLE);
	layout->set_width (_height * PANGO_SCALE);

	int text_width;
	int text_height;

	layout->set_text (_route ()->name ());
	layout->get_pixel_size (text_width, text_height);
	cairo_save (cr);
	cairo_move_to (cr, .5 * (_margin_x - text_height), .5 * (_height + text_width));
	cairo_rotate (cr, M_PI * -.5);
	cairo_set_source_rgba (cr, 1., 1., 1., 1.);
	pango_cairo_show_layout (cr, layout->gobj ());
	cairo_new_path (cr);
	cairo_restore (cr);

	layout->set_width ((_innerwidth - 2 * _pin_box_size) * PANGO_SCALE);
	layout->set_text (_pi->name ());
	layout->get_pixel_size (text_width, text_height);
	cairo_move_to (cr, _margin_x + _innerwidth - text_width - _pin_box_size * .5, _height - _margin_y - text_height);
	cairo_set_source_rgba (cr, 1., 1., 1., 1.);
	pango_cairo_show_layout (cr, layout->gobj ());

	if (_pi->strict_io ()) {
		layout->set_text (_("Strict I/O"));
		layout->get_pixel_size (text_width, text_height);
		const double sx0 = _margin_x + .5 * (_innerwidth - text_width);
		const double sy0 = _height - 3 - text_height;

		rounded_rectangle (cr, sx0 - 2, sy0 - 1, text_width + 4, text_height + 2, 7);
		cairo_set_source_rgba (cr, .4, .3, .1, 1.);
		cairo_fill (cr);

		cairo_set_source_rgba (cr, 1., 1., 1., 1.);
		cairo_move_to (cr, sx0, sy0);
		cairo_set_source_rgba (cr, 1., 1., 1., 1.);
		pango_cairo_show_layout (cr, layout->gobj ());
	}


	/* plugins & connection wires */
	for (uint32_t i = 0; i < _n_plugins; ++i) {
		double x0 = _margin_x + rint ((i + .5) * _innerwidth / (double)(_n_plugins)) - .5;

		/* plugin box */
		cairo_set_source_rgb (cr, .5, .5, .5);
		rounded_rectangle (cr, x0 - _bxw2, yc - _bxh2, 2 * _bxw2, 2 * _bxh2, 7);
		cairo_fill (cr);

		const ChanMapping::Mappings in_map = _pi->input_map (i).mappings ();
		const ChanMapping::Mappings out_map = _pi->output_map (i).mappings ();

		for (ChanMapping::Mappings::const_iterator t = in_map.begin (); t != in_map.end (); ++t) {
			for (ChanMapping::TypeMapping::const_iterator c = (*t).second.begin (); c != (*t).second.end () ; ++c) {
				const CtrlWidget& cw0 = get_io_ctrl (Input, t->first, c->second);
				const CtrlWidget& cw1 = get_io_ctrl (Sink, t->first, c->first, i);
				draw_connection (cr, cw0, cw1);
			}
		}

		for (ChanMapping::Mappings::const_iterator t = out_map.begin (); t != out_map.end (); ++t) {
			for (ChanMapping::TypeMapping::const_iterator c = (*t).second.begin (); c != (*t).second.end () ; ++c) {
				const CtrlWidget& cw0 = get_io_ctrl (Source, t->first, c->first, i);
				const CtrlWidget& cw1 = get_io_ctrl (Output, t->first, c->second);
				draw_connection (cr, cw0, cw1);
			}
		}
	}

	/* pins and ports */
	for (CtrlElemList::const_iterator i = _elements.begin (); i != _elements.end (); ++i) {
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
PluginPinDialog::darea_size_request (Gtk::Requisition* req)
{
	req->width = _min_width;
	req->height = _min_height;
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
	for (CtrlElemList::iterator i = _elements.begin (); i != _elements.end (); ++i) {
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
			break;
		case 3:
			if (_selection != _hover) {
				_selection = _hover;
				darea.queue_draw ();
			}
			_actor.reset ();
			break;
		default:
			break;
	}

	return true;
}

bool
PluginPinDialog::darea_button_release_event (GdkEventButton* ev)
{
	if (_hover == _actor && _actor && ev->button == 1) {
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
	} else if (_hover == _selection && _selection && ev->button == 3) {
		handle_disconnect (_selection);
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
	const uint32_t pc = s->ip;
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
PluginPinDialog::handle_disconnect (const CtrlElem &e)
{
	_ignore_updates = true;
	bool changed = false;
	bool valid;
	//ChanMapping out_map (_pi->output_map (pc));

	switch (e->ct) {
		case Input:
			for (uint32_t n = 0; n < _n_plugins; ++n) {
				ChanMapping map (_pi->input_map (n));
				for (uint32_t i = 0; i < _sinks.n_total (); ++i) {
					uint32_t idx = map.get (e->dt, i, &valid);
					if (valid && idx == e->id) {
						map.unset (e->dt, i);
						changed = true;
					}
				}
				_pi->set_input_map (n, map);
			}
			break;
		case Sink:
			{
				ChanMapping map (_pi->input_map (e->ip));
				map.get (e->dt, e->id, &valid);
				if (valid) {
					map.unset (e->dt, e->id);
					_pi->set_input_map (e->ip, map);
					changed = true;
				}
			}
			break;
		case Source:
			{
				ChanMapping map (_pi->output_map (e->ip));
				map.get (e->dt, e->id, &valid);
				if (valid) {
					map.unset (e->dt, e->id);
					_pi->set_output_map (e->ip, map);
					changed = true;
				}
			}
			break;
		case Output:
			for (uint32_t n = 0; n < _n_plugins; ++n) {
				ChanMapping map (_pi->output_map (n));
				for (uint32_t i = 0; i < _sources.n_total (); ++i) {
					uint32_t idx = map.get (e->dt, i, &valid);
					if (valid && idx == e->id) {
						map.unset (e->dt, i);
						changed = true;
					}
				}
				_pi->set_output_map (n, map);
			}
			break;
	}
	_ignore_updates = false;
	if (changed) {
		plugin_reconfigured ();
	}
}

void
PluginPinDialog::toggle_sidechain ()
{
	if (_session && _session->actively_recording()) { return; }
	_route ()->add_remove_sidechain (_pi, !_pi->has_sidechain ());
}

void
PluginPinDialog::connect_sidechain ()
{
	if (!_session) { return; }

	if (_sidechain_selector == 0) {
		_sidechain_selector = new IOSelectorWindow (_session, _pi->sidechain_input ());
	}

	if (_sidechain_selector->is_visible ()) {
		_sidechain_selector->get_toplevel ()->get_window ()->raise ();
	} else {
		_sidechain_selector->present ();
	}
}

void
PluginPinDialog::reset_configuration ()
{
	_route ()->reset_plugin_insert (_pi);
}

void
PluginPinDialog::reset_mapping ()
{
	_pi->reset_map ();
}

void
PluginPinDialog::add_remove_plugin_clicked (bool add)
{
	if (_session && _session->actively_recording()) { return; }
	ChanCount out = _out;
	assert (add || _n_plugins > 0);
	_route ()->customize_plugin_insert (_pi, _n_plugins + (add ? 1 : -1),  out);
}

void
PluginPinDialog::add_remove_port_clicked (bool add, ARDOUR::DataType dt)
{
	if (_session && _session->actively_recording()) { return; }
	ChanCount out = _out;
	assert (add || out.get (dt) > 0);
	out.set (dt, out.get (dt) + (add ? 1 : -1));
	_route ()->customize_plugin_insert (_pi, _n_plugins, out);
}

void
PluginPinDialog::add_sidechain_port (DataType dt)
{
	if (_session && _session->actively_recording()) { return; }
	boost::shared_ptr<IO> io = _pi->sidechain_input ();
	if (!io) {
		return;
	}
	io->add_port ("", this, dt);
}

void
PluginPinDialog::remove_port (boost::weak_ptr<ARDOUR::Port> wp)
{
	if (_session && _session->actively_recording()) { return; }
	boost::shared_ptr<ARDOUR::Port> p = wp.lock ();
	boost::shared_ptr<IO> io = _pi->sidechain_input ();
	if (!io || !p) {
		return;
	}
	io->remove_port (p, this);
}

void
PluginPinDialog::disconnect_port (boost::weak_ptr<ARDOUR::Port> wp)
{
	if (_session && _session->actively_recording()) { return; }
	boost::shared_ptr<ARDOUR::Port> p = wp.lock ();
	boost::shared_ptr<IO> io = _pi->sidechain_input ();
	if (!io || !p) {
		return;
	}
	io->disconnect (this);
}


bool
PluginPinDialog::sc_input_press (GdkEventButton *ev, boost::weak_ptr<ARDOUR::Port> wp)
{
	using namespace Menu_Helpers;
	if (!_session || _session->actively_recording()) { return false; }
	if (!_session->engine().connected()) { return false; }

	if (ev->button == 1) {
		MenuList& citems = input_menu.items();
		input_menu.set_name ("ArdourContextMenu");
		citems.clear();
		// TODO build menu -- list of ports that don't produce feedback.
		boost::shared_ptr<Port> p = wp.lock ();
		if (p && p->connected ()) {
			citems.push_back (MenuElem (_("Disconnect"), sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::disconnect_port), wp)));
			citems.push_back (SeparatorElem());
		}
		citems.push_back (MenuElem (_("Routing Grid"), sigc::mem_fun (*this, &PluginPinDialog::connect_sidechain)));
		input_menu.popup (1, ev->time);
	}
	return false;
}

bool
PluginPinDialog::sc_input_release (GdkEventButton *ev)
{
	if (_session && _session->actively_recording()) { return false; }
	if (ev->button == 3) {
		connect_sidechain ();
	}
	return false;
}
