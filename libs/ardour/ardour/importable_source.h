/*
    Copyright (C) 2007 Paul Davis 

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

#ifndef __ardour_importable_source_h__
#define __ardour_importable_source_h__

#include <sndfile.h>
#include <ardour/types.h>

namespace ARDOUR {

class ImportableSource {
   public:
    ImportableSource (SNDFILE* sf, SF_INFO* info) : in (sf), sf_info (info) {}
    virtual ~ImportableSource() {}

    virtual nframes_t read (Sample* buffer, nframes_t nframes) {
	    nframes_t per_channel = nframes / sf_info->channels;
	    per_channel = sf_readf_float (in, buffer, per_channel);
	    return per_channel * sf_info->channels;
    }

    virtual float ratio() const { return 1.0f; }

    uint channels() const { return sf_info->channels; }

    nframes_t length() const { return sf_info->frames; }

protected:
       SNDFILE* in;
       SF_INFO* sf_info;
};

}

#endif /* __ardour_importable_source_h__ */
