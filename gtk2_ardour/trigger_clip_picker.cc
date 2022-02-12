/*
 * Copyright (C) 2019-2021 Robin Gareus <robin@gareus.org>
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

#include <cassert>
#include <vector>

#include <gtkmm/stock.h>

#include "pbd/basename.h"
#include "pbd/file_utils.h"
#include "pbd/openuri.h"
#include "pbd/pathexpand.h"
#include "pbd/search_path.h"
#include "pbd/unwind.h"

#include "ardour/audiofilesource.h"
#include "ardour/audioregion.h"
#include "ardour/auditioner.h"
#include "ardour/clip_library.h"
#include "ardour/directory_names.h"
#include "ardour/filesystem_paths.h"
#include "ardour/midi_region.h"
#include "ardour/plugin_insert.h"
#include "ardour/region_factory.h"
#include "ardour/smf_source.h"
#include "ardour/source_factory.h"
#include "ardour/srcfilesource.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/menu_elems.h"
#include "gtkmm2ext/utils.h"

#include "widgets/paths_dialog.h"
#include "widgets/tooltips.h"
#include "widgets/ardour_icon.h"

#include "ardour_ui.h"
#include "plugin_ui.h"
#include "timers.h"
#include "trigger_clip_picker.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace PBD;
using namespace ARDOUR;

TriggerClipPicker::TriggerClipPicker ()
	: _fcd (_("Select Sample Folder"), FILE_CHOOSER_ACTION_SELECT_FOLDER)
	, _seek_slider (0, 1000, 1)
	, _autoplay_btn (_("Auto-play"))
	, _auditioner_combo (InstrumentSelector::ForAuditioner)
	, _clip_library_listed (false)
	, _ignore_list_dir (false)
	, _seeking (false)
	, _audition_plugnui (0)
{
	/* Setup Dropdown / File Browser */
#ifdef __APPLE__
	try {
		/* add_shortcut_folder throws an exception if the folder being added already has a shortcut */
		_fcd.add_shortcut_folder_uri ("file:///Library/GarageBand/Apple Loops");
		_fcd.add_shortcut_folder_uri ("file:///Library/Audio/Apple Loops");
		_fcd.add_shortcut_folder_uri ("file:///Library/Application Support/GarageBand/Instrument Library/Sampler/Sampler Files");
	}
	catch (Glib::Error & e) {}
