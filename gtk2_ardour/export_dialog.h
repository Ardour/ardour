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

#ifndef __export_dialog_h__
#define __export_dialog_h__

#include <boost/shared_ptr.hpp>

#include "ardour/export_handler.h"
#include "ardour/export_profile_manager.h"

#include "public_editor.h"
#include "export_timespan_selector.h"
#include "export_channel_selector.h"
#include "export_file_notebook.h"
#include "export_preset_selector.h"
#include "ardour_dialog.h"

#include <gtkmm.h>

#include "i18n.h"

namespace ARDOUR {
	class ExportStatus;
}

class ExportTimespanSelector;
class ExportChannelSelector;

class ExportDialog : public ArdourDialog {

  public:

	explicit ExportDialog (PublicEditor & editor, Glib::ustring title = _("Export"));
	~ExportDialog ();
	
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
	
	HandlerPtr      handler;
	ManagerPtr      profile_manager;

	// initializes GUI layout
	virtual void init_gui ();

	// Must initialize all the shared_ptrs below
	virtual void init_components ();

	boost::shared_ptr<ExportPresetSelector>   preset_selector;
	boost::shared_ptr<ExportTimespanSelector> timespan_selector;
	boost::shared_ptr<ExportChannelSelector>  channel_selector;
	boost::shared_ptr<ExportFileNotebook>     file_notebook;
	
	Gtk::VBox                                 warning_widget;
	Gtk::VBox                                 progress_widget;
	
	Gtk::Label *                              timespan_label;
	Gtk::Label *                              channels_label;

  private:

	void init ();

	void notify_errors ();
	void close_dialog ();
	
	void sync_with_manager ();
	void update_warnings ();
	void show_conflicting_files ();

	void export_rt ();
	void export_fw ();
	
	void show_progress ();
	gint progress_timeout ();
	
	typedef boost::shared_ptr<ARDOUR::ExportStatus> StatusPtr;
	
	PublicEditor &  editor;
	StatusPtr       status;
	
	/*** GUI components ***/
	
	/* Warning area */
	
	Gtk::HBox           warn_hbox;
	Gtk::Label          warn_label;
	Glib::ustring       warn_string;
	
	Gtk::HBox           list_files_hbox;
	Gtk::Label          list_files_label;
	Gtk::Button         list_files_button;
	Glib::ustring       list_files_string;
	
	void add_error (Glib::ustring const & text);
	void add_warning (Glib::ustring const & text);
	
	/* Progress bar */
	
	Gtk::Label              progress_label;
	Gtk::ProgressBar        progress_bar;
	sigc::connection        progress_connection;
	
	/* Buttons */
	
	Gtk::Button *           cancel_button;
	Gtk::Button *           rt_export_button;
	Gtk::Button *           fast_export_button;

};

class ExportRangeDialog : public ExportDialog
{
  public:
	ExportRangeDialog (PublicEditor & editor, Glib::ustring range_id);

  private:
	void init_components ();
	
	Glib::ustring range_id;
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

#endif /* __ardour_export_dialog_h__ */
