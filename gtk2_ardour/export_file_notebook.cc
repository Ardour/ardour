/*
 * Copyright (C) 2008-2012 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2012-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014 Colin Fletcher <colin.m.fletcher@googlemail.com>
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

#include "export_file_notebook.h"

#include "ardour/export_format_specification.h"

#include "gui_thread.h"
#include "utils.h"
#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
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
	update_soundcloud_upload ();
	CriticalSelectionChanged ();
}

void
ExportFileNotebook::update_soundcloud_upload ()
{
	int i;
	bool show_credentials_entry = false;
	ExportProfileManager::FormatStateList const & formats = profile_manager->get_formats ();
	ExportProfileManager::FormatStateList::const_iterator format_it;

	for (i = 0, format_it = formats.begin(); format_it != formats.end(); ++i, ++format_it) {
		FilePage * page;
		if ((page = dynamic_cast<FilePage *> (get_nth_page (i)))) {
			bool this_soundcloud_upload = page->get_soundcloud_upload ();
			(*format_it)->format->set_soundcloud_upload (this_soundcloud_upload);
			if (this_soundcloud_upload) {
				show_credentials_entry  = true;
			}
		}
	}
	soundcloud_export_selector->set_visible (show_credentials_entry);
}

void
ExportFileNotebook::FilePage::analysis_changed ()
{
	format_state->format->set_analyse (analysis_button.get_active ());
	profile_manager->save_format_to_disk (format_state->format);
}

void
ExportFileNotebook::FilePage::update_analysis_button ()
{
	analysis_button.set_active (format_state->format->analyse());
}

void
ExportFileNotebook::update_example_filenames ()
{
	int i = 0;
	FilePage * page;
	while ((page = dynamic_cast<FilePage *> (get_nth_page (i++)))) {
		page->update_example_filename();
	}
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
	update_soundcloud_upload ();
}

ExportFileNotebook::FilePage::FilePage (Session * s, ManagerPtr profile_manager, ExportFileNotebook * parent, uint32_t number,
                                        ExportProfileManager::FormatStatePtr format_state,
                                        ExportProfileManager::FilenameStatePtr filename_state) :
	format_state (format_state),
	filename_state (filename_state),
	profile_manager (profile_manager),

	format_label (_("Format"), Gtk::ALIGN_LEFT),
	filename_label (_("Location"), Gtk::ALIGN_LEFT),
	soundcloud_upload_button (_("Upload to Soundcloud")),
	analysis_button (_("Analyze Exported Audio")),
	tab_number (number)
{
	set_border_width (12);

	pack_start (format_label, false, false, 0);
	pack_start (format_align, false, false, 0);
	pack_start (filename_label, false, false, 0);
	pack_start (filename_align, false, false, 0);

	Gtk::HBox *hbox = Gtk::manage (new Gtk::HBox());
	hbox->set_spacing (6);
#ifndef NDEBUG // SoundCloud upload is currently b0rked, needs debugging
	hbox->pack_start (soundcloud_upload_button, false, false, 0);
#endif
	hbox->pack_start (analysis_button, false, false, 0);
	pack_start (*hbox, false, false, 0);

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
	analysis_button.set_active (format_state->format->analyse());
	soundcloud_upload_button.set_active (format_state->format->soundcloud_upload());

	/* Signals */

	tab_close_button.signal_clicked().connect (sigc::bind (sigc::mem_fun (*parent, &ExportFileNotebook::remove_file_page), this));

	profile_manager->FormatListChanged.connect (format_connection, invalidator (*this), boost::bind (&ExportFormatSelector::update_format_list, &format_selector), gui_context());

	format_selector.FormatEdited.connect (sigc::mem_fun (*this, &ExportFileNotebook::FilePage::save_format_to_manager));
	format_selector.FormatRemoved.connect (sigc::mem_fun (*profile_manager, &ExportProfileManager::remove_format_profile));
	format_selector.NewFormat.connect (sigc::mem_fun (*profile_manager, &ExportProfileManager::get_new_format));
	format_selector.FormatReverted.connect (sigc::mem_fun (*profile_manager, &ExportProfileManager::revert_format_profile));

	format_selector.CriticalSelectionChanged.connect (
		sigc::mem_fun (*this, &ExportFileNotebook::FilePage::critical_selection_changed));
	filename_selector.CriticalSelectionChanged.connect (
		sigc::mem_fun (*this, &ExportFileNotebook::FilePage::critical_selection_changed));

	soundcloud_upload_button.signal_toggled().connect (sigc::mem_fun (*parent, &ExportFileNotebook::update_soundcloud_upload));
	soundcloud_button_connection = soundcloud_upload_button.signal_toggled().connect (sigc::mem_fun (*this, &ExportFileNotebook::FilePage::soundcloud_upload_changed));
	analysis_button_connection = analysis_button.signal_toggled().connect (sigc::mem_fun (*this, &ExportFileNotebook::FilePage::analysis_changed));
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
	return _("No format!");
}

bool
ExportFileNotebook::FilePage::get_soundcloud_upload () const
{
#ifdef NDEBUG // SoundCloud upload is currently b0rked, needs debugging
	return false;
#endif
	return soundcloud_upload_button.get_active ();
}

void
ExportFileNotebook::FilePage::soundcloud_upload_changed ()
{
	profile_manager->save_format_to_disk (format_state->format);
}

void
ExportFileNotebook::FilePage::update_soundcloud_upload_button ()
{
	soundcloud_upload_button.set_active (format_state->format->soundcloud_upload());
}

void
ExportFileNotebook::FilePage::save_format_to_manager (FormatPtr format)
{
	profile_manager->save_format_to_disk (format);
}

void
ExportFileNotebook::FilePage::update_tab_label ()
{
	tab_label.set_text (string_compose (_("Format %1: %2"), tab_number, get_format_name()));
}

void
ExportFileNotebook::FilePage::update_example_filename()
{
	if (profile_manager) {
		if (profile_manager->get_timespans().size() > 1
				|| profile_manager->get_timespans().front()->timespans->size() > 1) {
			filename_selector.require_timespan (true);
		} else {
			filename_selector.require_timespan (false);
		}

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

	soundcloud_button_connection.block ();
	analysis_button_connection.block ();

	update_analysis_button();
	update_soundcloud_upload_button();

	analysis_button_connection.unblock ();
	soundcloud_button_connection.unblock ();

	CriticalSelectionChanged();
}
