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
#include "ardour/session.h"
#ifdef interface
#undef interface
#endif
#include "export_video_infobox.h"
#include "i18n.h"

using namespace Gtk;
using namespace std;
using namespace PBD;
using namespace ARDOUR;

ExportVideoInfobox::ExportVideoInfobox (Session* s)
	: ArdourDialog (_("Video Export Info"))
	, showagain_checkbox (_("Do Not Show This Dialog Again (Reset in Edit > Preferences > Video)."))
{
	set_session (s);

	set_name ("ExportVideoInfobox");
	set_modal (true);
	set_skip_taskbar_hint (true);
	set_resizable (false);

	Gtk::Label* l;
	VBox* vbox = manage (new VBox);

	l = manage (new Label (_("<b>Video Export Info</b>"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	l->set_use_markup ();
	vbox->pack_start (*l, false, true);
	l = manage (new Label (
				string_compose(
				_("Video encoding is a non-trivial task with many details.\n\nPlease see the manual at %1/video-timeline/operations/#export.\n\nOpen Manual in Browser? "),
				Config->get_reference_manual_url()
				), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	l->set_size_request(700,-1);
	l->set_line_wrap();
	vbox->pack_start (*l, false, true,4);

	vbox->pack_start (*(manage (new  HSeparator())), true, true, 2);
	vbox->pack_start (showagain_checkbox, false, true, 2);

	get_vbox()->set_spacing (4);
	get_vbox()->pack_start (*vbox, false, false);

	showagain_checkbox.set_active(false);
	show_all_children ();
	add_button (Stock::YES, RESPONSE_YES);
	add_button (Stock::NO, RESPONSE_NO);
}

ExportVideoInfobox::~ExportVideoInfobox ()
{
}
