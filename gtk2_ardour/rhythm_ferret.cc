#include <gtkmm/stock.h>
#include <gtkmm2ext/utils.h>

#include "pbd/memento_command.h"

#include "ardour/transient_detector.h"
#include "ardour/onset_detector.h"
#include "ardour/audiosource.h"
#include "ardour/audioregion.h"
#include "ardour/playlist.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"

#include "rhythm_ferret.h"
#include "audio_region_view.h"
#include "public_editor.h"
#include "utils.h"
#include "time_axis_view.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace PBD;
using namespace ARDOUR;

/* order of these must match the AnalysisMode enums
   in rhythm_ferret.h
*/
static const gchar * _analysis_mode_strings[] = {
	N_("Percussive Onset"),
	N_("Note Onset"),
	0
};

static const gchar * _onset_function_strings[] = {
	N_("Energy Based"),
	N_("Spectral Difference"),
	N_("High-Frequency Content"),
	N_("Complex Domain"),
	N_("Phase Deviation"),
	N_("Kullback-Liebler"),
	N_("Modified Kullback-Liebler"),
	0
};

RhythmFerret::RhythmFerret (PublicEditor& e)
	: ArdourDialog (_("Rhythm Ferret"))
	, editor (e)
	, operation_frame (_("Operation"))
	, selection_frame (_("Selection"))
	, ferret_frame (_("Analysis"))
	, logo (0)
	, region_split_button (operation_button_group, _("Split region"))
	, tempo_button (operation_button_group, _("Set tempo map"))
	, region_conform_button (operation_button_group, _("Conform region"))
	, analysis_mode_label (_("Mode"))
	, detection_threshold_adjustment (3, 0, 20, 1, 4)
	, detection_threshold_scale (detection_threshold_adjustment)
	, detection_threshold_label (_("Threshold"))
	, sensitivity_adjustment (40, 0, 100, 1, 10)
	, sensitivity_scale (sensitivity_adjustment)
	, sensitivity_label (_("Sensitivity"))
	, analyze_button (_("Analyze"))
	, onset_function_label (_("Detection function"))
	, peak_picker_threshold_adjustment (0.3, 0.0, 1.0, 0.01, 0.1)
	, peak_picker_threshold_scale (peak_picker_threshold_adjustment)
	, peak_picker_label (_("Peak Threshold"))
	, silence_threshold_adjustment (-90.0, -120.0, 0.0, 1, 10)
	, silence_threshold_scale (silence_threshold_adjustment)
	, silence_label (_("Silent Threshold (dB)"))
	, trigger_gap_adjustment (3, 0, 100, 1, 10)
	, trigger_gap_spinner (trigger_gap_adjustment)
	, trigger_gap_label (_("Trigger gap (msecs)"))
	, action_button (Stock::APPLY)

