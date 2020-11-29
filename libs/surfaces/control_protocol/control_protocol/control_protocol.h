/*
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2017 John Emmas <john@creativepost.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef ardour_control_protocols_h
#define ardour_control_protocols_h

#include <list>
#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

#include "pbd/signals.h"
#include "pbd/stateful.h"

#include "control_protocol/basic_ui.h"
#include "control_protocol/types.h"
#include "control_protocol/visibility.h"

namespace ARDOUR {

class Route;
class Session;
class Bundle;
class Stripable;

class LIBCONTROLCP_API ControlProtocol : public PBD::Stateful, public PBD::ScopedConnectionList, public BasicUI
{
public:
	ControlProtocol (Session&, std::string name);
	virtual ~ControlProtocol ();

	virtual std::string name () const { return _name; }

	virtual int set_active (bool yn);
	virtual bool active () const { return _active; }

	virtual int set_feedback (bool /*yn*/) { return 0; }
	virtual bool get_feedback () const { return false; }

	virtual void midi_connectivity_established () {}

	virtual void stripable_selection_changed () = 0;

	PBD::Signal0<void> ActiveChanged;

	/* signals that a control protocol can emit and other (presumably graphical)
	 * user interfaces can respond to
	 */

	static PBD::Signal0<void> ZoomToSession;
	static PBD::Signal0<void> ZoomIn;
	static PBD::Signal0<void> ZoomOut;
	static PBD::Signal0<void> Enter;
	static PBD::Signal0<void> Undo;
	static PBD::Signal0<void> Redo;
	static PBD::Signal1<void, float> ScrollTimeline;
	static PBD::Signal1<void, uint32_t> GotoView;
	static PBD::Signal0<void> CloseDialog;
	static PBD::Signal0<void> VerticalZoomInAll;
	static PBD::Signal0<void> VerticalZoomOutAll;
	static PBD::Signal0<void> VerticalZoomInSelected;
	static PBD::Signal0<void> VerticalZoomOutSelected;
	static PBD::Signal0<void> StepTracksDown;
	static PBD::Signal0<void> StepTracksUp;

	void add_stripable_to_selection (boost::shared_ptr<ARDOUR::Stripable>);
	void set_stripable_selection (boost::shared_ptr<ARDOUR::Stripable>);
	void toggle_stripable_selection (boost::shared_ptr<ARDOUR::Stripable>);
	void remove_stripable_from_selection (boost::shared_ptr<ARDOUR::Stripable>);
	void clear_stripable_selection ();

	boost::shared_ptr<ARDOUR::Stripable> first_selected_stripable () const;

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
	void  route_set_gain (uint32_t table_index, float);
	float route_get_effective_gain (uint32_t table_index);

	float route_get_peak_input_power (uint32_t table_index, uint32_t which_input);

	bool route_get_muted (uint32_t table_index);
	void route_set_muted (uint32_t table_index, bool);

	bool route_get_soloed (uint32_t table_index);
	void route_set_soloed (uint32_t table_index, bool);

	std::string route_get_name (uint32_t table_index);

	virtual std::list<boost::shared_ptr<ARDOUR::Bundle> > bundles ();

	virtual bool has_editor () const { return false; }
	virtual void* get_gui () const { return 0; }
	virtual void tear_down_gui () {}

	XMLNode& get_state ();
	int set_state (XMLNode const&, int version);

	static const std::string state_node_name;

	static StripableNotificationList const& last_selected () { return _last_selected; }
	static void notify_stripable_selection_changed (StripableNotificationListPtr);

	void event_loop_precall ();

protected:
	void next_track (uint32_t initial_id);
	void prev_track (uint32_t initial_id);

	std::vector<boost::shared_ptr<ARDOUR::Route> > route_table;
	std::string _name;

private:
	LIBCONTROLCP_LOCAL ControlProtocol (const ControlProtocol&); /* noncopyable */

	bool _active;

	static StripableNotificationList          _last_selected;
	static PBD::ScopedConnection              selection_connection;
	static bool                               selection_connected;
};

extern "C" {
class ControlProtocolDescriptor
{
public:
	const char* name;              /* descriptive */
	const char* id;                /* unique and version-specific */
	void*       ptr;               /* protocol can store a value here */
	void*       module;            /* not for public access */
	int         mandatory;         /* if non-zero, always load and do not make optional */
	bool        supports_feedback; /* if true, protocol has toggleable feedback mechanism */
	bool (*probe) (ControlProtocolDescriptor*);
	ControlProtocol* (*initialize) (ControlProtocolDescriptor*, Session*);
	void (*destroy) (ControlProtocolDescriptor*, ControlProtocol*);
	/* this is required if the control protocol connects to signals
	 * from libardour. they all do. It should allocate a
	 * type-specific request buffer for the calling thread, and
	 * store it in a thread-local location that will be used to
	 * find it when sending the event loop a message
	 * (e.g. call_slot()). It should also return the allocated
	 * buffer as a void*.
	 */
	void* (*request_buffer_factory) (uint32_t);
};
}
}

#endif // ardour_control_protocols_h
