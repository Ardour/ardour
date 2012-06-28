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

#include "export_file_notebook.h"

#include "ardour/export_format_specification.h"

#include "gui_thread.h"
#include "utils.h"
#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

ExportFileNotebook::ExportFileNotebook () :
  page_counter (1)
{
	/* Last page */

	new_file_button.set_image (*Gtk::manage (new Gtk::Image (::get_icon("add"))));
	new_file_button.set_label (_("Add another format"));
	new_file_button.set_alignment (0, 0.5);
	new_file_button.set_relief (Gtk::RELIEF_NONE);

	new_file_hbox.pack_start (new_file_button, true, true);
	append_page (new_file_dummy, new_file_hbox);
	set_tab_label_packing (new_file_dummy, true, true, Gtk::PACK_START);
	new_file_hbox.show_all_children ();

	page_change_connection = signal_switch_page().connect (sigc::mem_fun (*this, &ExportFileNotebook::handle_page_change));
	new_file_button.signal_clicked().connect (sigc::mem_fun (*this, &ExportFileNotebook::add_new_file_page));
}

void
ExportFileNotebook::set_session_and_manager (ARDOUR::Session * s, boost::shared_ptr<ARDOUR::ExportProfileManager> manager)
{
	SessionHandlePtr::set_session (s);
	profile_manager = manager;

	sync_with_manager ();
}

void
ExportFileNotebook::sync_with_manager ()
{
	/* Clear pages from notebook
	   The page switch handling has to be disabled during removing all pages due to a gtk bug
	 */

	page_change_connection.block();
	while (get_n_pages() > 1) {
		remove_page (0);
	}
	page_change_connection.block(false);

	page_counter = 1;
	last_visible_page = 0;

	/* File notebook */

	ExportProfileManager::FormatStateList const & formats = profile_manager->get_formats ();
	ExportProfileManager::FormatStateList::const_iterator format_it;
	ExportProfileManager::FilenameStateList const & filenames = profile_manager->get_filenames ();
	ExportProfileManager::FilenameStateList::const_iterator filename_it;
	for (format_it = formats.begin(), filename_it = filenames.begin();
	     format_it != formats.end() && filename_it != filenames.end();
	     ++format_it, ++filename_it) {
		add_file_page (*format_it, *filename_it);
	}

	set_current_page (0);
	CriticalSelectionChanged ();
}

void
ExportFileNotebook::update_example_filenames()
{
	int i = 0;
	FilePage * page;
	while ((page = dynamic_cast<FilePage *> (get_nth_page (i++)))) {
		page->update_example_filename();
	}
}

std::string
ExportFileNotebook::get_nth_format_name (uint32_t n)
{
	FilePage * page;
	if ((page = dynamic_cast<FilePage *> (get_nth_page (n - 1)))) {
		return page->get_format_name();
	}
	return "";
}

void
ExportFileNotebook::add_new_file_page ()
{
	FilePage * page;
	if ((page = dynamic_cast<FilePage *> (get_nth_page (get_current_page())))) {
		add_file_page (profile_manager->duplicate_format_state (page->get_format_state()),
		               profile_manager->duplicate_filename_state (page->get_filename_state()));
	}
}

void
ExportFileNotebook::add_file_page (ARDOUR::ExportProfileManager::FormatStatePtr format_state, ARDOUR::ExportProfileManager::FilenameStatePtr filename_state)
{
	FilePage * page = Gtk::manage (new FilePage (_session, profile_manager, this, page_counter, format_state, filename_state));
	page->CriticalSelectionChanged.connect (CriticalSelectionChanged.make_slot());
	insert_page (*page, page->get_tab_widget(), get_n_pages() - 1);

	update_remove_file_page_sensitivity ();
	show_all_children();
	++page_counter;

	CriticalSelectionChanged ();
}

void
ExportFileNotebook::remove_file_page (FilePage * page)
{
	profile_manager->remove_format_state (page->get_format_state());
	profile_manager->remove_filename_state (page->get_filename_state());

	remove_page (*page);
	update_remove_file_page_sensitivity ();

	CriticalSelectionChanged ();
}

void
ExportFileNotebook::update_remove_file_page_sensitivity ()
{
	FilePage * page;
	if ((page = dynamic_cast<FilePage *> (get_nth_page (0)))) {
		if (get_n_pages() > 2) {
			page->set_remove_sensitive (true);
		} else {
			page->set_remove_sensitive (false);
		}
	}
}

