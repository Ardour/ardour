/*
 * Copyright (C) 1998-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libmisc_receiver_h__
#define __libmisc_receiver_h__

#include <vector>

#include <sigc++/sigc++.h>

#include "pbd/libpbd_visibility.h"
#include "transmitter.h"

class strstream;

class LIBPBD_API Receiver : public sigc::trackable
{
  public:
	Receiver ();
	virtual ~Receiver ();

	void listen_to (Transmitter &);
	void hangup ();

  protected:
	virtual void receive (Transmitter::Channel, const char *) = 0;

  private:
	PBD::ScopedConnectionList connections;
};

#endif  // __libmisc_receiver_h__
