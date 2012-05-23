/*
    Copyright (C) 2012 Paul Davis

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

#ifndef __gtk2_ardour_midi_export_dialog_h__
#define __gtk2_ardour_midi_export_dialog_h__

#include <boost/shared_ptr.hpp>

#include <gtkmm/filechooser.h>

#include "ardour_dialog.h"
#include "public_editor.h"

class MidiExportDialog : public ArdourDialog {
  public:
	MidiExportDialog (PublicEditor& editor, boost::shared_ptr<ARDOUR::MidiRegion>);
	~MidiExportDialog ();

	std::string get_path() const;

  private:
	Gtk::FileChooserWidget file_chooser;
};

#endif /* __gtk2_ardour_midi_export_dialog_h__ */
