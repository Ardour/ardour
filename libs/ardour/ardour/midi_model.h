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
#include <deque>
#include <utility>
#include <boost/utility.hpp>
#include <glibmm/thread.h>
#include <pbd/command.h>
#include <ardour/types.h>
#include <ardour/midi_buffer.h>
#include <ardour/midi_ring_buffer.h>
#include <ardour/automatable.h>
#include <ardour/types.h>
#include <evoral/Note.hpp>
#include <evoral/Sequence.hpp>

namespace ARDOUR {

class Session;
class MidiSource;
	
/** This is a higher level (than MidiBuffer) model of MIDI data, with separate
 * representations for notes (instead of just unassociated note on/off events)
 * and controller data.  Controller data is represented as part of the
 * Automatable base (i.e. in a map of AutomationList, keyed by Parameter).
 * Because of this MIDI controllers and automatable controllers/widgets/etc
 * are easily interchangeable.
 */
class MidiModel : public Automatable, public Evoral::Sequence {
public:
	MidiModel(MidiSource* s, size_t size=0);
	
	NoteMode note_mode() const { return (percussive() ? Percussive : Sustained); }
	void set_note_mode(NoteMode mode) { set_percussive(mode == Percussive); };

	/** Add/Remove notes.
	 * Technically all operations can be implemented as one of these.
	 */
	class DeltaCommand : public Command
	{
	public:
		DeltaCommand (boost::shared_ptr<MidiModel> m, const std::string& name);
		DeltaCommand (boost::shared_ptr<MidiModel>,   const XMLNode& node);

		const std::string& name() const { return _name; }
		
		void operator()();
		void undo();
		
		int set_state (const XMLNode&);
		XMLNode& get_state ();

		void add(const boost::shared_ptr<Evoral::Note> note);
		void remove(const boost::shared_ptr<Evoral::Note> note);

	private:
		XMLNode &marshal_note(const boost::shared_ptr<Evoral::Note> note);
		boost::shared_ptr<Evoral::Note> unmarshal_note(XMLNode *xml_note);
		
		boost::shared_ptr<MidiModel>         _model;
		const std::string                    _name;
		
		typedef std::list< boost::shared_ptr<Evoral::Note> > NoteList;
		
		NoteList _added_notes;
		NoteList _removed_notes;
	};

	MidiModel::DeltaCommand* new_delta_command(const std::string name="midi edit");
	void                     apply_command(Command* cmd);

	bool write_to(boost::shared_ptr<MidiSource> source);
		
	// MidiModel doesn't use the normal AutomationList serialisation code
	// since controller data is stored in the .mid
	XMLNode& get_state();
	int set_state(const XMLNode&) { return 0; }

	sigc::signal<void> ContentsChanged;
	
	const MidiSource* midi_source() const { return _midi_source; }
	void set_midi_source(MidiSource* source) { _midi_source = source; } 
	
private:
	friend class DeltaCommand;
	
	// We cannot use a boost::shared_ptr here to avoid a retain cycle
	MidiSource* _midi_source;
};

} /* namespace ARDOUR */

#endif /* __ardour_midi_model_h__ */

