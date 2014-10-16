/*
    Copyright (C) 2000-2007 Paul Davis

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

#ifndef COMPILER_MSVC
#include <strings.h>
#endif
#include <map>

#include <sndfile.h>
#include "ardour/sndfile_helpers.h"

#include "i18n.h"

using std::map;
using namespace std;

const char * const sndfile_header_formats_strings[SNDFILE_HEADER_FORMATS+1] = {
	N_("WAV"),
	N_("AIFF"),
	N_("CAF"),
	N_("W64 (64 bit WAV)"),
	N_("FLAC"),
	N_("Ogg/Vorbis"),
	N_("raw (no header)"),
	0
};

const char* const sndfile_file_endings_strings[SNDFILE_HEADER_FORMATS+1] = {
	N_(".wav"),
	N_(".aiff"),
	N_(".caf"),
	N_(".w64"),
	N_(".flac"),
	N_(".ogg"),
	N_(".raw"),
	0
};

int sndfile_header_formats[SNDFILE_HEADER_FORMATS] = {
	SF_FORMAT_WAV,
	SF_FORMAT_AIFF,
	SF_FORMAT_CAF,
	SF_FORMAT_W64,
	SF_FORMAT_FLAC,
	SF_FORMAT_OGG,
	SF_FORMAT_RAW
};

const char * const sndfile_bitdepth_formats_strings[SNDFILE_BITDEPTH_FORMATS+1] = {
	N_("Signed 16 bit PCM"),
	N_("Signed 24 bit PCM"),
	N_("Signed 32 bit PCM"),
	N_("Signed 8 bit PCM"),
	N_("32 bit float"),
	0
};

int sndfile_bitdepth_formats[SNDFILE_BITDEPTH_FORMATS] = {
	SF_FORMAT_PCM_16,
	SF_FORMAT_PCM_24,
	SF_FORMAT_PCM_32,
	SF_FORMAT_PCM_S8,
	SF_FORMAT_FLOAT
};

const char * const sndfile_endian_formats_strings[SNDFILE_ENDIAN_FORMATS+1] = {
	N_("Little-endian (Intel)"),
	N_("Big-endian (PowerPC)"),
	0
};

int sndfile_endian_formats[SNDFILE_ENDIAN_FORMATS] = {
	SF_ENDIAN_LITTLE,
	SF_ENDIAN_BIG
};

int
sndfile_header_format_by_index (int index)
{
        if (index >= 0 && index < SNDFILE_HEADER_FORMATS) {
                return sndfile_header_formats[index];
	}
	return -1;
}

int
sndfile_bitdepth_format_by_index (int index)
{
        if (index >= 0 && index < SNDFILE_BITDEPTH_FORMATS) {
                return sndfile_bitdepth_formats[index];
	}
	return -1;
}

int
sndfile_endian_format_by_index (int index)
{
        if (index >= 0 && index < SNDFILE_ENDIAN_FORMATS) {
                return sndfile_endian_formats[index];
	}
	return -1;
}

int
sndfile_data_width (int format)
{
	int tval = format & 0xf;

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
		return 1; // heh, heh
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

