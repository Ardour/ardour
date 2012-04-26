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

#ifndef __session_metadata_dialog_h__
#define __session_metadata_dialog_h__

#include "ardour_dialog.h"

#include <gtkmm.h>
#include <boost/shared_ptr.hpp>

#include <string>
#include <list>

#include "ardour/session_metadata.h"

class MetadataField;
typedef boost::shared_ptr<MetadataField> MetadataPtr;

/// Wraps a metadata field to be used in a GUI
class MetadataField {
  public:
	MetadataField (std::string const & field_name);
	virtual ~MetadataField();
	virtual MetadataPtr copy () = 0;

	virtual void save_data (ARDOUR::SessionMetadata & data) const = 0;
	virtual void load_data (ARDOUR::SessionMetadata const & data) = 0;

	virtual std::string name() { return _name; }
	virtual std::string value() { return _value; }

	/// Get widget containing name of field
	virtual Gtk::Widget & name_widget () = 0;
	/// Get label containing value of field
	virtual Gtk::Widget & value_widget () = 0;
	/// Get widget for editing value
	virtual Gtk::Widget & edit_widget () = 0;
  protected:
        std::string _name;
        std::string _value;
};

/// MetadataField that contains text
class TextMetadataField : public MetadataField {
  private:
	typedef std::string (ARDOUR::SessionMetadata::*Getter) () const;
	typedef void (ARDOUR::SessionMetadata::*Setter) (std::string const &);
  public:
	TextMetadataField (Getter getter, Setter setter, std::string const & field_name, guint width = 50);
	MetadataPtr copy ();

	void save_data (ARDOUR::SessionMetadata & data) const;
	void load_data (ARDOUR::SessionMetadata const & data);

	Gtk::Widget & name_widget ();
	Gtk::Widget & value_widget ();
	Gtk::Widget & edit_widget ();
  private:
	void update_value ();

	Getter getter;
	Setter setter;

	Gtk::Label* label;
	Gtk::Label* value_label;
	Gtk::Entry* entry;

	uint width;
};

/// MetadataField that accepts only numbers
class NumberMetadataField : public MetadataField {
  private:
	typedef uint32_t (ARDOUR::SessionMetadata::*Getter) () const;
	typedef void (ARDOUR::SessionMetadata::*Setter) (uint32_t);
  public:
	NumberMetadataField (Getter getter, Setter setter, std::string const & field_name, guint numbers, guint width = 50);
	MetadataPtr copy ();

	void save_data (ARDOUR::SessionMetadata & data) const;
	void load_data (ARDOUR::SessionMetadata const & data);

	Gtk::Widget & name_widget ();
	Gtk::Widget & value_widget ();
	Gtk::Widget & edit_widget ();
  private:
	void update_value ();
        std::string uint_to_str (uint32_t i) const;
	uint32_t str_to_uint (std::string const & str) const;

	Getter getter;
	Setter setter;

	Gtk::Label* label;
	Gtk::Label* value_label;
	Gtk::Entry* entry;

	guint numbers;
	guint width;
};

/// Interface for MetadataFields
class SessionMetadataSet : public ARDOUR::SessionHandlePtr {
  public:
	SessionMetadataSet (std::string const & name);
	virtual ~SessionMetadataSet () {};

	void add_data_field (MetadataPtr field);

	/// allows loading extra data into data sets (for importing etc.)
	virtual void load_extra_data (ARDOUR::SessionMetadata const & /*data*/) { }
	/// Saves data to session
	virtual void save_data () = 0;

	virtual Gtk::Widget & get_widget () = 0;
	virtual Gtk::Widget & get_tab_widget () = 0;

  protected:
	typedef std::list<MetadataPtr> DataList;
	DataList list;
        std::string name;
};

/// Contains MetadataFields for editing
class SessionMetadataSetEditable : public SessionMetadataSet {
  public:
	SessionMetadataSetEditable (std::string const & name);

	Gtk::Widget & get_widget () { return vbox; }
	Gtk::Widget & get_tab_widget ();

	/// Sets session and loads data
	void set_session (ARDOUR::Session * s);
	/// Saves from MetadataFields into data
	void save_data ();

  private:
	Gtk::VBox vbox;
	Gtk::Table table;
	Gtk::Label tab_widget;
};

/// Contains MetadataFields for importing
class SessionMetadataSetImportable : public SessionMetadataSet {
  public:
	SessionMetadataSetImportable (std::string const & name);

	Gtk::Widget & get_widget () { return tree_view; }
	Gtk::Widget & get_tab_widget ();
	Gtk::Widget & get_select_all_widget ();

	/// Loads importable data from data
	void load_extra_data (ARDOUR::SessionMetadata const & data);
	/// Saves from importable data (see load_data) to session_data
	void save_data ();

  private:
	DataList & session_list; // References MetadataSet::list
	DataList import_list;

	struct Columns : public Gtk::TreeModel::ColumnRecord
	{
	  public:
		Gtk::TreeModelColumn<std::string>     field;
		Gtk::TreeModelColumn<std::string>     values;
		Gtk::TreeModelColumn<bool>        import;
		Gtk::TreeModelColumn<MetadataPtr> data;

		Columns() { add (field); add (values); add (import); add (data); }
	};

	Glib::RefPtr<Gtk::ListStore>  tree;
	Columns                       tree_cols;
	Gtk::TreeView                 tree_view;

	Gtk::Label                    tab_widget;
	Gtk::CheckButton              select_all_check;

	void select_all ();
	void selection_changed (std::string const & path);
};

/// Metadata dialog interface
/**
 * The DataSets are initalized in this class so that all
 * Dialogs have the same sets of data in the same order.
 */
template <typename DataSet>
class SessionMetadataDialog : public ArdourDialog
{
  public:
	SessionMetadataDialog (std::string const & name);

  protected:
	void init_data ( bool skip_user = false );
	void load_extra_data (ARDOUR::SessionMetadata const & data);
	void save_data ();

	virtual void init_gui () = 0;
	virtual void save_and_close ();
	virtual void end_dialog ();

	void warn_user (std::string const & string);

	typedef std::list<Gtk::Widget *> WidgetList;
	typedef boost::shared_ptr<WidgetList> WidgetListPtr;
	typedef Gtk::Widget & (DataSet::*WidgetFunc) ();

	/// Returns list of widgets gathered by calling f for each data set
	WidgetListPtr get_custom_widgets (WidgetFunc f);

	/// Adds a widget to the table (vertical stacking) with automatic spacing
	void add_widget (Gtk::Widget & widget);

	Gtk::Notebook     notebook;

  private:
	void init_user_data ();
	void init_track_data ();
	void init_album_data ();
	void init_people_data ();
	void init_school_data ();

	typedef boost::shared_ptr<SessionMetadataSet> DataSetPtr;
	typedef std::list<DataSetPtr> DataSetList;
	DataSetList data_list;

	Gtk::Button *     save_button;
	Gtk::Button *     cancel_button;
};

class SessionMetadataEditor : public SessionMetadataDialog<SessionMetadataSetEditable> {
  public:
	SessionMetadataEditor ();
	~SessionMetadataEditor ();
	void run ();
  private:
	void init_gui ();
};

class SessionMetadataImporter : public SessionMetadataDialog<SessionMetadataSetImportable> {
  public:
	SessionMetadataImporter ();
	~SessionMetadataImporter ();
	void run ();

  private:
	void init_gui ();

	// Select all from -widget
	Gtk::HBox    selection_hbox;
	Gtk::Label   selection_label;

};

#endif
