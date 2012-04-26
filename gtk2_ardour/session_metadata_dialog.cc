/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "session_metadata_dialog.h"

#include <sstream>

#include <gtkmm2ext/utils.h>

#include "pbd/xml++.h"
#include "ardour/session.h"
#include "ardour/session_directory.h"
#include "ardour/session_utils.h"
#include "ardour/configuration.h"

#include "i18n.h"

using namespace std;
using namespace Glib;

#define CALL_MEMBER_FN(object,ptrToMember)  ((object).*(ptrToMember))

/*** MetadataField ***/

MetadataField::MetadataField (string const & field_name) :
  _name (field_name)
{
}

MetadataField::~MetadataField() { }

/* TextMetadataField */

TextMetadataField::TextMetadataField (Getter getter, Setter setter, string const & field_name, guint width ) :
  MetadataField (field_name),
  getter (getter),
  setter (setter),
  width (width)
{
	entry = 0;
	label = 0;
	value_label = 0;
}

MetadataPtr
TextMetadataField::copy ()
{
	return MetadataPtr (new TextMetadataField (getter, setter, _name, width));
}

void
TextMetadataField::save_data (ARDOUR::SessionMetadata & data) const
{
	CALL_MEMBER_FN (data, setter) (_value);
}

void
TextMetadataField::load_data (ARDOUR::SessionMetadata const & data)
{
	_value = CALL_MEMBER_FN (data, getter) ();
	if (entry) {
		entry->set_text (_value);
	}
}

Gtk::Widget &
TextMetadataField::name_widget ()
{
	label = Gtk::manage (new Gtk::Label(_name + ':'));
	label->set_alignment (1, 0.5);
	return *label;
}

Gtk::Widget &
TextMetadataField::value_widget ()
{
	value_label = Gtk::manage (new Gtk::Label(_value));
	return *value_label;
}

Gtk::Widget &
TextMetadataField::edit_widget ()
{
	entry = Gtk::manage (new Gtk::Entry());

	entry->set_text (_value);
	entry->set_width_chars (width);
	entry->signal_changed().connect (sigc::mem_fun(*this, &TextMetadataField::update_value));

	return *entry;
}

void
TextMetadataField::update_value ()
{
	_value = entry->get_text ();
}

/* NumberMetadataField */

NumberMetadataField::NumberMetadataField (Getter getter, Setter setter, string const & field_name, guint numbers, guint width) :
  MetadataField (field_name),
  getter (getter),
  setter (setter),
  numbers (numbers),
  width (width)
{
	entry = 0;
	label = 0;
	value_label = 0;
}

MetadataPtr
NumberMetadataField::copy ()
{
	return MetadataPtr (new NumberMetadataField (getter, setter, _name, numbers, width));
}

void
NumberMetadataField::save_data (ARDOUR::SessionMetadata & data) const
{
	uint32_t number = str_to_uint (_value);
	CALL_MEMBER_FN (data, setter) (number);
}

void
NumberMetadataField::load_data (ARDOUR::SessionMetadata const & data)
{
	uint32_t number = CALL_MEMBER_FN (data, getter) ();
	_value = uint_to_str (number);
	if (entry) {
		entry->set_text (_value);
	}
}

void
NumberMetadataField::update_value ()
{
	// Accpt only numbers
	uint32_t number = str_to_uint (entry->get_text());
	_value = uint_to_str (number);
	entry->set_text (_value);
}

Gtk::Widget &
NumberMetadataField::name_widget ()
{
	label = Gtk::manage (new Gtk::Label(_name + ':'));
	label->set_alignment (1, 0.5);
	return *label;
}

Gtk::Widget &
NumberMetadataField::value_widget ()
{
	value_label = Gtk::manage (new Gtk::Label(_value));
	return *value_label;
}

Gtk::Widget &
NumberMetadataField::edit_widget ()
{
	entry = Gtk::manage (new Gtk::Entry());

	entry->set_text (_value);
	entry->set_width_chars (width);
	entry->set_max_length (numbers);
	entry->signal_changed().connect (sigc::mem_fun(*this, &NumberMetadataField::update_value));

	return *entry;
}

string
NumberMetadataField::uint_to_str (uint32_t i) const
{
	std::ostringstream oss ("");
	oss << i;
	if (oss.str().compare("0")) {
		return oss.str();
	} else {
		return "";
	}
}

