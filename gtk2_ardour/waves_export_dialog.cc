/*
    Copyright (C) 2008 Paul Davis
    Copyright (C) 2015 Waves Audio Ltd.
    Author: Sakari Bergen

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


#include <sigc++/signal.h>
#include <gtkmm/messagedialog.h>

#include "ardour/audioregion.h"
#include "ardour/export_status.h"
#include "ardour/export_handler.h"
#include "ardour/export_format_specification.h"

#include "waves_message_dialog.h"
#include "waves_export_dialog.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using std::string;

WavesExportDialog::WavesExportDialog (PublicEditor & editor, std::string title, ARDOUR::ExportProfileManager::ExportType type)
  : WavesDialog ("waves_export_dialog.xml", true, false )
  , type (type)
  , editor (editor)
  , _channel_selector_button (get_waves_button ("channel_selector_button"))
  , _export_progress_bar (get_progressbar ("export_progress_bar"))
  , _cancel_button (get_waves_button("cancel_button"))
  , _export_button (get_waves_button("export_button"))
  , _stop_export_button (get_waves_button("stop_export_button"))
  , _export_progress_widget (get_widget ("export_progress_widget"))
  , _warning_widget (get_widget ("warning_widget"))
  , _error_label (get_label ("error_label"))
  , _warn_label (get_label ("warn_label"))
  , _list_files_widget (get_widget ("list_files_widget"))
  , _file_format_selector_button (get_waves_button ("file_format_selector_button"))
  , _timespan_selector_button (get_waves_button ("timespan_selector_button"))
  , _selectors_home (get_container ("selectors_home"))
  , _file_format_selector (get_container ("file_format_selector"))
  , _preset_selector_home (get_container ("preset_selector_home"))
  , _file_notebook_home (get_container ("file_notebook_home"))
  , _timespan_selector_home (get_container ("timespan_selector_home"))
  , _channel_selector_home (get_container ("channel_selector_home"))
{
}

WavesExportDialog::~WavesExportDialog ()
{ }

void
WavesExportDialog::set_session (ARDOUR::Session* s)
{
	SessionHandlePtr::set_session (s);

	if (!_session) {
		return;
	}

	/* Init handler and profile manager */

	handler = _session->get_export_handler ();
	status = _session->get_export_status ();

	profile_manager.reset (new ExportProfileManager (*_session, type));

	/* Possibly init stuff in derived classes */

	init ();

	/* Rest of _session related initialization */

	preset_selector->set_manager (profile_manager);
	file_notebook->set_session_and_manager (_session, profile_manager);

	/* Hand on selection range to profile manager  */

	TimeSelection const & time (editor.get_selection().time);
	if (!time.empty()) {
		profile_manager->set_selection_range (time.front().start, time.front().end);
	} else {
		profile_manager->set_selection_range ();
	}

	/* Load states */

	profile_manager->load_profile ();

	// TRACKS SPECIFIC CONFIGURATION
	ExportProfileManager::FormatStateList const & formats = profile_manager->get_formats ();
	ExportProfileManager::FilenameStateList const & filenames = profile_manager->get_filenames ();
	while (formats.size () > 1) { // so far Tracks Live offers one format per time
		profile_manager->remove_format_state (formats.back ());
		profile_manager->remove_filename_state (filenames.back ());
	}
	if (0) { // so far Tracks Live offers one format per time
		if (formats.size () == 1) {
			profile_manager->duplicate_format_state (formats.front ()),
			profile_manager->duplicate_filename_state (filenames.front ());
		}
	}
    if (formats.size()) {
        ExportProfileManager::FormatStatePtr state = formats.front ();
        if (!state->format ) {
            state->format = state->list->front ();
        }
    }
    
	sync_with_manager ();

	/* Warnings */

	preset_selector->CriticalSelectionChanged.connect (sigc::mem_fun (*this, &WavesExportDialog::sync_with_manager));
	timespan_selector->CriticalSelectionChanged.connect (sigc::mem_fun (*this, &WavesExportDialog::update_warnings_and_example_filename));
	channel_selector->CriticalSelectionChanged.connect (sigc::mem_fun (*this, &WavesExportDialog::update_warnings_and_example_filename));
	file_notebook->CriticalSelectionChanged.connect (sigc::mem_fun (*this, &WavesExportDialog::update_warnings_and_example_filename));

	update_warnings_and_example_filename ();
}

