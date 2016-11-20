/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */


#ifndef _FLUIDSYNTH_PRIV_H
#define _FLUIDSYNTH_PRIV_H

#include <glib.h>

#if HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_STRING_H
#include <string.h>
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if HAVE_STDIO_H
#include <stdio.h>
#endif

#if HAVE_MATH_H
#include <math.h>
#endif

#if HAVE_ERRNO_H
#include <errno.h>
#endif

#if HAVE_STDARG_H
#include <stdarg.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#if HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#if HAVE_PTHREAD_H
#include <pthread.h>
#endif

#if HAVE_IO_H
#include <io.h>
#endif

#if HAVE_WINDOWS_H
#include <windows.h>
#endif

/* MinGW32 special defines */
#ifdef MINGW32

#include <stdint.h>
#define snprintf _snprintf
#define vsnprintf _vsnprintf

#define DSOUND_SUPPORT 1
#define WINMIDI_SUPPORT 1
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#elif defined _MSC_VER

#define STDIN_FILENO  _fileno(stdin)
#define STDOUT_FILENO _fileno(stdout)
#define STDERR_FILENO _fileno(stderr)

#endif

/* Darwin special defines (taken from config_macosx.h) */
#ifdef DARWIN
#define MACINTOSH
#define __Types__
#define WITHOUT_SERVER 1
#endif


#include "fluidsynth.h"


/***************************************************************
 *
 *         BASIC TYPES
 */

#if defined(WITH_FLOAT)
typedef float fluid_real_t;
#else
typedef double fluid_real_t;
#endif


#if defined(WIN32)
typedef SOCKET fluid_socket_t;
#else
typedef int fluid_socket_t;
#define INVALID_SOCKET -1
#endif

#if defined(SUPPORTS_VLA)
#  define FLUID_DECLARE_VLA(_type, _name, _len) \
     _type _name[_len]
#else
#  define FLUID_DECLARE_VLA(_type, _name, _len) \
     _type* _name = g_newa(_type, (_len))
#endif


/** Integer types  */
//typedef gint8              sint8;
typedef guint8             uint8;
//typedef gint16             sint16;
//typedef guint16            uint16;
typedef gint32             sint32;
typedef guint32            uint32;
//typedef gint64             sint64;
//typedef guint64            uint64;


/***************************************************************
 *
 *       FORWARD DECLARATIONS
 */
typedef struct _fluid_env_data_t fluid_env_data_t;
typedef struct _fluid_adriver_definition_t fluid_adriver_definition_t;
typedef struct _fluid_channel_t fluid_channel_t;
typedef struct _fluid_tuning_t fluid_tuning_t;
typedef struct _fluid_hashtable_t  fluid_hashtable_t;
typedef struct _fluid_client_t fluid_client_t;
typedef struct _fluid_server_socket_t fluid_server_socket_t;
typedef struct _fluid_sample_timer_t fluid_sample_timer_t;

/***************************************************************
 *
 *                      CONSTANTS
 */

#define FLUID_BUFSIZE                64         /**< FluidSynth internal buffer size (in samples) */
#define FLUID_MAX_EVENTS_PER_BUFSIZE 1024       /**< Maximum queued MIDI events per #FLUID_BUFSIZE */
#define FLUID_MAX_RETURN_EVENTS      1024       /**< Maximum queued synthesis thread return events */
#define FLUID_MAX_EVENT_QUEUES       16         /**< Maximum number of unique threads queuing events */
#define FLUID_DEFAULT_AUDIO_RT_PRIO  60         /**< Default setting for audio.realtime-prio */
#define FLUID_DEFAULT_MIDI_RT_PRIO   50         /**< Default setting for midi.realtime-prio */

#ifndef PI
#define PI                          3.141592654
#endif

/***************************************************************
 *
 *                      SYSTEM INTERFACE
 */
typedef FILE*  fluid_file;

#define FLUID_MALLOC(_n)             malloc(_n)
#define FLUID_REALLOC(_p,_n)         realloc(_p,_n)
#define FLUID_NEW(_t)                (_t*)malloc(sizeof(_t))
#define FLUID_ARRAY(_t,_n)           (_t*)malloc((_n)*sizeof(_t))
#define FLUID_FREE(_p)               free(_p)
#define FLUID_FOPEN(_f,_m)           fopen(_f,_m)
#define FLUID_FCLOSE(_f)             fclose(_f)
#define FLUID_FREAD(_p,_s,_n,_f)     fread(_p,_s,_n,_f)
#define FLUID_FSEEK(_f,_n,_set)      fseek(_f,_n,_set)
#define FLUID_MEMCPY(_dst,_src,_n)   memcpy(_dst,_src,_n)
#define FLUID_MEMSET(_s,_c,_n)       memset(_s,_c,_n)
#define FLUID_STRLEN(_s)             strlen(_s)
#define FLUID_STRCMP(_s,_t)          strcmp(_s,_t)
#define FLUID_STRNCMP(_s,_t,_n)      strncmp(_s,_t,_n)
#define FLUID_STRCPY(_dst,_src)      strcpy(_dst,_src)
#define FLUID_STRNCPY(_dst,_src,_n)  strncpy(_dst,_src,_n)
#define FLUID_STRCHR(_s,_c)          strchr(_s,_c)
#define FLUID_STRRCHR(_s,_c)         strrchr(_s,_c)
#ifdef strdup
#define FLUID_STRDUP(s)              strdup(s)
#else
#define FLUID_STRDUP(s) 		    FLUID_STRCPY(FLUID_MALLOC(FLUID_STRLEN(s) + 1), s)
#endif
#define FLUID_SPRINTF                sprintf
#define FLUID_FPRINTF                fprintf

#define fluid_clip(_val, _min, _max) \
{ (_val) = ((_val) < (_min))? (_min) : (((_val) > (_max))? (_max) : (_val)); }

#if WITH_FTS
#define FLUID_PRINTF                 post
#define FLUID_FLUSH()
#else
#define FLUID_PRINTF                 printf
#define FLUID_FLUSH()                fflush(stdout)
#endif

#define FLUID_LOG                    fluid_log

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif


#define FLUID_ASSERT(a,b)
#define FLUID_ASSERT_P(a,b)

char* fluid_error(void);


/* Internationalization */
#define _(s) s


#endif /* _FLUIDSYNTH_PRIV_H */
