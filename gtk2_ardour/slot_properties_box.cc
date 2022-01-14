/*
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2021 Ben Loftis <ben@harrisonconsoles.com>
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
#include "pbd/basename.h"
#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/file_utils.h"
#include "pbd/pathexpand.h"
#include "pbd/search_path.h"

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/actions.h"

#include <gtkmm/alignment.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/stock.h>

#include "ardour/location.h"
#include "ardour/profile.h"
#include "ardour/session.h"
#include "ardour/region.h"
#include "ardour/triggerbox.h"

#include "ardour_ui.h"
#include "audio_clock.h"
#include "region_view.h"
#include "trigger_ui.h"
#include "utils.h"

#include "audio_region_properties_box.h"
#include "audio_trigger_properties_box.h"
#include "audio_region_operations_box.h"

#include "midi_trigger_properties_box.h"
#include "midi_region_properties_box.h"
#include "midi_region_operations_box.h"
#include "midi_clip_editor.h"

#include "slot_properties_box.h"

#include "pbd/i18n.h"


using namespace Gtk;
using namespace PBD;
using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace Temporal;
using std::min;
using std::max;

SlotPropertiesBox::SlotPropertiesBox ()
{
	_header_label.set_text(_("Slot Properties:"));
	_header_label.set_alignment(0.0, 0.5);
//	pack_start(_header_label, false, false, 6);

	_triggerwidget = manage (new SlotPropertyWidget ());
	_triggerwidget->show();

	pack_start (*_triggerwidget, true, true);
}

SlotPropertiesBox::~SlotPropertiesBox ()
{
}

void
SlotPropertiesBox::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);
}

void
SlotPropertiesBox::set_slot (TriggerReference tref)
{
	_triggerwidget->set_trigger (tref);
}

/* **************************************** */

SlotPropertyTable::SlotPropertyTable ()
	: _color_button (ArdourButton::Element (ArdourButton::just_led_default_elements | ArdourButton::ColorBox))
	, _follow_action_button (ArdourButton::led_default_elements)
	, _velocity_adjustment(1.,0.,1.0,0.01,0.1)
	, _velocity_slider (&_velocity_adjustment, boost::shared_ptr<PBD::Controllable>(), 24/*length*/, 12/*girth*/ )
	, _follow_probability_adjustment(0,0,100,2,5)
	, _follow_probability_slider (&_follow_probability_adjustment, boost::shared_ptr<PBD::Controllable>(), 24/*length*/, 12/*girth*/ )
	, _follow_count_adjustment (1, 1, 128, 1, 4)
	, _follow_count_spinner (_follow_count_adjustment)
	, _follow_length_adjustment (0, 0, 128, 1, 4)
	, _follow_length_spinner (_follow_length_adjustment)
	, _legato_button (ArdourButton::led_default_elements)