void
WavesExportDialog::init ()
{
	init_components ();
	init_gui ();

	/* warnings */
	get_waves_button ("list_files_button").signal_clicked.connect (sigc::mem_fun (*this, &WavesExportDialog::show_conflicting_files));

	/* Buttons */
	_file_format_selector_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportDialog::show_file_format_selector));
	_timespan_selector_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportDialog::show_timespan_selector));
	_channel_selector_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportDialog::show_channel_selector));

	_cancel_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportDialog::close_dialog));
	_stop_export_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportDialog::close_dialog));
	_export_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportDialog::do_export));

	file_notebook->soundcloud_export_selector = soundcloud_selector;

	/* Done! */

	show_all_children ();
	_export_progress_widget.hide_all();
}

void
WavesExportDialog::init_gui ()
{
	_preset_selector_home.add (*preset_selector);
	_file_notebook_home.add (*file_notebook);
	_timespan_selector_home.add (*timespan_selector);
	_channel_selector_home.add (*channel_selector);

	show_file_format_selector (&_file_format_selector_button);
}

void
WavesExportDialog::init_components ()
{
	preset_selector.reset (new WavesExportPresetSelector ());
	timespan_selector.reset (new WavesExportTimespanSelectorMultiple (_session, profile_manager));
	channel_selector.reset (new WavesPortExportChannelSelector (_session, profile_manager));
	soundcloud_selector.reset (new SoundcloudExportSelector ());
	file_notebook.reset (new WavesExportFileNotebook ());
}

void
WavesExportDialog::notify_errors (bool force)
{
	if (force || status->errors()) {
		std::string txt = _("Export has been aborted due to an error!\nSee the Log for details.");
		WavesMessageDialog msg ("", txt);
		msg.run();
	}
}

void
WavesExportDialog::close_dialog (WavesButton*)
{
	if (status->running) {
		status->abort();
	}

	response (Gtk::RESPONSE_CANCEL);
}

void
WavesExportDialog::sync_with_manager ()
{
	timespan_selector->sync_with_manager();
	channel_selector->sync_with_manager();
	file_notebook->sync_with_manager ();

	update_warnings_and_example_filename ();
}

void
WavesExportDialog::show_selector (Gtk::Widget& widget)
{
	if (!widget.get_parent ()) {
		_selectors_home.add (widget);
		widget.show_all ();
	}
}

void
WavesExportDialog::hide_selector (Gtk::Widget& widget)
{
	if (widget.get_parent () == &_selectors_home) {
		_selectors_home.remove (widget);
	}
}

void
WavesExportDialog::show_file_format_selector (WavesButton*)
{
	_timespan_selector_button.set_active_state (Gtkmm2ext::Off);
	_channel_selector_button.set_active_state (Gtkmm2ext::Off);
	_file_format_selector_button.set_active_state (Gtkmm2ext::ExplicitActive);

	hide_selector (_timespan_selector_home);
	hide_selector (_channel_selector_home);
	show_selector(_file_format_selector);
}

void
WavesExportDialog::show_timespan_selector (WavesButton*)
{
	_file_format_selector_button.set_active_state (Gtkmm2ext::Off);
	_channel_selector_button.set_active_state (Gtkmm2ext::Off);
	_timespan_selector_button.set_active_state (Gtkmm2ext::ExplicitActive);

	hide_selector(_file_format_selector);
	hide_selector (_channel_selector_home);
	show_selector (_timespan_selector_home);
}

void
WavesExportDialog::show_channel_selector (WavesButton*)
{
	_file_format_selector_button.set_active_state (Gtkmm2ext::Off);
	_timespan_selector_button.set_active_state (Gtkmm2ext::Off);
	_channel_selector_button.set_active_state (Gtkmm2ext::ExplicitActive);

	hide_selector(_file_format_selector);
	hide_selector (_timespan_selector_home);
	show_selector (_channel_selector_home);
}

