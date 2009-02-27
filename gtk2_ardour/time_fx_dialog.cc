/*
    Copyright (C) 2000-2009 Paul Davis 

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

#include "time_fx_dialog.h"

#include <iostream>
#include <cstdlib>
#include <cmath>

#include <string>

#include "pbd/error.h"
#include "pbd/pthread_utils.h"
#include "pbd/memento_command.h"

#include <gtkmm2ext/window_title.h>
#include <gtkmm2ext/utils.h>

#include "editor.h"
#include "audio_time_axis.h"
#include "audio_region_view.h"
#include "region_selection.h"

#include "ardour/session.h"
#include "ardour/region.h"
#include "ardour/audioplaylist.h"
#include "ardour/audio_track.h"
#include "ardour/audioregion.h"
#include "ardour/audio_diskstream.h"
#include "ardour/stretch.h"
#include "ardour/midi_stretch.h"
#include "ardour/pitch.h"

#ifdef USE_RUBBERBAND
#include "rubberband/RubberBandStretcher.h"
using namespace RubberBand;
#endif

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace sigc;
using namespace Gtk;
using namespace Gtkmm2ext;

TimeFXDialog::TimeFXDialog (Editor& e, bool pitch)
	: ArdourDialog (X_("time fx dialog"))
	, editor (e)
	, pitching (pitch)
	, pitch_octave_adjustment (0.0, -4.0, 4.0, 1, 2.0)
	, pitch_semitone_adjustment (0.0, -12.0, 12.0, 1.0, 4.0)
	, pitch_cent_adjustment (0.0, -499.0, 500.0, 5.0, 15.0)
	, pitch_octave_spinner (pitch_octave_adjustment)
	, pitch_semitone_spinner (pitch_semitone_adjustment)
	, pitch_cent_spinner (pitch_cent_adjustment)
	, quick_button (_("Quick but Ugly"))
	, antialias_button (_("Skip Anti-aliasing"))
	, stretch_opts_label (_("Contents:"))
	, precise_button (_("Strict Linear"))
	, preserve_formants_button(_("Preserve Formants"))
{
	set_modal (true);
	set_position (Gtk::WIN_POS_MOUSE);
	set_name (N_("TimeFXDialog"));

	WindowTitle title(Glib::get_application_name());
	if (pitching) {
		title += _("Pitch Shift");
	} else {
		title += _("Time Stretch");
	}
	set_title(title.get_string());

	cancel_button = add_button (_("Cancel"), Gtk::RESPONSE_CANCEL);

	get_vbox()->set_spacing (5);
	get_vbox()->set_border_width (12);

	if (pitching) {

		upper_button_box.set_spacing (5);
		upper_button_box.set_border_width (5);
		
		Gtk::Label* l;

		l = manage (new Label (_("Octaves")));
		upper_button_box.pack_start (*l, false, false);
		upper_button_box.pack_start (pitch_octave_spinner, false, false);

		l = manage (new Label (_("Semitones (12TET)")));
		upper_button_box.pack_start (*l, false, false);
		upper_button_box.pack_start (pitch_semitone_spinner, false, false);

		l = manage (new Label (_("Cents")));
		upper_button_box.pack_start (*l, false, false);
		upper_button_box.pack_start (pitch_cent_spinner, false, false);

		pitch_cent_spinner.set_digits (1);

		upper_button_box.pack_start (preserve_formants_button, false, false);


		add_button (_("Shift"), Gtk::RESPONSE_ACCEPT);

		get_vbox()->pack_start (upper_button_box, false, false);

	} else {

#ifdef USE_RUBBERBAND
		opts_box.set_spacing (5);
		opts_box.set_border_width (5);
		vector<string> strings;

		set_popdown_strings (stretch_opts_selector, editor.rb_opt_strings);
		/* set default */
		stretch_opts_selector.set_active_text (editor.rb_opt_strings[4]);

		opts_box.pack_start (precise_button, false, false);
		opts_box.pack_start (stretch_opts_label, false, false);
		opts_box.pack_start (stretch_opts_selector, false, false);

		get_vbox()->pack_start (opts_box, false, false);

#else
		upper_button_box.set_homogeneous (true);
		upper_button_box.set_spacing (5);
		upper_button_box.set_border_width (5);

		upper_button_box.pack_start (quick_button, true, true);
		upper_button_box.pack_start (antialias_button, true, true);

		quick_button.set_name (N_("TimeFXButton"));
		antialias_button.set_name (N_("TimeFXButton"));

		get_vbox()->pack_start (upper_button_box, false, false);

#endif	
		add_button (_("Stretch/Shrink"), Gtk::RESPONSE_ACCEPT);
	}

	get_vbox()->pack_start (progress_bar);

	progress_bar.set_name (N_("TimeFXProgress"));

	show_all_children ();
}

gint
TimeFXDialog::update_progress ()
{
	progress_bar.set_fraction (request.progress);
	return !request.done;
}

void
TimeFXDialog::cancel_in_progress ()
{
	status = -2;
	request.cancel = true;
	first_cancel.disconnect();
}

gint
TimeFXDialog::delete_in_progress (GdkEventAny* ev)
{
	status = -2;
	request.cancel = true;
	first_delete.disconnect();
	return TRUE;
}

