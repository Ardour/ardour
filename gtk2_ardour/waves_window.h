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
#ifndef __waves_window_h__
#define __waves_window_h__

#include <string>
#include <gtkmm.h>
#include "waves_ui.h"


class WavesWindow : public Gtk::Window {
  public:
	WavesWindow (Gtk::WindowType window_type);
	WavesWindow (Gtk::WindowType window_type, std::string layout_script);

  protected:
	WavesUI::WidgetMap& named_children() { return _children; }

  private:
	WavesUI::WidgetMap _children;
};

#endif // __waves_window_h__
