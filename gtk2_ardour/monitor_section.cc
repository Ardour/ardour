/*
 * Copyright (C) 2010-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2011-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#include <gdkmm/pixbuf.h>

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/replace_all.h"

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/utils.h"

#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>

#include "widgets/tearoff.h"
#include "widgets/tooltips.h"

#include "ardour/amp.h"
#include "ardour/audioengine.h"
#include "ardour/monitor_processor.h"
#include "ardour/port.h"
#include "ardour/route.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/user_bundle.h"
#include "ardour/plugin_manager.h"

#include "ardour_ui.h"
#include "gui_thread.h"
#include "mixer_ui.h"
#include "monitor_section.h"
#include "public_editor.h"
#include "timers.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace ARDOUR_UI_UTILS;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace std;

#define PX_SCALE(px) std::max((float)px, rintf((float)px * UIConfiguration::instance().get_ui_scale()))

#define SYNCHRONIZE_TOGGLE_ACTION(action, value) \
	if (action && action->get_active() != value) { \
		action->set_active(value); \
	}

MonitorSection::MonitorSection ()
	: RouteUI ((Session*) 0)
	, _tearoff (0)
	, channel_table (0)
	, channel_table_viewport (*channel_table_scroller.get_hadjustment()
	                          , *channel_table_scroller.get_vadjustment ())
	, gain_control (0)
	, dim_control (0)
	, solo_boost_control (0)
	, solo_cut_control (0)
	, gain_display (0)
	, dim_display (0)
	, solo_boost_display (0)
	, solo_cut_display (0)
	, _output_selector (0)
	, solo_in_place_button (_("SiP"), ArdourButton::led_default_elements)
	, afl_button (_("AFL"), ArdourButton::led_default_elements)
	, pfl_button (_("PFL"), ArdourButton::led_default_elements)
	, exclusive_solo_button (ArdourButton::led_default_elements)
	, solo_mute_override_button (ArdourButton::led_default_elements)
	, toggle_processorbox_button (ArdourButton::default_elements)
	, _inhibit_solo_model_update (false)
	, _rr_selection ()
	, _ui_initialized (false)
{
	/* note that although this a RouteUI, we never called ::set_route() so
	 * we do not need to worry about self-destructing when the Route (the
	 * monitor out) is destroyed.
	 */

	using namespace Menu_Helpers;

	Glib::RefPtr<Action> act;

	load_bindings ();
	register_actions ();
	set_data ("ardour-bindings", bindings);

	channel_size_group = SizeGroup::create (SIZE_GROUP_HORIZONTAL);

	insert_box = new ProcessorBox (0, boost::bind (&MonitorSection::plugin_selector, this), _rr_selection, 0);
	insert_box->set_no_show_all ();
	insert_box->show ();
	// TODO allow keyboard shortcuts in ProcessorBox

	/* Rude Solo  & Solo Isolated */
	rude_solo_button.set_text (_("Soloing"));
	rude_solo_button.set_name ("rude solo");
	rude_solo_button.show ();

	rude_iso_button.set_text (_("Isolated"));
	rude_iso_button.set_name ("rude isolate");
	rude_iso_button.show ();

	rude_audition_button.set_text (_("Auditioning"));
	rude_audition_button.set_name ("rude audition");
	rude_audition_button.show ();

	Timers::blink_connect (sigc::mem_fun (*this, &MonitorSection::do_blink));

	UI::instance()->set_tip (rude_solo_button, _("When active, something is soloed.\nClick to de-solo everything"));

	rude_iso_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MonitorSection::cancel_isolate), false);
	UI::instance()->set_tip (rude_iso_button, _("When active, something is solo-isolated.\nClick to de-isolate everything"));

	rude_audition_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MonitorSection::cancel_audition), false);
	UI::instance()->set_tip (rude_audition_button, _("When active, auditioning is active.\nClick to stop the audition"));

	/* SIP, AFL, PFL radio */

	solo_in_place_button.set_name ("monitor section solo model");
	afl_button.set_name ("monitor section solo model");
	pfl_button.set_name ("monitor section solo model");

	solo_in_place_button.set_led_left (true);
	afl_button.set_led_left (true);
	pfl_button.set_led_left (true);

	solo_in_place_button.show ();
	afl_button.show ();
	pfl_button.show ();

	act = ActionManager::get_action (X_("Solo"), X_("solo-use-in-place"));
	set_tooltip (solo_in_place_button, _("Solo controls affect solo-in-place"));
	if (act) {
		solo_in_place_button.set_related_action (act);
	}

	act = ActionManager::get_action (X_("Solo"), X_("solo-use-afl"));
	set_tooltip (afl_button, _("Solo controls toggle after-fader-listen"));
	if (act) {
		afl_button.set_related_action (act);
	}

	act = ActionManager::get_action (X_("Solo"), X_("solo-use-pfl"));
	set_tooltip (pfl_button, _("Solo controls toggle pre-fader-listen"));
	if (act) {
		pfl_button.set_related_action (act);
	}

	/* Solo option buttons */
	exclusive_solo_button.set_text (_("Excl. Solo"));
	exclusive_solo_button.set_name (X_("monitor section solo option"));
	set_tooltip (&exclusive_solo_button, _("Exclusive solo means that only 1 solo is active at a time"));

	act = ActionManager::get_action (X_("Solo"), X_("toggle-exclusive-solo"));
	if (act) {
		exclusive_solo_button.set_related_action (act);
	}

	solo_mute_override_button.set_text (_("Solo » Mute"));
	solo_mute_override_button.set_name (X_("monitor section solo option"));
	set_tooltip (&solo_mute_override_button, _("If enabled, solo will override mute\n(a soloed & muted track or bus will be audible)"));

	solo_mute_override_button.set_related_action (ActionManager::get_action (X_("Solo"), X_("toggle-mute-overrides-solo")));

	/* Processor Box hide/shos */
	toggle_processorbox_button.set_text (_("Processors"));
	toggle_processorbox_button.set_name (X_("monitor section processors toggle"));
	set_tooltip (&toggle_processorbox_button, _("Allow one to add monitor effect processors"));

	proctoggle = ActionManager::get_toggle_action (X_("Monitor"), X_("toggle-monitor-processor-box"));
	toggle_processorbox_button.set_related_action (proctoggle);

	/* Knobs */
	Label* solo_boost_label;
	Label* solo_cut_label;
	Label* dim_label;

	/* Solo Boost Knob */

	solo_boost_control = new ArdourKnob ();
	solo_boost_control->set_name("monitor section knob");
	solo_boost_control->set_size_request (PX_SCALE(36), PX_SCALE(36));
	set_tooltip (*solo_boost_control, _("Gain increase for soloed signals (0dB is normal)"));

	solo_boost_display = new ArdourDisplay ();
	set_tooltip (*solo_boost_display, _("Gain increase for soloed signals (0dB is normal)"));
	solo_boost_display->set_name("monitor section button");
	solo_boost_display->set_size_request (PX_SCALE(68), PX_SCALE(20));
	solo_boost_display->add_controllable_preset(_("0 dB"), 0.0);
	solo_boost_display->add_controllable_preset(_("3 dB"), 3.0);
	solo_boost_display->add_controllable_preset(_("6 dB"), 6.0);
	solo_boost_display->add_controllable_preset(_("10 dB"), 10.0);

	solo_boost_label = manage (new Label (_("Solo Boost")));

	/* Solo (SiP) cut */

	solo_cut_control = new ArdourKnob ();
	solo_cut_control->set_name ("monitor section knob");
	solo_cut_control->set_size_request (PX_SCALE(36), PX_SCALE(36));
	set_tooltip (*solo_cut_control, _("Gain reduction non-soloed signals\nA value above -inf dB causes \"solo-in-front\""));

	solo_cut_display = new ArdourDisplay ();
	set_tooltip (*solo_cut_display, _("Gain reduction non-soloed signals\nA value above -inf dB causes \"solo-in-front\""));
	solo_cut_display->set_name("monitor section button");
	solo_cut_display->set_size_request (PX_SCALE(68), PX_SCALE(20));
	solo_cut_display->add_controllable_preset(_("0 dB"), 0.0);
	solo_cut_display->add_controllable_preset(_("-6 dB"), -6.0);
	solo_cut_display->add_controllable_preset(_("-12 dB"), -12.0);
	solo_cut_display->add_controllable_preset(_("-20 dB"), -20.0);
	solo_cut_display->add_controllable_preset(_("OFF"), -1200.0);

	solo_cut_label = manage (new Label (_("SiP Cut")));

	/* Dim */

	dim_control = new ArdourKnob (ArdourKnob::default_elements, ArdourKnob::Detent);
	dim_control->set_name ("monitor section knob");
	dim_control->set_size_request (PX_SCALE(36), PX_SCALE(36));
	set_tooltip (*dim_control, _("Gain reduction to use when dimming monitor outputs"));

	dim_display = new ArdourDisplay ();
	set_tooltip (*dim_display, _("Gain reduction to use when dimming monitor outputs"));
	dim_display->set_name ("monitor section button");
	dim_display->set_size_request (PX_SCALE(68), PX_SCALE(20));
	dim_display->add_controllable_preset(_("0 dB"), 0.0);
	dim_display->add_controllable_preset(_("-3 dB"), -3.0);
	dim_display->add_controllable_preset(_("-6 dB"), -6.0);
	dim_display->add_controllable_preset(_("-12 dB"), -12.0);
	dim_display->add_controllable_preset(_("-20 dB"), -20.0);

	dim_label = manage (new Label (_("Dim")));

	// mute button
	cut_all_button.set_text (_("Mute"));
	cut_all_button.set_name ("mute button");
	cut_all_button.set_size_request (-1, PX_SCALE(30));
	cut_all_button.show ();

	act = ActionManager::get_action (X_("Monitor Section"), X_("monitor-cut-all"));
	if (act) {
		cut_all_button.set_related_action (act);
	}

	// dim button
	dim_all_button.set_text (_("Dim"));
	dim_all_button.set_name ("monitor section dim");
	dim_all_button.set_size_request (-1, PX_SCALE(25));
	act = ActionManager::get_action (X_("Monitor Section"), X_("monitor-dim-all"));
	if (act) {
		dim_all_button.set_related_action (act);
	}

	// mono button
	mono_button.set_text (_("Mono"));
	mono_button.set_name ("monitor section mono");
	mono_button.set_size_request (-1, PX_SCALE(25));
	act = ActionManager::get_action (X_("Monitor Section"), X_("monitor-mono"));
	if (act) {
		mono_button.set_related_action (act);
	}

	/* Gain */

	gain_control = new ArdourKnob (ArdourKnob::default_elements, ArdourKnob::Detent);
	gain_control->set_name("monitor section knob");
	gain_control->set_size_request (PX_SCALE(60), PX_SCALE(60));

	gain_display = new ArdourDisplay ();
	gain_display->set_name("monitor section button");
	gain_display->set_size_request (PX_SCALE(68), PX_SCALE(20));
	gain_display->add_controllable_preset(_("0 dB"), 0.0);
	gain_display->add_controllable_preset(_("-3 dB"), -3.0);
	gain_display->add_controllable_preset(_("-6 dB"), -6.0);
	gain_display->add_controllable_preset(_("-12 dB"), -12.0);
	gain_display->add_controllable_preset(_("-20 dB"), -20.0);
	gain_display->add_controllable_preset(_("-30 dB"), -30.0);

	Label* output_label = manage (new Label (_("Output")));
	output_label->set_name (X_("MonitorSectionLabel"));

	output_button = new ArdourButton ();
	output_button->set_text (_("Output"));
	output_button->set_name (X_("monitor section button"));
	output_button->set_text_ellipsize (Pango::ELLIPSIZE_MIDDLE);
	output_button->set_layout_ellipsize_width (PX_SCALE(128) * PANGO_SCALE);

	channel_table_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	channel_table_scroller.set_size_request (-1, PX_SCALE(150));
	channel_table_scroller.set_shadow_type (Gtk::SHADOW_NONE);
	channel_table_scroller.show ();
	channel_table_scroller.add (channel_table_viewport);

	channel_size_group->add_widget (channel_table_header);
	channel_table_header.resize (1, 5);

	Label* l1 = manage (new Label (X_("  ")));
	l1->set_name (X_("MonitorSectionLabel"));
	channel_table_header.attach (*l1, 0, 1, 0, 1, EXPAND|FILL);

	l1 = manage (new Label (_("Mute")));
	l1->set_name (X_("MonitorSectionLabel"));
	channel_table_header.attach (*l1, 1, 2, 0, 1, EXPAND|FILL);

	l1 = manage (new Label (_("Dim")));
	l1->set_name (X_("MonitorSectionLabel"));
	channel_table_header.attach (*l1, 2, 3, 0, 1, EXPAND|FILL);

	l1 = manage (new Label (_("Solo")));
	l1->set_name (X_("MonitorSectionLabel"));
	channel_table_header.attach (*l1, 3, 4, 0, 1, EXPAND|FILL);

	l1 = manage (new Label (_("Inv")));
	l1->set_name (X_("MonitorSectionLabel"));
	channel_table_header.attach (*l1, 4, 5, 0, 1, EXPAND|FILL);

	channel_table_header.show ();


	/****************************************************************************
	 * LAYOUT  top to bottom
	 */

	Gtk::Label *top_spacer = manage (new Gtk::Label);

	// solo, iso information
	HBox* rude_box = manage (new HBox);
	rude_box->set_spacing (PX_SCALE(4));
	rude_box->set_homogeneous (true);
	rude_box->pack_start (rude_solo_button, true, true);
	rude_box->pack_start (rude_iso_button, true, true);

	// solo options (right align)
	HBox* tbx1 = manage (new HBox);
	tbx1->pack_end (exclusive_solo_button, false, false);

	HBox* tbx2 = manage (new HBox);
	tbx2->pack_end (solo_mute_override_button, false, false);

	HBox* tbx3 = manage (new HBox);
	tbx3->pack_end (toggle_processorbox_button, false, false);

	HBox* tbx0 = manage (new HBox); // space

	// combined solo mode (Sip, AFL, PFL) & solo options
	Table *solo_tbl = manage (new Table);
	solo_tbl->attach (solo_in_place_button,   0, 1, 0, 1, EXPAND|FILL, SHRINK, 0, 2);
	solo_tbl->attach (pfl_button,             0, 1, 1, 2, EXPAND|FILL, SHRINK, 0, 2);
	solo_tbl->attach (afl_button,             0, 1, 2, 3, EXPAND|FILL, SHRINK, 0, 2);
	solo_tbl->attach (*tbx0,                  1, 2, 0, 3, EXPAND|FILL, SHRINK, 2, 2);
	solo_tbl->attach (*tbx1,                  2, 3, 0, 1, EXPAND|FILL, SHRINK, 0, 2);
	solo_tbl->attach (*tbx2,                  2, 3, 1, 2, EXPAND|FILL, SHRINK, 0, 2);
	solo_tbl->attach (*tbx3,                  2, 3, 2, 3, EXPAND|FILL, SHRINK, 0, 2);

	// boost, cut, dim  volume control
	Table *level_tbl = manage (new Table);
	level_tbl->attach (*solo_boost_label,    0, 2, 0, 1, EXPAND|FILL, SHRINK, 1, 2);
	level_tbl->attach (*solo_boost_control,  0, 2, 1, 2, EXPAND|FILL, SHRINK, 1, 2);
	level_tbl->attach (*solo_boost_display,  0, 2, 2, 3, EXPAND     , SHRINK, 1, 2);

	level_tbl->attach (*solo_cut_label,      2, 4, 0, 1, EXPAND|FILL, SHRINK, 1, 2);
	level_tbl->attach (*solo_cut_control,    2, 4, 1, 2, EXPAND|FILL, SHRINK, 1, 2);
	level_tbl->attach (*solo_cut_display,    2, 4, 2, 3, EXPAND     , SHRINK, 1, 2);

	level_tbl->attach (*dim_label,           1, 3, 3, 4, EXPAND|FILL, SHRINK, 1, 2);
	level_tbl->attach (*dim_control,         1, 3, 4, 5, EXPAND|FILL, SHRINK, 1, 2);
	level_tbl->attach (*dim_display,         1, 3, 5, 6, EXPAND     , SHRINK, 1, 2);

	// mono, dim
	HBox* mono_dim_box = manage (new HBox);
	mono_dim_box->set_spacing (PX_SCALE(4));
	mono_dim_box->set_homogeneous (true);
	mono_dim_box->pack_start (mono_button, true, true);
	mono_dim_box->pack_end (dim_all_button, true, true);

	// master gain
	Label* spin_label = manage (new Label (_("Monitor")));
	VBox* spin_packer = manage (new VBox);
	spin_packer->set_spacing (PX_SCALE(2));
	spin_packer->pack_start (*spin_label, false, false);
	spin_packer->pack_start (*gain_control, false, false);
	spin_packer->pack_start (*gain_display, false, false);

	master_packer.pack_start (*spin_packer, true, false);

	// combined gain section (channels, mute, dim)
	VBox* lower_packer = manage (new VBox);
	lower_packer->pack_start (channel_table_header, false, false, PX_SCALE(0));
	lower_packer->pack_start (channel_table_packer, false, false, PX_SCALE(8));
	lower_packer->pack_start (*mono_dim_box,        false, false, PX_SCALE(2));
	lower_packer->pack_start (cut_all_button,       false, false, PX_SCALE(2));

	// calc height of mixer scrollbar
	int scrollbar_height = 0;
	{
		Gtk::Window window (WINDOW_TOPLEVEL);
		HScrollbar scrollbar;
		window.add (scrollbar);
		scrollbar.set_name ("MixerWindow");
		scrollbar.ensure_style();
		Gtk::Requisition requisition(scrollbar.size_request ());
		scrollbar_height = requisition.height;
		scrollbar_height += 3; // track_display_frame border/shadow
	}

	// output port select
	VBox* out_packer = manage (new VBox);
	out_packer->set_spacing (PX_SCALE(2));
	out_packer->pack_start (*output_label, false, false);
	out_packer->pack_start (*output_button, false, false);

	/****************************************************************************
	 * TOP LEVEL LAYOUT
	 */
	vpacker.set_border_width (PX_SCALE(3));
	vpacker.pack_start (*top_spacer,          false, false, PX_SCALE(3));
	vpacker.pack_start (*rude_box,            false, false, PX_SCALE(3));
	vpacker.pack_start (rude_audition_button, false, false, 0);
	vpacker.pack_start (*solo_tbl,            false, false, PX_SCALE(8));
	vpacker.pack_start (*insert_box,          true,  true,  PX_SCALE(8));
	vpacker.pack_start (*level_tbl,           false, false, PX_SCALE(8));
	vpacker.pack_start (*lower_packer,        false, false, PX_SCALE(8));
	vpacker.pack_start (master_packer,        false, false, PX_SCALE(10));
	vpacker.pack_end   (*out_packer,          false, false,
#ifdef MIXBUS
			scrollbar_height /* no outer frame */
#else
			scrollbar_height + 2 /* frame borders */
#endif
			);

	hpacker.set_spacing (0);
	hpacker.pack_start (vpacker, true, true);

	add (hpacker);

	gain_control->show_all ();
	gain_display->show_all ();
	dim_control->show_all ();
	dim_display->show_all();
	solo_boost_control->show_all ();
	solo_boost_display->show_all();

	mono_dim_box->show ();
	spin_packer->show ();
	master_packer.show ();

	rude_box->show();
	solo_tbl->show_all();
	level_tbl->show();
	lower_packer->show ();
	out_packer->show ();

	vpacker.show ();
	hpacker.show ();

	map_state ();

	output_button->signal_button_press_event().connect (sigc::mem_fun(*this, &MonitorSection::output_press), false);
	output_button->signal_button_release_event().connect (sigc::mem_fun(*this, &MonitorSection::output_release), false);

	signal_enter_notify_event().connect (sigc::mem_fun (*this, &MonitorSection::enter_handler));
	signal_leave_notify_event().connect (sigc::mem_fun (*this, &MonitorSection::leave_handler));
	set_flags (CAN_FOCUS);

	_tearoff = new TearOff (*this);

	if (!UIConfiguration::instance().get_floating_monitor_section()) {
		/* if torn off, make this a normal window
		 * (default is WINDOW_TYPE_HINT_UTILITY in libs/gtkmm2ext/tearoff.cc)
		 */
		_tearoff->tearoff_window().set_type_hint (Gdk::WINDOW_TYPE_HINT_NORMAL);
	}
	_tearoff->tearoff_window().set_title (X_("Monitor"));
	_tearoff->tearoff_window().signal_key_press_event().connect (sigc::bind (sigc::ptr_fun (relay_key_press), (Gtk::Window*) &_tearoff->tearoff_window()), false);

	update_output_display ();
	update_processor_box ();
	_ui_initialized = true;

	/* catch changes that affect us */
	AudioEngine::instance()->PortConnectedOrDisconnected.connect (
		*this, invalidator (*this), boost::bind (&MonitorSection::port_connected_or_disconnected, this, _1, _3), gui_context ()
		);
	AudioEngine::instance()->PortPrettyNameChanged.connect (
		*this, invalidator (*this), boost::bind (&MonitorSection::port_pretty_name_changed, this, _1), gui_context ()
		);
	Config->ParameterChanged.connect (config_connection, invalidator (*this), boost::bind (&MonitorSection::parameter_changed, this, _1), gui_context());
}

