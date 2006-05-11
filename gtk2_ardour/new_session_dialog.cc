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


NewSessionDialog::NewSessionDialog()
	: ArdourDialog ("New Session Dialog")
{
   session_name_label = Gtk::manage(new class Gtk::Label(_("Session Name")));
   m_name = Gtk::manage(new class Gtk::Entry());
   session_location_label = Gtk::manage(new class Gtk::Label(_("Session Location")));
   m_folder = Gtk::manage(new class Gtk::FileChooserButton(Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER));
   session_template_label = Gtk::manage(new class Gtk::Label(_("Session Template")));
   m_template = Gtk::manage(new class Gtk::FileChooserButton());
   chan_count_label = Gtk::manage(new class Gtk::Label(_("Channel Count")));
   m_create_control_bus = Gtk::manage(new class Gtk::CheckButton(_("Create Control Bus")));
   
   Gtk::Adjustment *m_control_bus_channel_count_adj = Gtk::manage(new class Gtk::Adjustment(2, 0, 100, 1, 10, 10));
   m_control_bus_channel_count = Gtk::manage(new class Gtk::SpinButton(*m_control_bus_channel_count_adj, 1, 0));
   
   Gtk::Adjustment *m_master_bus_channel_count_adj = Gtk::manage(new class Gtk::Adjustment(2, 0, 100, 1, 10, 10));
   m_master_bus_channel_count = Gtk::manage(new class Gtk::SpinButton(*m_master_bus_channel_count_adj, 1, 0));
   m_create_master_bus = Gtk::manage(new class Gtk::CheckButton(_("Create Master Bus")));
   advanced_table = Gtk::manage(new class Gtk::Table(2, 2, true));
   options_label = Gtk::manage(new class Gtk::Label(_("Track/Bus connection options")));
   m_connect_inputs = Gtk::manage(new class Gtk::CheckButton(_("Automatically connect inputs")));
   m_limit_input_ports = Gtk::manage(new class Gtk::CheckButton(_("Port limit")));
   
   Gtk::Adjustment *m_input_limit_count_adj = Gtk::manage(new class Gtk::Adjustment(1, 0, 100, 1, 10, 10));
   m_input_limit_count = Gtk::manage(new class Gtk::SpinButton(*m_input_limit_count_adj, 1, 0));
   input_port_limit_hbox = Gtk::manage(new class Gtk::HBox(false, 0));
   input_port_hbox = Gtk::manage(new class Gtk::HBox(false, 0));
   input_table = Gtk::manage(new class Gtk::Table(2, 2, false));
   input_port_alignment = Gtk::manage(new class Gtk::Alignment(0.5, 0.5, 1, 1));
   input_label = Gtk::manage(new class Gtk::Label(_("<b>Input</b>")));
   input_frame = Gtk::manage(new class Gtk::Frame());
   m_connect_outputs = Gtk::manage(new class Gtk::CheckButton(_("Automatically connect outputs")));
   m_limit_output_ports = Gtk::manage(new class Gtk::CheckButton(_("Port limit")));
   
   Gtk::Adjustment *m_output_limit_count_adj = Gtk::manage(new class Gtk::Adjustment(1, 0, 100, 1, 10, 10));
   m_output_limit_count = Gtk::manage(new class Gtk::SpinButton(*m_output_limit_count_adj, 1, 0));
   output_port_limit_hbox = Gtk::manage(new class Gtk::HBox(false, 0));
   output_port_hbox = Gtk::manage(new class Gtk::HBox(false, 0));
   
   Gtk::RadioButton::Group _RadioBGroup_m_connect_outputs_to_master;
   m_connect_outputs_to_master = Gtk::manage(new class Gtk::RadioButton(_RadioBGroup_m_connect_outputs_to_master, _("Connect to Master Bus")));
   m_connect_outputs_to_physical = Gtk::manage(new class Gtk::RadioButton(_RadioBGroup_m_connect_outputs_to_master, _("Connect to physical outputs")));
   output_conn_vbox = Gtk::manage(new class Gtk::VBox(false, 0));
   output_vbox = Gtk::manage(new class Gtk::VBox(false, 0));
   output_port_alignment = Gtk::manage(new class Gtk::Alignment(0.5, 0.5, 1, 1));
   output_label = Gtk::manage(new class Gtk::Label(_("<b>Output</b>")));
   output_frame = Gtk::manage(new class Gtk::Frame());
   advanced_vbox = Gtk::manage(new class Gtk::VBox(false, 0));
   advanced_label = Gtk::manage(new class Gtk::Label(_("<b>Advanced</b>")));
   advanced_expander = Gtk::manage(new class Gtk::Expander());
   new_session_table = Gtk::manage(new class Gtk::Table(2, 2, false));
   m_open_filechooser = Gtk::manage(new class Gtk::FileChooserButton());
   open_session_hbox = Gtk::manage(new class Gtk::HBox(false, 0));
   open_session_alignment = Gtk::manage(new class Gtk::Alignment(0.5, 0.5, 1, 1));
   open_sesion_label = Gtk::manage(new class Gtk::Label(_("Open Session")));
   open_session_frame = Gtk::manage(new class Gtk::Frame());
   m_treeview = Gtk::manage(new class Gtk::TreeView());
   recent_scrolledwindow = Gtk::manage(new class Gtk::ScrolledWindow());
   recent_alignment = Gtk::manage(new class Gtk::Alignment(0.5, 0.5, 1, 1));
   recent_sesion_label = Gtk::manage(new class Gtk::Label(_("Open Recent Session")));
   recent_frame = Gtk::manage(new class Gtk::Frame());
   open_session_vbox = Gtk::manage(new class Gtk::VBox(false, 0));
   m_notebook = Gtk::manage(new class Gtk::Notebook());
   session_name_label->set_alignment(0.5,0.5);
   session_name_label->set_padding(0,0);
   session_name_label->set_justify(Gtk::JUSTIFY_LEFT);
   session_name_label->set_line_wrap(false);
   session_name_label->set_use_markup(false);
   session_name_label->set_selectable(false);
   m_name->set_visibility(true);
   m_name->set_editable(true);
   m_name->set_max_length(0);
   m_name->set_text("");
   m_name->set_has_frame(true);
   m_name->set_activates_default(false);
   session_location_label->set_alignment(0.5,0.5);
   session_location_label->set_padding(0,0);
   session_location_label->set_justify(Gtk::JUSTIFY_LEFT);
   session_location_label->set_line_wrap(false);
   session_location_label->set_use_markup(false);
   session_location_label->set_selectable(false);
   session_template_label->set_alignment(0.5,0.5);
   session_template_label->set_padding(0,0);
   session_template_label->set_justify(Gtk::JUSTIFY_LEFT);
   session_template_label->set_line_wrap(false);
   session_template_label->set_use_markup(false);
   session_template_label->set_selectable(false);
   m_create_control_bus->set_flags(Gtk::CAN_FOCUS);
   m_create_control_bus->set_relief(Gtk::RELIEF_NORMAL);
   m_create_control_bus->set_mode(true);
   m_create_control_bus->set_active(false);
   m_control_bus_channel_count->set_flags(Gtk::CAN_FOCUS);
   m_control_bus_channel_count->set_update_policy(Gtk::UPDATE_ALWAYS);
   m_control_bus_channel_count->set_numeric(true);
   m_control_bus_channel_count->set_digits(0);
   m_control_bus_channel_count->set_wrap(false);
   m_master_bus_channel_count->set_flags(Gtk::CAN_FOCUS);
   m_master_bus_channel_count->set_update_policy(Gtk::UPDATE_ALWAYS);
   m_master_bus_channel_count->set_numeric(true);
   m_master_bus_channel_count->set_digits(0);
   m_master_bus_channel_count->set_wrap(false);
   m_create_master_bus->set_flags(Gtk::CAN_FOCUS);
   m_create_master_bus->set_relief(Gtk::RELIEF_NORMAL);
   m_create_master_bus->set_mode(true);
   m_create_master_bus->set_active(true);
   advanced_table->set_row_spacings(0);
   advanced_table->set_col_spacings(0);
   advanced_table->attach(*chan_count_label, 1, 2, 0, 1, Gtk::AttachOptions(), Gtk::AttachOptions(), 0, 0);
   advanced_table->attach(*m_create_control_bus, 0, 1, 2, 3, Gtk::FILL, Gtk::AttachOptions(), 0, 0);
   advanced_table->attach(*m_control_bus_channel_count, 1, 2, 2, 3, Gtk::AttachOptions(), Gtk::AttachOptions(), 0, 0);
   advanced_table->attach(*m_master_bus_channel_count, 1, 2, 1, 2, Gtk::AttachOptions(), Gtk::AttachOptions(), 0, 0);
   advanced_table->attach(*m_create_master_bus, 0, 1, 1, 2, Gtk::FILL, Gtk::AttachOptions(), 0, 0);
   options_label->set_alignment(0.5,0.5);
   options_label->set_padding(0,0);
   options_label->set_justify(Gtk::JUSTIFY_LEFT);
   options_label->set_line_wrap(false);
   options_label->set_use_markup(false);
   options_label->set_selectable(false);
   m_connect_inputs->set_flags(Gtk::CAN_FOCUS);
   m_connect_inputs->set_relief(Gtk::RELIEF_NORMAL);
   m_connect_inputs->set_mode(true);
   m_connect_inputs->set_active(false);
   m_limit_input_ports->set_flags(Gtk::CAN_FOCUS);
   m_limit_input_ports->set_relief(Gtk::RELIEF_NORMAL);
   m_limit_input_ports->set_mode(true);
   m_limit_input_ports->set_active(false);
   m_input_limit_count->set_flags(Gtk::CAN_FOCUS);
   m_input_limit_count->set_update_policy(Gtk::UPDATE_ALWAYS);
   m_input_limit_count->set_numeric(true);
   m_input_limit_count->set_digits(0);
   m_input_limit_count->set_wrap(false);
   input_port_limit_hbox->pack_start(*m_limit_input_ports, Gtk::PACK_SHRINK, 0);
   input_port_limit_hbox->pack_start(*m_input_limit_count);
   input_port_hbox->pack_start(*m_connect_inputs, Gtk::PACK_SHRINK, 0);
   input_port_hbox->pack_start(*input_port_limit_hbox, Gtk::PACK_EXPAND_PADDING, 0);
   input_table->set_row_spacings(0);
   input_table->set_col_spacings(0);
   input_table->attach(*input_port_hbox, 0, 1, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 0, 0);
   input_port_alignment->add(*input_table);
   input_label->set_alignment(0.5,0.5);
   input_label->set_padding(0,0);
   input_label->set_justify(Gtk::JUSTIFY_LEFT);
   input_label->set_line_wrap(false);
   input_label->set_use_markup(true);
   input_label->set_selectable(false);
   input_frame->set_shadow_type(Gtk::SHADOW_NONE);
   input_frame->set_label_align(0,0.5);
   input_frame->add(*input_port_alignment);
   input_frame->set_label_widget(*input_label);
   m_connect_outputs->set_flags(Gtk::CAN_FOCUS);
   m_connect_outputs->set_relief(Gtk::RELIEF_NORMAL);
   m_connect_outputs->set_mode(true);
   m_connect_outputs->set_active(false);
   m_limit_output_ports->set_flags(Gtk::CAN_FOCUS);
   m_limit_output_ports->set_relief(Gtk::RELIEF_NORMAL);
   m_limit_output_ports->set_mode(true);
   m_limit_output_ports->set_active(false);
   m_output_limit_count->set_flags(Gtk::CAN_FOCUS);
   m_output_limit_count->set_update_policy(Gtk::UPDATE_ALWAYS);
   m_output_limit_count->set_numeric(false);
   m_output_limit_count->set_digits(0);
   m_output_limit_count->set_wrap(false);
   output_port_limit_hbox->pack_start(*m_limit_output_ports, Gtk::PACK_SHRINK, 0);
   output_port_limit_hbox->pack_start(*m_output_limit_count);
   output_port_hbox->pack_start(*m_connect_outputs, Gtk::PACK_SHRINK, 0);
   output_port_hbox->pack_start(*output_port_limit_hbox, Gtk::PACK_EXPAND_PADDING, 0);
   m_connect_outputs_to_master->set_flags(Gtk::CAN_FOCUS);
   m_connect_outputs_to_master->set_relief(Gtk::RELIEF_NORMAL);
   m_connect_outputs_to_master->set_mode(true);
   m_connect_outputs_to_master->set_active(false);
   m_connect_outputs_to_physical->set_flags(Gtk::CAN_FOCUS);
   m_connect_outputs_to_physical->set_relief(Gtk::RELIEF_NORMAL);
   m_connect_outputs_to_physical->set_mode(true);
   m_connect_outputs_to_physical->set_active(false);
   output_conn_vbox->pack_start(*m_connect_outputs_to_master, Gtk::PACK_SHRINK, 0);
   output_conn_vbox->pack_start(*m_connect_outputs_to_physical, Gtk::PACK_SHRINK, 0);
   output_vbox->pack_start(*output_port_hbox);
   output_vbox->pack_start(*output_conn_vbox);
   output_port_alignment->add(*output_vbox);
   output_label->set_alignment(0.5,0.5);
   output_label->set_padding(0,0);
   output_label->set_justify(Gtk::JUSTIFY_LEFT);
   output_label->set_line_wrap(false);
   output_label->set_use_markup(true);
   output_label->set_selectable(false);
   output_frame->set_shadow_type(Gtk::SHADOW_NONE);
   output_frame->set_label_align(0,0.5);
   output_frame->add(*output_port_alignment);
   output_frame->set_label_widget(*output_label);
   advanced_vbox->pack_start(*advanced_table, Gtk::PACK_SHRINK, 0);
   advanced_vbox->pack_start(*options_label, Gtk::PACK_SHRINK, 14);
   advanced_vbox->pack_start(*input_frame);
   advanced_vbox->pack_start(*output_frame);
   advanced_label->set_alignment(0.5,0.5);
   advanced_label->set_padding(0,0);
   advanced_label->set_justify(Gtk::JUSTIFY_LEFT);
   advanced_label->set_line_wrap(false);
   advanced_label->set_use_markup(true);
   advanced_label->set_selectable(false);
   advanced_expander->set_flags(Gtk::CAN_FOCUS);
   advanced_expander->set_border_width(10);
   advanced_expander->set_expanded(true);
   advanced_expander->set_spacing(0);
   advanced_expander->add(*advanced_vbox);
   advanced_expander->set_label_widget(*advanced_label);
   new_session_table->set_border_width(5);
   new_session_table->set_row_spacings(1);
   new_session_table->set_col_spacings(1);
   new_session_table->attach(*session_name_label, 0, 1, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 0, 0);
   new_session_table->attach(*m_name, 1, 2, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 0, 0);
   new_session_table->attach(*session_location_label, 0, 1, 1, 2, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 0, 0);
   new_session_table->attach(*m_folder, 1, 2, 1, 2, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 0, 0);
   new_session_table->attach(*session_template_label, 0, 1, 2, 3, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 0, 0);
   new_session_table->attach(*m_template, 1, 2, 2, 3, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 0, 0);
   new_session_table->attach(*advanced_expander, 0, 2, 3, 4, Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 0, 0);
   chan_count_label->set_alignment(0.5,0.5);
   chan_count_label->set_padding(0,0);
   chan_count_label->set_justify(Gtk::JUSTIFY_LEFT);
   chan_count_label->set_line_wrap(false);
   chan_count_label->set_use_markup(false);
   chan_count_label->set_selectable(false);
   open_session_hbox->pack_start(*m_open_filechooser);
   open_session_alignment->add(*open_session_hbox);
   open_sesion_label->set_alignment(0.5,0.5);
   open_sesion_label->set_padding(0,0);
   open_sesion_label->set_justify(Gtk::JUSTIFY_LEFT);
   open_sesion_label->set_line_wrap(false);
   open_sesion_label->set_use_markup(false);
   open_sesion_label->set_selectable(false);
   open_session_frame->set_border_width(10);
   open_session_frame->set_shadow_type(Gtk::SHADOW_IN);
   open_session_frame->set_label_align(0,0.5);
   open_session_frame->add(*open_session_alignment);
   open_session_frame->set_label_widget(*open_sesion_label);
   m_treeview->set_flags(Gtk::CAN_FOCUS);
   m_treeview->set_headers_visible(true);
   m_treeview->set_rules_hint(false);
   m_treeview->set_reorderable(false);
   m_treeview->set_enable_search(true);
   m_treeview->set_fixed_height_mode(false);
   m_treeview->set_hover_selection(false);
   m_treeview->set_hover_expand(true);
   recent_scrolledwindow->set_flags(Gtk::CAN_FOCUS);
   recent_scrolledwindow->set_border_width(10);
   recent_scrolledwindow->set_shadow_type(Gtk::SHADOW_IN);
   recent_scrolledwindow->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
   recent_scrolledwindow->property_window_placement().set_value(Gtk::CORNER_TOP_LEFT);
   recent_scrolledwindow->add(*m_treeview);
   recent_alignment->add(*recent_scrolledwindow);
   recent_sesion_label->set_alignment(0.5,0.5);
   recent_sesion_label->set_padding(0,0);
   recent_sesion_label->set_justify(Gtk::JUSTIFY_LEFT);
   recent_sesion_label->set_line_wrap(false);
   recent_sesion_label->set_use_markup(false);
   recent_sesion_label->set_selectable(false);
   recent_frame->set_border_width(10);
   recent_frame->set_shadow_type(Gtk::SHADOW_IN);
   recent_frame->set_label_align(0,0.5);
   recent_frame->add(*recent_alignment);
   recent_frame->set_label_widget(*recent_sesion_label);
   open_session_vbox->pack_start(*open_session_frame, Gtk::PACK_SHRINK, 0);
   open_session_vbox->pack_start(*recent_frame, Gtk::PACK_EXPAND_WIDGET, 5);
   m_notebook->set_flags(Gtk::CAN_FOCUS);
   m_notebook->set_show_tabs(true);
   m_notebook->set_show_border(true);
   m_notebook->set_tab_pos(Gtk::POS_TOP);
   m_notebook->set_scrollable(false);
   m_notebook->append_page(*new_session_table, _("New Session"));
   m_notebook->pages().back().set_tab_label_packing(false, true, Gtk::PACK_START);
   m_notebook->append_page(*open_session_vbox, _("Open Session"));
   m_notebook->pages().back().set_tab_label_packing(false, true, Gtk::PACK_START);
   get_vbox()->set_homogeneous(false);
   get_vbox()->set_spacing(0);
   get_vbox()->pack_start(*m_notebook, Gtk::PACK_SHRINK, 0);
   set_title(_("Create New Session"));
   //set_modal(false);
   //property_window_position().set_value(Gtk::WIN_POS_NONE);
   set_resizable(true);
   //property_destroy_with_parent().set_value(false);
   set_has_separator(true);
   add_button(Gtk::Stock::HELP, Gtk::RESPONSE_HELP);
   add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
   add_button(Gtk::Stock::CLEAR, Gtk::RESPONSE_NONE);
   add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
   show_all_children();
   
	if (m_treeview) {
		recent_model = Gtk::TreeStore::create (recent_columns);
		m_treeview->set_model (recent_model);
		m_treeview->append_column (_("Recent Sessions"), recent_columns.visible_name);
		m_treeview->set_headers_visible (false);
		m_treeview->get_selection()->set_mode (Gtk::SELECTION_SINGLE);

	}

	std::string path = ARDOUR::get_user_ardour_path() + X_("templates/");
	if (path == Glib::ustring()) {
	  path = ARDOUR::get_system_data_path() + X_("templates/");
	}
	if (path != Glib::ustring()) {
	  m_template->set_current_folder (path);
	}
	m_template->set_show_hidden (true);
	set_response_sensitive (Gtk::RESPONSE_OK, false);
	set_response_sensitive (0, false);
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
	m_template->signal_selection_changed ().connect (mem_fun (*this, &NewSessionDialog::template_chosen));
	m_name->grab_focus();
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
	m_new_session_dialog->set_response_sensitive (Gtk::RESPONSE_OK, false);
	
}

