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

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/file_utils.h"

#include "ardour/filesystem_paths.h"
#include "ardour/zeroconf.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

ZeroConf::ZeroConf (std::string const& type, uint16_t port, std::string const& host)
	: _type (type)
	, _port (port)
	, _host (host)
#ifdef __APPLE__
	, _source_id (0)
	, _gio_channel (NULL)
#else
	, _avahi (NULL)
#endif
{
	start ();
}

ZeroConf::~ZeroConf ()
{
	stop ();
}

#ifdef __APPLE__

void
ZeroConf::stop ()
{
	if (_gio_channel) {
		g_source_remove (_source_id);
		g_io_channel_unref (_gio_channel);
	}
	DNSServiceRefDeallocate (_ref);
}

static void
dns_callback (DNSServiceRef       sdref,
              DNSServiceFlags     flags,
              DNSServiceErrorType errorCode,
              const char*         name,
              const char*         regtype,
              const char* domain, void* context)
{
	if (errorCode == kDNSServiceErr_NoError) {
		PBD::info << string_compose (_("ZeroConf registered: %1 %2 %3"), name, regtype, domain) << endmsg;
	} else {
		PBD::info << _("ZeroConf registration failed") << endmsg;
	}
}

gboolean
ZeroConf::event (GIOChannel*  source,
                 GIOCondition condition,
                 gpointer     data)
{
	ZeroConf* self = reinterpret_cast<ZeroConf*> (data);
	// assert (_fd == g_io_channel_unix_get_fd (source));

	if (condition & ~G_IO_IN) {
		/* remove on error */
		return false;
	} else {
		if (DNSServiceProcessResult (self->_ref) != kDNSServiceErr_NoError) {
			/* ZeroConf Error in data callback */
			return false;
		}
		return true;
	}
}

bool
ZeroConf::start ()
{
	if (kDNSServiceErr_NoError != DNSServiceRegister (&_ref, /*flags*/ 0, /*interfaceIndex*/ 0, /*name*/ NULL, _type.c_str (),
	                                                  /*domain*/ NULL, _host.empty () ? NULL : _host.c_str (), g_htons (_port),
	                                                  /*txtlen*/ 0, /*TXT*/ NULL, dns_callback, this)) {
		return false;
	}

	int fd       = DNSServiceRefSockFD (_ref);
	_gio_channel = g_io_channel_unix_new (fd);
	_source_id   = g_io_add_watch (_gio_channel, (GIOCondition) (G_IO_IN | G_IO_ERR | G_IO_HUP), ZeroConf::event, this);
	return true;
}

#elif defined PLATFORM_WINDOWS

/* in theory dns_sd API also works on Windows 10+
 * windns.h and dnsapi.dll (mingw does not currently support this),
 * besides we still offer windows 7 compatible binaries.
 *
 * alternatively https://github.com/videolabs/libmicrodns
 * (which will also be handy on Linux, rather than using a shell script)
 */

bool
ZeroConf::start ()
{
	return false;
}

bool
ZeroConf::stop ()
{
}

#else

bool
ZeroConf::start ()
{
	std::string avahi_exec;

	PBD::Searchpath sp (ARDOUR::ardour_dll_directory ());
	if (!PBD::find_file (sp, "ardour-avahi.sh", avahi_exec)) {
		PBD::warning << "ardour-avahi.sh was not found." << endmsg;
		return false;
	}

	char** argp;
	char   tmp[128];
	argp    = (char**)calloc (5, sizeof (char*));
	argp[0] = strdup (avahi_exec.c_str ());
	snprintf (tmp, sizeof (tmp), "%d", _port);
	argp[1] = strdup (tmp);
	argp[2] = strdup (_type.c_str ());
	snprintf (tmp, sizeof (tmp), "%d", getpid ());
	argp[3] = strdup (tmp);
	argp[4] = 0;

	_avahi = new ARDOUR::SystemExec (avahi_exec, argp);
	if (_avahi->start (SystemExec::ShareWithParent)) {
		return false;
	}
	return false;
}

void
ZeroConf::stop ()
{
	ARDOUR::SystemExec* tmp = _avahi;
	_avahi = 0;
	delete tmp;
}
#endif