#endif

	Gtkmm2ext::add_volume_shortcuts (_fcd);

	_fcd.add_button (Stock::CANCEL, RESPONSE_CANCEL);
	_fcd.add_button (Stock::ADD, RESPONSE_ACCEPT);
	_fcd.add_button (Stock::OPEN, RESPONSE_OK);

	refill_dropdown ();

	/* Audition */
	 _autoplay_btn.set_active (UIConfiguration::instance ().get_autoplay_clips ());

	_seek_slider.set_draw_value (false);

	_seek_slider.add_events (Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);
	_seek_slider.signal_button_press_event ().connect (sigc::mem_fun (*this, &TriggerClipPicker::seek_button_press), false);
	_seek_slider.signal_button_release_event ().connect (sigc::mem_fun (*this, &TriggerClipPicker::seek_button_release), false);

	_play_btn.set_name ("generic button");
	_play_btn.set_icon (ArdourWidgets::ArdourIcon::TransportPlay);
	_play_btn.signal_clicked.connect (sigc::mem_fun (*this, &TriggerClipPicker::audition_selected));

	_stop_btn.set_name ("generic button");
	_stop_btn.set_icon (ArdourWidgets::ArdourIcon::TransportStop);
	_stop_btn.signal_clicked.connect (sigc::mem_fun (*this, &TriggerClipPicker::stop_audition));

	_open_library_btn.set_name ("generic button");
	_open_library_btn.set_icon (ArdourWidgets::ArdourIcon::Folder);
	_open_library_btn.signal_clicked.connect (sigc::mem_fun (*this, &TriggerClipPicker::open_library));
	_open_library_btn.set_no_show_all ();

	_show_plugin_btn.set_name ("generic button");
	_show_plugin_btn.set_icon (ArdourWidgets::ArdourIcon::PsetBrowse);
	_show_plugin_btn.signal_clicked.connect (sigc::mem_fun (*this, &TriggerClipPicker::audition_show_plugin_ui));
	_show_plugin_btn.set_sensitive (false);

	_play_btn.set_sensitive (false);
	_stop_btn.set_sensitive (false);

	_autoplay_btn.set_can_focus(false);
	_autoplay_btn.signal_toggled ().connect (sigc::mem_fun (*this, &TriggerClipPicker::autoplay_toggled));

	auditioner_combo_changed();
	_auditioner_combo.signal_changed().connect(sigc::mem_fun(*this, &TriggerClipPicker::auditioner_combo_changed) );

	ArdourWidgets::set_tooltip (_play_btn, _("Audition selected clip"));
	ArdourWidgets::set_tooltip (_stop_btn, _("Stop the audition"));
	ArdourWidgets::set_tooltip (_open_library_btn, _("Open clip library folder"));

	format_text.set_alignment(Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER);
	channels_value.set_alignment(Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER);
	_midi_prop_table.attach (format_text,       0, 1, 0, 1, EXPAND | FILL, SHRINK);
	_midi_prop_table.attach (channels_value,    0, 1, 1, 2, EXPAND | FILL, SHRINK);
	_midi_prop_table.attach (_auditioner_combo, 0, 3, 2, 3, EXPAND | FILL, SHRINK);
	_midi_prop_table.attach (_show_plugin_btn,  3, 4, 2, 3, SHRINK, SHRINK);
	_midi_prop_table.set_border_width (4);
	_midi_prop_table.set_spacings (4);

	/* Layout */
	int r = 0;
	_auditable.set_homogeneous(false);
	_auditable.attach (_play_btn,         0, 1, r,r+1, SHRINK, SHRINK);
	_auditable.attach (_stop_btn,         1, 2, r,r+1, SHRINK, SHRINK);
	_auditable.attach (_autoplay_btn,     2, 3, r,r+1, EXPAND | FILL, SHRINK);  r++;
	_auditable.attach (_seek_slider,      0, 4, r,r+1, EXPAND | FILL, SHRINK);  r++;
	_auditable.attach (_midi_prop_table,  0, 4, r,r+1, EXPAND | FILL, SHRINK);
	_auditable.set_border_width (4);
	_auditable.set_spacings (4);

	_scroller.set_policy (POLICY_AUTOMATIC, POLICY_AUTOMATIC);
	_scroller.add (_view);

	Gtk::Table *dir_table = manage(new Gtk::Table());
	dir_table->set_border_width(4);
	dir_table->set_spacings(4);
	dir_table->attach (_clip_dir_menu,    0, 1, 0, 1, EXPAND | FILL, SHRINK);
	dir_table->attach (_open_library_btn, 1, 2, 0, 1, SHRINK, SHRINK);

	pack_start (*dir_table, false, false);
	pack_start (_scroller);
	pack_start (_auditable, false, false);

	/* TreeView */
	_model = TreeStore::create (_columns);
	_view.set_model (_model);
	_view.append_column (_("File Name"), _columns.name);
	_view.set_headers_visible (false);  //TODO: show headers when we have size/tags/etc
	_view.set_reorderable (false);
	_view.get_selection ()->set_mode (SELECTION_MULTIPLE);

	/* DnD source */
	std::vector<TargetEntry> dnd;
	dnd.push_back (TargetEntry ("text/uri-list"));
	_view.drag_source_set (dnd, Gdk::MODIFIER_MASK, Gdk::ACTION_COPY);
	_view.get_selection ()->signal_changed ().connect (sigc::mem_fun (*this, &TriggerClipPicker::row_selected));
	_view.signal_row_activated ().connect (sigc::mem_fun (*this, &TriggerClipPicker::row_activated));
	_view.signal_test_expand_row ().connect (sigc::mem_fun (*this, &TriggerClipPicker::test_expand));
	_view.signal_row_collapsed ().connect (sigc::mem_fun (*this, &TriggerClipPicker::row_collapsed));
	_view.signal_drag_data_get ().connect (sigc::mem_fun (*this, &TriggerClipPicker::drag_data_get));
	_view.signal_cursor_changed ().connect (sigc::mem_fun (*this, &TriggerClipPicker::cursor_changed));
	_view.signal_drag_end ().connect (sigc::mem_fun (*this, &TriggerClipPicker::drag_end));

	/* DnD target */
	std::vector<Gtk::TargetEntry> target_table;
	target_table.push_back (Gtk::TargetEntry ("x-ardour/region.pbdid", Gtk::TARGET_SAME_APP));
	target_table.push_back (TargetEntry ("text/uri-list"));
	_view.drag_dest_set (target_table, DEST_DEFAULT_ALL, Gdk::ACTION_COPY);
	_view.signal_drag_begin ().connect (sigc::mem_fun (*this, &TriggerClipPicker::drag_begin));
	_view.signal_drag_motion ().connect (sigc::mem_fun (*this, &TriggerClipPicker::drag_motion));
	_view.signal_drag_data_received ().connect (sigc::mem_fun (*this, &TriggerClipPicker::drag_data_received));

	Config->ParameterChanged.connect (_config_connection, invalidator (*this), boost::bind (&TriggerClipPicker::parameter_changed, this, _1), gui_context ());
	LibraryClipAdded.connect (_clip_added_connection, invalidator (*this), boost::bind (&TriggerClipPicker::clip_added, this, _1, _2), gui_context ());

	/* cache value */
	_clip_library_dir = clip_library_dir ();

	/* show off */
	_scroller.show ();
	_view.show ();
	_clip_dir_menu.show ();
	_auditable.show ();

	/* fill treeview with data */
	_clip_dir_menu.items ().front ().activate ();
}

