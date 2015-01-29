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
#include "waves_import_dialog.h"
WavesImportDialog::WavesImportDialog ()
  : WavesDialog ("waves_import_dialog.xml", true, false )
  , _add_as_dropdown (get_waves_dropdown ("add_as_dropdown"))
  , _insert_at_dropdown (get_waves_dropdown ("insert_at_dropdown"))
  , _mapping_dropdown (get_waves_dropdown ("mapping_dropdown"))
  , _quality_dropdown (get_waves_dropdown ("quality_dropdown"))
  , _copy_to_session_button (get_waves_button ("copy_to_session_button"))

{
	get_waves_button ("import_button").signal_clicked.connect (sigc::mem_fun (*this, &WavesImportDialog::_on_import_button));
	get_waves_button ("cancel_button").signal_clicked.connect (sigc::mem_fun (*this, &WavesImportDialog::_on_cancel_button));
}


ARDOUR::SrcQuality 
WavesImportDialog::_get_src_quality() const
{
	ARDOUR::SrcQuality quality;

	switch (_quality_dropdown.get_item_data_u (_quality_dropdown.get_current_item ())) {
	case Good:
		quality = ARDOUR::SrcGood;
		break;
	case Quick:
		quality = ARDOUR::SrcQuick;
		break;
	case Fast:
		quality = ARDOUR::SrcFast;
		break;
	case Fastest:
		quality = ARDOUR::SrcFastest;
		break;
	case Best:
	default:
		quality = ARDOUR::SrcBest;
		break;
	}
	
	return quality;
}

Editing::ImportMode
WavesImportDialog::_get_import_mode() const
{
	Editing::ImportMode import_mode;
	switch (_add_as_dropdown.get_item_data_u (_add_as_dropdown.get_current_item ())) {
	case AsTrack:
		import_mode = Editing::ImportAsTrack;
		break;
	case ToTrack:
		import_mode = Editing::ImportToTrack;
	case AsRegion:
		import_mode = Editing::ImportAsRegion;
		break;
	case AsTapeTrack:
		import_mode = Editing::ImportAsTapeTrack;
		break;
	default:
		break;
	}
	return import_mode;
}

void
WavesImportDialog::_on_cancel_button (WavesButton*)
{
	response (Gtk::RESPONSE_CANCEL);
}

void
WavesImportDialog::_on_import_button (WavesButton*)
{
	_done = true;
	_status = Gtk::RESPONSE_OK;
	ARDOUR::Session* session = ARDOUR_UI::instance()->the_session();
	framepos_t where;

	switch (_insert_at_dropdown.get_item_data_u (_insert_at_dropdown.get_current_item ())) {
	case EditPoint:
		where = PublicEditor::instance().get_preferred_edit_position ();
		break;
	case Timestamp:
		where = -1;
		break;
	case Playhead:
		where = session->transport_frame();
		break;
	case Start:
	default:
		where = session->current_start_frame();
		break;
	}

	ARDOUR::SrcQuality quality = _get_src_quality ();

	response (Gtk::RESPONSE_OK);
}
