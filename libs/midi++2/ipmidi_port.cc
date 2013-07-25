/*
    Copyright (C) 2012 Paul Davis

    Using code from Rui Nuno Capela's qmidinet as inspiration.
    
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

    $Id: port.cc 12065 2012-04-23 16:23:48Z paul $
*/
#include <iostream>
#include <cstdio>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifdef   COMPILER_MSVC
#undef   O_NONBLOCK
#define  O_NONBLOCK 0
#endif
#if defined(PLATFORM_WINDOWS)
#include <winsock2.h>
#else
#include <netdb.h>
#endif

#if defined(PLATFORM_WINDOWS)
static WSADATA g_wsaData;
typedef int socklen_t;
#else
#include <unistd.h>
#include <sys/ioctl.h>
inline void closesocket(int s) { ::close(s); }
#endif

#include <jack/jack.h>
#include <jack/midiport.h>

#include "pbd/xml++.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/convert.h"
#include "pbd/compose.h"

#include "midi++/types.h"
#include "midi++/ipmidi_port.h"
#include "midi++/channel.h"

using namespace MIDI;
using namespace std;
using namespace PBD;

IPMIDIPort::IPMIDIPort (int base_port, const string& iface)
	: Port (string_compose ("IPmidi@%1", base_port), Port::Flags (Port::IsInput|Port::IsOutput))
	, sockin (-1)
	, sockout (-1)
{
	if (!open_sockets (base_port, iface)) {
		throw (failed_constructor ());
	}
}

IPMIDIPort::IPMIDIPort (const XMLNode& node)
	: Port (node)
{
	/* base class does not class set_state() */
	set_state (node);
}

IPMIDIPort::~IPMIDIPort ()
{
	close_sockets ();
}

int
IPMIDIPort::selectable () const
{ 
	return sockin; 
}

XMLNode&
IPMIDIPort::get_state () const
{
	return Port::get_state ();
}

void
IPMIDIPort::set_state (const XMLNode& node)
{
	Port::set_state (node);
}

void
IPMIDIPort::close_sockets ()
{
	if (sockin >= 0) {
		::closesocket (sockin);
		sockin = -1;
	}
	
	if (sockout >= 0) {
		::closesocket (sockout);
		sockout = -1;
	}
}

static bool 
get_address (int sock, struct in_addr *inaddr, const string& ifname )
{
	// Get interface address from supplied name.

#if !defined(PLATFORM_WINDOWS)
	struct ifreq ifr;
	::strncpy(ifr.ifr_name, ifname.c_str(), sizeof(ifr.ifr_name));

	if (::ioctl(sock, SIOCGIFFLAGS, (char *) &ifr)) {
		::perror("ioctl(SIOCGIFFLAGS)");
		return false;
	}

	if ((ifr.ifr_flags & IFF_UP) == 0) {
		error << string_compose ("interface %1 is down", ifname) << endmsg;
		return false;
	}

	if (::ioctl(sock, SIOCGIFADDR, (char *) &ifr)) {
		::perror("ioctl(SIOCGIFADDR)");
		return false;
	}

	struct sockaddr_in sa;
	::memcpy(&sa, &ifr.ifr_addr, sizeof(struct sockaddr_in));
	inaddr->s_addr = sa.sin_addr.s_addr;

	return true;

#else

	return false;

#endif	// !PLATFORM_WINDOWS'
}

