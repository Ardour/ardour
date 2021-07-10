/*
 * Copyright (C) 2008-2010 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
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

#ifndef __export_format_selector_h__
#define __export_format_selector_h__

#include <string>
#include <sigc++/signal.h>
#include <boost/shared_ptr.hpp>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/combobox.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treemodel.h>

#include "ardour/export_profile_manager.h"
#include "ardour/session_handle.h"

namespace ARDOUR {
	class ExportFormatSpecification;
	class ExportProfileManager;
}

class ExportFormatSelector : public Gtk::HBox, public ARDOUR::SessionHandlePtr
{
private:

	typedef boost::shared_ptr<ARDOUR::ExportFormatSpecification> FormatPtr;
	typedef std::list<FormatPtr> FormatList;

public:

	ExportFormatSelector ();
	~ExportFormatSelector ();

	void set_state (ARDOUR::ExportProfileManager::FormatStatePtr state_, ARDOUR::Session * session_);
	void update_format_list ();

	sigc::signal<void, FormatPtr> FormatEdited;
	sigc::signal<void, FormatPtr> FormatRemoved;
	sigc::signal<FormatPtr, FormatPtr> NewFormat;
	sigc::signal<void, FormatPtr> FormatReverted;

	/* Compatibility with other elements */

	sigc::signal<void> CriticalSelectionChanged;

private:

	void select_format (FormatPtr f);
	void add_new_format ();
	void remove_format (bool called_from_button = false);
	int open_edit_dialog (bool new_dialog = false);
	void update_format_combo ();
	void update_format_description ();

	ARDOUR::ExportProfileManager::FormatStatePtr state;

	/*** GUI components ***/

	struct FormatCols : public Gtk::TreeModelColumnRecord
	{
	public:
		Gtk::TreeModelColumn<FormatPtr>      format;
		Gtk::TreeModelColumn<std::string>  label;

		FormatCols () { add (format); add (label); }
	};
	FormatCols                   format_cols;
	Glib::RefPtr<Gtk::ListStore> format_list;
	Gtk::ComboBox                format_combo;

	Gtk::Button edit_button;
	Gtk::Button remove_button;
	Gtk::Button new_button;
};

#endif /* __export_format_selector_h__ */
