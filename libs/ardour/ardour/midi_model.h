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
#include "pbd/command.h"
#include "ardour/types.h"
#include "ardour/midi_buffer.h"
#include "ardour/midi_ring_buffer.h"
#include "ardour/automatable_sequence.h"
#include "ardour/types.h"
#include "evoral/Note.hpp"
#include "evoral/Sequence.hpp"

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
class MidiModel : public AutomatableSequence<Evoral::MusicalTime> {
public:
	typedef Evoral::MusicalTime TimeType;

	MidiModel(MidiSource* s);

	NoteMode note_mode() const { return (percussive() ? Percussive : Sustained); }
	void set_note_mode(NoteMode mode) { set_percussive(mode == Percussive); };

	class DiffCommand : public Command {
	public:
		enum Property {
			NoteNumber,
			Velocity,
			StartTime,
			Length,
			Channel
		};

		DiffCommand (boost::shared_ptr<MidiModel> m, const std::string& name);
		DiffCommand (boost::shared_ptr<MidiModel> m, const XMLNode& node);

		const std::string& name() const { return _name; }

		void operator()();
		void undo();

		int set_state (const XMLNode&, int version);
		XMLNode& get_state ();

		void add (const NotePtr note);
		void remove (const NotePtr note);
                void side_effect_remove (const NotePtr note);

		void change (const NotePtr note, Property prop, uint8_t new_value);
		void change (const NotePtr note, Property prop, TimeType new_time);

                bool adds_or_removes() const { 
                        return !_added_notes.empty() || !_removed_notes.empty();
                }

                DiffCommand& operator+= (const DiffCommand& other);
                boost::shared_ptr<MidiModel> model() const { return _model; }

          private:
		boost::shared_ptr<MidiModel> _model;
		const std::string            _name;

		struct NoteChange {
			DiffCommand::Property property;
			NotePtr note;
			union {
				uint8_t  old_value;
				TimeType old_time;
			};
			union {
				uint8_t  new_value;
				TimeType new_time;
			};
		};

		typedef std::list<NoteChange> ChangeList;
		ChangeList _changes;

		typedef std::list< boost::shared_ptr< Evoral::Note<TimeType> > > NoteList;
		NoteList _added_notes;
		NoteList _removed_notes;

                std::set<NotePtr> side_effect_removals;

		XMLNode &marshal_change(const NoteChange&);
		NoteChange unmarshal_change(XMLNode *xml_note);

		XMLNode &marshal_note(const NotePtr note);
		NotePtr unmarshal_note(XMLNode *xml_note);
	};

	MidiModel::DiffCommand*  new_diff_command(const std::string name="midi edit");
	void                     apply_command(Session& session, Command* cmd);
	void                     apply_command_as_subcommand(Session& session, Command* cmd);

	bool sync_to_source ();
	bool write_to(boost::shared_ptr<MidiSource> source);
	bool write_section_to (boost::shared_ptr<MidiSource> source, Evoral::MusicalTime begin = Evoral::MinMusicalTime,
                               Evoral::MusicalTime end = Evoral::MaxMusicalTime);

	// MidiModel doesn't use the normal AutomationList serialisation code
	// since controller data is stored in the .mid
	XMLNode& get_state();
	int set_state(const XMLNode&) { return 0; }

	PBD::Signal0<void> ContentsChanged;

	const MidiSource* midi_source() const { return _midi_source; }
	void set_midi_source (MidiSource *);

	boost::shared_ptr<Evoral::Note<TimeType> > find_note (NotePtr);
	boost::shared_ptr<Evoral::Note<TimeType> > find_note (gint note_id);

        InsertMergePolicy insert_merge_policy () const;
        void set_insert_merge_policy (InsertMergePolicy);

	boost::shared_ptr<Evoral::Control> control_factory(const Evoral::Parameter& id);

protected:
        int resolve_overlaps_unlocked (const NotePtr, void* arg = 0);

private:
	struct WriteLockImpl : public AutomatableSequence<TimeType>::WriteLockImpl {
		WriteLockImpl(Glib::Mutex::Lock* source_lock, Glib::RWLock& s, Glib::Mutex& c)
			: AutomatableSequence<TimeType>::WriteLockImpl(s, c)
			, source_lock(source_lock)
		{}
		~WriteLockImpl() {
			delete source_lock;
		}
		Glib::Mutex::Lock* source_lock;
	};

public:
	virtual WriteLock edit_lock();
	virtual WriteLock write_lock();

private:
	friend class DeltaCommand;

	void source_interpolation_changed (Evoral::Parameter, Evoral::ControlList::InterpolationStyle);
	void source_automation_state_changed (Evoral::Parameter, AutoState);
	void control_list_interpolation_changed (Evoral::Parameter, Evoral::ControlList::InterpolationStyle);
	void automation_list_automation_state_changed (Evoral::Parameter, AutoState);
	
	PBD::ScopedConnectionList _midi_source_connections;

	// We cannot use a boost::shared_ptr here to avoid a retain cycle
	MidiSource* _midi_source;
        InsertMergePolicy _insert_merge_policy;
};

} /* namespace ARDOUR */

#endif /* __ardour_midi_model_h__ */

