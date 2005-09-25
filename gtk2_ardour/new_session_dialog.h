#ifndef __gtk_ardour_new_session_dialog_h__
#define __gtk_ardour_new_session_dialog_h__

#include <gtk--/adjustment.h>
#include <gtk--/radiobutton.h>
#include <gtk--/frame.h>
#include <gtk--/box.h>
#include <gtk--/checkbutton.h>

namespace Gtk {
	class CList;
}

namespace ARDOUR {
	class AudioEngine;
}

#include <gtkmmext/click_box.h>
#include <gtkmmext/selector.h>
#include <gtkmmext/newsavedialog.h>
#include "ardour_dialog.h"

class NewSessionDialog : public ArdourDialog
{
  public:
	NewSessionDialog (ARDOUR::AudioEngine&, bool startup, std::string path);
	
	Gtkmmext::NewSaveDialog file_selector;
	Gtk::Combo control_out_channel_combo;
	Gtk::Combo master_out_channel_combo;
	Gtk::CheckButton use_control_button;
	Gtk::CheckButton use_master_button;
	Gtk::CheckButton connect_to_physical_inputs_button;

	Gtk::RadioButton connect_to_master_button;
	Gtk::RadioButton connect_to_physical_outputs_button;
	Gtk::RadioButton manual_connect_outputs_button;

	Gtk::VBox input_vbox;
	Gtk::VBox manual_vbox;
	Gtk::VBox output_vbox;
	Gtk::VBox vbox;

	Gtk::Adjustment in_count_adjustment;
	Gtk::Adjustment out_count_adjustment;

	string get_template_name ();

  private:
	Gtk::Notebook notebook;
	Gtk::VBox     main_vbox;
	Gtk::VBox     fsbox;

	Gtk::Frame control_out_config_frame;
	Gtk::Frame master_out_config_frame;
	Gtk::Label output_label;
	Gtk::Label input_label;
	Gtk::Frame sr_frame;
	Gtk::Frame template_frame;
	Gtk::Frame manual_frame;
	Gtk::HBox control_hbox;
	Gtk::HBox master_hbox;
	Gtk::Table io_table;
	Gtk::VBox template_box;
	Gtk::HBox output_hbox;
	Gtk::HBox input_hbox;
	Gtk::HBox option_hbox;
	Gtk::VBox io_box;
	Gtk::Label sr_label2;
	Gtk::Label sr_label1;
	Gtk::VBox  sr_box;
	Gtk::Button expansion_button;
	Gtk::Table out_table;
	Gtk::CheckButton show_again;
	Gtk::Combo   template_combo;
	list<string> templates;
	Gtk::SpinButton in_count_spinner;
	Gtk::SpinButton out_count_spinner;
	Gtk::Label      in_count_label;
	Gtk::Label      out_count_label;

	void reset_templates ();
	
	static void _mix_template_refiller (Gtk::CList &clist, void *);
	void mix_template_refiller (Gtk::CList &clist);

	void mix_template_shift (Gtkmmext::Selector *, Gtkmmext::SelectionResult*);
	void mix_template_control (Gtkmmext::Selector *, Gtkmmext::SelectionResult*);

	void fixup_at_realize ();
	void fixup_at_show ();
	void toggle_expansion ();
	void file_selector_expansion (bool);

	void show_again_toggled ();
};

#endif // __gtk_ardour_new_session_dialog_h__ */