void
WavesExportDialog::update_warnings_and_example_filename ()
{
	/* Reset state */

	warn_string = "";
	_warn_label.set_text (warn_string);
    error_string="";
    _error_label.set_text (error_string);

	_list_files_widget.hide ();
	list_files_string = "";

	_export_button.set_sensitive (true);

	/* Add new warnings */

	boost::shared_ptr<ExportProfileManager::Warnings> warnings = profile_manager->get_warnings();

	for (std::list<string>::iterator it = warnings->errors.begin(); it != warnings->errors.end(); ++it) {
		add_error (*it);
	}

	for (std::list<string>::iterator it = warnings->warnings.begin(); it != warnings->warnings.end(); ++it) {
		add_warning (*it);
	}

	if (!warnings->conflicting_filenames.empty()) {
		_list_files_widget.show ();
		for (std::list<string>::iterator it = warnings->conflicting_filenames.begin(); it != warnings->conflicting_filenames.end(); ++it) {
			string::size_type pos = it->find_last_of ("/");
			list_files_string += it->substr (0, pos + 1) + "<b>" + it->substr (pos + 1) + "</b>\n";
		}
	}

	/* Update example filename */

	file_notebook->update_example_filenames();
}

void
WavesExportDialog::show_conflicting_files (WavesButton*)
{
	ArdourDialog dialog (_("Files that will be overwritten"), true);

	Gtk::Label label ("", Gtk::ALIGN_LEFT);
	label.set_use_markup (true);
	label.set_markup (list_files_string);

	dialog.get_vbox()->pack_start (label);
	dialog.add_button ("OK", 0);
	dialog.show_all_children ();

	dialog.run();
}

void
WavesExportDialog::soundcloud_upload_progress(double total, double now, std::string title)
{
	soundcloud_selector->do_progress_callback(total, now, title);
}

void
WavesExportDialog::do_export (WavesButton*)
{
	try {
		profile_manager->prepare_for_export ();
		handler->soundcloud_username     = soundcloud_selector->username ();
		handler->soundcloud_password     = soundcloud_selector->password ();
		handler->soundcloud_make_public  = soundcloud_selector->make_public ();
		handler->soundcloud_open_page    = soundcloud_selector->open_page ();
		handler->soundcloud_downloadable = soundcloud_selector->downloadable ();

		handler->SoundcloudProgress.connect_same_thread(
				*this, 
				boost::bind(&WavesExportDialog::soundcloud_upload_progress, this, _1, _2, _3)
				);
#if 0
                handler->SoundcloudProgress.connect(
                        *this, invalidator (*this),
                        boost::bind(&WavesExportDialog::soundcloud_upload_progress, this, _1, _2, _3),
                        gui_context()
                        );
#endif
                handler->do_export ();
                show_progress ();
        } catch(std::exception & e) {
                error << string_compose (_("Export initialization failed: %1"), e.what()) << endmsg;
                notify_errors(true);
        }
}

void
WavesExportDialog::show_progress ()
{
	status->running = true;

	_cancel_button.hide ();
	_export_button.hide ();
	_stop_export_button.show ();

	_export_progress_bar.set_fraction (0.0);
	_warning_widget.hide_all();
    _error_label.hide();
	_export_progress_widget.show_all ();
	progress_connection = Glib::signal_timeout().connect (sigc::mem_fun(*this, &WavesExportDialog::progress_timeout), 100);

	gtk_main_iteration ();

	while (status->running) {
		if (gtk_events_pending()) {
			gtk_main_iteration ();
		} else {
			Glib::usleep (10000);
		}
	}

	bool aborted = status->aborted();
	if (aborted) {
		notify_errors ();
	}

	status->finish ();
	if (!aborted) {
		response (Gtk::RESPONSE_OK);
	}
}

