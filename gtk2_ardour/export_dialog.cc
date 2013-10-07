/*
    Copyright (C) 2008 Paul Davis
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

#include "export_dialog.h"
#include "gui_thread.h"
#include "nag.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using std::string;

ExportDialog::ExportDialog (PublicEditor & editor, std::string title, ARDOUR::ExportProfileManager::ExportType type)
  : ArdourDialog (title)
  , type (type)
  , editor (editor)

  , warn_label ("", Gtk::ALIGN_LEFT)
  , list_files_label (_("<span color=\"#ffa755\">Some already existing files will be overwritten.</span>"), Gtk::ALIGN_RIGHT)
  , list_files_button (_("List files"))
{ }

ExportDialog::~ExportDialog ()
{ }

void
ExportDialog::set_session (ARDOUR::Session* s)
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
	sync_with_manager ();

	/* Warnings */

	preset_selector->CriticalSelectionChanged.connect (sigc::mem_fun (*this, &ExportDialog::sync_with_manager));
	timespan_selector->CriticalSelectionChanged.connect (sigc::mem_fun (*this, &ExportDialog::update_warnings_and_example_filename));
	channel_selector->CriticalSelectionChanged.connect (sigc::mem_fun (*this, &ExportDialog::update_warnings_and_example_filename));
	file_notebook->CriticalSelectionChanged.connect (sigc::mem_fun (*this, &ExportDialog::update_warnings_and_example_filename));

	update_warnings_and_example_filename ();
}

void
ExportDialog::init ()
{
	init_components ();
	init_gui ();

	/* warnings */

	warning_widget.pack_start (warn_hbox, true, true, 6);
	warning_widget.pack_end (list_files_hbox, false, false, 0);

	warn_hbox.pack_start (warn_label, true, true, 16);
	warn_label.set_use_markup (true);

	list_files_hbox.pack_end (list_files_button, false, false, 6);
	list_files_hbox.pack_end (list_files_label, false, false, 6);
	list_files_label.set_use_markup (true);

	list_files_button.signal_clicked().connect (sigc::mem_fun (*this, &ExportDialog::show_conflicting_files));

	/* Progress indicators */

	progress_widget.pack_start (progress_bar, false, false, 6);

	/* Buttons */

	cancel_button = add_button (Gtk::Stock::CANCEL, RESPONSE_CANCEL);
	export_button = add_button (_("Export"), RESPONSE_FAST);
	set_default_response (RESPONSE_FAST);

	list_files_button.set_name ("PaddedButton");

	cancel_button->signal_clicked().connect (sigc::mem_fun (*this, &ExportDialog::close_dialog));
	export_button->signal_clicked().connect (sigc::mem_fun (*this, &ExportDialog::do_export));

	/* Done! */

	show_all_children ();
	progress_widget.hide_all();
}

void
ExportDialog::init_gui ()
{
	Gtk::Alignment * preset_align = Gtk::manage (new Gtk::Alignment());
	preset_align->add (*preset_selector);
	preset_align->set_padding (0, 12, 0, 0);

	Gtk::VBox * file_format_selector = Gtk::manage (new Gtk::VBox());
	file_format_selector->set_homogeneous (false);
	file_format_selector->pack_start (*preset_align, false, false, 0);
	file_format_selector->pack_start (*file_notebook, false, false, 0);
	file_format_selector->pack_start (*soundcloud_selector, false, false, 0);

	export_notebook.append_page (*file_format_selector, _("File format"));
	export_notebook.append_page (*timespan_selector, _("Time Span"));
	export_notebook.append_page (*channel_selector, _("Channels"));
	
	get_vbox()->pack_start (export_notebook, true, true, 0);
	get_vbox()->pack_end   (warning_widget, false, false, 0);
	get_vbox()->pack_end   (progress_widget, false, false, 0);

}

void
ExportDialog::init_components ()
{
	preset_selector.reset (new ExportPresetSelector ());
	timespan_selector.reset (new ExportTimespanSelectorMultiple (_session, profile_manager));
	channel_selector.reset (new PortExportChannelSelector (_session, profile_manager));
	soundcloud_selector.reset (new SoundcloudExportSelector ());
	file_notebook.reset (new ExportFileNotebook ());
}

