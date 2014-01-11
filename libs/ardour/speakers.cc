/*
    Copyright (C) 2010 Paul Davis

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

#include "pbd/error.h"
#include "pbd/convert.h"
#include "pbd/locale_guard.h"

#include "ardour/speaker.h"
#include "ardour/speakers.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

Speaker::Speaker (int i, const AngularVector& position)
	: id (i)
{
	move (position);
}

Speaker::Speaker (Speaker const & o)
	: id (o.id)
	, _coords (o._coords)
	, _angles (o._angles)
{

}

Speaker &
Speaker::operator= (Speaker const & o)
{
	if (&o == this) {
		return *this;
	}

	id = o.id;
	_coords = o._coords;
	_angles = o._angles;

	return *this;
}

void
Speaker::move (const AngularVector& new_position)
{
	_angles = new_position;
	_angles.cartesian (_coords);

	PositionChanged (); /* EMIT SIGNAL */
}

Speakers::Speakers ()
{
}

Speakers::Speakers (const Speakers& s)
	: Stateful ()
{
        _speakers = s._speakers;
}

Speakers::~Speakers ()
{
}

Speakers&
Speakers::operator= (const Speakers& s)
{
        if (&s != this) {
                _speakers = s._speakers;
        }
        return *this;
}

void
Speakers::dump_speakers (ostream& o)
{
	for (vector<Speaker>::iterator i = _speakers.begin(); i != _speakers.end(); ++i) {
		o << "Speaker " << (*i).id << " @ "
		  << (*i).coords().x << ", " << (*i).coords().y << ", " << (*i).coords().z
		  << " azimuth " << (*i).angles().azi
		  << " elevation " << (*i).angles().ele
		  << " distance " << (*i).angles().length
		  << endl;
	}
}

void
Speakers::clear_speakers ()
{
	_speakers.clear ();
	update ();
}

int
Speakers::add_speaker (const AngularVector& position)
{
	int id = _speakers.size();

	_speakers.push_back (Speaker (id, position));
	update ();

	Changed ();

	return id;
}

void
Speakers::remove_speaker (int id)
{
	for (vector<Speaker>::iterator i = _speakers.begin(); i != _speakers.end(); ++i) {
		if (i->id == id) {
			i = _speakers.erase (i);
			update ();
			break;
		}
	}
}

void
Speakers::move_speaker (int id, const AngularVector& new_position)
{
	for (vector<Speaker>::iterator i = _speakers.begin(); i != _speakers.end(); ++i) {
		if ((*i).id == id) {
			(*i).move (new_position);
			update ();
			break;
		}
	}
}