{
	using namespace Gtk::Menu_Helpers;

	_follow_action_button.set_name("FollowAction");
	_follow_action_button.set_text (_("Follow Action"));
	_follow_action_button.signal_event().connect (sigc::mem_fun (*this, (&SlotPropertyTable::follow_action_button_event)));

	_follow_count_spinner.set_can_focus(false);
	_follow_count_spinner.signal_changed ().connect (sigc::mem_fun (*this, &SlotPropertyTable::follow_count_event));

	_follow_length_spinner.set_can_focus(false);
	_follow_length_spinner.signal_changed ().connect (sigc::mem_fun (*this, &SlotPropertyTable::follow_length_event));

	_velocity_adjustment.signal_value_changed ().connect (sigc::mem_fun (*this, &SlotPropertyTable::velocity_adjusted));

	_velocity_slider.set_name("FollowAction");

	_follow_probability_adjustment.signal_value_changed ().connect (sigc::mem_fun (*this, &SlotPropertyTable::probability_adjusted));

	_follow_probability_slider.set_name("FollowAction");

	_follow_left.set_name("FollowAction");
	_follow_left.AddMenuElem (MenuElem (follow_action_to_string(Trigger::None), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_follow_action),         Trigger::None, 0)));
	_follow_left.AddMenuElem (MenuElem (follow_action_to_string(Trigger::Stop), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_follow_action),         Trigger::Stop, 0)));
	_follow_left.AddMenuElem (MenuElem (follow_action_to_string(Trigger::Again), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_follow_action),        Trigger::Again, 0)));
	_follow_left.AddMenuElem (MenuElem (follow_action_to_string(Trigger::PrevTrigger), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_follow_action),  Trigger::PrevTrigger, 0)));
	_follow_left.AddMenuElem (MenuElem (follow_action_to_string(Trigger::NextTrigger), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_follow_action),  Trigger::NextTrigger, 0)));
	_follow_left.AddMenuElem (MenuElem (follow_action_to_string(Trigger::ReverseTrigger), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_follow_action),  Trigger::ReverseTrigger, 0)));
	_follow_left.AddMenuElem (MenuElem (follow_action_to_string(Trigger::ForwardTrigger), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_follow_action),  Trigger::ForwardTrigger, 0)));
	_follow_left.AddMenuElem (MenuElem (follow_action_to_string(Trigger::AnyTrigger), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_follow_action),   Trigger::AnyTrigger, 0)));
	_follow_left.AddMenuElem (MenuElem (follow_action_to_string(Trigger::OtherTrigger), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_follow_action), Trigger::OtherTrigger, 0)));
	_follow_left.set_sizing_text (longest_follow);

	_follow_right.set_name("FollowAction");
	_follow_right.AddMenuElem (MenuElem (follow_action_to_string(Trigger::None), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_follow_action),         Trigger::None, 1)));
	_follow_right.AddMenuElem (MenuElem (follow_action_to_string(Trigger::Stop), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_follow_action),         Trigger::Stop, 1)));
	_follow_right.AddMenuElem (MenuElem (follow_action_to_string(Trigger::Again), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_follow_action),        Trigger::Again, 1)));
	_follow_right.AddMenuElem (MenuElem (follow_action_to_string(Trigger::PrevTrigger), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_follow_action),  Trigger::PrevTrigger, 1)));
	_follow_right.AddMenuElem (MenuElem (follow_action_to_string(Trigger::NextTrigger), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_follow_action),  Trigger::NextTrigger, 1)));
	_follow_right.AddMenuElem (MenuElem (follow_action_to_string(Trigger::ReverseTrigger), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_follow_action),  Trigger::ReverseTrigger, 1)));
	_follow_right.AddMenuElem (MenuElem (follow_action_to_string(Trigger::ForwardTrigger), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_follow_action),  Trigger::ForwardTrigger, 1)));
	_follow_right.AddMenuElem (MenuElem (follow_action_to_string(Trigger::AnyTrigger), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_follow_action),   Trigger::AnyTrigger, 1)));
	_follow_right.AddMenuElem (MenuElem (follow_action_to_string(Trigger::OtherTrigger), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_follow_action), Trigger::OtherTrigger, 1)));
	_follow_right.set_sizing_text (longest_follow);

	_launch_style_button.set_name("FollowAction");
	_launch_style_button.set_sizing_text (longest_launch);
	_launch_style_button.AddMenuElem (MenuElem (launch_style_to_string (Trigger::OneShot), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_launch_style), Trigger::OneShot)));
	_launch_style_button.AddMenuElem (MenuElem (launch_style_to_string (Trigger::ReTrigger), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_launch_style), Trigger::ReTrigger)));
	_launch_style_button.AddMenuElem (MenuElem (launch_style_to_string (Trigger::Gate), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_launch_style), Trigger::Gate)));
	_launch_style_button.AddMenuElem (MenuElem (launch_style_to_string (Trigger::Toggle), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_launch_style), Trigger::Toggle)));
	_launch_style_button.AddMenuElem (MenuElem (launch_style_to_string (Trigger::Repeat), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_launch_style), Trigger::Repeat)));

	_launch_style_button.set_name("FollowAction");
	_legato_button.set_text (_("Legato"));
	_legato_button.signal_event().connect (sigc::mem_fun (*this, (&SlotPropertyTable::legato_button_event)));

