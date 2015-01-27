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

#ifndef __waves_export_format_selector_h__
#define __waves_export_format_selector_h__

#include <string>
#include <gtkmm.h>
#include <sigc++/signal.h>
#include <boost/shared_ptr.hpp>

#include "ardour/export_profile_manager.h"
#include "ardour/session_handle.h"
#include "waves_ui.h"

namespace ARDOUR {
	class ExportFormatSpecification;
	class ExportProfileManager;
}

///
class WavesExportFormatSelector : public Gtk::VBox, public WavesUI, public ARDOUR::SessionHandlePtr
{

  private:

	typedef boost::shared_ptr<ARDOUR::ExportFormatSpecification> FormatPtr;
	typedef std::list<FormatPtr> FormatList;

  public:

	WavesExportFormatSelector ();
	~WavesExportFormatSelector ();

	void set_state (ARDOUR::ExportProfileManager::FormatStatePtr state_, ARDOUR::Session * session_);

	sigc::signal<void, FormatPtr> FormatEdited;

	/* Compatibility with other elements */

	sigc::signal<void> CriticalSelectionChanged;

  private:
	  enum ExportFormatId{
          NoFormat = 0,  
		  Wave = 1,
          AIFF = 2,
          CAF = 3,
          FLAC = 4
	  };

	  enum ExportDitheringId{
          NoDithering = 0,  
		  Triangular = 1,
          Rectangular = 2,
          Shaped = 3,
	  };

	void update_selector ();
	void update_selector_format ();
	void update_selector_depth ();
	void update_selector_sample_rate ();
	void update_selector_dithering ();
	void update_selector_normalize ();

	void on_format_dropdown (WavesDropdown*, int);
	void on_depth_dropdown (WavesDropdown*, int);
	void on_sample_rate_dropdown (WavesDropdown*, int);
	void on_dithering_dropdown (WavesDropdown*, int);
	void on_normalize_button (WavesButton*);

	ARDOUR::ExportProfileManager::FormatStatePtr _state;

	/*** GUI componenets ***/

	WavesDropdown& _format_dropdown;
	WavesDropdown& _depth_dropdown;
	WavesDropdown& _sample_rate_dropdown;
	WavesDropdown& _dithering_dropdown;
	WavesButton& _normalize_button;
};

#endif // __waves_export_format_selector_h__