uint32_t
NumberMetadataField::str_to_uint (string const & str) const
{
	string tmp (str);
	string::size_type i;
	while ((i = tmp.find_first_not_of("1234567890")) != string::npos) {
		tmp.erase (i, 1);
	}

	std::istringstream iss(tmp);
	uint32_t result = 0;
	iss >> result;
	return result;
}


/* SessionMetadataSet */

SessionMetadataSet::SessionMetadataSet (string const & name)
  : name (name)
{
}

void
SessionMetadataSet::add_data_field (MetadataPtr field)
{
	list.push_back (field);
}

/* SessionMetadataSetEditable */

SessionMetadataSetEditable::SessionMetadataSetEditable (string const & name)
  : SessionMetadataSet (name)
{
	table.set_row_spacings (6);
	table.set_col_spacings (12);
	table.set_homogeneous (false);
	vbox.pack_start (table, false, false);
	vbox.set_spacing (6);
	vbox.set_border_width (6);
}

Gtk::Widget &
SessionMetadataSetEditable::get_tab_widget ()
{
	tab_widget.set_text (name);
	return tab_widget;
}

void
SessionMetadataSetEditable::set_session (ARDOUR::Session * s)
{
	SessionHandlePtr::set_session (s);

	if (!_session) {
		return;
	}

	ARDOUR::SessionMetadata const & data = *(ARDOUR::SessionMetadata::Metadata());

	table.resize (list.size(), 2);
	uint32_t row = 0;
	MetadataPtr field;
	for (DataList::const_iterator it = list.begin(); it != list.end(); ++it) {
		field = *it;
		field->load_data (data);
		table.attach (field->name_widget(), 0, 1, row, row + 1, Gtk::FILL);
		table.attach (field->edit_widget(), 1, 2, row, row + 1);
		++row;
	}
}

void
SessionMetadataSetEditable::save_data ()
{
	ARDOUR::SessionMetadata & data = *(ARDOUR::SessionMetadata::Metadata());
	for (DataList::const_iterator it = list.begin(); it != list.end(); ++it) {
		(*it)->save_data(data);
	}
}

/* SessionMetadataSetImportable */

SessionMetadataSetImportable::SessionMetadataSetImportable (string const & name)
  : SessionMetadataSet (name)
  , session_list (list)
{
	tree = Gtk::ListStore::create (tree_cols);
	tree_view.set_model (tree);

	Gtk::TreeView::Column * viewcol;

	// Add import column
	Gtk::CellRendererToggle * import_render = Gtk::manage(new Gtk::CellRendererToggle());
	import_render->signal_toggled().connect (sigc::mem_fun(*this, &SessionMetadataSetImportable::selection_changed));
	viewcol = Gtk::manage(new Gtk::TreeView::Column (_("Import"), *import_render));
	viewcol->add_attribute (import_render->property_active(), tree_cols.import);
	tree_view.append_column (*viewcol);

	// Add field name column
	tree_view.append_column(_("Field"), tree_cols.field);

	// Add values column with pango markup
	Gtk::CellRendererText * values_render = Gtk::manage(new Gtk::CellRendererText());
	viewcol = Gtk::manage(new Gtk::TreeView::Column (_("Values (current value on top)"), *values_render));
	viewcol->add_attribute (values_render->property_markup(), tree_cols.values);
	tree_view.append_column (*viewcol);

	select_all_check.signal_toggled().connect (sigc::mem_fun(*this, &SessionMetadataSetImportable::select_all));
}

Gtk::Widget &
SessionMetadataSetImportable::get_tab_widget ()
{
	tab_widget.set_text (name);
	return tab_widget;
}

Gtk::Widget &
SessionMetadataSetImportable::get_select_all_widget ()
{
	select_all_check.set_label (name);
	return select_all_check;
}