#define quantize_item(b) _quantize_button.AddMenuElem (MenuElem (quantize_length_to_string (b), sigc::bind (sigc::mem_fun (*this, &SlotPropertyTable::set_quantize), b)));

#if TRIGGER_PAGE_GLOBAL_QUANTIZATION_IMPLEMENTED
	quantize_item (BBT_Offset (0, 0, 0));
#endif
	quantize_item (BBT_Offset (4, 0, 0));
	quantize_item (BBT_Offset (2, 0, 0));
	quantize_item (BBT_Offset (1, 0, 0));
	quantize_item (BBT_Offset (0, 2, 0));
	quantize_item (BBT_Offset (0, 1, 0));
	quantize_item (BBT_Offset (0, 0, Temporal::ticks_per_beat/2));
	quantize_item (BBT_Offset (0, 0, Temporal::ticks_per_beat/4));
	quantize_item (BBT_Offset (0, 0, Temporal::ticks_per_beat/8));
	quantize_item (BBT_Offset (0, 0, Temporal::ticks_per_beat/16));
	quantize_item (BBT_Offset (-1, 0, 0));

	for (std::vector<std::string>::const_iterator i = quantize_strings.begin(); i != quantize_strings.end(); ++i) {
		if (i->length() > longest_quantize.length()) {
			longest_quantize = *i;
		}
	}
	_quantize_button.set_sizing_text (longest_quantize);
	_quantize_button.set_name("FollowAction");

