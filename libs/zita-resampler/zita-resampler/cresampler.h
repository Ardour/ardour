// ----------------------------------------------------------------------------
//
//  Copyright (C) 2013 Fons Adriaensen <fons@linuxaudio.org>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// ----------------------------------------------------------------------------


#ifndef _ZITA_CRESAMPLER_H_
#define _ZITA_CRESAMPLER_H_

#include "zita-resampler/zresampler_visibility.h"

namespace ArdourZita {

class LIBZRESAMPLER_API CResampler
{
public:
	CResampler (void);
	~CResampler (void);

	int  setup (double ratio, unsigned int nchan);

	void   clear (void);
	int    reset (void);
	int    nchan (void) const { return _nchan; }
	int    inpsize (void) const;
	double inpdist (void) const;
	int    process (void);

	void set_ratio (double r);
	void set_phase (double p);

	unsigned int         inp_count;
	unsigned int         out_count;
	float               *inp_data;
	float               *out_data;
	void                *inp_list;
	void                *out_list;

private:
	unsigned int         _nchan;
	unsigned int         _inmax;
	unsigned int         _index;
	unsigned int         _nread;
	unsigned int         _nzero;
	double               _phase;
	double               _pstep;
	float               *_buff;
};

};

#endif
