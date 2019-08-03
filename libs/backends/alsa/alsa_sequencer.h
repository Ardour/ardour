/*
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
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

#ifndef __libbackend_alsa_sequencer_h__
#define __libbackend_alsa_sequencer_h__

#include <stdint.h>
#include <poll.h>
#include <pthread.h>

#include <alsa/asoundlib.h>

#include "pbd/ringbuffer.h"
#include "ardour/types.h"
#include "alsa_midi.h"

namespace ARDOUR {

class AlsaSeqMidiIO : virtual public AlsaMidiIO {
public:
	AlsaSeqMidiIO (const std::string &name, const char *port_name, const bool input);
	virtual ~AlsaSeqMidiIO ();

protected:
	snd_seq_t *_seq;
	int _port;

private:
	void init (const char *device_name, const bool input);
};

class AlsaSeqMidiOut : public AlsaSeqMidiIO, public AlsaMidiOut
{
public:
	AlsaSeqMidiOut (const std::string &name, const char *port_name);
	void* main_process_thread ();
};

class AlsaSeqMidiIn : public AlsaSeqMidiIO, public AlsaMidiIn
{
public:
	AlsaSeqMidiIn (const std::string &name, const char *port_name);

	void* main_process_thread ();
};

} // namespace

#endif
