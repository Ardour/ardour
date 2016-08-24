/* a-fluidsynth -- simple & robust x-platform fluidsynth LV2
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define AFS_URN "urn:ardour:a-fluidsynth"

#ifdef HAVE_LV2_1_10_0
#define x_forge_object lv2_atom_forge_object
#else
#define x_forge_object lv2_atom_forge_blank
#endif

#include "fluidsynth.h"

#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/atom/forge.h>
#include <lv2/lv2plug.in/ns/ext/atom/util.h>
#include <lv2/lv2plug.in/ns/ext/log/logger.h>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include <lv2/lv2plug.in/ns/ext/patch/patch.h>
#include <lv2/lv2plug.in/ns/ext/state/state.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>
#include <lv2/lv2plug.in/ns/ext/worker/worker.h>

enum {
	FS_PORT_CONTROL = 0,
	FS_PORT_NOTIFY,
	FS_PORT_OUT_L,
	FS_PORT_OUT_R,
	FS_OUT_GAIN,
	FS_REV_ENABLE,
	FS_REV_ROOMSIZE,
	FS_REV_DAMPING,
	FS_REV_WIDTH,
	FS_REV_LEVEL,
	FS_CHR_ENABLE,
	FS_CHR_N,
	FS_CHR_SPEED,
	FS_CHR_DEPTH,
	FS_CHR_LEVEL,
	FS_CHR_TYPE,
	FS_PORT_LAST
};

enum {
  CMD_APPLY    = 0,
  CMD_FREE     = 1,
};

typedef struct {
	/* ports */
	const LV2_Atom_Sequence* control;
  LV2_Atom_Sequence*       notify;

	float* p_ports[FS_PORT_LAST];
	float  v_ports[FS_PORT_LAST];

	/* fluid synth */
	fluid_settings_t* settings;
	fluid_synth_t*    synth;
	int               synthId;

	/* lv2 URIDs */
	LV2_URID atom_Blank;
	LV2_URID atom_Object;
	LV2_URID atom_URID;
	LV2_URID atom_Path;
	LV2_URID midi_MidiEvent;
	LV2_URID patch_Get;
	LV2_URID patch_Set;
	LV2_URID patch_property;
	LV2_URID patch_value;
	LV2_URID afs_sf2file;

	/* lv2 extensions */
	LV2_Log_Log*         log;
	LV2_Log_Logger       logger;
  LV2_Worker_Schedule* schedule;
	LV2_Atom_Forge       forge;
	LV2_Atom_Forge_Frame frame;

	/* state */
	bool panic;
	bool initialized;
	bool inform_ui;

	char current_sf2_file_path[1024];
	char queue_sf2_file_path[1024];
	bool reinit_in_progress; // set in run, cleared in work_response
	bool queue_reinit; // set in restore, cleared in work_response

	fluid_midi_event_t* fmidi_event;

} AFluidSynth;

/* *****************************************************************************
 * helpers
 */

static bool
load_sf2 (AFluidSynth* self, const char* fn)
{
	const int synth_id = fluid_synth_sfload (self->synth, fn, 1);

	if (synth_id == FLUID_FAILED) {
		return false;
	}

	fluid_sfont_t* const sfont = fluid_synth_get_sfont_by_id (self->synth, synth_id);
	if (!sfont) {
		return false;
	}

	int chn;
	fluid_preset_t preset;
	sfont->iteration_start (sfont);
	for (chn = 0; sfont->iteration_next (sfont, &preset) && chn < 15; ++chn) {
		fluid_synth_program_select (self->synth, chn, synth_id,
				preset.get_banknum (&preset), preset.get_num (&preset));
	}

	if (chn == 0) {
		return false;
	}

	return true;
}

static const LV2_Atom*
parse_patch_msg (AFluidSynth* self, const LV2_Atom_Object* obj)
{
	const LV2_Atom* property = NULL;
	const LV2_Atom* file_path = NULL;

	if (obj->body.otype != self->patch_Set) {
		return NULL;
	}

	lv2_atom_object_get (obj, self->patch_property, &property, 0);
	if (!property || property->type != self->atom_URID) {
		return NULL;
	} else if (((const LV2_Atom_URID*)property)->body != self->afs_sf2file) {
		return NULL;
	}

	lv2_atom_object_get (obj, self->patch_value, &file_path, 0);
	if (!file_path || file_path->type != self->atom_Path) {
		return NULL;
	}

	return file_path;
}

