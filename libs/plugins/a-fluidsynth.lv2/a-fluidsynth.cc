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

#include <cstring>
#include <string>
#include <algorithm>
#include <map>
#include <vector>

#include <pthread.h>
#include <stdlib.h>
#include <math.h>

#define AFS_URN "urn:ardour:a-fluidsynth"

#ifdef HAVE_LV2_1_10_0
#define x_forge_object lv2_atom_forge_object
#else
#define x_forge_object lv2_atom_forge_blank
#endif

#ifdef LV2_EXTENDED
#include "../../ardour/ardour/lv2_extensions.h"
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

struct BankProgram {
	BankProgram (const std::string& n, int b, int p)
		: name (n)
		, bank (b)
		, program (p)
	{}

	BankProgram (const BankProgram& other)
		: name (other.name)
		, bank (other.bank)
		, program (other.program)
	{}

	std::string name;
	int bank;
	int program;
};

typedef std::vector<BankProgram> BPList;
typedef std::map<int, BPList> BPMap;
typedef std::map<int, BankProgram> BPState;

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

#ifdef LV2_EXTENDED
	LV2_Midnam*          midnam;
	BPMap                presets;
#endif
	pthread_mutex_t      bp_lock;

	/* state */
	bool panic;
	bool initialized;
	bool inform_ui;

	char current_sf2_file_path[1024];
	char queue_sf2_file_path[1024];
	bool reinit_in_progress; // set in run, cleared in work_response
	bool queue_reinit; // set in restore, cleared in work_response

	uint8_t last_bank_lsb;
	uint8_t last_bank_msb;
	BankProgram program_state[16];

	fluid_midi_event_t* fmidi_event;

} AFluidSynth;

/* *****************************************************************************
 * helpers
 */

static bool
load_sf2 (AFluidSynth* self, const char* fn)
{
	const int synth_id = fluid_synth_sfload (self->synth, fn, 1);

#ifdef LV2_EXTENDED
	pthread_mutex_lock (&self->bp_lock);
	self->presets.clear ();
	pthread_mutex_unlock (&self->bp_lock);
#endif

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
	pthread_mutex_lock (&self->bp_lock);
	for (chn = 0; sfont->iteration_next (sfont, &preset); ++chn) {
		if (chn < 16) {
			fluid_synth_program_select (self->synth, chn, synth_id,
					preset.get_banknum (&preset), preset.get_num (&preset));
		}
#ifndef LV2_EXTENDED
		else { break ; }
#else
		self->presets[preset.get_banknum (&preset)].push_back (
				BankProgram (
					preset.get_name (&preset),
					preset.get_banknum (&preset),
					preset.get_num (&preset)));
#endif
	}
	pthread_mutex_unlock (&self->bp_lock);

	if (chn == 0) {
		return false;
	}

	for (chn = 0; chn < 16; ++chn) {
		if (self->program_state[chn].program < 0) {
			continue;
		}
		fluid_synth_program_select (self->synth, chn, synth_id,
				self->program_state[chn].bank, self->program_state[chn].program);
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
#ifdef LV2_EXTENDED
		} else if (!strcmp (features[i]->URI, LV2_MIDNAM__update)) {
			self->midnam = (LV2_Midnam*)features[i]->data;
#endif
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
	fluid_settings_setstr (self->settings, "synth.midi-bank-select", "mma");

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

	pthread_mutex_init (&self->bp_lock, NULL);
#ifdef LV2_EXTENDED
	self->presets = BPMap();
#endif
	self->panic = false;
	self->inform_ui = false;
	self->initialized = false;
	self->reinit_in_progress = false;
	self->queue_reinit = false;
	for (int chn = 0; chn < 16; ++chn) {
		self->program_state[chn].program = -1;
	}

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
		else if (ev->body.type == self->midi_MidiEvent) {
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
				if (0xe0 /*PITCH_BEND*/ == fluid_midi_event_get_type (self->fmidi_event)) {
					fluid_midi_event_set_value (self->fmidi_event, 0);
					fluid_midi_event_set_pitch (self->fmidi_event, ((data[2] & 0x7f) << 7) | (data[1] & 0x7f));
				} else {
					fluid_midi_event_set_value (self->fmidi_event, data[2]);
				}
				if (0xb0 /* CC */ == fluid_midi_event_get_type (self->fmidi_event)) {
					if (data[1] == 0x00) { self->last_bank_msb = data[2]; }
					if (data[1] == 0x20) { self->last_bank_lsb = data[2]; }
				}
			}
			if (ev->body.size == 2 && 0xc0 /* Pgm */ == fluid_midi_event_get_type (self->fmidi_event)) {
				int chn = fluid_midi_event_get_channel (self->fmidi_event);
				assert (chn >= 0 && chn < 16);
				self->program_state[chn].bank = (self->last_bank_msb << 7) | self->last_bank_lsb;
				self->program_state[chn].program = data[1];
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
#ifdef LV2_EXTENDED
    self->midnam->update (self->midnam->handle);
#endif
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
	pthread_mutex_destroy (&self->bp_lock);
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
			self->atom_Path, LV2_STATE_IS_POD);

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
		strncpy (self->queue_sf2_file_path, (const char*) value, 1023);
		self->queue_sf2_file_path[1023] = '\0';
		self->queue_reinit = true;
	}
	return LV2_STATE_SUCCESS;
}

static std::string xml_escape (const std::string& s)
{
	std::string r (s);
	std::replace (r.begin (), r.end(), '"', '\'');
	size_t pos = 0;
	while((pos = r.find ("&", pos)) != std::string::npos) {
		r.replace (pos, 1, "&amp;");
		pos += 4;
	}
	return r;
}

