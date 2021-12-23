/*
 * Copyright (C) 2000-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2013 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2014 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2013-2015 John Emmas <john@creativepost.co.uk>
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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <stdint.h>

#include <cstdio> /* for sprintf */
#include <cstring>
#include <climits>
#include <cstdlib>
#include <cmath>
#include <cctype>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#ifndef COMPILER_MSVC
#include <dirent.h>
#endif
#include <errno.h>
#include <regex.h>

#include "pbd/gstdio_compat.h"

#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>

#include "pbd/cpus.h"
#include "pbd/control_math.h"
#include "pbd/error.h"
#include "pbd/xml++.h"
#include "pbd/basename.h"
#include "pbd/scoped_file_descriptor.h"
#include "pbd/strsplit.h"
#include "pbd/replace_all.h"

#include "ardour/utils.h"
#include "ardour/rc_configuration.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;

static string
replace_chars (const string& str, const string& illegal_chars)
{
	string::size_type pos;
	Glib::ustring legal;

	/* this is the one place in Ardour where we need to iterate across
	 * potential multibyte characters, and thus we need Glib::ustring
	 */

	legal = str;
	pos = 0;

	while ((pos = legal.find_first_of (illegal_chars, pos)) != string::npos) {
		legal.replace (pos, 1, "_");
		pos += 1;
	}

	return string (legal);
}
/** take an arbitrary string as an argument, and return a version of it
 * suitable for use as a path (directory/folder name). This is the Ardour 3.X
 * and later version of this code. It defines a very small number of characters
 * that are not allowed in a path on the build target filesystem (basically,
 * POSIX or Windows) and replaces any instances of them with an underscore.
 *
 * NOTE: this is intended only to legalize for the filesystem that Ardour
 * is running on. Export should use legalize_for_universal_path() since
 * the goal there is to be legal across filesystems.
 */
string
ARDOUR::legalize_for_path (const string& str)
{
	return replace_chars (str, "/\\");
}

/** take an arbitrary string as an argument, and return a version of it
 * suitable for use as a path (directory/folder name). This is the Ardour 3.X
 * and later version of this code. It defines a small number
 * of characters that are not allowed in a path on any of our target
 * filesystems, and replaces any instances of them with an underscore.
 *
 * NOTE: this is intended to create paths that should be legal on
 * ANY filesystem.
 */
string
ARDOUR::legalize_for_universal_path (const string& str)
{
	return replace_chars (str, "<>:\"/\\|?*");
}

/** Legalize for a URI path component.  This is like
 * legalize_for_universal_path, but stricter, disallowing spaces and hash.
 * This avoids %20 escapes in URIs, but probably needs work to be more strictly
 * correct.
 */
string
ARDOUR::legalize_for_uri (const string& str)
{
	return replace_chars (str, "<>:\"/\\|?* #");
}

/** take an arbitrary string as an argument, and return a version of it
 * suitable for use as a path (directory/folder name). This is the Ardour 2.X
 * version of this code, which used an approach that came to be seen as
 * problematic: defining the characters that were allowed and replacing all
 * others with underscores. See legalize_for_path() for the 3.X and later
 * version.
 */

string
ARDOUR::legalize_for_path_2X (const string& str)
{
	string::size_type pos;
	string legal_chars = "abcdefghijklmnopqrtsuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_+=: ";
        Glib::ustring legal;

	/* this is the one place in Ardour where we need to iterate across
	 * potential multibyte characters, and thus we need Glib::ustring
	 */

	legal = str;
	pos = 0;

	while ((pos = legal.find_first_not_of (legal_chars, pos)) != string::npos) {
		legal.replace (pos, 1, "_");
		pos += 1;
	}

	return string (legal);
}

string
ARDOUR::bump_name_once (const std::string& name, char delimiter)
{
	string::size_type delim;
	string newname;

	if ((delim = name.find_last_of (delimiter)) == string::npos) {
		newname  = name;
		newname += delimiter;
		newname += "1";
	} else {
		int isnumber = 1;
		const char *last_element = name.c_str() + delim + 1;
		for (size_t i = 0; i < strlen(last_element); i++) {
			if (!isdigit(last_element[i])) {
				isnumber = 0;
				break;
			}
		}

		errno = 0;
		int32_t version = strtol (name.c_str()+delim+1, (char **)NULL, 10);

		if (isnumber == 0 || errno != 0) {
			// last_element is not a number, or is too large
			newname  = name;
			newname  += delimiter;
			newname += "1";
		} else {
			char buf[32];

			snprintf (buf, sizeof(buf), "%d", version+1);

			newname  = name.substr (0, delim+1);
			newname += buf;
		}
	}

	return newname;

}