static void
inform_ui (AFluidSynth* self)
{
	if (strlen (self->current_sf2_file_path) == 0) {
		return;
	}

	LV2_Atom_Forge_Frame frame;
	lv2_atom_forge_frame_time (&self->forge, 0);
	x_forge_object (&self->forge, &frame, 1, self->patch_Set);
	lv2_atom_forge_property_head (&self->forge, self->patch_property, 0);
	lv2_atom_forge_urid (&self->forge, self->afs_sf2file);
	lv2_atom_forge_property_head (&self->forge, self->patch_value, 0);
	lv2_atom_forge_path (&self->forge, self->current_sf2_file_path, strlen (self->current_sf2_file_path));

	lv2_atom_forge_pop (&self->forge, &frame);
}

static float
db_to_coeff (float db)
{
	if (db <= -80) { return 0; }
	else if (db >=  20) { return 10; }
	return powf (10.f, .05f * db);
}

/* *****************************************************************************
 * LV2 Plugin
 */

static LV2_Handle
instantiate (const LV2_Descriptor*     descriptor,
             double                    rate,
             const char*               bundle_path,
             const LV2_Feature* const* features)
{
	AFluidSynth* self = (AFluidSynth*)calloc (1, sizeof (AFluidSynth));

	if (!self) {
		return NULL;
	}

	LV2_URID_Map* map = NULL;

	for (int i=0; features[i] != NULL; ++i) {
		if (!strcmp (features[i]->URI, LV2_URID__map)) {
			map = (LV2_URID_Map*)features[i]->data;
		} else if (!strcmp (features[i]->URI, LV2_LOG__log)) {
			self->log = (LV2_Log_Log*)features[i]->data;
		} else if (!strcmp (features[i]->URI, LV2_WORKER__schedule)) {
			self->schedule = (LV2_Worker_Schedule*)features[i]->data;
		}
	}

	lv2_log_logger_init (&self->logger, map, self->log);

	if (!map) {
		lv2_log_error (&self->logger, "a-fluidsynth.lv2: Host does not support urid:map\n");
		free (self);
		return NULL;
	}

	if (!self->schedule) {
		lv2_log_error (&self->logger, "a-fluidsynth.lv2: Host does not support worker:schedule\n");
		free (self);
		return NULL;
	}

	/* initialize fluid synth */
	self->settings = new_fluid_settings ();

	if (!self->settings) {
		lv2_log_error (&self->logger, "a-fluidsynth.lv2: cannot allocate Fluid Settings\n");
		free (self);
		return NULL;
	}

	fluid_settings_setnum (self->settings, "synth.sample-rate", rate);
	fluid_settings_setint (self->settings, "synth.parallel-render", 1);
	fluid_settings_setint (self->settings, "synth.threadsafe-api", 0);

	self->synth = new_fluid_synth (self->settings);

	if (!self->synth) {
		lv2_log_error (&self->logger, "a-fluidsynth.lv2: cannot allocate Fluid Synth\n");
    delete_fluid_settings (self->settings);
		free (self);
		return NULL;
	}

	fluid_synth_set_gain (self->synth, 1.0f);
	fluid_synth_set_polyphony (self->synth, 32);
	fluid_synth_set_sample_rate (self->synth, (float)rate);

	self->fmidi_event = new_fluid_midi_event ();

	if (!self->fmidi_event) {
		lv2_log_error (&self->logger, "a-fluidsynth.lv2: cannot allocate Fluid Event\n");
		delete_fluid_synth (self->synth);
    delete_fluid_settings (self->settings);
		free (self);
		return NULL;
	}

	/* initialize plugin state */

	self->panic = false;
	self->inform_ui = false;
	self->initialized = false;
	self->reinit_in_progress = false;
	self->queue_reinit = false;

	lv2_atom_forge_init (&self->forge, map);

	/* map URIDs */
	self->atom_Blank         = map->map (map->handle, LV2_ATOM__Blank);
	self->atom_Object        = map->map (map->handle, LV2_ATOM__Object);
	self->atom_Path          = map->map (map->handle, LV2_ATOM__Path);
	self->atom_URID          = map->map (map->handle, LV2_ATOM__URID);
	self->midi_MidiEvent     = map->map (map->handle, LV2_MIDI__MidiEvent);
	self->patch_Get          = map->map (map->handle, LV2_PATCH__Get);
	self->patch_Set          = map->map (map->handle, LV2_PATCH__Set);
	self->patch_property     = map->map (map->handle, LV2_PATCH__property);
	self->patch_value        = map->map (map->handle, LV2_PATCH__value);
	self->afs_sf2file        = map->map (map->handle, AFS_URN ":sf2file");

	return (LV2_Handle)self;
}

