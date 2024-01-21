/*
 * Copyright (C) 2023 Robin Gareus <robin@gareus.org>
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

#include "pbd/fastlog.h"

#include "ardour/logmeter.h"
#include "ardour/meter.h"
#include "ardour/profile.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/surround_return.h"
#include "ardour/value_as_string.h"
#include "ardour/vca_manager.h"

#include "gtkmm2ext/utils.h"

#include "widgets/tooltips.h"

#include "ardour_window.h"
#include "surround_strip.h"

#include "gui_thread.h"
#include "io_selector.h"
#include "keyboard.h"
#include "meter_patterns.h"
#include "mixer_ui.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace PBD;
using namespace Gtk;

#define PX_SCALE(px) std::max ((float)px, rintf ((float)px* UIConfiguration::instance ().get_ui_scale ()))

PBD::Signal1<void, SurroundStrip*> SurroundStrip::CatchDeletion;

SurroundStrip::SurroundStrip (Mixer_UI& mx, Session* s, std::shared_ptr<Route> r)
	: SessionHandlePtr (s)
	, RouteUI (s)
	, _width (80)
	, _output_button (false)
	, _comment_button (_("Comments"))
	, _level_control (ArdourKnob::default_elements, ArdourKnob::Detent)
{
	init ();
	set_route (r);
}

SurroundStrip::~SurroundStrip ()
{
	CatchDeletion (this);
	for (int i = 0; i < 14; ++i) {
		delete _meter[i];
	}
}

void
SurroundStrip::init ()
{
	_name_button.set_name ("mixer strip button");
	_name_button.set_text_ellipsize (Pango::ELLIPSIZE_END);
	_name_button.set_layout_ellipsize_width (PX_SCALE (_width) * PANGO_SCALE);

	_lufs_cap.set_name("OptionsLabel");
	_lufs_cap.set_alignment(1.0, 0.5);
	_lufs_cap.set_use_markup();
	_lufs_cap.set_markup ("<span size=\"large\" weight=\"bold\">LUFS:</span>");

	_lufs_label.set_name("OptionsLabel");
	_lufs_label.set_alignment(0.0, 0.5);
	_lufs_label.set_use_markup();
	_lufs_label.set_markup ("<span size=\"large\" weight=\"bold\"> --- </span>");

	_dbtp_cap.set_name("OptionsLabel");
	_dbtp_cap.set_alignment(1.0, 0.5);
	_dbtp_cap.set_use_markup();
	_dbtp_cap.set_markup ("<span size=\"large\" weight=\"bold\">dBTP:</span>");

	_dbtp_label.set_name("OptionsLabel");
	_dbtp_label.set_alignment(0.0, 0.5);
	_dbtp_label.set_use_markup();
	_dbtp_label.set_markup ("<span size=\"large\" weight=\"bold\"> --- </span>");

	Gtk::Table *lufs_table = manage(new Gtk::Table());
	lufs_table->set_homogeneous(true);
	lufs_table->set_border_width(2);
	lufs_table->set_spacings(4);
	lufs_table->attach(_lufs_cap,   0, 1, 0, 1, FILL|EXPAND, SHRINK);
	lufs_table->attach(_lufs_label, 1, 2, 0, 1, FILL|EXPAND, SHRINK);
	lufs_table->attach(_dbtp_cap,   0, 1, 1, 2, FILL|EXPAND, SHRINK);
	lufs_table->attach(_dbtp_label, 1, 2, 1, 2, FILL|EXPAND, SHRINK);

	uint32_t c[10];
	uint32_t b[4];
	float stp[4];

	c[0] = UIConfiguration::instance().color ("meter color0");
	c[1] = UIConfiguration::instance().color ("meter color1");
	c[2] = UIConfiguration::instance().color ("meter color2");
	c[3] = UIConfiguration::instance().color ("meter color3");
	c[4] = UIConfiguration::instance().color ("meter color4");
	c[5] = UIConfiguration::instance().color ("meter color5");
	c[6] = UIConfiguration::instance().color ("meter color6");
	c[7] = UIConfiguration::instance().color ("meter color7");
	c[8] = UIConfiguration::instance().color ("meter color8");
	c[9] = UIConfiguration::instance().color ("meter color9");
	b[0] = UIConfiguration::instance().color ("meter background bottom");
	b[1] = UIConfiguration::instance().color ("meter background top");
	b[2] = 0x991122ff; // red highlight gradient Bot
	b[3] = 0x551111ff; // red highlight gradient Top

	stp[0] = 115.0 * log_meter0dB (-15);
	stp[1] = 115.0 * log_meter0dB (-9);
	stp[2] = 115.0 * log_meter0dB (-3);
	stp[3] = 115.0;

	// XXX config changed -> update meter style (and size)

	for (int i = 0; i < 12; ++i) {
		_meter[i] = new FastMeter ((uint32_t)floor (UIConfiguration::instance ().get_meter_hold ()),
		                           8, FastMeter::Horizontal, PX_SCALE (100),
		                           c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8], c[9],
		                           b[0], b[1], b[2], b[3],
		                           stp[0], stp[1], stp[2], stp[3],
		                           (UIConfiguration::instance ().get_meter_style_led () ? 3 : 1));

		_surround_meter_box.pack_start (*_meter[i], false, false, 0);
	}

	_binaural_meter_box.pack_start (_meter_ticks1_area, false, false);
	for (int i = 12; i < 14; ++i) {
		_meter[i] = new FastMeter ((uint32_t)floor (UIConfiguration::instance ().get_meter_hold ()),
		                           8, FastMeter::Vertical, PX_SCALE (250),
		                           c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8], c[9],
		                           b[0], b[1], b[2], b[3],
		                           stp[0], stp[1], stp[2], stp[3],
		                           (UIConfiguration::instance ().get_meter_style_led () ? 3 : 1));

		_binaural_meter_box.pack_start (*_meter[i], false, false, 1);
	}
	_binaural_meter_box.pack_start (_meter_ticks2_area, false, false);
	_binaural_meter_box.pack_start (_meter_metric_area, false, false);

	_types.push_back (DataType::AUDIO);
	_types.push_back (DataType::AUDIO);

	_meter_metric_area.set_size_request (PX_SCALE(24), -1);
  _meter_ticks1_area.set_size_request (PX_SCALE(3), -1);
  _meter_ticks2_area.set_size_request (PX_SCALE(3), -1);

	_level_control.set_size_request (PX_SCALE (50), PX_SCALE (50));
	_level_control.set_tooltip_prefix (_("Level: "));
	_level_control.set_name ("monitor section knob");

	VBox* lcenter_box = manage (new VBox);
	lcenter_box->pack_start (_level_control, true, false);
	_level_box.pack_start (*lcenter_box, true, false);
	_level_box.set_size_request (-1, PX_SCALE (80));
	_level_box.set_name ("AudioBusStripBase");
	lcenter_box->show ();

	_output_button.set_text (_("Output"));
	_output_button.set_name ("mixer strip button");
	_output_button.set_text_ellipsize (Pango::ELLIPSIZE_MIDDLE);
	_output_button.set_layout_ellipsize_width (PX_SCALE (_width) * PANGO_SCALE);

	_comment_button.set_name (X_("mixer strip button"));
	_comment_button.set_text_ellipsize (Pango::ELLIPSIZE_END);
	_comment_button.set_layout_ellipsize_width (PX_SCALE (_width) * PANGO_SCALE);

	_global_vpacker.set_border_width (1);
	_global_vpacker.set_spacing (2);

	Gtk::Label* top_spacer = manage (new Gtk::Label);
	top_spacer->show ();

	_global_vpacker.pack_start (*top_spacer, false, false, PX_SCALE (3));
	_global_vpacker.pack_start (_name_button, Gtk::PACK_SHRINK);
	_global_vpacker.pack_start (_top_box, true, true); // expanding space

	update_spacers ();

#ifndef MIXBUS
	_global_vpacker.pack_end (_spacer, false, false);
#endif

	_binaural_meter_hbox.pack_end (_binaural_meter_box, false, false);

	_global_vpacker.pack_end (_comment_button, Gtk::PACK_SHRINK);
	_global_vpacker.pack_end (_output_button, Gtk::PACK_SHRINK);
	_global_vpacker.pack_end (_spacer_ctrl, Gtk::PACK_SHRINK);
	_global_vpacker.pack_end (_binaural_meter_hbox, Gtk::PACK_SHRINK);
	_global_vpacker.pack_end (_spacer_peak, Gtk::PACK_SHRINK);
	_global_vpacker.pack_end (*mute_button, false, false);
	_global_vpacker.pack_end (_level_box, Gtk::PACK_SHRINK);
	_global_vpacker.pack_end (_surround_meter_box, false, false, PX_SCALE (3));
	_global_vpacker.pack_end (*lufs_table, false, false);

	_global_frame.add (_global_vpacker);
	_global_frame.set_shadow_type (Gtk::SHADOW_IN);
	_global_frame.set_name ("MixerStripFrame");
	add (_global_frame);

	_name_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &SurroundStrip::name_button_button_press), false);
	_comment_button.signal_clicked.connect (sigc::mem_fun (*this, &RouteUI::toggle_comment_editor));

	_meter_metric_area.signal_expose_event().connect (sigc::mem_fun(*this, &SurroundStrip::meter_metrics_expose));
  _meter_ticks1_area.signal_expose_event().connect (sigc::mem_fun(*this, &SurroundStrip::meter_ticks1_expose));
  _meter_ticks2_area.signal_expose_event().connect (sigc::mem_fun(*this, &SurroundStrip::meter_ticks2_expose));


	add_events (Gdk::BUTTON_RELEASE_MASK |
	            Gdk::ENTER_NOTIFY_MASK |
	            Gdk::KEY_PRESS_MASK |
	            Gdk::KEY_RELEASE_MASK);

	set_can_focus ();

	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &SurroundStrip::parameter_changed));

	//PresentationInfo::Change.connect (*this, invalidator (*this), boost::bind (&SurroundStrip::presentation_info_changed, this, _1), gui_context ());
}

void
SurroundStrip::update_spacers ()
{
	std::string viz = UIConfiguration::instance().get_mixer_strip_visibility ();

	Gtk::Window window (WINDOW_TOPLEVEL);
	VBox         box;
	FocusEntry   pk;
	HScrollbar   scrollbar;

	ArdourButton small_btn ("btn");
	ArdourButton vca_btn (_("-VCAs-"));

	small_btn.set_name ("mixer strip button");
	small_btn.set_size_request (PX_SCALE(15), PX_SCALE(15));
	small_btn.ensure_style ();

	vca_btn.set_name (X_("vca assign button"));
	vca_btn.ensure_style ();

	scrollbar.set_name ("MixerWindow");
	scrollbar.ensure_style ();

	pk.set_name ("MixerStripPeakDisplay");
	pk.ensure_style ();
	Gtkmm2ext::set_size_request_to_display_given_text (pk, "-80.g", 2, 6);

	box.pack_start (pk);
	box.pack_start (small_btn);
	box.pack_start (scrollbar);
	box.pack_start (vca_btn);

	window.add (box);
	window.show_all ();

	_spacer.set_size_request (-1, scrollbar.size_request ().height + 3);
	_spacer_peak.set_size_request (-1, pk.size_request ().height + 3);

	int h = small_btn.size_request ().height;
	if (viz.find ("VCA") != std::string::npos && !_session->vca_manager().vcas().empty ()) {
		h += vca_btn.size_request ().height;
	}
	_spacer_ctrl.set_size_request (-1, h);
}

void
SurroundStrip::parameter_changed (std::string const& p)
{
	if (p == "mixer-element-visibility") {
		update_spacers ();
	}
}

void
SurroundStrip::set_route (std::shared_ptr<Route> r)
{
	assert (r);
	RouteUI::set_route (r);

	_output_button.set_route (_route, this);

	_level_control.set_controllable (_route->gain_control ());
	_level_control.show ();

	/* set up metering */
	_route->set_meter_type (MeterPeak0dB);

	_route->comment_changed.connect (route_connections, invalidator (*this), boost::bind (&SurroundStrip::setup_comment_button, this), gui_context ());

	_route->gain_control ()->MasterStatusChange.connect (route_connections, invalidator (*this), boost::bind (&SurroundStrip::update_spacers, this), gui_context());

	/* now force an update of all the various elements */
	name_changed ();
	comment_changed ();
	setup_comment_button ();

	add_events (Gdk::BUTTON_RELEASE_MASK);
	show_all ();
}

