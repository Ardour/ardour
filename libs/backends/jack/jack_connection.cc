/*
    Copyright (C) 2013 Paul Davis

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
#include <iostream>

#include <boost/scoped_ptr.hpp>
#include <jack/session.h>

#include "pbd/epa.h"

#include "jack_connection.h"
#include "jack_utils.h"

#define GET_PRIVATE_JACK_POINTER(j)  jack_client_t* _priv_jack = (jack_client_t*) (j); if (!_priv_jack) { return; }
#define GET_PRIVATE_JACK_POINTER_RET(j,r) jack_client_t* _priv_jack = (jack_client_t*) (j); if (!_priv_jack) { return r; }

using namespace ARDOUR;
using namespace PBD;
using std::string;
using std::vector;
using std::cerr;
using std::endl;

bool JackConnection::_in_control = false;

static void jack_halted_callback (void* arg)
{
	JackConnection* jc = static_cast<JackConnection*> (arg);
	jc->halted_callback ();
}

static void jack_halted_info_callback (jack_status_t code, const char* reason, void* arg)
{
	JackConnection* jc = static_cast<JackConnection*> (arg);
	jc->halted_info_callback (code, reason);
}


JackConnection::JackConnection (const std::string& arg1, const std::string& arg2)
	: _jack (0)
	, _client_name (arg1)
	, session_uuid (arg2)
{
	/* See if the server is already up 
	 */

        EnvironmentalProtectionAgency* global_epa = EnvironmentalProtectionAgency::get_global_epa ();
        boost::scoped_ptr<EnvironmentalProtectionAgency> current_epa;

        /* revert all environment settings back to whatever they were when
	 * ardour started, because ardour's startup script may have reset
	 * something in ways that interfere with finding/starting JACK.
         */

        if (global_epa) {
                current_epa.reset (new EnvironmentalProtectionAgency(true)); /* will restore settings when we leave scope */
                global_epa->restore ();
        }

	jack_status_t status;
	jack_client_t* c = jack_client_open ("ardourprobe", JackNoStartServer, &status);

	if (status == 0) {
		jack_client_close (c);
		_in_control = false;
	} else {
		_in_control = true;
	}
}

JackConnection::~JackConnection ()
{
	close ();
}

int
JackConnection::open ()
{
        EnvironmentalProtectionAgency* global_epa = EnvironmentalProtectionAgency::get_global_epa ();
        boost::scoped_ptr<EnvironmentalProtectionAgency> current_epa;
	jack_status_t status;

	close ();

        /* revert all environment settings back to whatever they were when ardour started
         */

        if (global_epa) {
                current_epa.reset (new EnvironmentalProtectionAgency(true)); /* will restore settings when we leave scope */
                global_epa->restore ();
        }

	/* ensure that PATH or equivalent includes likely locations of the JACK
	 * server, in case the user's default does not.
	 */

	vector<string> dirs;
	get_jack_server_dir_paths (dirs);
	set_path_env_for_jack_autostart (dirs);

	if ((_jack = jack_client_open (_client_name.c_str(), JackSessionID, &status, session_uuid.c_str())) == 0) {
		return -1;
	}

	if (status & JackNameNotUnique) {
		_client_name = jack_get_client_name (_jack);
	}

	/* attach halted handler */

        if (jack_on_info_shutdown) {
                jack_on_info_shutdown (_jack, jack_halted_info_callback, this);
        } else {
                jack_on_shutdown (_jack, jack_halted_callback, this);
        }


	Connected(); /* EMIT SIGNAL */

	return 0;
}

int
JackConnection::close ()
{
	GET_PRIVATE_JACK_POINTER_RET (_jack, -1);

	if (_priv_jack) {	
		int ret = jack_client_close (_priv_jack);
		_jack = 0;

		/* If we started JACK, it will be closing down */
		usleep (500000);

		Disconnected (""); /* EMIT SIGNAL */

		return ret;
	}

	return 0;
}

void
JackConnection::halted_callback ()
{
	_jack = 0;
	std::cerr << "JACK HALTED\n";
	Disconnected ("");
}

void
JackConnection::halted_info_callback (jack_status_t /*status*/, const char* reason)
{
	_jack = 0;
	std::cerr << "JACK HALTED: " << reason << std::endl;
	Disconnected (reason);
}


