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

#include <glibmm.h>

#include "ardour/export_channel_configuration.h"
#include "ardour/export_format_specification.h"
#include "ardour/export_filename.h"
#include "ardour/export_preset.h"
#include "ardour/export_profile_manager.h"
#include "ardour/export_status.h"
#include "ardour/export_timespan.h"
#include "ardour/profile.h"
#include "ardour/session_directory.h"
#include "ardour/simple_export.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

SimpleExport::SimpleExport ()
	: _pset_id ("df340c53-88b5-4342-a1c8-58e0704872ea" /* CD */)
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

	if (_name.empty ()) {
		_name = _session->snap_name ();
	}

	if (_folder.empty ()) {
		_folder = _session->session_directory ().export_path ();
	}

	Location* srl;
	if (_start != _end) {
		; // range already set
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
	assert (ts.front ()->timespans->size () < 2);
	/* when there is no session-range, ExportProfileManager::init_timespans
	 * does not add an ExportTimespanPtr.
	 */
	if (ts.front ()->timespans->size () < 1) {
		ExportTimespanPtr timespan = _handler->add_timespan ();
		ts.front ()->timespans->push_back (timespan);
	}

	ts.front ()->timespans->front ()->set_name (_name);
	ts.front ()->timespans->front ()->set_realtime (false);
	ts.front ()->timespans->front ()->set_range (_start, _end);

	/* Now update filename(s) for each format */
	auto fns = _manager->get_filenames ();
	assert (!fns.empty ());

	auto fms = _manager->get_formats ();
	if (ts.front ()->timespans->front ()->vapor().empty ()) {
		for (auto const& fm : fms) {
			for (auto const& fn : fns) {
				fn->filename->set_folder (_folder);
				fn->filename->set_timespan (ts.front ()->timespans->front ());
				info << string_compose (_("Exporting: '%1'"), fn->filename->get_path (fm->format)) << endmsg;
			}
		}
	} else {
		for (auto const& fm : fms) {
			std::shared_ptr<ExportFormatSpecification> fmp = fm->format;
			fmp->set_format_id (ExportFormatBase::F_None);
			fmp->set_type (ExportFormatBase::T_None);
			fmp->set_analyse (false);
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
		GUIIdle ();
		// TODO only sleep if GUI did not process any events
		Glib::usleep (10000);
	}

	_status->finish (TRS_UI);

	return !_status->aborted ();
}