TriggerClipPicker::~TriggerClipPicker ()
{
	_idle_connection.disconnect ();
}

void
TriggerClipPicker::auditioner_combo_changed()
{
	if (_session) {
		_session->the_auditioner()->set_audition_synth_info( _auditioner_combo.selected_instrument() );
	}
}


void
TriggerClipPicker::parameter_changed (std::string const& p)
{
	if (p == "sample-lib-path") {
		refill_dropdown ();
	} else if (p == "clip-library-dir") {
		_clip_library_dir = clip_library_dir ();
		refill_dropdown ();
	}
}

void
TriggerClipPicker::clip_added (std::string const&, void* src)
{
	if (!_clip_library_listed) {
		_clip_library_dir = clip_library_dir ();
		refill_dropdown ();
	}
	if (src == this) {
		list_dir (clip_library_dir ());
	} else {
		list_dir (_current_path);
	}
}

/* ****************************************************************************
 * Paths Dropdown Callbacks
 */

void
TriggerClipPicker::edit_path ()
{
	Gtk::Window* tlw = dynamic_cast<Gtk::Window*> (get_toplevel ());
	assert (tlw);
	ArdourWidgets::PathsDialog pd (*tlw, _("Edit Sample Library Path"), Config->get_sample_lib_path (), "");
	if (pd.run () != Gtk::RESPONSE_ACCEPT) {
		return;
	}
	Config->set_sample_lib_path (pd.get_serialized_paths ());
}

void
TriggerClipPicker::refill_dropdown ()
{
	_clip_dir_menu.clear_items ();
	_root_paths.clear ();

	/* Bundled Content */
	Searchpath spath (ardour_data_search_path ());
	spath.add_subdirectory_to_paths (media_dir_name);
	for (auto const& f : spath) {
		maybe_add_dir (f);
	}

	/* User config folder */
	maybe_add_dir (Glib::build_filename (user_config_directory (), media_dir_name));

	/* Anything added by Gtkmm2ext::add_volume_shortcuts */
	for (auto const& f : _fcd.list_shortcut_folders ()) {
		maybe_add_dir (f);
	}

	/* Custom Paths */
	assert (_clip_dir_menu.items ().size () > 0);
	if (!Config->get_sample_lib_path ().empty ()) {
		_clip_dir_menu.AddMenuElem (Menu_Helpers::SeparatorElem ());
		Searchpath cpath (Config->get_sample_lib_path ());
		for (auto const& f : cpath) {
			maybe_add_dir (f);
		}
	}

	_clip_library_listed = maybe_add_dir (clip_library_dir (false));

	_clip_dir_menu.AddMenuElem (Menu_Helpers::SeparatorElem ());
	_clip_dir_menu.AddMenuElem (Menu_Helpers::MenuElem (_("Edit..."), sigc::mem_fun (*this, &TriggerClipPicker::edit_path)));
	_clip_dir_menu.AddMenuElem (Menu_Helpers::MenuElem (_("Other..."), sigc::mem_fun (*this, &TriggerClipPicker::open_dir)));
}

static bool
is_subfolder (std::string const& parent, std::string dir)
{
	assert (Glib::file_test (dir, Glib::FILE_TEST_IS_DIR | Glib::FILE_TEST_EXISTS));
	assert (Glib::file_test (parent, Glib::FILE_TEST_IS_DIR | Glib::FILE_TEST_EXISTS));

	if (parent.size () > dir.size ()) {
		return false;
	}
	if (parent == dir) {
		return false;
	}
	if (dir == Glib::path_get_dirname (dir)) {
		/* dir must be root */
		return false;
	}
	while (parent.size () < dir.size ()) {
		/* step up, compare with parent */
		dir = Glib::path_get_dirname (dir);
		if (parent == dir) {
			return true;
		}
	}
	return false;
}

