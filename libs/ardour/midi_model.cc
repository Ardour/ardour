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

#define __STDC_LIMIT_MACROS 1
#include <set>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <stdint.h>
#include "pbd/error.h"
#include "pbd/enumwriter.h"
#include "midi++/events.h"

#include "ardour/midi_model.h"
#include "ardour/midi_source.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/smf_source.h"
#include "ardour/types.h"
#include "ardour/session.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

MidiModel::MidiModel(MidiSource* s)
	: AutomatableSequence<TimeType>(s->session())
	, _midi_source(s)
{
}

/** Start a new Diff command.
 *
 * This has no side-effects on the model or Session, the returned command
 * can be held on to for as long as the caller wishes, or discarded without
 * formality, until apply_command is called and ownership is taken.
 */
MidiModel::DiffCommand*
MidiModel::new_diff_command(const string name)
{
	DiffCommand* cmd = new DiffCommand(_midi_source->model(), name);
	return cmd;
}

/** Apply a command.
 *
 * Ownership of cmd is taken, it must not be deleted by the caller.
 * The command will constitute one item on the undo stack.
 */
void
MidiModel::apply_command(Session& session, Command* cmd)
{
	session.begin_reversible_command(cmd->name());
	(*cmd)();
	session.commit_reversible_command(cmd);
	set_edited(true);
}

/** Apply a command as part of a larger reversible transaction
 *
 * Ownership of cmd is taken, it must not be deleted by the caller.
 * The command will constitute one item on the undo stack.
 */
void
MidiModel::apply_command_as_subcommand(Session& session, Command* cmd)
{
	(*cmd)();
	session.add_command(cmd);
	set_edited(true);
}

/************** DIFF COMMAND ********************/

#define DIFF_COMMAND_ELEMENT "DiffCommand"
#define DIFF_NOTES_ELEMENT "ChangedNotes"
#define ADDED_NOTES_ELEMENT "AddedNotes"
#define REMOVED_NOTES_ELEMENT "RemovedNotes"
#define SIDE_EFFECT_REMOVALS_ELEMENT "SideEffectRemovals"

MidiModel::DiffCommand::DiffCommand(boost::shared_ptr<MidiModel> m, const std::string& name)
	: Command(name)
	, _model(m)
	, _name(name)
{
	assert(_model);
}

MidiModel::DiffCommand::DiffCommand(boost::shared_ptr<MidiModel> m, const XMLNode& node)
	: _model(m)
{
	assert(_model);
	set_state(node, Stateful::loading_state_version);
}

void
MidiModel::DiffCommand::add(const NotePtr note)
{
	_removed_notes.remove(note);
	_added_notes.push_back(note);
}

void
MidiModel::DiffCommand::remove(const NotePtr note)
{
	_added_notes.remove(note);
	_removed_notes.push_back(note);
}

