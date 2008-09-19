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

#include "export_main_dialog.h"

#include <sigc++/signal.h>

#include <pbd/filesystem.h>

#include "utils.h"

#include <ardour/export_handler.h>
#include <ardour/export_filename.h>
#include <ardour/export_format_specification.h>
#include <ardour/export_channel_configuration.h>
#include <ardour/export_preset.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

ExportMainDialog::ExportMainDialog (PublicEditor & editor) :
  ArdourDialog (_("Export")),
  editor (editor),

  preset_label (_("Preset:"), Gtk::ALIGN_LEFT),
  preset_save_button (Gtk::Stock::SAVE),
  preset_remove_button (Gtk::Stock::REMOVE),
  preset_new_button (Gtk::Stock::NEW),

  page_counter (1),

  warn_label ("", Gtk::ALIGN_LEFT),
  list_files_label (_("<span color=\"#ffa755\">Some already existing files will be overwritten.</span>"), Gtk::ALIGN_RIGHT),
  list_files_button (_("List files")),

  timespan_label (_("Time Span"), Gtk::ALIGN_LEFT),

  channels_label (_("Channels"), Gtk::ALIGN_LEFT)
{
	/* Main packing */

	get_vbox()->pack_start (preset_align, false, false, 0);
	get_vbox()->pack_start (timespan_label, false, false, 0);
	get_vbox()->pack_start (timespan_align, false, false, 0);
	get_vbox()->pack_start (channels_label, false, false, 0);
	get_vbox()->pack_start (channels_align, false, false, 0);
	get_vbox()->pack_start (file_notebook, false, false, 0);
	get_vbox()->pack_start (warn_container, true, true, 0);
	get_vbox()->pack_start (progress_container, true, true, 0);
	
	timespan_align.add (timespan_selector);
	timespan_align.set_padding (0, 12, 18, 0);
	
	channels_align.add (channel_selector);
	channels_align.set_padding (0, 12, 18, 0);
	
	/* Preset manipulation */
	
	preset_list = Gtk::ListStore::create (preset_cols);
	preset_entry.set_model (preset_list);
	preset_entry.set_text_column (preset_cols.label);
	
	preset_align.add (preset_hbox);
	preset_align.set_padding (0, 12, 0, 0);
	
	preset_hbox.pack_start (preset_label, false, false, 0);
	preset_hbox.pack_start (preset_entry, true, true, 6);
	preset_hbox.pack_start (preset_save_button, false, false, 0);
	preset_hbox.pack_start (preset_remove_button, false, false, 6);
	preset_hbox.pack_start (preset_new_button, false, false, 0);
	
	preset_save_button.set_sensitive (false);
	preset_remove_button.set_sensitive (false);
	preset_new_button.set_sensitive (false);
	
	preset_select_connection = preset_entry.signal_changed().connect (sigc::mem_fun (*this, &ExportMainDialog::select_preset));
	preset_save_button.signal_clicked().connect (sigc::mem_fun (*this, &ExportMainDialog::save_current_preset));
	preset_new_button.signal_clicked().connect (sigc::mem_fun (*this, &ExportMainDialog::save_current_preset));
	preset_remove_button.signal_clicked().connect (sigc::mem_fun (*this, &ExportMainDialog::remove_current_preset));
	
	/* warnings */
	
	warn_container.pack_start (warn_hbox, true, true, 6);
	warn_container.pack_end (list_files_hbox, false, false, 0);
	
	warn_hbox.pack_start (warn_label, true, true, 16);
	warn_label.set_use_markup (true);
	
	list_files_hbox.pack_end (list_files_button, false, false, 6);
	list_files_hbox.pack_end (list_files_label, false, false, 6);
	list_files_label.set_use_markup (true);
	
	list_files_button.signal_clicked().connect (sigc::mem_fun (*this, &ExportMainDialog::show_conflicting_files));
	
	/* Progress indicators */
	
	progress_container.pack_start (progress_label, false, false, 6);
	progress_container.pack_start (progress_bar, false, false, 6);
	
	/* Buttons */
	
	cancel_button = add_button (Gtk::Stock::CANCEL, RESPONSE_CANCEL);
	rt_export_button = add_button (_("Realtime export"), RESPONSE_RT);
	fast_export_button = add_button (_("Fast Export"), RESPONSE_FAST);
	
	cancel_button->signal_clicked().connect (sigc::mem_fun (*this, &ExportMainDialog::close_dialog));
	rt_export_button->signal_clicked().connect (sigc::mem_fun (*this, &ExportMainDialog::export_rt));
	fast_export_button->signal_clicked().connect (sigc::mem_fun (*this, &ExportMainDialog::export_fw));
	
	/* Bolding for labels */
	
	Pango::AttrList bold;
	Pango::Attribute b = Pango::Attribute::create_attr_weight (Pango::WEIGHT_BOLD);
	bold.insert (b);
	
	timespan_label.set_attributes (bold);
	channels_label.set_attributes (bold);
	
	/* Done! */
	
	show_all_children ();
	progress_container.hide_all();
}

