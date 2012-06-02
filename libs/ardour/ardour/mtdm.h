/*
    Copyright (C) 2003-2008 Fons Adriaensen <fons@kokkinizita.net>

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

#ifndef __libardour_mtdm_h__
#define __libardour_mtdm_h__

#include <stddef.h>

class MTDM {
public:

	MTDM ();

	int process (size_t len, float *inp, float *out);
	int resolve ();
	void invert () { _inv ^= 1; }
	int    inv () { return _inv; }
	double del () { return _del; }
	double err () { return _err; }

private:
	class Freq {
		public:
			int   p;
			int   f;
			float a;
			float xa;
			float ya;
			float xf;
			float yf;
	};

	double  _del;
	double  _err;
	int     _cnt;
	int     _inv;
	Freq    _freq [5];
};

#endif /* __libardour_mtdm_h__ */
