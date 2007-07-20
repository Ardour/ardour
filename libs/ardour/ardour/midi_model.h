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

#include <boost/utility.hpp>
#include <pbd/command.h>
#include <ardour/types.h>
#include <ardour/midi_buffer.h>

namespace ARDOUR {

class Session;


/** This is a slightly higher level (than MidiBuffer) model of MIDI note data.
 * Currently it only represents note data, which is represented as complete
 * note events (ie with a start time and a duration) rather than separate
 * note on and off events (controller data is not here since it's represented
 * as an AutomationList)
 */
class MidiModel : public boost::noncopyable {
public:
	struct Note {
		Note(double s=0, double d=0, uint8_t n=0, uint8_t v=0)
		: start(s), duration(d), note(n), velocity(v) {}

		inline bool operator==(const Note& other)
			{ return start == other.start && note == other.note; }

		double start;
		double duration;
		uint8_t note;
		uint8_t velocity;
	};

	MidiModel(Session& s, size_t size=0);

	void clear() { _notes.clear(); }

	void start_write();
	void end_write(bool delete_stuck=false);

	/** Resizes vector if necessary (NOT realtime safe) */
	void append(const MidiBuffer& data);
	
	/** Resizes vector if necessary (NOT realtime safe) */
	void append(double time, size_t size, const Byte* in_buffer);
	
	inline const Note& note_at(unsigned i) const { return _notes[i]; }

	inline size_t n_notes() const { return _notes.size(); }

	typedef std::vector<Note> Notes;
	
	inline static bool note_time_comparator (const Note& a, const Note& b) { 
		return a.start < b.start;
	}

	inline       Notes& notes()       { return _notes; }
	inline const Notes& notes() const { return _notes; }

	void     begin_command();
	Command* current_command() { return _command; }
	void     finish_command();

	// Commands
	void add_note(const Note& note);
	void remove_note(const Note& note);

	sigc::signal<void> ContentsChanged;
	
private:
	class MidiEditCommand : public Command
	{
	public:
		MidiEditCommand (MidiModel& m) : _model(m) {}
		//MidiEditCommand (MidiModel&, const XMLNode& node);
		
		void operator()();
		void undo();
		
		/*int set_state (const XMLNode&);
		XMLNode& get_state ();*/

		void add_note(const Note& note);
		void remove_note(const Note& note);

	private:
		MidiModel&      _model;
		std::list<Note> _added_notes;
		std::list<Note> _removed_notes;
	};

	void append_note_on(double time, uint8_t note, uint8_t velocity);
	void append_note_off(double time, uint8_t note);

	Session& _session;

	Notes _notes;
	
	typedef std::vector<size_t> WriteNotes;
	WriteNotes _write_notes;

	MidiEditCommand* _command; ///< In-progress command
};

} /* namespace ARDOUR */

#endif /* __ardour_midi_model_h__ */

