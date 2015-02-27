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

/*
 * This virtual parent class is so that each dialog box uses the
 * same mechanism to declare its closing. It shares a common
 * method of connecting and disconnecting from a Session with
 * all other objects that have a handle on a Session.
 */
class WavesDialog : public Gtk::Dialog, public ARDOUR::SessionHandlePtr, public WavesUI
{
  public:

	WavesDialog (const std::string& layout_script_file, bool modal = false, bool use_separator = false);
	virtual ~WavesDialog();
	void on_realize ();
	bool on_key_press_event (GdkEventKey*);
	void on_show ();
	bool on_delete_event (GdkEventAny*);
    

	enum {
		// We need one smaller then smallest Gtk::RESPONSE_*
		RESPONSE_DEFAULT = -1000
	};

  protected:
	virtual void on_default_response ();
    void on_response (int response_id);

  private:

	WM::ProxyTemporary* _proxy;
	bool _splash_pushed;
	
	static sigc::signal<void> CloseAllDialogs;
};

#endif // __waves_dialog_h__

