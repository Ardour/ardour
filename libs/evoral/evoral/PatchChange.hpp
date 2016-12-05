/*
  Copyright (C) 2010 Paul Davis
  Author: Carl Hetherington <cth@carlh.net>

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

#ifndef EVORAL_PATCH_CHANGE_HPP
#define EVORAL_PATCH_CHANGE_HPP

#include <boost/intrusive/list.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

#include "evoral/Event.hpp"

#include "evoral/visibility.h"

namespace Evoral {

/** Event representing a `patch change', composed of a LSB and MSB
 *  bank select and then a program change.
 */
template<typename Time>
class /*LIBEVORAL_API*/ PatchChange : public MultiEvent<Time,3>
{
public:
	using MultiEvent<Time,3>::_events;   /* C++ template arcana */
	using MultiEvent<Time,3>::set_event; /* C++ template arcana */

	/** @param t Time.
	 *  @param c Channel.
	 *  @param p Program change number (counted from 0).
	 *  @param b Bank number (counted from 0, 14-bit).
	 */
	PatchChange (EventPool& pool, Time t, uint8_t c, uint8_t p, int b)
	{
		uint8_t buf[3];

		buf[0] = MIDI_CMD_CONTROL | c;
		buf[1] = MIDI_CTL_MSB_BANK;
		buf[2] = (b >> 7) & 0x7f;
		set_event (0, EventPointer<Time>::create (pool, MIDI_EVENT, t, 3, buf));

		buf[0] = MIDI_CMD_CONTROL | c;
		buf[1] = MIDI_CTL_LSB_BANK;
		buf[2] = b & 0x7f;
		set_event (1, EventPointer<Time>::create (pool, MIDI_EVENT, t, 3, buf));

		buf[0] = MIDI_CMD_PGM_CHANGE | c;
		buf[1] = p;
		set_event (2, EventPointer<Time>::create (pool, MIDI_EVENT, t, 2, buf));
	}

	uint8_t program () const {
		return _events[2]->pgm_number ();
	}

	void set_program (uint8_t p) {
		_events[2]->set_pgm_number (p);
	}

	int bank () const {
		return (bank_msb() << 7) | bank_lsb();
	}

	void set_bank (int b) {
		_events[0]->set_cc_value ((b >> 7) & 0x7f);
		_events[0]->set_cc_value (b & 0x7f);
	}

	uint8_t bank_msb () const {
		return _events[0]->cc_value ();
	}

	uint8_t bank_lsb () const {
		return _events[1]->cc_value ();
	}

	uint8_t channel () const { return _events[0]->channel (); }

	inline bool operator< (const PatchChange<Time>& o) const {
		if (this->time() != o.time()) {
			return this->time() < o.time();
		}

		if (bank() != o.bank()) {
			return bank() < o.bank();
		}

		return (program() < o.program());
	}

	inline bool operator== (const PatchChange<Time>& o) const {
		return (this->time() == o.time() && program() == o.program() && bank() == o.bank());
	}

	EventPointer<Time> & bank_msb_message () { return _events[0]; }
	EventPointer<Time> & bank_lsb_message () { return _events[1]; }
	EventPointer<Time> & program_message ()  { return _events[2]; }
};

template<typename Time>
class PatchChangePointer : public MultiEventPointer<PatchChange<Time> >
{
  public:
	PatchChangePointer () {}
	PatchChangePointer (PatchChange<Time>* pc) : MultiEventPointer<PatchChange<Time> > (pc) {}
	PatchChangePointer (EventPool& pool, Time t, uint8_t c, uint8_t p, int b)
		: MultiEventPointer<PatchChange<Time> > (new PatchChange<Time> (pool, t, c, p, b)) {}

	~PatchChangePointer () { }

	PatchChangePointer& copy() const {
		/* XXX need pools for all this */
		return *new PatchChangePointer<Time> (new PatchChange<Time> (*(this->get())));
	}

	/* patch change pointers need to be safely created and destroyed in realtime
	 * contexts
	 */

	void* operator new (size_t sz) {
		return pool.alloc (sz);
	}

	void operator delete (void* ptr) {
		pool.release (ptr);
	}

	static void init_pool (size_t num_pointers) {
		EventPool::SizePairs sp;
		sp.push_back (EventPool::SizePair (sizeof (PatchChangePointer<Time>), num_pointers));
		pool.add (sp);
	}

  private:
	static EventPool pool;
};

template<typename Time>
EventPool PatchChangePointer<Time>::pool ("patch change pointer");

}

template<typename Time>
/*LIBEVORAL_API*/ std::ostream& operator<< (std::ostream& o, const Evoral::PatchChange<Time>& p) {
	o << "Patch Change " << p.id() << " @ " << p.time() << " bank " << (int) p.bank() << " program " << (int) p.program();
	return o;
}

#endif