MonitorSection::~MonitorSection ()
{
	for (ChannelButtons::iterator i = _channel_buttons.begin(); i != _channel_buttons.end(); ++i) {
		delete *i;
	}

	_channel_buttons.clear ();
	route_connections.drop_connections ();

	delete insert_box; insert_box = 0;
	delete output_button; output_button = 0;
	delete gain_control; gain_control = 0;
	delete gain_display; gain_display = 0;
	delete dim_control; dim_control = 0;
	delete dim_display; dim_display = 0;
	delete solo_boost_control; solo_boost_control = 0;
	delete solo_boost_display; solo_boost_display = 0;
	delete solo_cut_control; solo_cut_control = 0;
	delete solo_cut_display; solo_cut_display = 0;
	delete _tearoff; _tearoff = 0;
	delete _output_selector; _output_selector = 0;
	delete channel_table; channel_table = 0;
}

bool
MonitorSection::enter_handler (GdkEventCrossing* ev)
{
	grab_focus ();
	return false;
}

bool
MonitorSection::leave_handler (GdkEventCrossing* ev)
{
	switch (ev->detail) {
	case GDK_NOTIFY_INFERIOR:
		return false;
	default:
		break;
	}

	/* cancel focus if we're not torn off. With X11 WM's that do
	 * focus-follows-mouse, focus will be taken from us anyway.
	 */

	Widget* top = get_toplevel();

	if (top->is_toplevel() && top != &_tearoff->tearoff_window()) {
		Window* win = dynamic_cast<Window*> (top);
		gtk_window_set_focus (win->gobj(), 0);
	}

	return false;
}

