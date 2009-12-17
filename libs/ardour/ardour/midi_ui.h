#ifndef __libardour_midi_ui_h__
#define __libardour_midi_ui_h__

#include <list>
#include <boost/signals2.hpp>
#include "pbd/abstract_ui.h"

namespace MIDI { 
	class port;
}

namespace ARDOUR {

class Session;

/* this is mostly a placeholder because I suspect that at some
   point we will want to add more members to accomodate
   certain types of requests to the MIDI UI
*/

struct MidiUIRequest : public BaseUI::BaseRequestObject {
  public:
	MidiUIRequest () {}
	~MidiUIRequest() {}
};

class MidiControlUI : public AbstractUI<MidiUIRequest>
{
  public:
	MidiControlUI (Session& s);
	~MidiControlUI ();
	
	static BaseUI::RequestType PortChange;
	
	void change_midi_ports ();
	
  protected:
	void thread_init ();
	void do_request (MidiUIRequest*);
	
  private:
	typedef std::list<GSource*> PortSources;
	PortSources port_sources;
	ARDOUR::Session& _session;
	boost::signals2::scoped_connection rebind_connection;

	bool midi_input_handler (Glib::IOCondition, MIDI::Port*);
	void reset_ports ();
	void clear_ports ();
};

}

#endif /* __libardour_midi_ui_h__ */
