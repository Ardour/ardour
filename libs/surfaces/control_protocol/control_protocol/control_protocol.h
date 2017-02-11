/*
    Copyright (C) 2006 Paul Davis

    This program is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser
    General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your
    option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef ardour_control_protocols_h
#define ardour_control_protocols_h

#include <string>
#include <vector>
#include <list>

#include <boost/shared_ptr.hpp>

#include "pbd/stateful.h"
#include "pbd/signals.h"

#include "control_protocol/visibility.h"
#include "control_protocol/basic_ui.h"
#include "control_protocol/types.h"

namespace ARDOUR {

class Route;
class Session;
class Bundle;
class Stripable;

class LIBCONTROLCP_API ControlProtocol : public PBD::Stateful, public PBD::ScopedConnectionList, public BasicUI
{
  public:
	ControlProtocol (Session&, std::string name);
	virtual ~ControlProtocol();

	std::string name() const { return _name; }

        virtual int set_active (bool yn);
        bool active() const { return _active; }

	virtual int set_feedback (bool /*yn*/) { return 0; }
	virtual bool get_feedback () const { return false; }

	virtual void midi_connectivity_established () {}

	PBD::Signal0<void> ActiveChanged;

	/* signals that a control protocol can emit and other (presumably graphical)
	   user interfaces can respond to
	*/

	static PBD::Signal0<void> ZoomToSession;
	static PBD::Signal0<void> ZoomIn;
	static PBD::Signal0<void> ZoomOut;
	static PBD::Signal0<void> Enter;
	static PBD::Signal0<void> Undo;
	static PBD::Signal0<void> Redo;
	static PBD::Signal1<void,float> ScrollTimeline;
	static PBD::Signal1<void,uint32_t> GotoView;
	static PBD::Signal0<void> CloseDialog;
	static PBD::Signal0<void> VerticalZoomInAll;
	static PBD::Signal0<void> VerticalZoomOutAll;
	static PBD::Signal0<void> VerticalZoomInSelected;
	static PBD::Signal0<void> VerticalZoomOutSelected;
	static PBD::Signal0<void> StepTracksDown;
	static PBD::Signal0<void> StepTracksUp;

	static PBD::Signal1<void,boost::shared_ptr<ARDOUR::Stripable> > AddStripableToSelection;
	static PBD::Signal1<void,boost::shared_ptr<ARDOUR::Stripable> > SetStripableSelection;
	static PBD::Signal1<void,boost::shared_ptr<ARDOUR::Stripable> > ToggleStripableSelection;
	static PBD::Signal1<void,boost::shared_ptr<ARDOUR::Stripable> > RemoveStripableFromSelection;
	static PBD::Signal0<void>          ClearStripableSelection;

	/* signals that one UI (e.g. the GUI) can emit to get all other UI's to
	   respond. Typically this will always be GUI->"others" - the GUI pays
	   no attention to these signals.
	*/

	static PBD::Signal1<void,StripableNotificationListPtr> StripableSelectionChanged;

	static boost::shared_ptr<ARDOUR::Stripable> first_selected_stripable ();
	static void set_first_selected_stripable (boost::shared_ptr<ARDOUR::Stripable>);

	static boost::shared_ptr<ARDOUR::Stripable> leftmost_mixer_stripable ();
	static void set_leftmost_mixer_stripable (boost::shared_ptr<ARDOUR::Stripable>);

	/* the model here is as follows:

	   we imagine most control surfaces being able to control
	   from 1 to N tracks at a time, with a session that may
	   contain 1 to M tracks, where M may be smaller, larger or
	   equal to N.

	   the control surface has a fixed set of physical controllers
	   which can potentially be mapped onto different tracks/busses
	   via some mechanism.

	   therefore, the control protocol object maintains
	   a table that reflects the current mapping between
	   the controls and route object.
	*/

	void set_route_table_size (uint32_t size);
	void set_route_table (uint32_t table_index, boost::shared_ptr<ARDOUR::Route>);
	bool set_route_table (uint32_t table_index, uint32_t remote_control_id);

	void route_set_rec_enable (uint32_t table_index, bool yn);
	bool route_get_rec_enable (uint32_t table_index);

	float route_get_gain (uint32_t table_index);
	void route_set_gain (uint32_t table_index, float);
	float route_get_effective_gain (uint32_t table_index);

	float route_get_peak_input_power (uint32_t table_index, uint32_t which_input);

	bool route_get_muted (uint32_t table_index);
	void route_set_muted (uint32_t table_index, bool);

	bool route_get_soloed (uint32_t table_index);
	void route_set_soloed (uint32_t table_index, bool);

	std::string route_get_name (uint32_t table_index);

	virtual std::list<boost::shared_ptr<ARDOUR::Bundle> > bundles ();

	virtual bool  has_editor () const { return false; }
	virtual void* get_gui() const { return 0; }
	virtual void  tear_down_gui() { }

        XMLNode& get_state ();
        int set_state (XMLNode const &, int version);

        static const std::string state_node_name;
        static StripableNotificationList const & last_selected() { return _last_selected; }

  protected:
	std::vector<boost::shared_ptr<ARDOUR::Route> > route_table;
	std::string _name;

	void next_track (uint32_t initial_id);
	void prev_track (uint32_t initial_id);

  private:
	LIBCONTROLCP_LOCAL ControlProtocol (const ControlProtocol&); /* noncopyable */
        bool _active;


        static Glib::Threads::Mutex special_stripable_mutex;
        static boost::weak_ptr<ARDOUR::Stripable> _leftmost_mixer_stripable;
        static boost::weak_ptr<ARDOUR::Stripable> _first_selected_stripable;
        static StripableNotificationList _last_selected;
        static void stripable_selection_changed (StripableNotificationListPtr);
        static bool selection_connected;
        static PBD::ScopedConnection selection_connection;
};

extern "C" {
	class ControlProtocolDescriptor {
	public:
		const char* name;      /* descriptive */
		const char* id;        /* unique and version-specific */
		void*       ptr;       /* protocol can store a value here */
		void*       module;    /* not for public access */
		int         mandatory; /* if non-zero, always load and do not make optional */
		bool        supports_feedback; /* if true, protocol has toggleable feedback mechanism */
		bool             (*probe)(ControlProtocolDescriptor*);
		ControlProtocol* (*initialize)(ControlProtocolDescriptor*,Session*);
		void             (*destroy)(ControlProtocolDescriptor*,ControlProtocol*);
		/* this is required if the control protocol connects to signals
		   from libardour. they all do. It should allocate a
		   type-specific request buffer for the calling thread, and
		   store it in a thread-local location that will be used to
		   find it when sending the event loop a message
		   (e.g. call_slot()). It should also return the allocated
		   buffer as a void*.
		*/
		void*            (*request_buffer_factory)(uint32_t);
	};
}

}

#endif // ardour_control_protocols_h
