/*
    Copyright (C) 2005 Paul Davis 

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

    $Id$
*/

#include "i18n.h"
#include "new_session_dialog.h"
#include "glade_path.h"

#include <ardour/recent_sessions.h>
#include <ardour/session.h>

#include <pbd/basename.h>

#include <gtkmm/entry.h>
#include <gtkmm/filechooserbutton.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/filefilter.h>
#include <gtkmm/stock.h>


const char* NewSessionDialogFactory::s_m_top_level_widget_name = X_("NewSessionDialog");
const char* NewSessionDialogFactory::top_level_widget_name() { return s_m_top_level_widget_name; }

Glib::RefPtr<Gnome::Glade::Xml>
NewSessionDialogFactory::create()
{
	return GladeFactory::create(GladePath::path(X_("new_session_dialog.glade")));
}


NewSessionDialog::NewSessionDialog(BaseObjectType* cobject,
				   const Glib::RefPtr<Gnome::Glade::Xml>& xml)
	: Gtk::Dialog(cobject)	  
{
	// look up the widgets we care about.
        xml->get_widget(X_("NewSessionDialog"), m_new_session_dialog);
	xml->get_widget(X_("SessionNameEntry"), m_name);
	xml->get_widget(X_("SessionFolderChooser"), m_folder);
	xml->get_widget(X_("SessionTemplateChooser"), m_template);
	
	xml->get_widget(X_("CreateMasterBus"), m_create_master_bus);
	xml->get_widget(X_("MasterChannelCount"), m_master_bus_channel_count);
	
	xml->get_widget(X_("CreateControlBus"), m_create_control_bus);
	xml->get_widget(X_("ControlChannelCount"), m_control_bus_channel_count);

	xml->get_widget(X_("ConnectInputs"), m_connect_inputs);
	xml->get_widget(X_("LimitInputPorts"), m_limit_input_ports);
	xml->get_widget(X_("InputLimitCount"), m_input_limit_count);

	xml->get_widget(X_("ConnectOutputs"), m_connect_outputs);
	xml->get_widget(X_("LimitOutputPorts"), m_limit_output_ports);
	xml->get_widget(X_("OutputLimitCount"), m_output_limit_count);	
	xml->get_widget(X_("ConnectOutsToMaster"), m_connect_outputs_to_master);
	xml->get_widget(X_("ConnectOutsToPhysical"), m_connect_outputs_to_physical);

	xml->get_widget(X_("OpenFilechooserButton"), m_open_filechooser);
	xml->get_widget(X_("TheNotebook"), m_notebook);
	xml->get_widget(X_("TheTreeview"), m_treeview);
	xml->get_widget(X_("OkButton"), m_okbutton);


	if (m_treeview) {
	        /* Shamelessly ripped from ardour_ui.cc */
	        std::vector<string *> *sessions;
		std::vector<string *>::iterator i;
		RecentSessionsSorter cmp;
		
		recent_model = Gtk::TreeStore::create (recent_columns);
		m_treeview->set_model (recent_model);
		m_treeview->append_column (_("Recent Sessions"), recent_columns.visible_name);
		m_treeview->set_headers_visible (false);
		m_treeview->get_selection()->set_mode (Gtk::SELECTION_SINGLE);
		
		recent_model->clear ();
		
		ARDOUR::RecentSessions rs;
		ARDOUR::read_recent_sessions (rs);
		
		/* sort them alphabetically */
		sort (rs.begin(), rs.end(), cmp);
		sessions = new std::vector<std::string*>;
		
		for (ARDOUR::RecentSessions::iterator i = rs.begin(); i != rs.end(); ++i) {
		        sessions->push_back (new string ((*i).second));
		}
	  
		for (i = sessions->begin(); i != sessions->end(); ++i) {
		  
		  std::vector<std::string*>* states;
		  std::vector<const gchar*> item;
		  std::string fullpath = *(*i);
		  
		  /* remove any trailing / */
		  
		  if (fullpath[fullpath.length()-1] == '/') {
		          fullpath = fullpath.substr (0, fullpath.length()-1);
		  }
	    
		  /* now get available states for this session */
		  
		  if ((states = ARDOUR::Session::possible_states (fullpath)) == 0) {
		          /* no state file? */
		          continue;
		  }
	    
		  Gtk::TreeModel::Row row = *(recent_model->append());
		  
		  row[recent_columns.visible_name] = PBD::basename (fullpath);
		  row[recent_columns.fullpath] = fullpath;
		  
		  if (states->size() > 1) {
		    
		          /* add the children */
		    
		          for (std::vector<std::string*>::iterator i2 = states->begin(); i2 != states->end(); ++i2) {
			    
			          Gtk::TreeModel::Row child_row = *(recent_model->append (row.children()));
				  
				  child_row[recent_columns.visible_name] = **i2;
				  child_row[recent_columns.fullpath] = fullpath;
				  
				  delete *i2;
			  }
		  }
		  
		  delete states;
		}
		delete sessions;
	}
	
	m_new_session_dialog->set_response_sensitive (Gtk::RESPONSE_OK, false);
	m_new_session_dialog->set_default_response (Gtk::RESPONSE_OK);
	m_notebook->show_all_children();
	m_notebook->set_current_page(0);

	Gtk::FileFilter* filter = manage (new (Gtk::FileFilter));

	filter->add_pattern(X_("*.ardour"));
	filter->add_pattern(X_("*.ardour.bak"));
	m_open_filechooser->set_filter (*filter);

	///@ todo connect some signals

	m_name->signal_key_release_event().connect(mem_fun (*this, &NewSessionDialog::entry_key_release));
	m_notebook->signal_switch_page().connect (mem_fun (*this, &NewSessionDialog::notebook_page_changed));
	m_treeview->get_selection()->signal_changed().connect (mem_fun (*this, &NewSessionDialog::treeview_selection_changed));
	m_treeview->signal_row_activated().connect (mem_fun (*this, &NewSessionDialog::recent_row_activated));
	m_open_filechooser->signal_selection_changed ().connect (mem_fun (*this, &NewSessionDialog::file_chosen));
}

