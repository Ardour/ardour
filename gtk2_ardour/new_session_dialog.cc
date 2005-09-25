/*
    Copyright (C) 1999-2002 Paul Davis 

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

#include <list>
#include <string>
#include <ardour/session.h>
#include <ardour/audioengine.h>

#include "prompter.h"
#include "new_session_dialog.h"

using namespace Gtkmm2ext;
using namespace Gtk;
using namespace ARDOUR;

#include "i18n.h"

extern std::vector<string> channel_combo_strings;

NewSessionDialog::NewSessionDialog (ARDOUR::AudioEngine& engine, bool startup, string given_path)
	: ArdourDialog ("new session dialog"),
	  file_selector (_("Session name:"), _("Create")),
	  use_control_button (_("use control outs")),
	  use_master_button (_("use master outs")),
	  connect_to_physical_inputs_button (_("automatically connect track inputs to physical ports")),
	  connect_to_master_button (_("automatically connect track outputs to master outs")),
	  connect_to_physical_outputs_button (_("automatically connect track outputs to physical ports")),
	  manual_connect_outputs_button (_("manually connect track outputs")),
	  in_count_adjustment (2, 1, 1000, 1, 2),
	  out_count_adjustment (2, 1, 1000, 1, 2),
	  output_label (_("Output Connections")),
	  input_label (_("Input Connections")),
	  expansion_button (_("Advanced...")),
	  out_table (2, 2),
	  show_again (_("show again")),
	  in_count_spinner (in_count_adjustment),
	  out_count_spinner (out_count_adjustment),
	  in_count_label (_("Hardware Inputs: use")),
	  out_count_label (_("Hardware Outputs: use"))
	
{
	using namespace Notebook_Helpers;

	set_name ("NewSessionDialog");
	set_title (_("new session setup"));
	set_wmclass (_("ardour_new_session"), "Ardour");
	set_position (Gtk::WIN_POS_MOUSE);
	set_keyboard_input (true);
	set_policy (false, true, false);
	set_modal (true);

	/* sample rate */

	sr_label1.set_text (compose 
			   (_("This session will playback and record at %1 Hz"),
			    engine.frame_rate()));
	sr_label2.set_text (_("This rate is set by JACK and cannot be changed.\n"
			      "If you want to use a different sample rate\n"
			      "please exit and restart JACK"));
	sr_box.set_spacing (12);
	sr_box.set_border_width (12);
	sr_box.pack_start (sr_label1, false, false);
	sr_box.pack_start (sr_label2, false, false);
	sr_frame.add (sr_box);

	/* input */

	connect_to_physical_inputs_button.set_active (true);
	connect_to_physical_inputs_button.set_name ("NewSessionDialogButton");
	
	HBox* input_limit_box = manage (new HBox);
	input_limit_box->set_spacing (7);
	input_limit_box->pack_start (in_count_label, false, false);
	input_limit_box->pack_start (in_count_spinner, false, false);

	input_label.set_alignment (0.1, 0.5);
	input_vbox.pack_start (input_label, false, false, 7);
	input_vbox.pack_start (connect_to_physical_inputs_button, false, false);

	if (engine.n_physical_inputs() > 2) {
		input_vbox.pack_start (*input_limit_box, false, false);
	}

	/* output */

	use_master_button.set_active (true);
	use_master_button.set_name ("NewSessionDialogButton");
	
	connect_to_physical_outputs_button.set_group (connect_to_master_button.group());
	manual_connect_outputs_button.set_group (connect_to_master_button.group());
	connect_to_master_button.set_active (true);
	
	connect_to_physical_outputs_button.set_name ("NewSessionDialogButton");
	manual_connect_outputs_button.set_name ("NewSessionDialogButton");
	connect_to_master_button.set_name ("NewSessionDialogButton");
	use_control_button.set_name ("NewSessionDialogButton");
	
	out_count_adjustment.set_value (engine.n_physical_outputs());
	in_count_adjustment.set_value (engine.n_physical_inputs());

	control_out_channel_combo.set_popdown_strings (channel_combo_strings);
	control_out_channel_combo.set_name (X_("NewSessionChannelCombo"));
	control_out_channel_combo.get_entry()->set_name (X_("NewSessionChannelCombo"));
	control_out_channel_combo.get_popwin()->set_name (X_("NewSessionChannelCombo"));
	// use stereo as default
	control_out_channel_combo.get_list()->select_item (1);

	master_out_channel_combo.set_popdown_strings (channel_combo_strings);
	master_out_channel_combo.set_name (X_("NewSessionChannelCombo"));
	master_out_channel_combo.get_entry()->set_name (X_("NewSessionChannelCombo"));
	master_out_channel_combo.get_popwin()->set_name (X_("NewSessionChannelCombo"));
	// use stereo as default
	master_out_channel_combo.get_list()->select_item (1);

	
	out_table.set_col_spacings (7);
	out_table.set_row_spacings (7);
	if (engine.n_physical_outputs() > 2) {
		out_table.attach (out_count_label, 0, 1, 0, 1, 0, 0);	
		out_table.attach (out_count_spinner, 1, 2, 0, 1, 0, 0);
	}
	out_table.attach (use_control_button, 0, 1, 1, 2, 0, 0);
	out_table.attach (control_out_channel_combo, 1, 2, 1, 2, 0, 0);
	out_table.attach (use_master_button, 0, 1, 2, 3, 0, 0);
	out_table.attach (master_out_channel_combo, 1, 2, 2, 3, 0, 0);
	
	output_label.set_alignment (0.1, 0.5);
	output_vbox.pack_start (output_label, true, true, 7);
	output_vbox.pack_start (out_table, false, false, 5);
	output_vbox.pack_start (connect_to_master_button, false);
	output_vbox.pack_start (connect_to_physical_outputs_button, false);
	output_vbox.pack_start (manual_connect_outputs_button, false);
	
	input_hbox.pack_start (input_vbox, false, false);
	output_hbox.pack_start (output_vbox, false, false);

	VBox* template_vbox = manage (new VBox);
	Label* template_label = manage (new Label (_("Session template")));
	
	template_label->set_alignment (0.1, 0.5);
	template_vbox->pack_start (*template_label, true, true, 7);
	template_vbox->pack_start (template_combo, false, false);

	io_box.set_border_width (12);
	io_box.set_spacing (7);
	io_box.pack_start (*template_vbox);

	io_box.pack_start (input_hbox);
	io_box.pack_start (output_hbox);

	reset_templates();

	option_hbox.set_spacing (7);
	option_hbox.pack_start (io_box);

	fsbox.set_border_width (12);
	fsbox.set_spacing (7);
	fsbox.pack_start (file_selector.table, false, false);

	notebook.pages().push_back (TabElem (fsbox, _("Location")));
	notebook.pages().push_back (TabElem (option_hbox, _("Configuration")));

	if (startup) {
		show_again.set_active(true);
		show_again.toggled.connect (mem_fun(*this, &NewSessionDialog::show_again_toggled));
		file_selector.button_box.pack_end(show_again, false, false);
	}

	main_vbox.set_border_width (12);
	main_vbox.set_border_width (12);
	main_vbox.set_spacing (7);
	main_vbox.pack_start (sr_frame, false, false);
	main_vbox.pack_start (notebook, false, false);
	main_vbox.pack_start (file_selector.button_box, false, false);
	
	add (main_vbox);

