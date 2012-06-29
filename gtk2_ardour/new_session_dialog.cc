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

*/

#include <pbd/error.h>

#include <ardour/recent_sessions.h>
#include <ardour/session.h>
#include <ardour/profile.h>

#include <gtkmm/entry.h>
#include <gtkmm/filechooserbutton.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/filefilter.h>
#include <gtkmm/stock.h>
#include <gdkmm/cursor.h>

#include <gtkmm2ext/window_title.h>

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;

#include "opts.h"
#include "utils.h"
#include "i18n.h"
#include "new_session_dialog.h"

NewSessionDialog::NewSessionDialog()
	: ArdourDialog ("session control")
{
	in_destructor = false;
	session_name_label = new Gtk::Label(_("Name :"));
	last_name_page = NewPage;
	m_name = new Gtk::Entry();
	m_name->set_text(ARDOUR_COMMAND_LINE::session_name);

	chan_count_label_1 = new Gtk::Label(_("channels"));
	chan_count_label_2 = new Gtk::Label(_("channels"));
	chan_count_label_3 = new Gtk::Label(_("channels"));
	chan_count_label_4 = new Gtk::Label(_("channels"));

	chan_count_label_1->set_alignment(0,0.5);
	chan_count_label_1->set_padding(0,0);
	chan_count_label_1->set_line_wrap(false);

	chan_count_label_2->set_alignment(0,0.5);
	chan_count_label_2->set_padding(0,0);
	chan_count_label_2->set_line_wrap(false);

	chan_count_label_3->set_alignment(0,0.5);
	chan_count_label_3->set_padding(0,0);
	chan_count_label_3->set_line_wrap(false);

	chan_count_label_4->set_alignment(0,0.5);
	chan_count_label_4->set_padding(0,0);
	chan_count_label_4->set_line_wrap(false);

	bus_label = new Gtk::Label(_("<b>Busses</b>"));
	input_label = new Gtk::Label(_("<b>Inputs</b>"));
	output_label = new Gtk::Label(_("<b>Outputs</b>"));

	session_location_label = new Gtk::Label(_("Create Folder In :"));
	m_folder = new Gtk::FileChooserButton(Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
	session_template_label = new Gtk::Label(_("Template :"));
	m_template = new Gtk::FileChooserButton();
	m_create_control_bus = new Gtk::CheckButton(_("Create Monitor Bus"));
	
	Gtk::Adjustment *m_control_bus_channel_count_adj = Gtk::manage(new Gtk::Adjustment(2, 0, 100, 1, 10));
	m_control_bus_channel_count = new Gtk::SpinButton(*m_control_bus_channel_count_adj, 1, 0);
	
	Gtk::Adjustment *m_master_bus_channel_count_adj = Gtk::manage(new Gtk::Adjustment(2, 0, 100, 1, 10));
	m_master_bus_channel_count = new Gtk::SpinButton(*m_master_bus_channel_count_adj, 1, 0);
	m_create_master_bus = new Gtk::CheckButton(_("Create Master Bus"));
	advanced_table = new Gtk::Table(2, 2, true);
	m_connect_inputs = new Gtk::CheckButton(_("Automatically Connect to Physical Inputs"));
	m_limit_input_ports = new Gtk::CheckButton(_("Use only"));
	
	Gtk::Adjustment *m_input_limit_count_adj = Gtk::manage(new Gtk::Adjustment(1, 0, 100, 1, 10));
	m_input_limit_count = new Gtk::SpinButton(*m_input_limit_count_adj, 1, 0);
	input_port_limit_hbox = new Gtk::HBox(false, 0);
	input_port_vbox = new Gtk::VBox(false, 0);
	input_table = new Gtk::Table(2, 2, false);

	bus_frame = new Gtk::Frame();
	bus_table = new Gtk::Table (2, 3, false);
	
	input_frame = new Gtk::Frame();
	m_connect_outputs = new Gtk::CheckButton(_("Automatically Connect Outputs"));
	m_limit_output_ports = new Gtk::CheckButton(_("Use only"));
	
	Gtk::Adjustment *m_output_limit_count_adj = Gtk::manage(new Gtk::Adjustment(1, 0, 100, 1, 10));
	m_output_limit_count = new Gtk::SpinButton(*m_output_limit_count_adj, 1, 0);
	output_port_limit_hbox = new Gtk::HBox(false, 0);
	output_port_vbox = new Gtk::VBox(false, 0);
	
	Gtk::RadioButton::Group _RadioBGroup_m_connect_outputs_to_master;
	m_connect_outputs_to_master = new Gtk::RadioButton(_RadioBGroup_m_connect_outputs_to_master, _("... to Master Bus"));
	m_connect_outputs_to_physical = new Gtk::RadioButton(_RadioBGroup_m_connect_outputs_to_master, _("... to Physical Outputs"));
	output_conn_vbox = new Gtk::VBox(false, 0);
	output_vbox = new Gtk::VBox(false, 0);

	output_frame = new Gtk::Frame();
	advanced_vbox = new Gtk::VBox(false, 0);
	advanced_label = new Gtk::Label(_("Advanced Options"));
	advanced_expander = new Gtk::Expander();
	new_session_table = new Gtk::Table(2, 2, false);
	m_open_filechooser = new Gtk::FileChooserButton();
	open_session_hbox = new Gtk::HBox(false, 0);
	m_treeview = new Gtk::TreeView();
	recent_scrolledwindow = new Gtk::ScrolledWindow();

	recent_sesion_label = new Gtk::Label(_("Recent:"));
	recent_frame = new Gtk::Frame();
	open_session_vbox = new Gtk::VBox(false, 0);
	m_notebook = new Gtk::Notebook();
	session_name_label->set_alignment(0, 0.5);
	session_name_label->set_padding(6,0);
	session_name_label->set_line_wrap(false);
	session_name_label->set_selectable(false);
	m_name->set_editable(true);
	m_name->set_max_length(0);
	m_name->set_has_frame(true);
	m_name->set_activates_default(true);
	m_name->set_width_chars (40);
	session_location_label->set_alignment(0,0.5);
	session_location_label->set_padding(6,0);
	session_location_label->set_line_wrap(false);
	session_location_label->set_selectable(false);
	session_template_label->set_alignment(0,0.5);
	session_template_label->set_padding(6,0);
	session_template_label->set_line_wrap(false);
	session_template_label->set_selectable(false);
	m_create_control_bus->set_flags(Gtk::CAN_FOCUS);
	m_create_control_bus->set_relief(Gtk::RELIEF_NORMAL);
	m_create_control_bus->set_mode(true);
	m_create_control_bus->set_active(false);
	m_create_control_bus->set_border_width(0);
	m_control_bus_channel_count->set_flags(Gtk::CAN_FOCUS);
	m_control_bus_channel_count->set_update_policy(Gtk::UPDATE_ALWAYS);
	m_control_bus_channel_count->set_numeric(true);
	m_control_bus_channel_count->set_digits(0);
	m_control_bus_channel_count->set_wrap(false);
	m_control_bus_channel_count->set_sensitive(false);
	m_master_bus_channel_count->set_flags(Gtk::CAN_FOCUS);
	m_master_bus_channel_count->set_update_policy(Gtk::UPDATE_ALWAYS);
	m_master_bus_channel_count->set_numeric(true);
	m_master_bus_channel_count->set_digits(0);
	m_master_bus_channel_count->set_wrap(false);
	open_session_file_label = new Gtk::Label(_("Browse:"));
	open_session_file_label->set_alignment(0, 0.5);
	m_create_master_bus->set_flags(Gtk::CAN_FOCUS);
	m_create_master_bus->set_relief(Gtk::RELIEF_NORMAL);
	m_create_master_bus->set_mode(true);
	m_create_master_bus->set_active(true);
	m_create_master_bus->set_border_width(0);
	advanced_table->set_row_spacings(0);
	advanced_table->set_col_spacings(0);
	
	m_connect_inputs->set_flags(Gtk::CAN_FOCUS);
	m_connect_inputs->set_relief(Gtk::RELIEF_NORMAL);
	m_connect_inputs->set_mode(true);
	m_connect_inputs->set_active(true);
	m_connect_inputs->set_border_width(0);

	m_limit_input_ports->set_flags(Gtk::CAN_FOCUS);
	m_limit_input_ports->set_relief(Gtk::RELIEF_NORMAL);
	m_limit_input_ports->set_mode(true);
	m_limit_input_ports->set_sensitive(true);
	m_limit_input_ports->set_border_width(0);
	m_input_limit_count->set_flags(Gtk::CAN_FOCUS);
	m_input_limit_count->set_update_policy(Gtk::UPDATE_ALWAYS);
	m_input_limit_count->set_numeric(true);
	m_input_limit_count->set_digits(0);
	m_input_limit_count->set_wrap(false);
	m_input_limit_count->set_sensitive(false);

	bus_hbox = new Gtk::HBox (false, 0);
	bus_hbox->pack_start (*bus_table, Gtk::PACK_SHRINK, 18);

	bus_label->set_alignment(0, 0.5);
	bus_label->set_padding(0,0);
	bus_label->set_line_wrap(false);
	bus_label->set_selectable(false);
	bus_label->set_use_markup(true);
	bus_frame->set_shadow_type(Gtk::SHADOW_NONE);
	bus_frame->set_label_align(0,0.5);
	bus_frame->add(*bus_hbox);
	bus_frame->set_label_widget(*bus_label);
	
	bus_table->set_row_spacings (0);
	bus_table->set_col_spacings (0);
	bus_table->attach (*m_create_master_bus, 0, 1, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 0, 0);
	bus_table->attach (*m_master_bus_channel_count, 1, 2, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 0, 0);
	bus_table->attach (*chan_count_label_1, 2, 3, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 6, 0);
	bus_table->attach (*m_create_control_bus, 0, 1, 1, 2, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 0, 0);
	bus_table->attach (*m_control_bus_channel_count, 1, 2, 1, 2, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 0, 0);
	bus_table->attach (*chan_count_label_2, 2, 3, 1, 2, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 6, 0);

	input_port_limit_hbox->pack_start(*m_limit_input_ports, Gtk::PACK_SHRINK, 6);
	input_port_limit_hbox->pack_start(*m_input_limit_count, Gtk::PACK_SHRINK, 0);
	input_port_limit_hbox->pack_start(*chan_count_label_3, Gtk::PACK_SHRINK, 6);
	input_port_vbox->pack_start(*m_connect_inputs, Gtk::PACK_SHRINK, 0);
	input_port_vbox->pack_start(*input_port_limit_hbox, Gtk::PACK_EXPAND_PADDING, 0);
	input_table->set_row_spacings(0);
	input_table->set_col_spacings(0);
	input_table->attach(*input_port_vbox, 0, 1, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 6, 6);

	input_hbox = new Gtk::HBox (false, 0);
	input_hbox->pack_start (*input_table, Gtk::PACK_SHRINK, 18);

	input_label->set_alignment(0, 0.5);
	input_label->set_padding(0,0);
	input_label->set_line_wrap(false);
	input_label->set_selectable(false);
	input_label->set_use_markup(true);
	input_frame->set_shadow_type(Gtk::SHADOW_NONE);
	input_frame->set_label_align(0,0.5);
	input_frame->add(*input_hbox);
	input_frame->set_label_widget(*input_label);

	m_connect_outputs->set_flags(Gtk::CAN_FOCUS);
	m_connect_outputs->set_relief(Gtk::RELIEF_NORMAL);
	m_connect_outputs->set_mode(true);
	m_connect_outputs->set_active(true);
	m_connect_outputs->set_border_width(0);
	m_limit_output_ports->set_flags(Gtk::CAN_FOCUS);
	m_limit_output_ports->set_relief(Gtk::RELIEF_NORMAL);
	m_limit_output_ports->set_mode(true);
	m_limit_output_ports->set_sensitive(true);
	m_limit_output_ports->set_border_width(0);
	m_output_limit_count->set_flags(Gtk::CAN_FOCUS);
	m_output_limit_count->set_update_policy(Gtk::UPDATE_ALWAYS);
	m_output_limit_count->set_numeric(false);
	m_output_limit_count->set_digits(0);
	m_output_limit_count->set_wrap(false);
	m_output_limit_count->set_sensitive(false);
	output_port_limit_hbox->pack_start(*m_limit_output_ports, Gtk::PACK_SHRINK, 6);
	output_port_limit_hbox->pack_start(*m_output_limit_count, Gtk::PACK_SHRINK, 0);
	output_port_limit_hbox->pack_start(*chan_count_label_4, Gtk::PACK_SHRINK, 6);
	m_connect_outputs_to_master->set_flags(Gtk::CAN_FOCUS);
	m_connect_outputs_to_master->set_relief(Gtk::RELIEF_NORMAL);
	m_connect_outputs_to_master->set_mode(true);
	m_connect_outputs_to_master->set_active(false);
	m_connect_outputs_to_master->set_border_width(0);
	m_connect_outputs_to_physical->set_flags(Gtk::CAN_FOCUS);
	m_connect_outputs_to_physical->set_relief(Gtk::RELIEF_NORMAL);
	m_connect_outputs_to_physical->set_mode(true);
	m_connect_outputs_to_physical->set_active(false);
	m_connect_outputs_to_physical->set_border_width(0);
	output_conn_vbox->pack_start(*m_connect_outputs, Gtk::PACK_SHRINK, 0);
	output_conn_vbox->pack_start(*m_connect_outputs_to_master, Gtk::PACK_SHRINK, 0);
	output_conn_vbox->pack_start(*m_connect_outputs_to_physical, Gtk::PACK_SHRINK, 0);
	output_vbox->set_border_width(6);

	output_port_vbox->pack_start(*output_port_limit_hbox, Gtk::PACK_SHRINK, 0);

	output_vbox->pack_start(*output_conn_vbox);
	output_vbox->pack_start(*output_port_vbox);

	output_label->set_alignment(0, 0.5);
	output_label->set_padding(0,0);
	output_label->set_line_wrap(false);
	output_label->set_selectable(false);
	output_label->set_use_markup(true);
	output_frame->set_shadow_type(Gtk::SHADOW_NONE);
	output_frame->set_label_align(0,0.5);

	output_hbox = new Gtk::HBox (false, 0);
	output_hbox->pack_start (*output_vbox, Gtk::PACK_SHRINK, 18);

	output_frame->add(*output_hbox);
	output_frame->set_label_widget(*output_label);

	advanced_vbox->pack_start(*advanced_table, Gtk::PACK_SHRINK, 0);
	advanced_vbox->pack_start(*bus_frame, Gtk::PACK_SHRINK, 6);
	advanced_vbox->pack_start(*input_frame, Gtk::PACK_SHRINK, 6);
	advanced_vbox->pack_start(*output_frame, Gtk::PACK_SHRINK, 0);
	advanced_label->set_padding(0,0);
	advanced_label->set_line_wrap(false);
	advanced_label->set_selectable(false);
	advanced_label->set_alignment(0, 0.5);
	advanced_expander->set_flags(Gtk::CAN_FOCUS);
	advanced_expander->set_border_width(0);
	advanced_expander->set_expanded(false);
	advanced_expander->set_spacing(0);
	advanced_expander->add(*advanced_vbox);
	advanced_expander->set_label_widget(*advanced_label);
	new_session_table->set_border_width(12);
	new_session_table->set_row_spacings(6);
	new_session_table->set_col_spacings(0);
	new_session_table->attach(*session_name_label, 0, 1, 0, 1, Gtk::FILL, Gtk::FILL, 0, 0);
	new_session_table->attach(*m_name, 1, 2, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::FILL, 0, 0);
	new_session_table->attach(*session_location_label, 0, 1, 1, 2, Gtk::FILL, Gtk::FILL, 0, 0);
	new_session_table->attach(*m_folder, 1, 2, 1, 2, Gtk::EXPAND|Gtk::FILL, Gtk::FILL, 0, 0);
	new_session_table->attach(*session_template_label, 0, 1, 2, 3, Gtk::FILL, Gtk::FILL, 0, 0);
	new_session_table->attach(*m_template, 1, 2, 2, 3, Gtk::EXPAND|Gtk::FILL, Gtk::FILL, 0, 0);

	if (!ARDOUR::Profile->get_sae()) {
		new_session_table->attach(*advanced_expander, 0, 2, 3, 4, Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 0, 6);
	}

	open_session_hbox->pack_start(*open_session_file_label, false, false, 12);
	open_session_hbox->pack_start(*m_open_filechooser, true, true, 12);
	m_treeview->set_flags(Gtk::CAN_FOCUS);
	m_treeview->set_headers_visible(true);
	m_treeview->set_rules_hint(false);
	m_treeview->set_reorderable(false);
	m_treeview->set_enable_search(true);
	m_treeview->set_fixed_height_mode(false);
	m_treeview->set_hover_selection(false);
	m_treeview->set_size_request(-1, 150);
	recent_scrolledwindow->set_flags(Gtk::CAN_FOCUS);
	recent_scrolledwindow->set_border_width(6);
	recent_scrolledwindow->set_shadow_type(Gtk::SHADOW_IN);
	recent_scrolledwindow->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	recent_scrolledwindow->property_window_placement().set_value(Gtk::CORNER_TOP_LEFT);
	recent_scrolledwindow->add(*m_treeview);

	recent_sesion_label->set_padding(0,0);
	recent_sesion_label->set_line_wrap(false);
	recent_sesion_label->set_selectable(false);
	recent_frame->set_border_width(12);
	recent_frame->set_shadow_type(Gtk::SHADOW_NONE);
	recent_frame->add(*recent_scrolledwindow);
	recent_frame->set_label_widget(*recent_sesion_label);
	open_session_vbox->pack_start(*recent_frame, Gtk::PACK_EXPAND_WIDGET, 0);
	open_session_vbox->pack_start(*open_session_hbox, Gtk::PACK_SHRINK, 12);

	m_notebook->set_flags(Gtk::CAN_FOCUS);
	m_notebook->set_scrollable(true);
	
	get_vbox()->set_homogeneous(false);
	get_vbox()->set_spacing(0);
	get_vbox()->pack_start(*m_notebook, Gtk::PACK_SHRINK, 0);

	/* 
	   icon setting is done again in the editor (for the whole app),
	   but its all chickens and eggs at this point.
	*/

	list<Glib::RefPtr<Gdk::Pixbuf> > window_icons;
	Glib::RefPtr<Gdk::Pixbuf> icon;

	if ((icon = ::get_icon ("ardour_icon_16px")) != 0) {
		window_icons.push_back (icon);
	}
	if ((icon = ::get_icon ("ardour_icon_22px")) != 0) {
		window_icons.push_back (icon);
	}
	if ((icon = ::get_icon ("ardour_icon_32px")) != 0) {
		window_icons.push_back (icon);
	}
	if ((icon = ::get_icon ("ardour_icon_48px")) != 0) {
		window_icons.push_back (icon);
	}
	if (!window_icons.empty()) {
		set_icon_list (window_icons);
	}

	set_title(_("Session Control"));

	set_position (Gtk::WIN_POS_MOUSE);
	set_resizable(false);
	set_has_separator(false);
	add_button(Gtk::Stock::QUIT, Gtk::RESPONSE_CANCEL);
	add_button(Gtk::Stock::CLEAR, Gtk::RESPONSE_NONE);
	m_okbutton = add_button(Gtk::Stock::NEW, Gtk::RESPONSE_OK);

	recent_model = Gtk::TreeStore::create (recent_columns);
	m_treeview->set_model (recent_model);
	m_treeview->append_column (_("Recent Sessions"), recent_columns.visible_name);
	m_treeview->set_headers_visible (false);
	m_treeview->get_selection()->set_mode (Gtk::SELECTION_SINGLE);

	std::string path = ARDOUR::get_user_ardour_path();
	
	if (path.empty()) {
	        path = ARDOUR::get_system_data_path();
	}

	const char * const template_dir_name = X_("templates");

	//if SYSTEM template folder exists, add it to the file chooser
	const std::string sys_templates_path = ARDOUR::get_system_data_path() + "/" + template_dir_name;

	if (Glib::file_test(sys_templates_path, Glib::FILE_TEST_IS_DIR))
	{
		m_template->add_shortcut_folder(sys_templates_path);
		m_template->set_current_folder (sys_templates_path);
	}

	//if USER template folder exists, add it to the file chooser
	const std::string user_template_path = ARDOUR::get_user_ardour_path() + template_dir_name;
	bool utp_exists = true;

	if (!Glib::file_test(user_template_path, Glib::FILE_TEST_IS_DIR)) {
		if (g_mkdir_with_parents (user_template_path.c_str(), 0755)) {
			utp_exists = false;
		}
	}

	if (utp_exists) {
		m_template->add_shortcut_folder(user_template_path);
		m_template->set_current_folder (user_template_path);
	}

	m_template->set_title(_("select template"));
	Gtk::FileFilter* session_filter = manage (new (Gtk::FileFilter));
	session_filter->add_pattern(X_("*.ardour"));
	session_filter->add_pattern(X_("*.ardour.bak"));
	m_open_filechooser->set_filter (*session_filter);
	m_open_filechooser->set_current_folder(getenv ("HOME"));
	m_open_filechooser->set_title(_("select session file"));

	Gtk::FileFilter* template_filter = manage (new (Gtk::FileFilter));
	template_filter->add_pattern(X_("*.ardour"));
	template_filter->add_pattern(X_("*.ardour.bak"));
	template_filter->add_pattern(X_("*.template"));
	m_template->set_filter (*template_filter);

	m_folder->set_current_folder(getenv ("HOME"));
	m_folder->set_title(_("select directory"));

#ifdef GTKOSX
	m_folder->add_shortcut_folder("/Volumes");
	m_open_filechooser->add_shortcut_folder("/Volumes");
#endif

	on_new_session_page = true;
	m_notebook->set_current_page(0);
	m_notebook->show();
	m_notebook->show_all_children();

	engine_page_session_folder = X_("");
	engine_page_session_name = X_("");

	set_default_response (Gtk::RESPONSE_OK);
	if (!ARDOUR_COMMAND_LINE::session_name.length()) {
		set_response_sensitive (Gtk::RESPONSE_OK, false);
		set_response_sensitive (Gtk::RESPONSE_NONE, false);
	} else {
		set_response_sensitive (Gtk::RESPONSE_OK, true);
		set_response_sensitive (Gtk::RESPONSE_NONE, true);
	}

	///@ connect some signals

	m_connect_inputs->signal_clicked().connect (mem_fun (*this, &NewSessionDialog::connect_inputs_clicked));
	m_connect_outputs->signal_clicked().connect (mem_fun (*this, &NewSessionDialog::connect_outputs_clicked));
	m_limit_input_ports->signal_clicked().connect (mem_fun (*this, &NewSessionDialog::limit_inputs_clicked));
	m_limit_output_ports->signal_clicked().connect (mem_fun (*this, &NewSessionDialog::limit_outputs_clicked));
	m_create_master_bus->signal_clicked().connect (mem_fun (*this, &NewSessionDialog::master_bus_button_clicked));
	m_create_control_bus->signal_clicked().connect (mem_fun (*this, &NewSessionDialog::monitor_bus_button_clicked));
	m_name->signal_changed().connect(mem_fun (*this, &NewSessionDialog::on_new_session_name_entry_changed));
	m_notebook->signal_switch_page().connect (mem_fun (*this, &NewSessionDialog::notebook_page_changed));
	m_treeview->get_selection()->signal_changed().connect (mem_fun (*this, &NewSessionDialog::treeview_selection_changed));
	m_treeview->signal_row_activated().connect (mem_fun (*this, &NewSessionDialog::recent_row_activated));
	m_open_filechooser->signal_selection_changed ().connect (mem_fun (*this, &NewSessionDialog::file_chosen));
	m_template->signal_selection_changed ().connect (mem_fun (*this, &NewSessionDialog::template_chosen));
	
	page_set = Pages (0);
}

NewSessionDialog::~NewSessionDialog()
{
	in_destructor = true;
}

int
NewSessionDialog::run ()
{
	if (!page_set) {
		/* nothing to display */
		return Gtk::RESPONSE_OK;
	}
	if (!(page_set & NewPage) && !(page_set & OpenPage)) {
		set_response_sensitive (Gtk::RESPONSE_OK, true);
	}
	return ArdourDialog::run ();
}

void
NewSessionDialog::set_have_engine (bool yn)
{

	m_notebook->remove_page (engine_control);
	page_set = Pages (page_set & ~EnginePage);
	
	if (!yn) {

		engine_control.discover_servers ();

		if (engine_control.interface_chosen()) {
			m_notebook->append_page (engine_control, _("Audio Setup"));
			m_notebook->show_all_children();
			page_set = Pages (page_set | EnginePage);
		} else {
			m_notebook->prepend_page (engine_control, _("Audio Setup"));
			page_set = Pages (page_set | EnginePage);

			/* no interface ever selected - make it the first and only page */
			if (page_set & NewPage) {
				m_notebook->remove_page (*new_session_table);
				page_set = Pages (page_set & ~NewPage);
			}
			if (page_set & OpenPage) {
				m_notebook->remove_page (*open_session_vbox);
				page_set = Pages (page_set & ~OpenPage);
			}
			m_notebook->show_all_children();
		}
	}
}

void
NewSessionDialog::set_existing_session (bool yn)
{
	if (yn) {

		if (page_set & NewPage) {
			m_notebook->remove_page (*new_session_table);
			page_set = Pages (page_set & ~NewPage);
		}

		if (page_set & OpenPage) {
			m_notebook->remove_page (*open_session_vbox);
			page_set = Pages (page_set & ~OpenPage);
		}

	} else {
		if (!(page_set & NewPage)) {
			m_notebook->append_page(*new_session_table, _("New Session"));
			m_notebook->pages().back().set_tab_label_packing(false, true, Gtk::PACK_START);
			page_set = Pages (page_set | NewPage);
		}
		if (!(page_set & OpenPage)) {
			m_notebook->append_page(*open_session_vbox, _("Open Session"));
			m_notebook->pages().back().set_tab_label_packing(false, true, Gtk::PACK_START);
			page_set = Pages (page_set | OpenPage);
		}

		m_notebook->show_all_children();
	}
}

void
NewSessionDialog::set_session_name (const std::string& name)
{
	m_name->set_text (name);
	engine_page_session_name = name;
}

void
NewSessionDialog::set_session_folder(const std::string& dir)
{
	std::string realdir = dir;

	/* this little tangled mess is a result of 4 things:

	    1) GtkFileChooser vomits when given a non-absolute directory
                   argument to set_current_folder()
            2) canonicalize_file_name() doesn't exist on OS X
	    3) linux man page for realpath() says "do not use this function"
	    4) canonicalize_file_name() & realpath() have entirely
                   different semantics on OS X and Linux when given
		   a non-existent path.
		   
	   as result of all this, we take two distinct pathways through the code.
	*/


#ifdef __APPLE__

	char buf[PATH_MAX];

	if(realpath (dir.c_str(), buf) != 0) {
		if (!Glib::file_test (dir, Glib::FILE_TEST_IS_DIR)) {
			realdir = Glib::path_get_dirname (realdir);
		}
		m_folder->set_current_folder (realdir);
		engine_page_session_folder = realdir;
	}

	
#else 
	char* res;
	if (!Glib::file_test (dir, Glib::FILE_TEST_IS_DIR)) {
		realdir = Glib::path_get_dirname (realdir);
	}

	if ((res = canonicalize_file_name (realdir.c_str())) != 0) {
		m_folder->set_current_folder (res);
		engine_page_session_folder = res;
		free (res);
	}
	
#endif

}

std::string
NewSessionDialog::session_name() const
{
        std::string str = Glib::filename_from_utf8 (m_open_filechooser->get_filename());
	std::string::size_type position = str.find_last_of (G_DIR_SEPARATOR);
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

	switch (which_page()) {
	case NewPage:
	        return Glib::filename_from_utf8(m_name->get_text());

	case EnginePage:
		if (!(page_set & (OpenPage|NewPage))) {
			return engine_page_session_name;
		} else if (last_name_page == NewPage) {
			return Glib::filename_from_utf8(m_name->get_text());
		} else {
			/* relax and use the open page stuff at the end */
		}
		break;

	default:
		break;
	} 

	if (m_treeview->get_selection()->count_selected_rows() == 0) {
		return Glib::filename_from_utf8(str);
	}

	Gtk::TreeModel::iterator i = m_treeview->get_selection()->get_selected();
	return (*i)[recent_columns.visible_name];
}

std::string
NewSessionDialog::session_folder() const
{
        cerr << "Determining session folder, current page = " << which_page() << endl;

	switch (which_page()) {
	case NewPage:
                cerr << "mfolder says " << m_folder->get_current_folder() << endl;
	        return Glib::filename_from_utf8(m_folder->get_current_folder());
		
	case EnginePage:
		if (!(page_set & (OpenPage|NewPage))) {
                        cerr << "engine page session folder says " << engine_page_session_folder << endl;
			return Glib::filename_from_utf8(engine_page_session_folder);
		} else if (last_name_page == NewPage) {
			/* use m_folder since it should be set */
                        cerr << "mfolder2 says " << m_folder->get_current_folder() << endl;
			return Glib::filename_from_utf8(m_folder->get_current_folder());
		} else {
			/* relax and use the open page stuff at the end */
		}
		break;

	default:
		break;
	}
	       
	if (m_treeview->get_selection()->count_selected_rows() == 0) {
                cerr << "open filechooser says " << m_open_filechooser->get_filename() << endl;
		const string filename(Glib::filename_from_utf8(m_open_filechooser->get_filename()));
		return Glib::path_get_dirname(filename);
	}

	Gtk::TreeModel::iterator i = m_treeview->get_selection()->get_selected();
        string x = (*i)[recent_columns.fullpath];
        cerr << "recent says " << x << endl;
	return (*i)[recent_columns.fullpath];
}

bool
NewSessionDialog::use_session_template() const
{
        if(m_template->get_filename().empty() && (which_page() == NewPage)) return false;
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

NewSessionDialog::Pages
NewSessionDialog::which_page () const
{
	int num = m_notebook->get_current_page();

	if (page_set == NewPage) {
		return NewPage;

	} else if (page_set == OpenPage) {
		return OpenPage;

	} else if (page_set == EnginePage) {
		return EnginePage;

	} else if (page_set == (NewPage|OpenPage)) {
		switch (num) {
		case 0:
			return NewPage;
		default:
			return OpenPage;
		}

	} else if (page_set == (NewPage|EnginePage)) {
		if (engine_control.interface_chosen()) {
			switch (num) {
			case 0:
				return NewPage;
			default:
				return EnginePage;
			} 
		} else {
			switch (num) {
			case 0:
				return EnginePage;
			default:
				return NewPage;
			} 
		}

	} else if (page_set == (NewPage|EnginePage|OpenPage)) {
		if (engine_control.interface_chosen()) {
			switch (num) {
			case 0:
				return NewPage;
			case 1:
				return OpenPage;
			default:
				return EnginePage;
			}
		} else {
			switch (num) {
			case 0:
				return EnginePage;
			case 1:
				return NewPage;
			default:
				return OpenPage;
			}
		}

	} else if (page_set == (OpenPage|EnginePage)) {
		if (engine_control.interface_chosen()) {
			switch (num) {
			case 0:
				return OpenPage;
			default:
				return EnginePage;
			}
		} else {
			switch (num) {
			case 0:
				return EnginePage;
			default:
				return OpenPage;
			}
		}
	}

	return NewPage; /* shouldn't get here */
}

void
NewSessionDialog::set_current_page(int page)
{
	return m_notebook->set_current_page (page);
}

void
NewSessionDialog::reset_name()
{
	m_name->set_text("");
	set_response_sensitive (Gtk::RESPONSE_OK, false);
	
}

void
NewSessionDialog::on_new_session_name_entry_changed ()
{
	if (m_name->get_text() != "") {
		set_response_sensitive (Gtk::RESPONSE_OK, true);
		set_response_sensitive (Gtk::RESPONSE_NONE, true);
	} else {
		set_response_sensitive (Gtk::RESPONSE_OK, false);
	}
}

void
NewSessionDialog::notebook_page_changed (GtkNotebookPage* np, uint pagenum)
{
	if (in_destructor) {
		return;
	}

	switch (which_page()) {
	case OpenPage:
		on_new_session_page = false;
		m_okbutton->set_label(_("Open"));
		m_okbutton->set_image (*(manage (new Gtk::Image (Gtk::Stock::OPEN, Gtk::ICON_SIZE_BUTTON))));
		set_response_sensitive (Gtk::RESPONSE_NONE, false);
		if (m_treeview->get_selection()->count_selected_rows() == 0) {
			set_response_sensitive (Gtk::RESPONSE_OK, false);
		} else {
			set_response_sensitive (Gtk::RESPONSE_OK, true);
		}
		last_name_page = OpenPage;
		break;

	case EnginePage:
		on_new_session_page = false;
		if (!engine_control.interface_chosen()) {
			m_okbutton->set_label(_("Start Audio Engine"));
		} else {
			m_okbutton->set_label(_("Start"));
		}
		m_okbutton->set_image (*(manage (new Gtk::Image (Gtk::Stock::OPEN, Gtk::ICON_SIZE_BUTTON))));
		set_response_sensitive (Gtk::RESPONSE_NONE, false);
		set_response_sensitive (Gtk::RESPONSE_OK, true);
		break;

	case NewPage:
		on_new_session_page = true;
		m_okbutton->set_label(_("New"));
		m_okbutton->set_image (*(new Gtk::Image (Gtk::Stock::NEW, Gtk::ICON_SIZE_BUTTON)));
		if (m_name->get_text() == "") {
			set_response_sensitive (Gtk::RESPONSE_OK, false);
			m_name->grab_focus();
		} else {
			set_response_sensitive (Gtk::RESPONSE_OK, true);
		}
		last_name_page = NewPage;
		break;
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
	switch (which_page()) {
      case OpenPage:
         break;
	   case NewPage:
	   case EnginePage:
		   return;
	}

	m_treeview->get_selection()->unselect_all();

	Glib::RefPtr<Gdk::Window> win (get_window());

	if (win) {
		win->set_cursor(Gdk::Cursor(Gdk::WATCH));
	}

	if (!m_open_filechooser->get_filename().empty()) {
	        set_response_sensitive (Gtk::RESPONSE_OK, true);
		response (Gtk::RESPONSE_OK);
	} else {
	        set_response_sensitive (Gtk::RESPONSE_OK, false);
	}
}

void
NewSessionDialog::template_chosen ()
{
	if (m_template->get_filename() != "" ) {;
		set_response_sensitive (Gtk::RESPONSE_NONE, true);
	} else {
		set_response_sensitive (Gtk::RESPONSE_NONE, false);
	}
}

void
NewSessionDialog::recent_row_activated (const Gtk::TreePath& path, Gtk::TreeViewColumn* col)
{
        response (Gtk::RESPONSE_OK);
}

void
NewSessionDialog::connect_inputs_clicked ()
{
        m_limit_input_ports->set_sensitive(m_connect_inputs->get_active());

		if (m_connect_inputs->get_active() && m_limit_input_ports->get_active()) {
	        m_input_limit_count->set_sensitive(true);
		} else {
	        m_input_limit_count->set_sensitive(false);
		}
}

void
NewSessionDialog::connect_outputs_clicked ()
{
        m_limit_output_ports->set_sensitive(m_connect_outputs->get_active());

		if (m_connect_outputs->get_active() && m_limit_output_ports->get_active()) {
	        m_output_limit_count->set_sensitive(true);
		} else {
	        m_output_limit_count->set_sensitive(false);
		}
}

void
NewSessionDialog::limit_inputs_clicked ()
{
        m_input_limit_count->set_sensitive(m_limit_input_ports->get_active());
}

void
NewSessionDialog::limit_outputs_clicked ()
{
        m_output_limit_count->set_sensitive(m_limit_output_ports->get_active());
}

void
NewSessionDialog::master_bus_button_clicked ()
{
        m_master_bus_channel_count->set_sensitive(m_create_master_bus->get_active());
}

void
NewSessionDialog::monitor_bus_button_clicked ()
{
        m_control_bus_channel_count->set_sensitive(m_create_control_bus->get_active());
}

void
NewSessionDialog::reset_template()
{
        m_template->unselect_all ();
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
		
		/* remove any trailing separator */
		
		if (fullpath[fullpath.length()-1] == G_DIR_SEPARATOR) {
			fullpath = fullpath.substr (0, fullpath.length()-1);
		}
	    
		/* check whether session still exists */
		if (!Glib::file_test(fullpath, Glib::FILE_TEST_EXISTS)) {
			/* session doesn't exist */
			continue;
		}		
		
		/* now get available states for this session */
		  
		if ((states = ARDOUR::Session::possible_states (fullpath)) == 0) {
		        /* no state file? */
		        continue;
		}
	    
		Gtk::TreeModel::Row row = *(recent_model->append());
		
		row[recent_columns.visible_name] = Glib::path_get_basename (fullpath);
		row[recent_columns.fullpath] = fullpath;
		
		if (states->size()) {
		    
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
	set_response_sensitive (Gtk::RESPONSE_NONE, false);
}
