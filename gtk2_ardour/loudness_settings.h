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

#ifndef _gtkardour_loudness_settings_h_
#define _gtkardour_loudness_settings_h_

class XMLNode;

struct CLoudnessPreset
{
	enum {
		LR_dBFS,
		LR_dBTP,
		LR_Integrated,
		LR_Short,
		LR_Momentary,
		LR_LAST
	};

	std::string label;
	bool        enable[LR_LAST];
	float       level[LR_LAST];
	float       LUFS_range[2];
	bool        report;
	bool        user;
};

struct ALoudnessPreset : public CLoudnessPreset
{
	ALoudnessPreset (XMLNode const&);
	ALoudnessPreset (std::string const&, bool enable[LR_LAST], float level[LR_LAST]);
	ALoudnessPreset (CLoudnessPreset const& c) : CLoudnessPreset (c) {}

	XMLNode& state () const;
	bool operator==(ALoudnessPreset const&) const;
};

class ALoudnessPresets
{
public:
	ALoudnessPresets (bool built_in_only);
	~ALoudnessPresets ();

	std::vector<ALoudnessPreset> const& presets () const {
		return _p;
	}

	bool push_back (CLoudnessPreset const&);
	bool erase (CLoudnessPreset const&);
	bool erase (size_t);

	CLoudnessPreset const&
	operator[] (size_t which) const {
		if (which >= _p.size ()) {
			throw std::out_of_range ("preset index is out of range");
		}
		return _p[which];
	}

	CLoudnessPreset&
	operator[] (size_t which) {
		if (which >= _p.size ()) {
			throw std::out_of_range ("preset index is out of range");
		}
		return _p[which];
	}

	size_t n_presets () const {
		return _p.size ();
	}

	bool find_preset (CLoudnessPreset&) const;

private:
	std::vector <ALoudnessPreset> _p;
	bool _save;
};

#endif
