/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
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

#include <gtkmm/alignment.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/stock.h>

#include "pbd/compose.h"
#include "pbd/convert.h"

#include "ardour/region.h"
#include "ardour/triggerbox.h"

#include "gtkmm2ext/utils.h"

#include "audio_region_properties_box.h"
#include "audio_trigger_properties_box.h"
#include "audio_region_operations_box.h"

#include "midi_trigger_properties_box.h"
#include "midi_region_properties_box.h"
#include "midi_region_operations_box.h"

#include "slot_properties_box.h"
#include "midi_clip_editor.h"

#include "ardour_ui.h"
#include "gui_thread.h"
#include "trigger_ui.h"
#include "public_editor.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace Temporal;

static std::vector<std::string> follow_strings;
static std::string longest_follow;
static std::vector<std::string> quantize_strings;
static std::string longest_quantize;
static std::vector<std::string> launch_strings;
static std::string longest_launch;

TriggerUI::TriggerUI ()
	: _follow_action_button (ArdourButton::led_default_elements)
	, _velocity_adjustment(1.,0.,1.0,0.01,0.1)
	, _velocity_slider (&_velocity_adjustment, boost::shared_ptr<PBD::Controllable>(), 24/*length*/, 12/*girth*/ )
	, _follow_probability_adjustment(0,0,100,2,5)
	, _follow_probability_slider (&_follow_probability_adjustment, boost::shared_ptr<PBD::Controllable>(), 24/*length*/, 12/*girth*/ )
	, _follow_count_adjustment (1, 1, 128, 1, 4)
	, _follow_count_spinner (_follow_count_adjustment)
	, _legato_button (ArdourButton::led_default_elements)

