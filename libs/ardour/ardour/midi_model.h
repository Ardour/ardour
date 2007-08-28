/*
    Copyright (C) 2007 Paul Davis
    Author: Dave Robillard

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

#ifndef __ardour_midi_model_h__ 
#define __ardour_midi_model_h__

#include <queue>
#include <utility>
#include <boost/utility.hpp>
#include <glibmm/thread.h>
#include <pbd/command.h>
#include <ardour/types.h>
#include <ardour/midi_buffer.h>
#include <ardour/midi_ring_buffer.h>
#include <ardour/automatable.h>
#include <ardour/note.h>

namespace ARDOUR {

class Session;
class MidiSource;
	
//                                                                     x   ,  y
typedef std::pair<boost::shared_ptr<const AutomationList>, std::pair<double,double> >
		MidiControlIterator;


/** This is a slightly higher level (than MidiBuffer) model of MIDI note data.
 * Currently it only represents note data, which is represented as complete
 * note events (ie with a start time and a duration) rather than separate
 * note on and off events (controller data is not here since it's represented
 * as an AutomationList)
 */
class MidiModel : public boost::noncopyable, public Automatable {
public:
	MidiModel(Session& s, size_t size=0);
	
	// This is crap.
	void write_lock()        { _lock.writer_lock(); _automation_lock.lock(); }
	void write_unlock()      { _lock.writer_unlock(); _automation_lock.unlock(); }
	void read_lock()   const { _lock.reader_lock(); _automation_lock.lock(); }
	void read_unlock() const { _lock.reader_unlock(); _automation_lock.unlock(); }

	void clear() { _notes.clear(); }

	NoteMode note_mode() const            { return _note_mode; }
	void     set_note_mode(NoteMode mode) { _note_mode = mode; }

	void start_write();
	bool writing() const { return _writing; }
	void end_write(bool delete_stuck=false);

	size_t read (MidiRingBuffer& dst, nframes_t start, nframes_t nframes, nframes_t stamp_offset) const;

	/** Resizes vector if necessary (NOT realtime safe) */
	void append(const MidiBuffer& data);
	
	/** Resizes vector if necessary (NOT realtime safe) */
	void append(const MidiEvent& ev);
	
	inline const Note& note_at(unsigned i) const { return _notes[i]; }

	inline size_t n_notes() const { return _notes.size(); }
	inline bool   empty()   const { return _notes.size() == 0 && _controls.size() == 0; }

	typedef std::vector<Note> Notes;
	
	inline static bool note_time_comparator (const Note& a, const Note& b) { 
		return a.time() < b.time();
	}

	struct LaterNoteEndComparator {
		typedef const Note* value_type;
		inline bool operator()(const Note* const a, const Note* const b) { 
			return a->end_time() > b->end_time();
		}
	};

	inline       Notes& notes()       { return _notes; }
	inline const Notes& notes() const { return _notes; }
	
	/** Add/Remove notes.
	 * Technically all operations can be implemented as one of these.
	 */
	class DeltaCommand : public Command
	{
	public:
		DeltaCommand (MidiModel& m, const std::string& name)
			: Command(name), _model(m), _name(name) {}
		//DeltaCommand (MidiModel&, const XMLNode& node);

		const std::string& name() const { return _name; }
		
		void operator()();
		void undo();
		
		/*int set_state (const XMLNode&);
		XMLNode& get_state ();*/

		void add(const Note& note);
		void remove(const Note& note);

	private:
		MidiModel&      _model;
		std::string     _name;
		std::list<Note> _added_notes;
		std::list<Note> _removed_notes;
	};

	MidiModel::DeltaCommand* new_delta_command(const std::string name="midi edit");
	void                     apply_command(Command* cmd);

	bool edited() const { return _edited; }
	void set_edited(bool yn) { _edited = yn; }
	bool write_to(boost::shared_ptr<MidiSource> source);
		
	// MidiModel doesn't use the normal AutomationList serialisation code, as CC data is in the .mid
	XMLNode& get_state();
	int set_state(const XMLNode&) { return 0; }

	sigc::signal<void> ContentsChanged;
	
	/** Read iterator */
	class const_iterator {
	public:
		const_iterator(MidiModel& model, double t);
		~const_iterator();

		const MidiEvent& operator*() const { return _event; }

		const const_iterator& operator++(); // prefix only

	private:
		const MidiModel& _model;
		MidiEvent        _event;

		typedef std::priority_queue<const Note*,std::vector<const Note*>, LaterNoteEndComparator>
				ActiveNotes;
		mutable ActiveNotes _active_notes;

		Notes::iterator _note_iter;

		std::vector<MidiControlIterator> _control_iters;
	};
	
private:
	friend class DeltaCommand;
	void add_note_unlocked(const Note& note);
	void remove_note_unlocked(const Note& note);

	friend class const_iterator;
	bool control_to_midi_event(MidiEvent& ev, const MidiControlIterator& iter);

#ifndef NDEBUG
	bool is_sorted() const;
#endif

	void append_note_on_unlocked(double time, uint8_t note, uint8_t velocity);
	void append_note_off_unlocked(double time, uint8_t note);
	void append_cc_unlocked(double time, uint8_t number, uint8_t value);

	mutable Glib::RWLock _lock;

	Notes    _notes;
	NoteMode _note_mode;
	
	typedef std::vector<size_t> WriteNotes;
	WriteNotes _write_notes;
	bool       _writing;
	bool       _edited;
	
	// note state for read():
	// (TODO: Remove and replace with iterator)
	
	typedef std::priority_queue<const Note*,std::vector<const Note*>,
			LaterNoteEndComparator> ActiveNotes;

	mutable ActiveNotes _active_notes;
};

} /* namespace ARDOUR */

#endif /* __ardour_midi_model_h__ */

