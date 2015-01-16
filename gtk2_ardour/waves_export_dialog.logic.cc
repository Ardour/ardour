/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#include "ardour/audioregion.h"
#include "ardour/export_status.h"
#include "ardour/export_handler.h"
#include "waves_export_dialog.h"
#include "public_editor.h"
#include "ardour_ui.h"
#include "time_selection.h"
#include "i18n.h"

void
WavesExportDialog::init (ARDOUR::Session* session)
{
    _export_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportDialog::_on_export_button_clicked));
    _cancel_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportDialog::_on_cancel_button_clicked));

	SessionHandlePtr::set_session (session);
	if (!_session) {
		return;
	}
	/* Init handler and profile manager */
	_export_handler = _session->get_export_handler ();
	_export_status = _session->get_export_status ();

	_profile_manager.reset (new ARDOUR::ExportProfileManager (*_session, _export_type));

	TimeSelection const & time (ARDOUR_UI::instance()->the_editor ().get_selection().time);
	if (!time.empty()) {
		_profile_manager->set_selection_range (time.front().start, time.front().end);
	} else {
		_profile_manager->set_selection_range ();
	}
}

void
WavesExportDialog::_show_progress ()
{
	_export_status->running = true;

	_cancel_button.hide ();
	_stop_export_button.show ();

	_export_progress_bar.set_fraction (0.0);
	_export_progress_bar.show ();
	_progress_connection = Glib::signal_timeout().connect (sigc::mem_fun(*this, &WavesExportDialog::_on_progress_timeout), 100);

	gtk_main_iteration ();

	while (_export_status->running) {
		if (gtk_events_pending()) {
			gtk_main_iteration ();
		} else {
			Glib::usleep (10000);
		}
	}

	if (_export_status->aborted()) {
		_notify_errors ();
	}

	_export_status->finish ();
}

void
WavesExportDialog::_notify_errors (bool force)
{
	if (force || _export_status->errors()) {
		std::string txt = _("Export has been aborted due to an error!\nSee the Log for details.");
		WavesMessageDialog msg ("", txt);
		msg.run();
	}
}

void
WavesExportDialog::_on_export_button_clicked (WavesButton*)
{
	_export_error.clear ();
	_previous_progress = 0;
	try {
		_profile_manager->prepare_for_export ();
		_export_handler->soundcloud_make_public  = false;
		_export_handler->soundcloud_open_page    = false;
		_export_handler->soundcloud_downloadable = false;

        _export_handler->do_export ();
        _show_progress ();
    } catch(std::exception & e) {
        _export_error << string_compose (_("Export initialization failed: %1"), e.what()) << endmsg;
        _notify_errors(true);
    }
}

void
WavesExportDialog::_on_cancel_button_clicked (WavesButton*)
{
	if (_export_status->running) {
		_export_status->abort();
	}

	hide ();
	set_modal (false);
	response (Gtk::RESPONSE_CANCEL);
}

gint
WavesExportDialog::_on_progress_timeout ()
{
	std::string status_text;
	float progress = 0.0;

	if (_export_status->normalizing) {
		status_text = string_compose (_("Normalizing '%3' (timespan %1 of %2)"),
		                              _export_status->timespan, _export_status->total_timespans, _export_status->timespan_name);
		progress = ((float) _export_status->current_normalize_cycle) / _export_status->total_normalize_cycles;
	} else {
		status_text = string_compose (_("Exporting '%3' (timespan %1 of %2)"),
		                              _export_status->timespan, _export_status->total_timespans, _export_status->timespan_name);
		progress = ((float) _export_status->processed_frames_current_timespan) / _export_status->total_frames_current_timespan;
	}

	_export_progress_bar.set_text (status_text);

	if (progress < _previous_progress) {
		// Work around gtk bug
		_export_progress_bar.hide();
		_export_progress_bar.show();
	}

	_previous_progress = progress;

	_export_progress_bar.set_fraction (progress);
	return TRUE;
}