static void
connect_port (LV2_Handle instance,
              uint32_t   port,
              void*      data)
{
	AFluidSynth* self = (AFluidSynth*)instance;

	switch (port) {
		case FS_PORT_CONTROL:
			self->control = (const LV2_Atom_Sequence*)data;
			break;
		case FS_PORT_NOTIFY:
			self->notify = (LV2_Atom_Sequence*)data;
			break;
		default:
			if (port < FS_PORT_LAST) {
				self->p_ports[port] = (float*)data;
			}
			break;
	}
}

static void
deactivate (LV2_Handle instance)
{
	AFluidSynth* self = (AFluidSynth*)instance;
	self->panic = true;
}

static void
run (LV2_Handle instance, uint32_t n_samples)
{
	AFluidSynth* self = (AFluidSynth*)instance;

	if (!self->control || !self->notify) {
		return;
	}

	const uint32_t capacity = self->notify->atom.size;
	lv2_atom_forge_set_buffer (&self->forge, (uint8_t*)self->notify, capacity);
	lv2_atom_forge_sequence_head (&self->forge, &self->frame, 0);

	if (!self->initialized || self->reinit_in_progress) {
		memset (self->p_ports[FS_PORT_OUT_L], 0, n_samples * sizeof (float));
		memset (self->p_ports[FS_PORT_OUT_R], 0, n_samples * sizeof (float));
	} else if (self->panic) {
		fluid_synth_all_notes_off (self->synth, -1);
		fluid_synth_all_sounds_off (self->synth, -1);
		self->panic = false;
	}

	if (self->initialized && !self->reinit_in_progress) {
		bool rev_change = false;
		bool chr_change = false;
		// TODO clamp values to ranges
		if (self->v_ports[FS_OUT_GAIN] != *self->p_ports[FS_OUT_GAIN]) {
			fluid_synth_set_gain (self->synth, db_to_coeff (*self->p_ports[FS_OUT_GAIN]));
		}
		if (self->v_ports[FS_REV_ENABLE] != *self->p_ports[FS_REV_ENABLE]) {
			fluid_synth_set_reverb_on (self->synth, *self->p_ports[FS_REV_ENABLE] > 0 ? 1 : 0);
			rev_change = true;
		}
		if (self->v_ports[FS_CHR_ENABLE] != *self->p_ports[FS_CHR_ENABLE]) {
			fluid_synth_set_chorus_on (self->synth, *self->p_ports[FS_CHR_ENABLE] > 0 ? 1 : 0);
			chr_change = true;
		}

		for (uint32_t p = FS_REV_ROOMSIZE; p <= FS_REV_LEVEL && !rev_change; ++p) {
			if (self->v_ports[p] != *self->p_ports[p]) {
				rev_change = true;
			}
		}
		for (uint32_t p = FS_CHR_N; p <= FS_CHR_TYPE && !chr_change; ++p) {
			if (self->v_ports[p] != *self->p_ports[p]) {
				chr_change = true;
			}
		}

		if (rev_change) {
			fluid_synth_set_reverb (self->synth,
					*self->p_ports[FS_REV_ROOMSIZE],
					*self->p_ports[FS_REV_DAMPING],
					*self->p_ports[FS_REV_WIDTH],
					*self->p_ports[FS_REV_LEVEL]);
		}

		if (chr_change) {
			fluid_synth_set_chorus (self->synth,
					rintf (*self->p_ports[FS_CHR_N]),
					db_to_coeff (*self->p_ports[FS_CHR_LEVEL]),
					*self->p_ports[FS_CHR_SPEED],
					*self->p_ports[FS_CHR_DEPTH],
					(*self->p_ports[FS_CHR_TYPE] > 0) ? FLUID_CHORUS_MOD_SINE : FLUID_CHORUS_MOD_TRIANGLE);
		}
		for (uint32_t p = FS_OUT_GAIN; p < FS_PORT_LAST; ++p) {
			self->v_ports[p] = *self->p_ports[p];
		}
	}

	uint32_t offset = 0;

	LV2_ATOM_SEQUENCE_FOREACH (self->control, ev) {
		const LV2_Atom_Object* obj = (LV2_Atom_Object*)&ev->body;
		if (ev->body.type == self->atom_Blank || ev->body.type == self->atom_Object) {
			if (obj->body.otype == self->patch_Get) {
				self->inform_ui = false;
				inform_ui (self);
			}
			else if (obj->body.otype == self->patch_Set) {
				const LV2_Atom* file_path = parse_patch_msg (self, obj);
				if (file_path && !self->reinit_in_progress && !self->queue_reinit) {
					const char *fn = (const char*)(file_path+1);
					strncpy (self->queue_sf2_file_path, fn, 1023);
					self->queue_sf2_file_path[1023] = '\0';
					self->reinit_in_progress = true;
					int magic = 0x4711;
					self->schedule->schedule_work (self->schedule->handle, sizeof (int), &magic);
				}
			}
		}
		else if (ev->body.type == self->midi_MidiEvent && self->initialized && !self->reinit_in_progress) {
			if (ev->body.size > 3 || ev->time.frames >= n_samples) {
				continue;
			}

			if (ev->time.frames > offset) {
				fluid_synth_write_float (
						self->synth,
						ev->time.frames - offset,
						&self->p_ports[FS_PORT_OUT_L][offset], 0, 1,
						&self->p_ports[FS_PORT_OUT_R][offset], 0, 1);
			}

			offset = ev->time.frames;

			const uint8_t* const data = (const uint8_t*)(ev + 1);
			fluid_midi_event_set_type (self->fmidi_event, data[0] & 0xf0);
			fluid_midi_event_set_channel (self->fmidi_event, data[0] & 0x0f);
			if (ev->body.size > 1) {
				fluid_midi_event_set_key (self->fmidi_event, data[1]);
			}
			if (ev->body.size > 2) {
				fluid_midi_event_set_value (self->fmidi_event, data[2]);
			}
			fluid_synth_handle_midi_event (self->synth, self->fmidi_event);
		}
	}

	if (self->queue_reinit && !self->reinit_in_progress) {
		self->reinit_in_progress = true;
		int magic = 0x4711;
		self->schedule->schedule_work (self->schedule->handle, sizeof (int), &magic);
	}

	/* inform the GUI */
	if (self->inform_ui) {
		self->inform_ui = false;
		inform_ui (self);
	}

	if (n_samples > offset && self->initialized && !self->reinit_in_progress) {
		fluid_synth_write_float (
				self->synth,
				n_samples - offset,
				&self->p_ports[FS_PORT_OUT_L][offset], 0, 1,
				&self->p_ports[FS_PORT_OUT_R][offset], 0, 1);
	}
}

