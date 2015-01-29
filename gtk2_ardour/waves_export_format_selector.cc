/*
    Copyright (C) 2008 Paul Davis
    Copyright (C) 2015 Waves Audio Ltd.
    Author: Sakari Bergen

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

#include "waves_export_format_selector.h"
#include "export_format_dialog.h"
#include "ardour/export_format_specification.h"
#include "ardour/export_profile_manager.h"

#include "i18n.h"

WavesExportFormatSelector::WavesExportFormatSelector ()
  : Gtk::VBox ()
  , WavesUI ("waves_export_format_selector.xml", *this)
  , _format_dropdown (get_waves_dropdown ("format_dropdown"))
  , _depth_dropdown (get_waves_dropdown ("depth_dropdown"))
  , _sample_rate_dropdown (get_waves_dropdown ("sample_rate_dropdown"))
  , _dithering_dropdown (get_waves_dropdown ("dithering_dropdown"))
  , _normalize_button (get_waves_button ("normalize_button"))
{
	_depth_dropdown.selected_item_changed.connect (sigc::mem_fun (*this, &WavesExportFormatSelector::on_depth_dropdown));
	_format_dropdown.selected_item_changed.connect (sigc::mem_fun (*this, &WavesExportFormatSelector::on_format_dropdown));
	_sample_rate_dropdown.selected_item_changed.connect (sigc::mem_fun (*this, &WavesExportFormatSelector::on_sample_rate_dropdown));
	_dithering_dropdown.selected_item_changed.connect (sigc::mem_fun (*this, &WavesExportFormatSelector::on_dithering_dropdown));
	_normalize_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesExportFormatSelector::on_normalize_button));
}

WavesExportFormatSelector::~WavesExportFormatSelector ()
{

}

void
WavesExportFormatSelector::set_state (ARDOUR::ExportProfileManager::FormatStatePtr const state_, ARDOUR::Session * session_)
{
	SessionHandlePtr::set_session (session_);
	_state = state_;
	update_selector ();
}

void
WavesExportFormatSelector::update_selector ()
{
	set_visible (_state && _state->format);
	if (_state && _state->format) {
		update_selector_format ();
		update_selector_depth ();
		update_selector_sample_rate ();
		update_selector_dithering ();
		update_selector_normalize ();
	}
}

void
WavesExportFormatSelector::update_selector_format ()
{
	ExportFormatId export_format_id = NoFormat;
	if (_state && _state->format) {
		switch (_state->format->format_id ()) {
		case ARDOUR::ExportFormatBase::F_AIFF:
			export_format_id = AIFF;
			break;
		case ARDOUR::ExportFormatBase::F_CAF:
			export_format_id = CAF;
			break;
		case ARDOUR::ExportFormatBase::F_WAV:
		default:
			export_format_id = Wave;
			break;
		}
		int size = _format_dropdown.get_menu ().items ().size ();
		for (int i = 0; i < size; i++) {
			if (_format_dropdown.get_item_data_u (i) == export_format_id) {
				_format_dropdown.set_current_item (i);
				break;
			}
		}
	}
}

void
WavesExportFormatSelector::update_selector_depth ()
{
	int depth = 16;
	if (_state && _state->format) {
		switch (_state->format->sample_format ()) {
		case ARDOUR::ExportFormatBase::SF_24:
			depth = 24;
			break;
		case ARDOUR::ExportFormatBase::SF_16:
		default:
			depth = 16;
			break;
		}
		int size = _depth_dropdown.get_menu ().items ().size ();
		for (int i = 0; i < size; i++) {
			if (_depth_dropdown.get_item_data_u (i) == depth) {
				_depth_dropdown.set_current_item (i);
				break;
			}
		}
	}
}

void
WavesExportFormatSelector::update_selector_sample_rate ()
{
	int sample_rate = 44100;
	if (_state && _state->format) {
		switch (_state->format->sample_rate ()) {
		case ARDOUR::ExportFormatBase::SR_Session:
			sample_rate = 1;
			break;
		case ARDOUR::ExportFormatBase::SR_48:
			sample_rate = 48000;
			break;
		case ARDOUR::ExportFormatBase::SR_88_2:
			sample_rate = 88200;
			break;
		case ARDOUR::ExportFormatBase::SR_96:
			sample_rate = 96000;
			break;
		case ARDOUR::ExportFormatBase::SR_192:
			sample_rate = 192000;
			break;
		case ARDOUR::ExportFormatBase::SR_44_1:
		default:
			sample_rate = 44100;
			break;
		}
		int size = _sample_rate_dropdown.get_menu ().items ().size ();
		for (int i = 0; i < size; i++) {
			if (_sample_rate_dropdown.get_item_data_u (i) == sample_rate) {
				_sample_rate_dropdown.set_current_item (i);
				break;
			}
		}
	}
}

void
WavesExportFormatSelector::update_selector_dithering ()
{
	ExportDitheringId export_dithering_id = NoDithering;
	if (_state && _state->format) {
		switch (_state->format->dither_type ()) {
		case ARDOUR::ExportFormatBase::D_Shaped:
			export_dithering_id = Shaped;
			break;
		case ARDOUR::ExportFormatBase::D_Tri:
			export_dithering_id = Triangular;
			break;
		case ARDOUR::ExportFormatBase::D_Rect:
			export_dithering_id = Rectangular;
			break;
		case ARDOUR::ExportFormatBase::D_None:
		default:
			export_dithering_id = NoDithering;
			break;
		}
		int size = _dithering_dropdown.get_menu ().items ().size ();
		for (int i = 0; i < size; i++) {
			if (_dithering_dropdown.get_item_data_u (i) == export_dithering_id) {
				_dithering_dropdown.set_current_item (i);
				break;
			}
		}
	}
}

void
WavesExportFormatSelector::update_selector_normalize ()
{
	if (_state && _state->format) {
		_normalize_button.set_active_state (_state->format->normalize () ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	}
	CriticalSelectionChanged();
}

void 
WavesExportFormatSelector::on_format_dropdown (WavesDropdown*, int item)
{
	if (_state && _state->format) {
		ExportFormatId export_format_id = ExportFormatId(_format_dropdown.get_item_data_u(item));
		switch (export_format_id) {
		case AIFF:
			_state->format->set_format_id (ARDOUR::ExportFormatBase::F_AIFF);
			_state->format->set_extension ("aiff");
			break;
		case CAF:
			_state->format->set_format_id (ARDOUR::ExportFormatBase::F_CAF);
			_state->format->set_extension ("caf");
			break;
		case Wave:
		default:
			_state->format->set_format_id (ARDOUR::ExportFormatBase::F_WAV);
			_state->format->set_extension ("wav");
			break;
		}
		FormatEdited (_state->format);
		CriticalSelectionChanged();
	}
}

void 
WavesExportFormatSelector::on_depth_dropdown (WavesDropdown*, int item)
{
	if (_state && _state->format) {
		unsigned int depth = _depth_dropdown.get_item_data_u(item);

		switch (depth) {
		case 24:
			_state->format->set_sample_format (ARDOUR::ExportFormatBase::SF_24);
			break;
		case 16:
		default:
			_state->format->set_sample_format (ARDOUR::ExportFormatBase::SF_16);
			break;
		}
		FormatEdited (_state->format);
		CriticalSelectionChanged();
	}
}

void 
WavesExportFormatSelector::on_sample_rate_dropdown (WavesDropdown*, int item)
{
	if (_state && _state->format) {
		unsigned int sample_rate = _sample_rate_dropdown.get_item_data_u(item);

		switch (sample_rate) {
		case 44100:
			_state->format->set_sample_rate (ARDOUR::ExportFormatBase::SR_44_1);
			break;
		case 48000:
			_state->format->set_sample_rate (ARDOUR::ExportFormatBase::SR_48);
			break;
		case 88200:
			_state->format->set_sample_rate (ARDOUR::ExportFormatBase::SR_88_2);
			break;
		case 96000:
			_state->format->set_sample_rate (ARDOUR::ExportFormatBase::SR_96);
			break;
		case 192000:
			_state->format->set_sample_rate (ARDOUR::ExportFormatBase::SR_192);
			break;
		case 1: // from session
		default:
			_state->format->set_sample_rate (ARDOUR::ExportFormatBase::SR_Session);
			break;
		}
		FormatEdited (_state->format);
		CriticalSelectionChanged();
	}
}

void 
WavesExportFormatSelector::on_dithering_dropdown (WavesDropdown*, int item)
{
	if (_state && _state->format) {
		ExportDitheringId export_format_id = ExportDitheringId(_dithering_dropdown.get_item_data_u(item));
		switch (export_format_id) {
		case Shaped:
			_state->format->set_dither_type (ARDOUR::ExportFormatBase::D_Shaped);
			break;
		case Triangular:
			_state->format->set_dither_type (ARDOUR::ExportFormatBase::D_Tri);
			break;
		case Rectangular:
			_state->format->set_dither_type (ARDOUR::ExportFormatBase::D_Rect);
			break;
		case NoDithering:
		default:
			_state->format->set_dither_type (ARDOUR::ExportFormatBase::D_None);
			break;
		}
		FormatEdited (_state->format);
		CriticalSelectionChanged();
	}
}

void WavesExportFormatSelector::on_normalize_button (WavesButton*)
{
	if (_state && _state->format) {
		_state->format->set_normalize (_normalize_button.active_state() == Gtkmm2ext::ExplicitActive);
		FormatEdited (_state->format);
		CriticalSelectionChanged();
	}
}

