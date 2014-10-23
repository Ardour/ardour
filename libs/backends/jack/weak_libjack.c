/* runtime/weak dynamic JACK linking
 *
 * (C) 2014 Robin Gareus <robin@gareus.org>
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "weak_libjack.h"

#ifndef USE_WEAK_JACK

int have_libjack (void) {
	return 0;
}

#else

#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <jack/jack.h>
#include <jack/ringbuffer.h>
#include <jack/transport.h>
#include <jack/midiport.h>
#include <jack/session.h>
#include <jack/thread.h>

static void* lib_open(const char* const so) {
#ifdef PLATFORM_WINDOWS
	return (void*) LoadLibraryA(so);
#else
	return dlopen(so, RTLD_NOW|RTLD_LOCAL);
#endif
}

static void* lib_symbol(void* const lib, const char* const sym) {
#ifdef PLATFORM_WINDOWS
	return (void*) GetProcAddress((HMODULE)lib, sym);
#else
	return dlsym(lib, sym);
#endif
}

#ifdef COMPILER_MSVC
typedef void * pvoid_t;
#define MAPSYM(SYM, FAIL) _j._ ## SYM = lib_symbol(lib, "jack_" # SYM); \
	if (!_j._ ## SYM) err |= FAIL;
#else
typedef void * __attribute__ ((__may_alias__)) pvoid_t;
#define MAPSYM(SYM, FAIL) *(pvoid_t *)(&_j._ ## SYM) = lib_symbol(lib, "jack_" # SYM); \
	if (!_j._ ## SYM) err |= FAIL;
#endif
typedef void (* func_t) (void);

/* function pointers to the real jack API */
static struct WeakJack {
	func_t _client_open;
	func_t _client_close;
	func_t _get_client_name;

	func_t _get_buffer_size;
	func_t _get_sample_rate;
	func_t _frames_since_cycle_start;
	func_t _frame_time;
	func_t _last_frame_time;
	func_t _cpu_load;
	func_t _is_realtime;

	func_t _set_freewheel;
	func_t _set_buffer_size;

	func_t _on_shutdown;
	func_t _on_info_shutdown;
	func_t _set_process_callback;
	func_t _set_freewheel_callback;
	func_t _set_buffer_size_callback;
	func_t _set_sample_rate_callback;
	func_t _set_port_registration_callback;
	func_t _set_port_connect_callback;
	func_t _set_graph_order_callback;
	func_t _set_xrun_callback;
	func_t _set_latency_callback;
	func_t _set_error_function;

	func_t _activate;
	func_t _deactivate;

	func_t _recompute_total_latencies;
	func_t _port_get_total_latency;
	func_t _port_get_latency_range;
	func_t _port_set_latency_range;
	func_t _port_get_buffer;
	func_t _port_request_monitor;
	func_t _port_ensure_monitor;
	func_t _port_monitoring_input;

	func_t _port_name;
	func_t _port_flags;
	func_t _get_ports;
	func_t _port_name_size;
	func_t _port_type_size;
	func_t _port_type_get_buffer_size;
	func_t _port_by_name;
	func_t _port_by_id;
	func_t _port_register;
	func_t _port_unregister;
	func_t _port_type;
	func_t _port_get_connections;
	func_t _port_get_all_connections;
	func_t _port_set_name;
	func_t _port_disconnect;
	func_t _connect;
	func_t _disconnect;
	func_t _free;
	func_t _cycle_wait;
	func_t _cycle_signal;
	func_t _set_process_thread;
	func_t _set_thread_init_callback;

	func_t _get_current_transport_frame;
	func_t _transport_locate;
	func_t _transport_start;
	func_t _transport_stop;
	func_t _transport_query;
	func_t _set_sync_callback;
	func_t _set_timebase_callback;
	func_t _release_timebase;

	func_t _midi_get_event_count;
	func_t _midi_event_get;
	func_t _midi_event_write;
	func_t _midi_clear_buffer;

	func_t _set_session_callback;
	func_t _session_reply;
	func_t _session_event_free;

	func_t _ringbuffer_create;
	func_t _ringbuffer_free;
	func_t _ringbuffer_reset;
	func_t _ringbuffer_read_advance;
	func_t _ringbuffer_write_advance;
	func_t _ringbuffer_read_space;
	func_t _ringbuffer_write_space;
	func_t _ringbuffer_read;
	func_t _ringbuffer_write;
	func_t _ringbuffer_mlock;