//	template_selector.shift_made.connect (
//		mem_fun(*this, &NewSessionDialog::mix_template_shift));
//	template_selector.control_made.connect (
//		mem_fun(*this, &NewSessionDialog::mix_template_control));

	file_selector.cancel_button.signal_clicked().connect (bind (mem_fun(*this, &ArdourDialog::stop), -1));
	file_selector.op_button.signal_clicked().connect (bind (mem_fun(*this, &ArdourDialog::stop), 0));
	file_selector.Expanded.connect (mem_fun(*this, &NewSessionDialog::file_selector_expansion));

	delete_event.connect (mem_fun(*this, &ArdourDialog::wm_close_event));
	show.connect (mem_fun(*this, &NewSessionDialog::fixup_at_show));

	file_selector.entry_label.set_name ("NewSessionMainLabel");
	file_selector.where_label.set_name ("NewSessionMainLabel");
	template_label->set_name ("NewSessionIOLabel");
	input_label.set_name ("NewSessionIOLabel");
	output_label.set_name ("NewSessionIOLabel");
	sr_label1.set_name ("NewSessionSR1Label");
	sr_label2.set_name ("NewSessionSR2Label");

	if (given_path.empty()) {
		Session::FavoriteDirs favs;
		Session::read_favorite_dirs (favs);
		file_selector.set_favorites (favs);
	} else {
		file_selector.set_path (given_path, true);
		notebook.set_page (-1);
		notebook.show.connect (bind (mem_fun (notebook, &Notebook::set_page), -1));
	}
 
	set_default_size(531, 358);
}

