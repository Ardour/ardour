/* This file is part of Evoral.
 * Copyright (C) 2008-2016 David Robillard <http://drobilla.net>
 * Copyright (C) 2000-2008 Paul Davis
 *
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Evoral is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef EVORAL_EVENT_HPP
#define EVORAL_EVENT_HPP

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <new>
#include <stdint.h>

#include <boost/intrusive_ptr.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

#include "evoral/EventPool.h"
#include "evoral/midi_events.h"
#include "evoral/midi_util.h"
#include "evoral/types.hpp"
#include "evoral/visibility.h"

namespace Evoral {

LIBEVORAL_API event_id_t event_id_counter();
LIBEVORAL_API event_id_t next_event_id();
LIBEVORAL_API void       init_event_id_counter(event_id_t n);

template<typename T> class Sequence;

/** An event.
 *
 * Template parameter Time is the type of the time stamp used for this event.
 *
 * This is not POD: the structure uses the C-style zero-sized array hack to
 * allow us to place a contiguous data block immediately after the Event
 * structure.
 *
 * This object is otherwise intended to be POD: DO NOT ADD VIRTUAL METHODS.
 */
template<typename Time>
class LIBEVORAL_API Event {
public:
	Event (EventType ty, Time tm, size_t sz, uint8_t const * data, event_id_t id = -1)
		: _type (ty)
		, _time (tm)
		, _size (sz)
		, _id (id)
	{
		if (data) {
			/* specs for memcpy(2) state that passing size == zero is completely legal */
			memcpy (_buf, data, sz);
		} else {
			/* hopefully data will be written in shortly */
			memset (_buf, 0, sz);
		}
	}

	Event (Event const & other)
		: _type (other.event_type())
		, _time (other.time())
		, _size (other.size())
		, _id (next_event_id())
	{
		memcpy (_buf, other.buffer(), _size);
	}

	Event& operator= (Event const & other) {
		/* Does NOT copy event ID */
		if (this != &other) {
			_type = other._type;
			_time = other._time;
			_size = other._size;
			memcpy (_buf, other._buf, _size);
		}
		return *this;
	}

	bool operator== (Event const & other) const {
		/* Does NOT compare event IDs */
		return _type == other._type &&
			_time == other._time &&
			_size == other._size &&
			!memcmp (_buf, other._buf, _size);
	}

	inline EventType      event_type()    const { return _type; }
	inline Time           time()          const { return _time; }
	inline uint32_t       size()          const { return _size; }
	inline void           set_size (uint32_t s) { _size = s; /* CAREFUL !!! */ }
	inline uint32_t       object_size()   const { return size() + sizeof (*this); }
	inline const uint8_t* buffer()        const { return _buf; }
	inline uint8_t*       buffer()              { return _buf; }

	inline void set_event_type(EventType t) { _type = t; }

	inline void set_time(Time t) { _time = t; }

	inline event_id_t id() const           { return _id; }
	inline void       set_id(event_id_t n) { _id = n; }

	/* The following methods are type specific and only make sense for the
	   correct event type.  It is the caller's responsibility to only call
	   methods which make sense for the given event type.  Currently this means
	   they all only make sense for MIDI, but built-in support may be added for
	   other protocols in the future, or the internal representation may change
	   to be protocol agnostic. */

	uint8_t  type()                const { return midi_type (_buf); }
	uint8_t  channel()             const { return midi_channel (_buf); }
	bool     is_channel_msg()      const { return midi_is_channel_msg (_buf); }
	bool     is_note_on()          const { return midi_is_note_on (_buf); }
	bool     is_note_off()         const { return midi_is_note_off (_buf); }
	bool     is_note()             const { return midi_is_note (_buf); }
	bool     is_poly_pressure()    const { return midi_is_poly_pressure (_buf); }
	bool     is_channel_pressure() const { return midi_is_channel_pressure (_buf); }
	bool     is_cc()               const { return midi_is_cc (_buf); }
	bool     is_pgm_change()       const { return midi_is_pgm_change (_buf); }
	bool     is_pitch_bender()     const { return midi_is_pitch_bender (_buf); }
	bool     is_channel_event()    const { return midi_is_channel_event (_buf); }
	bool     is_smf_meta_event()   const { return midi_is_smf_meta_event (_buf); }
	bool     is_sysex()            const { return midi_is_sysex (_buf); }
	bool     is_spp()              const { return midi_is_spp (_buf, _size); }
	bool     is_mtc_quarter()      const { return midi_is_mtc_quarter (_buf, _size); }
	bool     is_mtc_full()         const { return midi_is_mtc_full (_buf, _size); }

