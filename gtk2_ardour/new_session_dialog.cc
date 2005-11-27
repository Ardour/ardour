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

const char* NewSessionDialogFactory::s_m_top_level_widget_name = X_("new_session_dialog");
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

	xml->get_widget(X_("SessionNameEntry"), m_name);
	xml->get_widget(X_("SessionFolderChooser"), m_folder);
	xml->get_widget(X_("SessionTemplateChooser"), m_template);
	
	xml->get_widget(X_("CreateMasterBus"), m_create_master_track);
	xml->get_widget(X_("MasterChannelCount"), m_master_track_channel_count);
	
	xml->get_widget(X_("CreateControlBus"), m_create_control_track);
	xml->get_widget(X_("ControlChannelCount"), m_control_track_channel_count);

	xml->get_widget(X_("ConnectInputs"), m_connect_inputs);
	xml->get_widget(X_("LimitInputPorts"), m_limit_input_ports);
	xml->get_widget(X_("InputLimitCount"), m_input_limit_count);

	xml->get_widget(X_("ConnectOutputs"), m_connect_outputs);
	xml->get_widget(X_("LimitOutputPorts"), m_limit_output_ports);
	xml->get_widget(X_("OutputLimitCount"), m_output_limit_count);	
	xml->get_widget(X_("ConnectOutsToMaster"), m_connect_outputs_to_master);
	xml->get_widget(X_("ConnectOutsToPhysical"), m_connect_outputs_to_physical);

	///@ todo connect some signals

}

void
NewSessionDialog::set_session_name(const Glib::ustring& name)
{
	m_name->set_text(name);
}

std::string
NewSessionDialog::session_name() const
{
	return Glib::filename_from_utf8(m_name->get_text());
}

std::string
NewSessionDialog::session_folder() const
{
	return Glib::filename_from_utf8(m_folder->get_current_folder());
}

bool
NewSessionDialog::use_session_template() const
{
	if(m_template->get_filename().empty()) return false;
	return true;
}

std::string
NewSessionDialog::session_template_name() const
{
	return Glib::filename_from_utf8(m_template->get_filename());
}

bool
NewSessionDialog::create_master_track() const
{
	return m_create_master_track->get_active();
}

int
NewSessionDialog::master_channel_count() const
{
	return m_master_track_channel_count->get_value_as_int();
}

bool
NewSessionDialog::create_control_track() const
{
	return m_create_control_track->get_active();
}

int
NewSessionDialog::control_channel_count() const
{
	return m_control_track_channel_count->get_value_as_int();
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


void
NewSessionDialog::reset_name()
{
	m_name->set_text(Glib::ustring());
	
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
