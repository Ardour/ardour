/*
 * Copyright (C) 2026 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <algorithm>
#include <cmath>
#include <fstream>

#include "pbd/compose.h"
#include "pbd/debug.h"
#include "pbd/enumwriter.h"
#include "pbd/failed_constructor.h"

#include "ardour/debug.h"
#include "ardour/scale.h"
#include "ardour/scala_file.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;

std::multimap<TuningSystem,MusicalMode> MusicalMode::modes_by_tuning;
std::map<std::string,MusicalMode> MusicalMode::modes_by_name;
std::map<int,MusicalMode> MusicalMode::modes_by_id;

std::map<TuningSystem,int> MusicalMode::tone_equivalent_ratio;
std::map<TuningSystem,int> MusicalMode::tones_per_equivalent;

void
MusicalMode::init ()
{
	if (!tone_equivalent_ratio.empty()) {
		return;
	}

	/* Frequency ratio considered to be an "equivalent" pitch. Most musical
	 * cultures use 2.0 (the octave), but there are some examples (most in
	 * experimental music) that use 3.0 (the tritave).
	 */

	tone_equivalent_ratio[TwelveTone] = 2;

	/* Number of tones the "equivalent pitch" ratio is divided into. This
	 * number does not imply any particular kind of division. "12" could
	 * mean just intonation or equal-tempered or anything else.
	 */

	tones_per_equivalent[TwelveTone] = 12;


	/* Actual scales */

	register_scales (TwelveTone, { _("Major"), _("Ionian") }, PitchClass, {  2 , 4 , 5 , 7 , 9 , 11 });
	register_scales (TwelveTone, { _("Minor"), _("Aeolian") }, PitchClass, {  2 , 3 , 5 , 7 , 8 , 10 });
	register_scale (TwelveTone, _("Dorian"), PitchClass, { 2 , 3 , 4 , 6 , 8 , 9 , 11 });
	register_scale (TwelveTone, _("Harmonic Minor"), PitchClass, { 2 , 3 , 5 , 7 , 10 , 11 });
	register_scale (TwelveTone, _("Blues"), PitchClass, { 2 , 3 , 5 , 6 , 7 , 9 , 10 , 11 });
	register_scale (TwelveTone, _("Melodic Minor Ascending"), PitchClass, { 2 , 3 , 5 , 7 , 9 , 11 });
	register_scale (TwelveTone, _("Melodic Minor Descending"), PitchClass, { 2 , 4 , 5 , 7 , 9 , 10 });
	register_scale (TwelveTone, _("Phrygian"), PitchClass, { 1 , 3 , 5 , 7 , 8 , 10 });
	register_scale (TwelveTone, _("Lydian"), PitchClass, { 2 , 4 , 6 , 7 , 9 , 11 });
	register_scale (TwelveTone, _("Mixolydian"), PitchClass, { 2 , 4 , 5 , 7 , 9 , 10 });
	register_scale (TwelveTone, _("Locrian"), PitchClass, { 1 , 3 , 4 , 6 , 8 , 10 });
	register_scale (TwelveTone, _("Pentatonic Major"), PitchClass, {  2 , 4 , 5 , 7 });
	register_scale (TwelveTone, _("Pentatonic Minor"), PitchClass, { 3 , 5 , 7 , 10 });
	register_scale (TwelveTone, _("Chromatic"), PitchClass, { 1 , 2 , 3 , 4 , 5 , 6 , 7 , 8 , 9 , 10 , 11 });
	register_scale (TwelveTone, _("Neapolitan Minor"), PitchClass, { 1 , 3 , 5 , 5 , 8 , 11 });
	register_scale (TwelveTone, _("Neapolitan Major"), PitchClass, { 1 , 3 , 5 , 7 , 9 , 11 });
	register_scale (TwelveTone, _("Oriental"), PitchClass, {1 , 4 , 5 , 6 , 9 , 10 });
	register_scale (TwelveTone, _("Double Harmonic"), PitchClass, { 1 , 3, 5, 7, 8, 11 });
	register_scale (TwelveTone, _("Enigmatic"), PitchClass, { 1 , 4 , 6 , 8 , 10 , 11 });
	register_scale (TwelveTone, _("Hirajoshi"), PitchClass, { 2 , 3 , 7 , 8 });
	register_scale (TwelveTone, _("Hungarian Minor"), PitchClass, { 2 , 3 , 6 , 7 , 8 , 11 });
	register_scale (TwelveTone, _("Hungarian Major"), PitchClass, { 2 , 4 , 6 , 7 , 8 , 10 });
	register_scale (TwelveTone, _("Kumoi"), PitchClass, { 1 , 5 , 7 , 8 });
	register_scale (TwelveTone, _("Iwato"), PitchClass, { 1 , 5 , 6 , 10 });
	register_scale (TwelveTone, _("Hindu"), PitchClass, { 2 , 4 , 5 , 7 , 8 , 10 });
	register_scale (TwelveTone, _("Spanish 8 Tone"), PitchClass, { 1 , 3 , 4 , 5 , 6 , 8 , 10 });
	register_scale (TwelveTone, _("Pelog"), PitchClass, { 1 , 3 , 7 , 10 });
	register_scale (TwelveTone, _("Hungarian Gypsy"), PitchClass, { 2 , 3 , 6 , 7 , 8 , 10 });
	register_scale (TwelveTone, _("Overtone"), PitchClass, { 2 , 4 , 6 , 7 , 9 , 10 });
	register_scale (TwelveTone, _("Leading Semitone"), PitchClass, { 2 , 4 , 6 , 8 , 10 , 11 });
	register_scale (TwelveTone, _("Arabian"), PitchClass, { 2 , 4 , 5 , 6 , 8 , 10 });
	register_scale (TwelveTone, _("Balinese"), PitchClass, { 1 , 3 , 7 , 8 });
	register_scale (TwelveTone, _("Gypsy"), PitchClass, { 2, 3, 6, 7, 8, 10 });
	register_scale (TwelveTone, _("Mohammedan"), PitchClass, { 2 , 3 , 5 , 7 , 8 , 11 });
	register_scale (TwelveTone, _("Javanese"), PitchClass, { 1 , 3 , 5 , 7 , 9 , 10 });
	register_scale (TwelveTone, _("Persian"), PitchClass, { 1 , 4 , 5 , 6 , 8 , 11 });
	register_scale (TwelveTone, _("Algerian"), PitchClass, { 2 , 3, 5, 6, 7, 8, 11 });
}