#undef quantize_item

	_name_label.set_name (X_("TrackNameEditor"));
	_name_label.set_alignment (0.0, 0.5);
	_name_label.set_padding (4, 0);
	_name_label.set_width_chars (12);

	_namebox.add (_name_label);
	_namebox.add_events (Gdk::BUTTON_PRESS_MASK);
	_namebox.signal_button_press_event ().connect (sigc::mem_fun (*this, &SlotPropertyTable::namebox_button_press));

	_name_frame.add (_namebox);
	_name_frame.set_edge_color (0x000000ff);
	_name_frame.set_border_width (0);
	_name_frame.set_padding (0);

	_load_button.set_name("FollowAction");
	_load_button.set_text (_("Load"));
	_load_button.signal_clicked.connect (sigc::bind((sigc::mem_fun (*this, (&TriggerUI::choose_sample))), false));

	_color_button.set_name("FollowAction");
	_color_button.signal_clicked.connect (sigc::mem_fun (*this, (&TriggerUI::choose_color)));

	_follow_size_group  = Gtk::SizeGroup::create (Gtk::SIZE_GROUP_VERTICAL);
	_follow_size_group->add_widget(_name_frame);
	_follow_size_group->add_widget(_load_button);
	_follow_size_group->add_widget(_color_button);
	_follow_size_group->add_widget(_velocity_slider);
	_follow_size_group->add_widget(_follow_count_spinner);

	set_spacings (4);
	set_homogeneous (false);

	int row=0;
	Gtk::Label *label;

	/* ---- Basic trigger properties (name, color) ----- */
	_trigger_table.set_spacings (4);
	_trigger_table.set_homogeneous (false);

	_trigger_table.attach(_name_frame,       0, 2, row, row+1, Gtk::FILL|Gtk::EXPAND, Gtk::SHRINK );
	_trigger_table.attach(_load_button,      2, 3, row, row+1, Gtk::SHRINK,           Gtk::SHRINK );
	_trigger_table.attach(_color_button,     3, 4, row, row+1, Gtk::SHRINK,           Gtk::SHRINK );


	/* ---- Launch settings ----- */
	_launch_table.set_spacings (2);
	_launch_table.set_border_width (8);
	_launch_table.set_homogeneous (false);
	row=0;

	label = manage(new Gtk::Label(_("Velocity Sense:")));  label->set_alignment(1.0, 0.5);
	_launch_table.attach(*label,                 0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK );
	_launch_table.attach(_velocity_slider,       1, 3, row, row+1, Gtk::FILL, Gtk::SHRINK ); row++;

	label = manage(new Gtk::Label(_("Launch Style:")));  label->set_alignment(1.0, 0.5);
	_launch_table.attach(*label,                 0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK );
	_launch_table.attach(_launch_style_button,   1, 3, row, row+1, Gtk::FILL, Gtk::SHRINK ); row++;

	label = manage(new Gtk::Label(_("Launch Quantize:")));  label->set_alignment(1.0, 0.5);
	_launch_table.attach(*label,            0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK );
	_launch_table.attach(_quantize_button,  1, 3, row, row+1, Gtk::FILL, Gtk::SHRINK ); row++;

	label = manage(new Gtk::Label(_("Legato Mode:")));  label->set_alignment(1.0, 0.5);
	_launch_table.attach(*label,          0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK );
	_launch_table.attach(_legato_button,  1, 3, row, row+1, Gtk::FILL, Gtk::SHRINK ); row++;


	/* ---- Follow settings ----- */
	_follow_table.set_spacings (2);
	_follow_table.set_border_width (8);
	_follow_table.set_homogeneous (false);
	row=0;

	Gtkmm2ext::set_size_request_to_display_given_text (_left_probability_label, "100% Left ", 12, 0);
	_left_probability_label.set_alignment(0.0, 0.5);
	Gtkmm2ext::set_size_request_to_display_given_text (_right_probability_label, "100% Right", 12, 0);
	_right_probability_label.set_alignment(1.0, 0.5);

	Gtk::Table *prob_table = manage(new Gtk::Table());
	prob_table->set_spacings(2);
	prob_table->set_border_width(4);
	prob_table->attach(_follow_probability_slider, 0, 2, 0, 1, Gtk::FILL, Gtk::SHRINK );
	prob_table->attach(_left_probability_label,    0, 1, 1, 2, Gtk::FILL,             Gtk::SHRINK );
	prob_table->attach(_right_probability_label,   1, 2, 1, 2, Gtk::FILL,             Gtk::SHRINK );

	_follow_table.attach(_follow_left,   0, 1, row, row+1, Gtk::FILL,             Gtk::SHRINK );
	_follow_table.attach(_follow_right,  1, 3, row, row+1, Gtk::FILL,             Gtk::SHRINK ); row++;
	_follow_table.attach( *prob_table,   0, 3, row, row+1, Gtk::FILL, Gtk::SHRINK ); row++;

	label = manage(new Gtk::Label(_("Follow Count:")));  label->set_alignment(1.0, 0.5);
	_follow_table.attach(*label,          0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK );
	Gtk::Alignment *align = manage (new Gtk::Alignment (0, .5, 0, 0));
	align->add (_follow_count_spinner);
	_follow_table.attach(*align,          1, 2, row, row+1, Gtk::FILL, Gtk::SHRINK, 0, 0 ); row++;

	label = manage(new Gtk::Label(_("Follow Length:")));  label->set_alignment(1.0, 0.5);
	Gtk::Label *beat_label = manage (new Gtk::Label (_("(beats)")));
	beat_label->set_alignment (0.0, 0.5);
	_follow_table.attach(*label,          0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK );
	Gtk::Alignment *fl_align = manage (new Gtk::Alignment (0, .5, 0, 0));
	fl_align->add (_follow_length_spinner);
	_follow_table.attach(*fl_align,       1, 2, row, row+1, Gtk::FILL, Gtk::SHRINK, 0, 0 );
	_follow_table.attach(*beat_label,     2, 3, row, row+1, Gtk::SHRINK, Gtk::SHRINK); row++;

	Gtk::EventBox* eFollowBox = manage (new Gtk::EventBox); // a themeable box
	eFollowBox->set_name("EditorDark");
	eFollowBox->add (_follow_table);

	Gtk::EventBox* eLaunchBox = manage (new Gtk::EventBox); // a themeable box
	eLaunchBox->set_name("EditorDark");
	eLaunchBox->add (_launch_table);

	attach(_trigger_table,  0,1, 0,1, Gtk::FILL, Gtk::SHRINK );
	attach(*eLaunchBox,     0,1, 1,2, Gtk::FILL, Gtk::SHRINK );
	attach(*eFollowBox,     1,2, 1,2, Gtk::FILL, Gtk::SHRINK );
}

