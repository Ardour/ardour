/*
  Copyright (C) 2007 Paul Davis
  Author: David Robillard

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

#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
#include <stdint.h>

#include "pbd/compose.h"
#include "pbd/enumwriter.h"
#include "pbd/error.h"

#include "evoral/Control.hpp"

#include "midi++/events.h"

#include "ardour/automation_control.h"
#include "ardour/midi_automation_list_binder.h"
#include "ardour/midi_model.h"
#include "ardour/midi_source.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/session.h"
#include "ardour/types.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

MidiModel::MidiModel (boost::shared_ptr<MidiSource> s)
	: AutomatableSequence<TimeType>(s->session())
{
	set_midi_source (s);
}

/** Start a new NoteDiff command.
 *
 * This has no side-effects on the model or Session, the returned command
 * can be held on to for as long as the caller wishes, or discarded without
 * formality, until apply_command is called and ownership is taken.
 */
MidiModel::NoteDiffCommand*
MidiModel::new_note_diff_command (const string name)
{
	boost::shared_ptr<MidiSource> ms = _midi_source.lock ();
	assert (ms);

	return new NoteDiffCommand (ms->model(), name);
}

/** Start a new SysExDiff command */
MidiModel::SysExDiffCommand*
MidiModel::new_sysex_diff_command (const string name)
{
	boost::shared_ptr<MidiSource> ms = _midi_source.lock ();
	assert (ms);

	return new SysExDiffCommand (ms->model(), name);
}

