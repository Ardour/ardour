/*
 Copyright (C) 2007 Paul Davis 
 Written by Dave Robillard, 2007

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
#include <pbd/error.h>
#include <pbd/enumwriter.h>
#include <midi++/events.h>

#include <ardour/midi_model.h>
#include <ardour/midi_source.h>
#include <ardour/types.h>
#include <ardour/session.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

MidiModel::MidiModel(MidiSource *s, size_t size)
	: AutomatableSequence(s->session(), size)
	, _midi_source(s)
{
	cerr << "MidiModel \"" << s->name() << "\" constructed: " << this << endl;
}

/** Start a new command.
 *
 * This has no side-effects on the model or Session, the returned command
 * can be held on to for as long as the caller wishes, or discarded without
 * formality, until apply_command is called and ownership is taken.
 */
MidiModel::DeltaCommand* MidiModel::new_delta_command(const string name)
{
	DeltaCommand* cmd = new DeltaCommand(_midi_source->model(), name);
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
	assert(is_sorted());
	session.commit_reversible_command(cmd);
	set_edited(true);
}


// DeltaCommand

MidiModel::DeltaCommand::DeltaCommand(boost::shared_ptr<MidiModel> m,
		const std::string& name)
	: Command(name)
	, _model(m)
	, _name(name)
{
}

MidiModel::DeltaCommand::DeltaCommand(boost::shared_ptr<MidiModel> m,
		const XMLNode& node)
	: _model(m)
{
	set_state(node);
}

void
MidiModel::DeltaCommand::add(const boost::shared_ptr<Evoral::Note> note)
{
	//cerr << "MEC: apply" << endl;
	_removed_notes.remove(note);
	_added_notes.push_back(note);
}

void
MidiModel::DeltaCommand::remove(const boost::shared_ptr<Evoral::Note> note)
{
	//cerr << "MEC: remove" << endl;
	_added_notes.remove(note);
	_removed_notes.push_back(note);
}

void
MidiModel::DeltaCommand::operator()()
{
	// This could be made much faster by using a priority_queue for added and
	// removed notes (or sort here), and doing a single iteration over _model
	
	_model->write_lock();

	// Store the current seek position so we can restore the read iterator
	// after modifying the contents of the model
	const double read_time = _model->read_time();

	for (NoteList::iterator i = _added_notes.begin(); i != _added_notes.end(); ++i)
		_model->add_note_unlocked(*i);

	for (NoteList::iterator i = _removed_notes.begin(); i != _removed_notes.end(); ++i)
		_model->remove_note_unlocked(*i);

	_model->write_unlock();
	// FIXME: race?
	_model->read_seek(read_time); // restore read position

	_model->ContentsChanged(); /* EMIT SIGNAL */
}

void
MidiModel::DeltaCommand::undo()
{
	// This could be made much faster by using a priority_queue for added and
	// removed notes (or sort here), and doing a single iteration over _model
	
	_model->write_lock();

	// Store the current seek position so we can restore the read iterator
	// after modifying the contents of the model
	const double read_time = _model->read_time();

	for (NoteList::iterator i = _added_notes.begin(); i != _added_notes.end(); ++i)
		_model->remove_note_unlocked(*i);

	for (NoteList::iterator i = _removed_notes.begin(); i != _removed_notes.end(); ++i)
		_model->add_note_unlocked(*i);

	_model->write_unlock();
	// FIXME: race?
	_model->read_seek(read_time); // restore read position

	_model->ContentsChanged(); /* EMIT SIGNAL */
}

XMLNode&
MidiModel::DeltaCommand::marshal_note(const boost::shared_ptr<Evoral::Note> note)
{
	XMLNode *xml_note = new XMLNode("note");
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

boost::shared_ptr<Evoral::Note>
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
		note = 1;
	}

	if ((prop = xml_note->property("velocity")) != 0) {
		istringstream velocity_str(prop->value());
		velocity_str >> velocity;
	} else {
		warning << "note information missing velocity" << endmsg;
		velocity = 127;
	}

	boost::shared_ptr<Evoral::Note> note_ptr(new Evoral::Note(channel, time, length, note, velocity));
	return note_ptr;
}

#define ADDED_NOTES_ELEMENT "added_notes"
#define REMOVED_NOTES_ELEMENT "removed_notes"
#define DELTA_COMMAND_ELEMENT "DeltaCommand"

int
MidiModel::DeltaCommand::set_state(const XMLNode& delta_command)
{
	if (delta_command.name() != string(DELTA_COMMAND_ELEMENT)) {
		return 1;
	}

	_added_notes.clear();
	XMLNode *added_notes = delta_command.child(ADDED_NOTES_ELEMENT);
	XMLNodeList notes = added_notes->children();
	transform(notes.begin(), notes.end(), back_inserter(_added_notes),
			sigc::mem_fun(*this, &DeltaCommand::unmarshal_note));

	_removed_notes.clear();
	XMLNode *removed_notes = delta_command.child(REMOVED_NOTES_ELEMENT);
	notes = removed_notes->children();
	transform(notes.begin(), notes.end(), back_inserter(_removed_notes),
			sigc::mem_fun(*this, &DeltaCommand::unmarshal_note));

	return 0;
}

XMLNode&
MidiModel::DeltaCommand::get_state()
{
	XMLNode *delta_command = new XMLNode(DELTA_COMMAND_ELEMENT);
	delta_command->add_property("midi-source", _model->midi_source()->id().to_s());

	XMLNode *added_notes = delta_command->add_child(ADDED_NOTES_ELEMENT);
	for_each(_added_notes.begin(), _added_notes.end(), sigc::compose(
			sigc::mem_fun(*added_notes, &XMLNode::add_child_nocopy),
			sigc::mem_fun(*this, &DeltaCommand::marshal_note)));

	XMLNode *removed_notes = delta_command->add_child(REMOVED_NOTES_ELEMENT);
	for_each(_removed_notes.begin(), _removed_notes.end(), sigc::compose(
			sigc::mem_fun(*removed_notes, &XMLNode::add_child_nocopy),
			sigc::mem_fun(*this, &DeltaCommand::marshal_note)));

	return *delta_command;
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
	
	for (Evoral::Sequence::const_iterator i = begin(); i != end(); ++i) {
		source->append_event_unlocked(Frames, *i);
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

