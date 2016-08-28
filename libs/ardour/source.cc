/*
    Copyright (C) 2000 Paul Davis

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

#include <sys/stat.h>
#include <unistd.h>
#include <float.h>
#include <cerrno>
#include <ctime>
#include <cmath>
#include <iomanip>
#include <algorithm>

#include "pbd/gstdio_compat.h"
#include <glibmm/threads.h>
#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>
#include "pbd/xml++.h"
#include "pbd/pthread_utils.h"
#include "pbd/enumwriter.h"
#include "pbd/types_convert.h"

#include "ardour/debug.h"
#include "ardour/profile.h"
#include "ardour/session.h"
#include "ardour/source.h"
#include "ardour/transient_detector.h"
#include "ardour/types_convert.h"

#include "pbd/i18n.h"

namespace PBD {
	DEFINE_ENUM_CONVERT(ARDOUR::Source::Flag);
}

using namespace std;
using namespace ARDOUR;
using namespace PBD;

Source::Source (Session& s, DataType type, const string& name, Flag flags)
	: SessionObject(s, name)
	, _type(type)
	, _flags(flags)
	, _timeline_position(0)
        , _use_count (0)
	, _level (0)
{
	_analysed = false;
	_timestamp = 0;
	fix_writable_flags ();
}

Source::Source (Session& s, const XMLNode& node)
	: SessionObject(s, "unnamed source")
	, _type(DataType::AUDIO)
	, _flags (Flag (Writable|CanRename))
	, _timeline_position(0)
        , _use_count (0)
	, _level (0)
{
	_timestamp = 0;
	_analysed = false;

	if (set_state (node, Stateful::loading_state_version) || _type == DataType::NIL) {
		throw failed_constructor();
	}

	fix_writable_flags ();
}

Source::~Source ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("Source %1 destructor %2\n", _name, this));
}

void
Source::fix_writable_flags ()
{
	if (!_session.writable()) {
		_flags = Flag (_flags & ~(Writable|Removable|RemovableIfEmpty|RemoveAtDestroy|CanRename));
	}
}

XMLNode&
Source::get_state ()
{
	XMLNode *node = new XMLNode ("Source");

	node->set_property ("name", name());
	node->set_property ("type", _type);
	node->set_property (X_("flags"), _flags);
	node->set_property ("id", id());

	if (_timestamp != 0) {
		node->set_property ("timestamp", (int64_t)_timestamp);
	}

	return *node;
}

int
Source::set_state (const XMLNode& node, int version)
{
	std::string str;
	if (node.get_property ("name", str)) {
		_name = str;
	} else {
		return -1;
	}

	if (!set_id (node)) {
		return -1;
	}

	node.get_property ("type", _type);

	int64_t t;
	if (node.get_property ("timestamp", t)) {
		_timestamp = t;
	}

	if (!node.get_property (X_("flags"), _flags)) {
		_flags = Flag (0);
	}

	/* old style, from the period when we had DestructiveFileSource */
	if (node.get_property (X_("destructive"), str)) {
		_flags = Flag (_flags | Destructive);
	}

	if (Profile->get_trx() && (_flags & Destructive)) {
		error << string_compose (_("%1: this session uses destructive tracks, which are not supported"), PROGRAM_NAME) << endmsg;
		return -1;
	}

	if (version < 3000) {
		/* a source with an XML node must necessarily already exist,
		   and therefore cannot be removable/writable etc. etc.; 2.X
		   sometimes marks sources as removable which shouldn't be.
		*/
		if (!(_flags & Destructive)) {
			_flags = Flag (_flags & ~(Writable|Removable|RemovableIfEmpty|RemoveAtDestroy|CanRename));
		}
	}

	return 0;
}

bool
Source::has_been_analysed() const
{
	Glib::Threads::Mutex::Lock lm (_analysis_lock);
	return _analysed;
}

void
Source::set_been_analysed (bool yn)
{
	if (yn) {
		if (0 == load_transients (get_transients_path())) {
			yn = false;
		}
	}
	if (yn != _analysed) {
		Glib::Threads::Mutex::Lock lm (_analysis_lock);
		_analysed = yn;
	}
	AnalysisChanged(); // EMIT SIGNAL
}

int
Source::load_transients (const string& path)
{
	int rv = 0;
	FILE *tf;
	if (! (tf = g_fopen (path.c_str (), "rb"))) {
		return -1;
	}

	transients.clear ();
	while (!feof (tf) && !ferror(tf)) {
		double val;
		if (1 != fscanf (tf, "%lf", &val)) {
			rv = -1;
			break;
		}

		framepos_t frame = (framepos_t) floor (val * _session.frame_rate());
		transients.push_back (frame);
	}

	::fclose (tf);
	return rv;
}

string
Source::get_transients_path () const
{
	vector<string> parts;
	string s;

	/* old sessions may not have the analysis directory */

	_session.ensure_subdirs ();

	s = _session.analysis_dir ();
	parts.push_back (s);

	s = id().to_s();
	s += '.';
	s += TransientDetector::operational_identifier();
	parts.push_back (s);

	return Glib::build_filename (parts);
}

bool
Source::check_for_analysis_data_on_disk ()
{
	/* looks to see if the analysis files for this source are on disk.
	   if so, mark us already analysed.
	*/

	string path = get_transients_path ();
	bool ok = true;

	if (!Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
		ok = false;
	}

	// XXX add other tests here as appropriate

	set_been_analysed (ok);
	return ok;
}

void
Source::mark_for_remove ()
{
	// This operation is not allowed for sources for destructive tracks or out-of-session files.

	/* XXX need a way to detect _within_session() condition here - move it from FileSource?
	 */

	if ((_flags & Destructive)) {
		return;
	}

	_flags = Flag (_flags | Removable | RemoveAtDestroy);
}

void
Source::set_timeline_position (framepos_t pos)
{
	_timeline_position = pos;
}

void
Source::set_allow_remove_if_empty (bool yn)
{
	if (!writable()) {
		return;
	}

	if (yn) {
		_flags = Flag (_flags | RemovableIfEmpty);
	} else {
		_flags = Flag (_flags & ~RemovableIfEmpty);
	}
}

void
Source::inc_use_count ()
{
        g_atomic_int_inc (&_use_count);
}

void
Source::dec_use_count ()
{
#ifndef NDEBUG
        gint oldval = g_atomic_int_add (&_use_count, -1);
        if (oldval <= 0) {
                cerr << "Bad use dec for " << name() << endl;
                abort ();
        }
        assert (oldval > 0);
#else
        g_atomic_int_add (&_use_count, -1);
#endif
}

bool
Source::writable () const
{
        return (_flags & Writable) && _session.writable();
}

