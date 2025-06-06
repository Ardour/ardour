/*
 * Copyright (C) 2005-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2007-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2013 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2014 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#pragma once

#include <string>
#include <ytkmm/box.h>
#include <ytkmm/button.h>
#include <ytkmm/label.h>
#include <ytkmm/notebook.h>
#include <ytkmm/progressbar.h>

#include "ardour/export_profile_manager.h"

#include "public_editor.h"
#include "export_timespan_selector.h"
#include "export_channel_selector.h"
#include "export_file_notebook.h"
#include "export_preset_selector.h"
#include "ardour_dialog.h"
#include "soundcloud_export_selector.h"

namespace ARDOUR {
	class ExportStatus;
	class ExportHandler;
}

class ExportTimespanSelector;
class ExportChannelSelector;

class ExportDialog : public ArdourDialog, public PBD::ScopedConnectionList
{
public:

	ExportDialog (PublicEditor & editor, std::string title, ARDOUR::ExportProfileManager::ExportType type);
	~ExportDialog ();

	void set_session (ARDOUR::Session* s);

	/* Responses */

	enum Responses {
		RESPONSE_RT,
		RESPONSE_FAST,
		RESPONSE_ANALYZE,
		RESPONSE_CANCEL
	};

protected:

	void on_response (int response_id) {
		Gtk::Dialog::on_response (response_id);
	}

	typedef std::shared_ptr<ARDOUR::ExportHandler> HandlerPtr;
	typedef std::shared_ptr<ARDOUR::ExportProfileManager> ManagerPtr;

	ARDOUR::ExportProfileManager::ExportType type;
	HandlerPtr      handler;
	ManagerPtr      profile_manager;

	// initializes GUI layout
	virtual void init_gui ();

	// Must initialize all the shared_ptrs below
	virtual void init_components ();

	std::unique_ptr<ExportPresetSelector>   preset_selector;
	std::unique_ptr<ExportTimespanSelector> timespan_selector;
	std::unique_ptr<ExportChannelSelector>  channel_selector;
	std::unique_ptr<ExportFileNotebook>     file_notebook;

	std::shared_ptr<SoundcloudExportSelector> soundcloud_selector;

	Gtk::VBox                                 warning_widget;
	Gtk::VBox                                 progress_widget;

	/*** GUI components ***/
	Gtk::Notebook export_notebook;

private:

	void init ();

	void notify_errors (bool force = false);
	void close_dialog ();

	void sync_with_manager ();
	void update_warnings_and_example_filename ();
	void show_conflicting_files ();

	void do_export (bool analysis_only);

	void maybe_set_session_dirty ();

	void update_realtime_selection ();
	void parameter_changed (std::string const&);

	void show_progress ();
	gint progress_timeout ();

	typedef std::shared_ptr<ARDOUR::ExportStatus> StatusPtr;

	PublicEditor &  editor;
	StatusPtr       status;

	typedef std::map<samplepos_t, std::vector<std::string>> ReImportMap;
	ReImportMap _files_to_reimport;

	/* Warning area */

	Gtk::HBox           warn_hbox;
	Gtk::Label          warn_label;
	std::string         warn_string;

	Gtk::HBox           list_files_hbox;
	Gtk::Label          list_files_label;
	Gtk::Button         list_files_button;
	std::string         list_files_string;

	void add_error (std::string const & text);
	void add_warning (std::string const & text);

	/* Progress bar */

	Gtk::ProgressBar        progress_bar;
	sigc::connection        progress_connection;

	float previous_progress; // Needed for gtk bug workaround

	bool _initialized;
	bool _analysis_only;

	void soundcloud_upload_progress(double total, double now, std::string title);

	/* Buttons */

	Gtk::Button* cancel_button;
	Gtk::Button* export_button;
	Gtk::Button* analyze_button;

};

class ExportRangeDialog : public ExportDialog
{
  public:
	ExportRangeDialog (PublicEditor & editor, std::string range_id);

  private:
	void init_components ();

	std::string range_id;
};

class ExportSelectionDialog : public ExportDialog
{
  public:
	ExportSelectionDialog (PublicEditor & editor);

  private:
	void init_components ();
};

class ExportRegionDialog : public ExportDialog
{
  public:
	ExportRegionDialog (PublicEditor & editor, ARDOUR::AudioRegion const & region, ARDOUR::AudioTrack & track);

  private:
	void init_gui ();
	void init_components ();

	ARDOUR::AudioRegion const & region;
	ARDOUR::AudioTrack & track;
};

class StemExportDialog : public ExportDialog
{
  public:
	StemExportDialog (PublicEditor & editor);

  private:
	void init_components ();
};

