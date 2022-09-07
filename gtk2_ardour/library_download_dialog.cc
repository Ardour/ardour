#include "pbd/i18n.h"

#include <glibmm/markup.h>

#include "ardour/library.h"

#include "library_download_dialog.h"

LibraryDownloadDialog::LibraryDownloadDialog ()
	: ArdourDialog (_("Loop Library Manager"), true) /* modal */
{
	_model = Gtk::ListStore::create (_columns);
	_display.set_model (_model);

	_display.append_column (_("Name"), _columns.name);
	_display.append_column (_("Author"), _columns.author);
	_display.append_column (_("License"), _columns.license);
	_display.append_column (_("Size"), _columns.size);
	_display.append_column_editable (_("Installed"), _columns.installed);

	Gtk::CellRendererToggle *toggle = dynamic_cast<Gtk::CellRendererToggle *> (_display.get_column_cell_renderer (4));
	toggle->set_alignment (0.0, 0.5);
	toggle->signal_toggled().connect (sigc::mem_fun (*this, &LibraryDownloadDialog::install_activated));

	_display.set_headers_visible (true);
	_display.set_tooltip_column (5); /* path */

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
LibraryDownloadDialog::add_library (ARDOUR::LibraryDescription const & ld)
{

	Gtk::TreeModel::iterator i = _model->append();
	(*i)[_columns.name] = ld.name();
	(*i)[_columns.author] = ld.author();
	(*i)[_columns.license] = ld.license();
	(*i)[_columns.size] = ld.size();
	(*i)[_columns.installed] = ld.installed();

	/* tooltip must be escape for pango markup, and we should strip all
	 * duplicate spaces
	 */

	(*i)[_columns.description] = Glib::Markup::escape_text (ld.description());
	std::cerr << "set descr to " << ld.description() << std::endl;
}


void
LibraryDownloadDialog::install_activated (std::string str)
{
}
