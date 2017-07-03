/*
    Copyright (C) 2010 Paul Davis
    Author: Johannes Mueller

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

#ifndef __gtk2_ardour_template_dialog_h__
#define __gtk2_ardour_template_dialog_h__

#include <vector>

#include <gtkmm/liststore.h>
#include <gtkmm/treeview.h>

#include "ardour_dialog.h"

namespace ARDOUR {
	struct TemplateInfo;
}

class TemplateDialog : public ArdourDialog
{
public:
	TemplateDialog ();
	~TemplateDialog () {}
};

class TemplateManager : public Gtk::HBox
{
public:
	virtual ~TemplateManager () {}
	virtual void init () = 0;

protected:
	TemplateManager ();

	void setup_model (const std::vector<ARDOUR::TemplateInfo>& templates);

	void row_selection_changed ();
	void render_template_names (Gtk::CellRenderer* rnd, const Gtk::TreeModel::iterator& it);
	void validate_edit (const Glib::ustring& path_string, const Glib::ustring& new_name);
	void start_edit ();

	bool key_event (GdkEventKey* ev);

	virtual void rename_template (Gtk::TreeModel::iterator& item, const Glib::ustring& new_name) = 0;
	virtual void delete_selected_template () = 0;

	struct SessionTemplateColumns : public Gtk::TreeModel::ColumnRecord {
		SessionTemplateColumns () {
			add (name);
			add (path);
		}

		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> path;
	};

	SessionTemplateColumns _template_columns;
	Glib::RefPtr<Gtk::ListStore>  _template_model;

	Gtk::TreeView _template_treeview;
	Gtk::CellRendererText _validating_cellrenderer;
	Gtk::TreeView::Column _validated_column;

	Gtk::Button _remove_button;
	Gtk::Button _rename_button;
};

class SessionTemplateManager : public TemplateManager
{
public:
	SessionTemplateManager () : TemplateManager () {}
	~SessionTemplateManager () {}

	void init ();

private:
	void rename_template (Gtk::TreeModel::iterator& item, const Glib::ustring& new_name);
	void delete_selected_template ();
};


class RouteTemplateManager : public TemplateManager
{
public:
	RouteTemplateManager () : TemplateManager () {}
	~RouteTemplateManager () {}

	void init ();

private:
	void rename_template (Gtk::TreeModel::iterator& item, const Glib::ustring& new_name);
	void delete_selected_template ();
};


#endif /* __gtk2_ardour_template_dialog_h__ */