void
SessionMetadataSetImportable::load_extra_data (ARDOUR::SessionMetadata const & data)
{
	if (!_session) {
		std::cerr << "Programming error: no session set for SessionMetaDataSetImportable (in load_data)!" << std::endl;
		return;
	}

	ARDOUR::SessionMetadata const & session_data = *(ARDOUR::SessionMetadata::Metadata());

	MetadataPtr session_field;
	MetadataPtr import_field;
	DataList::iterator session_it;
	DataList::iterator import_it;

	// Copy list and load data to import
	for (session_it = session_list.begin(); session_it != session_list.end(); ++session_it) {
		session_field = *session_it;
		session_field->load_data(session_data);
		import_list.push_back (session_field->copy());
	}

	// Fill widget
	session_it = session_list.begin();
	import_it = import_list.begin();
	while (session_it != session_list.end() && import_it != import_list.end()) { // _should_ be the same...
		session_field = *session_it;
		import_field = *import_it;

		import_field->load_data(data); // hasn't been done yet

		// Make string for values TODO get color from somewhere?
		string values = "<span weight=\"ultralight\" color=\"#777\">" + session_field->value() + "</span>\n"
                        + "<span weight=\"bold\">" + import_field->value() + "</span>";

		Gtk::TreeModel::iterator row_iter = tree->append();
		Gtk::TreeModel::Row row = *row_iter;

		row[tree_cols.field] = import_field->name();
		row[tree_cols.values] = values;
		row[tree_cols.import] = false;
		row[tree_cols.data] = import_field;

		++session_it;
		++import_it;
	}
}

void
SessionMetadataSetImportable::save_data ()
{
	if (!_session) {
		std::cerr << "Programming error: no session set for SessionMetaDataSetImportable (in import_data)!" << std::endl;
		return;
	}

	ARDOUR::SessionMetadata & session_data = *(ARDOUR::SessionMetadata::Metadata());

	Gtk::TreeModel::Children fields = tree->children();
	Gtk::TreeModel::Children::iterator it;
	for (it = fields.begin(); it != fields.end(); ++it) {
		if ((*it)[tree_cols.import]) {
			MetadataPtr field = (*it)[tree_cols.data];
			field->save_data (session_data);
		}
	}
}

void
SessionMetadataSetImportable::select_all ()
{
	select_all_check.set_inconsistent (false);
	bool state = select_all_check.get_active();

	Gtk::TreeModel::Children fields = tree->children();
	Gtk::TreeModel::Children::iterator it;
	for (it = fields.begin(); it != fields.end(); ++it) {
		(*it)[tree_cols.import] = state;
	}
}

void
SessionMetadataSetImportable::selection_changed (string const & path)
{
	select_all_check.set_inconsistent (true);

	Gtk::TreeModel::iterator iter = tree->get_iter (path);
	bool value((*iter)[tree_cols.import]);
	(*iter)[tree_cols.import] = !value;
}

/* SessionMetadataDialog */

template <typename DataSet>
SessionMetadataDialog<DataSet>::SessionMetadataDialog (string const & name) :
  ArdourDialog (name, true)
{
	cancel_button = add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	cancel_button->signal_clicked().connect (sigc::mem_fun(*this, &SessionMetadataDialog::end_dialog));
	save_button = add_button (Gtk::Stock::OK, Gtk::RESPONSE_ACCEPT);
	save_button->signal_clicked().connect (sigc::mem_fun(*this, &SessionMetadataDialog::save_and_close));
}

template <typename DataSet>
void
SessionMetadataDialog<DataSet>::init_data ( bool skip_user )
{
	if (!_session) {
		std::cerr << "Programming error: no session set for SessionMetaDataDialog (in init_data)!" << std::endl;
		return;
	}

	if (!skip_user)
		init_user_data ();
	init_track_data ();
	init_album_data ();
	init_people_data ();
	init_school_data ();

	for (DataSetList::iterator it = data_list.begin(); it != data_list.end(); ++it) {
		(*it)->set_session (_session);

		notebook.append_page ((*it)->get_widget(), (*it)->get_tab_widget());
	}
}

template <typename DataSet>
void
SessionMetadataDialog<DataSet>::load_extra_data (ARDOUR::SessionMetadata const & data)
{
	for (DataSetList::iterator it = data_list.begin(); it != data_list.end(); ++it) {
		(*it)->load_extra_data (data);
	}
}

template <typename DataSet>
void
SessionMetadataDialog<DataSet>::save_data ()
{
	for (DataSetList::iterator it = data_list.begin(); it != data_list.end(); ++it) {
		(*it)->save_data ();
	}
}

template <typename DataSet>
void
SessionMetadataDialog<DataSet>::save_and_close ()
{
	save_data ();
	_session->set_dirty();
	end_dialog ();
}

template <typename DataSet>
void
SessionMetadataDialog<DataSet>::end_dialog ()
{
	hide_all();
}

