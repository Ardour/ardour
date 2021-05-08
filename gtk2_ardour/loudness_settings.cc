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

#include <glibmm.h>
#include "pbd/gstdio_compat.h"

#include <glibmm/miscutils.h>

#include "pbd/compose.h"
#include "pbd/failed_constructor.h"
#include "pbd/xml++.h"

#include "ardour/filesystem_paths.h"

#include "loudness_settings.h"

/* https://www.masteringthemix.com/blogs/learn/76296773-mastering-audio-for-soundcloud-itunes-spotify-and-youtube
 * https://youlean.co/loudness-standards-full-comparison-table/
 */
static CLoudnessPreset factory_presets[] = {
	/* clang-format off */
	/*                   | dbFS   dBTP   LUFS   short   mom.    | FS,  TP , int, sht, mom |maxIntg|  notes */
	{"EBU R128",         { false, true,  true,  false, false},  {  0, -1.0, -23,   0,   0 }, {-22.5,  -23.5}, true,  false}, // +/- 0.5 LU
	{"EBU R128 S1",      { false, true,  true,  true,  false},  {  0, -1.0, -23, -18,   0 }, {-22.5,  -23.5}, false, false}, // +/- 0.5 LU
	{"ATSC A/85",        { false, true,  true,  true,  false},  {  0, -2.0, -24,   0,   0 }, {-22.0,  -26.0}, false, false}, // +/- 2 LU
	{"AES Streaming",    { false, true,  true,  false, false},  {  0, -1.0, -18,   0,   0 }, {-16.0,  -20.0}, true,  false}, // min/max Integrated: -20 / -16 LUFS - same as "ASWG-R001 PORTABLE"
	{"ASWG-R001 HOME",   { false, true,  true,  true,  false},  {  0, -1.0, -24,   0,   0 }, {-22.0,  -26.0}, false, false}, // +/- 2 LU
	{"Digital Peak",     { true,  false, false, false, false},  {  0,  0.0,   0,   0,   0 }, {  0.0, -200.0}, false, false},
	{"CD/DVD",           { true,  true,  true,  false, false},  {  0, -0.1,  -9,   0,   0 }, {  0.0, -200.0}, true,  false},

	{"Amazon Music",     { false, true,  true,  false, false},  {  0, -2.0, -14,   0,   0 }, { -9.0,  -19.0}, true,  false}, // -9 to -19 LUFS
	{"Apple Music",      { false, true,  true,  false, false},  {  0, -1.0, -16,   0,   0 }, {-15.0,  -17.0}, true,  false}, // (+/- 1.0 LU)
	{"Deezer",           { false, true,  true,  false, false},  {  0, -1.0, -15,   0,   0 }, {-14.0,  -16.0}, true,  false}, // -14 to -16 LUFS
	{"Soundcloud",       { false, true,  true,  false, false},  {  0, -1.0, -10,   0,   0 }, { -8.0,  -13.0}, true,  false}, // -8 to -13 LUFS
	{"Spotify",          { false, true,  true,  false, false},  {  0, -1.0, -14,   0,   0 }, { -8.0,  -20.0}, true,  false}, // Spotify use replay-gain to match -14 or -11 ..
	{"Spotify Loud",     { false, true,  true,  false, false},  {  0, -2.0, -11,   0,   0 }, { -5.0,  -17.0}, true,  false}, // .. so the min/max range is arbitrary +/- 6dB
	{"Youtube",          { false, true,  true,  false, false},  {  0, -1.0, -14,   0,   0 }, {-13.0,  -15.0}, true,  false}, // -13 to -15 LUFS
	/* clang-format on */
};

ALoudnessPreset::ALoudnessPreset (std::string const& n, bool en[LR_LAST], float lvl[LR_LAST]) 
{
	label = n;
	report = false;
	user = true;

	for (size_t i = 0; i < LR_LAST; ++i) {
		enable[i] = en[i];
		level[i] = level[i];
	}
	LUFS_range[0] = LUFS_range[1] = -200;
}