void
MusicalMode::register_scale (TuningSystem ts, std::string const & name, MusicalModeType type, std::vector<float> const & ints)
{
	MusicalMode mm (ts, name, type, ints);
	modes_by_tuning.insert (std::make_pair (ts, mm));
	modes_by_name.insert (std::make_pair (name, mm));
	int id = mm.ring_id ();

	if (id > 0) {
		auto ret = modes_by_id.find (id);
		if (ret != modes_by_id.end()) {
			std::cerr << "trying to insert " << name << " but ring ID matches " << ret->second.name() << std::endl;
		} else {
			modes_by_id.insert (std::make_pair (id, mm));
		}
	}
}

void
MusicalMode::register_scales (TuningSystem ts, std::vector<std::string> const & names, MusicalModeType type, std::vector<float> const & ints)
{
	assert (!names.empty());
	MusicalMode mm (ts, names.front(), type, ints);
	modes_by_tuning.insert (std::make_pair (ts, mm));
	for (auto const & nom : names) {
		modes_by_name.insert (std::make_pair (nom, mm));
	}

	int id = mm.ring_id ();

	if (id > 0) {
		auto ret = modes_by_id.find (id);
		if (ret != modes_by_id.end()) {
			std::cerr << "trying to insert " << names.front() << " but ring ID matches " << ret->second.name() << std::endl;
		} else {
			modes_by_id.insert (std::make_pair (id, mm));
		}
	}
}

MusicalMode::MusicalMode (TuningSystem ts, std::string const & name, MusicalModeType type, std::vector<float> const & elements)
	: _tuning (ts)
	, _ring_id (0)
	, _name (name)
	, _type (type)
{
	init ();

	_elements.push_back (0); /* root */
	_elements.insert (_elements.end(), elements.begin(), elements.end());
}

MusicalMode::MusicalMode (MusicalMode const & other)
	: _tuning (other._tuning)
	, _ring_id (other._ring_id)
	, _name (other._name)
	, _type (other._type)
	, _elements (other._elements)
{
	init ();
}

MusicalMode::MusicalMode (std::string const & name)
{
	init ();

	auto ret = modes_by_name.find (name);
	if (ret == modes_by_name.end()) {
		throw failed_constructor();
	}

	*this = ret->second;
}

MusicalMode::MusicalMode (std::ifstream& file)
	: _ring_id (0)
{
	init ();

	/* XXXX need to set tuning system */

	try {
		scala::scale scl (scala::read_scl (file));

		_type = RatioFromRoot;

		for (size_t i = 0; i < scl.get_scale_length(); i++){
			_elements.push_back (scl.get_ratio (i));
		}

	} catch (...) {
		throw failed_constructor ();
	}
}

MusicalMode&
MusicalMode::operator= (MusicalMode const & other)
{
	_tuning = other._tuning;
	_ring_id = other._ring_id;
	_name = other._name;
	_type = other._type;
	_elements = other._elements;

	return *this;
}

