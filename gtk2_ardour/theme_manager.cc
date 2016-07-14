/*
    Copyright (C) 2000-2007 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <cmath>
#include <errno.h>

#include "fix_carbon.h"

#include "pbd/gstdio_compat.h"

#include <gtkmm/settings.h>

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/utils.h"

#include "pbd/compose.h"

#include "ardour/profile.h"

#include "canvas/wave_view.h"

#include "ardour_button.h"
#include "ardour_dialog.h"
#include "theme_manager.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace PBD;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;

ThemeManager::ThemeManager()
	: flat_buttons (_("Draw \"flat\" buttons"))
	, blink_rec_button (_("Blink Rec-Arm buttons"))
	, region_color_button (_("Color regions using their track's color"))
	, show_clipping_button (_("Show waveform clipping"))
	, waveform_gradient_depth (0, 1.0, 0.05)
	, waveform_gradient_depth_label (_("Waveforms color gradient depth"))
	, timeline_item_gradient_depth (0, 1.0, 0.05)
	, timeline_item_gradient_depth_label (_("Timeline item gradient depth"))
	, all_dialogs (_("All floating windows are dialogs"))
	, transients_follow_front (_("Transient windows follow front window."))
	, floating_monitor_section (_("Float detached monitor-section window"))
	, icon_set_label (_("Icon Set"))
{
	Gtk::HBox* hbox;

	/* various buttons */

	set_homogeneous (false);


#ifndef __APPLE__
	pack_start (all_dialogs, PACK_SHRINK);
	pack_start (transients_follow_front, PACK_SHRINK);
#endif
	if (!Profile->get_mixbus()) {
		pack_start (floating_monitor_section, PACK_SHRINK);
	}
	pack_start (flat_buttons, PACK_SHRINK);
	pack_start (blink_rec_button, PACK_SHRINK);
	pack_start (region_color_button, PACK_SHRINK);
	pack_start (show_clipping_button, PACK_SHRINK);

	vector<string> icon_sets = ::get_icon_sets ();

	if (icon_sets.size() > 1) {
		Gtkmm2ext::set_popdown_strings (icon_set_dropdown, icon_sets);
		icon_set_dropdown.set_active_text (UIConfiguration::instance().get_icon_set());

		hbox = Gtk::manage (new Gtk::HBox());
		hbox->set_spacing (6);
		Gtk::Alignment* align = Gtk::manage (new Gtk::Alignment);
		align->set (0, 0.5);
		align->add (icon_set_dropdown);
		hbox->pack_start (icon_set_label, false, false);
		hbox->pack_start (*align, true, true);
		pack_start (*hbox, PACK_SHRINK);
	}

	hbox = Gtk::manage (new Gtk::HBox());
	hbox->set_spacing (6);
	hbox->pack_start (waveform_gradient_depth, true, true);
	hbox->pack_start (waveform_gradient_depth_label, false, false);
	pack_start (*hbox, PACK_SHRINK);

	hbox = Gtk::manage (new Gtk::HBox());
	hbox->set_spacing (6);
	hbox->pack_start (timeline_item_gradient_depth, true, true);
	hbox->pack_start (timeline_item_gradient_depth_label, false, false);
	pack_start (*hbox, PACK_SHRINK);

	show_all ();

	waveform_gradient_depth.set_update_policy (Gtk::UPDATE_DELAYED);
	timeline_item_gradient_depth.set_update_policy (Gtk::UPDATE_DELAYED);

	set_ui_to_state();

	flat_buttons.signal_toggled().connect (sigc::mem_fun (*this, &ThemeManager::on_flat_buttons_toggled));
	blink_rec_button.signal_toggled().connect (sigc::mem_fun (*this, &ThemeManager::on_blink_rec_arm_toggled));
	region_color_button.signal_toggled().connect (sigc::mem_fun (*this, &ThemeManager::on_region_color_toggled));
	show_clipping_button.signal_toggled().connect (sigc::mem_fun (*this, &ThemeManager::on_show_clip_toggled));
	waveform_gradient_depth.signal_value_changed().connect (sigc::mem_fun (*this, &ThemeManager::on_waveform_gradient_depth_change));
	timeline_item_gradient_depth.signal_value_changed().connect (sigc::mem_fun (*this, &ThemeManager::on_timeline_item_gradient_depth_change));
	all_dialogs.signal_toggled().connect (sigc::mem_fun (*this, &ThemeManager::on_all_dialogs_toggled));
	transients_follow_front.signal_toggled().connect (sigc::mem_fun (*this, &ThemeManager::on_transients_follow_front_toggled));
	floating_monitor_section.signal_toggled().connect (sigc::mem_fun (*this, &ThemeManager::on_floating_monitor_section_toggled));
	icon_set_dropdown.signal_changed().connect (sigc::mem_fun (*this, &ThemeManager::on_icon_set_changed));

	Gtkmm2ext::UI::instance()->set_tip (all_dialogs,
					    string_compose (_("Mark all floating windows to be type \"Dialog\" rather than using \"Utility\" for some.\n"
							      "This may help with some window managers. This requires a restart of %1 to take effect"),
							    PROGRAM_NAME));
	Gtkmm2ext::UI::instance()->set_tip (transients_follow_front,
					    string_compose (_("Make transient windows follow the front window when toggling between the editor and mixer.\n"
							      "This requires a restart of %1 to take effect"), PROGRAM_NAME));
	Gtkmm2ext::UI::instance()->set_tip (floating_monitor_section,
					    string_compose (_("When detaching the monitoring section, mark it as \"Utility\" window to stay in front.\n"
							      "This requires a restart of %1 to take effect"), PROGRAM_NAME));

}