void
Speakers::setup_default_speakers (uint32_t n)
{
	double o = 90.0;

        /* default assignment of speaker position for n speakers */

        assert (n>0);

	switch (n) {
        case 1:
                add_speaker (AngularVector (o   +0.0, 0.0));
                break;

        case 2:
                add_speaker (AngularVector (o  +60.0, 0.0));
                add_speaker (AngularVector (o  -60.0, 0.0));
                break;

	case 3:
                add_speaker (AngularVector (o  +60.0, 0.0));
                add_speaker (AngularVector (o  -60.0, 0.0));
                add_speaker (AngularVector (o +180.0, 0.0));
		break;
	case 4:
		/* 4.0 with regular spacing */
                add_speaker (AngularVector (o  +45.0, 0.0));
                add_speaker (AngularVector (o  -45.0, 0.0));
                add_speaker (AngularVector (o +135.0, 0.0));
                add_speaker (AngularVector (o -135.0, 0.0));
		break;
	case 5:
		/* 5.0 with regular spacing */
                add_speaker (AngularVector (o  +72.0, 0.0));
                add_speaker (AngularVector (o  -72.0, 0.0));
                add_speaker (AngularVector (o   +0.0, 0.0));
                add_speaker (AngularVector (o +144.0, 0.0));
                add_speaker (AngularVector (o -144.0, 0.0));
		break;
	case 6:
		/* 6.0 with regular spacing */
                add_speaker (AngularVector (o  +60.0, 0.0));
                add_speaker (AngularVector (o  -60.0, 0.0));
                add_speaker (AngularVector (o   +0.0, 0.0));
                add_speaker (AngularVector (o +120.0, 0.0));
                add_speaker (AngularVector (o -120.0, 0.0));
                add_speaker (AngularVector (o +180.0, 0.0));
		break;
	case 7:
		/* 7.0 with regular front spacing */
                add_speaker (AngularVector (o  +45.0, 0.0));
                add_speaker (AngularVector (o  -45.0, 0.0));
                add_speaker (AngularVector (o   +0.0, 0.0));
                add_speaker (AngularVector (o  +90.0, 0.0));
                add_speaker (AngularVector (o  -90.0, 0.0));
                add_speaker (AngularVector (o +150.0, 0.0));
                add_speaker (AngularVector (o -150.0, 0.0));
		break;
	case 10:
		/* 5+4 with 45°/90° spacing */
                add_speaker (AngularVector (o  +45.0, 0.0));
                add_speaker (AngularVector (o  -45.0, 0.0));
                add_speaker (AngularVector (o   +0.0, 0.0));
                add_speaker (AngularVector (o +135.0, 0.0));
                add_speaker (AngularVector (o -135.0, 0.0));
                add_speaker (AngularVector (o  +45.0, 60.0));
                add_speaker (AngularVector (o  -45.0, 60.0));
                add_speaker (AngularVector (o +135.0, 60.0));
                add_speaker (AngularVector (o -135.0, 60.0));
                add_speaker (AngularVector (o   +0.0, 90.0));
		break;

	default:
	{
		double degree_step = 360.0 / n;
		double deg;
		uint32_t i;

		/* even number of speakers? make sure the top two are either side of "top".
		   otherwise, just start at the "top" (90.0 degrees) and rotate around
		*/

		if (n % 2) {
			deg = 90.0 - degree_step;
		} else {
			deg = 90.0;
		}
		for (i = 0; i < n; ++i, deg += degree_step) {
			add_speaker (AngularVector (deg, 0.0));
		}
	}
        }
}

XMLNode&
Speakers::get_state ()
{
        XMLNode* node = new XMLNode (X_("Speakers"));
        char buf[32];
        LocaleGuard lg (X_("POSIX"));

        for (vector<Speaker>::const_iterator i = _speakers.begin(); i != _speakers.end(); ++i) {
                XMLNode* speaker = new XMLNode (X_("Speaker"));

                snprintf (buf, sizeof (buf), "%.12g", (*i).angles().azi);
                speaker->add_property (X_("azimuth"), buf);
                snprintf (buf, sizeof (buf), "%.12g", (*i).angles().ele);
                speaker->add_property (X_("elevation"), buf);
                snprintf (buf, sizeof (buf), "%.12g", (*i).angles().length);
                speaker->add_property (X_("distance"), buf);

                node->add_child_nocopy (*speaker);
        }

        return *node;
}

int
Speakers::set_state (const XMLNode& node, int /*version*/)
{
        XMLNodeConstIterator i;
        const XMLProperty* prop;
        double a, e, d;
        LocaleGuard lg (X_("POSIX"));
        int n = 0;

        _speakers.clear ();

        for (i = node.children().begin(); i != node.children().end(); ++i, ++n) {
                if ((*i)->name() == X_("Speaker")) {
                        if ((prop = (*i)->property (X_("azimuth"))) == 0) {
                                warning << _("Speaker information is missing azimuth - speaker ignored") << endmsg;
                                continue;
                        }
                        a = atof (prop->value());

                        if ((prop = (*i)->property (X_("elevation"))) == 0) {
                                warning << _("Speaker information is missing elevation - speaker ignored") << endmsg;
                                continue;
                        }
                        e = atof (prop->value());

                        if ((prop = (*i)->property (X_("distance"))) == 0) {
                                warning << _("Speaker information is missing distance - speaker ignored") << endmsg;
                                continue;
                        }
                        d = atof (prop->value());

                        add_speaker (AngularVector (a, e, d));
                }
        }

        update ();

        return 0;
}
