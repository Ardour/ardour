/*
    Copyright (C) 2005-2006 Paul Davis

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

#ifndef __waves_import_dialog_h__
#define __waves_import_dialog_h__

#include <string>
#include <vector>
#include <map>

#include <sigc++/signal.h>

#include "ardour/audiofilesource.h"
#include "ardour/session_handle.h"

#include "ardour/session.h"
#include "editing.h"
#include "ardour_ui.h"
#include "public_editor.h"
#include "waves_dialog.h"

class WavesImportDialog : public WavesDialog
{
public:
	WavesImportDialog ();

private:
	enum Impord {
		AsTrack = 0,
		ToTrack = 1,
		AsRegion = 2,
		AsTapeTrack = 3
	};

	enum InsertionPosition {
		Timestamp = 0,
		EditPoint = 1,
		Playhead = 2,
		Start = 3
	};

	enum ConversionQuality {
		Best = 0,
		Good = 1,
		Quick = 2,
		Fast = 3,
		Fastest = 4
	};

	ARDOUR::SrcQuality _get_src_quality () const;
	Editing::ImportMode _get_import_mode() const;

	void _on_cancel_button (WavesButton*);
	void _on_import_button (WavesButton*);
	
    int _status;
    bool _done;

	WavesDropdown& _add_as_dropdown;
	WavesDropdown& _insert_at_dropdown; 
	WavesDropdown& _mapping_dropdown;
	WavesDropdown& _quality_dropdown;
	WavesButton& _copy_to_session_button;
};

#endif // __waves_import_dialog_h__