	func_t _client_real_time_priority;
	func_t _client_max_real_time_priority;
	func_t _acquire_real_time_scheduling;
	func_t _drop_real_time_scheduling;
	func_t _client_stop_thread;
	func_t _client_kill_thread;
	func_t _client_create_thread;
} _j;

static int _status = -1;

int have_libjack (void) {
	return _status;
}

__attribute__((constructor))
static void init_weak_jack(void)
{
	void* lib;
	int err = 0;

	memset(&_j, 0, sizeof(_j));

#ifdef __APPLE__
	lib = lib_open("libjack.dylib");
	if (!lib) {
		lib = lib_open("/usr/local/lib/libjack.dylib");
	}
#elif (defined PLATFORM_WINDOWS)
	lib = lib_open("libjack.dll");
#else
	lib = lib_open("libjack.so");
#endif
	if (!lib) {
		_status = -2;
		return;
	}

	MAPSYM(client_open, 2)
	MAPSYM(client_close, 1)
	MAPSYM(get_client_name, 1)
	MAPSYM(get_sample_rate, 1)
	MAPSYM(get_buffer_size, 1)
	MAPSYM(frames_since_cycle_start, 1)
	MAPSYM(frame_time, 1)
	MAPSYM(last_frame_time, 1)
	MAPSYM(cpu_load, 1)
	MAPSYM(is_realtime, 1)
	MAPSYM(set_freewheel, 1)
	MAPSYM(set_buffer_size, 1)
	MAPSYM(on_shutdown, 0)
	MAPSYM(on_info_shutdown, 0)
	MAPSYM(set_process_callback, 1)
	MAPSYM(set_freewheel_callback, 1)
	MAPSYM(set_buffer_size_callback, 1)
	MAPSYM(set_sample_rate_callback, 1)
	MAPSYM(set_port_registration_callback, 1)
	MAPSYM(set_port_connect_callback, 1)
	MAPSYM(set_graph_order_callback, 1)
	MAPSYM(set_xrun_callback, 1)
	MAPSYM(set_latency_callback, 1)
	MAPSYM(set_error_function, 1)
	MAPSYM(activate, 1)
	MAPSYM(deactivate, 1)
	MAPSYM(recompute_total_latencies, 0)
	MAPSYM(port_get_total_latency, 0)
	MAPSYM(port_get_latency_range, 0)
	MAPSYM(port_set_latency_range, 0)
	MAPSYM(port_get_buffer, 1)
	MAPSYM(port_request_monitor, 1)
	MAPSYM(port_ensure_monitor, 1)
	MAPSYM(port_monitoring_input, 1)
	MAPSYM(port_name, 1)
	MAPSYM(port_flags, 1)
	MAPSYM(get_ports, 1)
	MAPSYM(port_name_size, 1)
	MAPSYM(port_type_size, 1)
	MAPSYM(port_type_get_buffer_size, 1)
	MAPSYM(port_by_name, 1)
	MAPSYM(port_by_id, 1)
	MAPSYM(port_register, 1)
	MAPSYM(port_unregister, 1)
	MAPSYM(port_type, 1)
	MAPSYM(port_get_connections, 1)
	MAPSYM(port_get_all_connections, 1)
	MAPSYM(port_set_name, 1)
	MAPSYM(port_disconnect, 1)
	MAPSYM(connect, 1)
	MAPSYM(disconnect, 1)
	MAPSYM(free, 0)
	MAPSYM(cycle_wait, 0)
	MAPSYM(cycle_signal, 0)
	MAPSYM(set_process_thread, 0)
	MAPSYM(set_thread_init_callback, 0)
	MAPSYM(get_current_transport_frame, 1)
	MAPSYM(transport_locate, 1)
	MAPSYM(transport_start, 1)
	MAPSYM(transport_stop, 1)
	MAPSYM(transport_query, 1)
	MAPSYM(set_sync_callback, 1)
	MAPSYM(set_timebase_callback, 1)
	MAPSYM(release_timebase, 1)
	MAPSYM(midi_get_event_count, 1)
	MAPSYM(midi_event_get, 1)
	MAPSYM(midi_event_write, 1)
	MAPSYM(midi_clear_buffer, 1)
	MAPSYM(set_session_callback, 0)
	MAPSYM(session_reply, 0)
	MAPSYM(session_event_free, 0)
	MAPSYM(ringbuffer_create, 1)
	MAPSYM(ringbuffer_free, 1)
	MAPSYM(ringbuffer_reset, 1)
	MAPSYM(ringbuffer_read_advance, 1)
	MAPSYM(ringbuffer_write_advance, 1)
	MAPSYM(ringbuffer_read_space, 1)
	MAPSYM(ringbuffer_write_space, 1)
	MAPSYM(ringbuffer_read, 1)
	MAPSYM(ringbuffer_write, 1)
	MAPSYM(ringbuffer_mlock, 0)
	MAPSYM(client_real_time_priority, 0)
	MAPSYM(client_max_real_time_priority, 0)
	MAPSYM(acquire_real_time_scheduling, 0)
	MAPSYM(client_create_thread, 0)
	MAPSYM(drop_real_time_scheduling, 0)
	MAPSYM(client_stop_thread, 0)
	MAPSYM(client_kill_thread, 0)

	/* if a required symbol is not found, disable JACK completly */
	if (err) {
		_j._client_open = NULL;
	}

	_status = err;
}

