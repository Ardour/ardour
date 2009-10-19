/* This file is part of Evoral.
 * Copyright (C) 2008 Dave Robillard <http://drobilla.net>
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

#ifndef EVORAL_SEQUENCE_HPP
#define EVORAL_SEQUENCE_HPP

#include <vector>
#include <queue>
#include <deque>
#include <map>
#include <utility>
#include <boost/shared_ptr.hpp>
#include <glibmm/thread.h>
#include "evoral/types.hpp"
#include "evoral/Note.hpp"
#include "evoral/Parameter.hpp"
#include "evoral/ControlSet.hpp"
#include "evoral/ControlList.hpp"

namespace Evoral {

class TypeMap;
template<typename Time> class EventSink;
template<typename Time> class Note;
template<typename Time> class Event;

/** An iterator over (the x axis of) a 2-d double coordinate space.
 */
class ControlIterator {
public:
	ControlIterator(boost::shared_ptr<const ControlList> al, double ax, double ay)
		: list(al)
		, x(ax)
		, y(ay)
	{}

	boost::shared_ptr<const ControlList> list;
	double x;
	double y;
};


/** This is a higher level view of events, with separate representations for
 * notes (instead of just unassociated note on/off events) and controller data.
 * Controller data is represented as a list of time-stamped float values. */
template<typename Time>
class Sequence : virtual public ControlSet {
public:
	Sequence(const TypeMap& type_map, size_t size=0);

	void write_lock();
	void write_unlock();

	void read_lock()   const;
	void read_unlock() const;

	void clear();

	bool percussive() const     { return _percussive; }
	void set_percussive(bool p) { _percussive = p; }

	void start_write();
	bool writing() const { return _writing; }
	void end_write(bool delete_stuck=false);

	/** Resizes vector if necessary (NOT realtime safe) */
	void append(const Event<Time>& ev);

	inline const boost::shared_ptr< const Note<Time> > note_at(size_t i) const { return _notes[i]; }
	inline const boost::shared_ptr< Note<Time> >       note_at(size_t i)       { return _notes[i]; }

	inline size_t n_notes() const { return _notes.size(); }
	inline bool   empty()   const { return _notes.size() == 0 && ControlSet::controls_empty(); }

	inline static bool note_time_comparator(const boost::shared_ptr< const Note<Time> >& a,
	                                        const boost::shared_ptr< const Note<Time> >& b) {
		return a->time() < b->time();
	}

	struct LaterNoteComparator {
		typedef const Note<Time>* value_type;
		inline bool operator()(const boost::shared_ptr< const Note<Time> > a,
		                       const boost::shared_ptr< const Note<Time> > b) const {
			return a->time() > b->time();
		}
	};

	struct LaterNoteEndComparator {
		typedef const Note<Time>* value_type;
		inline bool operator()(const boost::shared_ptr< const Note<Time> > a,
		                       const boost::shared_ptr< const Note<Time> > b) const {
			return a->end_time() > b->end_time();
		}
	};

	typedef std::vector< boost::shared_ptr< Note<Time> > > Notes;
	inline       Notes& notes()       { return _notes; }
	inline const Notes& notes() const { return _notes; }

	void set_notes (const Sequence<Time>::Notes& n);

	typedef std::vector< boost::shared_ptr< Event<Time> > > SysExes;
	inline       SysExes& sysexes()       { return _sysexes; }
	inline const SysExes& sysexes() const { return _sysexes; }

private:
	typedef std::priority_queue< boost::shared_ptr< Note<Time> >,
	                             std::deque< boost::shared_ptr< Note<Time> > >,
	                             LaterNoteEndComparator >
	        ActiveNotes;
public:

	/** Read iterator */
	class const_iterator {
	public:
		const_iterator();
		const_iterator(const Sequence<Time>& seq, Time t);
		~const_iterator();

		inline bool valid() const { return !_is_end && _event; }
		inline bool locked() const { return _locked; }

		void invalidate();

		const Event<Time>& operator*()  const { return *_event;  }
		const boost::shared_ptr< Event<Time> > operator->() const  { return _event; }
		const boost::shared_ptr< Event<Time> > get_event_pointer() { return _event; }

		const const_iterator& operator++(); // prefix only

		bool operator==(const const_iterator& other) const;
		bool operator!=(const const_iterator& other) const { return ! operator==(other); }

		const_iterator& operator=(const const_iterator& other);

	private:
		friend class Sequence<Time>;

		typedef std::vector<ControlIterator> ControlIterators;
		enum MIDIMessageType { NIL, NOTE_ON, NOTE_OFF, CONTROL, SYSEX };

		const Sequence<Time>*            _seq;
		boost::shared_ptr< Event<Time> > _event;
		mutable ActiveNotes              _active_notes;
		MIDIMessageType                  _type;
		bool                             _is_end;
		bool                             _locked;
		typename Notes::const_iterator   _note_iter;
		typename SysExes::const_iterator _sysex_iter;
		ControlIterators                 _control_iters;
		ControlIterators::iterator       _control_iter;
	};

	const_iterator        begin(Time t=0) const { return const_iterator(*this, t); }
	const const_iterator& end()           const { return _end_iter; }

	bool control_to_midi_event(boost::shared_ptr< Event<Time> >& ev,
	                           const ControlIterator&            iter) const;

	bool edited() const      { return _edited; }
	void set_edited(bool yn) { _edited = yn; }

	void add_note_unlocked(const boost::shared_ptr< Note<Time> > note);
	void remove_note_unlocked(const boost::shared_ptr< const Note<Time> > note);

	uint8_t lowest_note()  const { return _lowest_note; }
	uint8_t highest_note() const { return _highest_note; }

protected:
	bool _edited;

private:
	friend class const_iterator;

	void append_note_on_unlocked(uint8_t chan, Time time, uint8_t note, uint8_t velocity);
	void append_note_off_unlocked(uint8_t chan, Time time, uint8_t note);
	void append_control_unlocked(const Parameter& param, Time time, double value);
	void append_sysex_unlocked(const MIDIEvent<Time>& ev);

	mutable Glib::RWLock _lock;

	const TypeMap& _type_map;

	Notes   _notes;
	SysExes _sysexes;

	typedef std::vector<size_t> WriteNotes;
	WriteNotes _write_notes[16];
	bool       _writing;

	typedef std::vector< boost::shared_ptr<const ControlList> > ControlLists;
	ControlLists _dirty_controls;

	const   const_iterator _end_iter;
	bool                   _percussive;

	uint8_t _lowest_note;
	uint8_t _highest_note;
};


} // namespace Evoral

#endif // EVORAL_SEQUENCE_HPP