int
MusicalMode::ring_id () const
{
	if (_ring_id != 0) {
		return _ring_id;
	}

	auto ret = tones_per_equivalent.find (_tuning);

	if (ret == tones_per_equivalent.end()) {
		return -1;
	}

	int tpe = ret->second;
	std::vector<bool> intervals_included;
	intervals_included.assign (tpe, false);
	intervals_included[0] = 1; /* root note always included */

	switch (_type) {
	case AbsolutePitch:
	case RatioSteps:
	case RatioFromRoot:
		/* Must use name to look up this scale */
		_ring_id = -1;
		break;

	case PitchClass:
		for (auto e : _elements) {
			int interval = int (floor (e));
			intervals_included[interval] = true;
		}
		break;

	case MidiNote:
		for (auto e : _elements) {
			int interval = int (floor (e));
			intervals_included[interval] = true;
		}
		break;

	case WholeToneSteps:
		for (auto e : _elements) {
			int interval = int (floor (e * 2));
			intervals_included[interval] = true;
		}
		break;
	}


	int mult = 1;

	for (auto yn : intervals_included) {
		if (yn) {
			_ring_id += mult;
		}
		mult *= 2;
	}

	_ring_id += (tpe << 16);

	return _ring_id;
}

void
MusicalMode::set_name (std::string const & str)
{
	_name = str;
	NameChanged(); /* EMIT SIGNAL */
}


/** Return a sorted vector of all MIDI notes in a musical mode.
 *
 * The returned vector has every possible MIDI note number (0 through 127
 * inclusive) that is in the mode in any octave.
 */
std::vector<int>
MusicalMode::as_midi (int scale_root) const
{
	switch (_type) {
	case AbsolutePitch:
		return absolute_pitch_as_midi (scale_root);
	case PitchClass:
		return pitch_class_as_midi (scale_root);
	case WholeToneSteps:
		return wholetone_steps_as_midi (scale_root);
	case RatioSteps:
		return ratio_steps_as_midi (scale_root);
	case RatioFromRoot:
		return ratio_from_root_as_midi (scale_root);
	case MidiNote:
		return midi_note_as_midi (scale_root);
	}

	/*NOTREACHED*/
	return std::vector<int> ();
}

std::vector<int>
MusicalMode::absolute_pitch_as_midi (int root) const
{
	std::vector<int> midi_notes;
	midi_notes.reserve (128);

	/* You need MTS (MIDI Tuning Standard) to get MIDI to use absolute
	 * pitch values, so this type of scale definition just returns the MIDI
	 * note numbers 0-127. There is no other accurate answer that can be
	 * given without MTS.
	 */

	for (int n = 0; n < 128; ++n) {
		midi_notes.push_back (n);
	}
	return midi_notes;
}


std::vector<int>
MusicalMode::midi_note_as_midi (int root) const
{
	std::vector<int> midi_notes;
	midi_notes.reserve (_elements.size());

	for (auto e : _elements) {
		midi_notes.push_back (floor (e));
	}

	return midi_notes;
}

std::vector<int>
MusicalMode::pitch_class_as_midi (int scale_root) const
{
	std::vector<int> midi_notes;

	int root = (scale_root % 12) - 12;

	// Repeatedly loop through the intervals in an octave
	for (std::vector<float>::const_iterator i = _elements.begin ();;) {
		if (i == _elements.end ()) {
			// Reached the end of the scale, continue with the next octave
			root += 12;
			if (root > 127) {
				break;
			}

			midi_notes.push_back (root);
			i = _elements.begin ();

		} else {
			const int note = (int)floor (root + *i);
			if (note > 127) {
				break;
			}

			if (note > 0) {
				midi_notes.push_back (note);
			}

			++i;
		}
	}

	return midi_notes;
}

std::vector<int>
MusicalMode::wholetone_steps_as_midi (int scale_root) const
{
	std::vector<int> midi_notes;

	int root = (scale_root % 12) - 12;

	// Repeatedly loop through the intervals in an octave
	for (std::vector<float>::const_iterator i = _elements.begin ();;) {
		if (i == _elements.end ()) {
			// Reached the end of the scale, continue with the next octave
			root += 12;
			if (root > 127) {
				break;
			}

			midi_notes.push_back (root);
			i = _elements.begin ();

		} else {
			const int note = (int)floor (root + (2.0 * (*i)));
			if (note > 127) {
				break;
			}

			if (note > 0) {
				midi_notes.push_back (note);
			}

			++i;
		}
	}

	return midi_notes;
}

std::vector<int>
MusicalMode::ratio_steps_as_midi (int root) const
{
	std::vector<int> midi_notes;
	return midi_notes;
}

std::vector<int>
MusicalMode::ratio_from_root_as_midi (int root) const
{
	std::vector<int> midi_notes;
	return midi_notes;
}


/*---------*/

