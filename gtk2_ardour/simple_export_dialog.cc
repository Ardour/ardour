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

#include "nag.h"
#include "simple_export_dialog.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;

SimpleExport::SimpleExport (PublicEditor& editor)
	: _editor (editor)
	, _pset_id ("df340c53-88b5-4342-a1c8-58e0704872ea" /* CD */)
	, _start (0)
	, _end (0)
{
}

void
SimpleExport::set_session (ARDOUR::Session* s)
{
	SessionHandlePtr::set_session (s);
	if (!s) {
		_manager.reset ();
		return;
	}

	_handler = _session->get_export_handler ();
	_status  = _session->get_export_status ();

	/* create manager, by default it is preconfigured to
	 * - one Timespan (session-range, if set, otherwise empty)
	 * - one ChannelConfig (master-bus, IFF the session as a master)
	 */
	_manager.reset (new ExportProfileManager (*_session, ExportProfileManager::RangeExport));

	/* set formats(s) and export-filename */
	set_preset (_pset_id);
}

void
SimpleExport::set_name (std::string const& name)
{
	_name = name;
}

void
SimpleExport::set_folder (std::string const& folder)
{
	_folder = folder;
	if (!_folder.empty ()) {
		g_mkdir_with_parents (_folder.c_str (), 0755);
	}
}

void
SimpleExport::set_range (samplepos_t start, samplepos_t end)
{
	_start = start;
	_end   = end;
}

bool
SimpleExport::set_preset (std::string const& pset_uuid)
{
	if (!_manager) {
		return false;
	}

	ExportProfileManager::PresetList const& psets (_manager->get_presets ());
	assert (psets.size () > 0);
	bool rv = false;

	ExportPresetPtr epp = psets.front ();
	for (auto const& pset : psets) {
		if (pset->id ().to_s () == pset_uuid) {
			epp = pset;
			rv  = true;
			break;
		}
	}

	_pset_id = epp->id ().to_s ();
	/* Load preset(s) - this sets formats(s) and export-filename */
	_manager->load_preset (epp);
	return rv;
}

std::string
SimpleExport::preset_uuid () const
{
	if (!_manager) {
		return _pset_id;
	}
	return _manager->preset ()->id ().to_s ();
}

std::string
SimpleExport::folder () const
{
	return _folder;
}

bool
SimpleExport::check_outputs () const
{
	if (!_manager) {
		return false;
	}
	/* check that master-bus was added */
	auto cc (_manager->get_channel_configs ());
	assert (cc.size () == 1);
	if (cc.front ()->config->get_n_chans () == 0) {
		return false;
	}
	return true;
}

bool
SimpleExport::run_export ()
{
	if (!_session || !check_outputs ()) {
		return false;
	}

	Location*            srl;
	TimeSelection const& tsel (_editor.get_selection ().time);

	if (_name.empty ()) {
		_name = _session->snap_name ();
		if (!tsel.empty ()) {
			_name += _(" (selection)");
		}
	}

	if (_folder.empty ()) {
		_folder = _session->session_directory ().export_path ();
	}

	if (_start != _end) {
		; // range already set
	} else if (!tsel.empty ()) {
		_start = tsel.start_sample ();
		_end   = tsel.end_sample ();
	} else if (NULL != (srl = _session->locations ()->session_range_location ())) {
		_start = srl->start_sample ();
		_end   = srl->end_sample ();
	}

	if (_start >= _end) {
		return false;
	}

	/* Setup timespan */
	auto ts = _manager->get_timespans ();
	assert (ts.size () == 1);
	assert (ts.front ()->timespans->size () == 1);

	ts.front ()->timespans->front ()->set_name (_name);
	ts.front ()->timespans->front ()->set_realtime (false);
	ts.front ()->timespans->front ()->set_range (_start, _end);

	/* Now update filename(s) for each format */
	auto fns = _manager->get_filenames ();
	assert (!fns.empty ());

	auto fms = _manager->get_formats ();
	for (auto const& fm : fms) {
		for (auto const& fn : fns) {
			fn->filename->set_folder (_folder);
			fn->filename->set_timespan (ts.front ()->timespans->front ());
			info << string_compose (_("Exporting: '%1'"), fn->filename->get_path (fm->format)) << endmsg;
		}
	}

	/* All done, configure the handler */
	_manager->prepare_for_export ();

	try {
		if (0 != _handler->do_export ()) {
			return false;
		}
	} catch (std::exception& e) {
		error << string_compose (_("Export initialization failed: %1"), e.what ()) << endmsg;
		return false;
	}

	while (_status->running ()) {
		if (gtk_events_pending ()) {
			gtk_main_iteration ();
		} else {
			Glib::usleep (10000);
		}
	}

	_status->finish (TRS_UI);

	return !_status->aborted ();
}

/* ****************************************************************************/

SimpleExportDialog::SimpleExportDialog (PublicEditor& editor)
	: SimpleExport (editor)
	, ArdourDialog (_("Quick Audio Export"), true, false)
	, _eps (true)
{
	if (_eps.the_combo ().get_parent ()) {
		_eps.the_combo ().get_parent ()->remove (_eps.the_combo ());
	}

	Table* t = manage (new Table);
	int    r = 0;

	t->set_spacings (4);

#define LBL(TXT) *manage (new Label (_(TXT), ALIGN_END))

	/* clang-format off */
	t->attach (LBL ("Format preset:"),  0, 1, r, r + 1, FILL,          SHRINK, 0, 0);
	t->attach (_eps.the_combo (),       1, 2, r, r + 1, EXPAND,        SHRINK, 0, 0);
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

	_post_export_combo.append (_("open the folder where files are exported to."));
	_post_export_combo.append (_("do nothing."));
	_post_export_combo.set_active (0);

	get_vbox ()->pack_start (*t, false, false);

	_cancel_button = add_button (Gtk::Stock::CANCEL, RESPONSE_CANCEL);
	_export_button = add_button (_("Export"), RESPONSE_OK);
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

	_range_combo.remove_all ();

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
		set_error ("Error: Session has no master-bus");
		return;
	}

	/* check range */
	Location*            srl (s->locations ()->session_range_location ());
	TimeSelection const& tsel (_editor.get_selection ().time);

	bool ok = false;
	if (!tsel.empty ()) {
		ok = true;
		_range_combo.append (_("using time selection."));
	}
	if (srl) {
		ok = true;
		_range_combo.append (_("from session start marker to session end marker.")); // same text as ExportVideoDialog::apply_state
	}

	if (!ok) {
		set_error ("Error: No valid range to export. Select a range or create session start/end markers");
		return;
	}

	_range_combo.set_active (0);
	_range_combo.set_sensitive (true);
	_export_button->set_sensitive (true);
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
	Location*            srl = SimpleExport::_session->locations ()->session_range_location ();
	TimeSelection const& tsel (_editor.get_selection ().time);

	std::string range = _range_combo.get_active_text ();
	if (!tsel.empty () && range == _("range selection")) {
		set_range (tsel.start_sample (), tsel.end_sample ());
	} else {
		set_range (srl->start_sample (), srl->end_sample ());
	}

	SimpleExport::_session->add_extra_xml (get_state ());

	_cancel_button->set_label (_("Stop Export"));
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
			NagScreen* ns = NagScreen::maybe_nag (_("export"));
			if (ns) {
				ns->nag ();
				delete ns;
			}
		}
	} else if (!_status->aborted ()) {
		hide ();
		std::string        txt = _("Export has been aborted due to an error!\nSee the Log for details.");
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
			status_text = string_compose (_("Running Post Export Command for '%1'"), _status->timespan_name);
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