{
	using namespace Gtk::Menu_Helpers;

	if (follow_strings.empty()) {
		follow_strings.push_back (follow_action_to_string (Trigger::None));
		follow_strings.push_back (follow_action_to_string (Trigger::Stop));
		follow_strings.push_back (follow_action_to_string (Trigger::Again));
		follow_strings.push_back (follow_action_to_string (Trigger::QueuedTrigger));
		follow_strings.push_back (follow_action_to_string (Trigger::NextTrigger));
		follow_strings.push_back (follow_action_to_string (Trigger::PrevTrigger));
		follow_strings.push_back (follow_action_to_string (Trigger::FirstTrigger));
		follow_strings.push_back (follow_action_to_string (Trigger::LastTrigger));
		follow_strings.push_back (follow_action_to_string (Trigger::AnyTrigger));
		follow_strings.push_back (follow_action_to_string (Trigger::OtherTrigger));

		for (std::vector<std::string>::const_iterator i = follow_strings.begin(); i != follow_strings.end(); ++i) {
			if (i->length() > longest_follow.length()) {
				longest_follow = *i;
			}
		}

		launch_strings.push_back (launch_style_to_string (Trigger::OneShot));
		launch_strings.push_back (launch_style_to_string (Trigger::Gate));
		launch_strings.push_back (launch_style_to_string (Trigger::Toggle));
		launch_strings.push_back (launch_style_to_string (Trigger::Repeat));

		for (std::vector<std::string>::const_iterator i = launch_strings.begin(); i != launch_strings.end(); ++i) {
			if (i->length() > longest_launch.length()) {
				longest_launch = *i;
			}
		}
	}

	set_spacings (2);
	set_homogeneous (false);

	_follow_action_button.set_name("FollowAction");
	_follow_action_button.set_text (_("Follow Action"));
	_follow_action_button.signal_event().connect (sigc::mem_fun (*this, (&TriggerUI::follow_action_button_event)));

	_follow_count_spinner.set_can_focus(false);
	_follow_count_spinner.signal_changed ().connect (sigc::mem_fun (*this, &TriggerUI::follow_count_event));

	_velocity_adjustment.signal_value_changed ().connect (sigc::mem_fun (*this, &TriggerUI::velocity_adjusted));

	_velocity_slider.set_name("FollowAction");

	_follow_probability_adjustment.signal_value_changed ().connect (sigc::mem_fun (*this, &TriggerUI::probability_adjusted));

	_follow_probability_slider.set_name("FollowAction");

	_follow_left.set_name("FollowAction");
	_follow_left.AddMenuElem (MenuElem (follow_action_to_string(Trigger::None), sigc::bind (sigc::mem_fun (*this, &TriggerUI::set_follow_action),         Trigger::None, 0)));
	_follow_left.AddMenuElem (MenuElem (follow_action_to_string(Trigger::Stop), sigc::bind (sigc::mem_fun (*this, &TriggerUI::set_follow_action),         Trigger::Stop, 0)));
	_follow_left.AddMenuElem (MenuElem (follow_action_to_string(Trigger::Again), sigc::bind (sigc::mem_fun (*this, &TriggerUI::set_follow_action),        Trigger::Again, 0)));
	_follow_left.AddMenuElem (MenuElem (follow_action_to_string(Trigger::PrevTrigger), sigc::bind (sigc::mem_fun (*this, &TriggerUI::set_follow_action),  Trigger::PrevTrigger, 0)));
	_follow_left.AddMenuElem (MenuElem (follow_action_to_string(Trigger::NextTrigger), sigc::bind (sigc::mem_fun (*this, &TriggerUI::set_follow_action),  Trigger::NextTrigger, 0)));
	_follow_left.AddMenuElem (MenuElem (follow_action_to_string(Trigger::AnyTrigger), sigc::bind (sigc::mem_fun (*this, &TriggerUI::set_follow_action),   Trigger::AnyTrigger, 0)));
	_follow_left.AddMenuElem (MenuElem (follow_action_to_string(Trigger::OtherTrigger), sigc::bind (sigc::mem_fun (*this, &TriggerUI::set_follow_action), Trigger::OtherTrigger, 0)));
	_follow_left.set_sizing_text (longest_follow);

	_follow_right.set_name("FollowAction");
	_follow_right.AddMenuElem (MenuElem (follow_action_to_string(Trigger::None), sigc::bind (sigc::mem_fun (*this, &TriggerUI::set_follow_action),         Trigger::None, 1)));
	_follow_right.AddMenuElem (MenuElem (follow_action_to_string(Trigger::Stop), sigc::bind (sigc::mem_fun (*this, &TriggerUI::set_follow_action),         Trigger::Stop, 1)));
	_follow_right.AddMenuElem (MenuElem (follow_action_to_string(Trigger::Again), sigc::bind (sigc::mem_fun (*this, &TriggerUI::set_follow_action),        Trigger::Again, 1)));
	_follow_right.AddMenuElem (MenuElem (follow_action_to_string(Trigger::PrevTrigger), sigc::bind (sigc::mem_fun (*this, &TriggerUI::set_follow_action),  Trigger::PrevTrigger, 1)));
	_follow_right.AddMenuElem (MenuElem (follow_action_to_string(Trigger::NextTrigger), sigc::bind (sigc::mem_fun (*this, &TriggerUI::set_follow_action),  Trigger::NextTrigger, 1)));
	_follow_right.AddMenuElem (MenuElem (follow_action_to_string(Trigger::AnyTrigger), sigc::bind (sigc::mem_fun (*this, &TriggerUI::set_follow_action),   Trigger::AnyTrigger, 1)));
	_follow_right.AddMenuElem (MenuElem (follow_action_to_string(Trigger::OtherTrigger), sigc::bind (sigc::mem_fun (*this, &TriggerUI::set_follow_action), Trigger::OtherTrigger, 1)));
	_follow_right.set_sizing_text (longest_follow);

	_launch_style_button.set_name("FollowAction");
	_launch_style_button.set_sizing_text (longest_launch);
	_launch_style_button.AddMenuElem (MenuElem (launch_style_to_string (Trigger::OneShot), sigc::bind (sigc::mem_fun (*this, &TriggerUI::set_launch_style), Trigger::OneShot)));
	_launch_style_button.AddMenuElem (MenuElem (launch_style_to_string (Trigger::Gate), sigc::bind (sigc::mem_fun (*this, &TriggerUI::set_launch_style), Trigger::Gate)));
	_launch_style_button.AddMenuElem (MenuElem (launch_style_to_string (Trigger::Toggle), sigc::bind (sigc::mem_fun (*this, &TriggerUI::set_launch_style), Trigger::Toggle)));
	_launch_style_button.AddMenuElem (MenuElem (launch_style_to_string (Trigger::Repeat), sigc::bind (sigc::mem_fun (*this, &TriggerUI::set_launch_style), Trigger::Repeat)));

	_launch_style_button.set_name("FollowAction");
	_legato_button.set_text (_("Legato"));
	_legato_button.signal_event().connect (sigc::mem_fun (*this, (&TriggerUI::legato_button_event)));

#define quantize_item(b) _quantize_button.AddMenuElem (MenuElem (quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &TriggerUI::set_quantize), b)));

#if TRIGGER_PAGE_GLOBAL_QUANTIZATION_IMPLEMENTED
	quantize_item (BBT_Offset (0, 0, 0));
#endif
	quantize_item (BBT_Offset (1, 0, 0));
	quantize_item (BBT_Offset (0, 4, 0));
	quantize_item (BBT_Offset (0, 2, 0));
	quantize_item (BBT_Offset (0, 1, 0));
	quantize_item (BBT_Offset (0, 0, Temporal::ticks_per_beat/2));
	quantize_item (BBT_Offset (0, 0, Temporal::ticks_per_beat/4));
	quantize_item (BBT_Offset (0, 0, Temporal::ticks_per_beat/8));
	quantize_item (BBT_Offset (0, 0, Temporal::ticks_per_beat/16));

	for (std::vector<std::string>::const_iterator i = quantize_strings.begin(); i != quantize_strings.end(); ++i) {
		if (i->length() > longest_quantize.length()) {
			longest_quantize = *i;
		}
	}
	_quantize_button.set_sizing_text (longest_quantize);
	_quantize_button.set_name("FollowAction");

#undef quantize_item

	int row=0;
	Gtk::Label *label;

	label = manage(new Gtk::Label(_("Velocity Sense:")));  label->set_alignment(1.0, 0.5);
	attach(*label,                 0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK );
	attach(_velocity_slider,       1, 2, row, row+1, Gtk::FILL, Gtk::SHRINK ); row++;

	label = manage(new Gtk::Label(_("Launch Style:")));  label->set_alignment(1.0, 0.5);
	attach(*label,                 0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK );
	attach(_launch_style_button,   1, 2, row, row+1, Gtk::FILL, Gtk::SHRINK ); row++;

	label = manage(new Gtk::Label(_("Launch Quantize:")));  label->set_alignment(1.0, 0.5);
	attach(*label,            0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK );
	attach(_quantize_button,  1, 2, row, row+1, Gtk::FILL, Gtk::SHRINK ); row++;

	label = manage(new Gtk::Label(_("Legato Mode:")));  label->set_alignment(1.0, 0.5);
	attach(*label,          0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK );
	attach(_legato_button,  1, 2, row, row+1, Gtk::FILL, Gtk::SHRINK ); row++;

	attach(_follow_action_button,   0, 2, row, row+1, Gtk::FILL, Gtk::SHRINK ); row++;

	label = manage(new Gtk::Label(_("Follow Count:")));  label->set_alignment(1.0, 0.5);
	attach(*label,          0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK );
	Gtk::Alignment *align = manage (new Gtk::Alignment (0, .5, 0, 0));
	align->add (_follow_count_spinner);
	attach(*align,          1, 2, row, row+1, Gtk::FILL, Gtk::SHRINK, 0, 0 ); row++;

	Gtkmm2ext::set_size_request_to_display_given_text (_left_probability_label, "100% Left ", 12, 0);
	_left_probability_label.set_alignment(0.0, 0.5);
	Gtkmm2ext::set_size_request_to_display_given_text (_right_probability_label, "100% Right", 12, 0);
	_right_probability_label.set_alignment(1.0, 0.5);

	Gtk::Table *prob_table = manage(new Gtk::Table());
	prob_table->set_spacings(2);
	prob_table->attach(_follow_probability_slider, 0, 2, 0, 1, Gtk::FILL, Gtk::SHRINK );
	prob_table->attach(_left_probability_label,    0, 1, 1, 2, Gtk::FILL,             Gtk::SHRINK );
	prob_table->attach(_right_probability_label,   1, 2, 1, 2, Gtk::FILL,             Gtk::SHRINK );

	attach( *prob_table,   0, 2, row, row+1, Gtk::FILL, Gtk::SHRINK ); row++;
	attach(_follow_left,   0, 1, row, row+1, Gtk::FILL,             Gtk::SHRINK );
	attach(_follow_right,  1, 2, row, row+1, Gtk::FILL,             Gtk::SHRINK ); row++;
}