string
ARDOUR::bump_name_number (const std::string& name)
{
	size_t pos = name.length();
	size_t num = 0;
	bool have_number = false;
	while (pos > 0 && isdigit(name.at(--pos))) {
		have_number = true;
		num = pos;
	}

	string newname;
	if (have_number) {
		int32_t seq = strtol (name.c_str() + num, (char **)NULL, 10);
		char buf[32];
		snprintf (buf, sizeof(buf), "%d", seq + 1);
		newname = name.substr (0, num);
		newname += buf;
	} else {
		newname = name;
		newname += "1";
	}

	return newname;
}

XMLNode *
ARDOUR::find_named_node (const XMLNode& node, string name)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	XMLNode* child;

	nlist = node.children();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		child = *niter;

		if (child->name() == name) {
			return child;
		}
	}

	return 0;
}

int
ARDOUR::cmp_nocase (const string& s, const string& s2)
{
	string::const_iterator p = s.begin();
	string::const_iterator p2 = s2.begin();

	while (p != s.end() && p2 != s2.end()) {
		if (toupper(*p) != toupper(*p2)) {
			return (toupper(*p) < toupper(*p2)) ? -1 : 1;
		}
		++p;
		++p2;
	}

	return (s2.size() == s.size()) ? 0 : (s.size() < s2.size()) ? -1 : 1;
}

int
ARDOUR::cmp_nocase_utf8 (const string& s1, const string& s2)
{
	const char *cstr1 = s1.c_str();
	const char *cstr2 = s2.c_str();
	gchar *cstr1folded = NULL;
	gchar *cstr2folded = NULL;
	int retval;

	if (!g_utf8_validate (cstr1, -1, NULL) ||
		!g_utf8_validate (cstr2, -1, NULL)) {
		// fall back to comparing ASCII
		return g_ascii_strcasecmp (cstr1, cstr2);
	}

	cstr1folded = g_utf8_casefold (cstr1, -1);
	cstr2folded = g_utf8_casefold (cstr2, -1);

	if (cstr1folded && cstr2folded) {
		retval = strcmp (cstr1folded, cstr2folded);
	} else {
		// this shouldn't happen, make the best of it
		retval = g_ascii_strcasecmp (cstr1, cstr2);
	}

	if (cstr1folded) {
		g_free (cstr1folded);
	}

	if (cstr2folded) {
		g_free (cstr2folded);
	}

	return retval;
}

string
ARDOUR::region_name_from_path (string path, bool strip_channels, bool add_channel_suffix, uint32_t total, uint32_t this_one)
{
	path = PBD::basename_nosuffix (path);

	if (strip_channels) {

		/* remove any "?R", "?L" or "?[a-z]" channel identifier */

		string::size_type len = path.length();

		if (len > 3 && (path[len-2] == '%' || path[len-2] == '?' || path[len-2] == '.') &&
		    (path[len-1] == 'R' || path[len-1] == 'L' || (islower (path[len-1])))) {

			path = path.substr (0, path.length() - 2);
		}
	}

	if (add_channel_suffix) {

		/* compare to Session::format_audio_source_name */
		path += '%';

		if (total > 25) {
			path += string_compose ("%1", this_one + 1);
		} else if (total > 2) {
			path += (char) ('a' + this_one);
		} else {
			path += (char) (this_one == 0 ? 'L' : 'R');
		}
	}

	return path;
}

bool
ARDOUR::path_is_paired (string path, string& pair_base)
{
	string::size_type pos;

	/* remove any leading path */

	if ((pos = path.find_last_of (G_DIR_SEPARATOR)) != string::npos) {
		path = path.substr(pos+1);
	}

	/* remove filename suffixes etc. */

	if ((pos = path.find_last_of ('.')) != string::npos) {
		path = path.substr (0, pos);
	}

	string::size_type len = path.length();

	/* look for possible channel identifier: "?R", "%R", ".L" etc. */

	if (len > 3 && (path[len-2] == '%' || path[len-2] == '?' || path[len-2] == '.') &&
	    (path[len-1] == 'R' || path[len-1] == 'L' || (islower (path[len-1])))) {

		pair_base = path.substr (0, len-2);
		return true;

	}

	return false;
}

