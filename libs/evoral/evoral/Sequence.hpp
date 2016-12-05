/* This file is part of Evoral.
 * Copyright (C) 2008 David Robillard <http://drobilla.net>
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
#include <set>
#include <list>
#include <utility>
#include <boost/shared_ptr.hpp>
#include <glibmm/threads.h>

#include "evoral/visibility.h"
#include "evoral/EventPool.h"
#include "evoral/Note.hpp"
#include "evoral/ControlSet.hpp"
#include "evoral/ControlList.hpp"
#include "evoral/PatchChange.hpp"

namespace Evoral {

class Parameter;
class TypeMap;
template<typename Time> class EventSink;
template<typename Time> class Note;
template<typename Time> class Event;

/** An iterator over (the x axis of) a 2-d double coordinate space.
 */
class /*LIBEVORAL_API*/ ControlIterator {
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

template<typename T, typename Time>
struct TimeComparator
{
	inline bool operator() (T const & a, T const & b) const {
		return a->time() < b->time();
	}
	inline bool operator() (T const & thing, Time t) const {
		return thing->time() < t;
	}
	inline bool operator() (Time a, Time b) const {
		return a < b;
	}
};

/** This is a higher level view of events, with separate representations for
 * notes (instead of just unassociated note on/off events), patch changes (a
 * trio of events) and an additional representation of controller data as a
 * list of time-stamped float values (inherited from ControlSet).
 */
template<typename Time>
class LIBEVORAL_API Sequence : virtual public ControlSet {
public:
	Sequence(const TypeMap& type_map, EventPool* pool = 0);
	Sequence(const Sequence<Time>& other, EventPool* pool = 0);

protected:
	struct WriteLockImpl {
		WriteLockImpl(Glib::Threads::RWLock& s, Glib::Threads::Mutex& c)
			: sequence_lock(new Glib::Threads::RWLock::WriterLock(s))
			, control_lock(new Glib::Threads::Mutex::Lock(c)) { }
		~WriteLockImpl() {
			delete sequence_lock;
			delete control_lock;
		}
		Glib::Threads::RWLock::WriterLock* sequence_lock;
		Glib::Threads::Mutex::Lock*        control_lock;
	};

public:
	EventPool& event_pool() const { return *_event_pool; }

	void add_event (EventType type, Time tm, uint8_t sz, uint8_t* data, event_id_t evid = -1);
	void add_event (EventPointer<Time> & ev);

	void remove_event (EventPointer<Time> const & ev);

	typedef EventPointer<Time> EventPtr;

	typedef boost::intrusive::list<EventPtr > EventsByTime;
	typedef typename EventsByTime::iterator iterator;
	typedef typename EventsByTime::const_iterator const_iterator;

	const_iterator first_event_at_or_after (Time t) const {
		return std::lower_bound (_events.begin(), _events.end(), t, TimeComparator<EventPtr,Time>());
	}

	bool empty() const { return _events.empty(); }
	const_iterator end () const { return _events.end(); }
	iterator end () { return _events.end(); }

	iterator begin () { return _events.begin(); }
	const_iterator begin () const { return _events.begin(); }
	const_iterator begin (Time time) const { return first_event_at_or_after (time); }

	typedef boost::shared_ptr<Glib::Threads::RWLock::ReaderLock> ReadLock;
	typedef boost::shared_ptr<WriteLockImpl>                     WriteLock;

	virtual ReadLock  read_lock() const { return ReadLock(new Glib::Threads::RWLock::ReaderLock(_lock)); }
	virtual WriteLock write_lock()      { return WriteLock(new WriteLockImpl(_lock, _control_lock)); }

	// typedef typename boost::weak_ptr<Evoral::Note<Time> > WeakNotePtr;

	void clear();

	bool percussive() const     { return _percussive; }
	void set_percussive(bool p) { _percussive = p; }

	void start_write();
	bool writing() const { return _writing; }

	enum StuckNoteOption {
		Relax,
		DeleteStuckNotes,
		ResolveStuckNotes
	};

	void end_write (StuckNoteOption, Time when = Time());

	const TypeMap& type_map() const { return _type_map; }

	inline size_t n_notes() const { return _notes.size(); }

	typedef NotePointer<Time> NotePtr;

	inline static bool note_time_comparator(NotePtr const & a, NotePtr const & b) {
		return a->time() < b->time();
	}

	struct NoteNumberComparator {
		inline bool operator()(NotePtr const & a, NotePtr const & b) const {
			return a->note() < b->note();
		}
		inline bool operator()(NotePtr const & a, uint8_t val) const {
			return a->note() < val;
		}
		inline bool operator()(uint8_t a, uint8_t b) const {
			return a < b;
		}
	};

	struct EarlierNoteComparator {
		inline bool operator()(NotePtr const & a, NotePtr const & b) const {
			return a->time() < b->time();
		}
	};

#if 0 // NOT USED
	struct LaterNoteComparator {
		typedef const Note<Time>* value_type;
		inline bool operator()(const boost::shared_ptr< const Note<Time> > a,
		                       const boost::shared_ptr< const Note<Time> > b) const {
			return a->time() > b->time();
		}
	};
#endif

	struct LaterNoteEndComparator {
		typedef const Note<Time>* value_type;
		inline bool operator()(const boost::shared_ptr< const Note<Time> > a,
		                       const boost::shared_ptr< const Note<Time> > b) const {
			return a->end_time().to_double() > b->end_time().to_double();
		}
	};

