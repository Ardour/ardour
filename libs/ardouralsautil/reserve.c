/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: t -*-*/

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

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "ardouralsautil/reserve.h"

#ifndef DBUS_TIMEOUT_USE_DEFAULT
#define DBUS_TIMEOUT_USE_DEFAULT (-1)
#endif

struct rd_device {
	int ref;

	char *device_name;
	char *application_name;
	char *application_device_name;
	char *service_name;
	char *object_path;
	int32_t priority;

	DBusConnection *connection;

	unsigned owning:1;
	unsigned registered:1;
	unsigned filtering:1;
	unsigned gave_up:1;

	rd_request_cb_t request_cb;
	void *userdata;
};

#define SERVICE_PREFIX "org.freedesktop.ReserveDevice1."
#define OBJECT_PREFIX "/org/freedesktop/ReserveDevice1/"

static const char introspection[] =
	DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE
	"<node>"
	" <!-- If you are looking for documentation make sure to check out\n"
	"      http://git.0pointer.de/?p=reserve.git;a=blob;f=reserve.txt -->\n"
	" <interface name=\"org.freedesktop.ReserveDevice1\">"
	"  <method name=\"RequestRelease\">"
	"   <arg name=\"priority\" type=\"i\" direction=\"in\"/>"
	"   <arg name=\"result\" type=\"b\" direction=\"out\"/>"
	"  </method>"
	"  <property name=\"Priority\" type=\"i\" access=\"read\"/>"
	"  <property name=\"ApplicationName\" type=\"s\" access=\"read\"/>"
	"  <property name=\"ApplicationDeviceName\" type=\"s\" access=\"read\"/>"
	" </interface>"
	" <interface name=\"org.freedesktop.DBus.Properties\">"
	"  <method name=\"Get\">"
	"   <arg name=\"interface\" direction=\"in\" type=\"s\"/>"
	"   <arg name=\"property\" direction=\"in\" type=\"s\"/>"
	"   <arg name=\"value\" direction=\"out\" type=\"v\"/>"
	"  </method>"
	" </interface>"
	" <interface name=\"org.freedesktop.DBus.Introspectable\">"
	"  <method name=\"Introspect\">"
	"   <arg name=\"data\" type=\"s\" direction=\"out\"/>"
	"  </method>"
	" </interface>"
	"</node>";

static dbus_bool_t add_variant(
	DBusMessage *m,
	int type,
	const void *data) {

	DBusMessageIter iter, sub;
	char t[2];

	t[0] = (char) type;
	t[1] = 0;

	dbus_message_iter_init_append(m, &iter);

	if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, t, &sub))
		return FALSE;

	if (!dbus_message_iter_append_basic(&sub, type, data))
		return FALSE;

	if (!dbus_message_iter_close_container(&iter, &sub))
		return FALSE;

	return TRUE;
}

