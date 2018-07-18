/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Paul Davis
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef ardour_surface_mappa_h
#define ardour_surface_mappa_h

#include <ctlra/mappa.h>

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"
#include "pbd/properties.h"
#include "pbd/controllable.h"

#include "ardour/types.h"

#include "control_protocol/control_protocol.h"

namespace ARDOUR {
	class Session;
}

namespace ArdourSurface {

struct MappaRequest : public BaseUI::BaseRequestObject
{
public:
	MappaRequest () {}
	~MappaRequest () {}
};

class Mappa : public ARDOUR::ControlProtocol, public AbstractUI<MappaRequest>
{
public:
	Mappa (ARDOUR::Session&);
	virtual ~Mappa ();

	int set_active (bool yn);

	static bool  probe() { return true; }

	static void* request_factory (uint32_t);
	void do_request (MappaRequest*);
	void thread_init ();

#if 0
	/* configuration GUI */
	void* get_gui () const;
	void  tear_down_gui ();
#endif
	bool  has_editor () const { return false; }

private:
	void start ();
	void stop ();
	bool periodic ();

	void stripable_selection_changed ();
	void connect_session_signals ();
	void register_with_mappa ();

	static void _cb_target_float (uint32_t, float, void*, uint32_t, void*);
	void cb_target_float (uint32_t target_id, float value, void* token, uint32_t token_size);

	struct mappa_t* _mappa;
	sigc::connection _periodic_connection;
	PBD::ScopedConnectionList _session_connections;
};

} /* namespace */

#endif