void
ExportDialog::notify_errors ()
{
	if (status->errors()) {
		std::string txt = _("Export has been aborted due to an error!\nSee the Log for details.");
		Gtk::MessageDialog msg (txt, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		msg.run();
	}
}

void
ExportDialog::close_dialog ()
{
	if (status->running) {
		status->abort();
	}

	hide_all ();
	set_modal (false);

}

void
ExportDialog::sync_with_manager ()
{
	timespan_selector->sync_with_manager();
	channel_selector->sync_with_manager();
	file_notebook->sync_with_manager ();

	update_warnings_and_example_filename ();
}

void
ExportDialog::update_warnings_and_example_filename ()
{
	/* Reset state */

	warn_string = "";
	warn_label.set_markup (warn_string);

	list_files_hbox.hide ();
	list_files_string = "";

	export_button->set_sensitive (true);

	/* Add new warnings */

	boost::shared_ptr<ExportProfileManager::Warnings> warnings = profile_manager->get_warnings();

	for (std::list<string>::iterator it = warnings->errors.begin(); it != warnings->errors.end(); ++it) {
		add_error (*it);
	}

	for (std::list<string>::iterator it = warnings->warnings.begin(); it != warnings->warnings.end(); ++it) {
		add_warning (*it);
	}

	if (!warnings->conflicting_filenames.empty()) {
		list_files_hbox.show ();
		for (std::list<string>::iterator it = warnings->conflicting_filenames.begin(); it != warnings->conflicting_filenames.end(); ++it) {
			string::size_type pos = it->find_last_of ("/");
			list_files_string += it->substr (0, pos + 1) + "<b>" + it->substr (pos + 1) + "</b>\n";
		}
	}

	/* Update example filename */

	file_notebook->update_example_filenames();
}

void
ExportDialog::show_conflicting_files ()
{
	ArdourDialog dialog (_("Files that will be overwritten"), true);

	Gtk::Label label ("", Gtk::ALIGN_LEFT);
	label.set_use_markup (true);
	label.set_markup (list_files_string);

	dialog.get_vbox()->pack_start (label);
	dialog.add_button (Gtk::Stock::OK, 0);
	dialog.show_all_children ();

	dialog.run();
}

void
ExportDialog::soundcloud_upload_progress(double total, double now, std::string title)
{
	soundcloud_selector->do_progress_callback(total, now, title);

}

void
ExportDialog::do_export ()
{
	profile_manager->prepare_for_export ();
	handler->upload_username = soundcloud_selector->username();
	handler->upload_password = soundcloud_selector->password();
	handler->upload_public   = soundcloud_selector->upload_public();
	handler->upload_open     = soundcloud_selector->upload_open();

	handler->SoundcloudProgress.connect_same_thread(
			*this, 
		       	boost::bind(&ExportDialog::soundcloud_upload_progress, this, _1, _2, _3)
	);
#if 0
	handler->SoundcloudProgress.connect(
			*this, invalidator (*this),
		       	boost::bind(&ExportDialog::soundcloud_upload_progress, this, _1, _2, _3),
			gui_context()
	);
#endif
	handler->do_export ();
	show_progress ();
}

void
ExportDialog::show_progress ()
{
	status->running = true;

	cancel_button->set_label (_("Stop Export"));
	export_button->set_sensitive (false);

	progress_bar.set_fraction (0.0);
	warning_widget.hide_all();
	progress_widget.show ();
	progress_widget.show_all_children ();
	progress_connection = Glib::signal_timeout().connect (sigc::mem_fun(*this, &ExportDialog::progress_timeout), 100);

	gtk_main_iteration ();

	while (status->running) {
		if (gtk_events_pending()) {
			gtk_main_iteration ();
		} else {
			usleep (10000);
		}
	}

	if (!status->aborted()) {

		NagScreen* ns = NagScreen::maybe_nag (_("export"));
		
		if (ns) {
			ns->nag ();
			delete ns;
		}
	} else {
		notify_errors ();
	}

	status->finish ();
}

gint
ExportDialog::progress_timeout ()
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
	progress_bar.set_text (status_text);

	if (progress < previous_progress) {
		// Work around gtk bug
		progress_bar.hide();
		progress_bar.show();
	}
	previous_progress = progress;

	progress_bar.set_fraction (progress);
	return TRUE;
}