void
MonitorSection::update_processor_box ()
{
	bool show_processor_box = proctoggle->get_active ();

	if (count_processors () > 0 && !show_processor_box) {
		toggle_processorbox_button.set_name (X_("monitor section processors present"));
	} else {
		toggle_processorbox_button.set_name (X_("monitor section processors toggle"));
	}

	if (insert_box->is_visible() == show_processor_box) {
		return;
	}

	if (show_processor_box) {
		if (master_packer.get_parent()) {
			master_packer.get_parent()->remove (master_packer);
		}
		insert_box->show();
		vpacker.pack_start (master_packer,        false, false, PX_SCALE(10));
	} else {
		if (master_packer.get_parent()) {
			master_packer.get_parent()->remove (master_packer);
		}
		insert_box->hide();
		vpacker.pack_start (master_packer,        true,  false, PX_SCALE(10));
	}
}

void
MonitorSection::set_session (Session* s)
{
	RouteUI::set_session (s);
	insert_box->set_session (_session);

	Glib::RefPtr<ActionGroup> global_monitor_actions = ActionManager::get_action_group (X_("Monitor Section"));

	if (_session) {

		/* These are not actually dependent on the Session, but they
		 * need to be set after construction, not during, and
		 * this is as good a place as any.
		 */

		ActionManager::get_toggle_action (X_("Solo"), X_("toggle-exclusive-solo"))->set_active (Config->get_exclusive_solo());
		ActionManager::get_toggle_action (X_("Solo"), X_("toggle-mute-overrides-solo"))->set_active (Config->get_solo_mute_override());

		_route = _session->monitor_out ();

		if (_route) {
			/* session with monitor section */
			_monitor = _route->monitor_control ();
			assign_controllables ();
			_route->output()->changed.connect (route_connections, invalidator (*this),
			                                   boost::bind (&MonitorSection::update_output_display, this),
			                                   gui_context());
			insert_box->set_route (_route);
			_route->processors_changed.connect (route_connections, invalidator (*this), boost::bind (&MonitorSection::processors_changed, this, _1), gui_context());
			_route->output()->PortCountChanged.connect (route_connections, invalidator (*this), boost::bind (&MonitorSection::populate_buttons, this), gui_context());
			_route->DropReferences.connect (route_connections, invalidator (*this), boost::bind (&MonitorSection::drop_route, this), gui_context());

			if (_ui_initialized) {
				update_processor_box ();
				update_output_display ();
			}

			SYNCHRONIZE_TOGGLE_ACTION (ActionManager::get_toggle_action (X_("Monitor"), "UseMonitorSection"), true);
			ActionManager::set_sensitive (global_monitor_actions, true);
			ActionManager::set_sensitive (monitor_actions, true);
			ActionManager::set_sensitive (solo_actions, true);

		} else {
			/* session with no monitor section */
			route_connections.drop_connections();
			_monitor.reset ();
			_route.reset ();
			delete _output_selector;
			_output_selector = 0;

			SYNCHRONIZE_TOGGLE_ACTION (ActionManager::get_toggle_action (X_("Monitor"), "UseMonitorSection"), false);
			ActionManager::set_sensitive (global_monitor_actions, false);
			ActionManager::set_sensitive (monitor_actions, false);
			ActionManager::set_sensitive (solo_actions, true);
			/* this action needs to always be true in this, so that we can turn it back on */
			ActionManager::get_toggle_action (X_("Monitor"), X_("UseMonitorSection"))->set_sensitive (true);
		}

		populate_buttons ();
		map_state ();

	} else {

		/* no session */

		if (_route) {
			drop_route ();
			unassign_controllables ();
		}

		ActionManager::set_sensitive (monitor_actions, false);
		ActionManager::set_sensitive (solo_actions, false);
		ActionManager::set_sensitive (global_monitor_actions, false);
	}
}