/*******************************************************************************
 * Macros to wrap jack API
 */

#ifndef NDEBUG
# define WJACK_WARNING(NAME) \
	fprintf(stderr, "*** WEAK-JACK: function 'jack_%s' ignored\n", "" # NAME);
#else
# define WJACK_WARNING(NAME) ;
#endif

/* abstraction for jack_client functions */
#define JCFUN(RTYPE, NAME, RVAL) \
	RTYPE WJACK_ ## NAME (jack_client_t *client) { \
		if (_j._ ## NAME) { \
			return ((RTYPE (*)(jack_client_t *client)) _j._ ## NAME)(client); \
		} else { \
			WJACK_WARNING(NAME) \
			return RVAL; \
		} \
	}

/* abstraction for NOOP functions */
#define JPFUN(RTYPE, NAME, DEF, ARGS, RVAL) \
	RTYPE WJACK_ ## NAME DEF { \
		if (_j._ ## NAME) { \
			return ((RTYPE (*)DEF) _j._ ## NAME) ARGS; \
		} else { \
			WJACK_WARNING(NAME) \
			return RVAL; \
		} \
	}

/* abstraction for functions with return-value-pointer args */
#define JXFUN(RTYPE, NAME, DEF, ARGS, CODE) \
	RTYPE WJACK_ ## NAME DEF { \
		if (_j._ ## NAME) { \
			return ((RTYPE (*)DEF) _j._ ## NAME) ARGS; \
		} else { \
			WJACK_WARNING(NAME) \
			CODE \
		} \
	}

/* abstraction for void functions with return-value-pointer args */
#define JVFUN(RTYPE, NAME, DEF, ARGS, CODE) \
	RTYPE WJACK_ ## NAME DEF { \
		if (_j._ ## NAME) { \
			((RTYPE (*)DEF) _j._ ## NAME) ARGS; \
		} else { \
			WJACK_WARNING(NAME) \
			CODE \
		} \
	}

/******************************************************************************
 * wrapper functions.
 *
 * if a function pointer is set in the static struct WeakJack _j,
 * call the function, if not a dummy NOOP implementation is provided.
 *
 * The latter is mainly for the benefit for compile-time (warnings),
 * if libjack is not found, jack_client_open() will fail and none
 * of the application will never call any of the other functions.
 */

/* <jack/jack.h> */

/* expand ellipsis for jack-session */
jack_client_t * WJACK_client_open2 (const char *client_name, jack_options_t options, jack_status_t *status, const char *uuid) {
	if (_j._client_open) {
		return ((jack_client_t* (*)(const char *, jack_options_t, jack_status_t *, ...))(_j._client_open))(client_name, options, status, uuid);
	} else {
		WJACK_WARNING(client_open);
		if (status) *status = 0;
		return NULL;
	}
}

jack_client_t * WJACK_client_open1 (const char *client_name, jack_options_t options, jack_status_t *status) {
	if (_j._client_open) {
		return ((jack_client_t* (*)(const char *, jack_options_t, jack_status_t *, ...))_j._client_open)(client_name, options, status);
	} else {
		WJACK_WARNING(client_open);
		if (status) *status = 0;
		return NULL;
	}
}

JCFUN(int,   client_close, 0)
JCFUN(char*, get_client_name, NULL)
JVFUN(void,  on_shutdown, (jack_client_t *c, JackShutdownCallback s, void *a), (c,s,a),)
JVFUN(void,  on_info_shutdown, (jack_client_t *c, JackInfoShutdownCallback s, void *a), (c,s,a),)

JPFUN(int,   set_process_callback, (jack_client_t *c, JackProcessCallback p, void *a), (c,p,a), -1)
JPFUN(int,   set_freewheel_callback, (jack_client_t *c, JackFreewheelCallback p, void *a), (c,p,a), -1)
JPFUN(int,   set_buffer_size_callback, (jack_client_t *c, JackBufferSizeCallback p, void *a), (c,p,a), -1)
JPFUN(int,   set_sample_rate_callback, (jack_client_t *c, JackSampleRateCallback p, void *a), (c,p,a), -1)
JPFUN(int,   set_port_registration_callback, (jack_client_t *c, JackPortRegistrationCallback p, void *a), (c,p,a), -1)
JPFUN(int,   set_port_connect_callback, (jack_client_t *c, JackPortConnectCallback p, void *a), (c,p,a), -1)
JPFUN(int,   set_graph_order_callback, (jack_client_t *c, JackGraphOrderCallback g, void *a), (c,g,a), -1)
JPFUN(int,   set_xrun_callback, (jack_client_t *c, JackXRunCallback g, void *a), (c,g,a), -1)
JPFUN(int,   set_latency_callback, (jack_client_t *c, JackLatencyCallback g, void *a), (c,g,a), -1)
JVFUN(void,  set_error_function, (void (*f)(const char *)), (f),)

JCFUN(int,   activate, -1)
JCFUN(int,   deactivate, -1)

JCFUN(jack_nframes_t, get_sample_rate, 0)
JCFUN(jack_nframes_t, get_buffer_size, 0)
JPFUN(jack_nframes_t, frames_since_cycle_start, (const jack_client_t *c), (c), 0)
JPFUN(jack_nframes_t, frame_time, (const jack_client_t *c), (c), 0)
JPFUN(jack_nframes_t, last_frame_time, (const jack_client_t *c), (c), 0)
JCFUN(float,          cpu_load, 0)
JCFUN(int,            is_realtime, 0)

JPFUN(int, set_freewheel, (jack_client_t *c, int o), (c,o), 0)
JPFUN(int, set_buffer_size, (jack_client_t *c, jack_nframes_t b), (c,b), 0)

JCFUN(int,            recompute_total_latencies, 0)
JPFUN(jack_nframes_t, port_get_total_latency, (jack_client_t *c, jack_port_t *p), (c,p), 0)
JVFUN(void,           port_get_latency_range, (jack_port_t *p, jack_latency_callback_mode_t m, jack_latency_range_t *r), (p,m,r), if (r) {r->min = r->max = 0;})
JVFUN(void,           port_set_latency_range, (jack_port_t *p, jack_latency_callback_mode_t m, jack_latency_range_t *r), (p,m,r),)
JPFUN(void*,          port_get_buffer, (jack_port_t *p, jack_nframes_t n), (p,n), NULL)
JPFUN(int,            port_request_monitor, (jack_port_t *p, int o), (p,o), 0)
JPFUN(int,            port_ensure_monitor, (jack_port_t *p, int o), (p,o), 0)
JPFUN(int,            port_monitoring_input, (jack_port_t *p), (p), 0)

JPFUN(const char*,    port_name, (const jack_port_t *p), (p), NULL)
JPFUN(int,            port_flags, (const jack_port_t *p), (p), 0)
JPFUN(const char**,   get_ports,(jack_client_t *c, const char *p, const char *t, unsigned long f), (c,p,t,f), NULL)
JPFUN(int,            port_name_size, (void), (), 0)
JPFUN(int,            port_type_size, (void), (), 0)
JPFUN(size_t,         port_type_get_buffer_size, (jack_client_t *c, const char *t), (c,t), 0)
JPFUN(jack_port_t*,   port_by_name, (jack_client_t *c, const char *n), (c,n), NULL)
JPFUN(jack_port_t*,   port_by_id, (jack_client_t *c, jack_port_id_t i), (c,i), NULL)
JPFUN(jack_port_t*,   port_register, (jack_client_t *c, const char *n, const char *t, unsigned long f, unsigned long b), (c,n,t,f,b), NULL)
JPFUN(int,            port_unregister, (jack_client_t *c, jack_port_t *p), (c,p), 0)
JPFUN(const char *,   port_type, (const jack_port_t *p), (p), 0)
JPFUN(const char **,  port_get_connections, (const jack_port_t *p), (p), 0)
JPFUN(const char **,  port_get_all_connections, (const jack_client_t *c, const jack_port_t *p), (c,p), 0)
JPFUN(int,            port_set_name, (jack_port_t *p, const char *n), (p,n), 0)
JPFUN(int,            port_disconnect, (jack_client_t *c, jack_port_t *p), (c,p), 0)
JPFUN(int,            connect, (jack_client_t *c, const char *s, const char *d), (c,s,d), -1)
JPFUN(int,            disconnect, (jack_client_t *c, const char *s, const char *d), (c,s,d), -1)
JVFUN(void,           free, (void *p), (p), free(p);)
JCFUN(jack_nframes_t, cycle_wait, 0)
JVFUN(void,           cycle_signal, (jack_client_t *c, int s), (c,s),)
JPFUN(int,            set_process_thread, (jack_client_t *c, JackThreadCallback p, void *a), (c,p,a), -1)
JPFUN(int,            set_thread_init_callback, (jack_client_t *c, JackThreadInitCallback p, void *a), (c,p,a), -1)

JPFUN(int,  transport_locate, (jack_client_t *c, jack_nframes_t f), (c,f), 0)
JVFUN(void, transport_start, (jack_client_t *c), (c),)
JVFUN(void, transport_stop, (jack_client_t *c), (c),)
JPFUN(jack_nframes_t, get_current_transport_frame, (const jack_client_t *c), (c), 0)
JXFUN(jack_transport_state_t, transport_query, (const jack_client_t *c, jack_position_t *p), (c,p), memset(p, 0, sizeof(jack_position_t)); return 0;)
JPFUN(int,  set_sync_callback, (jack_client_t *c, JackSyncCallback p, void *a), (c,p,a), -1)
JPFUN(int,  set_timebase_callback, (jack_client_t *c, int l, JackTimebaseCallback p, void *a), (c,l,p,a), -1)
JCFUN(int,  release_timebase, 0)

/* <jack/midiport.h> */
JPFUN(uint32_t, midi_get_event_count, (void* p), (p), 0)
JPFUN(int,      midi_event_get, (jack_midi_event_t *e, void *p, uint32_t i), (e,p,i), -1)
JPFUN(int,      midi_event_write, (void *b, jack_nframes_t t, const jack_midi_data_t *d, size_t s), (b,t,d,s), -1)
JVFUN(void,     midi_clear_buffer, (void *b), (b),)

/* <jack/session.h> */
JPFUN(int, set_session_callback, (jack_client_t *c, JackSessionCallback s, void *a), (c,s,a), -1)
JPFUN(int, session_reply, (jack_client_t *c, jack_session_event_t *e), (c,e), -1)
JVFUN(void, session_event_free, (jack_session_event_t *e), (e), )

/* <jack/ringbuffer.h> */
JPFUN(jack_ringbuffer_t *, ringbuffer_create, (size_t s), (s), NULL)
JVFUN(void, ringbuffer_free, (jack_ringbuffer_t *rb), (rb), )
JVFUN(void, ringbuffer_reset, (jack_ringbuffer_t *rb), (rb), )
JVFUN(void, ringbuffer_read_advance, (jack_ringbuffer_t *rb, size_t c), (rb,c), )
JVFUN(void, ringbuffer_write_advance, (jack_ringbuffer_t *rb, size_t c), (rb,c), )
JPFUN(size_t, ringbuffer_read_space, (const jack_ringbuffer_t *rb), (rb), 0)
JPFUN(size_t, ringbuffer_write_space, (const jack_ringbuffer_t *rb), (rb), 0)
JPFUN(size_t, ringbuffer_read, (jack_ringbuffer_t *rb, char *d, size_t c), (rb,d,c), 0)
JPFUN(size_t, ringbuffer_write, (jack_ringbuffer_t *rb, const char *s, size_t c), (rb,s,c), 0)
JPFUN(int, ringbuffer_mlock, (jack_ringbuffer_t *rb), (rb), 0)

/* <jack/thread.h> */
JCFUN(int, client_real_time_priority, 0)
JCFUN(int, client_max_real_time_priority, 0)
JPFUN(int, acquire_real_time_scheduling, (jack_native_thread_t t, int p), (t,p), 0)
JPFUN(int, drop_real_time_scheduling, (jack_native_thread_t t), (t), 0)
JPFUN(int, client_stop_thread, (jack_client_t* c, jack_native_thread_t t), (c,t), 0)
JPFUN(int, client_kill_thread, (jack_client_t* c, jack_native_thread_t t), (c,t), 0)
JPFUN(int, client_create_thread, \
		(jack_client_t* c, jack_native_thread_t *t, int p, int r, void *(*f)(void*), void *a), (c,t,p,r,f,a), 0)

#endif