static std::string
display_name (std::string const& dir) {
	std::string metadata = Glib::build_filename (dir, ".daw-meta.xml");
	if (Glib::file_test (metadata, Glib::FILE_TEST_IS_REGULAR | Glib::FILE_TEST_EXISTS)) {
		XMLTree tree;
		if (tree.read (metadata) && tree.root()->name () == "DAWDirectory") {
			XMLNode* root = tree.root();
			std::string type;
			if (root->get_property ("type", type)) {
				if (type == "bundled") {
					return string_compose (_("%1 Bundled Content"), PROGRAM_NAME);
				}
			}
#if ENABLE_NLS
			if (ARDOUR::translations_are_enabled ()) {
				for (XMLNodeList::const_iterator n = root->children ("title").begin (); n != root->children ("title").end (); ++n) {
					std::string lang;
					if (!(*n)->get_property ("lang", lang)) {
						continue;
					}
					if (lang != "en_US") { // TODO: get current lang
						continue;
					}
					return (*n)->child_content ();
				}
			}
#endif
			/* pick first title, if any */
			XMLNode* child = root->child ("title");
			if (child) {
				return child->child_content ();
			}
		}
	}
	return Glib::path_get_basename (dir);
}

bool
TriggerClipPicker::maybe_add_dir (std::string const& dir)
{
	if (dir.empty () || !Glib::file_test (dir, Glib::FILE_TEST_IS_DIR | Glib::FILE_TEST_EXISTS)) {
		return false;
	}

	_clip_dir_menu.AddMenuElem (Gtkmm2ext::MenuElemNoMnemonic (display_name (dir), sigc::bind (sigc::mem_fun (*this, &TriggerClipPicker::list_dir), dir, (Gtk::TreeNodeChildren*)0)));

	/* check if a parent path of the given dir already exists,
	 * or if this new path is parent to any existing ones.
	 */
	bool insert = true;
	auto it = _root_paths.begin ();
	while (it != _root_paths.end ()) {
		bool erase = false;
		if (it->size () > dir.size()) {
			if (is_subfolder (dir, *it)) {
				erase = true;
			}
		} else if (is_subfolder (*it, dir)) {
			insert = false;
			break;
		}
		if (erase) {
			auto it2 = it;
			++it;
			_root_paths.erase (it2);
		} else {
			++it;
		}
	}
	if (insert) {
		_root_paths.insert (dir);
	}
	return true;
}

/* ****************************************************************************
 * Treeview Callbacks
 */

void
TriggerClipPicker::drag_begin (Glib::RefPtr<Gdk::DragContext> const& context)
{
	TreeView::Selection::ListHandle_Path rows = _view.get_selection ()->get_selected_rows ();
	if (!rows.empty()) {
		Glib::RefPtr< Gdk::Pixmap > pix = _view.create_row_drag_icon (*rows.begin ());

		int w, h;
		pix->get_size (w, h);
		context->set_icon (pix->get_colormap (), pix, Glib::RefPtr<Gdk::Bitmap> (), 4, h / 2);
	}
}

void
TriggerClipPicker::drag_end (Glib::RefPtr<Gdk::DragContext> const&)
{
	_session->cancel_audition ();
}

void
TriggerClipPicker::cursor_changed ()
{
	if (!_session || !_autoplay_btn.get_active ()) {
		return;
	}

	_session->cancel_audition ();

	TreeModel::Path p;
	TreeViewColumn* col = NULL;
	_view.get_cursor (p, col);
	TreeModel::iterator i = _model->get_iter (p);
	/* This also plays the file if the cursor change deselects the row.
	 * However, checking if `i` is _view.get_selection () does not reliably work from this context.
	 */
	if (i && (*i)[_columns.file]) {
		audition ((*i)[_columns.path]);
	}
}