void
MonitorSection::drop_route ()
{
	route_connections.drop_connections();
	_monitor.reset ();
	_route.reset ();
	unassign_controllables ();
	rude_iso_button.unset_active_state ();
	rude_solo_button.unset_active_state ();
	delete _output_selector;
	_output_selector = 0;
}

MonitorSection::ChannelButtonSet::ChannelButtonSet ()
{
	cut.set_name (X_("mute button"));
	dim.set_name (X_("monitor section dim"));
	solo.set_name (X_("solo button"));
	invert.set_name (X_("invert button"));

	cut.unset_flags (Gtk::CAN_FOCUS);
	dim.unset_flags (Gtk::CAN_FOCUS);
	solo.unset_flags (Gtk::CAN_FOCUS);
	invert.unset_flags (Gtk::CAN_FOCUS);
}

void
MonitorSection::populate_buttons ()
{
	if (!_monitor) {
		return;
	}

	if (channel_table) {
		channel_size_group->remove_widget (*channel_table);
		delete channel_table;
	}

	channel_table = new Gtk::Table();

	channel_table->set_col_spacings (6);
	channel_table->set_row_spacings (6);
	channel_table->set_homogeneous (true);

	channel_size_group->add_widget (*channel_table);
	channel_table->show ();
	table_hpacker.pack_start (*channel_table, true, true);

	for (ChannelButtons::iterator i = _channel_buttons.begin(); i != _channel_buttons.end(); ++i) {
		delete *i;
	}
	_channel_buttons.clear ();

	Glib::RefPtr<Action> act;
	uint32_t nchans = _monitor->output_streams().n_audio();

	channel_table->resize (nchans, 5);

	const uint32_t row_offset = 0;

	for (uint32_t i = 0; i < nchans; ++i) {

		string l;
		char buf[64];

		if (nchans == 2) {
			if (i == 0) {
				l = "L";
			} else {
				l = "R";
			}
		} else {
			char buf[32];
			snprintf (buf, sizeof (buf), "%d", i+1);
			l = buf;
		}

		Label* label = manage (new Label (l));
		channel_table->attach (*label, 0, 1, i+row_offset, i+row_offset+1, EXPAND|FILL);

		ChannelButtonSet* cbs = new ChannelButtonSet;

		_channel_buttons.push_back (cbs);

		channel_table->attach (cbs->cut, 1, 2, i+row_offset, i+row_offset+1, EXPAND|FILL);
		channel_table->attach (cbs->dim, 2, 3, i+row_offset, i+row_offset+1, EXPAND|FILL);
		channel_table->attach (cbs->solo, 3, 4, i+row_offset, i+row_offset+1, EXPAND|FILL);
		channel_table->attach (cbs->invert, 4, 5, i+row_offset, i+row_offset+1, EXPAND|FILL);

		snprintf (buf, sizeof (buf), "monitor-cut-%u", i);
		act = ActionManager::get_action (X_("Monitor"), buf);
		if (act) {
			cbs->cut.set_related_action (act);
		}

		snprintf (buf, sizeof (buf), "monitor-dim-%u", i);
		act = ActionManager::get_action (X_("Monitor"), buf);
		if (act) {
			cbs->dim.set_related_action (act);
		}

		snprintf (buf, sizeof (buf), "monitor-solo-%u", i);
		act = ActionManager::get_action (X_("Monitor"), buf);
		if (act) {
			cbs->solo.set_related_action (act);
		}

		snprintf (buf, sizeof (buf), "monitor-invert-%u", i);
		act = ActionManager::get_action (X_("Monitor"), buf);
		if (act) {
			cbs->invert.set_related_action (act);
		}
	}

	channel_table->show_all ();

	if (channel_table_scroller.get_parent()) {
		/* scroller is packed, so remove it */
		channel_table_packer.remove (channel_table_scroller);
	}

	if (table_hpacker.get_parent () == &channel_table_packer) {
		/* this occurs when the table hpacker is directly
			 packed, so remove it.
			 */
		channel_table_packer.remove (table_hpacker);
	} else if (table_hpacker.get_parent()) {
		channel_table_viewport.remove ();
	}

	if (nchans > 7) {
		/* put the table into a scrolled window, and then put
		 * that into the channel vpacker, after the table header
		 */
		channel_table_viewport.add (table_hpacker);
		channel_table_packer.pack_start (channel_table_scroller, true, true);
		channel_table_viewport.show ();
		channel_table_scroller.show ();

	} else {
		/* just put the channel table itself into the channel
		 * vpacker, after the table header
		 */
		channel_table_packer.pack_start (table_hpacker, true, true);
		channel_table_scroller.hide ();
	}
	table_hpacker.show ();
	channel_table->show ();
}

