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

#include <cstdlib>
#include <cassert>
#include <sstream>
#include <algorithm>

#include "ardour/debug.h"
#include "ardour/session.h"

#include "oav_mappa.h"

#include "pbd/i18n.h"

#include "pbd/abstract_ui.cc" // implementation

using namespace ARDOUR;
using namespace PBD;
using namespace ArdourSurface;

Mappa::Mappa (Session& s)
	: ControlProtocol (s, _("Mappa"))
	, AbstractUI<MappaRequest> (name())
{
}

Mappa::~Mappa ()
{
	mappa_destroy (_mappa);
	stop ();
	//tear_down_gui ();
}

/* ****************************************************************************
 * Event Loop
 */

void*
Mappa::request_factory (uint32_t num_requests)
{
	/* AbstractUI<T>::request_buffer_factory() is a template method only
	 * instantiated in this source module. To provide something visible for
	 * use in the interface/descriptor, we have this static method that is
	 * template-free.
	 */
	return request_buffer_factory (num_requests);
}

void
Mappa::do_request (MappaRequest* req)
{
	if (req->type == CallSlot) {
		call_slot (MISSING_INVALIDATOR, req->the_slot);
	} else if (req->type == Quit) {
		stop ();
	}
}

void
Mappa::thread_init ()
{
	pthread_set_name (event_loop_name().c_str());

	PBD::notify_event_loops_about_thread_creation (pthread_self(), event_loop_name(), 2048);
	ARDOUR::SessionEvent::create_per_thread_pool (event_loop_name(), 128);

	set_thread_priority ();
}

/* ****************************************************************************
 * Initialization, Desinitialization
 */

int
Mappa::set_active (bool yn)
{
	DEBUG_TRACE (DEBUG::Mappa, string_compose("set_active init with yn: '%1'\n", yn));

	if (yn == active()) {
		return 0;
	}

	if (yn) {
		_mappa = mappa_create(NULL);
		if (!_mappa) {
			return -1;
		}

		start ();

	} else {
		stop ();
	}

	ControlProtocol::set_active (yn);
	DEBUG_TRACE (DEBUG::Mappa, string_compose("set_active done with yn: '%1'\n", yn));
	return 0;
}

void
Mappa::start ()
{
	DEBUG_TRACE (DEBUG::Mappa, "BaseUI::run ()\n");
	BaseUI::run ();
	register_with_mappa ();
	connect_session_signals ();

	Glib::RefPtr<Glib::TimeoutSource> periodic_timer = Glib::TimeoutSource::create (50);
	_periodic_connection = periodic_timer->connect (sigc::mem_fun (*this, &Mappa::periodic));
	periodic_timer->attach (main_loop()->get_context());
}

void
Mappa::stop ()
{
	DEBUG_TRACE (DEBUG::Mappa, "BaseUI::quit ()\n");
	BaseUI::quit ();

	/* now drop references, disconnect from signals */
	_session_connections.drop_connections ();
	_periodic_connection.disconnect ();
}

bool
Mappa::periodic ()
{
	mappa_iter (_mappa);
	return true;
}

/* ****************************************************************************
 * Actions & Callbacks
 */
void
Mappa::_cb_target_float (uint32_t target_id, float value, void* token, uint32_t token_size, void* userdata)
{
	static_cast<Mappa*> (userdata)->cb_target_float (target_id, value, token, token_size);
}

void
Mappa::cb_target_float (uint32_t target_id, float value, void* token, uint32_t token_size)
{
	DEBUG_TRACE (DEBUG::Mappa, string_compose("cb_target_float '%1' '%2'\n", target_id, value));
}

void
Mappa::register_with_mappa ()
{
	struct mappa_target_t t = {
		.name = "t_1",
		.func = _cb_target_float,
		.userdata = (void*)this,
	};
	uint32_t tid = 0;
	int ret = mappa_target_add (_mappa, &t, &tid, 0, 0);
	assert (ret == 0);
}

void
Mappa::connect_session_signals ()
{
}

void
Mappa::stripable_selection_changed ()
{
	/* invoked by libardour whenever strip selection changed */
}