void
ExportDialog::add_error (string const & text)
{
	export_button->set_sensitive (false);

	if (warn_string.empty()) {
		warn_string = _("<span color=\"#ffa755\">Error: ") + text + "</span>";
	} else {
		warn_string = _("<span color=\"#ffa755\">Error: ") + text + "</span>\n" + warn_string;
	}

	warn_label.set_markup (warn_string);
}

void
ExportDialog::add_warning (string const & text)
{
	if (warn_string.empty()) {
		warn_string = _("<span color=\"#ffa755\">Warning: ") + text + "</span>";
	} else {
		warn_string = warn_string + _("\n<span color=\"#ffa755\">Warning: ") + text + "</span>";
	}

	warn_label.set_markup (warn_string);
}

/*** Dialog specializations ***/

ExportRangeDialog::ExportRangeDialog (PublicEditor & editor, string range_id) :
  ExportDialog (editor, _("Export Range"), ExportProfileManager::RangeExport),
  range_id (range_id)
{}

void
ExportRangeDialog::init_components ()
{
	preset_selector.reset (new ExportPresetSelector ());
	timespan_selector.reset (new ExportTimespanSelectorSingle (_session, profile_manager, range_id));
	channel_selector.reset (new PortExportChannelSelector (_session, profile_manager));
	soundcloud_selector.reset (new SoundcloudExportSelector ());
	file_notebook.reset (new ExportFileNotebook ());
}

ExportSelectionDialog::ExportSelectionDialog (PublicEditor & editor) :
  ExportDialog (editor, _("Export Selection"), ExportProfileManager::SelectionExport)
{}

void
ExportSelectionDialog::init_components ()
{
	preset_selector.reset (new ExportPresetSelector ());
	timespan_selector.reset (new ExportTimespanSelectorSingle (_session, profile_manager, X_("selection")));
	channel_selector.reset (new PortExportChannelSelector (_session, profile_manager));
	soundcloud_selector.reset (new SoundcloudExportSelector ());
	file_notebook.reset (new ExportFileNotebook ());
}

ExportRegionDialog::ExportRegionDialog (PublicEditor & editor, ARDOUR::AudioRegion const & region, ARDOUR::AudioTrack & track) :
  ExportDialog (editor, _("Export Region"), ExportProfileManager::RegionExport),
  region (region),
  track (track)
{}

void
ExportRegionDialog::init_gui ()
{
	ExportDialog::init_gui ();
	export_notebook.set_tab_label_text(*export_notebook.get_nth_page(2), _("Source"));
}

void
ExportRegionDialog::init_components ()
{
	string loc_id = profile_manager->set_single_range (region.position(), region.position() + region.length(), region.name());

	preset_selector.reset (new ExportPresetSelector ());
	timespan_selector.reset (new ExportTimespanSelectorSingle (_session, profile_manager, loc_id));
	channel_selector.reset (new RegionExportChannelSelector (_session, profile_manager, region, track));
	soundcloud_selector.reset (new SoundcloudExportSelector ());
	file_notebook.reset (new ExportFileNotebook ());
}

StemExportDialog::StemExportDialog (PublicEditor & editor)
  : ExportDialog(editor, _("Stem Export"), ExportProfileManager::StemExport)
{

}

void
StemExportDialog::init_components ()
{
	preset_selector.reset (new ExportPresetSelector ());
	timespan_selector.reset (new ExportTimespanSelectorMultiple (_session, profile_manager));
	channel_selector.reset (new TrackExportChannelSelector (_session, profile_manager));
	soundcloud_selector.reset (new SoundcloudExportSelector ());
	file_notebook.reset (new ExportFileNotebook ());
}