void
SurroundStrip::setup_comment_button ()
{
	std::string comment = _route->comment ();

	set_tooltip (_comment_button, comment.empty () ? _("Click to add/edit comments") : _route->comment ());

	if (comment.empty ()) {
		_comment_button.set_name ("generic button");
		_comment_button.set_text (_("Comments"));
		return;
	}

	_comment_button.set_name ("comment button");

	std::string::size_type pos = comment.find_first_of (" \t\n");
	if (pos != std::string::npos) {
		comment = comment.substr (0, pos);
	}
	if (comment.empty ()) {
		_comment_button.set_text (_("Comments"));
	} else {
		_comment_button.set_text (comment);
	}
}

Gtk::Menu*
SurroundStrip::build_route_ops_menu ()
{
	using namespace Menu_Helpers;

	Menu*     menu  = manage (new Menu);
	MenuList& items = menu->items ();
	menu->set_name ("ArdourContextMenu");

	assert (_route->active ());

	items.push_back (MenuElem (_("Color..."), sigc::mem_fun (*this, &RouteUI::choose_color)));
	items.push_back (MenuElem (_("Comments..."), sigc::mem_fun (*this, &RouteUI::open_comment_editor)));

	items.push_back (MenuElem (_("Outputs..."), sigc::mem_fun (*this, &RouteUI::edit_output_configuration)));

	items.push_back (SeparatorElem ());

	items.push_back (MenuElem (_("Rename..."), sigc::mem_fun (*this, &RouteUI::route_rename)));

	items.push_back (SeparatorElem ());

	if (!Profile->get_mixbus ()) {
		items.push_back (CheckMenuElem (_("Protect Against Denormals"), sigc::mem_fun (*this, &RouteUI::toggle_denormal_protection)));
		denormal_menu_item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
		denormal_menu_item->set_active (_route->denormal_protection ());
	}

	return menu;
}