void
TriggerClipPicker::row_selected ()
{
	if (!_session) {
		return;
	}

	if (!_autoplay_btn.get_active ()) {
		_session->cancel_audition ();
	}

	if (_view.get_selection ()->count_selected_rows () < 1) {
		_play_btn.set_sensitive (false);
		_midi_prop_table.hide();
	} else {
		TreeView::Selection::ListHandle_Path rows = _view.get_selection ()->get_selected_rows ();
		TreeIter                             i    = _model->get_iter (*rows.begin ());

		_play_btn.set_sensitive ((*i)[_columns.file] && !_autoplay_btn.get_active ());

		std::string path = (*i)[_columns.path];
		if (SMFSource::valid_midi_file (path)) {
			/* TODO: if it's a really big file, we could skip this check */
			boost::shared_ptr<SMFSource> ms;
			try {
				ms = boost::dynamic_pointer_cast<SMFSource> (
					SourceFactory::createExternal (DataType::MIDI, *_session,
												   path, 0, Source::Flag (0), false));
			} catch (const std::exception& e) {
				error << string_compose(_("Could not read file: %1 (%2)."),
										path, e.what()) << endmsg;
			}

			if (ms) {
				if (ms->smf_format()==0) {
					format_text.set_text ("MIDI Type 0");
				} else {
					format_text.set_text (string_compose( _("%1 (%2 Tracks)"), ms->smf_format()==2 ? X_("MIDI Type 2") : X_("MIDI Type 1"), ms->num_tracks()));
				}
				channels_value.set_text (string_compose(
				    _("Channel(s) used: %1 - %2 "),
					ARDOUR_UI_UTILS::midi_channels_as_string (ms->used_channels()),
					ms->has_pgm_change() ? _("with pgms") : X_("")
					));

				_midi_prop_table.show();
			}
		} else {
			_midi_prop_table.hide();
		}
	}
}

void
TriggerClipPicker::row_activated (TreeModel::Path const& p, TreeViewColumn*)
{
	TreeModel::iterator i = _model->get_iter (p);
	if (i && (*i)[_columns.file]) {
		audition ((*i)[_columns.path]);
	} else if (i) {
		list_dir ((*i)[_columns.path]);
	}
}

bool
TriggerClipPicker::test_expand (TreeModel::iterator const& i, Gtk::TreeModel::Path const&)
{
	TreeModel::Row row = *i;
	if (row[_columns.read]) {
		/* already expanded */
		return false; /* OK */
	}
	row[_columns.read] = true;

	/* remove stub */
	_model->erase (row.children ().begin ());

	list_dir (row[_columns.path], &row.children ());

	return row.children ().size () == 0;
}

void
TriggerClipPicker::row_collapsed (TreeModel::iterator const& i, Gtk::TreeModel::Path const&)
{
	/* forget about expanded sub-view, refresh when expanded again */
	TreeModel::Row row = *i;
	row[_columns.read] = false;
	Gtk::TreeIter ti;
	while ((ti = row.children ().begin ()) != row.children ().end ()) {
		_model->erase (ti);
	}
	/* add stub child */
	row                = *(_model->append (row.children ()));
	row[_columns.read] = false;
}

void
TriggerClipPicker::drag_data_get (Glib::RefPtr<Gdk::DragContext> const&, SelectionData& data, guint, guint time)
{
	if (data.get_target () != "text/uri-list") {
		return;
	}
	std::vector<std::string>             uris;
	TreeView::Selection::ListHandle_Path rows = _view.get_selection ()->get_selected_rows ();
	for (TreeView::Selection::ListHandle_Path::iterator i = rows.begin (); i != rows.end (); ++i) {
		TreeIter iter;
		if ((iter = _model->get_iter (*i))) {
			if ((*iter)[_columns.file]) {
				uris.push_back (Glib::filename_to_uri ((*iter)[_columns.path]));
			}
		}
	}
	data.set_uris (uris);
}

bool
TriggerClipPicker::drag_motion (Glib::RefPtr<Gdk::DragContext> const& context, int, int y, guint time)
{
	for (auto i : context->get_targets ()) {
		if (i == "text/uri-list") {
			context->drag_status (Gdk::ACTION_LINK, time);
			return true;
		}
	}

	if (!_clip_library_dir.empty () && _current_path != _clip_library_dir) {
		list_dir (_clip_library_dir);
	}

	context->drag_status (Gdk::ACTION_COPY, time);
	return true;
}

void
TriggerClipPicker::drag_data_received (Glib::RefPtr<Gdk::DragContext> const& context, int /*x*/, int y, Gtk::SelectionData const& data, guint /*info*/, guint time)
{
	if (data.get_target () == "x-ardour/region.pbdid") {
		PBD::ID rid (data.get_data_as_string ());
		boost::shared_ptr<Region> region = RegionFactory::region_by_id (rid);
		if (export_to_clip_library (region, this)) {
			context->drag_finish (true, false, time);
		} else {
			context->drag_finish (true, false, time);
		}
	} else {
		bool                     changed = false;
		std::string              path;
		std::string              path_to_list;
		std::vector<std::string> paths;

		std::vector<std::string> a = PBD::parse_path (Config->get_sample_lib_path ());
		if (ARDOUR_UI_UTILS::convert_drop_to_paths (paths, data)) {
			for (std::vector<std::string>::const_iterator s = paths.begin (); s != paths.end (); ++s) {
				if (Glib::file_test (*s, Glib::FILE_TEST_IS_DIR)) {
					if (std::find (a.begin(), a.end(), *s) == a.end()) {
						a.push_back (*s);
						changed = true;
					}
					path_to_list = *s;
				}
			}
			if (changed) {
				size_t j = 0;
				for (std::vector<std::string>::const_iterator i = a.begin (); i != a.end (); ++i, ++j) {
					if (j > 0)
						path += G_SEARCHPATH_SEPARATOR;
					path += *i;
				}
				Config->set_sample_lib_path (path);
			}
			if (!path_to_list.empty ()) {
				list_dir (path_to_list);
			}
		}
	}
}

