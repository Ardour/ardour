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

#ifndef __waves_dialog_h__
#define __waves_dialog_h__

#include <gtkmm.h>

#include "ardour/session_handle.h"
#include "waves_ui.h"
#include "canvas/xml_ui.h"

using namespace ArdourCanvas::XMLUI;

namespace WM {
	class ProxyTemporary;
}

class WavesButton;
class XMLNode;

class WavesDialog : public Gtk::Dialog, public ARDOUR::SessionHandlePtr, public WavesUI
{
  public:

	WavesDialog (std::string layout_script, bool modal = false, bool use_separator = false);
	~WavesDialog();

	bool on_enter_notify_event (GdkEventCrossing*);
	bool on_leave_notify_event (GdkEventCrossing*);
	bool on_delete_event (GdkEventAny*);
	bool on_key_press_event (GdkEventKey*);
	void on_unmap ();
	void on_show ();

  private:

	WM::ProxyTemporary* _proxy;
	bool _splash_pushed;
	
	static sigc::signal<void> CloseAllDialogs;
};

#endif // __waves_dialog_h__

