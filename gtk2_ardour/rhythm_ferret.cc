#include <gtkmm/stock.h>
#include <gtkmm2ext/utils.h>

#include "pbd/memento_command.h"
#include "pbd/convert.h"

#include "ardour/audioregion.h"
#include "ardour/onset_detector.h"
#include "ardour/session.h"
#include "ardour/transient_detector.h"

#include "rhythm_ferret.h"
#include "audio_region_view.h"
#include "editor.h"
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

static const gchar * _operation_strings[] = {
	N_("Split region"),
	N_("Snap regions"),
	N_("Conform regions"),
	0
};

RhythmFerret::RhythmFerret (Editor& e)
	: ArdourDialog (_("Rhythm Ferret"))
	, editor (e)
	, detection_threshold_adjustment (0.015, 0.0, 0.1, 0.001, 0.1)
	, detection_threshold_scale (detection_threshold_adjustment)
	, sensitivity_adjustment (40, 0, 100, 1, 10)
	, sensitivity_scale (sensitivity_adjustment)
	, analyze_button (_("Analyze"))
	, peak_picker_threshold_adjustment (0.3, 0.0, 1.0, 0.01, 0.1)
	, peak_picker_threshold_scale (peak_picker_threshold_adjustment)
	, silence_threshold_adjustment (-90.0, -120.0, 0.0, 1, 10)
	, silence_threshold_scale (silence_threshold_adjustment)
	, trigger_gap_adjustment (3, 0, 100, 1, 10)
	, trigger_gap_spinner (trigger_gap_adjustment)
	, action_button (Stock::APPLY)
{
	operation_strings = I18N (_operation_strings);
	Gtkmm2ext::set_popdown_strings (operation_selector, operation_strings);
	operation_selector.set_active (0);

	analysis_mode_strings = I18N (_analysis_mode_strings);
	Gtkmm2ext::set_popdown_strings (analysis_mode_selector, analysis_mode_strings);
	analysis_mode_selector.set_active_text (analysis_mode_strings.front());
	analysis_mode_selector.signal_changed().connect (sigc::mem_fun (*this, &RhythmFerret::analysis_mode_changed));

	onset_function_strings = I18N (_onset_function_strings);
	Gtkmm2ext::set_popdown_strings (onset_detection_function_selector, onset_function_strings);
	/* Onset plugin uses complex domain as default function
	   XXX there should be a non-hacky way to set this
	 */
	onset_detection_function_selector.set_active_text (onset_function_strings[3]);
	detection_threshold_scale.set_digits (3);

	Table* t = manage (new Table (7, 3));
	t->set_spacings (12);

	int n = 0;

	t->attach (*manage (new Label (_("Mode"), 1, 0.5)), 0, 1, n, n + 1, FILL);
	t->attach (analysis_mode_selector, 1, 2, n, n + 1, FILL);
	++n;

	t->attach (*manage (new Label (_("Detection function"), 1, 0.5)), 0, 1, n, n + 1, FILL);
	t->attach (onset_detection_function_selector, 1, 2, n, n + 1, FILL);
	++n;

	t->attach (*manage (new Label (_("Trigger gap"), 1, 0.5)), 0, 1, n, n + 1, FILL);
	t->attach (trigger_gap_spinner, 1, 2, n, n + 1, FILL);
	t->attach (*manage (new Label (_("ms"))), 2, 3, n, n + 1, FILL);
	++n;

	t->attach (*manage (new Label (_("Threshold"), 1, 0.5)), 0, 1, n, n + 1, FILL);
	t->attach (detection_threshold_scale, 1, 2, n, n + 1, FILL);
	t->attach (*manage (new Label (_("dB"))), 2, 3, n, n + 1, FILL);
	++n;

	t->attach (*manage (new Label (_("Peak threshold"), 1, 0.5)), 0, 1, n, n + 1, FILL);
	t->attach (peak_picker_threshold_scale, 1, 2, n, n + 1, FILL);
	t->attach (*manage (new Label (_("dB"))), 2, 3, n, n + 1, FILL);
	++n;

	t->attach (*manage (new Label (_("Silence threshold"), 1, 0.5)), 0, 1, n, n + 1, FILL);
	t->attach (silence_threshold_scale, 1, 2, n, n + 1, FILL);
	t->attach (*manage (new Label (_("dB"))), 2, 3, n, n + 1, FILL);
	++n;

	t->attach (*manage (new Label (_("Sensitivity"), 1, 0.5)), 0, 1, n, n + 1, FILL);
	t->attach (sensitivity_scale, 1, 2, n, n + 1, FILL);
	++n;

	t->attach (*manage (new Label (_("Operation"), 1, 0.5)), 0, 1, n, n + 1, FILL);
	t->attach (operation_selector, 1, 2, n, n + 1, FILL);
	++n;

	analyze_button.signal_clicked().connect (sigc::mem_fun (*this, &RhythmFerret::run_analysis));
	action_button.signal_clicked().connect (sigc::mem_fun (*this, &RhythmFerret::do_action));

	get_vbox()->set_border_width (6);
	get_vbox()->set_spacing (6);
	get_vbox()->pack_start (*t);

	add_action_widget (analyze_button, 1);
	add_action_widget (action_button, 0);

	show_all ();
	analysis_mode_changed ();
}