static void cleanup (LV2_Handle instance)
{
	AFluidSynth* self = (AFluidSynth*)instance;
	delete_fluid_synth (self->synth);
	delete_fluid_settings (self->settings);
	delete_fluid_midi_event (self->fmidi_event);
	free (self);
}

/* *****************************************************************************
 * LV2 Extensions
 */

static LV2_Worker_Status
work (LV2_Handle                  instance,
      LV2_Worker_Respond_Function respond,
      LV2_Worker_Respond_Handle   handle,
      uint32_t                    size,
      const void*                 data)
{
	AFluidSynth* self = (AFluidSynth*)instance;

  if (size != sizeof (int)) {
		return LV2_WORKER_ERR_UNKNOWN;
	}
	int magic = *((const int*)data);
	if (magic != 0x4711) {
		return LV2_WORKER_ERR_UNKNOWN;
	}

	self->initialized = load_sf2 (self, self->queue_sf2_file_path);

	if (self->initialized) {
		fluid_synth_all_notes_off (self->synth, -1);
		fluid_synth_all_sounds_off (self->synth, -1);
		self->panic = false;
		// boostrap synth engine.
		float l[1024];
		float r[1024];
		fluid_synth_write_float (self->synth, 1024, l, 0, 1, r, 0, 1);
	}

	respond (handle, 1, "");
	return LV2_WORKER_SUCCESS;
}