gint
WavesExportDialog::progress_timeout ()
{
	std::string status_text;
	float progress = 0.0;
	if (status->normalizing) {
		status_text = string_compose (_("Normalizing '%3' (timespan %1 of %2)"),
		                              status->timespan, status->total_timespans, status->timespan_name);
		progress = ((float) status->current_normalize_cycle) / status->total_normalize_cycles;
	} else {
		status_text = string_compose (_("Exporting '%3' (timespan %1 of %2)"),
		                              status->timespan, status->total_timespans, status->timespan_name);
		progress = ((float) status->processed_frames_current_timespan) / status->total_frames_current_timespan;
	}
	_export_progress_bar.set_text (status_text);

	if (progress < previous_progress) {
		// Work around gtk bug
		_export_progress_bar.hide();
		_export_progress_bar.show();
	}
	previous_progress = progress;

	_export_progress_bar.set_fraction (progress);
	return TRUE;
}

void
WavesExportDialog::add_error (string const & text)
{
	_export_button.set_sensitive (false);

	if (!error_string.empty()) {
		error_string = _("Error: ") + text;
	} else {
		error_string = _("Error: ") + text + "\n" + error_string;
	}

	_error_label.set_text (error_string);
}

void
WavesExportDialog::add_warning (string const & text)
{
	if (!warn_string.empty()) {
		warn_string += "\n";
	}
	warn_string += _("\nWarning: ") + text;
	_warn_label.set_text (warn_string);
}

/*** Dialog specializations ***/

WavesExportRangeDialog::WavesExportRangeDialog (PublicEditor & editor, string range_id) :
  WavesExportDialog (editor, _("Export Range"), ExportProfileManager::RangeExport),
  range_id (range_id)
{}

void
WavesExportRangeDialog::init_components ()
{
	preset_selector.reset (new WavesExportPresetSelector ());
	timespan_selector.reset (new WavesExportTimespanSelectorSingle (_session, profile_manager, range_id));
	channel_selector.reset (new WavesPortExportChannelSelector (_session, profile_manager));
	soundcloud_selector.reset (new SoundcloudExportSelector ());
	file_notebook.reset (new WavesExportFileNotebook ());
}

WavesExportSelectionDialog::WavesExportSelectionDialog (PublicEditor & editor) :
  WavesExportDialog (editor, _("Export Selection"), ExportProfileManager::SelectionExport)
{}

void
WavesExportSelectionDialog::init_components ()
{
	preset_selector.reset (new WavesExportPresetSelector ());
	timespan_selector.reset (new WavesExportTimespanSelectorSingle (_session, profile_manager, X_("selection")));
	channel_selector.reset (new WavesPortExportChannelSelector (_session, profile_manager));
	soundcloud_selector.reset (new SoundcloudExportSelector ());
	file_notebook.reset (new WavesExportFileNotebook ());
}

WavesExportRegionDialog::WavesExportRegionDialog (PublicEditor & editor, ARDOUR::AudioRegion const & region, ARDOUR::AudioTrack & track) :
  WavesExportDialog (editor, _("Export Region"), ExportProfileManager::RegionExport),
  region (region),
  track (track)
{}

void
WavesExportRegionDialog::init_gui ()
{
	WavesExportDialog::init_gui ();
	_channel_selector_button.set_text ("Source"); //export_notebook.set_tab_label_text(*export_notebook.get_nth_page(2), _("Source"));
}

void
WavesExportRegionDialog::init_components ()
{
	string loc_id = profile_manager->set_single_range (region.position(), region.position() + region.length(), region.name());

	preset_selector.reset (new WavesExportPresetSelector ());
	timespan_selector.reset (new WavesExportTimespanSelectorSingle (_session, profile_manager, loc_id));
	channel_selector.reset (new WavesRegionExportChannelSelector (_session, profile_manager, region, track));
	soundcloud_selector.reset (new SoundcloudExportSelector ());
	file_notebook.reset (new WavesExportFileNotebook ());
}

WavesStemExportDialog::WavesStemExportDialog (PublicEditor & editor)
  : WavesExportDialog(editor, _("Stem Export"), ExportProfileManager::StemExport)
{

}

void
WavesStemExportDialog::init_components ()
{
	preset_selector.reset (new WavesExportPresetSelector ());
	timespan_selector.reset (new WavesExportTimespanSelectorMultiple (_session, profile_manager));
	channel_selector.reset (new WavesTrackExportChannelSelector (_session, profile_manager));
	soundcloud_selector.reset (new SoundcloudExportSelector ());
	file_notebook.reset (new WavesExportFileNotebook ());
}