void
RhythmFerret::analysis_mode_changed ()
{
	bool const perc = get_analysis_mode() == PercussionOnset;

	trigger_gap_spinner.set_sensitive (!perc);
	detection_threshold_scale.set_sensitive (perc);
	sensitivity_scale.set_sensitive (perc);
	onset_detection_function_selector.set_sensitive (!perc);
	peak_picker_threshold_scale.set_sensitive (!perc);
	silence_threshold_scale.set_sensitive (!perc);
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
	if (operation_selector.get_active_row_number() == 1) {
		return SnapRegionsToGrid;
	} else if (operation_selector.get_active_row_number() == 2) {
		return ConformRegion;
	}

	return SplitRegion;
}

void
RhythmFerret::run_analysis ()
{
	if (!_session) {
		return;
	}

	clear_transients ();

	regions_with_transients = editor.get_selection().regions;

	current_results.clear ();

	if (regions_with_transients.empty()) {
		return;
	}

	for (RegionSelection::iterator i = regions_with_transients.begin(); i != regions_with_transients.end(); ++i) {

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

		(*i)->region()->set_transients (current_results);
		current_results.clear();
	}
}

int
RhythmFerret::run_percussion_onset_analysis (boost::shared_ptr<Readable> readable, frameoffset_t /*offset*/, AnalysisFeatureList& results)
{
	TransientDetector t (_session->frame_rate());

	for (uint32_t i = 0; i < readable->n_channels(); ++i) {

		AnalysisFeatureList these_results;

		t.reset ();
		t.set_threshold (detection_threshold_adjustment.get_value());
		t.set_sensitivity (sensitivity_adjustment.get_value());

		if (t.run ("", readable.get(), i, these_results)) {
			continue;
		}

		/* merge */

		results.insert (results.end(), these_results.begin(), these_results.end());
		these_results.clear ();

		t.update_positions (readable.get(), i, results);
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
RhythmFerret::run_note_onset_analysis (boost::shared_ptr<Readable> readable, frameoffset_t /*offset*/, AnalysisFeatureList& results)
{
	try {
		OnsetDetector t (_session->frame_rate());

		for (uint32_t i = 0; i < readable->n_channels(); ++i) {

			AnalysisFeatureList these_results;

			t.reset ();

			t.set_function (get_note_onset_function());
			t.set_silence_threshold (silence_threshold_adjustment.get_value());
			t.set_peak_threshold (peak_picker_threshold_adjustment.get_value());

			if (t.run ("", readable.get(), i, these_results)) {
				continue;
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
		OnsetDetector::cleanup_onsets (results, _session->frame_rate(), trigger_gap_adjustment.get_value());
	}

	return 0;
}

void
RhythmFerret::do_action ()
{
	if (!_session) {
		return;
	}

	switch (get_action()) {
	case SplitRegion:
		do_split_action ();
		break;
	case SnapRegionsToGrid:
		editor.snap_regions_to_grid();
		break;
	case ConformRegion:
		editor.close_region_gaps();
		break;
	default:
		break;
	}
}

void
RhythmFerret::do_split_action ()
{
	/* XXX: this is quite a special-case; (currently) the only operation which is
	   performed on the selection only (without entered_regionview or the edit point
	   being considered)
	*/
	RegionSelection regions = editor.get_regions_from_selection();

	if (regions.empty()) {
		return;
	}

	editor.EditorFreeze(); /* Emit signal */

	_session->begin_reversible_command (_("split regions (rhythm ferret)"));

	/* Merge the transient positions for regions in consideration */
	AnalysisFeatureList merged_features;

	for (RegionSelection::iterator i = regions.begin(); i != regions.end(); ++i) {

		AnalysisFeatureList features;
		features = (*i)->region()->transients();

		merged_features.insert (merged_features.end(), features.begin(), features.end());
	}

	merged_features.sort();
	merged_features.unique();

	for (RegionSelection::iterator i = regions.begin(); i != regions.end(); ) {

		RegionSelection::iterator tmp;

		tmp = i;
		++tmp;

		editor.split_region_at_points ((*i)->region(), merged_features, false, false);

		/* i is invalid at this point */
		i = tmp;
	}

	_session->commit_reversible_command ();

	editor.EditorThaw(); /* Emit signal */
}

void
RhythmFerret::set_session (Session* s)
{
	ArdourDialog::set_session (s);
	current_results.clear ();
}

void
RhythmFerret::on_hide ()
{
	ArdourDialog::on_hide ();
	clear_transients ();
}

/* Clear any transients that we have added */
void
RhythmFerret::clear_transients ()
{
	current_results.clear ();

	for (RegionSelection::iterator i = regions_with_transients.begin(); i != regions_with_transients.end(); ++i) {
		(*i)->region()->set_transients (current_results);
	}

	regions_with_transients.clear ();
}