/** Start a new PatchChangeDiff command */
MidiModel::PatchChangeDiffCommand*
MidiModel::new_patch_change_diff_command (const string name)
{
	boost::shared_ptr<MidiSource> ms = _midi_source.lock ();
	assert (ms);

	return new PatchChangeDiffCommand (ms->model(), name);
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

#define NOTE_DIFF_COMMAND_ELEMENT "NoteDiffCommand"
#define DIFF_NOTES_ELEMENT "ChangedNotes"
#define ADDED_NOTES_ELEMENT "AddedNotes"
#define REMOVED_NOTES_ELEMENT "RemovedNotes"
#define SIDE_EFFECT_REMOVALS_ELEMENT "SideEffectRemovals"
#define SYSEX_DIFF_COMMAND_ELEMENT "SysExDiffCommand"
#define DIFF_SYSEXES_ELEMENT "ChangedSysExes"
#define PATCH_CHANGE_DIFF_COMMAND_ELEMENT "PatchChangeDiffCommand"
#define ADDED_PATCH_CHANGES_ELEMENT "AddedPatchChanges"
#define REMOVED_PATCH_CHANGES_ELEMENT "RemovedPatchChanges"
#define DIFF_PATCH_CHANGES_ELEMENT "ChangedPatchChanges"

MidiModel::DiffCommand::DiffCommand(boost::shared_ptr<MidiModel> m, const std::string& name)
	: Command (name)
	, _model (m)
	, _name (name)
{
	assert(_model);
}

MidiModel::NoteDiffCommand::NoteDiffCommand (boost::shared_ptr<MidiModel> m, const XMLNode& node)
	: DiffCommand (m, "")
{
	assert (_model);
	set_state (node, Stateful::loading_state_version);
}

void
MidiModel::NoteDiffCommand::add (const NotePtr note)
{
	_removed_notes.remove(note);
	_added_notes.push_back(note);
}

void
MidiModel::NoteDiffCommand::remove (const NotePtr note)
{
	_added_notes.remove(note);
	_removed_notes.push_back(note);
}

void
MidiModel::NoteDiffCommand::side_effect_remove (const NotePtr note)
{
	side_effect_removals.insert (note);
}

void
MidiModel::NoteDiffCommand::change (const NotePtr note, Property prop,
                                    uint8_t new_value)
{
	assert (note);

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
MidiModel::NoteDiffCommand::change (const NotePtr note, Property prop,
                                    TimeType new_time)
{
	assert (note);

	NoteChange change;

	switch (prop) {
	case NoteNumber:
	case Channel:
	case Velocity:
		fatal << "MidiModel::NoteDiffCommand::change() with time argument called for note, channel or velocity" << endmsg;
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

MidiModel::NoteDiffCommand &
MidiModel::NoteDiffCommand::operator+= (const NoteDiffCommand& other)
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
MidiModel::NoteDiffCommand::operator() ()
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

			if (!i->note) {
				/* note found during deserialization, so try
				   again now that the model state is different.
				*/
				i->note = _model->find_note (i->note_id);
				assert (i->note);
			}

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
			NoteDiffCommand side_effects (model(), "side effects");
			if (_model->add_note_unlocked (*i, &side_effects)) {
				/* The note was re-added ok */
				*this += side_effects;
			} else {
				/* The note that we removed earlier could not be re-added.  This change record
				   must say that the note was removed.  We'll keep the changes we made, though,
				   as if the note is re-added by the undo the changes must also be undone.
				*/
				_removed_notes.push_back (*i);
			}
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
MidiModel::NoteDiffCommand::undo ()
{
	{
		MidiModel::WriteLock lock(_model->edit_lock());

		for (NoteList::iterator i = _added_notes.begin(); i != _added_notes.end(); ++i) {
			_model->remove_note_unlocked(*i);
		}

		/* Apply changes first; this is important in the case of a note change which
		   resulted in the note being removed by the overlap checker.  If the overlap
		   checker removes a note, it will be in _removed_notes.  We are going to re-add
		   it below, but first we must undo the changes we made so that the overlap
		   checker doesn't refuse the re-add.
		*/

		/* notes we modify in a way that requires remove-then-add to maintain ordering */
		set<NotePtr> temporary_removals;


		/* lazily discover any affected notes that were not discovered when
		 * loading the history because of deletions, etc.
		 */

		for (ChangeList::iterator i = _changes.begin(); i != _changes.end(); ++i) {
			if (!i->note) {
				i->note = _model->find_note (i->note_id);
				assert (i->note);
			}
		}
				
		for (ChangeList::iterator i = _changes.begin(); i != _changes.end(); ++i) {
			Property prop = i->property;

			switch (prop) {
			case NoteNumber:
				if (temporary_removals.find (i->note) == temporary_removals.end() &&
				    find (_removed_notes.begin(), _removed_notes.end(), i->note) == _removed_notes.end()) {

					/* We only need to mark this note for re-add if (a) we haven't
					   already marked it and (b) it isn't on the _removed_notes
					   list (which means that it has already been removed and it
					   will be re-added anyway)
					*/

					_model->remove_note_unlocked (i->note);
					temporary_removals.insert (i->note);
				}
				i->note->set_note (i->old_value);
				break;

			case StartTime:
				if (temporary_removals.find (i->note) == temporary_removals.end() &&
				    find (_removed_notes.begin(), _removed_notes.end(), i->note) == _removed_notes.end()) {

					/* See above ... */

					_model->remove_note_unlocked (i->note);
					temporary_removals.insert (i->note);
				}
				i->note->set_time (i->old_time);
				break;

			case Channel:
				if (temporary_removals.find (i->note) == temporary_removals.end() &&
				    find (_removed_notes.begin(), _removed_notes.end(), i->note) == _removed_notes.end()) {

					/* See above ... */

					_model->remove_note_unlocked (i->note);
					temporary_removals.insert (i->note);
				}
				i->note->set_channel (i->old_value);
				break;

				/* no remove-then-add required for these properties, since we do not index them
				 */

			case Velocity:
				i->note->set_velocity (i->old_value);
				break;

			case Length:
				i->note->set_length (i->old_time);
				break;
			}
		}

		for (NoteList::iterator i = _removed_notes.begin(); i != _removed_notes.end(); ++i) {
			_model->add_note_unlocked(*i);
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
MidiModel::NoteDiffCommand::marshal_note(const NotePtr note)
{
	XMLNode* xml_note = new XMLNode("note");

	{
		ostringstream id_str(ios::ate);
		id_str << int(note->id());
		xml_note->add_property("id", id_str.str());
	}

	{
		ostringstream note_str(ios::ate);
		note_str << int(note->note());
		xml_note->add_property("note", note_str.str());
	}

	{
		ostringstream channel_str(ios::ate);
		channel_str << int(note->channel());
		xml_note->add_property("channel", channel_str.str());
	}

	{
		ostringstream time_str(ios::ate);
		time_str << note->time();
		xml_note->add_property("time", time_str.str());
	}

	{
		ostringstream length_str(ios::ate);
		length_str << note->length();
		xml_note->add_property("length", length_str.str());
	}

	{
		ostringstream velocity_str(ios::ate);
		velocity_str << (unsigned int) note->velocity();
		xml_note->add_property("velocity", velocity_str.str());
	}

	return *xml_note;
}

Evoral::Sequence<MidiModel::TimeType>::NotePtr
MidiModel::NoteDiffCommand::unmarshal_note (XMLNode *xml_note)
{
	unsigned int note;
	XMLProperty* prop;
	unsigned int channel;
	MidiModel::TimeType time;
	MidiModel::TimeType length;
	unsigned int velocity;
	gint id;

	if ((prop = xml_note->property("id")) != 0) {
		istringstream id_str(prop->value());
		id_str >> id;
	} else {
		error << "note information missing ID value" << endmsg;
		id = -1;
	}

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
	note_ptr->set_id (id);

	return note_ptr;
}

XMLNode&
MidiModel::NoteDiffCommand::marshal_change (const NoteChange& change)
{
	XMLNode* xml_change = new XMLNode("Change");

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

	ostringstream id_str;
	id_str << change.note->id();
	xml_change->add_property ("id", id_str.str());

	return *xml_change;
}

MidiModel::NoteDiffCommand::NoteChange
MidiModel::NoteDiffCommand::unmarshal_change (XMLNode *xml_change)
{
	XMLProperty* prop;
	NoteChange change;

	if ((prop = xml_change->property("property")) != 0) {
		change.property = (Property) string_2_enum (prop->value(), change.property);
	} else {
		fatal << "!!!" << endmsg;
		/*NOTREACHED*/
	}

	if ((prop = xml_change->property ("id")) == 0) {
		error << _("No NoteID found for note property change - ignored") << endmsg;
		return change;
	}

	gint note_id = atoi (prop->value().c_str());

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

	/* we must point at the instance of the note that is actually in the model.
	   so go look for it ... it may not be there (it could have been
	   deleted in a later operation, so store the note id so that we can
	   look it up again later).
	*/

	change.note = _model->find_note (note_id);
	change.note_id = note_id;

	return change;
}

int
MidiModel::NoteDiffCommand::set_state (const XMLNode& diff_command, int /*version*/)
{
	if (diff_command.name() != string (NOTE_DIFF_COMMAND_ELEMENT)) {
		return 1;
	}

	/* additions */

	_added_notes.clear();
	XMLNode* added_notes = diff_command.child(ADDED_NOTES_ELEMENT);
	if (added_notes) {
		XMLNodeList notes = added_notes->children();
		transform(notes.begin(), notes.end(), back_inserter(_added_notes),
		          boost::bind (&NoteDiffCommand::unmarshal_note, this, _1));
	}


	/* removals */

	_removed_notes.clear();
	XMLNode* removed_notes = diff_command.child(REMOVED_NOTES_ELEMENT);
	if (removed_notes) {
		XMLNodeList notes = removed_notes->children();
		transform(notes.begin(), notes.end(), back_inserter(_removed_notes),
		          boost::bind (&NoteDiffCommand::unmarshal_note, this, _1));
	}


	/* changes */

	_changes.clear();

	XMLNode* changed_notes = diff_command.child(DIFF_NOTES_ELEMENT);

	if (changed_notes) {
		XMLNodeList notes = changed_notes->children();
		transform (notes.begin(), notes.end(), back_inserter(_changes),
		           boost::bind (&NoteDiffCommand::unmarshal_change, this, _1));

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
MidiModel::NoteDiffCommand::get_state ()
{
	XMLNode* diff_command = new XMLNode (NOTE_DIFF_COMMAND_ELEMENT);
	diff_command->add_property("midi-source", _model->midi_source()->id().to_s());

	XMLNode* changes = diff_command->add_child(DIFF_NOTES_ELEMENT);
	for_each(_changes.begin(), _changes.end(),
	         boost::bind (
		         boost::bind (&XMLNode::add_child_nocopy, changes, _1),
		         boost::bind (&NoteDiffCommand::marshal_change, this, _1)));

	XMLNode* added_notes = diff_command->add_child(ADDED_NOTES_ELEMENT);
	for_each(_added_notes.begin(), _added_notes.end(),
	         boost::bind(
		         boost::bind (&XMLNode::add_child_nocopy, added_notes, _1),
		         boost::bind (&NoteDiffCommand::marshal_note, this, _1)));

	XMLNode* removed_notes = diff_command->add_child(REMOVED_NOTES_ELEMENT);
	for_each(_removed_notes.begin(), _removed_notes.end(),
	         boost::bind (
		         boost::bind (&XMLNode::add_child_nocopy, removed_notes, _1),
		         boost::bind (&NoteDiffCommand::marshal_note, this, _1)));

	/* if this command had side-effects, store that state too
	 */

	if (!side_effect_removals.empty()) {
		XMLNode* side_effect_notes = diff_command->add_child(SIDE_EFFECT_REMOVALS_ELEMENT);
		for_each(side_effect_removals.begin(), side_effect_removals.end(),
		         boost::bind (
			         boost::bind (&XMLNode::add_child_nocopy, side_effect_notes, _1),
			         boost::bind (&NoteDiffCommand::marshal_note, this, _1)));
	}

	return *diff_command;
}

MidiModel::SysExDiffCommand::SysExDiffCommand (boost::shared_ptr<MidiModel> m, const XMLNode& node)
	: DiffCommand (m, "")
{
	assert (_model);
	set_state (node, Stateful::loading_state_version);
}

void
MidiModel::SysExDiffCommand::change (boost::shared_ptr<Evoral::Event<TimeType> > s, TimeType new_time)
{
	Change change;

	change.sysex = s;
	change.property = Time;
	change.old_time = s->time ();
	change.new_time = new_time;

	_changes.push_back (change);
}

void
MidiModel::SysExDiffCommand::operator() ()
{
	{
		MidiModel::WriteLock lock (_model->edit_lock ());

		for (list<SysExPtr>::iterator i = _removed.begin(); i != _removed.end(); ++i) {
			_model->remove_sysex_unlocked (*i);
		}

		/* find any sysex events that were missing when unmarshalling */

		for (ChangeList::iterator i = _changes.begin(); i != _changes.end(); ++i) {
			if (!i->sysex) {
				i->sysex = _model->find_sysex (i->sysex_id);
				assert (i->sysex);
			}
		}

		for (ChangeList::iterator i = _changes.begin(); i != _changes.end(); ++i) {
			switch (i->property) {
			case Time:
				i->sysex->set_time (i->new_time);
			}
		}
	}

	_model->ContentsChanged (); /* EMIT SIGNAL */
}

void
MidiModel::SysExDiffCommand::undo ()
{
	{
		MidiModel::WriteLock lock (_model->edit_lock ());

		for (list<SysExPtr>::iterator i = _removed.begin(); i != _removed.end(); ++i) {
			_model->add_sysex_unlocked (*i);
		}

		/* find any sysex events that were missing when unmarshalling */

		for (ChangeList::iterator i = _changes.begin(); i != _changes.end(); ++i) {
			if (!i->sysex) {
				i->sysex = _model->find_sysex (i->sysex_id);
				assert (i->sysex);
			}
		}

		for (ChangeList::iterator i = _changes.begin(); i != _changes.end(); ++i) {
			switch (i->property) {
			case Time:
				i->sysex->set_time (i->old_time);
				break;
			}
		}

	}

	_model->ContentsChanged(); /* EMIT SIGNAL */
}

void
MidiModel::SysExDiffCommand::remove (SysExPtr sysex)
{
	_removed.push_back(sysex);
}

XMLNode&
MidiModel::SysExDiffCommand::marshal_change (const Change& change)
{
	XMLNode* xml_change = new XMLNode ("Change");

	/* first, the change itself */

	xml_change->add_property ("property", enum_2_string (change.property));

	{
		ostringstream old_value_str (ios::ate);
		old_value_str << change.old_time;
		xml_change->add_property ("old", old_value_str.str());
	}

	{
		ostringstream new_value_str (ios::ate);
		new_value_str << change.new_time;
		xml_change->add_property ("new", new_value_str.str());
	}

	ostringstream id_str;
	id_str << change.sysex->id();
	xml_change->add_property ("id", id_str.str());

	return *xml_change;
}

MidiModel::SysExDiffCommand::Change
MidiModel::SysExDiffCommand::unmarshal_change (XMLNode *xml_change)
{
	XMLProperty* prop;
	Change change;

	if ((prop = xml_change->property ("property")) != 0) {
		change.property = (Property) string_2_enum (prop->value(), change.property);
	} else {
		fatal << "!!!" << endmsg;
		/*NOTREACHED*/
	}

	if ((prop = xml_change->property ("id")) == 0) {
		error << _("No SysExID found for sys-ex property change - ignored") << endmsg;
		return change;
	}

	gint sysex_id = atoi (prop->value().c_str());

	if ((prop = xml_change->property ("old")) != 0) {
		istringstream old_str (prop->value());
		old_str >> change.old_time;
	} else {
		fatal << "!!!" << endmsg;
		/*NOTREACHED*/
	}

	if ((prop = xml_change->property ("new")) != 0) {
		istringstream new_str (prop->value());
		new_str >> change.new_time;
	} else {
		fatal << "!!!" << endmsg;
		/*NOTREACHED*/
	}

	/* we must point at the instance of the sysex that is actually in the model.
	   so go look for it ...
	*/

	change.sysex = _model->find_sysex (sysex_id);
	change.sysex_id = sysex_id;

	return change;
}

int
MidiModel::SysExDiffCommand::set_state (const XMLNode& diff_command, int /*version*/)
{
	if (diff_command.name() != string (SYSEX_DIFF_COMMAND_ELEMENT)) {
		return 1;
	}

	/* changes */

	_changes.clear();

	XMLNode* changed_sysexes = diff_command.child (DIFF_SYSEXES_ELEMENT);

	if (changed_sysexes) {
		XMLNodeList sysexes = changed_sysexes->children();
		transform (sysexes.begin(), sysexes.end(), back_inserter (_changes),
		           boost::bind (&SysExDiffCommand::unmarshal_change, this, _1));

	}

	return 0;
}

XMLNode&
MidiModel::SysExDiffCommand::get_state ()
{
	XMLNode* diff_command = new XMLNode (SYSEX_DIFF_COMMAND_ELEMENT);
	diff_command->add_property ("midi-source", _model->midi_source()->id().to_s());

	XMLNode* changes = diff_command->add_child(DIFF_SYSEXES_ELEMENT);
	for_each (_changes.begin(), _changes.end(),
	          boost::bind (
		          boost::bind (&XMLNode::add_child_nocopy, changes, _1),
		          boost::bind (&SysExDiffCommand::marshal_change, this, _1)));

	return *diff_command;
}

MidiModel::PatchChangeDiffCommand::PatchChangeDiffCommand (boost::shared_ptr<MidiModel> m, const string& name)
	: DiffCommand (m, name)
{
	assert (_model);
}

MidiModel::PatchChangeDiffCommand::PatchChangeDiffCommand (boost::shared_ptr<MidiModel> m, const XMLNode & node)
	: DiffCommand (m, "")
{
	assert (_model);
	set_state (node, Stateful::loading_state_version);
}

void
MidiModel::PatchChangeDiffCommand::add (PatchChangePtr p)
{
	_added.push_back (p);
}

void
MidiModel::PatchChangeDiffCommand::remove (PatchChangePtr p)
{
	_removed.push_back (p);
}

void
MidiModel::PatchChangeDiffCommand::change_time (PatchChangePtr patch, TimeType t)
{
	Change c;
	c.property = Time;
	c.patch = patch;
	c.old_time = patch->time ();
	c.new_time = t;

	_changes.push_back (c);
}

void
MidiModel::PatchChangeDiffCommand::change_channel (PatchChangePtr patch, uint8_t channel)
{
	Change c;
	c.property = Channel;
	c.patch = patch;
	c.old_channel = patch->channel ();
	c.new_channel = channel;
	c.patch_id = patch->id();

	_changes.push_back (c);
}

void
MidiModel::PatchChangeDiffCommand::change_program (PatchChangePtr patch, uint8_t program)
{
	Change c;
	c.property = Program;
	c.patch = patch;
	c.old_program = patch->program ();
	c.new_program = program;
	c.patch_id = patch->id();

	_changes.push_back (c);
}

void
MidiModel::PatchChangeDiffCommand::change_bank (PatchChangePtr patch, int bank)
{
	Change c;
	c.property = Bank;
	c.patch = patch;
	c.old_bank = patch->bank ();
	c.new_bank = bank;

	_changes.push_back (c);
}

void
MidiModel::PatchChangeDiffCommand::operator() ()
{
	{
		MidiModel::WriteLock lock (_model->edit_lock ());

		for (list<PatchChangePtr>::iterator i = _added.begin(); i != _added.end(); ++i) {
			_model->add_patch_change_unlocked (*i);
		}

		for (list<PatchChangePtr>::iterator i = _removed.begin(); i != _removed.end(); ++i) {
			_model->remove_patch_change_unlocked (*i);
		}

		/* find any patch change events that were missing when unmarshalling */

		for (ChangeList::iterator i = _changes.begin(); i != _changes.end(); ++i) {
			if (!i->patch) {
				i->patch = _model->find_patch_change (i->patch_id);
				assert (i->patch);
			}
		}

		set<PatchChangePtr> temporary_removals;

		for (ChangeList::iterator i = _changes.begin(); i != _changes.end(); ++i) {
			switch (i->property) {
			case Time:
				if (temporary_removals.find (i->patch) == temporary_removals.end()) {
					_model->remove_patch_change_unlocked (i->patch);
					temporary_removals.insert (i->patch);
				}
				i->patch->set_time (i->new_time);
				break;

			case Channel:
				i->patch->set_channel (i->new_channel);
				break;

			case Program:
				i->patch->set_program (i->new_program);
				break;

			case Bank:
				i->patch->set_bank (i->new_bank);
				break;
			}
		}

		for (set<PatchChangePtr>::iterator i = temporary_removals.begin(); i != temporary_removals.end(); ++i) {
			_model->add_patch_change_unlocked (*i);
		}
	}

	_model->ContentsChanged (); /* EMIT SIGNAL */
}

void
MidiModel::PatchChangeDiffCommand::undo ()
{
	{
		MidiModel::WriteLock lock (_model->edit_lock());

		for (list<PatchChangePtr>::iterator i = _added.begin(); i != _added.end(); ++i) {
			_model->remove_patch_change_unlocked (*i);
		}

		for (list<PatchChangePtr>::iterator i = _removed.begin(); i != _removed.end(); ++i) {
			_model->add_patch_change_unlocked (*i);
		}

		/* find any patch change events that were missing when unmarshalling */

		for (ChangeList::iterator i = _changes.begin(); i != _changes.end(); ++i) {
			if (!i->patch) {
				i->patch = _model->find_patch_change (i->patch_id);
				assert (i->patch);
			}
		}

		set<PatchChangePtr> temporary_removals;

		for (ChangeList::iterator i = _changes.begin(); i != _changes.end(); ++i) {
			switch (i->property) {
			case Time:
				if (temporary_removals.find (i->patch) == temporary_removals.end()) {
					_model->remove_patch_change_unlocked (i->patch);
					temporary_removals.insert (i->patch);
				}
				i->patch->set_time (i->old_time);
				break;

			case Channel:
				i->patch->set_channel (i->old_channel);
				break;

			case Program:
				i->patch->set_program (i->old_program);
				break;

			case Bank:
				i->patch->set_bank (i->old_bank);
				break;
			}
		}

		for (set<PatchChangePtr>::iterator i = temporary_removals.begin(); i != temporary_removals.end(); ++i) {
			_model->add_patch_change_unlocked (*i);
		}

	}

	_model->ContentsChanged (); /* EMIT SIGNAL */
}

XMLNode &
MidiModel::PatchChangeDiffCommand::marshal_patch_change (constPatchChangePtr p)
{
	XMLNode* n = new XMLNode ("patch-change");

	{
		ostringstream s (ios::ate);
		s << int (p->id ());
		n->add_property ("id", s.str());
	}

	{
		ostringstream s (ios::ate);
		s << p->time ();
		n->add_property ("time", s.str ());
	}

	{
		ostringstream s (ios::ate);
		s << int (p->channel ());
		n->add_property ("channel", s.str ());
	}

	{
		ostringstream s (ios::ate);
		s << int (p->program ());
		n->add_property ("program", s.str ());
	}

	{
		ostringstream s (ios::ate);
		s << int (p->bank ());
		n->add_property ("bank", s.str ());
	}

	return *n;
}

XMLNode&
MidiModel::PatchChangeDiffCommand::marshal_change (const Change& c)
{
	XMLNode* n = new XMLNode (X_("Change"));

	n->add_property (X_("property"), enum_2_string (c.property));

	{
		ostringstream s (ios::ate);
		if (c.property == Time) {
			s << c.old_time;
		} else if (c.property == Channel) {
			s << c.old_channel;
		} else if (c.property == Program) {
			s << int (c.old_program);
		} else if (c.property == Bank) {
			s << c.old_bank;
		}

		n->add_property (X_("old"), s.str ());
	}

	{
		ostringstream s (ios::ate);

		if (c.property == Time) {
			s << c.new_time;
		} else if (c.property == Channel) {
			s << c.new_channel;
		} else if (c.property == Program) {
			s << int (c.new_program);
		} else if (c.property == Bank) {
			s << c.new_bank;
		}

		n->add_property (X_("new"), s.str ());
	}

	{
		ostringstream s;
		s << c.patch->id ();
		n->add_property ("id", s.str ());
	}

	return *n;
}

MidiModel::PatchChangePtr
MidiModel::PatchChangeDiffCommand::unmarshal_patch_change (XMLNode* n)
{
	XMLProperty* prop;
	Evoral::event_id_t id;
	Evoral::MusicalTime time = 0;
	int channel = 0;
	int program = 0;
	int bank = 0;
	
	if ((prop = n->property ("id")) != 0) {
		istringstream s (prop->value());
		s >> id;
	}

	if ((prop = n->property ("time")) != 0) {
		istringstream s (prop->value ());
		s >> time;
	}

	if ((prop = n->property ("channel")) != 0) {
		istringstream s (prop->value ());
		s >> channel;
	}

	if ((prop = n->property ("program")) != 0) {
		istringstream s (prop->value ());
		s >> program;
	}

	if ((prop = n->property ("bank")) != 0) {
		istringstream s (prop->value ());
		s >> bank;
	}

	PatchChangePtr p (new Evoral::PatchChange<TimeType> (time, channel, program, bank));
	p->set_id (id);
	return p;
}

MidiModel::PatchChangeDiffCommand::Change
MidiModel::PatchChangeDiffCommand::unmarshal_change (XMLNode* n)
{
	XMLProperty* prop;
	Change c;
	int an_int;

	prop = n->property ("property");
	assert (prop);
	c.property = (Property) string_2_enum (prop->value(), c.property);

	prop = n->property ("id");
	assert (prop);
	Evoral::event_id_t const id = atoi (prop->value().c_str());

	/* we need to load via an int intermediate for all properties that are 
	   actually uint8_t (char/byte).
	*/

	prop = n->property ("old");
	assert (prop);
	{
		istringstream s (prop->value ());
		if (c.property == Time) {
			s >> c.old_time;
		} else if (c.property == Channel) {
			s >> an_int;
			c.old_channel = an_int;
		} else if (c.property == Program) {
			s >> an_int;
			c.old_program = an_int;
		} else if (c.property == Bank) {
			s >> an_int;
			c.old_bank = an_int;
		}
	}

	prop = n->property ("new");
	assert (prop);
	{
		istringstream s (prop->value ());

		if (c.property == Time) {
			s >> c.new_time;
		} else if (c.property == Channel) {
			s >> an_int;
			c.new_channel = an_int;
		} else if (c.property == Program) {
			s >> an_int;
			c.new_program = an_int;
		} else if (c.property == Bank) {
			s >> an_int;
			c.new_bank = an_int;
		}
	}

	c.patch = _model->find_patch_change (id);
	c.patch_id = id;

	return c;
}

int
MidiModel::PatchChangeDiffCommand::set_state (const XMLNode& diff_command, int /*version*/)
{
	if (diff_command.name() != PATCH_CHANGE_DIFF_COMMAND_ELEMENT) {
		return 1;
	}

	_added.clear ();
	XMLNode* added = diff_command.child (ADDED_PATCH_CHANGES_ELEMENT);
	if (added) {
		XMLNodeList p = added->children ();
		transform (p.begin(), p.end(), back_inserter (_added), boost::bind (&PatchChangeDiffCommand::unmarshal_patch_change, this, _1));
	}

	_removed.clear ();
	XMLNode* removed = diff_command.child (REMOVED_PATCH_CHANGES_ELEMENT);
	if (removed) {
		XMLNodeList p = removed->children ();
		transform (p.begin(), p.end(), back_inserter (_removed), boost::bind (&PatchChangeDiffCommand::unmarshal_patch_change, this, _1));
	}

	_changes.clear ();
	XMLNode* changed = diff_command.child (DIFF_PATCH_CHANGES_ELEMENT);
	if (changed) {
		XMLNodeList p = changed->children ();
		transform (p.begin(), p.end(), back_inserter (_changes), boost::bind (&PatchChangeDiffCommand::unmarshal_change, this, _1));
	}

	return 0;
}

XMLNode &
MidiModel::PatchChangeDiffCommand::get_state ()
{
	XMLNode* diff_command = new XMLNode (PATCH_CHANGE_DIFF_COMMAND_ELEMENT);
	diff_command->add_property("midi-source", _model->midi_source()->id().to_s());

	XMLNode* added = diff_command->add_child (ADDED_PATCH_CHANGES_ELEMENT);
	for_each (_added.begin(), _added.end(),
		  boost::bind (
			  boost::bind (&XMLNode::add_child_nocopy, added, _1),
			  boost::bind (&PatchChangeDiffCommand::marshal_patch_change, this, _1)
			  )
		);

	XMLNode* removed = diff_command->add_child (REMOVED_PATCH_CHANGES_ELEMENT);
	for_each (_removed.begin(), _removed.end(),
		  boost::bind (
			  boost::bind (&XMLNode::add_child_nocopy, removed, _1),
			  boost::bind (&PatchChangeDiffCommand::marshal_patch_change, this, _1)
			  )
		);

	XMLNode* changes = diff_command->add_child (DIFF_PATCH_CHANGES_ELEMENT);
	for_each (_changes.begin(), _changes.end(),
		  boost::bind (
			  boost::bind (&XMLNode::add_child_nocopy, changes, _1),
			  boost::bind (&PatchChangeDiffCommand::marshal_change, this, _1)
			  )
		);

	return *diff_command;
}

/** Write all of the model to a MidiSource (i.e. save the model).
 * This is different from manually using read to write to a source in that
 * note off events are written regardless of the track mode.  This is so the
 * user can switch a recorded track (with note durations from some instrument)
 * to percussive, save, reload, then switch it back to sustained without
 * destroying the original note durations.
 *
 * Similarly, control events are written without interpolation (as with the
 * `Discrete' mode).
 */
bool
MidiModel::write_to (boost::shared_ptr<MidiSource> source)
{
	ReadLock lock(read_lock());

	const bool old_percussive = percussive();
	set_percussive(false);

	boost::shared_ptr<MidiSource> ms = _midi_source.lock ();
	assert (ms);

	source->drop_model();
	source->mark_streaming_midi_write_started (note_mode());

	for (Evoral::Sequence<TimeType>::const_iterator i = begin(0, true); i != end(); ++i) {
		source->append_event_unlocked_beats(*i);
	}

	set_percussive(old_percussive);
	source->mark_streaming_write_completed();

	set_edited(false);

	return true;
}

/** very similar to ::write_to() but writes to the model's own
    existing midi_source, without making it call MidiSource::drop_model().
    the caller is a MidiSource that needs to catch up with the state
    of the model.
*/
bool
MidiModel::sync_to_source ()
{
	ReadLock lock(read_lock());

	const bool old_percussive = percussive();
	set_percussive(false);

	boost::shared_ptr<MidiSource> ms = _midi_source.lock ();
	assert (ms);

	ms->mark_streaming_midi_write_started (note_mode());

	for (Evoral::Sequence<TimeType>::const_iterator i = begin(0, true); i != end(); ++i) {
		ms->append_event_unlocked_beats(*i);
	}

	set_percussive (old_percussive);
	ms->mark_streaming_write_completed ();

	set_edited (false);

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

	const bool old_percussive = percussive();
	set_percussive(false);

	boost::shared_ptr<MidiSource> ms = _midi_source.lock ();
	assert (ms);

	source->drop_model();
	source->mark_streaming_midi_write_started (note_mode());

	for (Evoral::Sequence<TimeType>::const_iterator i = begin(0, true); i != end(); ++i) {
		const Evoral::Event<Evoral::MusicalTime>& ev (*i);

		if (ev.time() >= begin_time && ev.time() < end_time) {

			const Evoral::MIDIEvent<Evoral::MusicalTime>* mev =
				static_cast<const Evoral::MIDIEvent<Evoral::MusicalTime>* > (&ev);

			if (!mev) {
				continue;
			}


			if (mev->is_note_off()) {

				if (!mst.active (mev->note(), mev->channel())) {
					/* the matching note-on was outside the
					   time range we were given, so just
					   ignore this note-off.
					*/
					continue;
				}

				source->append_event_unlocked_beats (*i);
				mst.remove (mev->note(), mev->channel());

			} else if (mev->is_note_on()) {
				mst.add (mev->note(), mev->channel());
				source->append_event_unlocked_beats(*i);
			} else {
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

Evoral::Sequence<MidiModel::TimeType>::NotePtr
MidiModel::find_note (gint note_id)
{
	/* used only for looking up notes when reloading history from disk,
	   so we don't care about performance *too* much.
	*/

	for (Notes::iterator l = notes().begin(); l != notes().end(); ++l) {
		if ((*l)->id() == note_id) {
			return *l;
		}
	}

	return NotePtr();
}

MidiModel::PatchChangePtr
MidiModel::find_patch_change (Evoral::event_id_t id)
{
	for (PatchChanges::iterator i = patch_changes().begin(); i != patch_changes().end(); ++i) {
		if ((*i)->id() == id) {
			return *i;
		}
	}

	return PatchChangePtr ();
}

boost::shared_ptr<Evoral::Event<MidiModel::TimeType> >
MidiModel::find_sysex (gint sysex_id)
{
	/* used only for looking up notes when reloading history from disk,
	   so we don't care about performance *too* much.
	*/

	for (SysExes::iterator l = sysexes().begin(); l != sysexes().end(); ++l) {
		if ((*l)->id() == sysex_id) {
			return *l;
		}
	}

	return boost::shared_ptr<Evoral::Event<TimeType> > ();
}

/** Lock and invalidate the source.
 * This should be used by commands and editing things
 */
MidiModel::WriteLock
MidiModel::edit_lock()
{
	boost::shared_ptr<MidiSource> ms = _midi_source.lock ();
	assert (ms);

	Glib::Threads::Mutex::Lock* source_lock = new Glib::Threads::Mutex::Lock (ms->mutex());
	ms->invalidate(); // Release cached iterator's read lock on model
	return WriteLock(new WriteLockImpl(source_lock, _lock, _control_lock));
}

/** Lock just the model, the source lock must already be held.
 * This should only be called from libardour/evoral places
 */
MidiModel::WriteLock
MidiModel::write_lock()
{
	boost::shared_ptr<MidiSource> ms = _midi_source.lock ();
	assert (ms);

	assert (!ms->mutex().trylock ());
	return WriteLock(new WriteLockImpl(0, _lock, _control_lock));
}

int
MidiModel::resolve_overlaps_unlocked (const NotePtr note, void* arg)
{
	using namespace Evoral;

	if (_writing || insert_merge_policy() == InsertMergeRelax) {
		return 0;
	}

	NoteDiffCommand* cmd = static_cast<NoteDiffCommand*>(arg);

	TimeType sa = note->time();
	TimeType ea  = note->end_time();

	const Pitches& p (pitches (note->channel()));
	NotePtr search_note(new Note<TimeType>(0, 0, 0, note->note()));
	set<NotePtr> to_be_deleted;
	bool set_note_length = false;
	bool set_note_time = false;
	TimeType note_time = note->time();
	TimeType note_length = note->length();

	DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1 checking overlaps for note %2 @ %3\n", this, (int)note->note(), note->time()));

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

		DEBUG_TRACE (DEBUG::Sequence, string_compose (
			             "\toverlap is %1 for (%2,%3) vs (%4,%5)\n",
			             enum_2_string(overlap), sa, ea, sb, eb));

		if (insert_merge_policy() == InsertMergeReject) {
			DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1 just reject\n", this));
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
					cmd->change (*i, NoteDiffCommand::Length, (note->time() - (*i)->time()));
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
					cmd->change ((*i), NoteDiffCommand::Length, note->end_time() - (*i)->time());
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
			cmd->change (note, NoteDiffCommand::StartTime, note_time);
		}
		note->set_time (note_time);
	}

	if (set_note_length) {
		if (cmd) {
			cmd->change (note, NoteDiffCommand::Length, note_length);
		}
		note->set_length (note_length);
	}

	return 0;
}

InsertMergePolicy
MidiModel::insert_merge_policy () const
{
	/* XXX ultimately this should be a per-track or even per-model policy */
	boost::shared_ptr<MidiSource> ms = _midi_source.lock ();
	assert (ms);

	return ms->session().config.get_insert_merge_policy ();
}

void
MidiModel::set_midi_source (boost::shared_ptr<MidiSource> s)
{
	boost::shared_ptr<MidiSource> old = _midi_source.lock ();

	if (old) {
		old->invalidate ();
	}

	_midi_source_connections.drop_connections ();

	_midi_source = s;

	s->InterpolationChanged.connect_same_thread (
		_midi_source_connections, boost::bind (&MidiModel::source_interpolation_changed, this, _1, _2)
		);

	s->AutomationStateChanged.connect_same_thread (
		_midi_source_connections, boost::bind (&MidiModel::source_automation_state_changed, this, _1, _2)
		);
}

/** The source has signalled that the interpolation style for a parameter has changed.  In order to
 *  keep MidiSource and ControlList interpolation state the same, we pass this change onto the
 *  appropriate ControlList.
 *
 *  The idea is that MidiSource and the MidiModel's ControlList states are kept in sync, and one
 *  or the other is listened to by the GUI.
 */
void
MidiModel::source_interpolation_changed (Evoral::Parameter p, Evoral::ControlList::InterpolationStyle s)
{
	Glib::Threads::Mutex::Lock lm (_control_lock);
	control(p)->list()->set_interpolation (s);
}

/** A ControlList has signalled that its interpolation style has changed.  Again, in order to keep
 *  MidiSource and ControlList interpolation state in sync, we pass this change onto our MidiSource.
 */
void
MidiModel::control_list_interpolation_changed (Evoral::Parameter p, Evoral::ControlList::InterpolationStyle s)
{
	boost::shared_ptr<MidiSource> ms = _midi_source.lock ();
	assert (ms);

	ms->set_interpolation_of (p, s);
}

void
MidiModel::source_automation_state_changed (Evoral::Parameter p, AutoState s)
{
	Glib::Threads::Mutex::Lock lm (_control_lock);
	boost::shared_ptr<AutomationList> al = boost::dynamic_pointer_cast<AutomationList> (control(p)->list ());
	al->set_automation_state (s);
}

void
MidiModel::automation_list_automation_state_changed (Evoral::Parameter p, AutoState s)
{
	boost::shared_ptr<MidiSource> ms = _midi_source.lock ();
	assert (ms);
	ms->set_automation_state_of (p, s);
}

boost::shared_ptr<Evoral::Control>
MidiModel::control_factory (Evoral::Parameter const & p)
{
	boost::shared_ptr<Evoral::Control> c = Automatable::control_factory (p);

	/* Set up newly created control's lists to the appropriate interpolation and
	   automation state from our source.
	*/

	boost::shared_ptr<MidiSource> ms = _midi_source.lock ();
	assert (ms);

	c->list()->set_interpolation (ms->interpolation_of (p));

	boost::shared_ptr<AutomationList> al = boost::dynamic_pointer_cast<AutomationList> (c->list ());
	assert (al);

	al->set_automation_state (ms->automation_state_of (p));

	return c;
}

boost::shared_ptr<const MidiSource>
MidiModel::midi_source ()
{
	return _midi_source.lock ();
}

/** Moves notes, patch changes, controllers and sys-ex to insert silence at the start of the model.
 *  Adds commands to the session's current undo stack to reflect the movements.
 */
void
MidiModel::insert_silence_at_start (TimeType t)
{
	boost::shared_ptr<MidiSource> s = _midi_source.lock ();
	assert (s);

	/* Notes */

	if (!notes().empty ()) {
		NoteDiffCommand* c = new_note_diff_command ("insert silence");

		for (Notes::const_iterator i = notes().begin(); i != notes().end(); ++i) {
			c->change (*i, NoteDiffCommand::StartTime, (*i)->time() + t);
		}

		apply_command_as_subcommand (s->session(), c);
	}

	/* Patch changes */

	if (!patch_changes().empty ()) {
		PatchChangeDiffCommand* c = new_patch_change_diff_command ("insert silence");

		for (PatchChanges::const_iterator i = patch_changes().begin(); i != patch_changes().end(); ++i) {
			c->change_time (*i, (*i)->time() + t);
		}

		apply_command_as_subcommand (s->session(), c);
	}

	/* Controllers */

	for (Controls::iterator i = controls().begin(); i != controls().end(); ++i) {
		boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl> (i->second);
		XMLNode& before = ac->alist()->get_state ();
		i->second->list()->shift (0, t);
		XMLNode& after = ac->alist()->get_state ();
		s->session().add_command (new MementoCommand<AutomationList> (new MidiAutomationListBinder (s, i->first), &before, &after));
	}

	/* Sys-ex */

	if (!sysexes().empty()) {
		SysExDiffCommand* c = new_sysex_diff_command ("insert silence");

		for (SysExes::iterator i = sysexes().begin(); i != sysexes().end(); ++i) {
			c->change (*i, (*i)->time() + t);
		}

		apply_command_as_subcommand (s->session(), c);
	}
}

/** Transpose notes in a time range by a given number of semitones.  Notes
 *  will be clamped at 0 and 127 if the transposition would make them exceed
 *  that range.
 *
 *  @param from Start time.
 *  @param end End time.
 *  @param semitones Number of semitones to transpose by (+ve is higher, -ve is lower).
 */
void
MidiModel::transpose (TimeType from, TimeType to, int semitones)
{
	boost::shared_ptr<const MidiSource> s = midi_source ();

	NoteDiffCommand* c = new_note_diff_command (_("transpose"));

	for (Notes::iterator i = notes().begin(); i != notes().end(); ++i) {

		if ((*i)->time() >= to) {

			/* finished */
			break;

		} else if ((*i)->time() >= from) {

			int new_note = (*i)->note() + semitones;

			if (new_note < 0) {
				new_note = 0;
			} else if (new_note > 127) {
				new_note = 127;
			}

			c->change (*i, NoteDiffCommand::NoteNumber, (uint8_t) new_note);

		}
	}

	apply_command (s->session (), c);
}

void
MidiModel::control_list_marked_dirty ()
{
	AutomatableSequence<Evoral::MusicalTime>::control_list_marked_dirty ();
	
	ContentsChanged (); /* EMIT SIGNAL */
}