{
	upper_hpacker.set_spacing (6);

	upper_hpacker.pack_start (ferret_frame, true, true);
	upper_hpacker.pack_start (selection_frame, true, true);
	upper_hpacker.pack_start (operation_frame, true, true);

	op_packer.pack_start (region_split_button, false, false);
	op_packer.pack_start (tempo_button, false, false);
	op_packer.pack_start (region_conform_button, false, false);

	operation_frame.add (op_packer);

	HBox* box;

	ferret_packer.set_spacing (6);
	ferret_packer.set_border_width (6);
	
	vector<string> strings;

	analysis_mode_strings = I18N (_analysis_mode_strings);
	Gtkmm2ext::set_popdown_strings (analysis_mode_selector, analysis_mode_strings);
	analysis_mode_selector.set_active_text (analysis_mode_strings.front());
	analysis_mode_selector.signal_changed().connect (mem_fun (*this, &RhythmFerret::analysis_mode_changed));

	onset_function_strings = I18N (_onset_function_strings);
	Gtkmm2ext::set_popdown_strings (onset_detection_function_selector, onset_function_strings);
	/* Onset plugin uses complex domain as default function 
	   XXX there should be a non-hacky way to set this
	 */
	onset_detection_function_selector.set_active_text (onset_function_strings[3]);

	box = manage (new HBox);
	box->set_spacing (6);
	box->pack_start (analysis_mode_label, false, false);
	box->pack_start (analysis_mode_selector, true, true);
	ferret_packer.pack_start (*box, false, false);

	ferret_packer.pack_start (analysis_packer, false, false);

	box = manage (new HBox);
	box->set_spacing (6);
	box->pack_start (trigger_gap_label, false, false);
	box->pack_start (trigger_gap_spinner, false, false);
	ferret_packer.pack_start (*box, false, false);

	ferret_packer.pack_start (analyze_button, false, false);

	analyze_button.signal_clicked().connect (mem_fun (*this, &RhythmFerret::run_analysis));
	
	box = manage (new HBox);
	box->set_spacing (6);
	box->pack_start (detection_threshold_label, false, false);
	box->pack_start (detection_threshold_scale, true, true);
	perc_onset_packer.pack_start (*box, false, false);
		
	box = manage (new HBox);
	box->set_spacing (6);
	box->pack_start (sensitivity_label, false, false);
	box->pack_start (sensitivity_scale, true, true);
	perc_onset_packer.pack_start (*box, false, false);

	box = manage (new HBox);
	box->set_spacing (6);
	box->pack_start (onset_function_label, false, false);
	box->pack_start (onset_detection_function_selector, true, true);
	note_onset_packer.pack_start (*box, false, false);
		
	box = manage (new HBox);
	box->set_spacing (6);
	box->pack_start (peak_picker_label, false, false);
	box->pack_start (peak_picker_threshold_scale, true, true);
	note_onset_packer.pack_start (*box, false, false);
	
	box = manage (new HBox);
	box->set_spacing (6);
	box->pack_start (silence_label, false, false);
	box->pack_start (silence_threshold_scale, true, true);
	note_onset_packer.pack_start (*box, false, false);

	analysis_mode_changed ();

	ferret_frame.add (ferret_packer);
	
	logo = manage (new Gtk::Image (::get_icon (X_("ferret_02"))));

	if (logo) {
		lower_hpacker.pack_start (*logo, false, false);
	}

	lower_hpacker.pack_start (operation_clarification_label, true, true);
	lower_hpacker.pack_start (action_button, false, false);
	lower_hpacker.set_border_width (6);
	lower_hpacker.set_spacing (6);

	action_button.signal_clicked().connect (mem_fun (*this, &RhythmFerret::do_action));
	
	get_vbox()->set_border_width (6);
	get_vbox()->set_spacing (6);
	get_vbox()->pack_start (upper_hpacker, true, true);
	get_vbox()->pack_start (lower_hpacker, false, false);

	show_all ();
}

RhythmFerret::~RhythmFerret()
{
	delete logo;
}

void
RhythmFerret::analysis_mode_changed ()
{
	analysis_packer.children().clear ();

	switch (get_analysis_mode()) {
	case PercussionOnset:
		analysis_packer.pack_start (perc_onset_packer);
		break;

	case NoteOnset:
		analysis_packer.pack_start (note_onset_packer);
		break;
	}

	analysis_packer.show_all ();
}

RhythmFerret::AnalysisMode
RhythmFerret::get_analysis_mode () const
{
	string str = analysis_mode_selector.get_active_text ();

	if (str == analysis_mode_strings[(int) NoteOnset]) {
		return NoteOnset;
	} 

	return PercussionOnset;
}

RhythmFerret::Action
RhythmFerret::get_action () const
{
	if (tempo_button.get_active()) {
		return DefineTempoMap;
	} else if (region_conform_button.get_active()) {
		return ConformRegion;
	}

	return SplitRegion;
}

void
RhythmFerret::run_analysis ()
{
	if (!session) {
		return;
	}

	RegionSelection& regions (editor.get_selection().regions);

	current_results.clear ();

	if (regions.empty()) {
		return;
	}

	for (RegionSelection::iterator i = regions.begin(); i != regions.end(); ++i) {

		boost::shared_ptr<Readable> rd = boost::static_pointer_cast<AudioRegion> ((*i)->region());

		switch (get_analysis_mode()) {
		case PercussionOnset:
			run_percussion_onset_analysis (rd, (*i)->region()->position(), current_results);
			break;
		case NoteOnset:
			run_note_onset_analysis (rd, (*i)->region()->position(), current_results);
			break;
		default:
			break;
		}

	}

	for (RegionSelection::iterator i = regions.begin(); i != regions.end(); ++i) {
		(*i)->get_time_axis_view()->show_feature_lines (current_results);
	}

}

