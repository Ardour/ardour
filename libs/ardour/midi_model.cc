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

#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <stdint.h>
#include "pbd/error.h"
#include "pbd/enumwriter.h"
#include "midi++/events.h"

#include "ardour/midi_model.h"
#include "ardour/midi_source.h"
#include "ardour/types.h"
#include "ardour/session.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

MidiModel::MidiModel(MidiSource* s, size_t size)
	: AutomatableSequence<TimeType>(s->session(), size)
	, _midi_source(s)
{
}

/** Start a new Delta command.
 *
 * This has no side-effects on the model or Session, the returned command
 * can be held on to for as long as the caller wishes, or discarded without
 * formality, until apply_command is called and ownership is taken.
 */
MidiModel::DeltaCommand*
MidiModel::new_delta_command(const string name)
{
	DeltaCommand* cmd = new DeltaCommand(_midi_source->model(), name);
	return cmd;
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


// DeltaCommand

MidiModel::DeltaCommand::DeltaCommand(boost::shared_ptr<MidiModel> m, const std::string& name)
	: Command(name)
	, _model(m)
	, _name(name)
{
	assert(_model);
}

MidiModel::DeltaCommand::DeltaCommand(boost::shared_ptr<MidiModel> m, const XMLNode& node)
	: _model(m)
{
	assert(_model);
	set_state(node);
}

void
MidiModel::DeltaCommand::add(const boost::shared_ptr< Evoral::Note<TimeType> > note)
{
	_removed_notes.remove(note);
	_added_notes.push_back(note);
}

void
MidiModel::DeltaCommand::remove(const boost::shared_ptr< Evoral::Note<TimeType> > note)
{
	_added_notes.remove(note);
	_removed_notes.push_back(note);
}

void
MidiModel::DeltaCommand::operator()()
{
	// This could be made much faster by using a priority_queue for added and
	// removed notes (or sort here), and doing a single iteration over _model

	Glib::Mutex::Lock lm (_model->_midi_source->mutex());
	_model->_midi_source->invalidate(); // release model read lock
	_model->write_lock();

	for (NoteList::iterator i = _added_notes.begin(); i != _added_notes.end(); ++i) {
		_model->add_note_unlocked(*i);
	}

	for (NoteList::iterator i = _removed_notes.begin(); i != _removed_notes.end(); ++i) {
		_model->remove_note_unlocked(*i);
	}
	
	_model->write_unlock();
	_model->ContentsChanged(); /* EMIT SIGNAL */
}

void
MidiModel::DeltaCommand::undo()
{
	// This could be made much faster by using a priority_queue for added and
	// removed notes (or sort here), and doing a single iteration over _model
	
	Glib::Mutex::Lock lm (_model->_midi_source->mutex());
	_model->_midi_source->invalidate(); // release model read lock
	_model->write_lock();

	for (NoteList::iterator i = _added_notes.begin(); i != _added_notes.end(); ++i) {
		_model->remove_note_unlocked(*i);
	}

	for (NoteList::iterator i = _removed_notes.begin(); i != _removed_notes.end(); ++i) {
		_model->add_note_unlocked(*i);
	}

	_model->write_unlock();
	_model->ContentsChanged(); /* EMIT SIGNAL */
}

XMLNode&
MidiModel::DeltaCommand::marshal_note(const boost::shared_ptr< Evoral::Note<TimeType> > note)
{
	XMLNode* xml_note = new XMLNode("note");
	ostringstream note_str(ios::ate);
	note_str << int(note->note());
	xml_note->add_property("note", note_str.str());

	ostringstream channel_str(ios::ate);
	channel_str << int(note->channel());
	xml_note->add_property("channel", channel_str.str());

	ostringstream time_str(ios::ate);
	time_str << int(note->time());
	xml_note->add_property("time", time_str.str());

	ostringstream length_str(ios::ate);
	length_str <<(unsigned int) note->length();
	xml_note->add_property("length", length_str.str());

	ostringstream velocity_str(ios::ate);
	velocity_str << (unsigned int) note->velocity();
	xml_note->add_property("velocity", velocity_str.str());

	return *xml_note;
}

boost::shared_ptr< Evoral::Note<MidiModel::TimeType> >
MidiModel::DeltaCommand::unmarshal_note(XMLNode *xml_note)
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

	boost::shared_ptr< Evoral::Note<TimeType> > note_ptr(new Evoral::Note<TimeType>(
			channel, time, length, note, velocity));
	return note_ptr;
}

#define ADDED_NOTES_ELEMENT "AddedNotes"
#define REMOVED_NOTES_ELEMENT "RemovedNotes"
#define DELTA_COMMAND_ELEMENT "DeltaCommand"