bool
SurroundStrip::name_button_button_press (GdkEventButton* ev)
{
	if (Gtkmm2ext::Keyboard::is_context_menu_event (ev)) {
		Menu* r_menu = build_route_ops_menu ();
		r_menu->popup (ev->button, ev->time);
		return true;
	}
	return false;
}

void
SurroundStrip::fast_update ()
{
	std::shared_ptr<PeakMeter> peak_meter = _route->shared_peak_meter ();
	for (uint32_t i = 0; i < 14; ++i) {
		const float meter_level = peak_meter->meter_level (i, MeterPeak0dB);
		_meter[i]->set (log_meter0dB (meter_level));
	}

	std::shared_ptr<SurroundReturn> sur = _route->surround_return ();

	//these 2 text meters should only be updated while rolling or exporting
	if (_route->session().transport_rolling()) {
		float loud = sur->integrated_loudness();
		if (loud > -90) {
			char buf[32];
			sprintf(buf, "%3.1f", loud);
			_lufs_label.set_markup (string_compose ("<span size=\"large\" weight=\"bold\">%1</span>", buf));
		} else {
			_lufs_label.set_markup ("-");
		}

		float dbtp = sur->max_dbtp();
		if (dbtp > -90) {
			char buf[32];
			sprintf(buf, "%3.1f", dbtp);
			_dbtp_label.set_markup (string_compose ("<span size=\"large\" weight=\"bold\">%1</span>", buf));
		} else {
			_dbtp_label.set_markup ("-");
		}
	}
}

