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

#ifndef __libmisc_receiver_h__
#define __libmisc_receiver_h__

#include <vector>

#include <sigc++/sigc++.h>

#include "transmitter.h"

class strstream;

class Receiver : public sigc::trackable
{
  public:
	Receiver ();
	virtual ~Receiver ();
	
	void listen_to (Transmitter &);
	void hangup ();

  protected:
	virtual void receive (Transmitter::Channel, const char *) = 0;

  private:
	std::vector<sigc::connection *> connections;
};

#endif  // __libmisc_receiver_h__
