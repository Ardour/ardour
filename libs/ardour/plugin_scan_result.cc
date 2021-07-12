/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#include "pbd/convert.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"

#include "ardour/plugin_scan_result.h"
#include "ardour/types_convert.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;

inline PluginScanLogEntry::PluginScanResult operator|= (PluginScanLogEntry::PluginScanResult& a, const PluginScanLogEntry::PluginScanResult& b) {
  return a = static_cast<PluginScanLogEntry::PluginScanResult> (static_cast <int>(a) | static_cast<int> (b));
}

inline PluginScanLogEntry::PluginScanResult operator&= (PluginScanLogEntry::PluginScanResult& a, const PluginScanLogEntry::PluginScanResult& b) {
  return a = static_cast<PluginScanLogEntry::PluginScanResult> (static_cast <int>(a) & static_cast<int> (b));
}

PluginScanLogEntry::PluginScanLogEntry (PluginType const t, std::string const& p)
	: _type (t)
	, _path (p)
{
	reset ();
}

PluginScanLogEntry::PluginScanLogEntry (PluginScanLogEntry const& o)
	: _type (o._type)
	, _path (o._path)
	, _result (o._result)
	, _scan_log (o._scan_log)
	, _info (o._info)
	, _recent (o._recent)
{
}

PluginScanLogEntry::PluginScanLogEntry (XMLNode const& node)
{
	reset ();

	bool err = false;
	if (node.name () != "PluginScanLogEntry") {
		throw failed_constructor ();
	}
	int res = Error;
	_recent  = false;

	err |= !node.get_property ("type", _type);
	err |= !node.get_property ("path", _path);
	err |= !node.get_property ("scan-log", _scan_log);
	err |= !node.get_property ("scan-result", res);

	_result = PluginScanResult (res);

	if (err) {
		throw failed_constructor ();
	}
}

XMLNode&
PluginScanLogEntry::state () const
{
	XMLNode* node = new XMLNode ("PluginScanLogEntry");
	node->set_property ("type", _type);
	node->set_property ("path", _path);
	node->set_property ("scan-log", _scan_log);
	node->set_property ("scan-result", (int)_result);
	return *node;
}

void
PluginScanLogEntry::reset ()
{
	_result = OK;
	_scan_log = "";
	_info.clear ();
	_recent = true;
}

void
PluginScanLogEntry::set_result (PluginScanResult r)
{
	_result = r;
	_recent = true;
}

void
PluginScanLogEntry::add (PluginInfoPtr info)
{
	_recent = true;
	_info.push_back (info);
}

static bool invalid_char (unsigned char c)
{
    return !isprint (c) && c != '\n';
}

void
PluginScanLogEntry::msg (PluginScanResult r, std::string msg)
{
	_result |= r;
	_recent = true;

	/* some plugins include control chars (e.g. change terminal color) or
	 * just print garbage. libXML saves this just fine but cannot read it:
	 * "parser error: PCDATA invalid Char value"
	 */
	msg.erase (std::remove_if (msg.begin(), msg.end(), invalid_char), msg.end());

	if (msg.empty ()) {
		return;
	}

	switch (r) {
		case Error:
			PBD::warning << string_compose ("%1<%2>: %3", enum_2_string (_type), _path, msg) << endmsg;
			break;
		default:
			break;
	}

	_scan_log += msg;
	if (msg.at (msg.size() -1) != '\n') {
		_scan_log += "\n";
	}
}
