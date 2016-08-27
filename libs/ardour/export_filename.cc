/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#include <string>

#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>

#include "pbd/xml++.h"
#include "pbd/convert.h"
#include "pbd/enumwriter.h"
#include "pbd/localtime_r.h"

#include "ardour/libardour_visibility.h"
#include "ardour/session.h"
#include "ardour/session_directory.h"
#include "ardour/export_filename.h"
#include "ardour/export_format_specification.h"
#include "ardour/export_channel_configuration.h"
#include "ardour/export_timespan.h"
#include "ardour/utils.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace Glib;
using std::string;

namespace ARDOUR
{

ExportFilename::ExportFilename (Session & session) :
  include_label (false),
  include_session (false),
  use_session_snapshot_name (false),
  include_revision (false),
  include_channel_config (false),
  include_format_name (false),
  include_channel (false),
  include_timespan (true), // Include timespan name always
  include_time (false),
  include_date (false),
  session (session),
  revision (1),
  date_format (D_None),
  time_format (T_None)
{
	time_t rawtime;
	std::time (&rawtime);
	localtime_r (&rawtime, &time_struct);

	folder = session.session_directory().export_path();

	XMLNode * extra_node = session.extra_xml ("ExportFilename");
	/* Legacy sessions used Session instant.xml for this */
	if (!extra_node) {
		session.instant_xml ("ExportFilename");
	}

	if (extra_node) {
		set_state (*extra_node);
	}
}

XMLNode &
ExportFilename::get_state ()
{
	XMLNode * node = new XMLNode ("ExportFilename");
	XMLNode * child;

	FieldPair dir = analyse_folder();
	child = node->add_child ("Folder");
	child->set_property ("relative", dir.first);
	child->set_property ("path", dir.second);

	add_field (node, "label", include_label, label);
	add_field (node, "session", include_session);
	add_field (node, "snapshot", use_session_snapshot_name);
	add_field (node, "timespan", include_timespan);
	add_field (node, "revision", include_revision);
	add_field (node, "time", include_time, enum_2_string (time_format));
	add_field (node, "date", include_date, enum_2_string (date_format));

	XMLNode * extra_node = new XMLNode ("ExportRevision");
	extra_node->set_property ("revision", revision);
	session.add_extra_xml (*extra_node);

	return *node;
}

int
ExportFilename::set_state (const XMLNode & node)
{
	XMLNode * child;
	FieldPair pair;

	child = node.child ("Folder");
	if (!child) { return -1; }

	folder = "";

	bool is_relative;
	if (child->get_property ("relative", is_relative) && is_relative) {
		folder = session.session_directory ().root_path ();
	}

	std::string tmp;
	if (child->get_property ("path", tmp)) {
		tmp = Glib::build_filename (folder, tmp);
		if (!Glib::file_test (tmp, Glib::FILE_TEST_EXISTS)) {
			warning << string_compose (_("Existing export folder for this session (%1) does not exist - ignored"), tmp) << endmsg;
		} else {
			folder = tmp;
		}
	}

	if (folder.empty()) {
		folder = session.session_directory().export_path();
	}

	pair = get_field (node, "label");
	include_label = pair.first;
	label = pair.second;

	pair = get_field (node, "session");
	include_session = pair.first;

	pair = get_field (node, "snapshot");
	use_session_snapshot_name = pair.first;

	pair = get_field (node, "timespan");
	include_timespan = pair.first;

	pair = get_field (node, "revision");
	include_revision = pair.first;

	pair = get_field (node, "time");
	include_time = pair.first;
	time_format = (TimeFormat) string_2_enum (pair.second, time_format);

	pair = get_field (node, "date");
	include_date = pair.first;
	date_format = (DateFormat) string_2_enum (pair.second, date_format);

	XMLNode * extra_node = session.extra_xml ("ExportRevision");
	/* Legacy sessions used Session instant.xml for this */
	if (!extra_node) {
		extra_node = session.instant_xml ("ExportRevision");
	}

	if (extra_node) {
		extra_node->get_property ("revision", revision);
	}

	return 0;
}

string
ExportFilename::get_path (ExportFormatSpecPtr format) const
{
	string path;
	bool filename_empty = true;
	bool with_timespan = include_timespan;

	if (!include_session
			&& !include_label
			&& !include_revision
			&& !include_timespan
			&& !include_channel_config
			&& !include_channel
			&& !include_date
			&& !include_format_name) {
		with_timespan = true;
	}

	if (include_session) {
		path += filename_empty ? "" : "_";
		if (use_session_snapshot_name) {
			path += session.snap_name();
		} else {
			path += session.name();
		}
		filename_empty = false;
	}

	if (include_label) {
		path += filename_empty ? "" : "_";
		path += label;
		filename_empty = false;
	}

	if (include_revision) {
		path += filename_empty ? "" : "_";
		path += "r";
		path += to_string (revision, std::dec);
		filename_empty = false;
	}

	if (with_timespan && timespan) {
		path += filename_empty ? "" : "_";
		path += timespan->name();
		filename_empty = false;
	}

	if (include_channel_config && channel_config) {
		path += filename_empty ? "" : "_";
		path += channel_config->name();
		filename_empty = false;
	}

	if (include_channel) {
		path += filename_empty ? "" : "_";
		path += "channel";
		path += to_string (channel, std::dec);
		filename_empty = false;
	}

	if (include_date) {
		path += filename_empty ? "" : "_";
		path += get_date_format_str (date_format);
		filename_empty = false;
	}

	if (include_time) {
		path += filename_empty ? "" : "_";
		path += get_time_format_str (time_format);
		filename_empty = false;
	}

	if (include_format_name) {
		path += filename_empty ? "" : "_";
		path += format->name();
		filename_empty = false;
	}

	if (path.empty ()) {
		path = "export";
	}

	path += ".";
	path += format->extension ();

	path = legalize_for_universal_path (path);

	return Glib::build_filename (folder, path);
}

string
ExportFilename::get_time_format_str (TimeFormat format) const
{
	switch ( format ) {
	  case T_None:
		return _("No Time");

	  case T_NoDelim:
		return get_formatted_time ("%H%M");

	  case T_Delim:
		return get_formatted_time ("%H.%M");

	  default:
		return _("Invalid time format");
	}
}

string
ExportFilename::get_date_format_str (DateFormat format) const
{
	switch (format) {
	  case D_None:
		return _("No Date");

	  case D_BE:
		return get_formatted_time ("%Y%m%d");

	  case D_ISO:
		return get_formatted_time ("%Y-%m-%d");

	  case D_BEShortY:
		return get_formatted_time ("%y%m%d");

	  case D_ISOShortY:
		return get_formatted_time ("%y-%m-%d");

	  default:
		return _("Invalid date format");
	}
}

void
ExportFilename::set_time_format (TimeFormat format)
{
	time_format = format;

	if (format == T_None) {
		include_time = false;
	} else {
		include_time = true;
	}
}

void
ExportFilename::set_date_format (DateFormat format)
{
	date_format = format;

	if (format == D_None) {
		include_date = false;
	} else {
		include_date = true;
	}
}

void
ExportFilename::set_label (string value)
{
	label = value;
	include_label = !value.compare ("");
}

bool
ExportFilename::set_folder (string path)
{
	// TODO check folder existence
	folder = path;
	return true;
}

string
ExportFilename::get_formatted_time (string const & format) const
{
	char buffer [80];
	strftime (buffer, 80, format.c_str(), &time_struct);

	string return_value (buffer);
	return return_value;
}

void
ExportFilename::add_field (XMLNode * node, string const & name, bool enabled, string const & value)
{
	XMLNode * child = node->add_child ("Field");

	if (!child) {
		std::cerr << "Error adding a field to ExportFilename XML-tree" << std::endl;
		return;
	}

	child->set_property ("name", name);
	child->set_property ("enabled", enabled);
	if (!value.empty()) {
		child->set_property ("value", value);
	}
}

ExportFilename::FieldPair
ExportFilename::get_field (XMLNode const & node, string const & name)
{
	FieldPair pair;
	pair.first = false;

	XMLNodeList children = node.children();

	for (XMLNodeList::iterator it = children.begin(); it != children.end(); ++it) {
		std::string str;
		if ((*it)->get_property ("name", str) && name == str) {

			(*it)->get_property ("enabled", pair.first);
			(*it)->get_property ("value", pair.second);

			return pair;
		}
	}

	return pair;
}

ExportFilename::FieldPair
ExportFilename::analyse_folder ()
{
	FieldPair pair;

	string session_dir = session.session_directory().root_path();
	string::size_type session_dir_len = session_dir.length();

	string folder_beginning = folder.substr (0, session_dir_len);

	if (!folder_beginning.compare (session_dir)) {
		pair.first = true;
		pair.second = folder.substr (session_dir_len);
	} else {
		pair.first = false;
		pair.second = folder;
	}

	return pair;
}

} // namespace ARDOUR
