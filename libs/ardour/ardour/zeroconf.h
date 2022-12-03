/*
 * Copyright (C) 2022 Robin Gareus <robin@gareus.org>
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

#include <cstdint>
#include <string>

#ifdef __APPLE__
#include <dns_sd.h>
#include <glibmm/iochannel.h>
#include <glibmm/main.h>
#endif

#include "ardour/libardour_visibility.h"
#include "ardour/system_exec.h"

namespace ARDOUR {

class LIBARDOUR_API ZeroConf
{
public:
	ZeroConf (std::string const& type = "_osc._udp", uint16_t port = 3819, std::string const& host = "");
	~ZeroConf ();

private:
	bool start ();
	void stop ();

	std::string _type;
	uint16_t    _port;
	std::string _host;

#ifdef __APPLE__
	static gboolean event (GIOChannel*, GIOCondition, gpointer);

	DNSServiceRef _ref;
	guint         _source_id;
	GIOChannel*   _gio_channel;
#else
	SystemExec* _avahi;
#endif
};

}; // namespace ARDOUR
