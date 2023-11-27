/*
 * Copyright (C) 2023 Robin Gareus <robin@gareus.org>
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

#ifdef NDEBUG
#error THIS PLUGIN EXISTS ONLY FOR THE PROTOTYPING
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // needed for M_PI
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

/******************************************************************************
 * LV2 wrapper
 */

#ifdef HAVE_LV2_1_18_6
#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/urid/urid.h>
#else
#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/atom/forge.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>
#endif

typedef struct {
	LV2_URID atom_Blank;
	LV2_URID atom_Object;
	LV2_URID atom_Vector;
	LV2_URID atom_Float;
	LV2_URID atom_Int;
	LV2_URID atom_eventTransfer;
} VaporLV2URIs;

typedef struct {
	/* audio I/O */
	float const* input[128];
  float*       out_surround[12];
  float*       out_binaural[2];
  float*       out_loudness[6];

  /* control Ports */
  LV2_Atom_Sequence const* p_control;
  LV2_Atom_Sequence*       p_notify;
	float const*             p_enable;
	float*                   p_latency;

  /* atom-forge and URI mapping */
  LV2_URID_Map*        map;
  LV2_Atom_Forge       forge;
  LV2_Atom_Forge_Frame frame;
	VaporLV2URIs         uris;

	/* internal config/state */
	uint32_t latency;
} AVapor;

static inline void
map_uris (LV2_URID_Map* map, VaporLV2URIs* uris)
{
  uris->atom_Blank         = map->map (map->handle, LV2_ATOM__Blank);
  uris->atom_Object        = map->map (map->handle, LV2_ATOM__Object);
  uris->atom_Vector        = map->map (map->handle, LV2_ATOM__Vector);
  uris->atom_Float         = map->map (map->handle, LV2_ATOM__Float);
  uris->atom_Int           = map->map (map->handle, LV2_ATOM__Int);
  uris->atom_eventTransfer = map->map (map->handle, LV2_ATOM__eventTransfer);
}

static LV2_Handle
instantiate (const LV2_Descriptor*     descriptor,
             double                    rate,
             const char*               bundle_path,
             const LV2_Feature* const* features)
{
#if 0 /* D018y requires 48k or 96k */
	if (rate != 48000 && rate != 96000) {
		return NULL;
	}
#endif

	AVapor* self = (AVapor*)calloc (1, sizeof (AVapor));
	if (!self) {
		return NULL;
	}

	for (int i = 0; features[i]; ++i) {
		if (!strcmp (features[i]->URI, LV2_URID__map)) {
			self->map = (LV2_URID_Map*)features[i]->data;
		}
	}

	if (!self->map) {
		fprintf (stderr, "a-vapor.lv2 error: Host does not support urid:map\n");
		free (self);
		return NULL;
	}

	lv2_atom_forge_init (&self->forge, self->map);
	map_uris (self->map, &self->uris);

	self->latency = 0;

	return (LV2_Handle)self;
}

static void
connect_port (LV2_Handle instance,
              uint32_t   port,
              void*      data)
{
	AVapor* self = (AVapor*)instance;
  switch (port) {
    case 0:
      self->p_control = (LV2_Atom_Sequence const*)data;
			return;
    case 1:
      self->p_notify = (LV2_Atom_Sequence*)data;
			return;
    case 2:
      self->p_enable = (float const*) data;
			return;
    case 3:
      self->p_latency = (float*) data;
			return;
		default:
			break;
	}

	if (port < 132) {
		self->input[port - 4] = (float const*) data;
	} else if (port < 144) {
		self->out_surround[port - 132] = (float*) data;
	} else if (port < 146) {
		self->out_binaural[port - 144] = (float*) data;
	} else if (port < 152) {
		self->out_loudness[port - 146] = (float*) data;
	}

}

static void
activate (LV2_Handle instance)
{
	//AVapor* self = (AVapor*)instance;
}

static void
deactivate (LV2_Handle instance)
{
	activate (instance);
}

static void
run (LV2_Handle instance, uint32_t n_samples)
{
	AVapor* self = (AVapor*)instance;

	/* announce latency */
	*self->p_latency = self->latency;

	if (!self->p_control || !self->p_notify || n_samples == 0) {
		/* latency measurement callback */
		return;
	}

	/* prepare forge buffer and initialize atom-sequence */
	const uint32_t capacity = self->p_notify->atom.size;
	lv2_atom_forge_set_buffer (&self->forge, (uint8_t*)self->p_notify, capacity);
	lv2_atom_forge_sequence_head (&self->forge, &self->frame, 0);

	  /* process messages from Host */
  if (self->p_control) {
		LV2_Atom_Event* ev = lv2_atom_sequence_begin (&(self->p_control)->body);
		while (!lv2_atom_sequence_is_end (&(self->p_control)->body, (self->p_control)->atom.size, ev)) {
			if (ev->body.type == self->uris.atom_Blank || ev->body.type == self->uris.atom_Object) {
				// TODO
				// const LV2_Atom_Object* obj = (LV2_Atom_Object*)&ev->body;
			}
			ev = lv2_atom_sequence_next (ev);
		}
	}

	/* process audio */
	for (uint32_t chn = 0; chn < 12; ++chn) {
		memset (self->out_surround[chn], 0, sizeof (float) * n_samples);
	}

	for (uint32_t chn = 0; chn < 2; ++chn) {
		memset (self->out_binaural[chn], 0, sizeof (float) * n_samples);
	}

	for (uint32_t chn = 0; chn < 6; ++chn) {
		memset (self->out_loudness[chn], 0, sizeof (float) * n_samples);
	}

	/* close off atom-sequence */
	lv2_atom_forge_pop (&self->forge, &self->frame);
}

static void
cleanup (LV2_Handle instance)
{
	//AVapor* self = (AVapor*)instance;
	free (instance);
}

static const void*
extension_data (const char* uri)
{
	return NULL;
}

static const LV2_Descriptor descriptor = {
	"urn:ardour:a-vapor",
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor (uint32_t index)
{
	switch (index) {
		case 0:
			return &descriptor;
		default:
			return NULL;
	}
}