TriggerUI::~TriggerUI ()
{
}

TriggerPtr
TriggerUI::trigger() const
{
	return tref.trigger();
}

void
TriggerUI::set_trigger (ARDOUR::TriggerReference tr)
{
	tref = tr;

	PropertyChange pc;

	pc.add (Properties::use_follow);
	pc.add (Properties::legato);
	pc.add (Properties::quantization);
	pc.add (Properties::launch_style);
	pc.add (Properties::follow_count);
	pc.add (Properties::follow_action0);
	pc.add (Properties::follow_action1);
	pc.add (Properties::velocity_effect);
	pc.add (Properties::follow_action_probability);

	trigger_changed (pc);

	trigger()->PropertyChanged.connect (trigger_connections, invalidator (*this), boost::bind (&TriggerUI::trigger_changed, this, _1), gui_context());
}


void
TriggerUI::set_quantize (BBT_Offset bbo)
{
#if TRIGGER_PAGE_GLOBAL_QUANTIZATION_IMPLEMENTED
	if (bbo == BBT_Offset (0, 0, 0)) {
		/* use grid */
		bbo = BBT_Offset (1, 2, 3); /* XXX get grid from editor */
	}
#endif

	trigger()->set_quantization (bbo);
}

void
TriggerUI::follow_count_event ()
{
	trigger()->set_follow_count ((int) _follow_count_adjustment.get_value());
}