#ifdef LV2_EXTENDED
static char*
mn_file (LV2_Handle instance)
{
	AFluidSynth* self = (AFluidSynth*)instance;
	char* rv = NULL;
	char tmp[1024];

	rv = (char*) calloc (1, sizeof (char));

#define pf(...)                                              \
	do {                                                       \
	  snprintf (tmp, sizeof(tmp), __VA_ARGS__);                \
	  tmp[sizeof(tmp) - 1] = '\0';                             \
	  rv = (char*)realloc (rv, strlen (rv) + strlen(tmp) + 1); \
	  strcat (rv, tmp);                                        \
	} while (0)

	pf ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<!DOCTYPE MIDINameDocument PUBLIC \"-//MIDI Manufacturers Association//DTD MIDINameDocument 1.0//EN\" \"http://dev.midi.org/dtds/MIDINameDocument10.dtd\">\n"
			"<MIDINameDocument>\n"
			"  <Author/>\n"
			"  <MasterDeviceNames>\n"
			"    <Manufacturer>Ardour Foundation</Manufacturer>\n"
			"    <Model>%s:%p</Model>\n", AFS_URN, (void*) self);


	pf ("    <CustomDeviceMode Name=\"Default\">\n");
	pf ("      <ChannelNameSetAssignments>\n");
	for (int c = 0; c < 16; ++c) {
		pf ("        <ChannelNameSetAssign Channel=\"%d\" NameSet=\"Presets\"/>\n", c + 1);
	}
	pf ("      </ChannelNameSetAssignments>\n");
	pf ("    </CustomDeviceMode>\n");

	// TODO collect used banks, std::set<> would be a nice here

	pf ("    <ChannelNameSet Name=\"Presets\">\n");
	pf ("      <AvailableForChannels>\n");
	for (int c = 0; c < 16; ++c) {
		pf ("        <AvailableChannel Channel=\"%d\" Available=\"true\"/>\n", c + 1);
	}
	pf ("      </AvailableForChannels>\n");
	pf ("      <UsesControlNameList Name=\"Controls\"/>\n");

	int bn = 1;
	pthread_mutex_lock (&self->bp_lock);
	const BPMap& ps (self->presets);
	pthread_mutex_unlock (&self->bp_lock);

	for (BPMap::const_iterator i = ps.begin (); i != ps.end (); ++i, ++bn) {
		pf ("      <PatchBank Name=\"Patch Bank %d\">\n", i->first);
		if (i->second.size() > 0) {
			pf ("        <MIDICommands>\n");
			pf ("            <ControlChange Control=\"0\" Value=\"%d\"/>\n", (i->first >> 7) & 127);
			pf ("            <ControlChange Control=\"32\" Value=\"%d\"/>\n", i->first & 127);
			pf ("        </MIDICommands>\n");
			pf ("        <PatchNameList>\n");
			int n = 0;
			for (BPList::const_iterator j = i->second.begin(); j != i->second.end(); ++j, ++n) {
				pf ("      <Patch Number=\"%d\" Name=\"%s\" ProgramChange=\"%d\"/>\n",
						n, xml_escape (j->name).c_str(), j->program);
			}
			pf ("        </PatchNameList>\n");
		}
		pf ("      </PatchBank>\n");
	}
	pf ("    </ChannelNameSet>\n");

	pf ("    <ControlNameList Name=\"Controls\">\n");
	pf ("       <Control Type=\"7bit\" Number=\"7\" Name=\"Channel Volume\"/>\n");
	pf ("       <Control Type=\"7bit\" Number=\"10\" Name=\"Pan\"/>\n");
	pf ("       <Control Type=\"7bit\" Number=\"39\" Name=\"Channel Volume (Fine)\"/>\n");
	pf ("       <Control Type=\"7bit\" Number=\"42\" Name=\"Pan (Fine)\"/>\n");
	pf ("       <Control Type=\"7bit\" Number=\"64\" Name=\"Damper Pedal (Sustain)\"/>\n");
	pf ("       <Control Type=\"7bit\" Number=\"66\" Name=\"Sostenuto\"/>\n");
	pf ("       <Control Type=\"7bit\" Number=\"91\" Name=\"Reverb\"/>\n");
	pf ("       <Control Type=\"7bit\" Number=\"93\" Name=\"Chorus\"/>\n");
	pf ("    </ControlNameList>\n");

	pf (
			"  </MasterDeviceNames>\n"
			"</MIDINameDocument>"
		 );

	//printf("-----\n%s\n------\n", rv);
	return rv;
}

static char*
mn_model (LV2_Handle instance)
{
	AFluidSynth* self = (AFluidSynth*)instance;
	char* rv = (char*) malloc (64 * sizeof (char));
	snprintf (rv, 64, "%s:%p", AFS_URN, (void*) self);
	rv[63] = 0;
	return rv;
}

static void
mn_free (char* v)
{
	free (v);
}
#endif

static const void*
extension_data (const char* uri)
{
	if (!strcmp (uri, LV2_WORKER__interface)) {
		static const LV2_Worker_Interface worker = { work, work_response, NULL };
		return &worker;
	}
	else if (!strcmp (uri, LV2_STATE__interface)) {
		static const LV2_State_Interface  state  = { save, restore };
		return &state;
	}
#ifdef LV2_EXTENDED
	else if (!strcmp (uri, LV2_MIDNAM__interface)) {
		static const LV2_Midnam_Interface midnam = { mn_file, mn_model, mn_free };
		return &midnam;
	}
#endif
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