ExportMainDialog::~ExportMainDialog ()
{
	if (session) {
		session->release_export_handler();
	}
}

void
ExportMainDialog::set_session (ARDOUR::Session* s)
{
	session = s;
	
	/* Init handler and profile manager */
	
	handler = session->get_export_handler ();
	profile_manager.reset (new ExportProfileManager (*session));
	
	/* Selection range  */
	
	TimeSelection const & time (editor.get_selection().time);
	if (!time.empty()) {
		profile_manager->set_selection_range (time.front().start, time.front().end);
	} else {
		profile_manager->set_selection_range ();
	}
	
	/* Last notebook page */
	
	new_file_button.add (*Gtk::manage (new Gtk::Image (::get_icon("add"))));
	new_file_button.set_alignment (0, 0.5);
	new_file_button.set_relief (Gtk::RELIEF_NONE);
	
	new_file_hbox.pack_start (new_file_button, true, true);
	file_notebook.append_page (new_file_dummy, new_file_hbox);
	file_notebook.set_tab_label_packing (new_file_dummy, true, true, Gtk::PACK_START);
	new_file_hbox.show_all_children ();
	
	page_change_connection = file_notebook.signal_switch_page().connect (sigc::mem_fun (*this, &ExportMainDialog::handle_page_change));
	new_file_button.signal_clicked().connect (sigc::mem_fun (*this, &ExportMainDialog::add_new_file_page));
	
	/* Load states */
	
	profile_manager->load_profile ();
	sync_with_manager ();
	
	/* Warnings */
	
	timespan_selector.CriticalSelectionChanged.connect (sigc::mem_fun (*this, &ExportMainDialog::update_warnings));
	channel_selector.CriticalSelectionChanged.connect (sigc::mem_fun (*this, &ExportMainDialog::update_warnings));
	
	update_warnings ();
}

void
ExportMainDialog::select_timespan (Glib::ustring id)
{
	set_title ("Export Range");
	timespan_selector.select_one_range (id);
}

void
ExportMainDialog::close_dialog ()
{
	ExportStatus & status = session->export_status;

	if (status.running) {
		status.abort();
	}
	
	hide_all ();
	set_modal (false);
	
}

void
ExportMainDialog::sync_with_manager ()
{
	/* Clear pages from notebook
	   The page switch handling has to be disabled during removing all pages due to a gtk bug
	 */

	page_change_connection.block();
	while (file_notebook.get_n_pages() > 1) {
		file_notebook.remove_page (0);
	}
	page_change_connection.block(false);
	
	page_counter = 1;
	last_visible_page = 0;

	/* Preset list */
	
	preset_list->clear();
	
	PresetList const & presets = profile_manager->get_presets();
	Gtk::ListStore::iterator tree_it;
	
	for (PresetList::const_iterator it = presets.begin(); it != presets.end(); ++it) {
		tree_it = preset_list->append();
		tree_it->set_value (preset_cols.preset, *it);
		tree_it->set_value (preset_cols.label, Glib::ustring ((*it)->name()));
		
		if (*it == current_preset) {
			preset_select_connection.block (true);
			preset_entry.set_active (tree_it);
			preset_select_connection.block (false);
		}
	}	

	/* Timespan and channel config */

	timespan_selector.set_state (profile_manager->get_timespans().front(), session);
	channel_selector.set_state (profile_manager->get_channel_configs().front(), session);

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
	
	file_notebook.set_current_page (0);
	update_warnings ();
}

void
ExportMainDialog::update_warnings ()
{
	/* Reset state */

	warn_string = "";
	warn_label.set_markup (warn_string);

	list_files_hbox.hide ();
	list_files_string = "";
	
	fast_export_button->set_sensitive (true);
	rt_export_button->set_sensitive (true);

	/* Add new warnings */

	boost::shared_ptr<ExportProfileManager::Warnings> warnings = profile_manager->get_warnings();

	for (std::list<Glib::ustring>::iterator it = warnings->errors.begin(); it != warnings->errors.end(); ++it) {
		add_error (*it);
	}

	for (std::list<Glib::ustring>::iterator it = warnings->warnings.begin(); it != warnings->warnings.end(); ++it) {
		add_warning (*it);
	}

	if (!warnings->conflicting_filenames.empty()) {
		list_files_hbox.show ();
		for (std::list<Glib::ustring>::iterator it = warnings->conflicting_filenames.begin(); it != warnings->conflicting_filenames.end(); ++it) {
			ustring::size_type pos = it->find_last_of ("/");
			list_files_string += "\n" + it->substr (0, pos + 1) + "<b>" + it->substr (pos + 1) + "</b>";
		}
	}
}

