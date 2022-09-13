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

#include <glib/gstdio.h>

#include "pbd/downloader.h"
#include "pbd/inflater.h"
#include "pbd/i18n.h"

#include <glibmm/markup.h>

#include "ardour/rc_configuration.h"
#include "ardour/library.h"

#include "gui_thread.h"
#include "library_download_dialog.h"

using namespace PBD;
using namespace ARDOUR;
using std::string;

LibraryDownloadDialog::LibraryDownloadDialog ()
	: ArdourDialog (_("Loop Library Manager"), true) /* modal */
	, inflater(0)
{
	_model = Gtk::ListStore::create (_columns);
	_display.set_model (_model);

	_display.append_column (_("Name"), _columns.name);
	_display.append_column (_("Author"), _columns.author);
	_display.append_column (_("License"), _columns.license);
	_display.append_column (_("Size"), _columns.size);
	_display.append_column (_("Installed"), _columns.installed);

	append_install_column ();
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

LibraryDownloadDialog::~LibraryDownloadDialog ()
{
	delete inflater;
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
LibraryDownloadDialog::append_install_column ()
{
	install_renderer = new Gtk::CellRendererText();
	Gtk::TreeViewColumn* tvc = manage (new Gtk::TreeViewColumn ("", *install_renderer));
	tvc->set_data (X_("index"), (void*) (intptr_t (_columns.install.index())));
	tvc->add_attribute (*install_renderer, "text", _columns.install);
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
	(*i)[_columns.toplevel] = ld.toplevel_dir();

	if (ld.installed()) {
		(*i)[_columns.install] = string();
	} else {
		(*i)[_columns.install] = string (_("Install"));
	}

	/* tooltip must be escape for pango markup
	 */

	(*i)[_columns.description] = Glib::Markup::escape_text (ld.description());
}

void
LibraryDownloadDialog::install (std::string const & path, Gtk::TreePath const & treepath)
{
	std::string destdir = Glib::path_get_dirname (path);

	inflater = new Inflater (path,  destdir);
	inflater->progress.connect (install_connection, invalidator(*this), boost::bind (&LibraryDownloadDialog::install_progress, this, _1, _2, path, treepath), gui_context());
	inflater->start (); /* starts unpacking in a thread */
}

void
LibraryDownloadDialog::install_progress (size_t nread, size_t total, std::string path, Gtk::TreePath treepath)
{
	Gtk::TreeModel::iterator row = _model->get_iter (treepath);

	if (!inflater) {
		return;
	}

	if (inflater->status() >= 0) {
		LibraryDownloadDialog::install_finished (row, path, inflater->status());
		return;
	}

	(*row)[_columns.progress] = (int) round ((double) nread / total);
}

void
LibraryDownloadDialog::install_finished (Gtk::TreeModel::iterator row, std::string path, int status)
{
	if (status == 0) {

		std::string toplevel = (*row)[_columns.toplevel];
		toplevel = Glib::build_filename (Glib::path_get_dirname (path), toplevel);

		LibraryFetcher lf;

		lf.add (toplevel);

		(*row)[_columns.installed] = true;
		(*row)[_columns.install] = string();
		(*row)[_columns.progress] = 100;
	} else {
		(*row)[_columns.installed] = false;
		(*row)[_columns.install] = _("Install");
		(*row)[_columns.progress] = 0;
	}

	/* Always unlink (remove) the downloaded archive */

	::g_unlink (path.c_str());

	/* reap thread */

	install_connection.disconnect ();
	delete inflater;
	inflater = 0;
}


void
LibraryDownloadDialog::download (Gtk::TreePath const & path)
{
	Gtk::TreeModel::iterator row = _model->get_iter (path);
	std::string url = (*row)[_columns.url];

	PBD::Downloader* downloader = new PBD::Downloader (url, ARDOUR::Config->get_clip_library_dir());

	/* setup timer callback to update progressbar */

	Glib::signal_timeout().connect (sigc::bind (sigc::mem_fun (*this, &LibraryDownloadDialog::dl_timer_callback), downloader, path), 40);

	(*row)[_columns.downloader] = downloader;

	/* and go ... */

	downloader->start ();

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
		(*row)[_columns.progress] = (int) round (dl->progress() * 100.0);
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

		int col = (intptr_t) column->get_data (X_("index"));

		if (col != _columns.install.index()) {
			std::cerr << "not install\n";
			return false;
		}

		Gtk::TreeIter iter = _model->get_iter (path);

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
