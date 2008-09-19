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
#include <boost/utility.hpp>
#include <glibmm/thread.h>
#include <evoral/types.hpp>
#include <evoral/Note.hpp>
#include <evoral/Parameter.hpp>
#include <evoral/ControlSet.hpp>

namespace Evoral {

class EventSink;
class Note;
class Event;
class ControlList;

/** This class keeps track of the current x and y for a control
 */
class ControlIterator {
public:
	boost::shared_ptr<const ControlList> list;
	double x;
	double y;
	
	ControlIterator(boost::shared_ptr<const ControlList> a_list,
			double a_x,
			double a_y)
		: list(a_list)
		, x(a_x)
		, y(a_y)
	{}
};


/** This is a higher level view of events, with separate representations for
 * notes (instead of just unassociated note on/off events) and controller data.
 * Controller data is represented as a list of time-stamped float values.
 */
class Sequence : public boost::noncopyable, virtual public ControlSet {
public:
	Sequence(size_t size);
	
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

	size_t read(EventSink&  dst,
	            timestamp_t start,
	            timedur_t   length,
	            timestamp_t stamp_offset) const;

	/** Resizes vector if necessary (NOT realtime safe) */
	void append(const Event& ev);
	
	inline const boost::shared_ptr<const Note> note_at(unsigned i) const { return _notes[i]; }
	inline const boost::shared_ptr<Note>       note_at(unsigned i)       { return _notes[i]; }

	inline size_t n_notes() const { return _notes.size(); }
	inline bool   empty()   const { return _notes.size() == 0 && _controls.size() == 0; }

	inline static bool note_time_comparator(const boost::shared_ptr<const Note> a,
	                                        const boost::shared_ptr<const Note> b) { 
		return a->time() < b->time();
	}

	struct LaterNoteEndComparator {
		typedef const Note* value_type;
		inline bool operator()(const boost::shared_ptr<const Note> a,
		                       const boost::shared_ptr<const Note> b) const { 
			return a->end_time() > b->end_time();
		}
	};

	typedef std::vector< boost::shared_ptr<Note> > Notes;
	inline       Notes& notes()       { return _notes; }
	inline const Notes& notes() const { return _notes; }

	/** Read iterator */
	class const_iterator {
	public:
		const_iterator(const Sequence& seq, double t);
		~const_iterator();

		inline bool locked() const { return _locked; }

		const Event& operator*()  const { return *_event;  }
		const boost::shared_ptr<Event> operator->() const  { return _event; }
		const boost::shared_ptr<Event> get_event_pointer() { return _event; }

		const const_iterator& operator++(); // prefix only
		bool operator==(const const_iterator& other) const;
		bool operator!=(const const_iterator& other) const { return ! operator==(other); }
		
		const_iterator& operator=(const const_iterator& other);

	private:
		friend class Sequence;

		const Sequence*          _seq;
		boost::shared_ptr<Event> _event;

		typedef std::priority_queue< boost::shared_ptr<Note>,
		                             std::deque< boost::shared_ptr<Note> >,
		                             LaterNoteEndComparator >
			ActiveNotes;
		
		mutable ActiveNotes _active_notes;

		bool                                   _is_end;
		bool                                   _locked;
		Notes::const_iterator                  _note_iter;
		std::vector<ControlIterator>           _control_iters;
		std::vector<ControlIterator>::iterator _control_iter;
	};
	
	const_iterator        begin() const { return const_iterator(*this, 0); }
	const const_iterator& end()   const { return _end_iter; }
	
	bool control_to_midi_event(boost::shared_ptr<Event>& ev,
	                           const ControlIterator&    iter) const;
	
	typedef std::map< Parameter, boost::shared_ptr<Control> > Controls;
	Controls&       controls()       { return _controls; }
	const Controls& controls() const { return _controls; }
	
	bool edited() const      { return _edited; }
	void set_edited(bool yn) { _edited = yn; }
	
protected:
	void add_note_unlocked(const boost::shared_ptr<Note> note);
	void remove_note_unlocked(const boost::shared_ptr<const Note> note);
	
	mutable const_iterator _read_iter;
	bool                   _edited;
#ifndef NDEBUG
	bool is_sorted() const;
#endif

private:
	friend class const_iterator;
	
	void append_note_on_unlocked(uint8_t chan, double time, uint8_t note, uint8_t velocity);
	void append_note_off_unlocked(uint8_t chan, double time, uint8_t note);
	void append_control_unlocked(Parameter param, double time, double value);

	mutable Glib::RWLock _lock;

	Notes    _notes;
	Controls _controls;
	
	typedef std::vector<size_t> WriteNotes;
	WriteNotes _write_notes[16];
	bool       _writing;
	
	typedef std::vector< boost::shared_ptr<const ControlList> > ControlLists;
	ControlLists _dirty_controls;

	const   const_iterator _end_iter;
	mutable nframes_t      _next_read;
	bool                   _percussive;

	/** FIXME: Make fully dynamic, map to URIs */
	enum EventTypes {
		midi_cc_type,
		midi_pc_type,
		midi_pb_type,
		midi_ca_type
	};

	typedef std::priority_queue<
			boost::shared_ptr<Note>, std::deque< boost::shared_ptr<Note> >,
			LaterNoteEndComparator>
		ActiveNotes;
};

} // namespace Evoral

#endif // EVORAL_SEQUENCE_HPP

