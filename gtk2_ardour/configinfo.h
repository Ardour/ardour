#include <gtkmm/textview.h>

#include "ardour_dialog.h"

class ConfigInfoDialog : public ArdourDialog
{
  public:
	ConfigInfoDialog();

  private:
	Gtk::TextView text;
};