void
MonitorSection::toggle_exclusive_solo ()
{
	if (!_monitor) {
		return;
	}

	Config->set_exclusive_solo (ActionManager::get_toggle_action (X_("Solo"), "toggle-exclusive-solo")->get_active());
}

void
MonitorSection::toggle_mute_overrides_solo ()
{
	if (!_monitor) {
		return;
	}

	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Solo"), "toggle-mute-overrides-solo");
	Config->set_solo_mute_override (tact->get_active());
}

void
MonitorSection::dim_all ()
{
	if (!_monitor) {
		return;
	}

	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Monitor Section"), "monitor-dim-all");
	_monitor->set_dim_all (tact->get_active());
}

void
MonitorSection::cut_all ()
{
	if (!_monitor) {
		return;
	}

	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Monitor Section"), "monitor-cut-all");
	_monitor->set_cut_all (tact->get_active());
}

void
MonitorSection::mono ()
{
	if (!_monitor) {
		return;
	}

	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Monitor Section"), "monitor-mono");
	_monitor->set_mono (tact->get_active());
}

void
MonitorSection::cut_channel (uint32_t chn)
{
	if (!_monitor) {
		return;
	}

	char buf[64];
	snprintf (buf, sizeof (buf), "monitor-cut-%u", chn);

	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Monitor"), buf);
	_monitor->set_cut (chn, tact->get_active());
}

void
MonitorSection::dim_channel (uint32_t chn)
{
	if (!_monitor) {
		return;
	}

	char buf[64];
	snprintf (buf, sizeof (buf), "monitor-dim-%u", chn);

	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Monitor"), buf);
	_monitor->set_dim (chn, tact->get_active());
}

void
MonitorSection::solo_channel (uint32_t chn)
{
	if (!_monitor) {
		return;
	}

	char buf[64];
	snprintf (buf, sizeof (buf), "monitor-solo-%u", chn);

	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Monitor"), buf);
	_monitor->set_solo (chn, tact->get_active());

}

void
MonitorSection::invert_channel (uint32_t chn)
{
	if (!_monitor) {
		return;
	}

	char buf[64];
	snprintf (buf, sizeof (buf), "monitor-invert-%u", chn);

	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Monitor"), buf);
	_monitor->set_polarity (chn, tact->get_active());
}

