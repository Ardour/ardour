/*
 * Copyright (C) 2022 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <gtkmm/messagedialog.h>
#include <gtkmm/stock.h>

#include <glib.h>

#include "pbd/openuri.h"

#include "ardour/export_channel_configuration.h"
#include "ardour/export_filename.h"
#include "ardour/export_preset.h"
#include "ardour/export_profile_manager.h"
#include "ardour/export_status.h"
#include "ardour/export_timespan.h"
#include "ardour/profile.h"
#include "ardour/session_directory.h"
#include "ardour/surround_return.h"

#include "nag.h"
#include "simple_export_dialog.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;

SimpleExportDialog::SimpleExportDialog (PublicEditor& editor, bool vapor_export)
	: ARDOUR::SimpleExport ()
	, ArdourDialog (vapor_export ? _("Surround Master Export") : _("Quick Audio Export"), true, false)
	, _editor (editor)
	, _eps (true)
	, _vapor_export (vapor_export)
{
	if (_eps.the_combo ().get_parent ()) {
		_eps.the_combo ().get_parent ()->remove (_eps.the_combo ());
	}

	_range_list = ListStore::create (_range_cols);
	_range_combo.set_model (_range_list);
	_range_combo.pack_start (_range_cols.label);

	Table* t = manage (new Table);
	int    r = 0;

	t->set_spacings (4);

#define LBL(TXT) *manage (new Label (_(TXT), ALIGN_END))

	/* clang-format off */
	t->attach (LBL ("Format preset:"),  0, 1, r, r + 1, FILL,          SHRINK, 0, 0);
	if (_vapor_export) {
		t->attach (LBL ("ADM BWF"),       1, 2, r, r + 1, EXPAND,        SHRINK, 0, 0);
	} else {
		t->attach (_eps.the_combo (),     1, 2, r, r + 1, EXPAND | FILL, SHRINK, 0, 0);
	}
	++r;
	t->attach (LBL ("Export range:"),   0, 1, r, r + 1, FILL,          SHRINK, 0, 0);
	t->attach (_range_combo,            1, 2, r, r + 1, EXPAND | FILL, SHRINK, 0, 0);
	++r;
	t->attach (LBL ("After export:"),   0, 1, r, r + 1, FILL,          SHRINK, 0, 0);
	t->attach (_post_export_combo,      1, 2, r, r + 1, EXPAND | FILL, SHRINK, 0, 0);
	++r;
	t->attach (_error_label,            0, 2, r, r + 1, EXPAND | FILL, SHRINK, 0, 0);
	++r;
	t->attach (_progress_bar,           0, 2, r, r + 1, EXPAND | FILL, SHRINK, 0, 0);
	/* clang-format on */

#undef LBL

	_post_export_combo.append (_("Open the folder where files are exported"));
	_post_export_combo.append (_("Do nothing"));
	_post_export_combo.set_active (0);

	get_vbox ()->pack_start (*t, false, false);

	_cancel_button = add_button (Gtk::Stock::CANCEL, RESPONSE_CANCEL);
	_export_button = add_button (_("_Export"), RESPONSE_OK);
	_cancel_button->signal_clicked ().connect (sigc::mem_fun (*this, &SimpleExportDialog::close_dialog));
	_export_button->signal_clicked ().connect (sigc::mem_fun (*this, &SimpleExportDialog::start_export));

	_progress_bar.set_no_show_all (true);
	_error_label.set_no_show_all (true);

	_export_button->set_sensitive (false);
	_range_combo.set_sensitive (false);

	t->show_all ();
}

XMLNode&
SimpleExportDialog::get_state () const
{
	XMLNode* node = new XMLNode (X_("QuickExport"));
	node->set_property (X_("PresetUUID"), preset_uuid ());
	node->set_property (X_("PostExport"), _post_export_combo.get_active_row_number ());
	return *node;
}

void
SimpleExportDialog::set_state (XMLNode const& node)
{
	int         post_export;
	std::string pset_uuid;
	if (node.get_property (X_("PresetUUID"), pset_uuid)) {
		set_preset (pset_uuid);
	}
	if (node.get_property (X_("PostExport"), post_export)) {
		_post_export_combo.set_active (post_export);
	}
}

