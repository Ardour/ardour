/*
    Copyright (C) 2012 Paul Davis 

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

#ifndef __gtk2_ardour_rhythm_ferret_h__
#define __gtk2_ardour_rhythm_ferret_h__

#include <gtkmm/box.h>
#include <gtkmm/scale.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/radiobuttongroup.h>
#include <gtkmm/frame.h>
#include <gtkmm/image.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/button.h>

#include "ardour_dialog.h"
#include "region_selection.h"

namespace ARDOUR {
	class Readable;
}

class Editor;
class RegionView;

class RhythmFerret : public ArdourDialog {
  public:
	/* order of these enums must match the _analyse_mode_strings
	   in rhythm_ferret.cc
	*/
	enum AnalysisMode {
		PercussionOnset,
		NoteOnset
	};

	enum Action {
		SplitRegion,
		SnapRegionsToGrid,
		ConformRegion
	};

	RhythmFerret (Editor&);

	void set_session (ARDOUR::Session*);

  protected:
	void on_hide ();

  private:
	Editor& editor;

	Gtk::ComboBoxText operation_selector;

	Gtk::ComboBoxText analysis_mode_selector;

	/* transient detection widgets */

	Gtk::Adjustment detection_threshold_adjustment;
	Gtk::HScale detection_threshold_scale;
	Gtk::Adjustment sensitivity_adjustment;
	Gtk::HScale sensitivity_scale;
	Gtk::Button analyze_button;

	/* onset detection widgets */

	Gtk::ComboBoxText onset_detection_function_selector;
	Gtk::Adjustment peak_picker_threshold_adjustment;
	Gtk::HScale peak_picker_threshold_scale;
	Gtk::Adjustment silence_threshold_adjustment;
	Gtk::HScale silence_threshold_scale;

	/* generic stuff */

	Gtk::Adjustment trigger_gap_adjustment;
	Gtk::SpinButton trigger_gap_spinner;

	Gtk::Button action_button;

	std::vector<std::string> analysis_mode_strings;
	std::vector<std::string> onset_function_strings;
	std::vector<std::string> operation_strings;

	ARDOUR::AnalysisFeatureList current_results;

	void clear_transients ();
	/** Regions that we have added transient marks to */
	RegionSelection regions_with_transients;

	AnalysisMode get_analysis_mode () const;
	Action get_action() const;
	void analysis_mode_changed ();
	int get_note_onset_function ();

	void run_analysis ();
	int run_percussion_onset_analysis (boost::shared_ptr<ARDOUR::Readable> region, ARDOUR::frameoffset_t offset, ARDOUR::AnalysisFeatureList& results);
	int run_note_onset_analysis (boost::shared_ptr<ARDOUR::Readable> region, ARDOUR::frameoffset_t offset, ARDOUR::AnalysisFeatureList& results);

	void do_action ();
	void do_split_action ();
	void do_region_split (RegionView* rv, const ARDOUR::AnalysisFeatureList&);
};

#endif /* __gtk2_ardour_rhythm_ferret_h__ */
