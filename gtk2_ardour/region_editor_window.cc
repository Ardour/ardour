/*
 * Copyright (C) 2024 Robin Gareus <robin@gareus.org>
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

#include "region_editor_window.h"
#include "audio_region_editor.h"
#include "audio_region_view.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

RegionEditorWindow::RegionEditorWindow (Session* s, RegionView* rv)
	: ArdourWindow (_("Region"))
{
	AudioRegionView* arv = dynamic_cast<AudioRegionView*> (rv);
	if (arv) {
		_region_editor = new AudioRegionEditor (s, arv);
	} else {
		_region_editor = new RegionEditor (s, rv->region());
	}
	add (*_region_editor);
	set_name ("RegionEditorWindow");
}

RegionEditorWindow::~RegionEditorWindow ()
{
	delete _region_editor;
}

void
RegionEditorWindow::set_session (Session* s)
{
	ArdourWindow::set_session (s);
	if (s) {
		_region_editor->set_session (s);
	}
}

void
RegionEditorWindow::on_unmap ()
{
	_region_editor->unmap ();
	ArdourWindow::on_unmap ();
}