template <typename DataSet>
void
SessionMetadataDialog<DataSet>::warn_user (string const & string)
{
	Gtk::MessageDialog msg (string, false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
	msg.run();
}

template <typename DataSet>
boost::shared_ptr<std::list<Gtk::Widget *> >
SessionMetadataDialog<DataSet>::get_custom_widgets (WidgetFunc f)
{
	WidgetListPtr list (new WidgetList);
	for (DataSetList::iterator it = data_list.begin(); it != data_list.end(); ++it)
	{
		DataSet * set = dynamic_cast<DataSet *> (it->get());
		list->push_back (& CALL_MEMBER_FN (*set, f) ());
	}

	return list;
}

template <typename DataSet>
void
SessionMetadataDialog<DataSet>::add_widget (Gtk::Widget & widget)
{
	get_vbox()->pack_start (widget, true, true, 0);
}

template <typename DataSet>
void
SessionMetadataDialog<DataSet>::init_user_data ()
{
	DataSetPtr data_set (new DataSet (_("User")));
	data_list.push_back (data_set);

	MetadataPtr ptr;

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::user_name, &ARDOUR::SessionMetadata::set_user_name, _("Name")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::user_email, &ARDOUR::SessionMetadata::set_user_email, _("Email")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::user_web, &ARDOUR::SessionMetadata::set_user_web, _("Web")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::organization, &ARDOUR::SessionMetadata::set_organization, _("Organization")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::country, &ARDOUR::SessionMetadata::set_country, _("Country")));
	data_set->add_data_field (ptr);

}

template <typename DataSet>
void
SessionMetadataDialog<DataSet>::init_track_data ()
{
	DataSetPtr data_set (new DataSet (_("Track")));
	data_list.push_back (data_set);

	MetadataPtr ptr;

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::title, &ARDOUR::SessionMetadata::set_title, _("Title")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new NumberMetadataField (&ARDOUR::SessionMetadata::track_number, &ARDOUR::SessionMetadata::set_track_number, _("Track Number"), 3));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::subtitle, &ARDOUR::SessionMetadata::set_subtitle, _("Subtitle")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::grouping, &ARDOUR::SessionMetadata::set_grouping, _("Grouping")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::artist, &ARDOUR::SessionMetadata::set_artist, _("Artist")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::genre, &ARDOUR::SessionMetadata::set_genre, _("Genre")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::comment, &ARDOUR::SessionMetadata::set_comment, _("Comment")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::copyright, &ARDOUR::SessionMetadata::set_copyright, _("Copyright")));
	data_set->add_data_field (ptr);
}

template <typename DataSet>
void
SessionMetadataDialog<DataSet>::init_album_data ()
{
	DataSetPtr data_set (new DataSet (_("Album")));
	data_list.push_back (data_set);

	MetadataPtr ptr;

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::album, &ARDOUR::SessionMetadata::set_album, _("Album")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new NumberMetadataField (&ARDOUR::SessionMetadata::year, &ARDOUR::SessionMetadata::set_year, _("Year"), 4));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::album_artist, &ARDOUR::SessionMetadata::set_album_artist, _("Album Artist")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new NumberMetadataField (&ARDOUR::SessionMetadata::total_tracks, &ARDOUR::SessionMetadata::set_total_tracks, _("Total Tracks"), 3));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::disc_subtitle, &ARDOUR::SessionMetadata::set_disc_subtitle, _("Disc Subtitle")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new NumberMetadataField (&ARDOUR::SessionMetadata::disc_number, &ARDOUR::SessionMetadata::set_disc_number, _("Disc Number"), 2));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new NumberMetadataField (&ARDOUR::SessionMetadata::total_discs, &ARDOUR::SessionMetadata::set_total_discs, _("Total Discs"), 2));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::compilation, &ARDOUR::SessionMetadata::set_compilation, _("Compilation")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::isrc, &ARDOUR::SessionMetadata::set_isrc, _("ISRC")));
	data_set->add_data_field (ptr);
}

template <typename DataSet>
void
SessionMetadataDialog<DataSet>::init_people_data ()
{
	DataSetPtr data_set (new DataSet (_("People")));
	data_list.push_back (data_set);

	MetadataPtr ptr;

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::lyricist, &ARDOUR::SessionMetadata::set_lyricist, _("Lyricist")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::composer, &ARDOUR::SessionMetadata::set_composer, _("Composer")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::conductor, &ARDOUR::SessionMetadata::set_conductor, _("Conductor")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::remixer, &ARDOUR::SessionMetadata::set_remixer, _("Remixer")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::arranger, &ARDOUR::SessionMetadata::set_arranger, _("Arranger")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::engineer, &ARDOUR::SessionMetadata::set_engineer, _("Engineer")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::producer, &ARDOUR::SessionMetadata::set_producer, _("Producer")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::dj_mixer, &ARDOUR::SessionMetadata::set_dj_mixer, _("DJ Mixer")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::mixer, &ARDOUR::SessionMetadata::set_mixer, S_("Metadata|Mixer")));
	data_set->add_data_field (ptr);
}