#if __APPLE__
string
ARDOUR::CFStringRefToStdString(CFStringRef stringRef)
{
	CFIndex size =
		CFStringGetMaximumSizeForEncoding(CFStringGetLength(stringRef) ,
		kCFStringEncodingUTF8);
	    char *buf = new char[size];

	std::string result;

	if(CFStringGetCString(stringRef, buf, size, kCFStringEncodingUTF8)) {
	    result = buf;
	}
	delete [] buf;
	return result;
}
#endif // __APPLE__

void
ARDOUR::compute_equal_power_fades (samplecnt_t nframes, float* in, float* out)
{
	double step;

	step = 1.0/(nframes-1);

	in[0] = 0.0f;

	for (samplecnt_t i = 1; i < nframes - 1; ++i) {
		in[i] = in[i-1] + step;
	}

	in[nframes-1] = 1.0;

	const float pan_law_attenuation = -3.0f;
	const float scale = 2.0f - 4.0f * powf (10.0f,pan_law_attenuation/20.0f);

	for (samplecnt_t n = 0; n < nframes; ++n) {
		float inVal = in[n];
		float outVal = 1 - inVal;
		out[n] = outVal * (scale * outVal + 1.0f - scale);
		in[n] = inVal * (scale * inVal + 1.0f - scale);
	}
}

EditMode
ARDOUR::string_to_edit_mode (string str)
{
	if (str == _("Slide")) {
		return Slide;
	} else if (str == _("Ripple")) {
		return Ripple;
	} else if (str == _("Ripple All")) {
		return RippleAll;
	} else if (str == _("Lock")) {
		return Lock;
	}
	fatal << string_compose (_("programming error: unknown edit mode string \"%1\""), str) << endmsg;
	abort(); /*NOTREACHED*/
	return Slide;
}

const char*
ARDOUR::edit_mode_to_string (EditMode mode)
{
	switch (mode) {
	case Lock:
		return _("Lock");

	case Ripple:
		return _("Ripple");

	case RippleAll:
		return _("Ripple All");

	default:
	case Slide:
		return _("Slide");
	}
}

float
ARDOUR::meter_falloff_to_float (MeterFalloff falloff)
{
	switch (falloff) {
	case MeterFalloffOff:
		return METER_FALLOFF_OFF;
	case MeterFalloffSlowest:
		return METER_FALLOFF_SLOWEST;
	case MeterFalloffSlow:
		return METER_FALLOFF_SLOW;
	case MeterFalloffSlowish:
		return METER_FALLOFF_SLOWISH;
	case MeterFalloffMedium:
		return METER_FALLOFF_MEDIUM;
	case MeterFalloffModerate:
		return METER_FALLOFF_MODERATE;
	case MeterFalloffFast:
	case MeterFalloffFaster:  // backwards compat enum MeterFalloff
	case MeterFalloffFastest:
	default:
		return METER_FALLOFF_FAST;
	}
}

MeterFalloff
ARDOUR::meter_falloff_from_float (float val)
{
	if (val == METER_FALLOFF_OFF) {
		return MeterFalloffOff;
	}
	else if (val <= METER_FALLOFF_SLOWEST) {
		return MeterFalloffSlowest;
	}
	else if (val <= METER_FALLOFF_SLOW) {
		return MeterFalloffSlow;
	}
	else if (val <= METER_FALLOFF_SLOWISH) {
		return MeterFalloffSlowish;
	}
	else if (val <= METER_FALLOFF_MODERATE) {
		return MeterFalloffModerate;
	}
	else if (val <= METER_FALLOFF_MEDIUM) {
		return MeterFalloffMedium;
	}
	else {
		return MeterFalloffFast;
	}
}

AutoState
ARDOUR::string_to_auto_state (std::string str)
{
	if (str == X_("Off")) {
		return Off;
	} else if (str == X_("Play")) {
		return Play;
	} else if (str == X_("Write")) {
		return Write;
	} else if (str == X_("Touch")) {
		return Touch;
	} else if (str == X_("Latch")) {
		return Latch;
	}

	fatal << string_compose (_("programming error: %1 %2"), "illegal AutoState string: ", str) << endmsg;
	abort(); /*NOTREACHED*/
	return Touch;
}

