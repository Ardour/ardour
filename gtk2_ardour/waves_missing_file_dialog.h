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

#ifndef __waves_missing_file_dialog_h__
#define __waves_missing_file_dialog_h__

#include <string>
#include <vector>
#include <map>

#include <sigc++/signal.h>

#include "ardour/types.h"
#include "waves_dialog.h"

class WavesMissingFileDialog : public WavesDialog
{
public:
	WavesMissingFileDialog (ARDOUR::Session*, const std::string&, ARDOUR::DataType);
    int get_action();

protected:

private:
	ARDOUR::DataType _filetype;
	std::string _additional_folder_name;

	WavesButton& _add_folder_button;
	WavesButton& _skip_file_button;
	WavesButton& _skip_all_files_button;
	WavesButton& _stop_loading_button;
	WavesButton& _browse_button;
	WavesButton& _done_button;

	void _add_chosen ();
	void _on_option_button (WavesButton*);
	void _on_browse_button (WavesButton*);
	void _on_done_button (WavesButton*);
};

#endif // __waves_missing_file_dialog_h__
