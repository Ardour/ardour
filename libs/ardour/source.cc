/*
 * Copyright (C) 2000-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005-2006 Jesse Chappell <jesse@essej.net>
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Ben Loftis <ben@harrisonconsoles.com>
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
	, _natural_position(0)
	, _have_natural_position (false)
	, _level (0)
{
	g_atomic_int_set (&_use_count, 0);
	_analysed = false;
	_timestamp = 0;

	fix_writable_flags ();
}

Source::Source (Session& s, const XMLNode& node)
	: SessionObject(s, "unnamed source")
	, _type(DataType::AUDIO)
	, _flags (Flag (Writable|CanRename))
	, _natural_position(0)
	, _have_natural_position (false)
	, _level (0)
{
	g_atomic_int_set (&_use_count, 0);
	_analysed = false;
	_timestamp = 0;

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
	XMLNode *node = new XMLNode (X_("Source"));

	node->set_property (X_("name"), name());
	node->set_property (X_("take-id"), take_id());
	node->set_property (X_("type"), _type);
	node->set_property (X_(X_("flags")), _flags);
	node->set_property (X_("id"), id());

	if (_timestamp != 0) {
		int64_t t = _timestamp;
		node->set_property (X_("timestamp"), t);
	}

	if (_have_natural_position) {
		node->set_property (X_("natural-position"), _natural_position);
	}

	if (!_xruns.empty ()) {
		stringstream str;
		for (XrunPositions::const_iterator xx = _xruns.begin(); xx != _xruns.end(); ++xx) {
			str << PBD::to_string (*xx) << '\n';
		}
		XMLNode* xnode = new XMLNode (X_("xruns"));
		XMLNode* content_node = new XMLNode (X_("foo")); /* it gets renamed by libxml when we set content */
		content_node->set_content (str.str());
		xnode->add_child_nocopy (*content_node);
		node->add_child_nocopy (*xnode);
	}

	if (!_cue_markers.empty()) {
		node->add_child_nocopy (get_cue_state());
	}

	return *node;
}

int
Source::set_state (const XMLNode& node, int version)
{
	std::string str;
	const CueMarkers old_cues = _cue_markers;
	XMLNodeList nlist = node.children();
	int64_t t;
	samplepos_t ts;

	if (node.name() == X_("Cues")) {
		/* partial state */
		int ret = set_cue_state (node, version);
		if (ret) {
			return ret;
		}
		goto out;
	}

	if (node.get_property ("name", str)) {
		_name = str;
	} else {
		return -1;
	}

	if (!set_id (node)) {
		return -1;
	}

	node.get_property ("type", _type);

	if (node.get_property ("timestamp", t)) {
		_timestamp = (time_t) t;
	}

	if (node.get_property ("natural-position", ts)) {
		_natural_position = timepos_t (ts);
		_have_natural_position = true;
	} else if (node.get_property ("timeline-position", ts)) {
		/* some older versions of ardour might have stored this with
		   this property name.
		*/
		_natural_position = timepos_t (ts);
		_have_natural_position = true;
	}

	if (!node.get_property (X_("flags"), _flags)) {
		_flags = Flag (0);
	}

	_xruns.clear ();
	for (XMLNodeIterator niter = nlist.begin(); niter != nlist.end(); ++niter) {

		if ((*niter)->name() == X_("xruns")) {

			const XMLNode& xruns (*(*niter));
			if (xruns.children().empty()) {
				break;
			}
			XMLNode* content_node = xruns.children().front();
			stringstream str (content_node->content());
			while (str) {
				samplepos_t x;
				std::string x_str;
				str >> x_str;
				if (!str || !PBD::string_to<samplepos_t> (x_str, x)) {
					break;
				}
				_xruns.push_back (x);
			}

		} else if ((*niter)->name() == X_("Cues")) {
			set_cue_state (**niter, version);
		}
	}

	/* Destructive is no longer valid */

	if (_flags & Destructive) {
		_session.set_had_destructive_tracks (true);
	}
	_flags = Flag (_flags & ~Destructive);

	if (!node.get_property (X_("take-id"), _take_id)) {
		_take_id = "";
	}

	/* old style, from the period when we had DestructiveFileSource */
	if (node.get_property (X_("destructive"), str)) {
		_session.set_had_destructive_tracks (true);
	}

	if (version < 3000) {
		/* a source with an XML node must necessarily already exist,
		   and therefore cannot be removable/writable etc. etc.; 2.X
		   sometimes marks sources as removable which shouldn't be.
		*/
		_flags = Flag (_flags & ~(Writable|Removable|RemovableIfEmpty|RemoveAtDestroy|CanRename));
	}

	/* support to make undo/redo actually function. Very few things about
	 * Sources are ever part of undo/redo history, but this can
	 * be. Undo/Redo uses a MementoCommand<> pattern, which will not in
	 * itself notify anyone when the operation changes the cue markers.
	 */

  out:
	if (old_cues != _cue_markers) {
		CueMarkersChanged (); /* EMIT SIGNAL */
	}

	return 0;
}