void
NewSessionDialog::file_selector_expansion (bool expanded)
{
	if (expanded) {
		fsbox.pack_start (file_selector.expansion_vbox);
		fsbox.reorder_child (file_selector.expansion_vbox, 2);
	} else {
		fsbox.remove (file_selector.expansion_vbox);
	}
}

void
NewSessionDialog::fixup_at_show ()
{
//	if (template_selector.clist().rows().size() == 0) {
//		use_template_button.set_sensitive (false);
//	}

	Session::FavoriteDirs favs;
	Session::read_favorite_dirs (favs);
	file_selector.set_favorites (favs);

	file_selector.entry.grab_focus ();
}

void
NewSessionDialog::_mix_template_refiller (CList &clist, void *arg)

{
	((NewSessionDialog*) arg)->mix_template_refiller (clist);
}

void
NewSessionDialog::mix_template_refiller (CList &clist)
{
	const gchar *rowdata[2];
	list<string> templates;
	list<string>::iterator i;
	
	Session::get_template_list(templates);
	
	rowdata[0] = _("blank");
	clist.insert_row (0, rowdata);

	guint row;
	for (row=1, i=templates.begin(); i != templates.end(); ++row, ++i) {
		rowdata[0] = (*i).c_str();
		clist.insert_row (row, rowdata);
	}
}

void
NewSessionDialog::mix_template_shift (Gtkmm2ext::Selector* selector, Gtkmm2ext::SelectionResult* res)
{
	if (res && res->text){
		Session::delete_template(*res->text);
		// template_selector.rescan();
	}
}

void
NewSessionDialog::mix_template_control (Gtkmm2ext::Selector* selector, Gtkmm2ext::SelectionResult* res)
{
#if 0
	if (res && res->text) {
		ArdourPrompter prompter (true);
		prompter.set_prompt(_("Name for mix template:"));

		string old_name = *(res->text);
		prompter.set_initial_text (old_name);
		prompter.done.connect (Gtk::Main::quit.slot());
		prompter.show_all();
		
		Gtk::Main::run();
		
		if (prompter.status == Gtkmm2ext::Prompter::entered) {
			string name;

			prompter.get_result (name);

			if (name.length() && name != old_name) {
				Session::rename_template(old_name, name);
				template_selector.rescan();
			}
		}
	}
#endif
}

void
NewSessionDialog::show_again_toggled ()
{
	Config->set_no_new_session_dialog(!show_again.get_active());
	Config->save_state();
}

void
NewSessionDialog::reset_templates ()
{
	templates.clear ();
	templates.push_back (_("No template - create tracks/busses manually"));
	Session::get_template_list (templates);
	template_combo.set_popdown_strings (templates);
}

string
NewSessionDialog::get_template_name()
{
	string str = template_combo.get_entry()->get_text();
	if (str.substr (0, 11) == _("No template")) {
		return "";
	} else {
		return str;
	}
}
