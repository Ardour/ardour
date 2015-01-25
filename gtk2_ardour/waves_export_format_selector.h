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

#ifndef __waves_export_format_selector_h__
#define __waves_export_format_selector_h__

#include <string>
#include <gtkmm.h>
#include <sigc++/signal.h>
#include <boost/shared_ptr.hpp>

#include "ardour/export_profile_manager.h"
#include "ardour/session_handle.h"
#include "waves_ui.h"

namespace ARDOUR {
	class ExportFormatSpecification;
	class ExportProfileManager;
}

///
class WavesExportFormatSelector : public Gtk::VBox, public WavesUI, public ARDOUR::SessionHandlePtr
{

  private:

	typedef boost::shared_ptr<ARDOUR::ExportFormatSpecification> FormatPtr;
	typedef std::list<FormatPtr> FormatList;

  public:

	WavesExportFormatSelector ();
	~WavesExportFormatSelector ();

	void set_state (ARDOUR::ExportProfileManager::FormatStatePtr state_, ARDOUR::Session * session_);
	void update_format_list ();

	sigc::signal<void, FormatPtr> FormatEdited;
	sigc::signal<void, FormatPtr> FormatRemoved;
	sigc::signal<FormatPtr, FormatPtr> NewFormat;

	/* Compatibility with other elements */

	sigc::signal<void> CriticalSelectionChanged;

  private:

	void select_format (FormatPtr f);
	void add_new_format (WavesButton*);
	void remove_format (bool called_from_button = false);
	int open_edit_dialog (bool new_dialog = false);
	void update_format_dropdown (WavesDropdown*, int);
	void update_format_description ();
	void on_edit_button (WavesButton*);
	void on_remove_button (WavesButton*);

	ARDOUR::ExportProfileManager::FormatStatePtr state;

	/*** GUI componenets ***/

	struct FormatCols : public Gtk::TreeModelColumnRecord
	{
	  public:
		Gtk::TreeModelColumn<FormatPtr>    format;
		Gtk::TreeModelColumn<std::string>  label;

		FormatCols () { add (format); add (label); }
	};
	FormatCols                   format_cols;
	Glib::RefPtr<Gtk::ListStore> format_list;

	WavesDropdown& _format_dropdown;
	WavesButton& _edit_button;
	WavesButton& _remove_button;
	WavesButton& _new_button;
};

#endif // __waves_export_format_selector_h__
