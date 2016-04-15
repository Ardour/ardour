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

#include <boost/algorithm/string.hpp>

#include <gtkmm/table.h>
#include <gtkmm/frame.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>

#include "pbd/replace_all.h"

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"

#include "ardour/audioengine.h"
#include "ardour/plugin.h"
#include "ardour/port.h"
#include "ardour/profile.h"
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
	, _set_config (_("Manual Config"), ArdourButton::led_default_elements)
	, _tgl_sidechain (_("Side Chain"), ArdourButton::led_default_elements)
	, _add_plugin (_("+"))
	, _del_plugin (_("-"))
	, _add_output_audio (_("+"))
	, _del_output_audio (_("-"))
	, _add_output_midi (_("+"))
	, _del_output_midi (_("-"))
	, _add_sc_audio (_("Audio"))
	, _add_sc_midi (_("MIDI"))
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
	, _dragging (false)
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
	_set_config.set_name ("pinrouting custom");

	Menu_Helpers::MenuList& citems = reset_menu.items();
	reset_menu.set_name ("ArdourContextMenu");
	citems.clear();
	citems.push_back (Menu_Helpers::MenuElem (_("Reset"), sigc::mem_fun (*this, &PluginPinDialog::reset_mapping)));

	_pm_size_group  = SizeGroup::create (SIZE_GROUP_BOTH);
	_add_plugin.set_tweaks (ArdourButton::Square);
	_del_plugin.set_tweaks (ArdourButton::Square);
	_pm_size_group->add_widget (_add_plugin);
	_pm_size_group->add_widget (_del_plugin);
	_pm_size_group->add_widget (_add_output_audio);
	_pm_size_group->add_widget (_del_output_audio);
	_pm_size_group->add_widget (_add_output_midi);
	_pm_size_group->add_widget (_del_output_midi);

	Box* box;
	Frame *f;

	VBox* tl = manage (new VBox ());
	tl->set_border_width (2);
	tl->set_spacing (2);

	VBox* tr = manage (new VBox ());
	tr->set_border_width (2);
	tr->set_spacing (2);

	/* left side */
	tl->pack_start (_set_config, false, false);

	box = manage (new HBox ());
	box->set_border_width (2);
	box->pack_start (_add_plugin, true, false);
	box->pack_start (_del_plugin, true, false);
	f = manage (new Frame());
	f->set_label (_("Instances"));
	f->add (*box);
	tl->pack_start (*f, false, false);

	box = manage (new HBox ());
	box->set_border_width (2);
	box->pack_start (_add_output_audio, true, false);
	box->pack_start (_del_output_audio, true, false);
	f = manage (new Frame());
	f->set_label (_("Audio Out"));
	f->add (*box);
	tl->pack_start (*f, false, false);

	box = manage (new HBox ());
	box->set_border_width (2);
	box->pack_start (_add_output_midi, true, false);
	box->pack_start (_del_output_midi, true, false);
	f = manage (new Frame());
	f->set_label (_("MIDI Out"));
	f->add (*box);
	tl->pack_start (*f, false, false);

	tl->pack_start (*manage (new Label ("")), true, true); // invisible separator
	tl->pack_start (*manage (new HSeparator ()), false, false, 4);
	_out_presets.disable_scrolling ();
	ARDOUR_UI_UTILS::set_tooltip (_out_presets, _("Output Presets"));
	tl->pack_start (_out_presets, false, false);

	/* right side */
	_sidechain_tbl = manage (new Gtk::Table ());
	_sidechain_tbl->set_spacings (2);

	tr->pack_start (_tgl_sidechain, false, false);
	tr->pack_start (*_sidechain_tbl, true, true);

	box = manage (new VBox ());
	box->set_border_width (2);
	box->set_spacing (2);
	box->pack_start (_add_sc_audio, false, false);
	box->pack_start (_add_sc_midi , false, false);
	f = manage (new Frame());
	f->set_label (_("Add Sidechain Input"));
	f->add (*box);

	tr->pack_start (*f, false, false);

	/* global packing */
	HBox* hbox = manage (new HBox ());
	hbox->set_spacing (4);
	hbox->pack_start (*tl, false, false);
	hbox->pack_start (darea, true, true);
	hbox->pack_start (*tr, false, false);

	VBox* vbox = manage (new VBox ());
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

	_set_config.signal_clicked.connect (sigc::mem_fun (*this, &PluginPinDialog::reset_configuration));
	_add_plugin.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::add_remove_plugin_clicked), true));
	_del_plugin.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::add_remove_plugin_clicked), false));

	_add_output_audio.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::add_remove_port_clicked), true, DataType::AUDIO));
	_del_output_audio.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::add_remove_port_clicked), false, DataType::AUDIO));
	_add_output_midi.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::add_remove_port_clicked), true, DataType::MIDI));
	_del_output_midi.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::add_remove_port_clicked), false, DataType::MIDI));
	_add_sc_audio.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::add_sidechain_port), DataType::AUDIO));
	_add_sc_midi.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::add_sidechain_port), DataType::MIDI));

	AudioEngine::instance()->PortConnectedOrDisconnected.connect (
			_io_connection, invalidator (*this), boost::bind (&PluginPinDialog::port_connected_or_disconnected, this, _1, _3), gui_context ()
			);
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


	_tgl_sidechain.set_active (_pi->has_sidechain ());
	_add_sc_audio.set_sensitive (_pi->has_sidechain ());
	_add_sc_midi.set_sensitive (_pi->has_sidechain ());

	if (_pi->custom_cfg ()) {
		_set_config.set_active (true);
		_add_plugin.set_sensitive (true);
		_add_output_audio.set_sensitive (true);
		_add_output_midi.set_sensitive (true);
		_del_plugin.set_sensitive (_n_plugins > 1);
		_del_output_audio.set_sensitive (_out.n_audio () > 0 && _out.n_total () > 1);
		_del_output_midi.set_sensitive (_out.n_midi () > 0 && _out.n_total () > 1);
		_out_presets.set_sensitive (false);
		_out_presets.set_text (_("Manual"));
	} else {
		_set_config.set_active (false);
		_add_plugin.set_sensitive (false);
		_add_output_audio.set_sensitive (false);
		_add_output_midi.set_sensitive (false);
		_del_plugin.set_sensitive (false);
		_del_output_audio.set_sensitive (false);
		_del_output_midi.set_sensitive (false);
		_out_presets.set_sensitive (true);
		refill_output_presets ();
	}

	if (!_pi->has_sidechain () && _sidechain_selector) {
		delete _sidechain_selector;
		_sidechain_selector = 0;
	}

	refill_sidechain_table ();

	/* update elements */

	_elements.clear ();
	_hover.reset ();
	_actor.reset ();
	_selection.reset ();
	_drag_dst.reset();
	_dragging = false;

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

		CtrlWidget cw (CtrlWidget ("", Input, dt, id, 0, sidechain));
		_elements.push_back (cw);
	}

	for (uint32_t i = 0; i < _out.n_total (); ++i) {
		int id = (i < _out.n_midi ()) ? i : i - _out.n_midi ();
		_elements.push_back (CtrlWidget ("", Output, (i < _out.n_midi () ? DataType::MIDI : DataType::AUDIO), id));
	}

	for (uint32_t n = 0; n < _n_plugins; ++n) {
		boost::shared_ptr<Plugin> plugin = _pi->plugin (n);
		for (uint32_t i = 0; i < _sinks.n_total (); ++i) {
			DataType dt (i < _sinks.n_midi () ? DataType::MIDI : DataType::AUDIO);
			int idx = (dt == DataType::MIDI) ? i : i - _sinks.n_midi ();
			const Plugin::IOPortDescription& iod (plugin->describe_io_port (dt, true, idx));
			CtrlWidget cw (CtrlWidget (iod.name, Sink, dt, idx, n, iod.is_sidechain));
			_elements.push_back (cw);
		}
		for (uint32_t i = 0; i < _sources.n_total (); ++i) {
			DataType dt (i < _sources.n_midi () ? DataType::MIDI : DataType::AUDIO);
			int idx = (dt == DataType::MIDI) ? i : i - _sources.n_midi ();
			const Plugin::IOPortDescription& iod (plugin->describe_io_port (dt, false, idx));
			_elements.push_back (CtrlWidget (iod.name, Source, dt, idx, n));
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
PluginPinDialog::refill_output_presets ()
{
	using namespace Menu_Helpers;
	_out_presets.clear_items();

	_out_presets.AddMenuElem (MenuElem(_("Automatic"), sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::select_output_preset), 0)));

	PluginOutputConfiguration ppc (_pi->plugin (0)->possible_output ());
	const uint32_t n_audio = _pi->preset_out ().n_audio ();
	if (n_audio == 0) {
		_out_presets.set_text(_("Automatic"));
	}

	if (ppc.find (0) != ppc.end ()) {
		// anyting goes
		ppc.clear ();
		if (n_audio != 0) {
			ppc.insert (n_audio);
		}
		ppc.insert (1);
		ppc.insert (2);
		ppc.insert (8);
		ppc.insert (16);
		ppc.insert (24);
		ppc.insert (32);
	}

	for (PluginOutputConfiguration::const_iterator i = ppc.begin () ; i != ppc.end (); ++i) {
		assert (*i > 0);
		std::string tmp;
		switch (*i) {
			case 1:
				tmp = _("Mono");
				break;
			case 2:
				tmp = _("Stereo");
				break;
			default:
				tmp = string_compose (P_("%1 Channel", "%1 Channels", *i), *i);
				break;
		}
		_out_presets.AddMenuElem (MenuElem(tmp, sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::select_output_preset), *i)));
		if (n_audio == *i) {
			_out_presets.set_text(tmp);
		}
	}
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
		tip += " &lt;- ";
	} else {
		string lpn (PROGRAM_NAME);
		boost::to_lower (lpn);
		std::string program_port_prefix = lpn + ":"; // e.g. "ardour:"

		lbl = cns[0];
		tip += " &lt;- ";
		if (lbl.find ("system:capture_") == 0) {
			lbl = AudioEngine::instance ()->get_pretty_name_by_name (lbl);
			if (lbl.empty ()) {
				lbl = cns[0].substr (15);
			}
		} else if (lbl.find("system:midi_capture_") == 0) {
			lbl = AudioEngine::instance ()->get_pretty_name_by_name (lbl);
			if (lbl.empty ()) {
				// "system:midi_capture_123" -> "123"
				lbl = "M " + cns[0].substr (20);
			}
		} else if (lbl.find (program_port_prefix) == 0) {
			lbl = lbl.substr (program_port_prefix.size());
		}
	}
	for (std::vector<std::string>::const_iterator i = cns.begin(); i != cns.end(); ++i) {
		tip += *i;
		tip += " ";
	}
	replace_all (lbl, "_", " ");

	ArdourButton *pb = manage (new ArdourButton (lbl));
	pb->set_text_ellipsize (Pango::ELLIPSIZE_MIDDLE);
	pb->set_layout_ellipsize_width (108 * PANGO_SCALE);
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
					uint32_t idx = i->e->id;
					if (i->e->dt == DataType::AUDIO) { idx += _sinks.n_midi (); }
					const double x0 = rint ((i->e->ip + .5) * _innerwidth / (double)(_n_plugins)) - .5 - bxw2;
					i->x = _margin_x + rint (x0 + (idx + 1) * bxw / (1. + _sinks.n_total ())) - .5 - dx;
					i->y = yc - bxh2 - dx;
					i->w = _pin_box_size;
					i->h = _pin_box_size;
				}
				break;
			case Source:
				{
					uint32_t idx = i->e->id;
					if (i->e->dt == DataType::AUDIO) { idx += _sources.n_midi (); }
					const double x0 = rint ((i->e->ip + .5) * _innerwidth / (double)(_n_plugins)) - .5 - bxw2;
					i->x = _margin_x + rint (x0 + (idx + 1) * bxw / (1. + _sources.n_total ())) - .5 - dx;
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

	if ((w.prelight || w.e == _selection) && !w.name.empty()) {
		int text_width;
		int text_height;
		Glib::RefPtr<Pango::Layout> layout;
		layout = Pango::Layout::create (get_pango_context ());
		layout->set_text (w.name);
		layout->get_pixel_size (text_width, text_height);

		rounded_rectangle (cr, w.x + dx - .5 * text_width - 2, w.y - text_height - 2,  text_width + 4, text_height + 2, 7);
		cairo_set_source_rgba (cr, 0, 0, 0, .5);
		cairo_fill (cr);

		cairo_move_to (cr, w.x + dx - .5 * text_width, w.y - text_height - 1);
		cairo_set_source_rgba (cr, 1., 1., 1., 1.);
		pango_cairo_show_layout (cr, layout->gobj ());
	}
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
	assert (0);
	fatal << string_compose (_("programming error: %1"),
			X_("Invalid Plugin I/O Port."))
		<< endmsg;
	abort (); /*NOTREACHED*/
	static CtrlWidget screw_old_compilers ("", Input, DataType::NIL, 0);
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

	if (_pi->signal_latency () > 0) {
		// TODO: this needs a better location also format to msec (and cache)
		layout->set_width ((_innerwidth - 2 * _pin_box_size) * PANGO_SCALE);
		layout->set_text (string_compose (_("Latency %1 spl"), _pi->signal_latency ()));
		layout->get_pixel_size (text_width, text_height);
		cairo_move_to (cr, _margin_x + _pin_box_size * .5, _margin_y + 2);
		cairo_set_source_rgba (cr, 1., 1., 1., 1.);
		pango_cairo_show_layout (cr, layout->gobj ());
	}

	if (_pi->strict_io () && !Profile->get_mixbus ()) {
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

	/* draw midi-bypass (behind) */
	if (_pi->has_midi_bypass ()) {
		const CtrlWidget& cw0 = get_io_ctrl (Input, DataType::MIDI, 0);
		const CtrlWidget& cw1 = get_io_ctrl (Output, DataType::MIDI, 0);
		draw_connection (cr, cw0, cw1, true);
	}

	/* thru connections */
	const ChanMapping::Mappings thru_map = _pi->thru_map ().mappings ();
	for (ChanMapping::Mappings::const_iterator t = thru_map.begin (); t != thru_map.end (); ++t) {
		for (ChanMapping::TypeMapping::const_iterator c = (*t).second.begin (); c != (*t).second.end () ; ++c) {
			const CtrlWidget& cw0 = get_io_ctrl (Output, t->first, c->first);
			const CtrlWidget& cw1 = get_io_ctrl (Input, t->first, c->second);
			if (!(_dragging && cw1.e == _selection && cw0.e == _drag_dst)) {
				draw_connection (cr, cw1, cw0, true);
			}
		}
	}


	/* plugins & connection wires */
	for (uint32_t i = 0; i < _n_plugins; ++i) {
		double x0 = _margin_x + rint ((i + .5) * _innerwidth / (double)(_n_plugins)) - .5;

		/* plugin box */
		cairo_set_source_rgb (cr, .5, .5, .5);
		rounded_rectangle (cr, x0 - _bxw2, yc - _bxh2, 2 * _bxw2, 2 * _bxh2, 7);
		cairo_fill (cr);

		layout->set_width (1.9 * _bxw2 * PANGO_SCALE);
		layout->set_text (string_compose (_("Plugin #%1"), i + 1));
		layout->get_pixel_size (text_width, text_height);
		cairo_move_to (cr, x0 - text_width * .5, yc - text_height * .5);
		cairo_set_source_rgba (cr, 1., 1., 1., 1.);
		pango_cairo_show_layout (cr, layout->gobj ());

		const ChanMapping::Mappings in_map = _pi->input_map (i).mappings ();
		const ChanMapping::Mappings out_map = _pi->output_map (i).mappings ();

		for (ChanMapping::Mappings::const_iterator t = in_map.begin (); t != in_map.end (); ++t) {
			for (ChanMapping::TypeMapping::const_iterator c = (*t).second.begin (); c != (*t).second.end () ; ++c) {
				const CtrlWidget& cw0 = get_io_ctrl (Input, t->first, c->second);
				const CtrlWidget& cw1 = get_io_ctrl (Sink, t->first, c->first, i);
				if (!(_dragging && cw0.e == _selection && cw1.e == _drag_dst)) {
					draw_connection (cr, cw0, cw1);
				}
			}
		}

		for (ChanMapping::Mappings::const_iterator t = out_map.begin (); t != out_map.end (); ++t) {
			for (ChanMapping::TypeMapping::const_iterator c = (*t).second.begin (); c != (*t).second.end () ; ++c) {
				const CtrlWidget& cw0 = get_io_ctrl (Source, t->first, c->first, i);
				const CtrlWidget& cw1 = get_io_ctrl (Output, t->first, c->second);
				if (!(_dragging && cw0.e == _selection && cw1.e == _drag_dst)) {
					draw_connection (cr, cw0, cw1);
				}
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

	/* DnD wire */
	CtrlWidget *drag_src = NULL;
	if (_dragging) {
		for (CtrlElemList::iterator i = _elements.begin (); i != _elements.end (); ++i) {
			if (i->e  == _selection ) {
				drag_src = &(*i);
			}
		}
	}

	if (drag_src) {
		double x0, y0;
		if (_selection->ct == Input || _selection->ct == Source) {
			edge_coordinates (*drag_src, x0, y0);
			draw_connection (cr, x0, _drag_x, y0, _drag_y,
					_selection->dt == DataType::MIDI, _selection->sc);
		} else {
			edge_coordinates (*drag_src, x0, y0);
			draw_connection (cr, _drag_x, x0, _drag_y, y0,
					_selection->dt == DataType::MIDI, _selection->sc);
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
PluginPinDialog::drag_type_matches (const CtrlElem& e)
{
	if (!_dragging || !_selection) {
		return true;
	}
	if (_selection->dt != e->dt) {
		return false;
	}
	if (_selection->ct == Input  && e->ct == Sink)   { return true; }
	if (_selection->ct == Sink   && e->ct == Input)  { return true; }
	if (_selection->ct == Output && e->ct == Source) { return true; }
	if (_selection->ct == Source && e->ct == Output) { return true; }
	if (_selection->ct == Input  && e->ct == Output) { return true; }
	if (_selection->ct == Output && e->ct == Input)  { return true; }
	return false;
}

void
PluginPinDialog::start_drag (const CtrlElem& e, double x, double y)
{
	assert (_selection == e);
	_drag_dst.reset ();
	if (e->ct == Sink) {
		bool valid;
		const ChanMapping& map (_pi->input_map (e->ip));
		uint32_t idx = map.get (e->dt, e->id, &valid);
		if (valid) {
			const CtrlWidget& cw = get_io_ctrl (Input, e->dt, idx, 0);
			_drag_dst = e;
			_selection = cw.e;
		}
	}
	else if (e->ct == Output) {
		for (uint32_t i = 0; i < _n_plugins; ++i) {
			bool valid;
			const ChanMapping& map (_pi->output_map (i));
			uint32_t idx = map.get_src (e->dt, e->id, &valid);
			if (valid) {
				const CtrlWidget& cw = get_io_ctrl (Source, e->dt, idx, i);
				_drag_dst = e;
				_selection = cw.e;
				break;
			}
		}
		if (!_drag_dst) {
			bool valid;
			const ChanMapping& map (_pi->thru_map ());
			uint32_t idx = map.get (e->dt, e->id, &valid);
			if (valid) {
				const CtrlWidget& cw = get_io_ctrl (Input, e->dt, idx, 0);
				_drag_dst = e;
				_selection = cw.e;
			}
		}
	}
	_dragging = true;
	_drag_x = x;
	_drag_y = y;
}

bool
PluginPinDialog::darea_motion_notify_event (GdkEventMotion* ev)
{
	bool changed = false;
	_hover.reset ();
	for (CtrlElemList::iterator i = _elements.begin (); i != _elements.end (); ++i) {
		if (ev->x >= i->x && ev->x <= i->x + i->w
				&& ev->y >= i->y && ev->y <= i->y + i->h
				&& drag_type_matches (i->e))
		{
			if (!i->prelight) changed = true;
			i->prelight = true;
			_hover = i->e;
		} else {
			if (i->prelight) changed = true;
			i->prelight = false;
		}
	}
	if (_dragging) {
		_drag_x = ev->x;
		_drag_y = ev->y;
	}
	if (changed || _dragging) {
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
			_drag_dst.reset ();
			if (!_selection || (_selection && !_hover)) {
				_selection = _hover;
				_actor.reset ();
				if (_selection) {
					start_drag (_selection, ev->x, ev->y);
				}
				darea.queue_draw ();
			} else if (_selection && _hover && _selection != _hover) {
				if (_selection->dt != _hover->dt) { _actor.reset (); }
				else if (_selection->ct == Input  && _hover->ct == Sink)   { _actor = _hover; }
				else if (_selection->ct == Sink   && _hover->ct == Input)  { _actor = _hover; }
				else if (_selection->ct == Output && _hover->ct == Source) { _actor = _hover; }
				else if (_selection->ct == Source && _hover->ct == Output) { _actor = _hover; }
				else if (_selection->ct == Input  && _hover->ct == Output) { _actor = _hover; }
				else if (_selection->ct == Output && _hover->ct == Input)  { _actor = _hover; }
				if (!_actor) {
					_selection = _hover;
					start_drag (_selection, ev->x, ev->y);
				}
				darea.queue_draw ();
			} else if (_hover) {
				_selection = _hover;
				_actor.reset ();
				start_drag (_selection, ev->x, ev->y);
			}
			break;
		case 3:
			_drag_dst.reset ();
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
	if (_dragging && _selection && _drag_dst && _drag_dst == _hover) {
		// select click. (or re-connect same)
		assert (_selection != _hover);
		_actor.reset ();
		_dragging = false;
		_drag_dst.reset ();
		_selection =_hover;
		darea.queue_draw ();
		return true;
	}

	if (_dragging && _hover && _hover != _selection) {
		_actor = _hover;
	}

	if (_hover == _actor && _actor && ev->button == 1) {
		assert (_selection);
		assert (_selection->dt == _actor->dt);
		if (_drag_dst) {
			assert (_dragging && _selection != _drag_dst);
			handle_disconnect (_drag_dst, true);
		}
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
		else if (_selection->ct == Input && _actor->ct == Output) {
			handle_thru_action (_actor, _selection);
		}
		else if (_selection->ct == Output && _actor->ct == Input) {
			handle_thru_action (_selection, _actor);
		}
		_selection.reset ();
	} else if (_hover == _selection && _selection && ev->button == 3) {
		handle_disconnect (_selection);
	} else if (!_hover && ev->button == 3) {
		reset_menu.popup (1, ev->time);
	}

	if (_dragging && _hover != _selection) {
		_selection.reset ();
	}
	_actor.reset ();
	_dragging = false;
	_drag_dst.reset ();
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
		if (!_dragging) {
			in_map.unset (s->dt, s->id);
			_pi->set_input_map (pc, in_map);
		} else {
			plugin_reconfigured ();
		}
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
PluginPinDialog::disconnect_other_outputs (uint32_t skip_pc, DataType dt, uint32_t id)
{
	_ignore_updates = true;
	for (uint32_t n = 0; n < _n_plugins; ++n) {
		if (n == skip_pc) {
			continue;
		}
		bool valid;
		ChanMapping n_out_map (_pi->output_map (n));
		uint32_t idx = n_out_map.get_src (dt, id, &valid);
		if (valid) {
			n_out_map.unset (dt, idx);
			_pi->set_output_map (n, n_out_map);
		}
	}
	_ignore_updates = false;
}

void
PluginPinDialog::disconnect_other_thru (DataType dt, uint32_t id)
{
	_ignore_updates = true;
	bool valid;
	ChanMapping n_thru_map (_pi->thru_map ());
	n_thru_map.get (dt, id, &valid);
	if (valid) {
		n_thru_map.unset (dt, id);
		_pi->set_thru_map (n_thru_map);
	}
	_ignore_updates = false;
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
		if (!_dragging) {
			out_map.unset (s->dt, s->id);
			_pi->set_output_map (pc, out_map);
		} else {
			plugin_reconfigured ();
		}
	}
	else {
		// disconnect source
		disconnect_other_outputs (pc, s->dt, o->id);
		disconnect_other_thru (s->dt, o->id);
		out_map = _pi->output_map (pc); // re-read map
		if (valid) {
			out_map.unset (s->dt, s->id);
		}
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
PluginPinDialog::handle_thru_action (const CtrlElem &o, const CtrlElem &i)
{
	bool valid;
	ChanMapping thru_map (_pi->thru_map ());
	uint32_t idx = thru_map.get (o->dt, o->id, &valid);

	if (valid && idx == i->id) {
		if (!_dragging) {
			thru_map.unset (o->dt, o->id);
		}
	} else {
		// disconnect other outputs first
		disconnect_other_outputs (UINT32_MAX, o->dt, o->id);
		disconnect_other_thru (o->dt, o->id);
		thru_map = _pi->thru_map (); // re-read map

		thru_map.set (o->dt, o->id, i->id);
	}
	_pi->set_thru_map (thru_map);
}

bool
PluginPinDialog::handle_disconnect (const CtrlElem &e, bool no_signal)
{
	_ignore_updates = true;
	bool changed = false;
	bool valid;

	switch (e->ct) {
		case Input:
			{
				ChanMapping n_thru_map (_pi->thru_map ());
				for (uint32_t i = 0; i < _sources.n_total (); ++i) {
					uint32_t idx = n_thru_map.get (e->dt, i, &valid);
					if (valid && idx == e->id) {
						n_thru_map.unset (e->dt, i);
						changed = true;
					}
				}
				if (changed) {
					_pi->set_thru_map (n_thru_map);
				}
			}
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
				if (changed) {
					_pi->set_output_map (n, map);
				}
			}
			{
				ChanMapping n_thru_map (_pi->thru_map ());
				n_thru_map.get (e->dt, e->id, &valid);
				if (valid) {
					n_thru_map.unset (e->dt, e->id);
					changed = true;
					_pi->set_thru_map (n_thru_map);
				}
			}
			break;
	}
	_ignore_updates = false;
	if (changed && !no_signal) {
		plugin_reconfigured ();
	}
	return changed;
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
	if (_set_config.get_active ()) {
		_route ()->reset_plugin_insert (_pi);
	} else {
		_route ()->customize_plugin_insert (_pi, _n_plugins, _out);
	}
}

void
PluginPinDialog::reset_mapping ()
{
	_pi->reset_map ();
}

void
PluginPinDialog::select_output_preset (uint32_t n_audio)
{
	if (_session && _session->actively_recording()) { return; }
	ChanCount out (DataType::AUDIO, n_audio);
	_route ()->plugin_preset_output (_pi, out);
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
	p->disconnect_all ();
}

void
PluginPinDialog::connect_port (boost::weak_ptr<ARDOUR::Port> wp0, boost::weak_ptr<ARDOUR::Port> wp1)
{
	if (_session && _session->actively_recording()) { return; }
	boost::shared_ptr<ARDOUR::Port> p0 = wp0.lock ();
	boost::shared_ptr<ARDOUR::Port> p1 = wp1.lock ();
	boost::shared_ptr<IO> io = _pi->sidechain_input ();
	if (!io || !p0 || !p1) {
		return;
	}
	_ignore_updates = true;
	p0->disconnect_all ();
	_ignore_updates = false;
	p0->connect (p1->name ());
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

struct RouteCompareByName {
	bool operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b) {
		return a->name().compare (b->name()) < 0;
	}
};

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

		boost::shared_ptr<Port> p = wp.lock ();
		if (p && p->connected ()) {
			citems.push_back (MenuElem (_("Disconnect"), sigc::bind (sigc::mem_fun (*this, &PluginPinDialog::disconnect_port), wp)));
			citems.push_back (SeparatorElem());
		}

#if 0
		// TODO add system inputs, too ?!
		boost::shared_ptr<ARDOUR::BundleList> b = _session->bundles ();
		for (ARDOUR::BundleList::iterator i = b->begin(); i != b->end(); ++i) {
			for (uint32_t j = 0; j < i->nchannels ().n_total (); ++j) {
			}
			//maybe_add_bundle_to_input_menu (*i, current);
		}
#endif

		boost::shared_ptr<ARDOUR::RouteList> routes = _session->get_routes ();
		RouteList copy = *routes;
		copy.sort (RouteCompareByName ());
		uint32_t added = 0;
		for (ARDOUR::RouteList::const_iterator i = copy.begin(); i != copy.end(); ++i) {
			added += maybe_add_route_to_input_menu (*i, p->type (), wp);
		}

		if (added > 0) {
			citems.push_back (SeparatorElem());
		}
		citems.push_back (MenuElem (_("Routing Grid"), sigc::mem_fun (*this, &PluginPinDialog::connect_sidechain)));
		input_menu.popup (1, ev->time);
	}
	return false;
}

uint32_t
PluginPinDialog::maybe_add_route_to_input_menu (boost::shared_ptr<Route> r, DataType dt, boost::weak_ptr<Port> wp)
{
	uint32_t added = 0;
	using namespace Menu_Helpers;
	if (r->output () == _route()->output()) {
		return added;
	}

	if (_route ()->feeds_according_to_graph (r)) {
		return added;
	}

	MenuList& citems = input_menu.items();
	const IOVector& iov (r->all_outputs());

	for (IOVector::const_iterator o = iov.begin(); o != iov.end(); ++o) {
		boost::shared_ptr<IO> op = o->lock();
		if (!op) {
			continue;
		}
		PortSet& p (op->ports ());
		for (PortSet::iterator i = p.begin (dt); i != p.end (dt); ++i) {
			std::string n = i->name ();
			replace_all (n, "_", " ");
			citems.push_back (MenuElem (n, sigc::bind (sigc::mem_fun(*this, &PluginPinDialog::connect_port), wp, boost::weak_ptr<Port> (*i))));
			++added;
		}
	}
	return added;
}

void
PluginPinDialog::port_connected_or_disconnected (boost::weak_ptr<ARDOUR::Port> w0, boost::weak_ptr<ARDOUR::Port> w1)
{
	boost::shared_ptr<Port> p0 = w0.lock ();
	boost::shared_ptr<Port> p1 = w1.lock ();

	boost::shared_ptr<IO> io = _pi->sidechain_input ();
	if (!io) { return; }

	if (p0 && io->has_port (p0)) {
		plugin_reconfigured ();
	}
	else if (p1 && io->has_port (p1)) {
		plugin_reconfigured ();
	}
}
