/*
    Copyright (C) 2012 Paul Davis 

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

#ifndef __libardour_midi_ui_h__
#define __libardour_midi_ui_h__

#include <list>

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"
#include "pbd/signals.h"
#include "pbd/stacktrace.h"


namespace ARDOUR {

class Session;
class AsyncMIDIPort;

/* this is mostly a placeholder because I suspect that at some
   point we will want to add more members to accomodate
   certain types of requests to the MIDI UI
*/

struct LIBARDOUR_API MidiUIRequest : public BaseUI::BaseRequestObject {
  public:
	MidiUIRequest () { }
	~MidiUIRequest() { }
};

class LIBARDOUR_API MidiControlUI : public AbstractUI<MidiUIRequest>
{
  public:
	MidiControlUI (Session& s);
	~MidiControlUI ();

	static MidiControlUI* instance() { return _instance; }

	static BaseUI::RequestType PortChange;

	void change_midi_ports ();

  protected:
	void thread_init ();
	void do_request (MidiUIRequest*);

  private:
	typedef std::list<GSource*> PortSources;
	PortSources port_sources;
	ARDOUR::Session& _session;
	PBD::ScopedConnection rebind_connection;

	bool midi_input_handler (Glib::IOCondition, AsyncMIDIPort*);
	void reset_ports ();
	void clear_ports ();

	static MidiControlUI* _instance;
};

}

#endif /* __libardour_midi_ui_h__ */
