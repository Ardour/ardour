/*	Copyright (C) 2014 Waves Audio Ltd.
    This program is free software; you can redistribute it and/or modify    it under the terms of the GNU General Public License as published by    the Free Software Foundation; either version 2 of the License, or    (at your option) any later version.
    This program is distributed in the hope that it will be useful,    but WITHOUT ANY WARRANTY; without even the implied warranty of    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License    along with this program; if not, write to the Free Software    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.*/
#include "waves_window.h"#include "dbg_msg.h"
WavesWindow::WavesWindow (Gtk::WindowType window_type)	: Gtk::Window (window_type){}
WavesWindow::WavesWindow (Gtk::WindowType window_type, std::string layout_script) : Gtk::Window (window_type){	const XMLTree* layout = WavesUI::load_layout(layout_script);	if (layout == NULL) {		return;	}
	XMLNode* root  = layout->root();	if ((root == NULL) || strcasecmp(root->name().c_str(), "Window")) {		return;	}
	WavesUI::create_ui(layout, *this, _children);}