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

#include <cmath>
#include <iostream>

#include "pbd/i18n.h"

#include <glibmm/markup.h>

#include "ardour/rc_configuration.h"
#include "ardour/library.h"

#include "library_download_dialog.h"

using namespace ARDOUR;
using std::string;

LibraryDownloadDialog::LibraryDownloadDialog ()
	: ArdourDialog (_("Loop Library Manager"), true) /* modal */
{
	_model = Gtk::ListStore::create (_columns);
	_display.set_model (_model);

	_display.append_column (_("Name"), _columns.name);
	_display.append_column (_("Author"), _columns.author);
	_display.append_column (_("License"), _columns.license);
	_display.append_column (_("Size"), _columns.size);
	_display.append_column (_("Installed"), _columns.installed);
	_display.append_column_editable ("", _columns.install);
	append_progress_column ();

	_display.set_headers_visible (true);
	_display.set_tooltip_column (5); /* path */

	_display.signal_button_press_event().connect (sigc::mem_fun (*this, &LibraryDownloadDialog::display_button_press), false);

	Gtk::HBox* h = new Gtk::HBox;
	h->set_spacing (8);
	h->set_border_width (8);
	h->pack_start (_display);

	get_vbox()->set_spacing (8);
	get_vbox()->pack_start (*Gtk::manage (h));


	ARDOUR::LibraryFetcher lf;
	lf.get_descriptions ();
	lf.foreach_description (boost::bind (&LibraryDownloadDialog::add_library, this, _1));
}

void
LibraryDownloadDialog::append_progress_column ()
{
	progress_renderer = new Gtk::CellRendererProgress();
	progress_renderer->property_width() = 100;
	Gtk::TreeViewColumn* tvc = manage (new Gtk::TreeViewColumn ("", *progress_renderer));
	tvc->add_attribute (*progress_renderer, "value", _columns.progress);
	_display.append_column (*tvc);
}


void
LibraryDownloadDialog::add_library (ARDOUR::LibraryDescription const & ld)
{

	Gtk::TreeModel::iterator i = _model->append();
	(*i)[_columns.name] = ld.name();
	(*i)[_columns.author] = ld.author();
	(*i)[_columns.license] = ld.license();
	(*i)[_columns.size] = ld.size();
	(*i)[_columns.installed] = ld.installed();
	(*i)[_columns.url] = ld.url();

	if (ld.installed()) {
		(*i)[_columns.install] = string();
	} else {
		(*i)[_columns.install] = string (_("Install"));
	}

	/* tooltip must be escape for pango markup, and we should strip all
	 * duplicate spaces
	 */

	(*i)[_columns.description] = Glib::Markup::escape_text (ld.description());
}

void
LibraryDownloadDialog::install (std::string const & path, Gtk::TreePath const & treepath)
{
	Gtk::TreeModel::iterator row = _model->get_iter (treepath);
	LibraryFetcher lf;

	if (lf.add (path) == 0) {
		(*row)[_columns.installed] = true;
		(*row)[_columns.install] = string();;
	} else {
		(*row)[_columns.installed] = false;
		(*row)[_columns.install] = _("Install");
	}
}


void
LibraryDownloadDialog::download (Gtk::TreePath const & path)
{
	Gtk::TreeModel::iterator row = _model->get_iter (path);
	std::string url = (*row)[_columns.url];

	std::cerr << "will download " << url << " to " << Config->get_clip_library_dir() << std::endl;

	ARDOUR::Downloader* downloader = new ARDOUR::Downloader (url, ARDOUR::Config->get_clip_library_dir());

	/* setup timer callback to update progressbar */

	Glib::signal_timeout().connect (sigc::bind (sigc::mem_fun (*this, &LibraryDownloadDialog::dl_timer_callback), downloader, path), 40);

	/* and go ... */

	downloader->start ();

	(*row)[_columns.downloader] = downloader;

	/* and back to the GUI event loop, though we're modal so not much is possible */
}

bool
LibraryDownloadDialog::dl_timer_callback (Downloader* dl, Gtk::TreePath treepath)
{
	Gtk::TreeModel::iterator row = _model->get_iter (treepath);

	/* zero status indicates still running; positive status indicates
	 * success; negative value indicates failure
	 */

	if (dl->status() == 0) {
		(*row)[_columns.progress] = (int) round ((dl->progress() * 100.0));
		return true; /* call again */
	}

	(*row)[_columns.progress] = 0.;
	(*row)[_columns.downloader] = 0;

	if (dl->status() < 0) {
		(*row)[_columns.install] = _("Install");;
	} else {
		(*row)[_columns.install] = _("Installing");
		install (dl->download_path(), treepath);
	}

	delete dl;

	return false; /* no more calls, done or cancelled */
}

bool
LibraryDownloadDialog::display_button_press (GdkEventButton* ev)
{
	if ((ev->type == GDK_BUTTON_PRESS) && (ev->button == 1)) {

		Gtk::TreeModel::Path path;
		Gtk::TreeViewColumn* column;
		int cellx, celly;

		if (!_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
			return false;
		}

		Gtk::TreeIter iter = _model->get_iter (path);

		std::cerr << "Click\n";

		string cur = (*iter)[_columns.install];
		if (cur == _("Install")) {
			if (!(*iter)[_columns.installed]) {
				(*iter)[_columns.install] = _("Cancel");
				download (path);
			}
		} else {
			Downloader* dl = (*iter)[_columns.downloader];

			if (dl) {
				dl->cancel ();
			}

			(*iter)[_columns.install] = _("Install");
		}

		return true;
	}

	return false;
}