MusicalKey::MusicalKey (float root, MusicalMode const & sc)
	: MusicalMode (sc)
	, _root (root)
{
	/* Not sure this is conceptually correct. We haven't specified what the
	   units of "root" are to the caller.
	*/
	switch (_tuning) {
	case TwelveTone:
		_root = int (_root) % 12;
		break;
	}
}

MusicalKey::MusicalKey (float root, std::string const & mode_name)
	: MusicalMode (mode_name)
	, _root (root)
{
	/* Not sure this is conceptually correct. We haven't specified what the
	   units of "root" are to the caller.
	*/
	switch (_tuning) {
	case TwelveTone:
		_root = int (_root) % 12;
		break;
	}
}

MusicalKey::MusicalKey (MusicalKey const & other)
	: MusicalMode (other)
	, _root (other._root)
{
}

float
MusicalKey::nth (unsigned n) const
{
	switch (_type) {
	case PitchClass:
		return _root + _elements[n%_elements.size()];
		break;
	}

	return -1.f;
}

bool
MusicalKey::in_key (int midi_note) const
{
	if (_midi_notes.empty()) {
		_midi_notes = as_midi (_root);
	}

	return (std::find (_midi_notes.begin(), _midi_notes.end(), midi_note) != _midi_notes.end());
}

int
MusicalKey::lower_midi_note (int midi_note) const
{
	if (_midi_notes.empty()) {
		_midi_notes = as_midi (_root);
	}

	assert (!_midi_notes.empty());

	if (midi_note < _midi_notes.front()) {
		return _midi_notes.front();
	}

	if (midi_note > _midi_notes.back()) {
		return _midi_notes.back();
	}

	auto lb = std::lower_bound (_midi_notes.begin(), _midi_notes.end(), midi_note);

	assert (lb != _midi_notes.end()); /* addressed in previous conditionals */
	assert (lb != _midi_notes.begin()); /* addressed in previous conditionals */

	--lb;

	/* *lb could equal midi_note or be lower than it */

	return *lb;
}

int
MusicalKey::higher_midi_note (int midi_note) const
{
	if (_midi_notes.empty()) {
		_midi_notes = as_midi (_root);
	}

	assert (!_midi_notes.empty());

	if (midi_note < _midi_notes.front()) {
		return _midi_notes.front();
	}

	if (midi_note > _midi_notes.back()) {
		return _midi_notes.back();
	}

	auto ub = std::upper_bound (_midi_notes.begin(), _midi_notes.end(), midi_note);

	assert (ub != _midi_notes.end()); /* addressed in previous conditionals */

	/* *ub must be > midi_note */

	return *ub;
}

int
MusicalKey::closest_midi_note (int midi_note) const
{
	if (_midi_notes.empty()) {
		_midi_notes = as_midi (_root);
	}

	assert (!_midi_notes.empty());

	if (midi_note < _midi_notes.front()) {
		return _midi_notes.front();
	}

	if (midi_note > _midi_notes.back()) {
		return _midi_notes.back();
	}

	auto lb = std::lower_bound (_midi_notes.begin(), _midi_notes.end(), midi_note);

	assert (lb != _midi_notes.end()); /* addressed in previous conditionals */
	assert (lb != _midi_notes.begin()); /* addressed in previous conditionals */

	if (midi_note == *lb) {
		return midi_note;
	}

	/* lower bound did not equal midi_note, so it must be lower */

	int prev = *lb;
	++lb;
	int next = *lb;

	if ((midi_note - prev) < (next - midi_note)) {
		return prev;
	}

	return next;
}

int
MusicalKey::conform_midi_note (int midi_note, KeyEnforcementPolicy key_enforcment_policy) const
{
	if (key_enforcment_policy & NoInsert) {
		DEBUG_TRACE (DEBUG::MidiIO, string_compose ("dropped note %1 due to key enforcement policy for %2\n", midi_note, name()));
		return -1;
	}

	int new_note;

	if (key_enforcment_policy & ForceNearest) {
		int old = midi_note;
		new_note = closest_midi_note (midi_note);
		std::cerr << string_compose ("mutated note %1 to %2 due to key enforcement policy for %2\n", new_note, old, name());
	} else if (key_enforcment_policy & ForceLower) {
		int old = midi_note;
		new_note = lower_midi_note (midi_note);
		std::cerr << string_compose ("mutated note %1 to %2 due to key enforcement policy for %2\n", new_note, old, name());
	} else if (key_enforcment_policy & ForceHigher) {
		int old = midi_note;
		new_note = higher_midi_note (midi_note);
		std::cerr << string_compose ("mutated note %1 to %2 due to key enforcement policy for %2\n", new_note, old, name());
	} else {
		new_note = midi_note;
	}

	return new_note;
}

std::string
MusicalKey::name() const
{
	return string_compose (_("%1 %2"), (char) (_root + 'C'), mode_name());
}

std::string
MusicalKey::mode_name() const
{
	return MusicalMode::name();
}