void
MonitorSection::register_actions ()
{
	string action_name;
	string action_descr;
	Glib::RefPtr<Action> act;

	/* ...will get sensitized if a mon-session is added */

	monitor_actions = ActionManager::create_action_group (bindings, X_("Monitor"));
	solo_actions = ActionManager::create_action_group (bindings, X_("Monitor"));


	ActionManager::register_toggle_action (monitor_actions, X_("UseMonitorSection"), _("Use Monitor Section"), sigc::mem_fun(*this, &MonitorSection::toggle_use_monitor_section));

	/* these are global monitor actions that invoke MonitorSectioncode. Do
	 * not create local versions (i.e. as part of "monitor_actions")
	 * because then we can end up with two different bindings (one global,
	 * one local to the monitor section) for the same action.
	 */

	Glib::RefPtr<ActionGroup> global_monitor_actions = ActionManager::get_action_group (X_("Monitor Section"));

	ActionManager::register_toggle_action (global_monitor_actions, "monitor-mono", _("Mono"), sigc::mem_fun (*this, &MonitorSection::mono));
	ActionManager::register_toggle_action (global_monitor_actions, "monitor-cut-all", _("Mute"), sigc::mem_fun (*this, &MonitorSection::cut_all));
	ActionManager::register_toggle_action (global_monitor_actions, "monitor-dim-all", _("Dim"), sigc::mem_fun (*this, &MonitorSection::dim_all));

	ActionManager::register_toggle_action (monitor_actions, "toggle-monitor-processor-box", _("Toggle Monitor Section Processor Box"),
	                                       sigc::mem_fun (*this, &MonitorSection::update_processor_box));


	for (uint32_t chn = 0; chn < 16; ++chn) {

		action_name = string_compose (X_("monitor-cut-%1"), chn);
		action_descr = string_compose (_("Cut monitor channel %1"), chn);
		ActionManager::register_toggle_action (monitor_actions, action_name.c_str(), action_descr.c_str(),
		                                       sigc::bind (sigc::mem_fun (*this, &MonitorSection::cut_channel), chn));

		action_name = string_compose (X_("monitor-dim-%1"), chn);
		action_descr = string_compose (_("Dim monitor channel %1"), chn);
		ActionManager::register_toggle_action (monitor_actions, action_name.c_str(), action_descr.c_str(),
		                                       sigc::bind (sigc::mem_fun (*this, &MonitorSection::dim_channel), chn));

		action_name = string_compose (X_("monitor-solo-%1"), chn);
		action_descr = string_compose (_("Solo monitor channel %1"), chn);
		ActionManager::register_toggle_action (monitor_actions, action_name.c_str(), action_descr.c_str(),
		                                       sigc::bind (sigc::mem_fun (*this, &MonitorSection::solo_channel), chn));

		action_name = string_compose (X_("monitor-invert-%1"), chn);
		action_descr = string_compose (_("Invert monitor channel %1"), chn);
		ActionManager::register_toggle_action (monitor_actions, action_name.c_str(), action_descr.c_str(),
		                                       sigc::bind (sigc::mem_fun (*this, &MonitorSection::invert_channel), chn));

	}

	solo_actions = ActionManager::create_action_group (bindings, X_("Solo"));
	RadioAction::Group solo_group;

	ActionManager::register_radio_action (solo_actions, solo_group, "solo-use-in-place", _("In-place solo"),
	                                      sigc::mem_fun (*this, &MonitorSection::solo_use_in_place));
	ActionManager::register_radio_action (solo_actions, solo_group, "solo-use-afl", _("After Fade Listen (AFL) solo"),
	                                      sigc::mem_fun (*this, &MonitorSection::solo_use_afl));
	ActionManager::register_radio_action (solo_actions, solo_group, "solo-use-pfl", _("Pre Fade Listen (PFL) solo"),
	                                      sigc::mem_fun (*this, &MonitorSection::solo_use_pfl));

	ActionManager::register_toggle_action (solo_actions, "toggle-exclusive-solo", _("Toggle exclusive solo mode"),
	                                       sigc::mem_fun (*this, &MonitorSection::toggle_exclusive_solo));
	ActionManager::register_toggle_action (solo_actions, "toggle-mute-overrides-solo", _("Toggle mute overrides solo mode"),
	                                       sigc::mem_fun (*this, &MonitorSection::toggle_mute_overrides_solo));
}

void
MonitorSection::solo_use_in_place ()
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
		 once for the item that became inactive and once for the one that became
		 active.
		 */

	Glib::RefPtr<RadioAction> ract = ActionManager::get_radio_action (X_("Solo"), X_("solo-use-in-place"));
	if (!ract->get_active ()) {
		/* We are turning SiP off, which means that AFL or PFL will be turned on
		   shortly; don't update the solo model in the mean time, as if the currently
		   configured listen position is not the one that is about to be turned on,
		   things will go wrong.
		*/
		_inhibit_solo_model_update = true;
	}
	Config->set_solo_control_is_listen_control (!ract->get_active());
	_inhibit_solo_model_update = false;
}

void
MonitorSection::solo_use_afl ()
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
		 once for the item that became inactive and once for the one that became
		 active.
		 */

	Glib::RefPtr<RadioAction> ract = ActionManager::get_radio_action (X_("Solo"), X_("solo-use-afl"));
	if (ract->get_active()) {
		Config->set_solo_control_is_listen_control (true);
		Config->set_listen_position (AfterFaderListen);
	}
}

void
MonitorSection::solo_use_pfl ()
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	Glib::RefPtr<RadioAction> ract = ActionManager::get_radio_action (X_("Solo"), X_("solo-use-pfl"));
	if (ract->get_active()) {
		Config->set_solo_control_is_listen_control (true);
		Config->set_listen_position (PreFaderListen);
	}
}

void
MonitorSection::update_solo_model ()
{
	if (_inhibit_solo_model_update) {
		return;
	}

	const char* action_name = 0;
	Glib::RefPtr<RadioAction> ract;

	if (Config->get_solo_control_is_listen_control()) {
		switch (Config->get_listen_position()) {
			case AfterFaderListen:
				action_name = X_("solo-use-afl");
				break;
			case PreFaderListen:
				action_name = X_("solo-use-pfl");
				break;
		}
	} else {
		action_name = X_("solo-use-in-place");
	}

	ract = ActionManager::get_radio_action (X_("Solo"), action_name);

	/* because these are radio buttons, one of them will be
	   active no matter what. to trigger a change in the
	   action so that the view picks it up, toggle it.
	*/

	if (ract->get_active()) {
		ract->set_active (false);
	}

	ract->set_active (true);
}

void
MonitorSection::map_state ()
{
	if (!_route || !_monitor) {
		return;
	}

	update_solo_model ();

	Glib::RefPtr<Action> act;
	Glib::RefPtr<ToggleAction> tact;

	tact = ActionManager::get_toggle_action (X_("Monitor Section"), "monitor-cut-all");
	tact->set_active (_monitor->cut_all());

	tact = ActionManager::get_toggle_action (X_("Monitor Section"), "monitor-dim-all");
	tact->set_active (_monitor->dim_all());

	tact = ActionManager::get_toggle_action (X_("Monitor Section"), "monitor-mono");
	tact->set_active (_monitor->mono());

	uint32_t nchans = _monitor->output_streams().n_audio();

	assert (nchans == _channel_buttons.size ());

	for (uint32_t n = 0; n < nchans; ++n) {

		char action_name[32];

		snprintf (action_name, sizeof (action_name), "monitor-cut-%u", n);
		tact = ActionManager::get_toggle_action (X_("Monitor"), action_name);
		tact->set_active (_monitor->cut (n));

		snprintf (action_name, sizeof (action_name), "monitor-dim-%u", n);
		tact = ActionManager::get_toggle_action (X_("Monitor"), action_name);
		tact->set_active (_monitor->dimmed (n));

		snprintf (action_name, sizeof (action_name), "monitor-solo-%u", n);
		tact = ActionManager::get_toggle_action (X_("Monitor"), action_name);
		tact->set_active (_monitor->soloed (n));

		snprintf (action_name, sizeof (action_name), "monitor-invert-%u", n);
		tact = ActionManager::get_toggle_action (X_("Monitor"), action_name);
		tact->set_active (_monitor->inverted (n));
	}
}

void
MonitorSection::do_blink (bool onoff)
{
	if (!UIConfiguration::instance().get_blink_alert_indicators ()) {
		onoff = true;
	}

	solo_blink (onoff);
	audition_blink (onoff);
}

void
MonitorSection::audition_blink (bool onoff)
{
	if (_session == 0) {
		return;
	}

	if (_session->is_auditioning()) {
		rude_audition_button.set_active (onoff);
	} else {
		rude_audition_button.set_active (false);
	}
}

