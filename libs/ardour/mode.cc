/*
 * Copyright (C) 1999-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/mode.h"

MusicalMode::MusicalMode (MusicalMode::Type t)
{
	fill (*this, t);
}

MusicalMode::~MusicalMode()
{
}

void
MusicalMode::fill (MusicalMode& m, MusicalMode::Type t)
{
	m.steps.clear ();

	/* scales/modes as distances from root, expressed
	   in fractional whole tones.
	*/

	switch (t) {
	case Dorian:
		m.steps.push_back (1.0);
		m.steps.push_back (1.5);
		m.steps.push_back (2.0);
		m.steps.push_back (3.0);
		m.steps.push_back (4.0);
		m.steps.push_back (4.5);
		m.steps.push_back (5.5);
		break;
	case IonianMajor:
		m.steps.push_back (1.0);
		m.steps.push_back (2.0);
		m.steps.push_back (2.5);
		m.steps.push_back (3.5);
		m.steps.push_back (4.5);
		m.steps.push_back (5.5);
		break;
	case AeolianMinor:
		m.steps.push_back (1.0);
		m.steps.push_back (1.5);
		m.steps.push_back (2.5);
		m.steps.push_back (3.5);
		m.steps.push_back (4.0);
		m.steps.push_back (5.0);
		break;
	case HarmonicMinor:
		m.steps.push_back (1.0);
		m.steps.push_back (1.5);
		m.steps.push_back (2.5);
		m.steps.push_back (3.5);
		m.steps.push_back (5.0);
		m.steps.push_back (5.5);
		break;
	case  BluesScale:
		m.steps.push_back (1.0);
		m.steps.push_back (1.5);
		m.steps.push_back (2.5);
		m.steps.push_back (3);
		m.steps.push_back (3.5);
		m.steps.push_back (4.5);
		m.steps.push_back (5.0);
		m.steps.push_back (5.5);
		break;
	case MelodicMinorAscending:
		m.steps.push_back (1.0);
		m.steps.push_back (1.5);
		m.steps.push_back (2.5);
		m.steps.push_back (3.5);
		m.steps.push_back (4.5);
		m.steps.push_back (5.5);
		break;
	case MelodicMinorDescending:
		m.steps.push_back (1.0);
		m.steps.push_back (2.0);
		m.steps.push_back (2.5);
		m.steps.push_back (3.5);
		m.steps.push_back (4.5);
		m.steps.push_back (5.0);
		break;
	case Phrygian:
		m.steps.push_back (0.5);
		m.steps.push_back (1.5);
		m.steps.push_back (2.5);
		m.steps.push_back (3.5);
		m.steps.push_back (4.0);
		m.steps.push_back (5.0);
		break;
	case Lydian:
		m.steps.push_back (1.0);
		m.steps.push_back (2.0);
		m.steps.push_back (3.0);
		m.steps.push_back (3.5);
		m.steps.push_back (4.5);
		m.steps.push_back (5.5);
		break;
	case Mixolydian:
		m.steps.push_back (1.0);
		m.steps.push_back (2.0);
		m.steps.push_back (2.5);
		m.steps.push_back (3.5);
		m.steps.push_back (4.5);
		m.steps.push_back (5.0);
		break;
	case Locrian:
		m.steps.push_back (0.5);
		m.steps.push_back (1.5);
		m.steps.push_back (2.0);
		m.steps.push_back (3.0);
		m.steps.push_back (4.0);
		m.steps.push_back (5.0);
		break;
	case PentatonicMajor:
		m.steps.push_back (1.0);
		m.steps.push_back (2.0);
		m.steps.push_back (2.5);
		m.steps.push_back (3.5);
		break;
	case PentatonicMinor:
		m.steps.push_back (1.5);
		m.steps.push_back (2.5);
		m.steps.push_back (3.5);
		m.steps.push_back (5.0);
		break;
	case  Chromatic:
		m.steps.push_back (0.5);
		m.steps.push_back (1.0);
		m.steps.push_back (1.5);
		m.steps.push_back (2.0);
		m.steps.push_back (2.5);
		m.steps.push_back (3.0);
		m.steps.push_back (3.5);
		m.steps.push_back (4.0);
		m.steps.push_back (4.5);
		m.steps.push_back (5.0);
		m.steps.push_back (5.5);
		break;
	case  NeapolitanMinor:
		m.steps.push_back (0.5);
		m.steps.push_back (1.5);
		m.steps.push_back (2.5);
		m.steps.push_back (2.5);
		m.steps.push_back (4.0);
		m.steps.push_back (5.5);
		break;
	case  NeapolitanMajor:
		m.steps.push_back (0.5);
		m.steps.push_back (1.5);
		m.steps.push_back (2.5);
		m.steps.push_back (3.5);
		m.steps.push_back (4.5);
		m.steps.push_back (5.5);
		break;
	case  Oriental:
		m.steps.push_back (0.5);
		m.steps.push_back (2.0);
		m.steps.push_back (2.5);
		m.steps.push_back (3.0);
		m.steps.push_back (4.5);
		m.steps.push_back (5.0);
		break;
	case  DoubleHarmonic:
		m.steps.push_back (0.5);
		m.steps.push_back (2.0);
		m.steps.push_back (2.5);
		m.steps.push_back (3.5);
		m.steps.push_back (4.0);
		m.steps.push_back (5.5);
		break;
	case  Enigmatic:
		m.steps.push_back (0.5);
		m.steps.push_back (2.0);
		m.steps.push_back (3.0);
		m.steps.push_back (4.0);
		m.steps.push_back (5.0);
		m.steps.push_back (5.5);
		break;
	case  Hirajoshi:
		m.steps.push_back (1.0);
		m.steps.push_back (1.5);
		m.steps.push_back (3.5);
		m.steps.push_back (4.0);
		break;
	case  HungarianMinor:
		m.steps.push_back (1.0);
		m.steps.push_back (1.5);
		m.steps.push_back (3.0);
		m.steps.push_back (3.5);
		m.steps.push_back (4.0);
		m.steps.push_back (5.5);
		break;
	case  HungarianMajor:
		m.steps.push_back (1.0);
		m.steps.push_back (2.0);
		m.steps.push_back (3.0);
		m.steps.push_back (3.5);
		m.steps.push_back (4.0);
		m.steps.push_back (5.0);
		break;
	case  Kumoi:
		m.steps.push_back (0.5);
		m.steps.push_back (2.5);
		m.steps.push_back (3.5);
		m.steps.push_back (4.0);
		break;
	case  Iwato:
		m.steps.push_back (0.5);
		m.steps.push_back (2.5);
		m.steps.push_back (3.0);
		m.steps.push_back (5.0);
		break;
	case  Hindu:
		m.steps.push_back (1.0);
		m.steps.push_back (2.0);
		m.steps.push_back (2.5);
		m.steps.push_back (3.5);
		m.steps.push_back (4.0);
		m.steps.push_back (5.0);
		break;
	case  Spanish8Tone:
		m.steps.push_back (0.5);
		m.steps.push_back (1.5);
		m.steps.push_back (2.0);
		m.steps.push_back (2.5);
		m.steps.push_back (3.0);
		m.steps.push_back (4.0);
		m.steps.push_back (5.0);
		break;
	case  Pelog:
		m.steps.push_back (0.5);
		m.steps.push_back (1.5);
		m.steps.push_back (3.5);
		m.steps.push_back (5.0);
		break;
	case  HungarianGypsy:
		m.steps.push_back (1.0);
		m.steps.push_back (1.5);
		m.steps.push_back (3.0);
		m.steps.push_back (3.5);
		m.steps.push_back (4.0);
		m.steps.push_back (5.0);
		break;
	case  Overtone:
		m.steps.push_back (1.0);
		m.steps.push_back (2.0);
		m.steps.push_back (3.0);
		m.steps.push_back (3.5);
		m.steps.push_back (4.5);
		m.steps.push_back (5.0);
		break;
	case  LeadingWholeTone:
		m.steps.push_back (1.0);
		m.steps.push_back (2.0);
		m.steps.push_back (3.0);
		m.steps.push_back (4.0);
		m.steps.push_back (5.0);
		m.steps.push_back (5.5);
		break;
	case  Arabian:
		m.steps.push_back (1.0);
		m.steps.push_back (2.0);
		m.steps.push_back (2.5);
		m.steps.push_back (3.0);
		m.steps.push_back (4.0);
		m.steps.push_back (5.0);
		break;
	case  Balinese:
		m.steps.push_back (0.5);
		m.steps.push_back (1.5);
		m.steps.push_back (3.5);
		m.steps.push_back (4.0);
		break;
	case  Gypsy:
		m.steps.push_back (0.5);
		m.steps.push_back (2.0);
		m.steps.push_back (2.5);
		m.steps.push_back (3.5);
		m.steps.push_back (4.0);
		m.steps.push_back (5.5);
		break;
	case  Mohammedan:
		m.steps.push_back (1.0);
		m.steps.push_back (1.5);
		m.steps.push_back (2.5);
		m.steps.push_back (3.5);
		m.steps.push_back (4.0);
		m.steps.push_back (5.5);
		break;
	case  Javanese:
		m.steps.push_back (0.5);
		m.steps.push_back (1.5);
		m.steps.push_back (2.5);
		m.steps.push_back (3.5);
		m.steps.push_back (4.5);
		m.steps.push_back (5.0);
		break;
	case  Persian:
		m.steps.push_back (0.5);
		m.steps.push_back (2.0);
		m.steps.push_back (2.5);
		m.steps.push_back (3.0);
		m.steps.push_back (4.0);
		m.steps.push_back (5.5);
		break;
	case  Algerian:
		m.steps.push_back (1.0);
		m.steps.push_back (1.5);
		m.steps.push_back (3.0);
		m.steps.push_back (3.5);
		m.steps.push_back (4.0);
		m.steps.push_back (5.5);
		m.steps.push_back (6.0);
		m.steps.push_back (7.0);
		m.steps.push_back (7.5);
		m.steps.push_back (8.5);
		break;
	}
}
