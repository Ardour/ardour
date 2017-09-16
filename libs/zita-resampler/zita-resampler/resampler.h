// ----------------------------------------------------------------------------
//
//  Copyright (C) 2006-2012 Fons Adriaensen <fons@linuxaudio.org>
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


#ifndef _ZITA_RESAMPLER_H_
#define _ZITA_RESAMPLER_H_

#include "zita-resampler/zresampler_visibility.h"
#include "zita-resampler/resampler-table.h"

namespace ArdourZita {

class LIBZRESAMPLER_API Resampler
{

public:

	Resampler (void);
	~Resampler (void);

	int  setup (unsigned int fs_inp,
	            unsigned int fs_out,
	            unsigned int nchan,
	            unsigned int hlen);

	int  setup (unsigned int fs_inp,
	            unsigned int fs_out,
	            unsigned int nchan,
	            unsigned int hlen,
	            double       frel);

	void   clear (void);
	int    reset (void);
	int    nchan (void) const { return _nchan; }
	int    inpsize (void) const;
	double inpdist (void) const;
	int    process (void);

	unsigned int         inp_count;
	unsigned int         out_count;
	float               *inp_data;
	float               *out_data;
	void                *inp_list;
	void                *out_list;

private:

	Resampler_table     *_table;
	unsigned int         _nchan;
	unsigned int         _inmax;
	unsigned int         _index;
	unsigned int         _nread;
	unsigned int         _nzero;
	unsigned int         _phase;
	unsigned int         _pstep;
	float               *_buff;
	void                *_dummy [8];
};

};

#endif
