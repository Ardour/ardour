/*
    Copyright (C) 1998-2010 Paul Barton-Davis
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

#ifndef  __libmidi_ipmidi_port_h__
#define  __libmidi_ipmidi_port_h__

#include <string>
#include <iostream>
#if defined(PLATFORM_WINDOWS)
#include <winsock.h>
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
#include <netinet/in.h>
#include <sys/socket.h>
#if defined(__FreeBSD__)
#include <sys/_sockaddr_storage.h>
#endif
#include <arpa/inet.h>
#include <netinet/ip_carp.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#else
#include <arpa/inet.h>
#include <net/if.h>
#endif

#include <glibmm/threads.h>

#include "pbd/xml++.h"
#include "pbd/crossthread.h"
#include "pbd/signals.h"
#include "pbd/ringbuffer.h"

#include "midi++/libmidi_visibility.h"
#include "midi++/types.h"
#include "midi++/parser.h"
#include "midi++/port.h"

namespace MIDI {

class LIBMIDIPP_API IPMIDIPort : public Port {
  public:
    IPMIDIPort (int base_port = lowest_ipmidi_port_default, const std::string& ifname = std::string());
    IPMIDIPort (const XMLNode&);
    ~IPMIDIPort ();

    XMLNode& get_state () const;
    void set_state (const XMLNode&);

    int write (const byte *msg, size_t msglen, timestamp_t timestamp);
    int read (byte *buf, size_t bufsize);
    void parse (framecnt_t timestamp);
    int selectable () const;

    static const int lowest_ipmidi_port_default = 21928;

private:
    int sockin;
    int sockout;
    struct sockaddr_in addrout;
    Glib::Threads::Mutex write_lock;

    bool open_sockets (int base_port, const std::string& ifname);
    void close_sockets ();

    void init (std::string const &, Flags);
};

} // namespace MIDI

#endif // __libmidi_ipmidi_port_h__
