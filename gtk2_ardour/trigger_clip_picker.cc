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
#include "pbd/search_path.h"

#include "ardour/audiofilesource.h"
#include "ardour/directory_names.h"
#include "ardour/filesystem_paths.h"
#include "ardour/smf_source.h"

#include "gtkmm2ext/menu_elems.h"
#include "gtkmm2ext/utils.h"

#include "trigger_clip_picker.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;

TriggerClipPicker::TriggerClipPicker ()
	: _fcd (_("Select Sample Folder"), FILE_CHOOSER_ACTION_SELECT_FOLDER)
{
	_scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);
	_scroller.add (_view);

	Gtkmm2ext::add_volume_shortcuts (_fcd);
	_fcd.add_button (Stock::CANCEL, RESPONSE_CANCEL);
	_fcd.add_button (Stock::OK, RESPONSE_ACCEPT);

	PBD::Searchpath spath (ardour_data_search_path ());
	spath.add_subdirectory_to_paths(media_dir_name);
	for (auto const& f : spath) {
		maybe_add_dir (f);
	}

	maybe_add_dir (Glib::build_filename (user_config_directory(), media_dir_name));

	for (auto const& f : _fcd.list_shortcut_folders ()) {
		maybe_add_dir (f);
	}
	assert (_dir.items ().size () > 0);

	_dir.AddMenuElem (Menu_Helpers::SeparatorElem ());
	_dir.AddMenuElem (Menu_Helpers::MenuElem (_("Other..."), sigc::mem_fun (*this, &TriggerClipPicker::open_dir)));

	pack_start (_dir, false, false);
	pack_start (_scroller);

	_model = TreeStore::create (_columns);
	_view.set_model (_model);
	_view.append_column (_("File Name"), _columns.name);
	_view.set_headers_visible (true);
	_view.set_reorderable (false);
	_view.get_selection ()->set_mode (SELECTION_MULTIPLE);

	/* DnD */
	std::vector<TargetEntry> dnd;
	dnd.push_back (TargetEntry ("text/uri-list"));
	_view.enable_model_drag_source (dnd, Gdk::MODIFIER_MASK, Gdk::ACTION_COPY);

	_view.get_selection ()->signal_changed ().connect (sigc::mem_fun (*this, &TriggerClipPicker::row_selected));
	_view.signal_row_activated ().connect (sigc::mem_fun (*this, &TriggerClipPicker::row_activated));
	_view.signal_drag_data_get ().connect (sigc::mem_fun (*this, &TriggerClipPicker::drag_data_get));

	_scroller.show ();
	_view.show ();
	_dir.show ();

	_dir.items ().front ().activate ();
}

TriggerClipPicker::~TriggerClipPicker ()
{
}

void
TriggerClipPicker::maybe_add_dir (std::string const& dir)
{
	if (Glib::file_test (dir, Glib::FILE_TEST_IS_DIR | Glib::FILE_TEST_EXISTS)) {
		_dir.AddMenuElem (Gtkmm2ext::MenuElemNoMnemonic (Glib::path_get_basename (dir), sigc::bind (sigc::mem_fun (*this, &TriggerClipPicker::list_dir), dir)));
	}
}

void
TriggerClipPicker::row_selected ()
{
	if (_view.get_selection ()->count_selected_rows () < 1) {
		return;
	}

	TreeView::Selection::ListHandle_Path rows = _view.get_selection ()->get_selected_rows ();
	for (TreeView::Selection::ListHandle_Path::iterator i = rows.begin (); i != rows.end (); ++i) {
		TreeIter iter;
		if ((iter = _model->get_iter (*i))) {
		}
	}
}

void
TriggerClipPicker::row_activated (TreeModel::Path const&, TreeViewColumn*)
{
	// TODO audition
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
			uris.push_back (Glib::filename_to_uri ((*iter)[_columns.path]));
		}
	}
	data.set_uris (uris);
}

static bool
audio_midi_suffix (const std::string& str, void* /*arg*/)
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
	if (tlw) {
	_fcd.set_transient_for (*tlw);
	}
	int result = _fcd.run();
	_fcd.hide ();
	switch (result) {
		case RESPONSE_ACCEPT:
			list_dir (_fcd.get_filename ());
			break;
		default:
			break;
	}
}

void
TriggerClipPicker::list_dir (std::string const& dir)
{
	std::vector<std::string> fl;
	PBD::find_files_matching_filter (fl, dir, audio_midi_suffix, 0, false, true, false);
	_dir.set_active (Glib::path_get_basename (dir));

	_model->clear ();
	for (auto& f : fl) {
		TreeModel::Row row = *(_model->append ());
		row[_columns.name] = Glib::path_get_basename (f);
		row[_columns.path] = f;
	}
}