void
NewSessionDialog::set_session_name(const Glib::ustring& name)
{
	m_name->set_text(name);
}

std::string
NewSessionDialog::session_name() const
{
        std::string str = Glib::filename_from_utf8(m_open_filechooser->get_filename());
	std::string::size_type position = str.find_last_of ('/');
	str = str.substr (position+1);
	position = str.find_last_of ('.');
	str = str.substr (0, position);

	/*
	  XXX what to do if it's a .bak file?
	  load_session doesn't allow it!

	if ((position = str.rfind(".bak")) != string::npos) {
	        str = str.substr (0, position);
	}	  
	*/

	if (m_notebook->get_current_page() == 0) {
	        return Glib::filename_from_utf8(m_name->get_text());
	} else {
		if (m_treeview->get_selection()->count_selected_rows() == 0) {
		        return Glib::filename_from_utf8(str);
		}
		Gtk::TreeModel::iterator i = m_treeview->get_selection()->get_selected();
		return (*i)[recent_columns.visible_name];
	}
}

std::string
NewSessionDialog::session_folder() const
{
        if (m_notebook->get_current_page() == 0) {
	        return Glib::filename_from_utf8(m_folder->get_current_folder());
	} else {
	       
		if (m_treeview->get_selection()->count_selected_rows() == 0) {
		        return Glib::filename_from_utf8(m_open_filechooser->get_current_folder());
		}
		Gtk::TreeModel::iterator i = m_treeview->get_selection()->get_selected();
		return (*i)[recent_columns.fullpath];
	}
}

bool
NewSessionDialog::use_session_template() const
{
        if(m_template->get_filename().empty() && (m_notebook->get_current_page() == 0)) return false;
	return true;
}

std::string
NewSessionDialog::session_template_name() const
{
	return Glib::filename_from_utf8(m_template->get_filename());
}

bool
NewSessionDialog::create_master_bus() const
{
	return m_create_master_bus->get_active();
}

