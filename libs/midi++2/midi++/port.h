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

#ifndef  __libmidi_port_base_h__
#define  __libmidi_port_base_h__

#include <string>
#include <iostream>

#include <pthread.h>

#include "pbd/xml++.h"
#ifndef PLATFORM_WINDOWS
#include "pbd/crossthread.h"
#endif
#include "pbd/signals.h"
#include "pbd/ringbuffer.h"

#include "midi++/libmidi_visibility.h"
#include "midi++/types.h"
#include "midi++/parser.h"

namespace MIDI {

class Channel;
class PortRequest;

class LIBMIDIPP_API Port {
  public:
	enum Flags {
		IsInput = 0x1,  /* MUST MATCH JACK's JackPortIsInput */
		IsOutput = 0x2, /* MUST MATCH JACK's JackPortIsOutput */
	};

	Port (std::string const &, Flags);
	Port (const XMLNode&);
	virtual ~Port ();

	virtual XMLNode& get_state () const;
	virtual void set_state (const XMLNode&);

	/** Write a message to port.
	 * @param msg Raw MIDI message to send
	 * @param msglen Size of @a msg
	 * @param timestamp Time stamp in samples of this message (relative to cycle start)
	 * @return number of bytes successfully written
	 */
	virtual int write (const byte *msg, size_t msglen, timestamp_t timestamp) = 0;

	/** Read raw bytes from a port.
	 * @param buf memory to store read data in
	 * @param bufsize size of @a buf
	 * @return number of bytes successfully read, negative if error
	 */
	virtual int read (byte *buf, size_t bufsize) = 0;

	/** block until the output FIFO used by non-process threads
	 * is empty, checking every @a check_interval_usecs usecs
	 * for current status. Not to be called by a thread that
	 * executes any part of a JACK process callback (will
	 * simply return immediately in that situation).
	 */
	virtual void drain (int /* check_interval_usecs */, int /* total_usecs_to_wait */) {}

	/** Write a message to port.
	 * @return true on success.
	 * FIXME: describe semantics here
	 */
	int midimsg (byte *msg, size_t len, timestamp_t timestamp) {
		return !(write (msg, len, timestamp) == (int) len);
	}

	virtual void parse (samplecnt_t timestamp) = 0;

	bool clock (timestamp_t timestamp);

	/* select(2)/poll(2)-based I/O */

	/** Get the file descriptor for port.
	 * @return File descriptor, or -1 if not selectable.
	 */
	virtual int selectable () const = 0;

	Channel *channel (channel_t chn) {
		return _channel[chn&0x7F];
	}

	Parser* parser () {
		return _parser;
	}

	const char *name () const   { return _tagname.c_str(); }
	bool   ok ()   const        { return _ok; }

	virtual bool centrally_parsed() const;
	void set_centrally_parsed (bool yn) { _centrally_parsed = yn; }

	bool receives_input () const {
		return _flags == IsInput;
	}

	bool sends_output () const {
		return _flags == IsOutput;
	}

	struct Descriptor {
	    std::string tag;
	    Flags flags;

	    Descriptor (const XMLNode&);
	    XMLNode& get_state();
	};

	static std::string state_node_name;

  protected:
	bool              _ok;
	std::string       _tagname;
	Channel*          _channel[16];
	Parser*           _parser;
	Flags             _flags;
	bool              _centrally_parsed;

	void init (std::string const &, Flags);
};

struct LIBMIDIPP_API PortSet {
    PortSet (std::string str) : owner (str) { }

    std::string owner;
    std::list<XMLNode> ports;
};

std::ostream & operator << (std::ostream& os, const Port& port);

} // namespace MIDI

#endif // __libmidi_port_base_h__