void
MonitorSection::solo_blink (bool onoff)
{
	if (_session == 0) {
		return;
	}

	if (_session->soloing() || _session->listening()) {
		rude_solo_button.set_active (onoff);

		if (_session->soloing()) {
			if (_session->solo_isolated()) {
				rude_iso_button.set_active (onoff);
			} else {
				rude_iso_button.set_active (false);
			}
		}

	} else {
		rude_solo_button.set_active (false);
		rude_iso_button.set_active (false);
	}
}

bool
MonitorSection::cancel_isolate (GdkEventButton*)
{
	if (_session) {
		boost::shared_ptr<RouteList> rl (_session->get_routes ());
		_session->set_controls (route_list_to_control_list (rl, &Stripable::solo_isolate_control), 0.0, Controllable::NoGroup);
	}

	return true;
}

bool
MonitorSection::cancel_audition (GdkEventButton*)
{
	if (_session) {
		_session->cancel_audition();
	}
	return true;
}

void
MonitorSection::parameter_changed (std::string name)
{
	if (name == "solo-control-is-listen-control") {
		update_solo_model ();
	} else if (name == "listen-position") {
		update_solo_model ();
	} else if (name == "solo-mute-override") {
		SYNCHRONIZE_TOGGLE_ACTION (ActionManager::get_toggle_action (X_("Solo"), "toggle-mute-overrides-solo"), Config->get_solo_mute_override ());
	} else if (name == "exclusive-solo") {
		SYNCHRONIZE_TOGGLE_ACTION (ActionManager::get_toggle_action (X_("Solo"), "toggle-exclusive-solo"), Config->get_exclusive_solo ());
	}
}

void
MonitorSection::unassign_controllables ()
{
	boost::shared_ptr<Controllable> none;

	solo_cut_control->set_controllable (none);
	solo_cut_display->set_controllable (none);
	gain_control->set_controllable (none);
	gain_display->set_controllable (none);
	cut_all_button.set_controllable (none);
	dim_all_button.set_controllable (none);
	mono_button.set_controllable (none);
	dim_control->set_controllable (none);
	dim_display->set_controllable (none);
	solo_boost_control->set_controllable (none);
	solo_boost_display->set_controllable (none);
}

void
MonitorSection::assign_controllables ()
{
	assert (_session);
	assert (_route);
	assert (_monitor);

	solo_cut_control->set_controllable (_session->solo_cut_control());
	solo_cut_display->set_controllable (_session->solo_cut_control());

	gain_control->set_controllable (_route->gain_control());
	gain_display->set_controllable (_route->gain_control());
	cut_all_button.set_controllable (_monitor->cut_control());
	cut_all_button.watch ();
	dim_all_button.set_controllable (_monitor->dim_control());
	dim_all_button.watch ();
	mono_button.set_controllable (_monitor->mono_control());
	mono_button.watch ();
	dim_control->set_controllable (_monitor->dim_level_control ());
	dim_display->set_controllable (_monitor->dim_level_control ());
	solo_boost_control->set_controllable (_monitor->solo_boost_control ());
	solo_boost_display->set_controllable (_monitor->solo_boost_control ());
}

string
MonitorSection::state_id() const
{
	return "monitor-section";
}

void
MonitorSection::maybe_add_bundle_to_output_menu (boost::shared_ptr<Bundle> b, ARDOUR::BundleList const& /*current*/)
{
	using namespace Menu_Helpers;

	if (b->ports_are_inputs() == false || b->nchannels() != _route->n_outputs() || *b == *_route->input()->bundle()) {
		return;
	}

	list<boost::shared_ptr<Bundle> >::iterator i = output_menu_bundles.begin ();
	while (i != output_menu_bundles.end() && b->has_same_ports (*i) == false) {
		++i;
	}

	if (i != output_menu_bundles.end()) {
		return;
	}

	output_menu_bundles.push_back (b);

	MenuList& citems = output_menu.items();

	std::string n = b->name ();
	replace_all (n, "_", " ");

	citems.push_back (MenuElem (n, sigc::bind (sigc::mem_fun(*this, &MonitorSection::bundle_output_chosen), b)));
}

void
MonitorSection::bundle_output_chosen (boost::shared_ptr<ARDOUR::Bundle> c)
{

	ARDOUR::BundleList current = _route->output()->bundles_connected ();

	if (std::find (current.begin(), current.end(), c) == current.end()) {
		_route->output()->connect_ports_to_bundle (c, true, this);
	} else {
		_route->output()->disconnect_ports_from_bundle (c, this);
	}
}

gint
MonitorSection::output_release (GdkEventButton *ev)
{
	switch (ev->button) {
	case 3:
		edit_output_configuration ();
		break;
	}

	return false;
}

struct RouteCompareByName {
	bool operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b) {
		return a->name().compare (b->name()) < 0;
	}
};

gint
MonitorSection::output_press (GdkEventButton *ev)
{
	using namespace Menu_Helpers;
	if (!_session) {
		MessageDialog msg (_("No session - no I/O changes are possible"));
		msg.run ();
		return true;
	}

	MenuList& citems = output_menu.items();
	switch (ev->button) {

	case 3:
		return false;  //wait for the mouse-up to pop the dialog

	case 1:
	{
		output_menu.set_name ("ArdourContextMenu");
		citems.clear ();
		output_menu_bundles.clear ();

		citems.push_back (MenuElem (_("Disconnect"), sigc::mem_fun (*(this), &MonitorSection::disconnect_output)));

		citems.push_back (SeparatorElem());
		uint32_t const n_with_separator = citems.size ();

		ARDOUR::BundleList current = _route->output()->bundles_connected ();

		boost::shared_ptr<ARDOUR::BundleList> b = _session->bundles ();

		/* give user bundles first chance at being in the menu */

		for (ARDOUR::BundleList::iterator i = b->begin(); i != b->end(); ++i) {
			if (boost::dynamic_pointer_cast<UserBundle> (*i)) {
				maybe_add_bundle_to_output_menu (*i, current);
			}
		}

		for (ARDOUR::BundleList::iterator i = b->begin(); i != b->end(); ++i) {
			if (boost::dynamic_pointer_cast<UserBundle> (*i) == 0) {
				maybe_add_bundle_to_output_menu (*i, current);
			}
		}

		boost::shared_ptr<ARDOUR::RouteList> routes = _session->get_routes ();
		RouteList copy = *routes;
		copy.sort (RouteCompareByName ());
		for (ARDOUR::RouteList::const_iterator i = copy.begin(); i != copy.end(); ++i) {
			maybe_add_bundle_to_output_menu ((*i)->output()->bundle(), current);
		}

		if (citems.size() == n_with_separator) {
			/* no routes added; remove the separator */
			citems.pop_back ();
		}

		citems.push_back (SeparatorElem());
		citems.push_back (MenuElem (_("Routing Grid"), sigc::mem_fun (*(this), &MonitorSection::edit_output_configuration)));

		output_menu.popup (1, ev->time);
		break;
	}

	default:
		break;
	}
	return TRUE;
}