void
MidiModel::DiffCommand::side_effect_remove(const NotePtr note)
{
	side_effect_removals.insert (note);
}

 void
 MidiModel::DiffCommand::change(const NotePtr note, Property prop,
                                uint8_t new_value)
 {
         NoteChange change;

         switch (prop) {
         case NoteNumber:
                 if (new_value == note->note()) {
                         return;
                 }
                 change.old_value = note->note();
                 break;
         case Velocity:
                 if (new_value == note->velocity()) {
                         return;
                 }
                 change.old_value = note->velocity();
                 break;
         case Channel:
                 if (new_value == note->channel()) {
                         return;
                 }
                 change.old_value = note->channel();
                 break;


         case StartTime:
                 fatal << "MidiModel::DiffCommand::change() with integer argument called for start time" << endmsg;
                 /*NOTREACHED*/
                 break;
         case Length:
                 fatal << "MidiModel::DiffCommand::change() with integer argument called for length" << endmsg;
                 /*NOTREACHED*/
                 break;
         }

         change.note = note;
         change.property = prop;
         change.new_value = new_value;

         _changes.push_back (change);
 }

 void
 MidiModel::DiffCommand::change(const NotePtr note, Property prop,
                                TimeType new_time)
 {
         NoteChange change;

         switch (prop) {
         case NoteNumber:
         case Channel:
         case Velocity:
                 fatal << "MidiModel::DiffCommand::change() with time argument called for note, channel or velocity" << endmsg;
                 break;

         case StartTime:
                 if (Evoral::musical_time_equal (note->time(), new_time)) {
                         return;
                 }
                 change.old_time = note->time();
                 break;
         case Length:
                 if (Evoral::musical_time_equal (note->length(), new_time)) {
                         return;
                 }
                 change.old_time = note->length();
                 break;
         }

         change.note = note;
         change.property = prop;
         change.new_time = new_time;

         _changes.push_back (change);
 }

 MidiModel::DiffCommand&
 MidiModel::DiffCommand::operator+= (const DiffCommand& other)
 {
         if (this == &other) {
                 return *this;
         }

         if (_model != other._model) {
                 return *this;
         }

         _added_notes.insert (_added_notes.end(), other._added_notes.begin(), other._added_notes.end());
         _removed_notes.insert (_removed_notes.end(), other._removed_notes.begin(), other._removed_notes.end());
         side_effect_removals.insert (other.side_effect_removals.begin(), other.side_effect_removals.end());
         _changes.insert (_changes.end(), other._changes.begin(), other._changes.end());

         return *this;
 }

 void
 MidiModel::DiffCommand::operator()()
 {
         {
                 MidiModel::WriteLock lock(_model->edit_lock());

                 for (NoteList::iterator i = _added_notes.begin(); i != _added_notes.end(); ++i) {
                         if (!_model->add_note_unlocked(*i)) {
                                 /* failed to add it, so don't leave it in the removed list, to
                                    avoid apparent errors on undo.
                                  */
                                 _removed_notes.remove (*i);
                         }
                 }

                 for (NoteList::iterator i = _removed_notes.begin(); i != _removed_notes.end(); ++i) {
                         _model->remove_note_unlocked(*i);
                 }

                 /* notes we modify in a way that requires remove-then-add to maintain ordering */
                 set<NotePtr> temporary_removals;

                 for (ChangeList::iterator i = _changes.begin(); i != _changes.end(); ++i) {
                         Property prop = i->property;
                         switch (prop) {
                         case NoteNumber:
                                 if (temporary_removals.find (i->note) == temporary_removals.end()) {
                                         _model->remove_note_unlocked (i->note);
                                         temporary_removals.insert (i->note);
                                 }
                                 i->note->set_note (i->new_value);
                                 break;

                         case StartTime:
                                 if (temporary_removals.find (i->note) == temporary_removals.end()) {
                                         _model->remove_note_unlocked (i->note);
                                         temporary_removals.insert (i->note);

                                 }
                                 i->note->set_time (i->new_time);
                                 break;

                         case Channel:
                                 if (temporary_removals.find (i->note) == temporary_removals.end()) {
                                         _model->remove_note_unlocked (i->note);
                                         temporary_removals.insert (i->note);
                                 }
                                 i->note->set_channel (i->new_value);
                                 break;

                         /* no remove-then-add required for these properties, since we do not index them
                          */

                         case Velocity:
                                 i->note->set_velocity (i->new_value);
                                 break;

                         case Length:
                                 i->note->set_length (i->new_time);
                                 break;

                         }
                 }


                 for (set<NotePtr>::iterator i = temporary_removals.begin(); i != temporary_removals.end(); ++i) {
                         DiffCommand side_effects (model(), "side effects");
                         _model->add_note_unlocked (*i, &side_effects);
                         *this += side_effects;
                 }

                 if (!side_effect_removals.empty()) {
                         cerr << "SER: \n";
                         for (set<NotePtr>::iterator i = side_effect_removals.begin(); i != side_effect_removals.end(); ++i) {
                                 cerr << "\t" << *i << ' ' << **i << endl;
                         }
                 }
         }

         _model->ContentsChanged(); /* EMIT SIGNAL */
 }

 void
 MidiModel::DiffCommand::undo()
 {
         {
                 MidiModel::WriteLock lock(_model->edit_lock());

                 for (NoteList::iterator i = _added_notes.begin(); i != _added_notes.end(); ++i) {
                         _model->remove_note_unlocked(*i);
                 }

                 for (NoteList::iterator i = _removed_notes.begin(); i != _removed_notes.end(); ++i) {
                         _model->add_note_unlocked(*i);
                 }

                 /* notes we modify in a way that requires remove-then-add to maintain ordering */
                 set<NotePtr> temporary_removals;

                 for (ChangeList::iterator i = _changes.begin(); i != _changes.end(); ++i) {
                         Property prop = i->property;
                         switch (prop) {
                         case NoteNumber:
                                 if (temporary_removals.find (i->note) == temporary_removals.end()) {
                                         _model->remove_note_unlocked (i->note);
                                         temporary_removals.insert (i->note);
                                 }
                                 i->note->set_note (i->old_value);
                                 break;
                         case Velocity:
                                 i->note->set_velocity (i->old_value);
                                 break;
                         case StartTime:
                                 if (temporary_removals.find (i->note) == temporary_removals.end()) {
                                         _model->remove_note_unlocked (i->note);
                                         temporary_removals.insert (i->note);
                                 }
                                 i->note->set_time (i->old_time);
                                 break;
                         case Length:
                                 i->note->set_length (i->old_time);
                                 break;
                         case Channel:
                                 if (temporary_removals.find (i->note) == temporary_removals.end()) {
                                         _model->remove_note_unlocked (i->note);
                                         temporary_removals.insert (i->note);
                                 }
                                 i->note->set_channel (i->old_value);
                                 break;
                         }
                 }

                 for (set<NotePtr>::iterator i = temporary_removals.begin(); i != temporary_removals.end(); ++i) {
                         _model->add_note_unlocked (*i);
                 }

                 /* finally add back notes that were removed by the "do". we don't care
                    about side effects here since the model should be back to its original
                    state once this is done.
                  */

                 for (set<NotePtr>::iterator i = side_effect_removals.begin(); i != side_effect_removals.end(); ++i) {
                         _model->add_note_unlocked (*i);
                 }
         }

         _model->ContentsChanged(); /* EMIT SIGNAL */
 }

 XMLNode&
 MidiModel::DiffCommand::marshal_note(const NotePtr note)
 {
         XMLNode* xml_note = new XMLNode("note");

         cerr << "Marshalling note: " << *note << endl;

         ostringstream note_str(ios::ate);
         note_str << int(note->note());
         xml_note->add_property("note", note_str.str());

         ostringstream channel_str(ios::ate);
         channel_str << int(note->channel());
         xml_note->add_property("channel", channel_str.str());

         ostringstream time_str(ios::ate);
         time_str << note->time();
         xml_note->add_property("time", time_str.str());

         ostringstream length_str(ios::ate);
         length_str << note->length();
         xml_note->add_property("length", length_str.str());

         ostringstream velocity_str(ios::ate);
         velocity_str << (unsigned int) note->velocity();
         xml_note->add_property("velocity", velocity_str.str());

         return *xml_note;
 }

 Evoral::Sequence<MidiModel::TimeType>::NotePtr
 MidiModel::DiffCommand::unmarshal_note(XMLNode *xml_note)
 {
         unsigned int note;
         XMLProperty* prop;
         unsigned int channel;
         unsigned int time;
         unsigned int length;
         unsigned int velocity;

         if ((prop = xml_note->property("note")) != 0) {
                 istringstream note_str(prop->value());
                 note_str >> note;
         } else {
                 warning << "note information missing note value" << endmsg;
                 note = 127;
         }

         if ((prop = xml_note->property("channel")) != 0) {
                 istringstream channel_str(prop->value());
                 channel_str >> channel;
         } else {
                 warning << "note information missing channel" << endmsg;
                 channel = 0;
         }

         if ((prop = xml_note->property("time")) != 0) {
                 istringstream time_str(prop->value());
                 time_str >> time;
         } else {
                 warning << "note information missing time" << endmsg;
                 time = 0;
         }

         if ((prop = xml_note->property("length")) != 0) {
                 istringstream length_str(prop->value());
                 length_str >> length;
         } else {
                 warning << "note information missing length" << endmsg;
                 length = 1;
         }

         if ((prop = xml_note->property("velocity")) != 0) {
                 istringstream velocity_str(prop->value());
                 velocity_str >> velocity;
         } else {
                 warning << "note information missing velocity" << endmsg;
                 velocity = 127;
         }

         NotePtr note_ptr(new Evoral::Note<TimeType>(channel, time, length, note, velocity));

         return note_ptr;
 }

 XMLNode&
 MidiModel::DiffCommand::marshal_change(const NoteChange& change)
 {
         XMLNode* xml_change = new XMLNode("change");

         /* first, the change itself */

         xml_change->add_property ("property", enum_2_string (change.property));

         {
                 ostringstream old_value_str (ios::ate);
                 if (change.property == StartTime || change.property == Length) {
                         old_value_str << change.old_time;
                 } else {
                         old_value_str << (unsigned int) change.old_value;
                 }
                 xml_change->add_property ("old", old_value_str.str());
         }

         {
                 ostringstream new_value_str (ios::ate);
                 if (change.property == StartTime || change.property == Length) {
                         new_value_str << change.new_time;
                 } else {
                         new_value_str << (unsigned int) change.new_value;
                 }
                 xml_change->add_property ("new", new_value_str.str());
         }

         /* now the rest of the note */

         const SMFSource* smf = dynamic_cast<const SMFSource*> (_model->midi_source());

         if (change.property != NoteNumber) {
                 ostringstream note_str;
                 note_str << int(change.note->note());
                 xml_change->add_property("note", note_str.str());
         }

         if (change.property != Channel) {
                 ostringstream channel_str;
                 channel_str << int(change.note->channel());
                 xml_change->add_property("channel", channel_str.str());
         }

         if (change.property != StartTime) {
                 ostringstream time_str;
                 if (smf) {
                         time_str << smf->round_to_file_precision (change.note->time());
                 } else {
                         time_str << change.note->time();
                 }
                 xml_change->add_property("time", time_str.str());
         }

         if (change.property != Length) {
                 ostringstream length_str;
                 if (smf) {
                         length_str << smf->round_to_file_precision (change.note->length());
                 } else {
                         length_str << change.note->length();
                 }
                 xml_change->add_property ("length", length_str.str());
         }

         if (change.property != Velocity) {
                 ostringstream velocity_str;
                 velocity_str << int (change.note->velocity());
                 xml_change->add_property("velocity", velocity_str.str());
         }

         /* and now notes that were remove as a side-effect */

         return *xml_change;
 }

 MidiModel::DiffCommand::NoteChange
 MidiModel::DiffCommand::unmarshal_change(XMLNode *xml_change)
 {
         XMLProperty* prop;
         NoteChange change;
         unsigned int note;
         unsigned int channel;
         unsigned int velocity;
         Evoral::MusicalTime time;
         Evoral::MusicalTime length;

         if ((prop = xml_change->property("property")) != 0) {
                 change.property = (Property) string_2_enum (prop->value(), change.property);
         } else {
                 fatal << "!!!" << endmsg;
                 /*NOTREACHED*/
         }

         if ((prop = xml_change->property ("old")) != 0) {
                 istringstream old_str (prop->value());
                 if (change.property == StartTime || change.property == Length) {
                         old_str >> change.old_time;
                 } else {
                         int integer_value_so_that_istream_does_the_right_thing;
                         old_str >> integer_value_so_that_istream_does_the_right_thing;
                         change.old_value = integer_value_so_that_istream_does_the_right_thing;
                 }
         } else {
                 fatal << "!!!" << endmsg;
                 /*NOTREACHED*/
         }

         if ((prop = xml_change->property ("new")) != 0) {
                 istringstream new_str (prop->value());
                 if (change.property == StartTime || change.property == Length) {
                         new_str >> change.new_time;
                 } else {
                         int integer_value_so_that_istream_does_the_right_thing;
                         new_str >> integer_value_so_that_istream_does_the_right_thing;
                         change.new_value = integer_value_so_that_istream_does_the_right_thing;
                 }
         } else {
                 fatal << "!!!" << endmsg;
                 /*NOTREACHED*/
         }

         if (change.property != NoteNumber) {
                 if ((prop = xml_change->property("note")) != 0) {
                         istringstream note_str(prop->value());
                         note_str >> note;
                 } else {
                         warning << "note information missing note value" << endmsg;
                         note = 127;
                 }
         } else {
                 note = change.new_value;
         }

         if (change.property != Channel) {
                 if ((prop = xml_change->property("channel")) != 0) {
                         istringstream channel_str(prop->value());
                         channel_str >> channel;
                 } else {
                         warning << "note information missing channel" << endmsg;
                         channel = 0;
                 }
         } else {
                 channel = change.new_value;
         }

         if (change.property != StartTime) {
                 if ((prop = xml_change->property("time")) != 0) {
                         istringstream time_str(prop->value());
                         time_str >> time;
                 } else {
                         warning << "note information missing time" << endmsg;
                         time = 0;
                 }
         } else {
                 time = change.new_time;
         }

         if (change.property != Length) {
                 if ((prop = xml_change->property("length")) != 0) {
                         istringstream length_str(prop->value());
                         length_str >> length;
                 } else {
                         warning << "note information missing length" << endmsg;
                         length = 1;
                 }
         } else {
                 length = change.new_time;
         }

         if (change.property != Velocity) {
                 if ((prop = xml_change->property("velocity")) != 0) {
                         istringstream velocity_str(prop->value());
                         velocity_str >> velocity;
                 } else {
                         warning << "note information missing velocity" << endmsg;
                         velocity = 127;
                 }
         } else {
                 velocity = change.new_value;
         }

         /* we must point at the instance of the note that is actually in the model.
            so go look for it ...
         */

         NotePtr new_note (new Evoral::Note<TimeType> (channel, time, length, note, velocity));

         change.note = _model->find_note (new_note);

         if (!change.note) {
                 warning << "MIDI note " << *new_note << " not found in model - programmers should investigate this" << endmsg;
                 /* use the actual new note */
                 change.note = new_note;
         }

         return change;
 }

 int
 MidiModel::DiffCommand::set_state(const XMLNode& diff_command, int /*version*/)
 {
         if (diff_command.name() != string(DIFF_COMMAND_ELEMENT)) {
                 return 1;
         }

         /* additions */

         _added_notes.clear();
         XMLNode* added_notes = diff_command.child(ADDED_NOTES_ELEMENT);
         if (added_notes) {
                 XMLNodeList notes = added_notes->children();
                 transform(notes.begin(), notes.end(), back_inserter(_added_notes),
                           boost::bind (&DiffCommand::unmarshal_note, this, _1));
         }


         /* removals */

         _removed_notes.clear();
         XMLNode* removed_notes = diff_command.child(REMOVED_NOTES_ELEMENT);
         if (removed_notes) {
                 XMLNodeList notes = removed_notes->children();
                 transform(notes.begin(), notes.end(), back_inserter(_removed_notes),
                           boost::bind (&DiffCommand::unmarshal_note, this, _1));
         }


         /* changes */

         _changes.clear();

         XMLNode* changed_notes = diff_command.child(DIFF_NOTES_ELEMENT);

         if (changed_notes) {
                 XMLNodeList notes = changed_notes->children();
                 transform (notes.begin(), notes.end(), back_inserter(_changes),
                            boost::bind (&DiffCommand::unmarshal_change, this, _1));

         }

         /* side effect removals caused by changes */

         side_effect_removals.clear();

         XMLNode* side_effect_notes = diff_command.child(SIDE_EFFECT_REMOVALS_ELEMENT);

         if (side_effect_notes) {
                 XMLNodeList notes = side_effect_notes->children();
                 for (XMLNodeList::iterator n = notes.begin(); n != notes.end(); ++n) {
                         side_effect_removals.insert (unmarshal_note (*n));
                 }
         }

         return 0;
 }

 XMLNode&
 MidiModel::DiffCommand::get_state ()
 {
         XMLNode* diff_command = new XMLNode(DIFF_COMMAND_ELEMENT);
         diff_command->add_property("midi-source", _model->midi_source()->id().to_s());

         XMLNode* changes = diff_command->add_child(DIFF_NOTES_ELEMENT);
         for_each(_changes.begin(), _changes.end(), 
                  boost::bind (
                          boost::bind (&XMLNode::add_child_nocopy, changes, _1),
                          boost::bind (&DiffCommand::marshal_change, this, _1)));

         XMLNode* added_notes = diff_command->add_child(ADDED_NOTES_ELEMENT);
         for_each(_added_notes.begin(), _added_notes.end(), 
                  boost::bind(
                          boost::bind (&XMLNode::add_child_nocopy, added_notes, _1),
                          boost::bind (&DiffCommand::marshal_note, this, _1)));

         XMLNode* removed_notes = diff_command->add_child(REMOVED_NOTES_ELEMENT);
         for_each(_removed_notes.begin(), _removed_notes.end(), 
                  boost::bind (
                          boost::bind (&XMLNode::add_child_nocopy, removed_notes, _1),
                          boost::bind (&DiffCommand::marshal_note, this, _1)));

         /* if this command had side-effects, store that state too 
          */

         if (!side_effect_removals.empty()) {
                 XMLNode* side_effect_notes = diff_command->add_child(SIDE_EFFECT_REMOVALS_ELEMENT);
                 for_each(side_effect_removals.begin(), side_effect_removals.end(), 
                          boost::bind (
                                  boost::bind (&XMLNode::add_child_nocopy, side_effect_notes, _1),
                                  boost::bind (&DiffCommand::marshal_note, this, _1)));
         }

         return *diff_command;
 }

 /** Write all of the model to a MidiSource (i.e. save the model).
  * This is different from manually using read to write to a source in that
  * note off events are written regardless of the track mode.  This is so the
  * user can switch a recorded track (with note durations from some instrument)
  * to percussive, save, reload, then switch it back to sustained without
  * destroying the original note durations.
  */
 bool
 MidiModel::write_to (boost::shared_ptr<MidiSource> source)
 {
         ReadLock lock(read_lock());

         const bool old_percussive = percussive();
         set_percussive(false);

         source->drop_model();
         source->mark_streaming_midi_write_started(note_mode(), _midi_source->timeline_position());

         for (Evoral::Sequence<TimeType>::const_iterator i = begin(); i != end(); ++i) {
                 source->append_event_unlocked_beats(*i);
         }

         set_percussive(old_percussive);
         source->mark_streaming_write_completed();

         set_edited(false);
        
         return true;
 }

 /** Write part or all of the model to a MidiSource (i.e. save the model).
  * This is different from manually using read to write to a source in that
  * note off events are written regardless of the track mode.  This is so the
  * user can switch a recorded track (with note durations from some instrument)
  * to percussive, save, reload, then switch it back to sustained without
  * destroying the original note durations.
  */
 bool
 MidiModel::write_section_to (boost::shared_ptr<MidiSource> source, Evoral::MusicalTime begin_time, Evoral::MusicalTime end_time)
 {
         ReadLock lock(read_lock());
         MidiStateTracker mst;
         Evoral::MusicalTime extra_note_on_time = end_time;

         const bool old_percussive = percussive();
         set_percussive(false);

         source->drop_model();
         source->mark_streaming_midi_write_started(note_mode(), _midi_source->timeline_position());

         for (Evoral::Sequence<TimeType>::const_iterator i = begin(); i != end(); ++i) {
                 const Evoral::Event<Evoral::MusicalTime>& ev (*i);

                 if (ev.time() >= begin_time && ev.time() < end_time) {

                         const Evoral::MIDIEvent<Evoral::MusicalTime>* mev = 
                                 static_cast<const Evoral::MIDIEvent<Evoral::MusicalTime>* > (&ev);

                         if (!mev) {
                                 continue;
                         }


                         if (mev->is_note_off()) {

                                 if (!mst.active (mev->note(), mev->channel())) {

                                         /* add a note-on at the start of the range we're writing
                                            to the file. velocity is just an arbitary reasonable value.
                                         */

                                         Evoral::MIDIEvent<Evoral::MusicalTime> on (mev->event_type(), extra_note_on_time, 3, 0, true);
                                         on.set_type (mev->type());
                                         on.set_note (mev->note());
                                         on.set_channel (mev->channel());
                                         on.set_velocity (mev->velocity());

                                         cerr << "Add note on for odd note off, note = " << (int) on.note() << endl;
                                         source->append_event_unlocked_beats (on);
                                         mst.add (on.note(), on.channel());
                                         mst.dump (cerr);
                                         extra_note_on_time += 1.0/128.0;
                                 }

                                 cerr << "MIDI Note off (note = " << (int) mev->note() << endl;
                                 source->append_event_unlocked_beats (*i);
                                 mst.remove (mev->note(), mev->channel());
                                 mst.dump (cerr);

                         } else if (mev->is_note_on()) {
                                 cerr << "MIDI Note on (note = " << (int) mev->note() << endl;
                                 mst.add (mev->note(), mev->channel());
                                 source->append_event_unlocked_beats(*i);
                                 mst.dump (cerr);
                         } else {
                                 cerr << "MIDI other event type\n";
                                 source->append_event_unlocked_beats(*i);
                         }
                 }
         }

         mst.resolve_notes (*source, end_time);

         set_percussive(old_percussive);
         source->mark_streaming_write_completed();

         set_edited(false);

         return true;
 }

 XMLNode&
 MidiModel::get_state()
 {
         XMLNode *node = new XMLNode("MidiModel");
         return *node;
 }

 Evoral::Sequence<MidiModel::TimeType>::NotePtr
 MidiModel::find_note (NotePtr other)
 {
         Notes::iterator l = notes().lower_bound(other);

         if (l != notes().end()) {
                 for (; (*l)->time() == other->time(); ++l) {
                         /* NB: compare note contents, not note pointers.
                            If "other" was a ptr to a note already in
                            the model, we wouldn't be looking for it,
                            would we now?
                          */
                         if (**l == *other) {
                                 return *l;
                         }
                 }
         }

         return NotePtr();
 }

 /** Lock and invalidate the source.
  * This should be used by commands and editing things
  */
 MidiModel::WriteLock
 MidiModel::edit_lock()
 {
         Glib::Mutex::Lock* source_lock = new Glib::Mutex::Lock(_midi_source->mutex());
         _midi_source->invalidate(); // Release cached iterator's read lock on model
         return WriteLock(new WriteLockImpl(source_lock, _lock, _control_lock));
 }

 /** Lock just the model, the source lock must already be held.
  * This should only be called from libardour/evoral places
  */
 MidiModel::WriteLock
 MidiModel::write_lock()
 {
         assert(!_midi_source->mutex().trylock());
         return WriteLock(new WriteLockImpl(NULL, _lock, _control_lock));
 }

 int
 MidiModel::resolve_overlaps_unlocked (const NotePtr note, void* arg)
 {
         using namespace Evoral;

         if (_writing || insert_merge_policy() == InsertMergeRelax) {
                 return 0;
         }

         DiffCommand* cmd = static_cast<DiffCommand*>(arg);

         TimeType sa = note->time();
         TimeType ea  = note->end_time();

         const Pitches& p (pitches (note->channel()));
         NotePtr search_note(new Note<TimeType>(0, 0, 0, note->note()));
         set<NotePtr> to_be_deleted;
         bool set_note_length = false;
         bool set_note_time = false;
         TimeType note_time = note->time();
         TimeType note_length = note->length();

         for (Pitches::const_iterator i = p.lower_bound (search_note); 
              i != p.end() && (*i)->note() == note->note(); ++i) {

                 TimeType sb = (*i)->time();
                 TimeType eb = (*i)->end_time();
                 OverlapType overlap = OverlapNone;

                 if ((sb > sa) && (eb <= ea)) {
                         overlap = OverlapInternal;
                 } else if ((eb >= sa) && (eb <= ea)) {
                         overlap = OverlapStart;
                 } else if ((sb > sa) && (sb <= ea)) {
                         overlap = OverlapEnd;
                 } else if ((sa >= sb) && (sa <= eb) && (ea <= eb)) {
                         overlap = OverlapExternal;
                 } else {
                         /* no overlap */
                         continue;
                 }

                 if (insert_merge_policy() == InsertMergeReject) {
                         return -1;
                 }

                 switch (overlap) {
                 case OverlapStart:
                         cerr << "OverlapStart\n";
                         /* existing note covers start of new note */
                         switch (insert_merge_policy()) {
                         case InsertMergeReplace:
                                 to_be_deleted.insert (*i);
                                 break;
                         case InsertMergeTruncateExisting:
                                 if (cmd) {
                                         cmd->change (*i, DiffCommand::Length, (note->time() - (*i)->time()));
                                 }
                                 (*i)->set_length (note->time() - (*i)->time());
                                 break;
                         case InsertMergeTruncateAddition:
                                 set_note_time = true;
                                 set_note_length = true;
                                 note_time = (*i)->time() + (*i)->length();
                                 note_length = min (note_length, (*i)->length() - ((*i)->end_time() - note->time()));
                                 break;
                         case InsertMergeExtend:
                                 if (cmd) {
                                         cmd->change ((*i), DiffCommand::Length, note->end_time() - (*i)->time());
                                 } 
                                 (*i)->set_length (note->end_time() - (*i)->time());
                                 return -1; /* do not add the new note */
                                 break;
                         default:
                                 /*NOTREACHED*/
                                 /* stupid gcc */
                                 break;
                         }
                         break;

                 case OverlapEnd:
                         cerr << "OverlapEnd\n";
                         /* existing note covers end of new note */
                         switch (insert_merge_policy()) {
                         case InsertMergeReplace:
                                 to_be_deleted.insert (*i);
                                 break;

                         case InsertMergeTruncateExisting:
                                 /* resetting the start time of the existing note
                                    is a problem because of time ordering.
                                 */
                                 break;

                         case InsertMergeTruncateAddition:
                                 set_note_length = true;
                                 note_length = min (note_length, ((*i)->time() - note->time()));
                                 break;

                         case InsertMergeExtend:
                                 /* we can't reset the time of the existing note because
                                    that will corrupt time ordering. So remove the
                                    existing note and change the position/length
                                    of the new note (which has not been added yet)
                                 */
                                 to_be_deleted.insert (*i);
                                 set_note_length = true;
                                 note_length = min (note_length, (*i)->end_time() - note->time());
                                 break;
                         default:
                                 /*NOTREACHED*/
                                 /* stupid gcc */
                                 break;
                         }
                         break;

                 case OverlapExternal:
                         cerr << "OverlapExt\n";
                         /* existing note overlaps all the new note */
                         switch (insert_merge_policy()) {
                         case InsertMergeReplace:
                                 to_be_deleted.insert (*i);
                                 break;
                         case InsertMergeTruncateExisting:
                         case InsertMergeTruncateAddition:
                         case InsertMergeExtend:
                                 /* cannot add in this case */
                                 return -1;
                         default:
                                 /*NOTREACHED*/
                                 /* stupid gcc */
                                 break;
                         }
                         break;

                 case OverlapInternal:
                         cerr << "OverlapInt\n";
                         /* new note fully overlaps an existing note */
                         switch (insert_merge_policy()) {
                         case InsertMergeReplace:
                         case InsertMergeTruncateExisting:
                         case InsertMergeTruncateAddition:
                         case InsertMergeExtend:
                                 /* delete the existing note, the new one will cover it */
                                 to_be_deleted.insert (*i);
                                 break;
                         default:
                                 /*NOTREACHED*/
                                 /* stupid gcc */
                                 break;
                         }
                         break;

                 default:
                         /*NOTREACHED*/
                         /* stupid gcc */
                         break;
                 }
         }

         for (set<NotePtr>::iterator i = to_be_deleted.begin(); i != to_be_deleted.end(); ++i) {
                 remove_note_unlocked (*i);

                 if (cmd) {
                         cmd->side_effect_remove (*i);
                 }
         }

         if (set_note_time) {
                 if (cmd) {
                         cmd->change (note, DiffCommand::StartTime, note_time);
                 } 
                 note->set_time (note_time);
         }

         if (set_note_length) {
                 if (cmd) {
                         cmd->change (note, DiffCommand::Length, note_length);
                 } 
                 note->set_length (note_length);
         }

         return 0;
 }

 InsertMergePolicy
 MidiModel::insert_merge_policy () const 
 {
         /* XXX ultimately this should be a per-track or even per-model policy */

         return _midi_source->session().config.get_insert_merge_policy();
}
                        
void
MidiModel::set_midi_source (MidiSource* s)
{
	_midi_source->invalidate ();
	_midi_source = s;
}
