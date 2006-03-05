#include <map>
#include <vector>

#include <sndfile.h>
#include <ardour/sndfile_helpers.h>

#include "i18n.h"

using std::map;
using namespace std;

const char * const sndfile_header_formats_strings[SNDFILE_HEADER_FORMATS+1] = {
	N_("WAV"),
	N_("AIFF"),
	N_("raw (no header)"),
	N_("PAF (Ensoniq Paris)"),
	N_("AU (Sun/NeXT)"),
	N_("IRCAM"),
	N_("W64 (64 bit WAV)"),
	0
};

const char* const sndfile_file_endings_strings[SNDFILE_HEADER_FORMATS+1] = {
	N_(".wav"),
	N_(".aiff"),
	N_(".raw"),
	N_(".paf"),
	N_(".au"),
	N_(".ircam"),
	N_(".w64"),
	0
};

int sndfile_header_formats[SNDFILE_HEADER_FORMATS] = {
	SF_FORMAT_WAV,
	SF_FORMAT_AIFF,
	SF_FORMAT_RAW,
	SF_FORMAT_PAF,
	SF_FORMAT_AU,
	SF_FORMAT_IRCAM,
	SF_FORMAT_W64
};

const char * const sndfile_bitdepth_formats_strings[SNDFILE_BITDEPTH_FORMATS+1] = {
	N_("16 bit"),
	N_("24 bit"),
	N_("32 bit"),
	N_("8 bit"),
	N_("float"),
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
	N_("Big-endian (Mac)"),
	0
};

int sndfile_endian_formats[SNDFILE_ENDIAN_FORMATS] = {
	SF_ENDIAN_LITTLE,
	SF_ENDIAN_BIG
};

int
sndfile_header_format_from_string (string str)
{
	for (int n = 0; sndfile_header_formats_strings[n]; ++n) {
		if (str == sndfile_header_formats_strings[n]) {
			return sndfile_header_formats[n];
		}
	}
	return -1;
}

int
sndfile_bitdepth_format_from_string (string str)
{
	for (int n = 0; sndfile_bitdepth_formats_strings[n]; ++n) {
		if (str == sndfile_bitdepth_formats_strings[n]) {
			return sndfile_bitdepth_formats[n];
		}
	}
	return -1;
}

int
sndfile_endian_format_from_string (string str)
{
	for (int n = 0; sndfile_endian_formats_strings[n]; ++n) {
		if (str == sndfile_endian_formats_strings[n]) {
			return sndfile_endian_formats[n];
		}
	}
	return -1;
}

string
sndfile_file_ending_from_string (string str)
{
	static vector<string> file_endings;

	if (file_endings.empty()) {
		file_endings = internationalize((const char **) sndfile_file_endings_strings);
	}

	for (int n = 0; sndfile_header_formats_strings[n]; ++n) {
		if (str == sndfile_header_formats_strings[n]) {
			return file_endings[n];
		}
	}
	return 0;
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
