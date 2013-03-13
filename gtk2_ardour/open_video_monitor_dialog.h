/*
    Copyright (C) 2010 Paul Davis
    Author: Robin Gareus <robin@gareus.org>

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
#ifdef WITH_VIDEOTIMELINE

#ifndef __gtk_ardour_open_video_monitor_dialog_h__
#define __gtk_ardour_open_video_monitor_dialog_h__

#include <string>

#include <gtkmm.h>

#include "ardour/types.h"
#include "ardour/template_utils.h"
#include "ardour_dialog.h"

/** @class OpenVideoMonitorDialog
 *  @brief video-monitor start-option dialog
 *
 * This dialog allows to override xjadeo startup-options
 * eg. restore previous size&position, offset or letterbox
 * settings.
 *
 * This dialog is optional and can be en/disabled in the
 * Preferences.
 */
class OpenVideoMonitorDialog : public ArdourDialog
{
  public:
	OpenVideoMonitorDialog (ARDOUR::Session*);
	~OpenVideoMonitorDialog ();

	bool show_again () { return showagain_checkbox.get_active(); }
	int xj_settings_mask ();
	void setup_settings_mask (const int);
	void set_filename (const std::string);
#if 1
	bool enable_debug () { return debug_checkbox.get_active(); }
#endif

  private:
	void on_show ();
	Gtk::Label filename_label;
	Gtk::CheckButton showagain_checkbox;
	Gtk::CheckButton win_checkbox;
	Gtk::CheckButton att_checkbox;
	Gtk::CheckButton osd_checkbox;
	Gtk::CheckButton off_checkbox;

	Gtk::Label label_winsize;
	Gtk::Label label_winpos;
	Gtk::Label label_letterbox;
	Gtk::Label label_ontop;
	Gtk::Label label_fullscreen;
	Gtk::Label label_osd;
	Gtk::Label label_offset;
#if 1
	Gtk::CheckButton debug_checkbox;
#endif
};

#endif /* __gtk_ardour_open_video_monitor_dialog_h__ */

#endif /* WITH_VIDEOTIMELINE */
