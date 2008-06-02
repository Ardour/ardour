/*
    Copyright (C) 1998-99 Paul Barton-Davis 
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

#ifndef  __libmidi_port_h__
#define  __libmidi_port_h__

#include <string>
#include <iostream>

#include <sigc++/sigc++.h>
#include <pbd/xml++.h>

#include <midi++/types.h>
#include <midi++/parser.h>

namespace MIDI {

class Channel;
class PortRequest;

class Port : public sigc::trackable {
  public:
	enum Type {
		Unknown,
		JACK_Midi,
		ALSA_RawMidi,
		ALSA_Sequencer,
		CoreMidi_MidiPort,
		Null,
		FIFO
	};


	Port (const XMLNode&);
	virtual ~Port ();

	virtual XMLNode& get_state () const;
	virtual void set_state (const XMLNode&);

	// FIXME: make Manager a friend of port so these can be hidden?

	/* Only for use by MidiManager.  Don't ever call this. */
	virtual void cycle_start(nframes_t nframes);
	/* Only for use by MidiManager.  Don't ever call this. */
	virtual void cycle_end();

	/** Write a message to port.
	 * @param msg Raw MIDI message to send
	 * @param msglen Size of @a msg
	 * @param timestamp Time stamp in frames of this message (relative to cycle start)
	 * @return number of bytes successfully written
	 */
	virtual int write (byte *msg, size_t msglen, timestamp_t timestamp) = 0;	

	/** Read raw bytes from a port.
	 * @param buf memory to store read data in
	 * @param bufsize size of @a buf
	 * @return number of bytes successfully read, negative if error
	 */
	virtual int read (byte *buf, size_t bufsize) = 0;

	void parse ();
	
	/** Write a message to port.
	 * @return true on success.
	 * FIXME: describe semantics here
	 */
	int midimsg (byte *msg, size_t len, timestamp_t timestamp) {
		return !(write (msg, len, timestamp) == (int) len);
	} 

	int three_byte_msg (byte a, byte b, byte c, timestamp_t timestamp) {
		byte msg[3];

            	msg[0] = a;
		msg[1] = b;
		msg[2] = c;

		return !(write (msg, 3, timestamp) == 3);
	} 
	
	bool clock (timestamp_t timestamp);
	
	/* slowdown i/o to a loop of single byte emissions
	   interspersed with a busy loop of 10000 * this value.

	   This may be ignored by a particular instance
	   of this virtual class. See FD_MidiPort for an 
	   example of where it used.  
	*/

	void set_slowdown (size_t n) { slowdown = n; }

	/* select(2)/poll(2)-based I/O */

	/** Get the file descriptor for port.
	 * @return File descriptor, or -1 if not selectable. 
	 */
	virtual int selectable() const = 0;

	static void gtk_read_callback (void *ptr, int fd, int cond);
	static void write_callback (byte *msg, unsigned int len, void *);
	
	Channel *channel (channel_t chn) { 
		return _channel[chn&0x7F];
	}
	
	Parser *input()     { return input_parser; }
	Parser *output()    { return output_parser; }

	void iostat (int *written, int *read, 
		     const size_t **in_counts,
		     const size_t **out_counts) {

		*written = bytes_written;
		*read = bytes_read;
		if (input_parser) {
			*in_counts = input_parser->message_counts();
		} else {
			*in_counts = 0;
		}
		if (output_parser) {
			*out_counts = output_parser->message_counts();
		} else {
			*out_counts = 0;
		}
	}
	
	const char *device () const { return _devname.c_str(); }
	const char *name () const   { return _tagname.c_str(); }
	Type   type () const        { return _type; }
	int    mode () const        { return _mode; }
	bool   ok ()   const        { return _ok; }

	struct Descriptor {
	    std::string tag;
	    std::string device;
	    int mode;
	    Port::Type type;

	    Descriptor (const XMLNode&);
	    XMLNode& get_state();
	};

  protected:
	bool             _ok;
	bool             _currently_in_cycle;
	nframes_t        _nframes_this_cycle;
	Type             _type;
	std::string      _devname;
	std::string      _tagname;
	int              _mode;
	size_t           _number;
	Channel          *_channel[16];
	sigc::connection thru_connection;
	unsigned int     bytes_written;
	unsigned int     bytes_read;
	Parser           *input_parser;
	Parser           *output_parser;
	size_t           slowdown;

	virtual std::string get_typestring () const = 0;

  private:
	static size_t nports;
};

struct PortSet {
    PortSet (std::string str) : owner (str) { }
    
    std::string owner;
    std::list<XMLNode> ports;
};

std::ostream & operator << ( std::ostream & os, const Port & port );

} // namespace MIDI

#endif // __libmidi_port_h__