bool
NewSessionDialog::entry_key_release (GdkEventKey* ev)
{
        if (m_name->get_text() != "") {
	        set_response_sensitive (Gtk::RESPONSE_OK, true);
	} else {
	        set_response_sensitive (Gtk::RESPONSE_OK, false);
	}
	return true;
}

void
NewSessionDialog::notebook_page_changed (GtkNotebookPage* np, uint pagenum)
{
        if (pagenum == 1) {
	       // m_okbutton->set_label(_("Open"));
			//m_okbutton->set_image (*(new Gtk::Image (Gtk::Stock::OPEN, Gtk::ICON_SIZE_BUTTON)));
			if (m_treeview->get_selection()->count_selected_rows() == 0) {
		        set_response_sensitive (Gtk::RESPONSE_OK, false);
		} else {
		        set_response_sensitive (Gtk::RESPONSE_OK, true);
		}
	} else {
	       // m_okbutton->set_label(_("New"));
	       // m_okbutton->set_image (*(new Gtk::Image (Gtk::Stock::NEW, Gtk::ICON_SIZE_BUTTON)));
		if (m_name->get_text() == "") {
		       set_response_sensitive (Gtk::RESPONSE_OK, false);
		} else {
		        set_response_sensitive (Gtk::RESPONSE_OK, true);
		}
	}
}