int
MidiModel::DeltaCommand::set_state(const XMLNode& delta_command)
{
	if (delta_command.name() != string(DELTA_COMMAND_ELEMENT)) {
		return 1;
	}

	_added_notes.clear();
	XMLNode* added_notes = delta_command.child(ADDED_NOTES_ELEMENT);
	XMLNodeList notes = added_notes->children();
	transform(notes.begin(), notes.end(), back_inserter(_added_notes),
			sigc::mem_fun(*this, &DeltaCommand::unmarshal_note));

	_removed_notes.clear();
	XMLNode* removed_notes = delta_command.child(REMOVED_NOTES_ELEMENT);
	notes = removed_notes->children();
	transform(notes.begin(), notes.end(), back_inserter(_removed_notes),
			sigc::mem_fun(*this, &DeltaCommand::unmarshal_note));

	return 0;
}

XMLNode&
MidiModel::DeltaCommand::get_state()
{
	XMLNode* delta_command = new XMLNode(DELTA_COMMAND_ELEMENT);
	delta_command->add_property("midi-source", _model->midi_source()->id().to_s());

	XMLNode* added_notes = delta_command->add_child(ADDED_NOTES_ELEMENT);
	for_each(_added_notes.begin(), _added_notes.end(), sigc::compose(
			sigc::mem_fun(*added_notes, &XMLNode::add_child_nocopy),
			sigc::mem_fun(*this, &DeltaCommand::marshal_note)));

	XMLNode* removed_notes = delta_command->add_child(REMOVED_NOTES_ELEMENT);
	for_each(_removed_notes.begin(), _removed_notes.end(), sigc::compose(
			sigc::mem_fun(*removed_notes, &XMLNode::add_child_nocopy),
			sigc::mem_fun(*this, &DeltaCommand::marshal_note)));

	return *delta_command;
}

/************** DIFF COMMAND ********************/

#define DIFF_NOTES_ELEMENT "ChangedNotes"
#define DIFF_COMMAND_ELEMENT "DiffCommand"

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
	set_state(node);
}

void
MidiModel::DiffCommand::change(const boost::shared_ptr< Evoral::Note<TimeType> > note, Property prop,
			       uint8_t new_value)
{
	NotePropertyChange change;

	change.note = note;
	change.property = prop;
	change.new_value = new_value;

	switch (prop) {
	case NoteNumber:
		change.old_value = note->note();
		break;
	case Velocity:
		change.old_value = note->velocity();
		break;
	case StartTime:
		fatal << "MidiModel::DiffCommand::change() with integer argument called for start time" << endmsg;
		/*NOTREACHED*/
		break;
	case Length:
		fatal << "MidiModel::DiffCommand::change() with integer argument called for length" << endmsg;
		/*NOTREACHED*/
		break;
	case Channel:
		change.old_value = note->channel();
		break;
	}

	_changes.push_back (change);
}

void
MidiModel::DiffCommand::change(const boost::shared_ptr< Evoral::Note<TimeType> > note, Property prop,
			       TimeType new_time)
{
	NotePropertyChange change;

	change.note = note;
	change.property = prop;
	change.new_time = new_time;

	switch (prop) {
	case NoteNumber:
	case Channel:
	case Velocity:
		fatal << "MidiModel::DiffCommand::change() with time argument called for note, channel or velocity" << endmsg;
		break;
	case StartTime:
		change.old_time = note->time();
		break;
	case Length:
		change.old_time = note->length();
		break;
	}

	_changes.push_back (change);
}

void
MidiModel::DiffCommand::operator()()
{
	Glib::Mutex::Lock lm (_model->_midi_source->mutex());
	_model->_midi_source->invalidate(); // release model read lock
	_model->write_lock();

	for (ChangeList::iterator i = _changes.begin(); i != _changes.end(); ++i) {
		Property prop = i->property;
		switch (prop) {
		case NoteNumber:
			i->note->set_note (i->new_value);
			break;
		case Velocity:
			i->note->set_velocity (i->new_value);
			break;
		case StartTime:
			i->note->set_time (i->new_time);
			break;
		case Length:
			i->note->set_length (i->new_time);
			break;
		case Channel:
			i->note->set_channel (i->new_value);
			break;
		}
	}
	
	_model->write_unlock();
	_model->ContentsChanged(); /* EMIT SIGNAL */
}

void
MidiModel::DiffCommand::undo()
{
	Glib::Mutex::Lock lm (_model->_midi_source->mutex());
	_model->_midi_source->invalidate(); // release model read lock
	_model->write_lock();

	for (ChangeList::iterator i = _changes.begin(); i != _changes.end(); ++i) {
		Property prop = i->property;
		switch (prop) {
		case NoteNumber:
			i->note->set_note (i->old_value);
			break;
		case Velocity:
			i->note->set_velocity (i->old_value);
			break;
		case StartTime:
			i->note->set_time (i->old_time);
			break;
		case Length:
			i->note->set_length (i->old_time);
			break;
		case Channel:
			i->note->set_channel (i->old_value);
			break;
		}
	}

	_model->write_unlock();
	_model->ContentsChanged(); /* EMIT SIGNAL */
}

