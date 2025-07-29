/*
 * Copyright (C) 2025 Paul Davis <paul@linuxaudiosystems.com>
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

#include "pbd/types_convert.h"
#include "pbd/error.h"
#include "pbd/xml++.h"

#include "ardour/types_convert.h"

#include "editing_convert.h"
#include "region_ui_settings.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace Editing;

RegionUISettings::RegionUISettings ()
	: grid_type (GridTypeBeat)
	, samples_per_pixel (2048)
	, follow_playhead (true)
	, play_selection (true)
	, snap_mode (Editing::SnapMagnetic)
	, zoom_focus (ZoomFocusLeft)
	, mouse_mode (MouseContent)
	, x_origin (0)
	, recording_length (1, 0, 0)
	, draw_length (Editing::GridTypeBeat)
	, draw_velocity (64)
	, channel (0)
	, note_min (32)
	, note_max (96)
{
}

XMLNode&
RegionUISettings::get_state () const
{
	XMLNode* node = new XMLNode (X_("RegionUISetting"));
	node->set_property (X_("grid-type"), grid_type);
	node->set_property (X_("samples-per-pixel"), samples_per_pixel);
	node->set_property (X_("follow-playhead"), follow_playhead);
	node->set_property (X_("play-selection"), play_selection);
	node->set_property (X_("snap-mode"), snap_mode);
	node->set_property (X_("zoom-focus"), zoom_focus);
	node->set_property (X_("mouse-mode"), mouse_mode);
	node->set_property (X_("x-origin"), x_origin);
	node->set_property (X_("recording_length"), recording_length);

	node->set_property (X_("draw-length"), draw_length);
	node->set_property (X_("draw-velocity"), draw_velocity);
	node->set_property (X_("channel"), channel);
	node->set_property (X_("note-min"), note_min);
	node->set_property (X_("note-max"), note_max);

	return *node;
}

int
RegionUISettings::set_state (XMLNode const & state, int)
{
	if (state.name() != X_("RegionUISetting")) {
		return -1;
	}
	state.get_property (X_("grid-type"), grid_type);
	state.get_property (X_("samples-per-pixel"), samples_per_pixel);
	state.get_property (X_("follow-playhead"), follow_playhead);
	state.get_property (X_("play-selection"), play_selection);
	state.get_property (X_("snap-mode"), snap_mode);
	state.get_property (X_("zoom-focus"), zoom_focus);
	state.get_property (X_("mouse-mode"), mouse_mode);
	state.get_property (X_("x-origin"), x_origin);
	state.get_property (X_("recording_length"), recording_length);

	state.get_property (X_("draw-length"), draw_length);
	state.get_property (X_("draw-velocity"), draw_velocity);
	state.get_property (X_("channel"), channel);
	state.get_property (X_("note-min"), note_min);
	state.get_property (X_("note-max"), note_max);

	return 0;
}

XMLNode&
RegionUISettingsManager::get_state () const
{
	XMLNode* node = new XMLNode (X_("RegionUISettings"));
	for (auto & [id,settings] : *this) {
		XMLNode& n (settings.get_state());
		n.set_property (X_("id"), id);
		node->add_child_nocopy (n);
	}
	return *node;
}

int
RegionUISettingsManager::set_state (XMLNode const & state, int version)
{
	if (state.name() != X_("RegionUISettings")) {
		return -1;
	}

	RegionUISettings rus;
	PBD::ID id;

	clear ();

	for (auto & child : state.children()) {
		if (rus.set_state (*child, version)) {
			return -1;
		}
		child->get_property (X_("id"), id);
		insert (std::make_pair (id, rus));
	}

	return 0;
}

void
RegionUISettingsManager::save (std::string const & path)
{
	XMLTree state_tree;

	state_tree.set_root (&get_state());
	state_tree.set_filename (path);

	if (state_tree.write()) {
		error << string_compose (_("could not save region GUI settings to %1"), path) << endmsg;
	}
}

int
RegionUISettingsManager::load (std::string const & xmlpath)
{
	XMLTree state_tree;

	clear ();

	if (!state_tree.read (xmlpath)) {
		std::cerr << "bad xmlpath " << xmlpath << std::endl;
		return -1;
	}
	std::cerr << "loading " << xmlpath << std::endl;

	XMLNode const & root (*state_tree.root());

	if (root.name() != X_("RegionUISettings")) {
		std::cerr << "bad root\n";
		return -1;
	}

	for (auto const & node : root.children()) {
		RegionUISettings rsu;
		PBD::ID id;
		node->get_property ("id", id);

		std::cerr << "loaded RSU for " << id << std::endl;

		if (rsu.set_state (*node, 0) == 0) {
			insert (std::make_pair (id, rsu));
		}
	}

	return 0;
}