string
ARDOUR::auto_state_to_string (AutoState as)
{
	/* to be used only for XML serialization, no i18n done */

	switch (as) {
	case Off:
		return X_("Off");
		break;
	case Play:
		return X_("Play");
		break;
	case Write:
		return X_("Write");
		break;
	case Touch:
		return X_("Touch");
		break;
	case Latch:
		return X_("Latch");
		break;
	}

	fatal << string_compose (_("programming error: %1 %2"), "illegal AutoState type: ", as) << endmsg;
	abort(); /*NOTREACHED*/
	return "";
}

std::string
bool_as_string (bool yn)
{
	return (yn ? "yes" : "no");
}

const char*
ARDOUR::native_header_format_extension (HeaderFormat hf, const DataType& type)
{
        if (type == DataType::MIDI) {
                return ".mid";
        }

        switch (hf) {
        case BWF:
                return ".wav";
        case WAVE:
                return ".wav";
        case WAVE64:
                return ".w64";
        case CAF:
                return ".caf";
        case AIFF:
                return ".aif";
        case iXML:
                return ".ixml";
        case FLAC:
                return ".flac";
        case RF64:
        case RF64_WAV:
        case MBWF:
                return ".rf64";
        }

        fatal << string_compose (_("programming error: unknown native header format: %1"), hf);
        abort(); /*NOTREACHED*/
        return ".wav";
}

bool
ARDOUR::matching_unsuffixed_filename_exists_in (const string& dir, const string& path)
{
	string bws = basename_nosuffix (path);
	struct dirent* dentry;
	GStatBuf statbuf;
	DIR* dead;
	bool ret = false;

        if ((dead = ::opendir (dir.c_str())) == 0) {
                error << string_compose (_("cannot open directory %1 (%2)"), dir, strerror (errno)) << endl;
                return false;
        }

        while ((dentry = ::readdir (dead)) != 0) {

                /* avoid '.' and '..' */

                if ((dentry->d_name[0] == '.' && dentry->d_name[1] == '\0') ||
                    (dentry->d_name[2] == '\0' && dentry->d_name[0] == '.' && dentry->d_name[1] == '.')) {
                        continue;
                }

                string fullpath = Glib::build_filename (dir, dentry->d_name);

                if (g_stat (fullpath.c_str(), &statbuf)) {
                        continue;
                }

                if (!S_ISREG (statbuf.st_mode)) {
                        continue;
                }

                string bws2 = basename_nosuffix (dentry->d_name);

                if (bws2 == bws) {
                        ret = true;
                        break;
                }
        }

        ::closedir (dead);
        return ret;
}

uint32_t
ARDOUR::how_many_dsp_threads ()
{
        /* CALLER MUST HOLD PROCESS LOCK */

        int num_cpu = hardware_concurrency();
        int pu = Config->get_processor_usage ();
        uint32_t num_threads = max (num_cpu - 1, 2); // default to number of cpus minus one, or 2, whichever is larger

        if (pu < 0) {
                /* pu is negative: use "pu" less cores for DSP than appear to be available
                 */

                if (-pu < num_cpu) {
                        num_threads = num_cpu + pu;
                }

        } else if (pu == 0) {

                /* use all available CPUs
                 */

                num_threads = num_cpu;

        } else {
                /* use "pu" cores, if available
                 */

                num_threads = min (num_cpu, pu);
        }

        return num_threads;
}

double
ARDOUR::gain_to_slider_position_with_max (double g, double max_gain)
{
	return gain_to_position (g * 2.0 / max_gain);
}

double
ARDOUR::slider_position_to_gain_with_max (double g, double max_gain)
{
	return position_to_gain (g) * max_gain / 2.0;
}

#include "sha1.c"

std::string
ARDOUR::compute_sha1_of_file (std::string path)
{
	PBD::ScopedFileDescriptor fd (g_open (path.c_str(), O_RDONLY, 0444));
	if (fd < 0) {
		return std::string ();
	}
	char buf[4096];
	ssize_t n_read;
	char hash[41];
	Sha1Digest s;
	sha1_init (&s);

	while ((n_read = ::read(fd, buf, sizeof(buf))) > 0) {
		sha1_write (&s, (const uint8_t*) buf, n_read);
	}

	sha1_result_hash (&s, hash);
	return std::string (hash);
}