template <typename DataSet>
void
SessionMetadataDialog<DataSet>::init_school_data ()
{
	DataSetPtr data_set (new DataSet (_("School")));
	data_list.push_back (data_set);

	MetadataPtr ptr;

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::instructor, &ARDOUR::SessionMetadata::set_instructor, _("Instructor")));
	data_set->add_data_field (ptr);

	ptr = MetadataPtr (new TextMetadataField (&ARDOUR::SessionMetadata::course, &ARDOUR::SessionMetadata::set_course, _("Course")));
	data_set->add_data_field (ptr);

}

/* SessionMetadataEditor */

SessionMetadataEditor::SessionMetadataEditor () :
  SessionMetadataDialog<SessionMetadataSetEditable> (_("Edit Session Metadata"))
{

}

SessionMetadataEditor::~SessionMetadataEditor ()
{
	// Remove pages from notebook to get rid of gsignal runtime warnings
	notebook.pages().clear();
}

void
SessionMetadataEditor::run ()
{
	init_data ();
	init_gui();

	ArdourDialog::run();
}

void
SessionMetadataEditor::init_gui ()
{
	add_widget (notebook);

	show_all();
}

/* SessionMetadataImporter */

SessionMetadataImporter::SessionMetadataImporter () :
  SessionMetadataDialog<SessionMetadataSetImportable> (_("Import session metadata"))
{

}

SessionMetadataImporter::~SessionMetadataImporter ()
{
	// Remove pages from notebook to get rid of gsignal runtime warnings
	notebook.pages().clear();
}

void
SessionMetadataImporter::run ()
{
	if (!_session) {
		std::cerr << "Programming error: no session set for SessionMetaDataImporter (in run)!" << std::endl;
		return;
	}

	/* Open session file selector */

	Gtk::FileChooserDialog session_selector(_("Choose session to import metadata from"), Gtk::FILE_CHOOSER_ACTION_OPEN);
	session_selector.add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	session_selector.add_button (Gtk::Stock::OPEN, Gtk::RESPONSE_ACCEPT);
	session_selector.set_default_response(Gtk::RESPONSE_ACCEPT);

	Gtk::FileFilter session_filter;
	session_filter.add_pattern ("*.ardour");
	session_filter.set_name (string_compose (_("%1 sessions"), PROGRAM_NAME));
	session_selector.add_filter (session_filter);
	session_selector.set_filter (session_filter);

	int response = session_selector.run();
	session_selector.hide ();

	switch (response) {
	case Gtk::RESPONSE_ACCEPT:
		break;
	default:
		return;
	}

	string session_path = session_selector.get_filename();
	string path, name;
	bool isnew;

	if (session_path.length() > 0) {
		if (ARDOUR::find_session (session_path, path, name, isnew) != 0) {
			return;
		}
	} else {
		return;
	}

	/* We have a session: load the data and run dialog */

	string filename = Glib::build_filename (path, name + ".ardour");
	XMLTree session_tree;
	if (!session_tree.read (filename)) {
		warn_user (_("This session file could not be read!"));
		return;
	}

	/* XXX GET VERSION FROM TREE */
	int version = 3000;

	XMLNode * node = session_tree.root()->child ("Metadata");

	if (!node) {
		warn_user (_("The session file didn't contain metadata!\nMaybe this is an old session format?"));
		return;
	}

	//create a temporary 
	ARDOUR::SessionMetadata data;
	data.set_state (*node, version);
	init_data ( true );  //skip user data here
	load_extra_data (data);
	init_gui();

	ArdourDialog::run();
}

void
SessionMetadataImporter::init_gui ()
{
	// Select all from -widget
	add_widget (selection_hbox);
	selection_label.set_text (_("Import all from:"));
	selection_hbox.pack_start (selection_label, false, false);

	WidgetListPtr list = get_custom_widgets (&SessionMetadataSetImportable::get_select_all_widget);
	for (WidgetList::iterator it = list->begin(); it != list->end(); ++it) {
		selection_hbox.pack_start (**it, false, false, 6);
	}

	add_widget (notebook);

	show_all();
}
