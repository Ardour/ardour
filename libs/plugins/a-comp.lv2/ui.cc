/* a-comp UI -- test/example
 *
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define ACOMP_URI "urn:ardour:a-comp"

#include <stdlib.h>

#include <gtkmm.h>

#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

using namespace Gtk;

typedef struct {
	LV2UI_Write_Function write;
	LV2UI_Controller     controller;

	Box* box;
	Label*  label;
} ACompUI;


/******************************************************************************
 * GUI
 */

static void* setup_ui (ACompUI* ui) {
	ui->box = manage (new HBox);

	ui->label = manage (new Label ("Hello World"));
	ui->box->pack_start (*ui->label, false, false, 4);

	return ui->box->gobj ();
}


/******************************************************************************
 * LV2 callbacks
 */

static LV2UI_Handle
instantiate (const LV2UI_Descriptor*   descriptor,
             const char*               plugin_uri,
             const char*               bundle_path,
             LV2UI_Write_Function      write_function,
             LV2UI_Controller          controller,
             LV2UI_Widget*             widget,
             const LV2_Feature* const* features)
{
	ACompUI* ui = (ACompUI*)calloc (1, sizeof (ACompUI));
	ui->write      = write_function;
	ui->controller = controller;
	ui->box        = NULL;

	*widget = setup_ui (ui);
	return ui;
}

static void
cleanup (LV2UI_Handle handle)
{
	ACompUI* ui = (ACompUI*)handle;
	free (ui);
}

static void
port_event (LV2UI_Handle handle,
            uint32_t     port_index,
            uint32_t     buffer_size,
            uint32_t     format,
            const void*  buffer)
{
	ACompUI* ui = (ACompUI*)handle;
}

/******************************************************************************
 * LV2 setup
 */

static const void*
extension_data (const char* uri)
{
	return NULL;
}

static const LV2UI_Descriptor descriptor = {
	ACOMP_URI "#ui",
	instantiate,
	cleanup,
	port_event,
	extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor*
lv2ui_descriptor (uint32_t index)
{
	switch (index) {
	case 0:
		return &descriptor;
	default:
		return NULL;
	}
}