ALoudnessPreset::ALoudnessPreset (XMLNode const& node)
{
	user = true;
	report = false;
	LUFS_range[0] = LUFS_range [1] = -200;

	if (node.name () != "LoudnessPreset") {
		throw failed_constructor ();
	}
	if (!node.get_property ("label", label)) {
		throw failed_constructor ();
	}

	for (size_t i = 0; i < LR_LAST; ++i) {
		if (node.get_property (string_compose ("level-%1", i).c_str(), level[i])) {
			enable[i] = true;
		} else {
			enable[i] = false;
			level[i]  = 0;
		}
	}
}

XMLNode&
ALoudnessPreset::state () const
{
	assert (user);

	XMLNode* node = new XMLNode("LoudnessPreset");
	node->set_property ("label", label);
	for (size_t i = 0; i < LR_LAST; ++i) {
		if (enable[i]) {
			node->set_property (string_compose ("level-%1", i).c_str(), level[i]);
		}
	}
	return *node;
}

bool
ALoudnessPreset::operator==(ALoudnessPreset const& o) const
{
	for (size_t i = 0; i < LR_LAST; ++i) {
		if (enable[i] != o.enable[i]) {
			return false;
		}
		if (enable[i] && level[i] != o.level[i]) {
			return false;
		}
	}
	return true;
}

/* ****************************************************************************/

ALoudnessPresets::ALoudnessPresets (bool built_in_only)
	: _save (!built_in_only)
{
	for (size_t i = 0; i < sizeof (factory_presets) / sizeof (CLoudnessPreset); ++i) {
		_p.push_back (factory_presets[i]);
	}
	if (built_in_only) {
		return;
	}
	std::string fn = Glib::build_filename (ARDOUR::user_config_directory (), "loudness-presets");
	XMLTree tree;
	if (Glib::file_test (fn, Glib::FILE_TEST_EXISTS) && tree.read (fn)) {
		XMLNodeList nlist = tree.root()->children();
		XMLNodeConstIterator i;
		for (i = nlist.begin(); i != nlist.end(); ++i) {
			try {
			_p.push_back (ALoudnessPreset (**i));
			} catch (...) {
			}
		}
	}
}

ALoudnessPresets::~ALoudnessPresets ()
{
	if (!_save) {
		return;
	}
	bool have_presets = false;
	XMLNode* root (new XMLNode ("LoudnessPresets"));
	for (std::vector <ALoudnessPreset>::const_iterator i = _p.begin(); i != _p.end (); ++i) {
		if (!i->user) {
			continue;
		}
		root->add_child_nocopy (i->state ());
		have_presets = true;
	}
	std::string fn = Glib::build_filename (ARDOUR::user_config_directory (), "loudness-presets");
	if (have_presets) {
		XMLTree tree (fn);
		tree.set_root (root);
		tree.write();
	} else {
		delete root;
		::g_unlink (fn.c_str ());
	}
}

bool
ALoudnessPresets::find_preset (CLoudnessPreset& clp) const
{
	std::vector <ALoudnessPreset>::const_iterator i;
	i = std::find (_p.begin (), _p.end(), clp);
	if (i == _p.end ()) {
		return false;
	}
	clp = *i;
	return true;
}

bool
ALoudnessPresets::push_back (CLoudnessPreset const& clp)
{
	if (clp.report) {
		return false;
	}
	if (std::find (_p.begin (), _p.end(), clp) != _p.end ()) {
		return false;
	}
	std::vector <ALoudnessPreset>::iterator i;
	for (i = _p.begin (); i != _p.end (); ++i) {
		if (i->label == clp.label) {
			if (i->user) {
				*i = clp;
				return true;
			}
			return false;
		}
	}
	_p.push_back (clp);
	return true;
}

bool
ALoudnessPresets::erase (CLoudnessPreset const& clp)
{
	std::vector <ALoudnessPreset>::iterator i;
	i = std::find (_p.begin (), _p.end(), clp);
	if (i == _p.end ()) {
		return false;
	}
	_p.erase (i);
	return true;
}

bool
ALoudnessPresets::erase (size_t which)
{
	if (which >= _p.size ()) {
		return false;
	}
	if (!_p[which].user) {
		return false;
	}
	_p.erase (_p.begin () + which);
	return true;
}