	uint8_t  note()               const { return midi_note (_buf); }
	uint8_t  velocity()           const { return midi_velocity (_buf); }
	uint8_t  poly_note()          const { return midi_poly_note (_buf); }
	uint8_t  poly_pressure()      const { return midi_poly_pressure (_buf); }
	uint8_t  channel_pressure()   const { return midi_channel_pressure (_buf); }
	uint8_t  cc_number()          const { return midi_cc_number (_buf); }
	uint8_t  cc_value()           const { return midi_cc_value (_buf); }
	uint8_t  pgm_number()         const { return midi_pgm_number (_buf); }
	uint8_t  pitch_bender_lsb()   const { return midi_pitch_bender_lsb (_buf); }
	uint8_t  pitch_bender_msb()   const { return midi_pitch_bender_msb (_buf); }
	uint16_t pitch_bender_value() const { return midi_pitch_bender_value (_buf); }

	void set_channel(uint8_t channel)  { _buf[0] = (0xF0 & _buf[0]) | (0x0F & channel); }
	void set_type(uint8_t type)        { _buf[0] = (0x0F & _buf[0]) | (0xF0 & type); }
	void set_note(uint8_t num)         { _buf[1] = num; }
	void set_velocity(uint8_t val)     { _buf[2] = val; }
	void set_cc_number(uint8_t num)    { _buf[1] = num; }
	void set_cc_value(uint8_t val)     { _buf[2] = val; }
	void set_pgm_number(uint8_t num)   { _buf[1] = num; }

	uint16_t value() const {
		switch (type()) {
		case MIDI_CMD_CONTROL:
			return cc_value();
		case MIDI_CMD_BENDER:
			return pitch_bender_value();
		case MIDI_CMD_NOTE_PRESSURE:
			return poly_pressure();
		case MIDI_CMD_CHANNEL_PRESSURE:
			return channel_pressure();
		default:
			return 0;
		}
	}

	inline bool time_order_before (Event const & other) const {
		if (_time < other.time()) {
			return true;
		} else if (_time > other.time()) {
			return false;
		}

		if (_type != MIDI_EVENT) {
			/* times are equal, sort order is arbitrary */
			return false;
		}

		/* times are equal. Use MIDI semantics */

		bool other_first = false;

		/* two events at identical times. we need to determine
		   the order in which they should occur.

		   the rule is:

		   Controller messages
		   Program Change
		   Note Off
		   Note On
		   Note Pressure
		   Channel Pressure
		   Pitch Bend
		*/

		if (!is_channel_msg() || !other.is_channel_msg() || (channel() != other.channel())) {

			/* if either message is not a channel message, or if the channels are
			 * different, we don't care about the type.
			 */

			other_first = true;

		} else {

			switch (other.type()) {
			case MIDI_CMD_CONTROL:
				other_first = true;
				break;

			case MIDI_CMD_PGM_CHANGE:
				switch (type()) {
				case MIDI_CMD_CONTROL:
					break;
				case MIDI_CMD_PGM_CHANGE:
				case MIDI_CMD_NOTE_OFF:
				case MIDI_CMD_NOTE_ON:
				case MIDI_CMD_NOTE_PRESSURE:
				case MIDI_CMD_CHANNEL_PRESSURE:
				case MIDI_CMD_BENDER:
					other_first = true;
				}
				break;

			case MIDI_CMD_NOTE_OFF:
				switch (type()) {
				case MIDI_CMD_CONTROL:
				case MIDI_CMD_PGM_CHANGE:
					break;
				case MIDI_CMD_NOTE_OFF:
				case MIDI_CMD_NOTE_ON:
				case MIDI_CMD_NOTE_PRESSURE:
				case MIDI_CMD_CHANNEL_PRESSURE:
				case MIDI_CMD_BENDER:
					other_first = true;
				}
				break;

			case MIDI_CMD_NOTE_ON:
				switch (type()) {
				case MIDI_CMD_CONTROL:
				case MIDI_CMD_PGM_CHANGE:
				case MIDI_CMD_NOTE_OFF:
					break;
				case MIDI_CMD_NOTE_ON:
				case MIDI_CMD_NOTE_PRESSURE:
				case MIDI_CMD_CHANNEL_PRESSURE:
				case MIDI_CMD_BENDER:
					other_first = true;
				}
				break;
			case MIDI_CMD_NOTE_PRESSURE:
				switch (type()) {
				case MIDI_CMD_CONTROL:
				case MIDI_CMD_PGM_CHANGE:
				case MIDI_CMD_NOTE_OFF:
				case MIDI_CMD_NOTE_ON:
					break;
				case MIDI_CMD_NOTE_PRESSURE:
				case MIDI_CMD_CHANNEL_PRESSURE:
				case MIDI_CMD_BENDER:
					other_first = true;
				}
				break;

			case MIDI_CMD_CHANNEL_PRESSURE:
				switch (type()) {
				case MIDI_CMD_CONTROL:
				case MIDI_CMD_PGM_CHANGE:
				case MIDI_CMD_NOTE_OFF:
				case MIDI_CMD_NOTE_ON:
				case MIDI_CMD_NOTE_PRESSURE:
					break;
				case MIDI_CMD_CHANNEL_PRESSURE:
				case MIDI_CMD_BENDER:
					other_first = true;
				}
				break;
			case MIDI_CMD_BENDER:
				switch (type()) {
				case MIDI_CMD_CONTROL:
				case MIDI_CMD_PGM_CHANGE:
				case MIDI_CMD_NOTE_OFF:
				case MIDI_CMD_NOTE_ON:
				case MIDI_CMD_NOTE_PRESSURE:
				case MIDI_CMD_CHANNEL_PRESSURE:
					break;
				case MIDI_CMD_BENDER:
					other_first = true;
				}
				break;
			}
		}

		return other_first;
	}