void
ExportFileNotebook::handle_page_change (GtkNotebookPage*, uint32_t page)
{
	if (page + 1 == (uint32_t) get_n_pages()) {
		set_current_page (last_visible_page);
	} else {
		last_visible_page = page;
	}
}

ExportFileNotebook::FilePage::FilePage (Session * s, ManagerPtr profile_manager, ExportFileNotebook * parent, uint32_t number,
                                        ExportProfileManager::FormatStatePtr format_state,
                                        ExportProfileManager::FilenameStatePtr filename_state) :
  format_state (format_state),
  filename_state (filename_state),
  profile_manager (profile_manager),

  format_label (_("Format"), Gtk::ALIGN_LEFT),
  filename_label (_("Location"), Gtk::ALIGN_LEFT),
  tab_number (number)
{
	set_border_width (12);

	pack_start (format_label, false, false, 0);
	pack_start (format_align, false, false, 0);
	pack_start (filename_label, false, false, 0);
	pack_start (filename_align, false, false, 0);

	format_align.add (format_selector);
	format_align.set_padding (6, 12, 18, 0);

	filename_align.add (filename_selector);
	filename_align.set_padding (0, 12, 18, 0);

	Pango::AttrList bold;
	Pango::Attribute b = Pango::Attribute::create_attr_weight (Pango::WEIGHT_BOLD);
	bold.insert (b);

	format_label.set_attributes (bold);
	filename_label.set_attributes (bold);
	tab_label.set_attributes (bold);

	/* Set states */
	format_selector.set_state (format_state, s);
 	filename_selector.set_state (filename_state, s);

	/* Signals */

	tab_close_button.signal_clicked().connect (sigc::bind (sigc::mem_fun (*parent, &ExportFileNotebook::remove_file_page), this));

	profile_manager->FormatListChanged.connect (format_connection, invalidator (*this), boost::bind (&ExportFormatSelector::update_format_list, &format_selector), gui_context());

	format_selector.FormatEdited.connect (sigc::mem_fun (*this, &ExportFileNotebook::FilePage::save_format_to_manager));
	format_selector.FormatRemoved.connect (sigc::mem_fun (*profile_manager, &ExportProfileManager::remove_format_profile));
	format_selector.NewFormat.connect (sigc::mem_fun (*profile_manager, &ExportProfileManager::get_new_format));

	format_selector.CriticalSelectionChanged.connect (
		sigc::mem_fun (*this, &ExportFileNotebook::FilePage::critical_selection_changed));
	filename_selector.CriticalSelectionChanged.connect (
		sigc::mem_fun (*this, &ExportFileNotebook::FilePage::critical_selection_changed));

	/* Tab widget */

	tab_close_button.add (*Gtk::manage (new Gtk::Image (::get_icon("close"))));
	tab_close_alignment.add (tab_close_button);
	tab_close_alignment.set (0.5, 0.5, 0, 0);

	tab_widget.pack_start (tab_label, false, false, 3);
	tab_widget.pack_end (tab_close_alignment, false, false, 0);
	tab_widget.show_all_children ();
	update_tab_label ();
	update_example_filename();

	/* Done */

	show_all_children ();
}

ExportFileNotebook::FilePage::~FilePage ()
{
}

void
ExportFileNotebook::FilePage::set_remove_sensitive (bool value)
{
	tab_close_button.set_sensitive (value);
}

std::string
ExportFileNotebook::FilePage::get_format_name () const
{
	if (format_state && format_state->format) {
		return format_state->format->name();
	}
	return "No format!";
}

void
ExportFileNotebook::FilePage::save_format_to_manager (FormatPtr format)
{
	profile_manager->save_format_to_disk (format);
}

void
ExportFileNotebook::FilePage::update_tab_label ()
{
	tab_label.set_text (string_compose ("Format %1: %2", tab_number, get_format_name()));
}

void
ExportFileNotebook::FilePage::update_example_filename()
{
	if (profile_manager) {

		std::string example;
		if (format_state->format) {
			example = profile_manager->get_sample_filename_for_format (
				filename_state->filename, format_state->format);
		}
		
		if (example != "") {
			filename_selector.set_example_filename(Glib::path_get_basename (example));
		} else {
			filename_selector.set_example_filename("");
		}
	}
}

void
ExportFileNotebook::FilePage::critical_selection_changed ()
{
	update_tab_label();
	update_example_filename();
	CriticalSelectionChanged();
}
