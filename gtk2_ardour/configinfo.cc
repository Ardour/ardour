#include "ardour/ardour.h"

#include "configinfo.h"
#include "i18n.h"

ConfigInfoDialog::ConfigInfoDialog ()
	: ArdourDialog (_("Build Configuration"))
{
	set_border_width (12);
	text.get_buffer()->set_text (Glib::ustring (ARDOUR::ardour_config_info));
	text.set_wrap_mode (Gtk::WRAP_WORD);
	text.show ();
	text.set_size_request (300, 800);

	get_vbox()->pack_start (text, true, true);
}
