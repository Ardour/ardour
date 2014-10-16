/*
    Copyright (C) 2008-2011 Fons Adriaensen <fons@linuxaudio.org>
    Adopted for Ardour 2013 by Robin Gareus <robin@gareus.org>

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

#ifndef __KMETERDSP_H
#define	__KMETERDSP_H

#include "ardour/libardour_visibility.h"

class LIBARDOUR_API Kmeterdsp
{
public:

    Kmeterdsp (void);
    ~Kmeterdsp (void);

    void process (float const *p, int n);
    float read ();
    void reset ();

    static void init (int fsamp);

private:

    float          _z1;          // filter state
    float          _z2;          // filter state
    float          _rms;         // max rms value since last read()
    bool           _flag;        // flag set by read(), resets _rms

    static float   _omega;       // ballistics filter constant.
};

#endif