int
RhythmFerret::run_percussion_onset_analysis (boost::shared_ptr<Readable> readable, nframes64_t offset, AnalysisFeatureList& results)
{
	TransientDetector t (session->frame_rate());

	for (uint32_t i = 0; i < readable->n_channels(); ++i) {

		AnalysisFeatureList these_results;

		t.reset ();
		t.set_threshold (detection_threshold_adjustment.get_value());
		t.set_sensitivity (sensitivity_adjustment.get_value());

		if (t.run ("", readable.get(), i, these_results)) {
			continue;
		}

		/* translate all transients to give absolute position */

		for (AnalysisFeatureList::iterator x = these_results.begin(); x != these_results.end(); ++x) {
			(*x) += offset;
		}

		/* merge */
		
		results.insert (results.end(), these_results.begin(), these_results.end());
		these_results.clear ();
	}

	if (!results.empty()) {
		TransientDetector::cleanup_transients (results, session->frame_rate(), trigger_gap_adjustment.get_value());
	}

	return 0;
}

int
RhythmFerret::get_note_onset_function ()
{
	string txt = onset_detection_function_selector.get_active_text();

	for (int n = 0; _onset_function_strings[n]; ++n) {
		/* compare translated versions */
		if (txt == onset_function_strings[n]) {
			return n;
		}
	}
	fatal << string_compose (_("programming error: %1 (%2)"), X_("illegal note onset function string"), txt)
	      << endmsg;
	/*NOTREACHED*/
	return -1;
}

int
RhythmFerret::run_note_onset_analysis (boost::shared_ptr<Readable> readable, nframes64_t offset, AnalysisFeatureList& results)
{
	try {
		OnsetDetector t (session->frame_rate());
		
		for (uint32_t i = 0; i < readable->n_channels(); ++i) {
			
			AnalysisFeatureList these_results;
			
			t.reset ();
			
			t.set_function (get_note_onset_function());
			t.set_silence_threshold (silence_threshold_adjustment.get_value());
			t.set_peak_threshold (peak_picker_threshold_adjustment.get_value());
			
			if (t.run ("", readable.get(), i, these_results)) {
				continue;
			}
			
			/* translate all transients to give absolute position */
			
			for (AnalysisFeatureList::iterator x = these_results.begin(); x != these_results.end(); ++x) {
				(*x) += offset;
			}
			
			/* merge */
			
			results.insert (results.end(), these_results.begin(), these_results.end());
			these_results.clear ();
		}

	} catch (failed_constructor& err) {
		error << "Could not load note onset detection plugin" << endmsg;
		return -1;
	}

	if (!results.empty()) {
		OnsetDetector::cleanup_onsets (results, session->frame_rate(), trigger_gap_adjustment.get_value());
	}

	return 0;
}

void
RhythmFerret::do_action ()
{
	if (!session || current_results.empty()) {
		return;
	}

	switch (get_action()) {
	case SplitRegion:
		do_split_action ();
		break;

	default:
		break;
	}
}

void
RhythmFerret::do_split_action ()
{
	/* this can/will change the current selection, so work with a copy */

	RegionSelection& regions (editor.get_selection().regions);

	if (regions.empty()) {
		return;
	}

	session->begin_reversible_command (_("split regions (rhythm ferret)"));

	for (RegionSelection::iterator i = regions.begin(); i != regions.end(); ) {

		RegionSelection::iterator tmp;

		tmp = i;
		++tmp;

		(*i)->get_time_axis_view()->hide_feature_lines ();

		editor.split_region_at_points ((*i)->region(), current_results, false);

		/* i is invalid at this point */

		i = tmp;
	}
	
	session->commit_reversible_command ();
}

void
RhythmFerret::set_session (Session* s)
{
	ArdourDialog::set_session (s);
	current_results.clear ();
}

static void hide_time_axis_features (TimeAxisViewPtr tav)
{
	tav->hide_feature_lines ();
}

void
RhythmFerret::on_hide ()
{
	editor.foreach_time_axis_view (sigc::ptr_fun (hide_time_axis_features));
	ArdourDialog::on_hide ();
}

