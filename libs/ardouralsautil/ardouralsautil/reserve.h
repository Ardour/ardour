/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: t -*-*/

#ifndef fooreservehfoo
#define fooreservehfoo

/***
  Copyright 2009 Lennart Poettering

  Permission is hereby granted, free of charge, to any person
  obtaining a copy of this software and associated documentation files
  (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify, merge,
  publish, distribute, sublicense, and/or sell copies of the Software,
  and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
***/

#include <dbus/dbus.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rd_device rd_device;

/* Prototype for a function that is called whenever someone else wants
 * your application to release the device it has locked. A return
 * value <= 0 denies the request, a positive return value agrees to
 * it. Before returning your application should close the device in
 * question completely to make sure the new application may access
 * it. */
typedef int (*rd_request_cb_t)(
	rd_device *d,
	int forced);                  /* Non-zero if an application forcibly took the lock away without asking. If this is the case then the return value of this call is ignored. */

/* Try to lock the device. Returns 0 on success, a negative errno
 * style return value on error. The DBus error might be set as well if
 * the error was caused D-Bus. */
int rd_acquire(
	rd_device **d,                /* On success a pointer to the newly allocated rd_device object will be filled in here */
	DBusConnection *connection,   /* Session bus (when D-Bus learns about user busses we should switch to user busses) */
	const char *device_name,      /* The device to lock, e.g. "Audio0" */
	const char *application_name, /* A human readable name of the application, e.g. "PulseAudio Sound Server" */
	int32_t priority,             /* The priority for this application. If unsure use 0 */
	rd_request_cb_t request_cb,   /* Will be called whenever someone requests that this device shall be released. May be NULL if priority is INT32_MAX */
	DBusError *error);            /* If we fail due to a D-Bus related issue the error will be filled in here. May be NULL. */

/* Unlock (if needed) and destroy an rd_device object again */
void rd_release(rd_device *d);

/* Set the application device name for an rd_device object. Returns 0
 * on success, a negative errno style return value on error. */
int rd_set_application_device_name(rd_device *d, const char *name);

/* Attach a userdata pointer to an rd_device */
void rd_set_userdata(rd_device *d, void *userdata);

/* Query the userdata pointer from an rd_device. Returns NULL if no
 * userdata was set. */
void* rd_get_userdata(rd_device *d);

/* Helper function to get the unique connection name owning a given
 * name. Returns 0 on success, a negative errno style return value on
 * error. */
int rd_dbus_get_name_owner(
	DBusConnection *connection,
	const char *name,
	char **name_owner,
	DBusError *error);

#ifdef __cplusplus
}
#endif

#endif
