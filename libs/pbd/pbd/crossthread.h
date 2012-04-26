/*
    Copyright (C) 2000-2007 Paul Davis 

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

#ifndef __pbd__crossthread_h__
#define __pbd__crossthread_h__

#ifdef check
#undef check
#endif

#include <glibmm/main.h>

/** A simple abstraction of a mechanism of signalling one thread from another.
 * The signaller calls ::wakeup() to tell the signalled thread to check for
 * work to be done. 
 *
 * This implementation provides both ::selectable() for use in direct
 * poll/select-based event loops, and a Glib::IOSource via ::ios() for use
 * in Glib main loop based situations. 
 */

class CrossThreadChannel { 
  public:
	/** if @a non_blocking is true, the channel will not cause blocking
	 * when used in an event loop based on poll/select or the glib main
	 * loop.
	 */
	CrossThreadChannel(bool non_blocking);
	~CrossThreadChannel();
	
	/** Tell the listening thread that is has work to do.
	 */
	void wakeup();
	
	/* if the listening thread cares about the precise message
	 * it is being sent, then ::deliver() can be used to send
	 * a single byte message rather than a simple wakeup. These
	 * two mechanisms should not be used on the same CrossThreadChannel
	 * because there is no way to know which byte value will be used
	 * for ::wakeup()
	 */
        int deliver (char msg);

	/** if using ::deliver() to wakeup the listening thread, then
	 * the listener should call ::receive() to fetch the message
	 * type from the channel.
	 */
        int receive (char& msg);

	/** empty the channel of all requests.
	 * Typically this is done as soon as input 
	 * is noticed on the channel, because the
	 * handler will look at a separately managed work
	 * queue. The actual number of queued "wakeups"
	 * in the channel will not be important.
	 */
	void drain ();
	static void drain (int fd);

	/** File descriptor that can be used with poll/select to
	 * detect when wakeup() has been called on this channel.
	 * It be marked as readable/input-ready when this condition
	 * is true. It has already been marked non-blocking.
	 */
	int selectable() const { return fds[0]; }

	/* glibmm 2.22 and earlier has a terrifying bug that will
	   cause crashes whenever a Source is removed from
	   a MainContext (including the destruction of the MainContext),
	   because the Source is destroyed "out from under the nose of" 
	   the RefPtr. I (Paul) have fixed this (https://bugzilla.gnome.org/show_bug.cgi?id=561885)
	   but in the meantime, we need a hack to get around the issue.
	*/
	Glib::RefPtr<Glib::IOSource> ios();
	void drop_ios ();

	/** returns true if the CrossThreadChannel was
	 * correctly constructed.
	 */
	bool ok() const { return fds[0] >= 0 && fds[1] >= 0; }

  private:
	Glib::RefPtr<Glib::IOSource>* _ios; // lazily constructed
	int fds[2]; // current implementation uses a pipe/fifo
};

#endif /* __pbd__crossthread_h__ */