  protected:
	EventType  _type;   ///< Type of event (application relative, NOT MIDI 'type')
	Time       _time;   ///< Time stamp of event
	uint32_t   _size;   ///< Size of buffer in bytes
	event_id_t _id;     ///< Unique event ID
	uint8_t    _buf[0]; ///< Event data. Must be at end, to use C-style variable-sized structure hack

  private:
	/* hide these methods since they are illegal. Event<T> can only be
	 * allocated using operator new (size_t, void*) (i.e. placement new).
	 * to ensure that the _buf member sits at the beginning of _size bytes
	 * of space.
	 */

	void* operator new (size_t) { ::abort(); }
	void* operator new[] (size_t) { ::abort(); }
};

template<typename Time>
/*LIBEVORAL_API*/ std::ostream& operator<<(std::ostream& o, const Evoral::Event<Time>& ev) {
	o << "Event #" << ev.id() << " type = " << ev.event_type() << " @ " << ev.time();
	o << std::hex;
	for (uint32_t n = 0; n < ev.size(); ++n) {
		o << ' ' << (int) ev.buffer()[n];
	}
	o << std::dec;
	return o;
}

/** An reference-counted version of an Event, to be used in contexts where
 * the event is shared between various contexts (e.g. editing of a Sequence)
 *
 * Inheritance order is based on the assumption that the ref-counter will
 * likely be accessed more frequently than any actual data elements in
 * Event. Could be wrong and probably needs measuring. But also based on the
 * fact that the Event<T> needs to be at the end in order for the zero-size
 * array hack to work.
 */
template<typename Time>
class LIBEVORAL_API ManagedEvent : public boost::intrusive_ref_counter<ManagedEvent<Time> >,
                                   public PoolAllocated,
                                   public boost::noncopyable,
                                   public Event<Time>
{
  public:
	static uint32_t memory_size (uint32_t size) { return sizeof (ManagedEvent<Time>) + size; }
	static EventPool default_event_pool;

	/* No public constructors because these objects must:
	 *
	 * (a) use placement operator new() to place the Event at an address
	 * pointing to a sufficiently large area of memory.
	 *
	 * (b) use an realtime-safe allocator to ensure that we can freely
	 * create and destroy these events in a realtime context.
	 */

	static ManagedEvent<Time>* create (EventPool& p, EventType ty, Time tm, size_t sz, uint8_t* data, event_id_t id = -1) {
		return ::new (p.alloc (memory_size (sz))) ManagedEvent<Time> (p, ty, tm, sz, data, id);
	}

	static ManagedEvent<Time>* create (EventPool& p, Event<Time> const & ev) {
		return ::new (p.alloc (memory_size (ev.size()))) ManagedEvent<Time> (p, ev);
	}

	static ManagedEvent<Time>* create (EventType ty, Time tm, size_t sz, uint8_t* data, event_id_t id = -1) {
		return ::new (default_event_pool.alloc (memory_size (sz))) ManagedEvent<Time> (default_event_pool, ty, tm, sz, data, id);
	}

	static ManagedEvent<Time>* create (Event<Time> const & ev) {
		return ::new (default_event_pool.alloc (memory_size (ev.size()))) ManagedEvent<Time> (default_event_pool, ev);
	}

  private:
	ManagedEvent (EventPool& p, EventType ty, Time tm, size_t sz, uint8_t* data, event_id_t id = -1)
		: PoolAllocated (&p)
		, Event<Time> (ty, tm, sz, data, id)
	{}

	ManagedEvent (EventPool& p, Event<Time> const & ev)
		: PoolAllocated (&p)
		, Event<Time> (ev)
	{}

	ManagedEvent (EventType ty, Time tm, size_t sz, uint8_t* data, event_id_t id = -1)
		: PoolAllocated (0)
		, Event<Time> (ty, tm, sz, data, id)
	{}

	ManagedEvent (Event<Time> const & ev)
		: PoolAllocated (0)
		, Event<Time> (ev)
	{}
};

