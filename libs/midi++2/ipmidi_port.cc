/*
 * Copyright (C) 2012-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
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
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif

#if defined(PLATFORM_WINDOWS)
typedef int socklen_t;
#else
#include <unistd.h>
#include <sys/ioctl.h>
inline void closesocket(int s) { ::close(s); }
#endif

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

#ifndef PLATFORM_WINDOWS
static bool
get_address (int sock, struct in_addr *inaddr, const string& ifname )
{
	// Get interface address from supplied name.

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
}
#endif

bool
IPMIDIPort::open_sockets (int base_port, const string& ifname)
{
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
#ifndef PLATFORM_WINDOWS
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
#else
	if_addr_in.s_addr = htonl (INADDR_ANY);
#endif

	struct ip_mreq mreq;
	mreq.imr_multiaddr.s_addr = ::inet_addr("225.0.0.37"); /* ipMIDI group multicast address */
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
#ifndef PLATFORM_WINDOWS
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
#endif

	::memset(&addrout, 0, sizeof(struct sockaddr_in));
	addrout.sin_family = AF_INET;
	addrout.sin_addr.s_addr = ::inet_addr("225.0.0.37");
	addrout.sin_port = htons (base_port);

	// Turn off loopback...
	int loop = 0;

#ifdef PLATFORM_WINDOWS

	/* https://msdn.microsoft.com/en-us/library/windows/desktop/ms739161%28v=vs.85%29.aspx
	 *
	 * ------------------------------------------------------------------------------
	 * Note The Winsock version of the IP_MULTICAST_LOOP option is
	 * semantically different than the UNIX version of the
	 * IP_MULTICAST_LOOP option:
	 *
	 * In Winsock, the IP_MULTICAST_LOOP option applies only to the receive path.
	 * In the UNIX version, the IP_MULTICAST_LOOP option applies to the send path.
	 *
	 * For example, applications ON and OFF (which are easier to track than
	 * X and Y) join the same group on the same interface; application ON
	 * sets the IP_MULTICAST_LOOP option on, application OFF sets the
	 * IP_MULTICAST_LOOP option off. If ON and OFF are Winsock
	 * applications, OFF can send to ON, but ON cannot sent to OFF. In
	 * contrast, if ON and OFF are UNIX applications, ON can send to OFF,
	 * but OFF cannot send to ON.
	 * ------------------------------------------------------------------------------
	 *
	 * Alles klar? Gut! 
	 */

	const int target_sock = sockin;
#else
	const int target_sock = sockout;
#endif

	if (::setsockopt (target_sock, IPPROTO_IP, IP_MULTICAST_LOOP, (char *) &loop, sizeof (loop)) < 0) {
		::perror("setsockopt(IP_MULTICAST_LOOP)");
		return false;
	}

#ifndef PLATFORM_WINDOWS

	if (fcntl (sockin, F_SETFL, O_NONBLOCK)) {
		error << "cannot set non-blocking mode for IP MIDI input socket (" << ::strerror (errno) << ')' << endmsg;
		return false;
	}

	if (fcntl (sockout, F_SETFL, O_NONBLOCK)) {
		error << "cannot set non-blocking mode for IP MIDI output socket (" << ::strerror (errno) << ')' << endmsg;
		return false;
	}

#else
	// If imode !=0, non-blocking mode is enabled.
	u_long mode=1;
	if (ioctlsocket(sockin,FIONBIO,&mode)) {
		error << "cannot set non-blocking mode for IP MIDI input socket (" << ::strerror (errno) << ')' << endmsg;
		return false;
	}
	mode = 1; /* just in case it was modified in the previous call */
	if (ioctlsocket(sockout,FIONBIO,&mode)) {
		error << "cannot set non-blocking mode for IP MIDI output socket (" << ::strerror (errno) << ')' << endmsg;
		return false;
	}
#endif

	return true;
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
IPMIDIPort::parse (samplecnt_t timestamp)
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