SlotPropertyTable::~SlotPropertyTable ()
{
}

void
SlotPropertyTable::set_quantize (BBT_Offset bbo)
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
SlotPropertyTable::follow_length_event ()
{
	int beatz = (int) _follow_length_adjustment.get_value();

	int metrum_numerator = trigger()->meter().divisions_per_bar();

	int bars = beatz/metrum_numerator;
	int beats = beatz%metrum_numerator;

	trigger()->set_follow_length(Temporal::BBT_Offset(bars,beats,0));
}

void
SlotPropertyTable::follow_count_event ()
{
	trigger()->set_follow_count ((int) _follow_count_adjustment.get_value());
}

void
SlotPropertyTable::velocity_adjusted ()
{
	trigger()->set_midi_velocity_effect (_velocity_adjustment.get_value());
}

void
SlotPropertyTable::probability_adjusted ()
{
	trigger()->set_follow_action_probability ((int) _follow_probability_adjustment.get_value());
}

bool
SlotPropertyTable::follow_action_button_event (GdkEvent* ev)
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
SlotPropertyTable::legato_button_event (GdkEvent* ev)
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
SlotPropertyTable::set_launch_style (Trigger::LaunchStyle ls)
{
	trigger()->set_launch_style (ls);
}

void
SlotPropertyTable::set_follow_action (Trigger::FollowAction fa, uint64_t idx)
{
	trigger()->set_follow_action (fa, idx);
}

void
SlotPropertyTable::on_trigger_changed (PropertyChange const& pc)
{
	int probability = trigger()->follow_action_probability();

	if (pc.contains (Properties::name)) {
		_name_label.set_text (trigger()->name());
	}
	if (pc.contains (Properties::color)) {
		_color_button.set_custom_led_color (trigger()->color());
	}
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

	if (pc.contains (Properties::tempo_meter)) {
		int metrum_numerator = trigger()->meter().divisions_per_bar();
		int bar_beats = metrum_numerator * trigger()->follow_length().bars;
		int beats = trigger()->follow_length().beats;
		_follow_length_adjustment.set_value (bar_beats+beats);  //note: 0 is a special case meaning "use clip length"
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
		_follow_probability_adjustment.set_value (probability);
		_left_probability_label.set_text (string_compose(_("%1%% Left"), 100-probability));
		_right_probability_label.set_text (string_compose(_("%1%% Right"), probability));
	}

	bool follow_widgets_sensitive = trigger()->follow_action (0) != Trigger::None;

	if (follow_widgets_sensitive) {
		_follow_right.set_sensitive(true);
		_follow_count_spinner.set_sensitive(true);
		_follow_length_spinner.set_sensitive(true);
		_follow_probability_slider.set_sensitive(true);
	} else {
		_follow_right.set_sensitive(false);
		_follow_count_spinner.set_sensitive(false);
		_follow_length_spinner.set_sensitive(false);
		_follow_probability_slider.set_sensitive(false);
	}
}

/* ------------ */

SlotPropertyWidget::SlotPropertyWidget ()
{
	ui = new SlotPropertyTable ();
	pack_start(*ui);
	ui->show();
}

/* ------------ */

SlotPropertyWindow::SlotPropertyWindow (TriggerReference tref)
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
SlotPropertyWindow::on_key_press_event (GdkEventKey* ev)
{
	Gtk::Window& main_window (ARDOUR_UI::instance()->main_window());
	return ARDOUR_UI_UTILS::relay_key_press (ev, &main_window);
}

bool
SlotPropertyWindow::on_key_release_event (GdkEventKey* ev)
{
	Gtk::Window& main_window (ARDOUR_UI::instance()->main_window());
	return ARDOUR_UI_UTILS::relay_key_press (ev, &main_window);
}
