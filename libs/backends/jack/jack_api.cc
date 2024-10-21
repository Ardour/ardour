/*
 * Copyright (C) 2013-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#include "jack_connection.h"
#include "jack_audiobackend.h"

using namespace ARDOUR;

static std::shared_ptr<JACKAudioBackend> backend;
static std::shared_ptr<JackConnection> jack_connection;

static std::shared_ptr<AudioBackend> backend_factory (AudioEngine& ae);
static int  instantiate (const std::string& arg1, const std::string& arg2);
static int  deinstantiate ();
static bool already_configured ();
static bool available ();

static ARDOUR::AudioBackendInfo _descriptor = {
#if ! (defined(__APPLE__) || defined(PLATFORM_WINDOWS))
	"JACK/Pipewire",
#else
	"JACK",
#endif
	instantiate,
	deinstantiate,
	backend_factory,
	already_configured,
	available
};

static std::shared_ptr<AudioBackend>
backend_factory (AudioEngine& ae)
{
	if (!jack_connection) {
		return std::shared_ptr<AudioBackend>();
	}

	if (!backend) {
		backend.reset (new JACKAudioBackend (ae, _descriptor, jack_connection));
	}

	return backend;
}

static int
instantiate (const std::string& arg1, const std::string& arg2)
{
	try {
		jack_connection.reset (new JackConnection (arg1, arg2));
		backend.reset ();
	} catch (...) {
		return -1;
	}

	return 0;
}

static int
deinstantiate ()
{
	backend.reset ();
	jack_connection.reset ();

	return 0;
}

static bool
already_configured ()
{
	return !JackConnection::in_control ();
}

static bool
available ()
{
	return have_libjack() ? false : true;
}

extern "C" ARDOURBACKEND_API ARDOUR::AudioBackendInfo* descriptor() { return &_descriptor; }

