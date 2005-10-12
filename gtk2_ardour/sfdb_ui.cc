#include "sfdb_ui.h"

#include "i18n.h"

SoundFileChooser::SoundFileChooser (std::string title,
		bool split_makes_sense)
	:
	Gtk::FileChooserDialog(title)
{
	
}

SoundFileChooser::~SoundFileChooser ()
{

}