void
TriggerUI::velocity_adjusted ()
{
	trigger()->set_midi_velocity_effect (_velocity_adjustment.get_value());
}

void
TriggerUI::probability_adjusted ()
{
	trigger()->set_follow_action_probability ((int) _follow_probability_adjustment.get_value());
}

bool
TriggerUI::follow_action_button_event (GdkEvent* ev)
{
	switch (ev->type) {
	case GDK_BUTTON_PRESS:
		trigger()->set_use_follow (!trigger()->use_follow());
		return true;

	default:
		break;
	}

	return false;
}

bool
TriggerUI::legato_button_event (GdkEvent* ev)
{
	switch (ev->type) {
	case GDK_BUTTON_PRESS:
		trigger()->set_legato (!trigger()->legato());
		return true;

	default:
		break;
	}

	return false;
}

void
TriggerUI::set_launch_style (Trigger::LaunchStyle ls)
{
	trigger()->set_launch_style (ls);
}

void
TriggerUI::set_follow_action (Trigger::FollowAction fa, uint64_t idx)
{
	trigger->set_follow_action (fa, idx);
}

std::string
TriggerUI::launch_style_to_string (Trigger::LaunchStyle ls)
{
	switch (ls) {
	case Trigger::OneShot:
		return _("One Shot");
	case Trigger::Gate:
		return _("Gate");
	case Trigger::Toggle:
		return _("Toggle");
	case Trigger::Repeat:
		return _("Repeat");
	}
	/*NOTREACHED*/
	return std::string();
}

std::string
TriggerUI::quantize_length_to_string (BBT_Offset const & ql)
{
	if (ql == BBT_Offset (1, 0, 0)) {
		return _("1 Bar");
	} else if (ql == BBT_Offset (0, 1, 0)) {
		return _("1/4");
	} else if (ql == BBT_Offset (0, 2, 0)) {
		return _("1/2");
	} else if (ql == BBT_Offset (0, 4, 0)) {
		return _("Whole");
	} else if (ql == BBT_Offset (0, 0,Temporal::ticks_per_beat/2)) {
		return _("1/8");
	} else if (ql == BBT_Offset (0, 0,Temporal::ticks_per_beat/4)) {
		return _("1/16");
	} else if (ql == BBT_Offset (0, 0,Temporal::ticks_per_beat/8)) {
		return _("1/32");
	} else if (ql == BBT_Offset (0, 0,Temporal::ticks_per_beat/16)) {
		return _("1/64");
	} else {
		return "???";
	}
}

std::string
TriggerUI::follow_action_to_string (Trigger::FollowAction fa)
{
	switch (fa) {
	case Trigger::None:
		return _("None");
	case Trigger::Stop:
		return _("Stop");
	case Trigger::Again:
		return _("Again");
	case Trigger::QueuedTrigger:
		return _("Queued");
	case Trigger::NextTrigger:
		return _("Next");
	case Trigger::PrevTrigger:
		return _("Prev");
	case Trigger::FirstTrigger:
		return _("First");
	case Trigger::LastTrigger:
		return _("Last");
	case Trigger::AnyTrigger:
		return _("Any");
	case Trigger::OtherTrigger:
		return _("Other");
	}
	/*NOTREACHED*/
	return std::string();
}

