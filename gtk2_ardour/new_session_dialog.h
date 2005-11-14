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

// -*- c++ -*-

#ifndef NEW_SESSION_DIALOG_H
#define NEW_SESSION_DIALOG_H

#include <string>
#include <gtkmm.h>

#include "glade_factory.h"

struct NewSessionDialogFactory : public GladeFactory
{
	static GladeRef create();

	static const char* top_level_widget_name();

private:

	static const char* s_m_top_level_widget_name;
	
};

class NewSessionDialog : public Gtk::Dialog
{
public:
		
	NewSessionDialog(BaseObjectType* cobject,
			 const Glib::RefPtr<Gnome::Glade::Xml>& xml);

	void set_session_name(const Glib::ustring& name);

	std::string session_name() const;
	std::string session_folder() const;
	
	bool use_session_template() const;
	std::string session_template_name() const;

	// advanced.

	bool create_master_track() const;
		int master_channel_count() const;

	bool create_control_track() const;
	int control_channel_count() const;

	bool connect_inputs() const;
	bool limit_inputs_used_for_connection() const;
	int input_limit_count() const;

	bool connect_outputs() const;
	bool limit_outputs_used_for_connection() const;
	int output_limit_count() const;

	bool connect_outs_to_master() const;
	bool connect_outs_to_physical() const ;

protected:

	void reset_name();
	void reset_template();
	
	// reset everything to default values.
	void reset();

	// references to widgets we care about.

	Gtk::Entry*  m_name;
	Gtk::FileChooserButton* m_folder;
	Gtk::FileChooserButton* m_template;
	
	Gtk::CheckButton* m_create_master_track;
	Gtk::SpinButton* m_master_track_channel_count;
       	
	Gtk::CheckButton* m_create_control_track;
	Gtk::SpinButton* m_control_track_channel_count;

	Gtk::CheckButton* m_connect_inputs;
	Gtk::CheckButton* m_limit_input_ports;
	Gtk::SpinButton* m_input_limit_count;

	Gtk::CheckButton* m_connect_outputs;	
	Gtk::CheckButton* m_limit_output_ports;
	Gtk::SpinButton* m_output_limit_count;

	Gtk::RadioButton* m_connect_outputs_to_master;
	Gtk::RadioButton* m_connect_outputs_to_physical;
	
};

#endif // NEW_SESSION_DIALOG_H