int
NewSessionDialog::master_channel_count() const
{
	return m_master_bus_channel_count->get_value_as_int();
}

bool
NewSessionDialog::create_control_bus() const
{
	return m_create_control_bus->get_active();
}

int
NewSessionDialog::control_channel_count() const
{
	return m_control_bus_channel_count->get_value_as_int();
}

bool
NewSessionDialog::connect_inputs() const
{
	return m_connect_inputs->get_active();
}

bool
NewSessionDialog::limit_inputs_used_for_connection() const
{
	return m_limit_input_ports->get_active();
}

int
NewSessionDialog::input_limit_count() const
{
	return m_input_limit_count->get_value_as_int();
}

bool
NewSessionDialog::connect_outputs() const
{
	return m_connect_outputs->get_active();
}

bool
NewSessionDialog::limit_outputs_used_for_connection() const
{
	return m_limit_output_ports->get_active();
}

int
NewSessionDialog::output_limit_count() const
{
	return m_output_limit_count->get_value_as_int();
}

bool
NewSessionDialog::connect_outs_to_master() const
{
	return m_connect_outputs_to_master->get_active();
}

bool
NewSessionDialog::connect_outs_to_physical() const
{
	return m_connect_outputs_to_physical->get_active();
}

int
NewSessionDialog::get_current_page()
{
	return m_notebook->get_current_page();
	
}

void
NewSessionDialog::reset_name()
{
	m_name->set_text(Glib::ustring());
	
}

bool
NewSessionDialog::entry_key_release (GdkEventKey* ev)
{
        if (m_name->get_text() != "") {
	        m_new_session_dialog->set_response_sensitive (Gtk::RESPONSE_OK, true);
	} else {
	        m_new_session_dialog->set_response_sensitive (Gtk::RESPONSE_OK, false);
	}
	return true;
}

void
NewSessionDialog::notebook_page_changed (GtkNotebookPage* np, uint pagenum)
{
        if (pagenum == 1) {
	        m_okbutton->set_label(_("Open"));
		m_okbutton->set_image (*(new Gtk::Image (Gtk::Stock::OPEN, Gtk::ICON_SIZE_BUTTON)));
		if (m_treeview->get_selection()->count_selected_rows() == 0) {
		        m_new_session_dialog->set_response_sensitive (Gtk::RESPONSE_OK, false);
		} else {
		        m_new_session_dialog->set_response_sensitive (Gtk::RESPONSE_OK, true);
		}
	} else {
	        m_okbutton->set_label(_("New"));
	        m_okbutton->set_image (*(new Gtk::Image (Gtk::Stock::NEW, Gtk::ICON_SIZE_BUTTON)));
		if (m_name->get_text() == "") {
		        m_new_session_dialog->set_response_sensitive (Gtk::RESPONSE_OK, false);
		} else {
		        m_new_session_dialog->set_response_sensitive (Gtk::RESPONSE_OK, true);
		}
	}
}

void
NewSessionDialog::treeview_selection_changed ()
{
  if (m_treeview->get_selection()->count_selected_rows() == 0) {
          if (!m_open_filechooser->get_filename().empty()) {
	          m_new_session_dialog->set_response_sensitive (Gtk::RESPONSE_OK, true);
	  } else {
	          m_new_session_dialog->set_response_sensitive (Gtk::RESPONSE_OK, false);
	  }
  } else {
          m_new_session_dialog->set_response_sensitive (Gtk::RESPONSE_OK, true);
  }
}

void
NewSessionDialog::file_chosen ()
{
        m_treeview->get_selection()->unselect_all();
  
	if (m_treeview->get_selection()->count_selected_rows() == 0) {
	        m_new_session_dialog->set_response_sensitive (Gtk::RESPONSE_OK, true);
	}
}

void
NewSessionDialog::recent_row_activated (const Gtk::TreePath& path, Gtk::TreeViewColumn* col)
{
        m_new_session_dialog->response (Gtk::RESPONSE_YES);
}

/// @todo
void
NewSessionDialog::reset_template()
{

}

void
NewSessionDialog::reset()
{
	reset_name();
	reset_template();
}