bool
IPMIDIPort::open_sockets (int base_port, const string& ifname)
{
#if !defined(PLATFORM_WINDOWS)
	int protonum = 0;
	struct protoent *proto = ::getprotobyname("IP");

	if (proto) {
		protonum = proto->p_proto;
	}

	sockin = ::socket (PF_INET, SOCK_DGRAM, protonum);
	if (sockin < 0) {
		::perror("socket(in)");
		return false;
	}

	struct sockaddr_in addrin;
	::memset(&addrin, 0, sizeof(addrin));
	addrin.sin_family = AF_INET;
	addrin.sin_addr.s_addr = htonl(INADDR_ANY);
	addrin.sin_port = htons(base_port);
	
	if (::bind(sockin, (struct sockaddr *) (&addrin), sizeof(addrin)) < 0) {
		::perror("bind");
		return false;
	}
	
	// Will Hall, 2007
	// INADDR_ANY will bind to default interface,
	// specify alternate interface nameon which to bind...
	struct in_addr if_addr_in;
	if (!ifname.empty()) {
		if (!get_address(sockin, &if_addr_in, ifname)) {
			error << string_compose ("socket(in): could not find interface address for %1", ifname) << endmsg;
			return false;
		}
		if (::setsockopt(sockin, IPPROTO_IP, IP_MULTICAST_IF,
				 (char *) &if_addr_in, sizeof(if_addr_in))) {
			::perror("setsockopt(IP_MULTICAST_IF)");
			return false;
		}
	} else {
		if_addr_in.s_addr = htonl (INADDR_ANY);
	}
	
	struct ip_mreq mreq;
	mreq.imr_multiaddr.s_addr = ::inet_addr("225.0.0.37");
	mreq.imr_interface.s_addr = if_addr_in.s_addr;
	if(::setsockopt (sockin, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mreq, sizeof(mreq)) < 0) {
		::perror("setsockopt(IP_ADD_MEMBERSHIP)");
		fprintf(stderr, "socket(in): your kernel is probably missing multicast support.\n");
		return false;
	}

	// Output socket...

	sockout = ::socket (AF_INET, SOCK_DGRAM, protonum);

	if (sockout < 0) {
		::perror("socket(out)");
		return false;
	}
	
	// Will Hall, Oct 2007
	if (!ifname.empty()) {
		struct in_addr if_addr_out;
		if (!get_address(sockout, &if_addr_out, ifname)) {
			error << string_compose ("socket(out): could not find interface address for %1", ifname) << endmsg;
			return false;
		}
		if (::setsockopt(sockout, IPPROTO_IP, IP_MULTICAST_IF, (char *) &if_addr_out, sizeof(if_addr_out))) {
			::perror("setsockopt(IP_MULTICAST_IF)");
			return false;
		}
	}
	
	::memset(&addrout, 0, sizeof(struct sockaddr_in));
	addrout.sin_family = AF_INET;
	addrout.sin_addr.s_addr = ::inet_addr("225.0.0.37");
	addrout.sin_port = htons (base_port);
	
	// Turn off loopback...
	int loop = 0;
	if (::setsockopt(sockout, IPPROTO_IP, IP_MULTICAST_LOOP, (char *) &loop, sizeof (loop)) < 0) {
		::perror("setsockopt(IP_MULTICAST_LOOP)");
		return false;
	}

	if (fcntl (sockin, F_SETFL, O_NONBLOCK)) {
		error << "cannot set non-blocking mode for IP MIDI input socket (" << ::strerror (errno) << ')' << endmsg;
		return false;
	}

	if (fcntl (sockout, F_SETFL, O_NONBLOCK)) {
		error << "cannot set non-blocking mode for IP MIDI output socket (" << ::strerror (errno) << ')' << endmsg;
		return false;
	}
	
	return true;
#else
	return false;
#endif	// !PLATFORM_WINDOWS'
}

int
IPMIDIPort::write (const MIDI::byte* msg, size_t msglen, timestamp_t /* ignored */) {

	if (sockout) {
		Glib::Threads::Mutex::Lock lm (write_lock);
		if (::sendto (sockout, (const char *) msg, msglen, 0, (struct sockaddr *) &addrout, sizeof(struct sockaddr_in)) < 0) {
			::perror("sendto");
			return -1;
		}
		return msglen;
	}
	return 0;
}

int
IPMIDIPort::read (MIDI::byte* /*buf*/, size_t /*bufsize*/)
{
	/* nothing to do here - all handled by parse() */
	return 0;
}

void
IPMIDIPort::parse (framecnt_t timestamp)
{
	/* input was detected on the socket, so go get it and hand it to the
	 * parser. This will emit appropriate signals that will be handled
	 * by anyone who cares.
	 */
	
	unsigned char buf[1024];
	struct sockaddr_in sender;
	socklen_t slen = sizeof(sender);
	int r = ::recvfrom (sockin, (char *) buf, sizeof(buf), 0, (struct sockaddr *) &sender, &slen);

	if (r >= 0) {

		_parser->set_timestamp (timestamp);
		
		for (int i = 0; i < r; ++i) {
			_parser->scanner (buf[i]);
		}
	} else {
		::perror ("failed to recv from socket");
	}
}

