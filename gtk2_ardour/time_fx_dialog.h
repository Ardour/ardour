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

#ifndef __ardour_time_fx_dialog_h__
#define __ardour_time_fx_dialog_h__

#include <gtkmm.h>

#include "ardour/playlist.h"
#include "ardour/timefx_request.h"

#include "ardour_dialog.h"
#include "region_selection.h"
#include "progress_reporter.h"

class Editor;

class TimeFXDialog : public ArdourDialog, public ProgressReporter
{
public:
    ARDOUR::TimeFXRequest request;
    Editor&               editor;
    bool                  pitching;
    Gtk::Adjustment       pitch_octave_adjustment;
    Gtk::Adjustment       pitch_semitone_adjustment;
    Gtk::Adjustment       pitch_cent_adjustment;
    Gtk::SpinButton       pitch_octave_spinner;
    Gtk::SpinButton       pitch_semitone_spinner;
    Gtk::SpinButton       pitch_cent_spinner;
    Gtk::ProgressBar      progress_bar;
    ARDOUR::RegionList    regions;

    /* SoundTouch */
    Gtk::CheckButton      quick_button;
    Gtk::CheckButton      antialias_button;
    Gtk::VBox             upper_button_box;

    /* RubberBand */
    Gtk::ComboBoxText     stretch_opts_selector;
    Gtk::Label            stretch_opts_label;
    Gtk::CheckButton      precise_button;
    Gtk::CheckButton      preserve_formants_button;

    Gtk::Button*          cancel_button;
    Gtk::Button*          action_button;
    Gtk::VBox             packer;
    int                   status;

    TimeFXDialog (Editor& e, bool for_pitch);

    sigc::connection first_cancel;
    sigc::connection first_delete;
    void cancel_in_progress ();
    gint delete_in_progress (GdkEventAny*);

private:
	
    void update_progress_gui (float);
};

#endif /* __ardour_time_fx_dialog_h__ */
