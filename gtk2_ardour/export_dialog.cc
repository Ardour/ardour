/*
 * Copyright (C) 2005-2006 Doug McLain <doug@nostar.net>
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2008 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2008-2013 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2014 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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


#include <sigc++/signal.h>

#include <gtkmm/messagedialog.h>
#include <gtkmm/stock.h>

#include "pbd/gstdio_compat.h"
#include "pbd/file_utils.h"

#include "ardour/audioregion.h"
#include "ardour/export_channel_configuration.h"
#include "ardour/export_format_specification.h"
#include "ardour/export_status.h"
#include "ardour/export_handler.h"
#include "ardour/profile.h"

#include "export_dialog.h"
#include "export_report.h"
#include "gui_thread.h"
#include "mixer_ui.h"
#include "nag.h"
#include "ui_config.h"

#include "pbd/i18n.h"

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
  , previous_progress (0)
  , _initialized (false)
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
		profile_manager->set_selection_range (time.front().start().samples(), time.front().end().samples());
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
	channel_selector->CriticalSelectionChanged.connect (sigc::mem_fun (*this, &ExportDialog::update_realtime_selection));
	file_notebook->CriticalSelectionChanged.connect (sigc::mem_fun (*this, &ExportDialog::update_warnings_and_example_filename));

	/* Catch major selection changes, and set the session dirty */

	preset_selector->CriticalSelectionChanged.connect (sigc::mem_fun (*this, &ExportDialog::maybe_set_session_dirty));
	timespan_selector->CriticalSelectionChanged.connect (sigc::mem_fun (*this, &ExportDialog::maybe_set_session_dirty));
	channel_selector->CriticalSelectionChanged.connect (sigc::mem_fun (*this, &ExportDialog::maybe_set_session_dirty));
	channel_selector->CriticalSelectionChanged.connect (sigc::mem_fun (*this, &ExportDialog::maybe_set_session_dirty));
	file_notebook->CriticalSelectionChanged.connect (sigc::mem_fun (*this, &ExportDialog::maybe_set_session_dirty));

	update_warnings_and_example_filename ();
	update_realtime_selection ();

	_initialized = true;

	_session->config.ParameterChanged.connect (*this, invalidator (*this), boost::bind (&ExportDialog::parameter_changed, this, _1), gui_context());
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

	cancel_button->signal_clicked().connect (sigc::mem_fun (*this, &ExportDialog::close_dialog));
	export_button->signal_clicked().connect (sigc::mem_fun (*this, &ExportDialog::do_export));

	file_notebook->soundcloud_export_selector = soundcloud_selector;

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
ExportDialog::notify_errors (bool force)
{
	if (force || status->errors()) {
		std::string txt = _("Export has been aborted due to an error!\nSee the Log for details.");
		Gtk::MessageDialog msg (txt, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		msg.run();
	}
}

void
ExportDialog::close_dialog ()
{
	if (status->running ()) {
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
	update_realtime_selection ();
}


void
ExportDialog::maybe_set_session_dirty ()
{
	/* Presumably after all initialization is finished, sync_with_manager means that something important changed. */
	/* Let's prompt the user to save the session; otherwise these Export settings changes would be lost on re-open */
	if (_initialized) {
		_session->set_dirty();
	}
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

	/* add channel count warning */
	if (channel_selector && channel_selector->channel_limit_reached ()) {
		add_warning (_("A track or bus has more channels than the target."));
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
ExportDialog::update_realtime_selection ()
{
	bool rt_ok = true;
	switch (profile_manager->type ()) {
		case ExportProfileManager::RegularExport:
		case ExportProfileManager::RangeExport:
		case ExportProfileManager::SelectionExport:
			break;
		case ExportProfileManager::RegionExport:
			rt_ok = false;
			break;
		case ExportProfileManager::StemExport:
			if (! static_cast<TrackExportChannelSelector*>(channel_selector.get())->track_output ()) {
				rt_ok = false;
			}
			break;
	}

	timespan_selector->allow_realtime_export (rt_ok);
}

void
ExportDialog::parameter_changed (std::string const& p)
{
	if (p == "realtime-export") {
		update_realtime_selection ();
	}
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
	try {
		profile_manager->prepare_for_export ();
		handler->soundcloud_username     = soundcloud_selector->username ();
		handler->soundcloud_password     = soundcloud_selector->password ();
		handler->soundcloud_make_public  = soundcloud_selector->make_public ();
		handler->soundcloud_open_page    = soundcloud_selector->open_page ();
		handler->soundcloud_downloadable = soundcloud_selector->downloadable ();

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
	} catch(std::exception & e) {
		error << string_compose (_("Export initialization failed: %1"), e.what()) << endmsg;
		notify_errors(true);
	}
}

void
ExportDialog::show_progress ()
{
	export_notebook.set_sensitive (false);

	cancel_button->set_label (_("Stop Export"));
	export_button->set_sensitive (false);

	progress_bar.set_fraction (0.0);
	warning_widget.hide_all();
	progress_widget.show ();
	progress_widget.show_all_children ();
	progress_connection = Glib::signal_timeout().connect (sigc::mem_fun(*this, &ExportDialog::progress_timeout), 100);

	gtk_main_iteration ();

	while (status->running ()) {
		if (gtk_events_pending()) {
			gtk_main_iteration ();
		} else {
			Glib::usleep (10000);
		}
	}

	status->finish (TRS_UI);

	if (!status->aborted() && UIConfiguration::instance().get_save_export_mixer_screenshot ()) {
		ExportProfileManager::TimespanStateList const& timespans = profile_manager->get_timespans();
		ExportProfileManager::FilenameStateList const& filenames = profile_manager->get_filenames ();

		std::list<std::string> paths;
		for (ExportProfileManager::FilenameStateList::const_iterator fi = filenames.begin(); fi != filenames.end(); ++fi) {
			for (ExportProfileManager::TimespanStateList::const_iterator ti = timespans.begin(); ti != timespans.end(); ++ti) {
				ExportProfileManager::TimespanListPtr tlp = (*ti)->timespans;
				for (ExportProfileManager::TimespanList::const_iterator eti = tlp->begin(); eti != tlp->end(); ++eti) {
					(*fi)->filename->set_timespan (*eti);
					paths.push_back ((*fi)->filename->get_path (ExportFormatSpecPtr ()) + "-mixer.png");
				}
			}
		}

		if (paths.size() > 0) {
			PBD::info << string_compose(_("Writing Mixer Screenshot: %1."), paths.front()) << endmsg;
			Mixer_UI::instance()->screenshot (paths.front());

			std::list<std::string>::const_iterator it = paths.begin ();
			++it;
			for (; it != paths.end(); ++it) {
				PBD::info << string_compose(_("Copying Mixer Screenshot: %1."), *it) << endmsg;
				::g_unlink (it->c_str());
				if (!hard_link (paths.front(), *it)) {
					copy_file (paths.front(), *it);
				}
			}
		}
	}

	if (!status->aborted() && _session->export_xruns () > 0) {
		std::string txt = string_compose (_("There have been %1 dropouts during realtime-export."), _session->export_xruns ());
		Gtk::MessageDialog msg (txt, false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
		msg.run();
	}

	if (!status->aborted() && status->result_map.size() > 0) {
		hide();
		ExportReport er (_session, status);
		er.run();
	}

	if (!status->aborted()) {
		hide();
		if (!ARDOUR::Profile->get_mixbus()) {
			NagScreen* ns = NagScreen::maybe_nag (_("export"));
			if (ns) {
				ns->nag ();
				delete ns;
			}
		}
	} else {
		notify_errors ();
	}
	export_notebook.set_sensitive (true);
}

gint
ExportDialog::progress_timeout ()
{
	std::string status_text;
	float progress = -1;
	switch (status->active_job) {
	case ExportStatus::Exporting:
		status_text = string_compose (_("Exporting '%3' (timespan %1 of %2)"),
		                              status->timespan, status->total_timespans, status->timespan_name);
		progress = ((float) status->processed_samples_current_timespan) / status->total_samples_current_timespan;
		break;
	case ExportStatus::Normalizing:
		status_text = string_compose (_("Normalizing '%3' (timespan %1 of %2)"),
		                              status->timespan, status->total_timespans, status->timespan_name);
		progress = ((float) status->current_postprocessing_cycle) / status->total_postprocessing_cycles;
		break;
	case ExportStatus::Encoding:
		status_text = string_compose (_("Encoding '%3' (timespan %1 of %2)"),
		                              status->timespan, status->total_timespans, status->timespan_name);
		progress = ((float) status->current_postprocessing_cycle) / status->total_postprocessing_cycles;
		break;
	case ExportStatus::Tagging:
		status_text = string_compose (_("Tagging '%3' (timespan %1 of %2)"),
		                              status->timespan, status->total_timespans, status->timespan_name);
		break;
	case ExportStatus::Uploading:
		status_text = string_compose (_("Uploading '%3' (timespan %1 of %2)"),
		                              status->timespan, status->total_timespans, status->timespan_name);
		break;
	case ExportStatus::Command:
		status_text = string_compose (_("Running Post Export Command for '%1'"), status->timespan_name);
		break;
	}

	progress_bar.set_text (status_text);

	if (progress < previous_progress) {
		// Work around gtk bug
		progress_bar.hide();
		progress_bar.show();
	}
	previous_progress = progress;

	if (progress >= 0) {
		progress_bar.set_fraction (progress);
	} else {
		progress_bar.set_pulse_step(.1);
		progress_bar.pulse();
	}
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
	string loc_id = profile_manager->set_single_range (region.position_sample(), (region.position() + region.length()).samples(), region.name());

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