static LV2_Worker_Status
work_response (LV2_Handle  instance,
               uint32_t    size,
               const void* data)
{
	AFluidSynth* self = (AFluidSynth*)instance;

	if (self->initialized) {
		strcpy (self->current_sf2_file_path, self->queue_sf2_file_path);
	} else {
		self->current_sf2_file_path[0] = 0;
	}

	self->reinit_in_progress = false;
	self->inform_ui = true;
	self->queue_reinit = false;
	return LV2_WORKER_SUCCESS;
}

static LV2_State_Status
save (LV2_Handle                instance,
      LV2_State_Store_Function  store,
      LV2_State_Handle          handle,
      uint32_t                  flags,
      const LV2_Feature* const* features)
{
	AFluidSynth* self = (AFluidSynth*)instance;

	if (strlen (self->current_sf2_file_path) == 0) {
		return LV2_STATE_ERR_NO_PROPERTY;
	}

	LV2_State_Map_Path* map_path = NULL;

	for (int i = 0; features[i]; ++i) {
		if (!strcmp (features[i]->URI, LV2_STATE__mapPath)) {
			map_path = (LV2_State_Map_Path*) features[i]->data;
		}
	}

	if (!map_path) {
		return LV2_STATE_ERR_NO_FEATURE;
	}

	char* apath = map_path->abstract_path (map_path->handle, self->current_sf2_file_path);
	store (handle, self->afs_sf2file,
			apath, strlen (apath) + 1,
			self->atom_Path,
			LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);

	return LV2_STATE_SUCCESS;
}

static LV2_State_Status
restore (LV2_Handle                  instance,
         LV2_State_Retrieve_Function retrieve,
         LV2_State_Handle            handle,
         uint32_t                    flags,
         const LV2_Feature* const*   features)
{
	AFluidSynth* self = (AFluidSynth*)instance;
	if (self->reinit_in_progress || self->queue_reinit) {
		lv2_log_warning (&self->logger, "a-fluidsynth.lv2: sf2 load already queued.\n");
		return LV2_STATE_ERR_UNKNOWN;
	}

  size_t   size;
  uint32_t type;
  uint32_t valflags;

  const void* value = retrieve (handle, self->afs_sf2file, &size, &type, &valflags);
	if (value) {
		strncpy (self->queue_sf2_file_path, value, 1023);
		self->queue_sf2_file_path[1023] = '\0';
		self->queue_reinit = true;
	}
	return LV2_STATE_SUCCESS;
}

static const void*
extension_data (const char* uri)
{
	static const LV2_Worker_Interface worker = { work, work_response, NULL };
	static const LV2_State_Interface  state  = { save, restore };
	if (!strcmp (uri, LV2_WORKER__interface)) {
		return &worker;
	}
	else if (!strcmp (uri, LV2_STATE__interface)) {
		return &state;
	}
	return NULL;
}

static const LV2_Descriptor descriptor = {
	AFS_URN,
	instantiate,
	connect_port,
	NULL,
	run,
	deactivate,
	cleanup,
	extension_data
};

#undef LV2_SYMBOL_EXPORT
#ifdef _WIN32
#    define LV2_SYMBOL_EXPORT __declspec(dllexport)
#else
#    define LV2_SYMBOL_EXPORT  __attribute__ ((visibility ("default")))
#endif
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
