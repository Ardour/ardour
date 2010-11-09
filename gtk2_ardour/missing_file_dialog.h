#ifndef __gtk_ardour_missing_file_dialog_h__
#define __gtk_ardour_missing_file_dialog_h__

#include <string>
#include <gtkmm/label.h>
#include <gtkmm/filechooserbutton.h>
#include <gtkmm/radiobutton.h>

#include "ardour/types.h"

#include "ardour_dialog.h"

namespace ARDOUR {
        class Session;
}

class MissingFileDialog : public ArdourDialog
{
  public:
        MissingFileDialog (ARDOUR::Session*, const std::string& path, ARDOUR::DataType type);

        int get_action();

  private:
        ARDOUR::DataType filetype;

        Gtk::FileChooserButton chooser;
        Gtk::RadioButton use_chosen;
        Gtk::RadioButton::Group choice_group;
        Gtk::RadioButton use_chosen_and_no_more_questions;
        Gtk::RadioButton stop_loading_button;
        Gtk::RadioButton all_missing_ok;
        Gtk::RadioButton this_missing_ok;
        Gtk::Label msg;

        void add_chosen ();
};

#endif /* __gtk_ardour_missing_file_dialog_h__ */