/* ****************************************************************************
 * Dir Listing
 */

static bool
audio_midi_suffix (const std::string& str)
{
	if (AudioFileSource::safe_audio_file_extension (str)) {
		return true;
	}
	if (SMFSource::safe_midi_file_extension (str)) {
		return true;
	}
	return false;
}

void
TriggerClipPicker::open_dir ()
{
	Gtk::Window* tlw = dynamic_cast<Gtk::Window*> (get_toplevel ());
	assert (tlw);
	_fcd.set_transient_for (*tlw);

	int result = _fcd.run ();
	_fcd.hide ();

	switch (result) {
		case RESPONSE_OK:
			list_dir (_fcd.get_filename ());
			break;
		case RESPONSE_ACCEPT:
			if (Glib::file_test (_fcd.get_filename (), Glib::FILE_TEST_IS_DIR)) {
				size_t                   j = 0;
				std::string              path;
				std::vector<std::string> a = PBD::parse_path (Config->get_sample_lib_path ());
				if (std::find (a.begin(), a.end(), _fcd.get_filename ()) != a.end()) {
					list_dir (_fcd.get_filename ());
					break;
				}
				a.push_back (_fcd.get_filename ());
				for (std::vector<std::string>::const_iterator i = a.begin (); i != a.end (); ++i, ++j) {
					if (j > 0)
						path += G_SEARCHPATH_SEPARATOR;
					path += *i;
				}
				Config->set_sample_lib_path (path);
				list_dir (_fcd.get_filename ());
			}
			break;
		default:
			break;
	}
}

void
TriggerClipPicker::list_dir (std::string const& path, Gtk::TreeNodeChildren const* pc)
{
	if (_ignore_list_dir) {
		return;
	}
	/* do not recurse when calling _clip_dir_menu.set_active() */
	PBD::Unwinder<bool> uw (_ignore_list_dir, true);

	if (!Glib::file_test (path, Glib::FILE_TEST_IS_DIR)) {
		assert (0);
		return;
	}

	if (!pc) {
		_view.set_model (Glib::RefPtr<Gtk::TreeStore>(0));
		_model->clear ();
		_clip_dir_menu.set_active (display_name (path));
	}

	_current_path = path;

	if (_clip_library_dir == path) {
		_open_library_btn.show ();
	} else {
		_open_library_btn.hide ();
	}

	std::vector<std::string> dirs;
	std::vector<std::string> files;

	try {
		Glib::Dir dir (path);
		for (Glib::DirIterator i = dir.begin (); i != dir.end (); ++i) {
			std::string fullpath = Glib::build_filename (path, *i);
			std::string basename = *i;

			if (basename.size () == 0 || basename[0] == '.') {
				continue;
			}

			if (Glib::file_test (fullpath, Glib::FILE_TEST_IS_DIR)) {
				dirs.push_back (*i);
				continue;
			}

			if (audio_midi_suffix (fullpath)) {
				files.push_back (*i);
			}
		}
	} catch (Glib::FileError const& err) {
	}

	std::sort (dirs.begin (), dirs.end ());
	std::sort (files.begin (), files.end ());

	if (!pc) {
		if (_root_paths.find (_current_path) == _root_paths.end ()) {
			TreeModel::Row row = *(_model->append ());
			row[_columns.name] = "..";
			row[_columns.path] = Glib::path_get_dirname (_current_path);
			row[_columns.read] = false;
			row[_columns.file] = false;
		}
	}

	for (auto& f : dirs) {
		TreeModel::Row row;
		if (pc) {
			row = *(_model->append (*pc));
		} else {
			row = *(_model->append ());
		}
		row[_columns.name] = f;
		row[_columns.path] = Glib::build_filename (path, f);
		row[_columns.read] = false;
		row[_columns.file] = false;
		/* add stub child */
		row                = *(_model->append (row.children ()));
		row[_columns.read] = false;
	}

	for (auto& f : files) {
		TreeModel::Row row;
		if (pc) {
			row = *(_model->append (*pc));
		} else {
			row = *(_model->append ());
		}
		row[_columns.name] = f;
		row[_columns.path] = Glib::build_filename (path, f);
		row[_columns.read] = false;
		row[_columns.file] = true;
	}

	if (!pc) {
		_view.set_model (_model);
	}
}

