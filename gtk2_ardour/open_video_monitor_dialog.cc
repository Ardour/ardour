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

#include <cstdio>
#include <cmath>

#include <sigc++/bind.h>

#include "pbd/file_utils.h"
#include "pbd/error.h"
#include "pbd/convert.h"
#include "gtkmm2ext/utils.h"
#include "ardour/profile.h"
#include "ardour/template_utils.h"
#include "ardour/session.h"
#include "ardour_ui.h"

#include "utils.h"
#include "add_video_dialog.h"
#include "video_monitor.h"
#include "i18n.h"

using namespace Gtk;
using namespace std;
using namespace PBD;
using namespace ARDOUR;

OpenVideoMonitorDialog::OpenVideoMonitorDialog (Session* s)
	: ArdourDialog (_("Open Video Monitor"))
	, filename_label ()
	, showagain_checkbox (_("Don't show this dialog again. (Reset in Edit->Preferences)."))
	, win_checkbox (_("Restore last window size and position."))
	, att_checkbox (_("Restore Window Attributes (fullscreen, on-top)."))
	, osd_checkbox (_("Restore On-Screen-Display settings."))
	, off_checkbox (_("Restore Time Offset."))
	, label_winsize ()
	, label_winpos ()
	, label_letterbox ()
	, label_ontop ()
	, label_fullscreen ()
	, label_osd ()
	, label_offset ()
#if 1
	, debug_checkbox (_("Enable Debug Mode: Dump Communication to stdout."))
#endif
{
	set_session (s);

	set_name ("OpenVideoMonitorDialog");
	set_position (Gtk::WIN_POS_MOUSE);
	set_modal (true);
	set_skip_taskbar_hint (true);
	set_resizable (false);

	Gtk::Label* l;
	VBox* vbox = manage (new VBox);
	VBox* options_box = manage (new VBox);

	l = manage (new Label (_("<b>Video Monitor Window</b>"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	l->set_use_markup ();
	vbox->pack_start (*l, false, true);
	l = manage (new Label (_("The video monitor state can restored to the last known settings for this session. To modify the settings, interact with the monitor itself: Move its window or focus it and use keyboard shortcuts (or the OSX menu bar). Consult the xjadeo documentation for available keyboard shortcuts."), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	l->set_size_request(550,-1);
	l->set_line_wrap();
	vbox->pack_start (*l, false, true,4);

	l = manage (new Label (_("<b>Open Video file:</b>"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	l->set_use_markup ();
	vbox->pack_start (*l, false, true, 4);
	vbox->pack_start (filename_label, false, false);

	l = manage (new Label (_("<b>Session Options</b>"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	l->set_use_markup ();
	options_box->pack_start (*l, false, true, 4);

	options_box->pack_start (win_checkbox, false, true, 2);
	options_box->pack_start (label_winpos, false, false, 2);
	options_box->pack_start (label_winsize, false, false, 2);
	options_box->pack_start (label_letterbox, false, false, 2);

	options_box->pack_start (att_checkbox, false, true, 2);
	options_box->pack_start (label_fullscreen, false, false, 2);
	options_box->pack_start (label_ontop, false, false, 2);

	options_box->pack_start (osd_checkbox, false, true, 2);
	options_box->pack_start (label_osd, false, false, 2);

	options_box->pack_start (off_checkbox, false, true, 2);
	options_box->pack_start (label_offset, false, false, 2);

	options_box->pack_start (*(manage (new  HSeparator())), true, true, 2);
	options_box->pack_start (showagain_checkbox, false, true, 2);

#if 1
	options_box->pack_start (debug_checkbox, false, true, 2);
	debug_checkbox.set_active(false);
#endif

	vbox->pack_start (*options_box, false, true);

	get_vbox()->set_spacing (4);
	get_vbox()->pack_start (*vbox, false, false);

	showagain_checkbox.set_active(false);
	show_all_children ();
	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::OPEN, RESPONSE_ACCEPT);
}

OpenVideoMonitorDialog::~OpenVideoMonitorDialog ()
{
}

void
OpenVideoMonitorDialog::on_show ()
{
	label_offset.set_text(string_compose(_("Offset: %1 Video frame(s)"), "-"));
	label_osd.set_text(string_compose(_("On-Screen-Display: %1"), "-"));
	label_letterbox.set_text(string_compose(_("Letterbox: %1"), "-"));
	label_winsize.set_text(string_compose(_("Size: %1"), "-"));
	label_winpos.set_text(string_compose(_("Position: %1"), "-"));
	label_fullscreen.set_text(string_compose(_("Fullscreen: %1"), "-"));
	label_ontop.set_text(string_compose(_("Window on Top: %1"), "-"));

#define L_YESNO(v) (atoi(v)?_("Yes"):_("No"))
#define L_OSDMODE(i) ( std::string((i)? "":_("(Off)")) \
		                  +std::string((i&1)?_("Frame Number "):"") \
                      +std::string((i&2)?_("SMPTE "):"") \
                      +std::string((i&4)?_("Text "):"") \
                      +std::string((i&8)?_("Box "):"") )

	XMLNode* node = _session->extra_xml (X_("XJSettings"));
	if (node) {
		XMLNodeList nlist = node->children();
		XMLNodeConstIterator niter;
		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
			std::string k = (*niter)->property(X_("k"))->value();
			std::string v = (*niter)->property(X_("v"))->value();
			if (k == "osd mode") { label_osd.set_text(string_compose(_("On-Screen-Display: %1"), L_OSDMODE(atoi(v)))); }
			if (k == "window letterbox") { label_letterbox.set_text(string_compose(_("Letterbox: %1"), L_YESNO(v))); }
			if (k == "window xy") { label_winpos.set_text(string_compose(_("Position: %1"), v)); }
			if (k == "window ontop") { label_ontop.set_text(string_compose(_("Window On Top: %1"), L_YESNO(v))); }
			if (k == "window zoom") { label_fullscreen.set_text(string_compose(_("Fullscreen: %1"), L_YESNO(v))); }
			if (k == "window size") { label_winsize.set_text(string_compose(_("Size: %1"), v)); }
			if (k == "set offset") { label_offset.set_text(string_compose(_("Offset: %1 video-frame(s)"), v)); }
		}
	}

	Dialog::on_show ();
}

int
OpenVideoMonitorDialog::xj_settings_mask ()
{
	int rv =0;
	if (!win_checkbox.get_active()) { rv |= XJ_WINDOW_SIZE | XJ_WINDOW_POS | XJ_LETTERBOX; }
	if (!att_checkbox.get_active()) { rv |= XJ_WINDOW_ONTOP | XJ_FULLSCREEN; }
	if (!osd_checkbox.get_active()) { rv |= XJ_OSD; }
	if (!off_checkbox.get_active()) { rv |= XJ_OFFSET; }
	return rv;
}

void
OpenVideoMonitorDialog::set_filename (const std::string fn)
{
  filename_label.set_text(fn);
}

void
OpenVideoMonitorDialog::setup_settings_mask (const int f)
{
	win_checkbox.set_active((f&XJ_WINDOW_SIZE) == 0);
	att_checkbox.set_active((f&XJ_WINDOW_ONTOP) == 0);
	osd_checkbox.set_active((f&XJ_OSD) == 0);
	off_checkbox.set_active((f&XJ_OFFSET) == 0);
}

#endif /* WITH_VIDEOTIMELINE */
