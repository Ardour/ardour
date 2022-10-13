/*
 * Copyright (C) 2000-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
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

#include <sndfile.h>
#include "ardour/sndfile_helpers.h"
#include "ardour/export_format_base.h"

using namespace std;

int
sndfile_data_width (int format)
{
	int tval = format & SF_FORMAT_SUBMASK;

	switch (tval) {
	case SF_FORMAT_PCM_S8:
	case SF_FORMAT_PCM_U8:
		return 8;
	case SF_FORMAT_PCM_16:
		return 16;
	case SF_FORMAT_PCM_24:
		return 24;
	case SF_FORMAT_PCM_32:
		return 32;
	case SF_FORMAT_FLOAT:
	case SF_FORMAT_DOUBLE:
	case ARDOUR::ExportFormatBase::SF_MPEG_LAYER_III:
		return 1; /* ridiculous but used as a magic value */
	default:
		// we don't handle anything else within ardour
		return 0;
	}
}

string
sndfile_major_format(int format)
{
	static map<int, string> m;

	if(m.empty()){
		SF_FORMAT_INFO format_info;
		int count;
		sf_command(0, SFC_GET_FORMAT_MAJOR_COUNT, &count, sizeof (int));
		for (int i = 0; i < count; ++i){
			format_info.format = i;
			sf_command (0, SFC_GET_FORMAT_MAJOR,
					&format_info, sizeof (format_info));
			m[format_info.format & SF_FORMAT_TYPEMASK] = format_info.name;

                        /* normalize a couple of names rather than use what libsndfile gives us */

                        if (strncasecmp (format_info.name, "OGG", 3) == 0) {
                                m[format_info.format & SF_FORMAT_TYPEMASK] = "Ogg";
                        } else if (strncasecmp (format_info.name, "WAV", 3) == 0) {
                                m[format_info.format & SF_FORMAT_TYPEMASK] = "WAV";
                        } else {
                                m[format_info.format & SF_FORMAT_TYPEMASK] = format_info.name;
                        }
		}
	}

	map<int, string>::iterator p = m.find(format & SF_FORMAT_TYPEMASK);
	if(p != m.end()){
		return m[format & SF_FORMAT_TYPEMASK];
	} else {
		return "-Unknown-";
	}
}

string
sndfile_minor_format(int format)
{
	static map<int, string> m;

	if(m.empty()){
		SF_FORMAT_INFO format_info;
		int count;
		sf_command(0, SFC_GET_FORMAT_SUBTYPE_COUNT, &count, sizeof (int));
		for (int i = 0; i < count; ++i){
			format_info.format = i;
			sf_command (0, SFC_GET_FORMAT_SUBTYPE,
					&format_info, sizeof (format_info));
			m[format_info.format & SF_FORMAT_SUBMASK] = format_info.name;
		}
	}

	map<int, string>::iterator p = m.find(format & SF_FORMAT_SUBMASK);
	if(p != m.end()){
		return m[format & SF_FORMAT_SUBMASK];
	} else {
		return "-Unknown-";
	}
}