void
NewSessionDialog::treeview_selection_changed ()
{
  if (m_treeview->get_selection()->count_selected_rows() == 0) {
          if (!m_open_filechooser->get_filename().empty()) {
	          set_response_sensitive (Gtk::RESPONSE_OK, true);
	  } else {
	          set_response_sensitive (Gtk::RESPONSE_OK, false);
	  }
  } else {
          set_response_sensitive (Gtk::RESPONSE_OK, true);
  }
}

void
NewSessionDialog::file_chosen ()
{
        m_treeview->get_selection()->unselect_all();
  
	if (m_treeview->get_selection()->count_selected_rows() == 0) {
	        set_response_sensitive (Gtk::RESPONSE_OK, true);
	}
}

void
NewSessionDialog::template_chosen ()
{
  if (m_template->get_filename() != "" ) {;
    m_new_session_dialog->set_response_sensitive (0, true);
  } else {
    m_new_session_dialog->set_response_sensitive (0, false);
  }

}

void
NewSessionDialog::recent_row_activated (const Gtk::TreePath& path, Gtk::TreeViewColumn* col)
{
        response (Gtk::RESPONSE_YES);
}

void
NewSessionDialog::reset_template()
{
  m_template->set_filename("");
}

void
NewSessionDialog::reset_recent()
{
	        /* Shamelessly ripped from ardour_ui.cc */
	        std::vector<string *> *sessions;
		std::vector<string *>::iterator i;
		RecentSessionsSorter cmp;
		
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

void
NewSessionDialog::reset()
{
	reset_name();
	reset_template();
}