/* A ref-counted pointer to an Event that can be placed inside an intrusive
 * (doubly-linked) list. This is what most things that manipulate Events in
 * "complex" or "smart" data structures should use at all times as the handle
 * on an Event.
 *
 * Note that these are extremely lightweight ("flyweight") objects. They
 * contain a pointer to the ManagedEvent, doubly-linked list pointers and
 * nothing more. Their constructor and destructor modifies the refcnt
 * of the ManagedEvent that they reference.
 *
 * This can only exist in a single intrusive list at one time.
 */
template<typename Time>
struct LIBEVORAL_API EventPointer : public boost::intrusive_ptr<ManagedEvent<Time > >,
                                    public boost::intrusive::list_base_hook<>
{
	EventPointer () {}
	EventPointer (ManagedEvent<Time> * ev) : boost::intrusive_ptr<ManagedEvent<Time> > (ev) { }

	/* convenience static factory method to hide the need to call a
	   ManagedEvent factory method with the same parameters.
	*/

	static EventPointer create (EventPool& pool, EventType type, Time tm, uint8_t sz, uint8_t* data, event_id_t evid = -1) {
		return EventPointer<Time> (ManagedEvent<Time>::create (pool, type, tm, sz, data, evid));
	}

	static EventPointer create (EventType type, Time tm, uint8_t sz, uint8_t* data, event_id_t evid = -1) {
		return EventPointer<Time> (ManagedEvent<Time>::create (type, tm, sz, data, evid));
	}

	/* event pointers need to be safely created and destroyed in realtime
	 * contexts
	 */

	void* operator new (size_t sz) {
		return pool.alloc (sz);
	}

	void operator delete (void* ptr) {
		pool.release (ptr);
	}

  private:
	static EventPool pool;
};

/** Base class for objects that reference several events to provide a useful
 * abstraction. See Note and PatchChange objects for examples of use.
 */
template<typename Time, size_t _num_events>
struct MultiEvent : public boost::intrusive_ref_counter<MultiEvent<Time,_num_events>,boost::thread_safe_counter>
{
   public:
	MultiEvent () {}
	virtual ~MultiEvent() {}

	size_t num_events() const { return _num_events; }

	EventPointer<Time> event (size_t n) const {
		assert (n < _num_events);
		return _events[n];
	}

	void set_event (size_t n, EventPointer<Time> const & ep) {
		assert (n < _num_events);
		_events[n] = ep;
	}

	event_id_t id () const {
		return _events[0]->id ();
	}


	void set_id (event_id_t id) {
		for (size_t n = 0; n < _num_events; ++n) {
			_events[n]->set_id (id);
		}
	}

	Time time () const {
		return _events[0]->time ();
	}

	void set_time (Time t) {
		for (size_t n = 0; n < _num_events; ++n) {
			_events[n]->set_time (t);
		}
	}

	uint8_t channel() const {
		return _events[0]->channel();
	}

	void set_channel (uint8_t c) {
		for (size_t n = 0; n < _num_events; ++n) {
			_events[n]->set_channel (c);
		}
	}

   protected:
	EventPointer<Time> _events[_num_events];
};

/* Base class for an intrusive (smart) pointer that references a
 * MultiEventObject.
 *
 * This can only exist in a single intrusive list at one time, but like
 * EventPointer<T>, it is a fly weight object that can be copied cheaply.
 */
template<typename MultiEventObject>
struct MultiEventPointer : public boost::intrusive_ptr<MultiEventObject>,
                           public boost::intrusive::list_base_hook<>
{
	MultiEventPointer () {}
	MultiEventPointer (MultiEventObject* meo) : boost::intrusive_ptr<MultiEventObject> (meo) { }

	~MultiEventPointer () { }
};

/* A comparison functor that orders Events by time, and for Events with
 * identical times, by important data comparison semantics.
 */
template<typename Time>
struct LIBEVORAL_API EventTimeComparator {
	inline bool operator()(EventPointer<Time> const & a, EventPointer<Time> const & b) const {
		return a->time_order_before (*b);
	}
};

} // namespace Evoral

#endif // EVORAL_EVENT_HPP