void
ExportMainDialog::show_conflicting_files ()
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
ExportMainDialog::export_rt ()
{
	profile_manager->prepare_for_export ();
	handler->do_export (true);
	show_progress ();
}

void
ExportMainDialog::export_fw ()
{
	profile_manager->prepare_for_export ();
	handler->do_export (false);
	show_progress ();
}

void
ExportMainDialog::show_progress ()
{
	ARDOUR::ExportStatus & status = session->export_status;
	status.running = true;

	cancel_button->set_label (_("Stop Export"));
	rt_export_button->set_sensitive (false);
	fast_export_button->set_sensitive (false);

	progress_bar.set_fraction (0.0);
	warn_container.hide_all();
	progress_container.show ();
	progress_container.show_all_children ();
	progress_connection = Glib::signal_timeout().connect (mem_fun(*this, &ExportMainDialog::progress_timeout), 100);
	
	gtk_main_iteration ();
	while (status.running) {
		if (gtk_events_pending()) {
			gtk_main_iteration ();
		} else {
			usleep (10000);
		}
	}
}

Glib::ustring
ExportMainDialog::get_nth_format_name (uint32_t n)
{
	FilePage * page;
	if ((page = dynamic_cast<FilePage *> (file_notebook.get_nth_page (n - 1)))) {
		return page->get_format_name();
	}
	return "";
}

gint
ExportMainDialog::progress_timeout ()
{
	ARDOUR::ExportStatus & status = session->export_status;

	switch (status.stage) {
	  case export_None:
		progress_label.set_text ("");
		break;
	  case export_ReadTimespan:
		progress_label.set_text (string_compose (_("Reading timespan %1 of %2"), status.timespan, status.total_timespans));
		break;
	  case export_PostProcess:
		progress_label.set_text (string_compose (_("Processing file %2 of %3 (%1) from timespan %4 of %5"),
		                                         get_nth_format_name (status.format),
		                                         status.format, status.total_formats,
		                                         status.timespan, status.total_timespans));
		break;
	  case export_Write:
		progress_label.set_text (string_compose (_("Encoding file %2 of %3 (%1) from timespan %4 of %5"),
		                                         get_nth_format_name (status.format),
		                                         status.format, status.total_formats,
		                                         status.timespan, status.total_timespans));
		break;
	}

	progress_bar.set_fraction (status.progress);
	return TRUE;
}

void
ExportMainDialog::select_preset ()
{
	Gtk::ListStore::iterator it = preset_entry.get_active ();
	Glib::ustring text = preset_entry.get_entry()->get_text();
	
	if (preset_list->iter_is_valid (it)) {
	
		previous_preset = current_preset = it->get_value (preset_cols.preset);
		profile_manager->load_preset (current_preset);
		sync_with_manager ();
		
		/* Make an edit, so that signal changed will be emitted on re-selection */
		
		preset_select_connection.block (true);
		preset_entry.get_entry()->set_text ("");
		preset_entry.get_entry()->set_text (text);
		preset_select_connection.block (false);
	
	} else { // Text has been edited 
		if (previous_preset && !text.compare (previous_preset->name())) {
			
			current_preset = previous_preset;
			
		} else {
			current_preset.reset ();
			profile_manager->load_preset (current_preset);
		}
	}
	
	preset_save_button.set_sensitive (current_preset);
	preset_remove_button.set_sensitive (current_preset);
	preset_new_button.set_sensitive (!current_preset && !text.empty());
}

void
ExportMainDialog::save_current_preset ()
{
	if (!profile_manager) { return; }
	
	previous_preset = current_preset = profile_manager->save_preset (preset_entry.get_entry()->get_text());
	sync_with_manager ();
	select_preset (); // Update preset widget states
}

void
ExportMainDialog::remove_current_preset ()
{
	if (!profile_manager) { return; }
	
	profile_manager->remove_preset();
	preset_entry.get_entry()->set_text ("");
	sync_with_manager ();
}