void
TriggerClipPicker::open_library ()
{
	PBD::open_folder (_clip_library_dir);
}

/* ****************************************************************************
 * Auditioner
 */

void
TriggerClipPicker::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	_play_btn.set_sensitive (false);
	_stop_btn.set_sensitive (false);
	_midi_prop_table.hide ();  //only shown when a valid smf is chosen

	if (!_session) {
		_seek_slider.set_sensitive (false);
		_auditioner_connections.drop_connections ();
		_processor_connections.drop_connections ();
		audition_processor_going_away ();
	} else {
		_auditioner_connections.drop_connections ();
		_session->AuditionActive.connect (_auditioner_connections, invalidator (*this), boost::bind (&TriggerClipPicker::audition_active, this, _1), gui_context ());
		_session->the_auditioner ()->AuditionProgress.connect (_auditioner_connections, invalidator (*this), boost::bind (&TriggerClipPicker::audition_progress, this, _1, _2), gui_context ());
		_session->the_auditioner ()->processors_changed.connect (_auditioner_connections, invalidator (*this), boost::bind (&TriggerClipPicker::audition_processors_changed, this), gui_context ());
		audition_processors_changed (); /* set sensitivity */
	}
}

void
TriggerClipPicker::autoplay_toggled ()
{
	UIConfiguration::instance ().set_autoplay_clips (_autoplay_btn.get_active ());
	row_selected (); /* maybe cancel audition, update sensitivity */
}

void
TriggerClipPicker::stop_audition ()
{
	if (_session) {
		_session->cancel_audition ();
	}
}

void
TriggerClipPicker::audition_active (bool active)
{
	_play_btn.set_sensitive (!active && !_autoplay_btn.get_active ());
	_stop_btn.set_sensitive (active);
	_seek_slider.set_sensitive (active);

	if (!active) {
		_seek_slider.set_value (0);
		_seeking = false;
	}
}

void
TriggerClipPicker::audition_progress (ARDOUR::samplecnt_t pos, ARDOUR::samplecnt_t len)
{
	if (!_seeking) {
		_seek_slider.set_value (1000.0 * pos / len);
		_seek_slider.set_sensitive (true);
	}
}

bool
TriggerClipPicker::seek_button_press (GdkEventButton*)
{
	_seeking = true;
	return false;
}

bool
TriggerClipPicker::seek_button_release (GdkEventButton*)
{
	_seeking = false;
	_session->the_auditioner ()->seek_to_percent (_seek_slider.get_value () / 10.0);
	_seek_slider.set_sensitive (false);
	return false;
}

void
TriggerClipPicker::audition_selected ()
{
	if (_view.get_selection ()->count_selected_rows () < 1) {
		return;
	}
	TreeView::Selection::ListHandle_Path rows = _view.get_selection ()->get_selected_rows ();
	TreeIter                             i    = _model->get_iter (*rows.begin ());
	audition ((*i)[_columns.path]);
}

