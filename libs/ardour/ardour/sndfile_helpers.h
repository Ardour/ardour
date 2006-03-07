#ifndef __sndfile_helpers_h__
#define __sndfile_helpers_h__

#include <string>

using std::string;

// Use this define when initializing arrarys for use in sndfile_*_format()
#define SNDFILE_STR_LENGTH 32

#define SNDFILE_HEADER_FORMATS 7
extern const char * const sndfile_header_formats_strings[SNDFILE_HEADER_FORMATS+1];
extern const char * const sndfile_file_endings_strings[SNDFILE_HEADER_FORMATS+1];

extern int sndfile_header_formats[SNDFILE_HEADER_FORMATS];

#define SNDFILE_BITDEPTH_FORMATS 5
extern const char * const sndfile_bitdepth_formats_strings[SNDFILE_BITDEPTH_FORMATS+1];

extern int sndfile_bitdepth_formats[SNDFILE_BITDEPTH_FORMATS];

#define SNDFILE_ENDIAN_FORMATS 2
extern const char * const sndfile_endian_formats_strings[SNDFILE_ENDIAN_FORMATS+1];

extern int sndfile_endian_formats[SNDFILE_ENDIAN_FORMATS];

int sndfile_bitdepth_format_from_string(string);
int sndfile_header_format_from_string(string);
int sndfile_endian_format_from_string(string);
string sndfile_file_ending_from_string(string);

int sndfile_data_width (int format);

// It'd be nice if libsndfile did this for us
string sndfile_major_format(int);
string sndfile_minor_format(int);

struct SoundFileInfo {
	float    samplerate;
	uint16_t channels;
	int64_t length;
	std::string format_name;
};

bool get_soundfile_info (string path, SoundFileInfo& _info);

#endif /* __sndfile_helpers_h__ */