	typedef boost::intrusive::list<NotePtr > Notes;
	inline       Notes& notes()       { return _notes; }
	inline const Notes& notes() const { return _notes; }

	enum NoteOperator {
		PitchEqual,
		PitchLessThan,
		PitchLessThanOrEqual,
		PitchGreater,
		PitchGreaterThanOrEqual,
		VelocityEqual,
		VelocityLessThan,
		VelocityLessThanOrEqual,
		VelocityGreater,
		VelocityGreaterThanOrEqual,
	};

	typename Notes::const_iterator first_note_at_or_after (Time t) const {
		return std::lower_bound (_notes.begin(), _notes.end(), t, TimeComparator<NotePtr,Time>());
	}

	void get_notes (Notes&, NoteOperator, uint8_t val, int chan_mask = 0) const;

	void remove_overlapping_notes ();
	void trim_overlapping_notes ();
	void remove_duplicate_notes ();

	enum OverlapPitchResolution {
		LastOnFirstOff,
		FirstOnFirstOff
	};

	bool overlapping_pitches_accepted() const { return _overlapping_pitches_accepted; }
	void overlapping_pitches_accepted(bool yn)  { _overlapping_pitches_accepted = yn; }
	OverlapPitchResolution overlap_pitch_resolution() const { return _overlap_pitch_resolution; }
	void set_overlap_pitch_resolution(OverlapPitchResolution opr);

	void set_notes (const typename Sequence<Time>::Notes& n);

	inline       EventsByTime& sysexes()       { return _sysexes; }
	inline const EventsByTime& sysexes() const { return _sysexes; }

	typedef boost::intrusive::list<PatchChangePointer<Time> > PatchChangesByTime;

	struct EarlierPatchChangeComparator {
		inline bool operator() (PatchChangePointer<Time> const & a, PatchChangePointer<Time> const & b) const {
			return a->time() < b->time();
		}
		inline bool operator() (PatchChangePointer<Time> const & a, Time t) const {
			return a->time() < t;
		}
	};

	typedef PatchChangePointer<Time> PatchChangePtr;
	typedef boost::intrusive::list<PatchChangePtr> PatchChanges;
	inline       PatchChanges& patch_changes ()       { return _patch_changes; }
	inline const PatchChanges& patch_changes () const { return _patch_changes; }

	void dump (std::ostream&) const;

private:
	typedef std::priority_queue<NotePointer<Time>, std::deque<NotePointer<Time> >, LaterNoteEndComparator> ActiveNotes;
public:
	bool edited() const      { return _edited; }
	void set_edited(bool yn) { _edited = yn; }

	bool overlaps (NotePtr const & ev, const NotePtr& ignore_this_note) const;
	bool contains (NotePtr const & ev) const;

	void start_note (EventPtr & note);
	bool end_note (EventPtr & note);

	bool add_note_unlocked (NotePtr & note, void* arg = 0);
	void remove_note_unlocked (NotePtr & note);

	void add_patch_change_unlocked (PatchChangePtr &);
	void remove_patch_change_unlocked (PatchChangePtr &);

	void add_sysex_unlocked (EventPtr &);
	void remove_sysex_unlocked (EventPtr &);

	uint8_t lowest_note()  const { return _lowest_note; }
	uint8_t highest_note() const { return _highest_note; }


protected:
	bool                   _edited;
	bool                   _overlapping_pitches_accepted;
	OverlapPitchResolution _overlap_pitch_resolution;
	mutable Glib::Threads::RWLock   _lock;
	bool                   _writing;

	virtual int resolve_overlaps_unlocked (const NotePointer<Time>, void* /* arg */ = 0) {
		return 0;
	}

	typedef boost::intrusive::list<NotePtr > Pitches; /* sorted by note value */
	inline Pitches const & pitches(uint8_t chan) const { return _pitches[chan&0xf]; }
	inline Pitches &       pitches(uint8_t chan)       { return _pitches[chan&0xf]; }

	virtual void control_list_marked_dirty ();

private:
	bool overlaps_unlocked (const NotePtr& ev, const NotePtr& ignore_this_note) const;
	bool contains_unlocked (const NotePtr& ev) const;

	void append_control_unlocked (Parameter const & param, Time time, double value);

	void get_notes_by_pitch (Notes&, NoteOperator, uint8_t val, int chan_mask = 0) const;
	void get_notes_by_velocity (Notes&, NoteOperator, uint8_t val, int chan_mask = 0) const;

	const TypeMap& _type_map;

	EventsByTime _events;        // all events, ordered by time
	EventsByTime _sysexes;       // all sysex events, ordered by time
	Notes        _notes;         // notes indexed by time
	Pitches      _pitches[16];   // notes indexed by channel ([]) and then pitch
	PatchChanges _patch_changes; // all patch changes, ordered by time
	EventsByTime _write_notes[16]; // note-ons without note-offs, indexed by channel and then time

	/** Current bank number on each channel so that we know what
	 *  to put in PatchChange events when program changes are
	 *  seen.
	 */
	int _bank[16];

	bool                   _percussive;

	uint8_t _lowest_note;
	uint8_t _highest_note;

	EventPool* _event_pool;
};


} // namespace Evoral

template<typename Time> /*LIBEVORAL_API*/ std::ostream& operator<<(std::ostream& o, const Evoral::Sequence<Time>& s) { s.dump (o); return o; }


#endif // EVORAL_SEQUENCE_HPP