void
TriggerUI::trigger_changed (PropertyChange pc)
{
	if (pc.contains (Properties::quantization)) {
		BBT_Offset bbo (trigger()->quantization());
		_quantize_button.set_active (quantize_length_to_string (bbo));
	}

	if (pc.contains (Properties::use_follow)) {
		_follow_action_button.set_active_state (trigger()->use_follow() ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	}

	if (pc.contains (Properties::follow_count)) {
		_follow_count_adjustment.set_value (trigger()->follow_count());
	}

	if (pc.contains (Properties::legato)) {
		_legato_button.set_active_state (trigger()->legato() ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	}

	if (pc.contains (Properties::launch_style)) {
		_launch_style_button.set_active (launch_style_to_string (trigger()->launch_style()));
	}

	if (pc.contains (Properties::follow_action0)) {
			_follow_left.set_text (follow_action_to_string (trigger()->follow_action (0)));
	}

	if (pc.contains (Properties::follow_action1)) {
		_follow_right.set_text (follow_action_to_string (trigger()->follow_action (1)));
	}

	if (pc.contains (Properties::velocity_effect)) {
		_velocity_adjustment.set_value (trigger()->midi_velocity_effect());
	}

	if (pc.contains (Properties::follow_action_probability)) {
		int pval = trigger()->follow_action_probability();
		_follow_probability_adjustment.set_value (pval);
		_left_probability_label.set_text (string_compose(_("%1%% Left"), pval));
		_right_probability_label.set_text (string_compose(_("%1%% Right"), 100-pval));
	}

	if (trigger()->use_follow()) {
		_follow_left.set_sensitive(true);
		_follow_right.set_sensitive(true);
		_follow_count_spinner.set_sensitive(true);
		_follow_probability_slider.set_sensitive(true);
		_left_probability_label.set_sensitive(true);
		_right_probability_label.set_sensitive(true);
	} else {
		_follow_left.set_sensitive(false);
		_follow_right.set_sensitive(false);
		_follow_count_spinner.set_sensitive(false);
		_follow_probability_slider.set_sensitive(false);
		_left_probability_label.set_sensitive(false);
		_right_probability_label.set_sensitive(false);
	}
}

/* ------------ */

TriggerWidget::TriggerWidget ()
{
	ui = new TriggerUI ();
	pack_start(*ui);
	ui->show();
//	set_background_color (UIConfiguration::instance().color (X_("theme:bg")));
}

/* ------------ */

TriggerWindow::TriggerWindow (TriggerReference tref)
{
	TriggerPtr trigger (tref.trigger());

	set_title (string_compose (_("Trigger: %1"), trigger->name()));

	SlotPropertiesBox* slot_prop_box = manage (new SlotPropertiesBox ());
	slot_prop_box->set_slot (tref);

	Gtk::Table* table = manage (new Gtk::Table);
	table->set_homogeneous (false);
	table->set_spacings (16);
	table->set_border_width (8);

	int col = 0;
	table->attach(*slot_prop_box,  col, col+1, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND );  col++;

	if (trigger->region()) {
		if (trigger->region()->data_type() == DataType::AUDIO) {
			_trig_box = manage(new AudioTriggerPropertiesBox ());
			_ops_box = manage(new AudioRegionOperationsBox ());
			_trim_box = manage(new AudioClipEditorBox ());

			_trig_box->set_trigger (tref);
		} else {
			_trig_box = manage(new MidiTriggerPropertiesBox ());
			_ops_box = manage(new MidiRegionOperationsBox ());
			_trim_box = manage(new MidiClipEditorBox ());

			_trig_box->set_trigger (tref);
		}

		_trim_box->set_region(trigger->region(), tref);
		_ops_box->set_session(&trigger->region()->session());

		table->attach(*_trig_box,  col, col+1, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND );  col++;
		table->attach(*_trim_box,  col, col+1, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND );  col++;
		table->attach(*_ops_box,   col, col+1, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND );  col++;
	}

	add (*table);
	table->show_all();
}

bool
TriggerWindow::on_key_press_event (GdkEventKey* ev)
{
	Gtk::Window& main_window (ARDOUR_UI::instance()->main_window());
	return ARDOUR_UI_UTILS::relay_key_press (ev, &main_window);
}

bool
TriggerWindow::on_key_release_event (GdkEventKey* ev)
{
	Gtk::Window& main_window (ARDOUR_UI::instance()->main_window());
	return ARDOUR_UI_UTILS::relay_key_press (ev, &main_window);
}