ExportMainDialog::FilePage::FilePage (Session * s, ManagerPtr profile_manager, ExportMainDialog * parent, uint32_t number,
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
	
	tab_close_button.signal_clicked().connect (sigc::bind (sigc::mem_fun (*parent, &ExportMainDialog::remove_file_page), this));
	
	profile_manager->FormatListChanged.connect (sigc::mem_fun (format_selector, &ExportFormatSelector::update_format_list));
	
	format_selector.FormatEdited.connect (sigc::mem_fun (*this, &ExportMainDialog::FilePage::save_format_to_manager));
	format_selector.FormatRemoved.connect (sigc::mem_fun (*profile_manager, &ExportProfileManager::remove_format_profile));
	format_selector.NewFormat.connect (sigc::mem_fun (*profile_manager, &ExportProfileManager::get_new_format));
	
	format_selector.CriticalSelectionChanged.connect (sigc::mem_fun (*this, &ExportMainDialog::FilePage::update_tab_label));
	filename_selector.CriticalSelectionChanged.connect (CriticalSelectionChanged.make_slot());
	
	/* Tab widget */
	
	tab_close_button.add (*Gtk::manage (new Gtk::Image (::get_icon("close"))));
	tab_close_alignment.add (tab_close_button);
	tab_close_alignment.set (0.5, 0.5, 0, 0);
	
	tab_widget.pack_start (tab_label, false, false, 3);
	tab_widget.pack_end (tab_close_alignment, false, false, 0);
	tab_widget.show_all_children ();
	update_tab_label ();
	
	/* Done */
	
	show_all_children ();
}

ExportMainDialog::FilePage::~FilePage ()
{
}

void
ExportMainDialog::FilePage::set_remove_sensitive (bool value)
{
	tab_close_button.set_sensitive (value);
}

Glib::ustring
ExportMainDialog::FilePage::get_format_name () const
{
	if (format_state && format_state->format) {
		return format_state->format->name();
	}
	return "No format!";
}

void
ExportMainDialog::FilePage::save_format_to_manager (FormatPtr format)
{
	profile_manager->save_format_to_disk (format);
}

void
ExportMainDialog::FilePage::update_tab_label ()
{
	tab_label.set_text (string_compose ("%1 %2", tab_number, get_format_name()));
	CriticalSelectionChanged();
}

void
ExportMainDialog::add_new_file_page ()
{
	FilePage * page;
	if ((page = dynamic_cast<FilePage *> (file_notebook.get_nth_page (file_notebook.get_current_page())))) {
		add_file_page (profile_manager->duplicate_format_state (page->get_format_state()),
		               profile_manager->duplicate_filename_state (page->get_filename_state()));
	}
}

void
ExportMainDialog::add_file_page (ExportProfileManager::FormatStatePtr format_state, ExportProfileManager::FilenameStatePtr filename_state)
{
	FilePage * page = Gtk::manage (new FilePage (session, profile_manager, this, page_counter, format_state, filename_state));
	page->CriticalSelectionChanged.connect (sigc::mem_fun (*this, &ExportMainDialog::update_warnings));
	file_notebook.insert_page (*page, page->get_tab_widget(), file_notebook.get_n_pages() - 1);

	update_remove_file_page_sensitivity ();
	file_notebook.show_all_children();
	++page_counter;
	
	update_warnings ();
}

void
ExportMainDialog::remove_file_page (FilePage * page)
{
	profile_manager->remove_format_state (page->get_format_state());
	profile_manager->remove_filename_state (page->get_filename_state());

	file_notebook.remove_page (*page);
	update_remove_file_page_sensitivity ();
	
	update_warnings ();
}

void
ExportMainDialog::update_remove_file_page_sensitivity ()
{
	FilePage * page;
	if ((page = dynamic_cast<FilePage *> (file_notebook.get_nth_page (0)))) {
		if (file_notebook.get_n_pages() > 2) {
			page->set_remove_sensitive (true);
		} else {
			page->set_remove_sensitive (false);
		}
	}
}

void
ExportMainDialog::handle_page_change (GtkNotebookPage*, uint page)
{
	if (page + 1 == (uint32_t) file_notebook.get_n_pages()) {
		file_notebook.set_current_page (last_visible_page);
	} else {
		last_visible_page = page;
	}
}

void
ExportMainDialog::add_error (Glib::ustring const & text)
{
	fast_export_button->set_sensitive (false);
	rt_export_button->set_sensitive (false);
	
	if (warn_string.empty()) {
		warn_string = _("<span color=\"#ffa755\">Error: ") + text + "</span>";
	} else {
		warn_string = _("<span color=\"#ffa755\">Error: ") + text + "</span>\n" + warn_string;
	}
	
	warn_label.set_markup (warn_string);
}

void
ExportMainDialog::add_warning (Glib::ustring const & text)
{
	if (warn_string.empty()) {
		warn_string = _("<span color=\"#ffa755\">Warning: ") + text + "</span>";
	} else {
		warn_string = warn_string + _("\n<span color=\"#ffa755\">Warning: ") + text + "</span>";
	}
	
	warn_label.set_markup (warn_string);
}