void
ThemeManager::on_flat_buttons_toggled ()
{
	UIConfiguration::instance().set_flat_buttons (flat_buttons.get_active());
	ArdourButton::set_flat_buttons (flat_buttons.get_active());
	/* force a redraw */
	gtk_rc_reset_styles (gtk_settings_get_default());
}

void
ThemeManager::on_blink_rec_arm_toggled ()
{
	UIConfiguration::instance().set_blink_rec_arm (blink_rec_button.get_active());
	UIConfiguration::instance().ParameterChanged("blink-rec-arm");
}

void
ThemeManager::on_region_color_toggled ()
{
	UIConfiguration::instance().set_color_regions_using_track_color (region_color_button.get_active());
}

void
ThemeManager::on_show_clip_toggled ()
{
	UIConfiguration::instance().set_show_waveform_clipping (show_clipping_button.get_active());
	// "show-waveform-clipping" was a session config key
	ArdourCanvas::WaveView::set_global_show_waveform_clipping (UIConfiguration::instance().get_show_waveform_clipping());
}

void
ThemeManager::on_all_dialogs_toggled ()
{
	UIConfiguration::instance().set_all_floating_windows_are_dialogs (all_dialogs.get_active());
}

void
ThemeManager::on_transients_follow_front_toggled ()
{
	UIConfiguration::instance().set_transients_follow_front (transients_follow_front.get_active());
}

void
ThemeManager::on_floating_monitor_section_toggled ()
{
	UIConfiguration::instance().set_floating_monitor_section (floating_monitor_section.get_active());
}

void
ThemeManager::on_waveform_gradient_depth_change ()
{
	double v = waveform_gradient_depth.get_value();

	UIConfiguration::instance().set_waveform_gradient_depth (v);
	ArdourCanvas::WaveView::set_global_gradient_depth (v);
}

void
ThemeManager::on_timeline_item_gradient_depth_change ()
{
	double v = timeline_item_gradient_depth.get_value();

	UIConfiguration::instance().set_timeline_item_gradient_depth (v);
}

void
ThemeManager::on_icon_set_changed ()
{
	string new_set = icon_set_dropdown.get_active_text();
	UIConfiguration::instance().set_icon_set (new_set);
}

void
ThemeManager::set_ui_to_state()
{
	/* there is no need to block signal handlers, here,
	 * all elements check if the value has changed and ignore NOOPs
	 */
	all_dialogs.set_active (UIConfiguration::instance().get_all_floating_windows_are_dialogs());
	transients_follow_front.set_active (UIConfiguration::instance().get_transients_follow_front());
	floating_monitor_section.set_active (UIConfiguration::instance().get_floating_monitor_section());
	flat_buttons.set_active (UIConfiguration::instance().get_flat_buttons());
	blink_rec_button.set_active (UIConfiguration::instance().get_blink_rec_arm());
	region_color_button.set_active (UIConfiguration::instance().get_color_regions_using_track_color());
	show_clipping_button.set_active (UIConfiguration::instance().get_show_waveform_clipping());
	waveform_gradient_depth.set_value(UIConfiguration::instance().get_waveform_gradient_depth());
	timeline_item_gradient_depth.set_value(UIConfiguration::instance().get_timeline_item_gradient_depth());
}