XMLNode&
MidiModel::DiffCommand::marshal_change(const NotePropertyChange& change)
{
	XMLNode* xml_change = new XMLNode("change");
	
	/* first, the change itself */

	xml_change->add_property ("property", enum_2_string (change.property));

	{
		ostringstream old_value_str (ios::ate);
		old_value_str << (unsigned int) change.old_value;
		xml_change->add_property ("old", old_value_str.str());
	}

	{
		ostringstream new_value_str (ios::ate);
		new_value_str << (unsigned int) change.old_value;
		xml_change->add_property ("new", new_value_str.str());
	}

	/* now the rest of the note */
	
	if (change.property != NoteNumber) {
		ostringstream note_str(ios::ate);
		note_str << int(change.note->note());
		xml_change->add_property("note", note_str.str());
	}
	
	if (change.property != Channel) {
		ostringstream channel_str(ios::ate);
		channel_str << int(change.note->channel());
		xml_change->add_property("channel", channel_str.str());
	}

	if (change.property != StartTime) {
		ostringstream time_str(ios::ate);
		time_str << int(change.note->time());
		xml_change->add_property("time", time_str.str());
	}

	if (change.property != Length) {
		ostringstream length_str(ios::ate);
		length_str <<(unsigned int) change.note->length();
		xml_change->add_property("length", length_str.str());
	}

	if (change.property != Velocity) {
		ostringstream velocity_str(ios::ate);
		velocity_str << (unsigned int) change.note->velocity();
		xml_change->add_property("velocity", velocity_str.str());
	}

	return *xml_change;
}

MidiModel::DiffCommand::NotePropertyChange
MidiModel::DiffCommand::unmarshal_change(XMLNode *xml_change)
{
	XMLProperty* prop;
	NotePropertyChange change;
	unsigned int note;
	unsigned int channel;
	unsigned int time;
	unsigned int length;
	unsigned int velocity;

	if ((prop = xml_change->property("property")) != 0) {
		change.property = (Property) string_2_enum (prop->value(), change.property);
	} else {
		fatal << "!!!" << endmsg;
		/*NOTREACHED*/
	}

	if ((prop = xml_change->property ("old")) != 0) {
		istringstream old_str (prop->value());
		old_str >> change.old_value;
	} else {
		fatal << "!!!" << endmsg;
		/*NOTREACHED*/
	}

	if ((prop = xml_change->property ("new")) != 0) {
		istringstream new_str (prop->value());
		new_str >> change.new_value;
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
		time = change.new_value;
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
		length = change.new_value;
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

	boost::shared_ptr<Evoral::Note<TimeType> > new_note (new Evoral::Note<TimeType> (channel, time, length, note, velocity));

	change.note = _model->find_note (new_note);

	if (!change.note) {
		warning << "MIDI note not found in model - programmers should investigate this" << endmsg;
		/* use the actual new note */
		change.note = new_note;
	}

	return change;
}

int
MidiModel::DiffCommand::set_state(const XMLNode& diff_command)
{
	if (diff_command.name() != string(DIFF_COMMAND_ELEMENT)) {
		return 1;
	}

	_changes.clear();

	XMLNode* changed_notes = diff_command.child(DIFF_NOTES_ELEMENT);
	XMLNodeList notes = changed_notes->children();

	transform (notes.begin(), notes.end(), back_inserter(_changes),
		   sigc::mem_fun(*this, &DiffCommand::unmarshal_change));
	
	return 0;
}

XMLNode&
MidiModel::DiffCommand::get_state ()
{
	XMLNode* diff_command = new XMLNode(DIFF_COMMAND_ELEMENT);
	diff_command->add_property("midi-source", _model->midi_source()->id().to_s());

	XMLNode* changes = diff_command->add_child(DIFF_NOTES_ELEMENT);
	for_each(_changes.begin(), _changes.end(), sigc::compose(
			 sigc::mem_fun(*changes, &XMLNode::add_child_nocopy),
			 sigc::mem_fun(*this, &DiffCommand::marshal_change)));

	return *diff_command;
}

/** Write the model to a MidiSource (i.e. save the model).
 * This is different from manually using read to write to a source in that
 * note off events are written regardless of the track mode.  This is so the
 * user can switch a recorded track (with note durations from some instrument)
 * to percussive, save, reload, then switch it back to sustained without
 * destroying the original note durations.
 */
bool
MidiModel::write_to(boost::shared_ptr<MidiSource> source)
{
	read_lock();

	const bool old_percussive = percussive();
	set_percussive(false);

	source->drop_model();
	
	for (Evoral::Sequence<TimeType>::const_iterator i = begin(); i != end(); ++i) {
		source->append_event_unlocked_beats(*i);
	}
		
	set_percussive(old_percussive);
	
	read_unlock();
	set_edited(false);

	return true;
}

XMLNode&
MidiModel::get_state()
{
	XMLNode *node = new XMLNode("MidiModel");
	return *node;
}

boost::shared_ptr<Evoral::Note<MidiModel::TimeType> >
MidiModel::find_note (boost::shared_ptr<Evoral::Note<TimeType> > other) 
{
	Notes::iterator i = find (notes().begin(), notes().end(), other);

	if (i == notes().end()) {
		return boost::shared_ptr<Evoral::Note<TimeType> > ();
	}
	
	return *i;
}
