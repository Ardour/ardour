/*
 * Copyright (C) 2022 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __gtk2_ardour_library_download_dialog_h__
#define __gtk2_ardour_library_download_dialog_h__

#include <string>

#include <gtkmm/cellrendererprogress.h>
#include <gtkmm/cellrenderertext.h>
#include <gtkmm/entry.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treeview.h>

#include "ardour_dialog.h"

namespace PBD {
	class Inflater;
	class Downloader;
}

namespace ARDOUR {
	class LibraryDescription;
	class LibraryFetcher;
}

class LibraryDownloadDialog : public ArdourDialog
{
  public:
	LibraryDownloadDialog ();
	~LibraryDownloadDialog ();

	void add_library (ARDOUR::LibraryDescription const &);

  private:
	class LibraryColumns : public Gtk::TreeModelColumnRecord {
	  public:
		LibraryColumns() {
			add (name);
			add (author);
			add (license);
			add (size);
			add (installed);
			add (description);
			add (url);
			add (toplevel);
			add (install);
			add (progress);
			add (downloader);
		}

		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> author;
		Gtk::TreeModelColumn<std::string> license;
		Gtk::TreeModelColumn<std::string> size;
		Gtk::TreeModelColumn<std::string> install;
		Gtk::TreeModelColumn<bool> installed;
		/* these are not displayed */
		Gtk::TreeModelColumn<std::string> url;
		Gtk::TreeModelColumn<std::string> toplevel;
		Gtk::TreeModelColumn<PBD::Downloader*> downloader;
		Gtk::TreeModelColumn<int> progress;
		/* used as tooltip */
		Gtk::TreeModelColumn<std::string> description;
	};

	Gtk::TreeView _display;
	Glib::RefPtr<Gtk::ListStore> _model;
	LibraryColumns _columns;

	template <class T>
	Gtk::TreeViewColumn* append_col (Gtk::TreeModelColumn<T> const& col, int width = 0)
	{
		Gtk::TreeViewColumn* c = manage (new Gtk::TreeViewColumn ("", col));
		if (width) {
			c->set_fixed_width (width);
			c->set_sizing (Gtk::TREE_VIEW_COLUMN_FIXED);
		}
		_display.append_column (*c);
		return c;
	}

	Gtk::CellRendererProgress* progress_renderer;
	Gtk::CellRendererText* install_renderer;

	void append_progress_column ();
	void append_install_column ();

	void download (Gtk::TreePath const &);

	bool dl_timer_callback (PBD::Downloader*, Gtk::TreePath);
	bool display_button_press (GdkEventButton* ev);

	PBD::Inflater* inflater;
	void install (std::string const & path, Gtk::TreePath const & treepath);
	void install_progress (size_t, size_t, std::string, Gtk::TreePath);
	void install_finished (Gtk::TreeModel::iterator row, std::string path, int status);
	PBD::ScopedConnection install_connection;
};



#endif /* __gtk2_ardour_library_download_dialog_h__ */