void
TriggerClipPicker::audition (std::string const& path)
{
	if (!_session) {
		return;
	}
	_session->cancel_audition ();

	if (!Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
		warning << string_compose (_("Could not read file: %1 (%2)."), path, strerror (errno)) << endmsg;
		return;
	}

	boost::shared_ptr<Region> r;

	if (SMFSource::valid_midi_file (path)) {
		boost::shared_ptr<SMFSource> ms = boost::dynamic_pointer_cast<SMFSource> (SourceFactory::createExternal (DataType::MIDI, *_session, path, 0, Source::Flag (0), false));

		std::string rname = region_name_from_path (ms->path (), false);

		PropertyList plist;
		plist.add (ARDOUR::Properties::start, timepos_t (Temporal::Beats ()));
		plist.add (ARDOUR::Properties::length, ms->length ());
		plist.add (ARDOUR::Properties::name, rname);
		plist.add (ARDOUR::Properties::layer, 0);

		r = boost::dynamic_pointer_cast<MidiRegion> (RegionFactory::create (boost::dynamic_pointer_cast<Source> (ms), plist, false));
		assert (r);

	} else {
		SourceList                         srclist;
		boost::shared_ptr<AudioFileSource> afs;
		bool                               old_sbp = AudioSource::get_build_peakfiles ();

		/* don't even think of building peakfiles for these files */

		SoundFileInfo info;
		std::string   error_msg;
		if (!AudioFileSource::get_soundfile_info (path, info, error_msg)) {
			error << string_compose (_("Cannot get info from audio file %1 (%2)"), path, error_msg) << endmsg;
			return;
		}

		AudioSource::set_build_peakfiles (false);

		for (uint16_t n = 0; n < info.channels; ++n) {
			try {
				afs = boost::dynamic_pointer_cast<AudioFileSource> (SourceFactory::createExternal (DataType::AUDIO, *_session, path, n, Source::Flag (ARDOUR::AudioFileSource::NoPeakFile), false));
				if (afs->sample_rate () != _session->nominal_sample_rate ()) {
					boost::shared_ptr<SrcFileSource> sfs (new SrcFileSource (*_session, afs, ARDOUR::SrcGood));
					srclist.push_back (sfs);
				} else {
					srclist.push_back (afs);
				}
			} catch (failed_constructor& err) {
				error << _("Could not access soundfile: ") << path << endmsg;
				AudioSource::set_build_peakfiles (old_sbp);
				return;
			}
		}

		AudioSource::set_build_peakfiles (old_sbp);

		if (srclist.empty ()) {
			return;
		}

		afs               = boost::dynamic_pointer_cast<AudioFileSource> (srclist[0]);
		std::string rname = region_name_from_path (afs->path (), false);

		PropertyList plist;
		plist.add (ARDOUR::Properties::start, timepos_t (0));
		plist.add (ARDOUR::Properties::length, srclist[0]->length ());
		plist.add (ARDOUR::Properties::name, rname);
		plist.add (ARDOUR::Properties::layer, 0);

		r = boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (srclist, plist, false));
	}

	r->set_position (timepos_t ());

	_session->audition_region (r);
}

void
TriggerClipPicker::audition_processor_idle ()
{
	if (!_session || _session->deletion_in_progress () || !_session->the_auditioner ()) {
		return;
	}
	assert (_session && _session->the_auditioner ());
	ARDOUR_UI::instance ()->get_process_buffers ();
	_session->the_auditioner ()->idle_synth_update ();
	ARDOUR_UI::instance ()->drop_process_buffers ();
}

bool
TriggerClipPicker::audition_processor_viz (bool show)
{
	if (show) {
		_idle_connection = Timers::fps_connect (sigc::mem_fun (*this, &TriggerClipPicker::audition_processor_idle));
	} else {
		_idle_connection.disconnect ();
	}
	return false;
}

void
TriggerClipPicker::audition_show_plugin_ui ()
{
	if (!_audition_plugnui) {
		boost::shared_ptr<PluginInsert> plugin_insert = boost::dynamic_pointer_cast<PluginInsert> (_session->the_auditioner ()->the_instrument ());
		if (plugin_insert) {
			_audition_plugnui = new PluginUIWindow (plugin_insert);
			_audition_plugnui->set_session (_session);
			_audition_plugnui->show_all ();
			_audition_plugnui->set_title (/* generate_processor_title (plugin_insert)*/ _("Audition Synth"));
			plugin_insert->DropReferences.connect (_processor_connections, invalidator (*this), boost::bind (&TriggerClipPicker::audition_processor_going_away, this), gui_context());

			_audition_plugnui->signal_map_event ().connect (sigc::hide (sigc::bind (sigc::mem_fun (*this, &TriggerClipPicker::audition_processor_viz), true)));
			_audition_plugnui->signal_unmap_event ().connect (sigc::hide (sigc::bind (sigc::mem_fun (*this, &TriggerClipPicker::audition_processor_viz), false)));
		}
	}
	if (_audition_plugnui) {
		_audition_plugnui->present ();
	}
}

void
TriggerClipPicker::audition_processor_going_away ()
{
	if (_audition_plugnui) {
		_idle_connection.disconnect ();
		delete _audition_plugnui;
	}
	_audition_plugnui = 0;
}

void
TriggerClipPicker::audition_processors_changed ()
{
	if (!_session || _session->deletion_in_progress () || !_session->the_auditioner ()) {
		_show_plugin_btn.set_sensitive (false);
		set_tooltip (_show_plugin_btn, "You must first play one midi file to show the plugin's GUI");
		return;
	}

	if (_session && _session->the_auditioner ()->get_audition_synth_info()) {
		boost::shared_ptr<PluginInsert> plugin_insert = boost::dynamic_pointer_cast<PluginInsert> (_session->the_auditioner ()->the_instrument ());
		if (plugin_insert) {
			set_tooltip (_show_plugin_btn, "Show the selected audition-instrument's GUI");
			_show_plugin_btn.set_sensitive (true);
		}
	}
}