XMLNode&
Source::get_cue_state () const
{
	XMLNode* cue_parent = new XMLNode (X_("Cues"));

	for (CueMarkers::const_iterator c = _cue_markers.begin(); c != _cue_markers.end(); ++c) {
		XMLNode* cue_child = new XMLNode (X_("Cue"));
		cue_child->set_property ("text", c->text());
		cue_child->set_property ("position", c->position());
		cue_parent->add_child_nocopy (*cue_child);
	}

	return *cue_parent;
}

int
Source::set_cue_state (XMLNode const & cues, int /* version */)
{
	_cue_markers.clear ();

	const XMLNodeList cuelist = cues.children();

	for (XMLNodeConstIterator citer = cuelist.begin(); citer != cuelist.end(); ++citer) {
		string text;
		samplepos_t position;

		if (!(*citer)->get_property (X_("text"), text) || !(*citer)->get_property (X_("position"), position)) {
			continue;
		}

		_cue_markers.insert (CueMarker (text, position));
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

		samplepos_t sample = (samplepos_t) floor (val * _session.sample_rate());
		transients.push_back (sample);
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
	// This operation is not allowed for sources for out-of-session files.

	/* XXX need a way to detect _within_session() condition here - move it from FileSource?
	 */

	_flags = Flag (_flags | Removable | RemoveAtDestroy);
}

void
Source::set_natural_position (timepos_t const & pos)
{
	_natural_position = pos;
	_have_natural_position = true;
	_length.set_position (pos);
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

	try {
		boost::shared_ptr<Source> sptr = shared_from_this();
	} catch (...) {
		/* no shared_ptr available, relax; */
	}
}

bool
Source::writable () const
{
        return (_flags & Writable) && _session.writable();
}

bool
Source::add_cue_marker (CueMarker const & cm)
{
	if (_cue_markers.insert (cm).second) {
		CueMarkersChanged(); /* EMIT SIGNAL */
		return true;
	}

	return false;
}

bool
Source::move_cue_marker (CueMarker const & cm, timepos_t const & source_relative_position)
{
	if (source_relative_position > length ()) {
		return false;
	}

	if (remove_cue_marker (cm)) {
		return add_cue_marker (CueMarker (cm.text(), source_relative_position));
	}

	return false;
}

bool
Source::rename_cue_marker (CueMarker& cm, std::string const & str)
{
	CueMarkers::iterator m = _cue_markers.find (cm);

	if (m != _cue_markers.end()) {
		_cue_markers.erase (m);
		return add_cue_marker (CueMarker (str, cm.position()));
	}

	return false;
}

bool
Source::remove_cue_marker (CueMarker const & cm)
{
	if (_cue_markers.erase (cm)) {
		CueMarkersChanged(); /* EMIT SIGNAL */
		return true;
	}

	return false;
}

bool
Source::clear_cue_markers ()
{
	if (_cue_markers.empty()) {
		return false;
	}

	_cue_markers.clear();
	CueMarkersChanged(); /* EMIT SIGNAL */
	return true;
}

Source::empty () const
{
	return _length == timecnt_t();
}

timecnt_t
Source::length() const
{
	return _length;
}

