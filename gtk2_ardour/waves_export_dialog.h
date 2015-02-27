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

#ifndef __waves_export_dialog_h__
#define __waves_export_dialog_h__

#include <boost/scoped_ptr.hpp>
#include <string>

#include "ardour/export_profile_manager.h"

#include "public_editor.h"
#include "waves_export_timespan_selector.h"
#include "waves_export_channel_selector.h"
#include "waves_export_file_notebook.h"
#include "waves_export_preset_selector.h"
#include "waves_dialog.h"
#include "soundcloud_export_selector.h"

#include <gtkmm.h>

namespace ARDOUR {
	class ExportStatus;
	class ExportHandler;
}

class WavesExportDialog : public WavesDialog, public PBD::ScopedConnectionList 
{

  public:

	WavesExportDialog (PublicEditor & editor, std::string title, ARDOUR::ExportProfileManager::ExportType type);
	~WavesExportDialog ();

	void set_session (ARDOUR::Session* s);

	/* Responses */

	enum Responses {
		RESPONSE_RT,
		RESPONSE_FAST,
		RESPONSE_CANCEL
	};

  protected:

	typedef boost::shared_ptr<ARDOUR::ExportHandler> HandlerPtr;
	typedef boost::shared_ptr<ARDOUR::ExportProfileManager> ManagerPtr;

	ARDOUR::ExportProfileManager::ExportType type;
	HandlerPtr      handler;
	ManagerPtr      profile_manager;

	// initializes GUI layout
	virtual void init_gui ();

	// Must initialize all the shared_ptrs below
	virtual void init_components ();

	void on_default_response ();
	void on_response(int response_id);

	boost::scoped_ptr<WavesExportPresetSelector>   preset_selector;
	boost::scoped_ptr<WavesExportTimespanSelector> timespan_selector;
	boost::scoped_ptr<WavesExportChannelSelector>  channel_selector;
	boost::scoped_ptr<WavesExportFileNotebook>     file_notebook;

	boost::shared_ptr<SoundcloudExportSelector> soundcloud_selector;

	/*** GUI components ***/
    WavesButton& _channel_selector_button;

  private:

	void init ();
	void notify_errors (bool force = false);
	void close_dialog (WavesButton*);
	void show_file_format_selector (WavesButton*);
	void show_selector (Gtk::Widget&);
	void hide_selector (Gtk::Widget&);
	void show_timespan_selector (WavesButton*);
	void show_channel_selector (WavesButton*);

	void sync_with_manager ();
	void update_warnings_and_example_filename ();
	void show_conflicting_files (WavesButton*);

	void on_export (WavesButton*);
	void do_export ();

	void show_progress ();
	gint progress_timeout ();


	typedef boost::shared_ptr<ARDOUR::ExportStatus> StatusPtr;

	PublicEditor &  editor;
	StatusPtr       status;

	/* Warning area */

    std::string       error_string;
	std::string       warn_string;
	std::string       list_files_string;

	void add_error (std::string const & text);
	void add_warning (std::string const & text);

	/* Progress bar */

	Gtk::ProgressBar&       _export_progress_bar;
	sigc::connection        progress_connection;

	float previous_progress; // Needed for gtk bug workaround

	void soundcloud_upload_progress(double total, double now, std::string title);

	/* Buttons */

	WavesButton& _cancel_button;
	WavesButton& _export_button;
	WavesButton& _stop_export_button;
	Gtk::Widget& _export_progress_widget;
	Gtk::Widget& _warning_widget;
    Gtk::Label&  _error_label;
	Gtk::Label&  _warn_label;
	Gtk::Widget& _list_files_widget;
    WavesButton& _file_format_selector_button;
    WavesButton& _timespan_selector_button;
	Gtk::Container& _selectors_home;
	Gtk::Container& _file_format_selector;
	Gtk::Container& _preset_selector_home;
	Gtk::Container& _file_notebook_home;
	Gtk::Container& _timespan_selector_home;
	Gtk::Container& _channel_selector_home;
};

class WavesExportRangeDialog : public WavesExportDialog
{
  public:
	WavesExportRangeDialog (PublicEditor & editor, std::string range_id);

  private:
	void init_components ();

	std::string range_id;
};

class WavesExportSelectionDialog : public WavesExportDialog
{
  public:
	WavesExportSelectionDialog (PublicEditor & editor);

  private:
	void init_components ();
};

class WavesExportRegionDialog : public WavesExportDialog
{
  public:
	WavesExportRegionDialog (PublicEditor & editor, ARDOUR::AudioRegion const & region, ARDOUR::AudioTrack & track);

  private:
	void init_gui ();
	void init_components ();

	ARDOUR::AudioRegion const & region;
	ARDOUR::AudioTrack & track;
};

class WavesStemExportDialog : public WavesExportDialog
{
  public:
	WavesStemExportDialog (PublicEditor & editor);

  private:
	void init_components ();
};

#endif /* __waves_export_dialog_h__ */