void
SurroundStrip::route_property_changed (const PropertyChange& what_changed)
{
	if (what_changed.contains (ARDOUR::Properties::name)) {
		name_changed ();
	}
}

void
SurroundStrip::name_changed ()
{
	_name_button.set_text (_route->name ());
	set_tooltip (_name_button, Gtkmm2ext::markup_escape_text (_route->name ()));
}

void
SurroundStrip::set_button_names ()
{
	mute_button->set_text (_("Mute"));
}

void
SurroundStrip::hide_spacer (bool yn)
{
	if (!yn) {
		_spacer.show ();
	} else {
		_spacer.hide ();
	}
}
gint
SurroundStrip::meter_metrics_expose (GdkEventExpose* ev)
{
	return ArdourMeter::meter_expose_metrics (ev, MeterPeak0dB, _types, &_meter_metric_area);
}

gint
SurroundStrip::meter_ticks1_expose (GdkEventExpose* ev)
{
	return ArdourMeter::meter_expose_ticks (ev, MeterPeak0dB, _types, &_meter_ticks1_area);
}

gint
SurroundStrip::meter_ticks2_expose (GdkEventExpose* ev)
{
	 return ArdourMeter::meter_expose_ticks (ev, MeterPeak0dB, _types, &_meter_ticks2_area);
}
