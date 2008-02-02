#include <gtkmm/stock.h>
#include <gtkmm2ext/utils.h>

#include <pbd/memento_command.h>

#include <ardour/transient_detector.h>
#include <ardour/audiosource.h>
#include <ardour/audioregion.h>
#include <ardour/playlist.h>
#include <ardour/region_factory.h>
#include <ardour/session.h>

#include "rhythm_ferret.h"
#include "audio_region_view.h"
#include "public_editor.h"

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

RhythmFerret::RhythmFerret (PublicEditor& e)
	: ArdourDialog (_("Rhythm Ferret"))
	, editor (e)
	, operation_frame (_("Operation"))
	, selection_frame (_("Selection"))
	, ferret_frame (_("Analysis"))
	, logo (0)
	, region_split_button (operation_button_group, _("Split Region"))
	, tempo_button (operation_button_group, _("Set Tempo Map"))
	, region_conform_button (operation_button_group, _("Conform Region"))
	, analysis_mode_label (_("Mode"))
	, detection_threshold_adjustment (3, 0, 20, 1, 4)
	, detection_threshold_scale (detection_threshold_adjustment)
	, detection_threshold_label (_("Threshold"))
	, sensitivity_adjustment (40, 0, 100, 1, 10)
	, sensitivity_scale (sensitivity_adjustment)
	, sensitivity_label (_("Sensitivity"))
	, analyze_button (_("Analyze"))
	, trigger_gap_adjustment (3, 0, 100, 1, 10)
	, trigger_gap_spinner (trigger_gap_adjustment)
	, trigger_gap_label (_("Trigger gap (msecs)"))
	, action_button (Stock::APPLY)

{
	upper_hpacker.set_spacing (6);

	upper_hpacker.pack_start (operation_frame, true, true);
	upper_hpacker.pack_start (selection_frame, true, true);
	upper_hpacker.pack_start (ferret_frame, true, true);

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

	box = manage (new HBox);
	box->set_spacing (6);
	box->pack_start (analysis_mode_label, false, false);
	box->pack_start (analysis_mode_selector, true, true);
	ferret_packer.pack_start (*box, false, false);

	box = manage (new HBox);
	box->set_spacing (6);
	box->pack_start (detection_threshold_label, false, false);
	box->pack_start (detection_threshold_scale, true, true);
	ferret_packer.pack_start (*box, false, false);

	box = manage (new HBox);
	box->set_spacing (6);
	box->pack_start (sensitivity_label, false, false);
	box->pack_start (sensitivity_scale, true, true);
	ferret_packer.pack_start (*box, false, false);

	box = manage (new HBox);
	box->set_spacing (6);
	box->pack_start (trigger_gap_label, false, false);
	box->pack_start (trigger_gap_spinner, false, false);
	ferret_packer.pack_start (*box, false, false);

	ferret_packer.pack_start (analyze_button, false, false);

	analyze_button.signal_clicked().connect (mem_fun (*this, &RhythmFerret::run_analysis));
	
	ferret_frame.add (ferret_packer);

	// Glib::RefPtr<Pixbuf> logo_pixbuf ("somefile");
	
	if (logo) {
		lower_hpacker.pack_start (*logo, false, false);
	}

	lower_hpacker.pack_start (operation_clarification_label, false, false);
	lower_hpacker.pack_start (action_button, false, false);

	action_button.signal_clicked().connect (mem_fun (*this, &RhythmFerret::do_action));
	
	get_vbox()->set_border_width (6);
	get_vbox()->set_spacing (6);
	get_vbox()->pack_start (upper_hpacker, true, true);
	get_vbox()->pack_start (lower_hpacker, false, false);

	show_all ();
}

RhythmFerret::~RhythmFerret()
{
	if (logo) {
		delete logo;
	}
}

RhythmFerret::AnalysisMode
RhythmFerret::get_analysis_mode () const
{
	string str = analysis_mode_selector.get_active_text ();

	if (str == _(_analysis_mode_strings[(int) NoteOnset])) {
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
		default:
			break;
		}

	}

	for (RegionSelection::iterator i = regions.begin(); i != regions.end(); ++i) {
		(*i)->get_time_axis_view().show_temporary_lines (current_results);
	}

}

int
RhythmFerret::run_percussion_onset_analysis (boost::shared_ptr<Readable> readable, nframes64_t offset, vector<nframes64_t>& results)
{
	TransientDetector t (session->frame_rate());

	for (uint32_t i = 0; i < readable->n_channels(); ++i) {

		vector<nframes64_t> these_results;

		t.reset ();
		t.set_threshold (detection_threshold_adjustment.get_value());
		t.set_sensitivity (sensitivity_adjustment.get_value());

		if (t.run ("", readable.get(), i, these_results)) {
			continue;
		}

		/* translate all transients to give absolute position */

		for (vector<nframes64_t>::iterator i = these_results.begin(); i != these_results.end(); ++i) {
			(*i) += offset;
		}

		/* merge */
		
		results.insert (results.end(), these_results.begin(), these_results.end());
	}
		
	if (!results.empty()) {
		
		/* now resort to bring transients from different channels together */
		
		sort (results.begin(), results.end());

		/* remove duplicates or other things that are too close */

		vector<nframes64_t>::iterator i = results.begin();
		nframes64_t curr = (*i);
		nframes64_t gap_frames = (nframes64_t) floor (trigger_gap_adjustment.get_value() * (session->frame_rate() / 1000.0));

		++i;

		while (i != results.end()) {
			if (((*i) == curr) || (((*i) - curr) < gap_frames)) {
				    i = results.erase (i);
			} else {
				++i;
				curr = *i;
			}
		}

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

		(*i)->get_time_axis_view().hide_temporary_lines ();

		editor.split_region_at_points ((*i)->region(), current_results);

		/* i is invalid at this point */

		i = tmp;
	}

}

void
RhythmFerret::set_session (Session* s)
{
	ArdourDialog::set_session (s);
	current_results.clear ();
}