void
MonitorSection::update_output_display ()
{
	if (!_route || !_monitor || _session->deletion_in_progress()) {
		return;
	}

	uint32_t io_count;
	uint32_t io_index;
	boost::shared_ptr<Port> port;
	vector<string> port_connections;

	uint32_t total_connection_count = 0;
	uint32_t io_connection_count = 0;
	uint32_t ardour_connection_count = 0;
	uint32_t system_connection_count = 0;
	uint32_t other_connection_count = 0;

	ostringstream label;

	bool have_label = false;
	bool each_io_has_one_connection = true;

	string connection_name;
	string ardour_track_name;
	string other_connection_type;
	string system_ports;
	string system_port;

	ostringstream tooltip;
	char * tooltip_cstr;

	io_count = _route->n_outputs().n_total();
	tooltip << string_compose (_("<b>OUTPUT</b> from %1"), Gtkmm2ext::markup_escape_text (_route->name()));


	for (io_index = 0; io_index < io_count; ++io_index) {

		port = _route->output()->nth (io_index);

		//ignore any port connections that don't match our DataType
		if (port->type() != DataType::AUDIO) {
			continue;
		}

		port_connections.clear ();
		port->get_connections(port_connections);
		io_connection_count = 0;

		if (!port_connections.empty()) {
			for (vector<string>::iterator i = port_connections.begin(); i != port_connections.end(); ++i) {
				string pn = "";
				string& connection_name (*i);

				if (connection_name.find("system:") == 0) {
					pn = AudioEngine::instance()->get_pretty_name_by_name (connection_name);
				}

				if (io_connection_count == 0) {
					tooltip << endl << Gtkmm2ext::markup_escape_text (port->name().substr(port->name().find("/") + 1))
						<< " -> "
						<< Gtkmm2ext::markup_escape_text ( pn.empty() ? connection_name : pn );
				} else {
					tooltip << ", "
						<< Gtkmm2ext::markup_escape_text ( pn.empty() ? connection_name : pn );
				}

				if (connection_name.find(RouteUI::program_port_prefix) == 0) {
					if (ardour_track_name.empty()) {
						// "ardour:Master/in 1" -> "ardour:Master/"
						string::size_type slash = connection_name.find("/");
						if (slash != string::npos) {
							ardour_track_name = connection_name.substr(0, slash + 1);
						}
					}

					if (connection_name.find(ardour_track_name) == 0) {
						++ardour_connection_count;
					}
				} else if (!pn.empty()) {
					if (system_ports.empty()) {
						system_ports += pn;
					} else {
						system_ports += "/" + pn;
					}
					if (connection_name.find("system:") == 0) {
						++system_connection_count;
					}
				} else if (connection_name.find("system:") == 0) {
					// "system:playback_123" -> "123"
					system_port = connection_name.substr(16);
					if (system_ports.empty()) {
						system_ports += system_port;
					} else {
						system_ports += "/" + system_port;
					}

					++system_connection_count;
				} else {
					if (other_connection_type.empty()) {
						// "jamin:in 1" -> "jamin:"
						other_connection_type = connection_name.substr(0, connection_name.find(":") + 1);
					}

					if (connection_name.find(other_connection_type) == 0) {
						++other_connection_count;
					}
				}

				++total_connection_count;
				++io_connection_count;
			}
		}

		if (io_connection_count != 1) {
			each_io_has_one_connection = false;
		}
	}

	if (total_connection_count == 0) {
		tooltip << endl << _("Disconnected");
	}

	tooltip_cstr = new char[tooltip.str().size() + 1];
	strcpy(tooltip_cstr, tooltip.str().c_str());

	set_tooltip (output_button, tooltip_cstr, "");

	if (each_io_has_one_connection) {
		if (total_connection_count == ardour_connection_count) {
			// all connections are to the same track in ardour
			// "ardour:Master/" -> "Master"
			string::size_type slash = ardour_track_name.find("/");
			if (slash != string::npos) {
				label << ardour_track_name.substr(7, slash - 7);
				have_label = true;
			}
		} else if (total_connection_count == system_connection_count) {
			// all connections are to system ports
			label << system_ports;
			have_label = true;
		} else if (total_connection_count == other_connection_count) {
			// all connections are to the same external program eg jamin
			// "jamin:" -> "jamin"
			label << other_connection_type.substr(0, other_connection_type.size() - 1);
			have_label = true;
		}
	}

	if (!have_label) {
		if (total_connection_count == 0) {
			// Disconnected
			label << "-";
		} else {
			// Odd configuration
			label << "*" << total_connection_count << "*";
		}
	}

	output_button->set_text (label.str());
}

void
MonitorSection::disconnect_output ()
{
	if (_route) {
		_route->output()->disconnect(this);
	}
}

void
MonitorSection::edit_output_configuration ()
{
	if (_output_selector == 0) {
		_output_selector = new MonitorSelectorWindow (_session, _route->output());
	}
	_output_selector->present ();
}

void
MonitorSection::port_connected_or_disconnected (boost::weak_ptr<Port> wa, boost::weak_ptr<Port> wb)
{
	if (!_route) {
		return;
	}
	boost::shared_ptr<Port> a = wa.lock ();
	boost::shared_ptr<Port> b = wb.lock ();
	if ((a && _route->output()->has_port (a)) || (b && _route->output()->has_port (b))) {
		update_output_display ();
	}
}

void
MonitorSection::port_pretty_name_changed (std::string pn)
{
	if (!_route) {
		return;
	}
	if (_route->output()->connected_to (pn)) {
		update_output_display ();
	}
}


void
MonitorSection::load_bindings ()
{
	bindings = Bindings::get_bindings (X_("Monitor Section"));
}

void
MonitorSection::help_count_processors (boost::weak_ptr<Processor> p, uint32_t* cnt) const
{
	boost::shared_ptr<Processor> processor (p.lock ());
	if (!processor || !processor->display_to_user()) {
		return;
	}
	if (boost::dynamic_pointer_cast<Amp>(processor)) {
		return;
	}
	++(*cnt);
}

uint32_t
MonitorSection::count_processors ()
{
	uint32_t processor_count = 0;
	if (_route) {
		_route->foreach_processor (sigc::bind (sigc::mem_fun (*this, &MonitorSection::help_count_processors), &processor_count));
	}
	return processor_count;
}

void
MonitorSection::processors_changed (ARDOUR::RouteProcessorChange)
{
	update_processor_box ();
}

PluginSelector*
MonitorSection::plugin_selector ()
{
	return Mixer_UI::instance()->plugin_selector ();
}

void
MonitorSection::use_others_actions ()
{
	rude_solo_button.set_related_action (ActionManager::get_action (X_("Main"), X_("cancel-solo")));
}

void
MonitorSection::toggle_use_monitor_section ()
{
	if (!_session) {
		return;
	}
	bool want_ms = ActionManager::get_toggle_action (X_("Monitor"), "UseMonitorSection")->get_active();
	bool have_ms = Config->get_use_monitor_bus ();

	if (want_ms == have_ms) {
		return;
	}

	if (want_ms) {
		Config->set_use_monitor_bus (true);
		ActionManager::get_toggle_action (X_("Mixer"), X_("ToggleMonitorSection"))->set_active (true);
	} else {
		Config->set_use_monitor_bus (false);
	}
}
