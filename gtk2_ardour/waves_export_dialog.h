/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#ifndef __waves_export_dialog_h__
#define __waves_export_dialog_h__

#include "ardour/export_profile_manager.h"

#include "waves_dialog.h"

class WavesExportDialog : public WavesDialog
{
public:
	WavesExportDialog(const std::string &title, ARDOUR::Session* session, ARDOUR::ExportProfileManager::ExportType type);

protected:
	WavesButton& _export_button;
	WavesButton& _cancel_button;
	WavesButton& _stop_export_button;
	Gtk::ProgressBar &_export_progress_bar;
   
private:
	#include "waves_export_dialog.logic.h"
};

#endif /* __waves_export_dialog_h__ */
