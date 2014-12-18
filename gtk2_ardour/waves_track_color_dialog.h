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

#ifndef __waves_track_color_dialog_h__
#define __waves_track_color_dialog_h__

#include "ardour/ardour.h"
#include "ardour/route.h"
#include "ardour/session_handle.h"
#include "waves_ui.h"

class WavesTrackColorDialog : public Gtk::Window, public ARDOUR::SessionHandlePtr, public WavesUI
{
public:
	WavesTrackColorDialog();
	virtual ~WavesTrackColorDialog();

protected:
	void on_realize ();

private:
	void _init();

	WavesButton* color_button[15];
	Gtk::Container& _empty_panel;
	Gtk::Container& _color_buttons_home;
	#include "waves_track_color_dialog.logic.h"
};

#endif /* __waves_track_color_dialog_h__ */