void
SimpleExportDialog::set_session (ARDOUR::Session* s)
{
	SimpleExport::set_session (s);
	ArdourDialog::set_session (s);

	_range_list->clear ();
	_preset_cfg_connection.disconnect ();

	if (!s) {
		_export_button->set_sensitive (false);
		_range_combo.set_sensitive (false);
		return;
	}

	XMLNode* node = s->extra_xml (X_("QuickExport"));
	if (node) {
		set_state (*node);
	}

	_eps.set_manager (_manager);

	if (!check_outputs ()) {
		set_error ("Error: Session has no master bus");
		return;
	}

	if (_vapor_export && (!s->surround_master () || !s->vapor_export_barrier ())) {
		set_error ("Error: Session has no exportable surround master.");
		return;
	}

	if (_vapor_export && (s->surround_master ()->surround_return ()->total_n_channels () > 128)) {
		set_error ("Error: ADM BWF files cannot contain more than 128 channels.");
		return;
	}

	/* check range */
	Location*            srl (s->locations ()->session_range_location ());
	TimeSelection const& tsel (_editor.get_selection ().time);

	if (!tsel.empty ()) {
		TreeModel::Row row     = *_range_list->append ();
		row[_range_cols.label] = _("Using time selection");
		row[_range_cols.start] = tsel.start_sample ();
		row[_range_cols.end]   = tsel.end_sample ();
		row[_range_cols.name]  = string_compose (_("%1 (selection)"), SimpleExport::_session->snap_name ());
	}

	if (srl) {
		TreeModel::Row row     = *_range_list->append ();
		row[_range_cols.label] = _("Session start to session end"); // same text as ExportVideoDialog::apply_state
		row[_range_cols.start] = srl->start_sample ();
		row[_range_cols.end]   = srl->end_sample ();
		row[_range_cols.name]  = SimpleExport::_session->snap_name ();
	}

	struct LocationSorter {
		bool operator() (Location const* a, Location const* b) {
			return a->start_sample () < b->start_sample ();
		}
	};

	Locations::LocationList ll (s->locations ()->list ());
	ll.sort (LocationSorter ());

	for (auto const& l : ll) {
		if (l->is_session_range () || !l->is_range_marker () || l->name ().empty ()) {
			continue;
		}
		TreeModel::Row row     = *_range_list->append ();
		row[_range_cols.label] = l->name (); // string_compose (_("Range '%1'"), l->name ());
		row[_range_cols.start] = l->start_sample ();
		row[_range_cols.end]   = l->end_sample ();
		row[_range_cols.name]  = string_compose (_("%1 - %2"), SimpleExport::_session->snap_name (), l->name ());
	}

	if (_range_list->children ().size () == 0) {
		set_error ("Error: No valid range to export. Select a range or create session start/end markers");
		return;
	}

	_range_combo.set_active (0);
	_range_combo.set_sensitive (true);
	_export_button->set_sensitive (true);

	_preset_cfg_connection = _eps.CriticalSelectionChanged.connect (sigc::mem_fun (*this, &SimpleExportDialog::check_manager));
}

void
SimpleExportDialog::check_manager ()
{
	bool ok = _manager && _manager->preset ();

	if (ok && _manager->get_formats ().empty ()) {
		ok = false;
	}

	if (ok) {
		/* check for NULL ExportFormatSpecPtr */
		auto fms = _manager->get_formats ();
		for (auto const& fm : fms) {
			if (!fm->format) {
				ok = false;
				break;
			}
		}
	}

	_export_button->set_sensitive (ok);
}

void
SimpleExportDialog::set_error (std::string const& err)
{
	_export_button->set_sensitive (false);
	_range_combo.set_sensitive (false);
	_error_label.set_text (err);
	_error_label.show ();
}

void
SimpleExportDialog::close_dialog ()
{
	if (_status->running ()) {
		_status->abort ();
	}
}