static DBusHandlerResult object_handler(
	DBusConnection *c,
	DBusMessage *m,
	void *userdata) {

	rd_device *d;
	DBusError error;
	DBusMessage *reply = NULL;

	dbus_error_init(&error);

	d = userdata;
	assert(d->ref >= 1);

	if (dbus_message_is_method_call(
		    m,
		    "org.freedesktop.ReserveDevice1",
		    "RequestRelease")) {

		int32_t priority;
		dbus_bool_t ret;

		if (!dbus_message_get_args(
			    m,
			    &error,
			    DBUS_TYPE_INT32, &priority,
			    DBUS_TYPE_INVALID))
			goto invalid;

		ret = FALSE;

		if (priority > d->priority && d->request_cb) {
			d->ref++;

			if (d->request_cb(d, 0) > 0) {
				ret = TRUE;
				d->gave_up = 1;
			}

			rd_release(d);
		}

		if (!(reply = dbus_message_new_method_return(m)))
			goto oom;

		if (!dbus_message_append_args(
			    reply,
			    DBUS_TYPE_BOOLEAN, &ret,
			    DBUS_TYPE_INVALID))
			goto oom;

		if (!dbus_connection_send(c, reply, NULL))
			goto oom;

		dbus_message_unref(reply);

		return DBUS_HANDLER_RESULT_HANDLED;

	} else if (dbus_message_is_method_call(
			   m,
			   "org.freedesktop.DBus.Properties",
			   "Get")) {

		const char *interface, *property;

		if (!dbus_message_get_args(
			    m,
			    &error,
			    DBUS_TYPE_STRING, &interface,
			    DBUS_TYPE_STRING, &property,
			    DBUS_TYPE_INVALID))
			goto invalid;

		if (strcmp(interface, "org.freedesktop.ReserveDevice1") == 0) {
			const char *empty = "";

			if (strcmp(property, "ApplicationName") == 0 && d->application_name) {
				if (!(reply = dbus_message_new_method_return(m)))
					goto oom;

				if (!add_variant(
					    reply,
					    DBUS_TYPE_STRING,
					    d->application_name ? (const char * const *) &d->application_name : &empty))
					goto oom;

			} else if (strcmp(property, "ApplicationDeviceName") == 0) {
				if (!(reply = dbus_message_new_method_return(m)))
					goto oom;

				if (!add_variant(
					    reply,
					    DBUS_TYPE_STRING,
					    d->application_device_name ? (const char * const *) &d->application_device_name : &empty))
					goto oom;

			} else if (strcmp(property, "Priority") == 0) {
				if (!(reply = dbus_message_new_method_return(m)))
					goto oom;

				if (!add_variant(
					    reply,
					    DBUS_TYPE_INT32,
					    &d->priority))
					goto oom;
			} else {
				if (!(reply = dbus_message_new_error_printf(
					      m,
					      DBUS_ERROR_UNKNOWN_METHOD,
					      "Unknown property %s",
					      property)))
					goto oom;
			}

			if (!dbus_connection_send(c, reply, NULL))
				goto oom;

			dbus_message_unref(reply);

			return DBUS_HANDLER_RESULT_HANDLED;
		}

	} else if (dbus_message_is_method_call(
			   m,
			   "org.freedesktop.DBus.Introspectable",
			   "Introspect")) {
			    const char *i = introspection;

		if (!(reply = dbus_message_new_method_return(m)))
			goto oom;

		if (!dbus_message_append_args(
			    reply,
			    DBUS_TYPE_STRING,
			    &i,
			    DBUS_TYPE_INVALID))
			goto oom;

		if (!dbus_connection_send(c, reply, NULL))
			goto oom;

		dbus_message_unref(reply);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

invalid:
	if (reply)
		dbus_message_unref(reply);

	if (!(reply = dbus_message_new_error(
		      m,
		      DBUS_ERROR_INVALID_ARGS,
		      "Invalid arguments")))
		goto oom;

	if (!dbus_connection_send(c, reply, NULL))
		goto oom;

	dbus_message_unref(reply);

	dbus_error_free(&error);

	return DBUS_HANDLER_RESULT_HANDLED;

oom:
	if (reply)
		dbus_message_unref(reply);

	dbus_error_free(&error);

	return DBUS_HANDLER_RESULT_NEED_MEMORY;
}

static DBusHandlerResult filter_handler(
	DBusConnection *c,
	DBusMessage *m,
	void *userdata) {

	rd_device *d;
	DBusError error;
	char *name_owner = NULL;

	dbus_error_init(&error);

	d = userdata;
	assert(d->ref >= 1);

	if (dbus_message_is_signal(m, "org.freedesktop.DBus", "NameLost")) {
		const char *name;

		if (!dbus_message_get_args(
			    m,
			    &error,
			    DBUS_TYPE_STRING, &name,
			    DBUS_TYPE_INVALID))
			goto invalid;

		if (strcmp(name, d->service_name) == 0 && d->owning) {
			/* Verify the actual owner of the name to avoid leaked NameLost
			 * signals from previous reservations. The D-Bus daemon will send
			 * all messages asynchronously in the correct order, but we could
			 * potentially process them too late due to the pseudo-blocking
			 * call mechanism used during both acquisition and release. This
			 * can happen if we release the device and immediately after
			 * reacquire it before NameLost is processed. */
			if (!d->gave_up) {
				const char *un;

				if ((un = dbus_bus_get_unique_name(c)) && rd_dbus_get_name_owner(c, d->service_name, &name_owner, &error) == 0)
					if (name_owner && strcmp(name_owner, un) == 0)
						goto invalid; /* Name still owned by us */
			}

			d->owning = 0;

			if (!d->gave_up)  {
				d->ref++;

				if (d->request_cb)
					d->request_cb(d, 1);
				d->gave_up = 1;

				rd_release(d);
			}

		}
	}

invalid:
	free(name_owner);
	dbus_error_free(&error);

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


static const struct DBusObjectPathVTable vtable ={
	.message_function = object_handler
};

int rd_acquire(
	rd_device **_d,
	DBusConnection *connection,
	const char *device_name,
	const char *application_name,
	int32_t priority,
	rd_request_cb_t request_cb,
	DBusError *error) {

	rd_device *d = NULL;
	int r, k;
	DBusError _error;
	DBusMessage *m = NULL, *reply = NULL;
	dbus_bool_t good;

	if (!error)
		error = &_error;

	dbus_error_init(error);

	if (!_d)
		return -EINVAL;

	if (!connection)
		return -EINVAL;

	if (!device_name)
		return -EINVAL;

	if (!request_cb && priority != INT32_MAX)
		return -EINVAL;

	if (!(d = calloc(sizeof(rd_device), 1)))
		return -ENOMEM;

	d->ref = 1;

	if (!(d->device_name = strdup(device_name))) {
		r = -ENOMEM;
		goto fail;
	}

	if (!(d->application_name = strdup(application_name))) {
		r = -ENOMEM;
		goto fail;
	}

	d->priority = priority;
	d->connection = dbus_connection_ref(connection);
	d->request_cb = request_cb;

	if (!(d->service_name = malloc(sizeof(SERVICE_PREFIX) + strlen(device_name)))) {
		r = -ENOMEM;
		goto fail;
	}
	sprintf(d->service_name, SERVICE_PREFIX "%s", d->device_name);

	if (!(d->object_path = malloc(sizeof(OBJECT_PREFIX) + strlen(device_name)))) {
		r = -ENOMEM;
		goto fail;
	}
	sprintf(d->object_path, OBJECT_PREFIX "%s", d->device_name);

	if ((k = dbus_bus_request_name(
		     d->connection,
		     d->service_name,
		     DBUS_NAME_FLAG_DO_NOT_QUEUE|
		     (priority < INT32_MAX ? DBUS_NAME_FLAG_ALLOW_REPLACEMENT : 0),
		     error)) < 0) {
		r = -EIO;
		goto fail;
	}

	if (k == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
		goto success;

	if (k != DBUS_REQUEST_NAME_REPLY_EXISTS) {
		r = -EIO;
		goto fail;
	}

	if (priority <= INT32_MIN) {
		r = -EBUSY;
		goto fail;
	}

	if (!(m = dbus_message_new_method_call(
		      d->service_name,
		      d->object_path,
		      "org.freedesktop.ReserveDevice1",
		      "RequestRelease"))) {
		r = -ENOMEM;
		goto fail;
	}

	if (!dbus_message_append_args(
		    m,
		    DBUS_TYPE_INT32, &d->priority,
		    DBUS_TYPE_INVALID)) {
		r = -ENOMEM;
		goto fail;
	}

	if (!(reply = dbus_connection_send_with_reply_and_block(
		      d->connection,
		      m,
		      5000, /* 5s */
		      error))) {

		if (dbus_error_has_name(error, DBUS_ERROR_TIMED_OUT) ||
		    dbus_error_has_name(error, DBUS_ERROR_UNKNOWN_METHOD) ||
		    dbus_error_has_name(error, DBUS_ERROR_NO_REPLY)) {
			/* This must be treated as denied. */
			r = -EBUSY;
			goto fail;
		}

		r = -EIO;
		goto fail;
	}

	if (!dbus_message_get_args(
		    reply,
		    error,
		    DBUS_TYPE_BOOLEAN, &good,
		    DBUS_TYPE_INVALID)) {
		r = -EIO;
		goto fail;
	}

	if (!good) {
		r = -EBUSY;
		goto fail;
	}

	if ((k = dbus_bus_request_name(
		     d->connection,
		     d->service_name,
		     DBUS_NAME_FLAG_DO_NOT_QUEUE|
		     (priority < INT32_MAX ? DBUS_NAME_FLAG_ALLOW_REPLACEMENT : 0)|
		     DBUS_NAME_FLAG_REPLACE_EXISTING,
		     error)) < 0) {
		r = -EIO;
		goto fail;
	}

	if (k != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		r = -EIO;
		goto fail;
	}

success:
	d->owning = 1;

	if (!(dbus_connection_register_object_path(
		      d->connection,
		      d->object_path,
		      &vtable,
		      d))) {
		r = -ENOMEM;
		goto fail;
	}

	d->registered = 1;

	if (!dbus_connection_add_filter(
		    d->connection,
		    filter_handler,
		    d,
		    NULL)) {
		r = -ENOMEM;
		goto fail;
	}

	d->filtering = 1;

	*_d = d;
	return 0;

fail:
	if (m)
		dbus_message_unref(m);

	if (reply)
		dbus_message_unref(reply);

	if (&_error == error)
		dbus_error_free(&_error);

	if (d)
		rd_release(d);

	return r;
}

void rd_release(
	rd_device *d) {

	if (!d)
		return;

	assert(d->ref > 0);

	if (--d->ref > 0)
		return;


	if (d->filtering)
		dbus_connection_remove_filter(
			d->connection,
			filter_handler,
			d);

	if (d->registered)
		dbus_connection_unregister_object_path(
			d->connection,
			d->object_path);

	if (d->owning)
		dbus_bus_release_name(
			d->connection,
			d->service_name,
			NULL);

	free(d->device_name);
	free(d->application_name);
	free(d->application_device_name);
	free(d->service_name);
	free(d->object_path);

	if (d->connection)
		dbus_connection_unref(d->connection);

	free(d);
}

int rd_set_application_device_name(rd_device *d, const char *n) {
	char *t;

	if (!d)
		return -EINVAL;

	assert(d->ref > 0);

	if (!(t = strdup(n)))
		return -ENOMEM;

	free(d->application_device_name);
	d->application_device_name = t;
	return 0;
}

void rd_set_userdata(rd_device *d, void *userdata) {

	if (!d)
		return;

	assert(d->ref > 0);
	d->userdata = userdata;
}

void* rd_get_userdata(rd_device *d) {

	if (!d)
		return NULL;

	assert(d->ref > 0);

	return d->userdata;
}

int rd_dbus_get_name_owner(
	DBusConnection *connection,
	const char *name,
	char **name_owner,
	DBusError *error) {

	DBusMessage *msg, *reply;
	int r;

	*name_owner = NULL;

	if (!(msg = dbus_message_new_method_call(DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS, "GetNameOwner"))) {
		r = -ENOMEM;
		goto fail;
	}

	if (!dbus_message_append_args(msg, DBUS_TYPE_STRING, &name, DBUS_TYPE_INVALID)) {
		r = -ENOMEM;
		goto fail;
	}

	reply = dbus_connection_send_with_reply_and_block(connection, msg, DBUS_TIMEOUT_USE_DEFAULT, error);
	dbus_message_unref(msg);
	msg = NULL;

	if (reply) {
		if (!dbus_message_get_args(reply, error, DBUS_TYPE_STRING, name_owner, DBUS_TYPE_INVALID)) {
			dbus_message_unref(reply);
			r = -EIO;
			goto fail;
		}

		*name_owner = strdup(*name_owner);
		dbus_message_unref(reply);

		if (!*name_owner) {
			r = -ENOMEM;
			goto fail;
		}

	} else if (dbus_error_has_name(error, "org.freedesktop.DBus.Error.NameHasNoOwner"))
		dbus_error_free(error);
	else {
		r = -EIO;
		goto fail;
	}

	return 0;

fail:
	if (msg)
		dbus_message_unref(msg);

	return r;
}
