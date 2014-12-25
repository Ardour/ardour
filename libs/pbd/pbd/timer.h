/*
    Copyright (C) 2014 Tim Mayberry

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

#ifndef __libpbd_timer_h__
#define __libpbd_timer_h__

#include <sigc++/signal.h>

#include <glibmm/main.h>

#include "pbd/libpbd_visibility.h"

namespace PBD {

/**
 * The Timer class is a wrapper around Glib TimeoutSources
 * The Timer will start automatically when the first connection
 * is made and stop when the last callback is disconnected.
 */
class LIBPBD_API Timer
{
public:

	Timer (unsigned int interval,
	       const Glib::RefPtr<Glib::MainContext>& main_context);

	unsigned int get_interval () const;

	void set_interval (unsigned int new_interval);

	virtual unsigned int connection_count () const = 0;

protected:

	virtual ~Timer() { }

	void start ();

	void stop ();

	virtual bool on_elapsed () = 0;

private:

	Timer(const Timer&);
	Timer& operator= (const Timer&);

private:

	static gboolean _timeout_handler (void *data);

	bool timeout_handler ();

	GSource*                               m_timeout_source;

	unsigned int                           m_timeout_interval;

	const Glib::RefPtr<Glib::MainContext>  m_main_context;

};

class LIBPBD_API StandardTimer : public Timer
{
public:

	StandardTimer (unsigned int interval,
	               const Glib::RefPtr<Glib::MainContext>& main_context = Glib::MainContext::get_default());

	sigc::connection connect (const sigc::slot<void>& slot);

	virtual unsigned int connection_count () const
	{ return m_signal.size (); }

protected:

	virtual bool on_elapsed ();

	sigc::signal<void>                  m_signal;

};

class LIBPBD_API BlinkTimer : public Timer
{
public:

	BlinkTimer (unsigned int interval,
	            const Glib::RefPtr<Glib::MainContext>& main_context = Glib::MainContext::get_default());


	sigc::connection connect (const sigc::slot<void, bool>& slot);

	virtual unsigned int connection_count () const
	{ return m_blink_signal.size (); }

protected:

	virtual bool on_elapsed ();

	sigc::signal<void, bool> m_blink_signal;

};

} // namespace PBD

#endif // __libpbd_timer_h__