void
SimpleExportDialog::start_export ()
{
	TreeModel::iterator r = _range_combo.get_active ();
	std::string range_name = (*r)[_range_cols.name];
	set_range ((*r)[_range_cols.start], (*r)[_range_cols.end]);
	SimpleExport::set_name (range_name);

	if (_vapor_export) {
		if (range_name.empty ()) {
			range_name = SimpleExport::_session->snap_name ();
		}

		samplepos_t rend = (*r)[_range_cols.end];
		samplepos_t t24h;
		Timecode::Time tc (SimpleExport::_session->timecode_frames_per_second ());
		tc.hours = 24;

		SimpleExport::_session->timecode_to_sample (tc, t24h, false /* use_offset */, false /* use_subframes */);

		if (rend >= t24h) {
			hide ();
			std::string        txt = _("Error: ADM BWF files timecode cannot be past 24h.");
			Gtk::MessageDialog msg (txt, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
			msg.run ();
			return;
		}

		/* C_Ex_08 - prevent export that might fail on some systems - 23.976 vs. 24/1001 */
		switch (SimpleExport::_session->config.get_timecode_format ()) {
			case Timecode::timecode_23976:
			case Timecode::timecode_2997:
			case Timecode::timecode_2997drop:
			case Timecode::timecode_2997000drop:
				tc.hours = 23;
				tc.minutes = 58;
				tc.seconds = 35;
				tc.frames = 0;
				SimpleExport::_session->timecode_to_sample (tc, t24h, false /* use_offset */, false /* use_subframes */);
				if (rend >= t24h) {
					hide ();
					std::string        txt = _("Error: The file to be exported contains an illegal timecode value near the midnight boundary. Try moving the export-range earlier on the product timeline.");
					Gtk::MessageDialog msg (txt, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
					msg.run ();
					return;
				}
				break;
			default:
				break;
		}

		/* Ensure timespan exists, see also SimpleExport::run_export */
		auto ts = _manager->get_timespans ();
		assert (ts.size () == 1);
		assert (ts.front ()->timespans->size () < 2);
		if (ts.front ()->timespans->size () < 1) {
			ExportTimespanPtr timespan = _handler->add_timespan ();
			ts.front ()->timespans->push_back (timespan);
		}

		/* https://professional.dolby.com/siteassets/content-creation/dolby-atmos/dolby_atmos_renderer_guide.pdf
		 * chapter 13.9, page 155 suggests .wav.
		 * There may however already be a .wav file with the given name, so -adm.wav is used.
		 */
		std::string vapor = Glib::build_filename (SimpleExport::_session->session_directory ().export_path (), range_name + "-adm.wav");
		_manager->get_timespans ().front ()->timespans->front ()->set_vapor (vapor);
	}

	SimpleExport::_session->add_extra_xml (get_state ());

	_cancel_button->set_label (_("_Abort"));
	_export_button->set_sensitive (false);
	_progress_bar.set_fraction (0.0);
	_progress_bar.show ();

#if 0
	_eps.hide ();
	_range_combo.hide ();
	_post_export_combo,.hide ();
#endif

	_progress_connection = Glib::signal_timeout ().connect (sigc::mem_fun (*this, &SimpleExportDialog::progress_timeout), 100);

	if (run_export ()) {
		hide ();
		if (_post_export_combo.get_active_row_number () == 0) {
			PBD::open_folder (folder ());
		}
		if (!ARDOUR::Profile->get_mixbus ()) {
			NagScreen* ns = NagScreen::maybe_nag (_("Export"));
			if (ns) {
				ns->nag ();
				delete ns;
			}
		}
	} else if (!_status->aborted ()) {
		hide ();
		std::string        txt = _("Export has been aborted due to an error!\nSee the Log window for details.");
		Gtk::MessageDialog msg (txt, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		msg.run ();
	}
}

bool
SimpleExportDialog::progress_timeout ()
{
	std::string status_text;
	float       progress = -1;

	switch (_status->active_job) {
		case ExportStatus::Exporting:
			status_text = string_compose (_("Exporting '%3' (timespan %1 of %2)"),
			                              _status->timespan, _status->total_timespans, _status->timespan_name);
			progress    = ((float)_status->processed_samples_current_timespan) / _status->total_samples_current_timespan;
			break;
		case ExportStatus::Normalizing:
			status_text = string_compose (_("Normalizing '%3' (timespan %1 of %2)"),
			                              _status->timespan, _status->total_timespans, _status->timespan_name);
			progress    = ((float)_status->current_postprocessing_cycle) / _status->total_postprocessing_cycles;
			break;
		case ExportStatus::Encoding:
			status_text = string_compose (_("Encoding '%3' (timespan %1 of %2)"),
			                              _status->timespan, _status->total_timespans, _status->timespan_name);
			progress    = ((float)_status->current_postprocessing_cycle) / _status->total_postprocessing_cycles;
			break;
		case ExportStatus::Tagging:
			status_text = string_compose (_("Tagging '%3' (timespan %1 of %2)"),
			                              _status->timespan, _status->total_timespans, _status->timespan_name);
		case ExportStatus::Uploading:
			break;
		case ExportStatus::Command:
			status_text = string_compose (_("Running Post-Export Command for '%1'"), _status->timespan_name);
			break;
	}

	_progress_bar.set_text (status_text);

	if (progress >= 0) {
		_progress_bar.set_fraction (progress);
	} else {
		_progress_bar.set_pulse_step (.1);
		_progress_bar.pulse ();
	}

	return true;
}
