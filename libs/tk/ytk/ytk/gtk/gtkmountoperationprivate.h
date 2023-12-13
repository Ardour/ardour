/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) David Zeuthen <davidz@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_MOUNT_OPERATION_PRIVATE_H__
#define __GTK_MOUNT_OPERATION_PRIVATE_H__

#include <gio/gio.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

struct _GtkMountOperationLookupContext;
typedef struct _GtkMountOperationLookupContext GtkMountOperationLookupContext;

GtkMountOperationLookupContext *_gtk_mount_operation_lookup_context_get  (GdkDisplay *display);

gboolean _gtk_mount_operation_lookup_info         (GtkMountOperationLookupContext *context,
                                                   GPid                            pid,
                                                   gint                            size_pixels,
                                                   gchar                         **out_name,
                                                   gchar                         **out_command_line,
                                                   GdkPixbuf                     **out_pixbuf);

void     _gtk_mount_operation_lookup_context_free (GtkMountOperationLookupContext *context);

/* throw G_IO_ERROR_FAILED_HANDLED if a helper already reported the error to the user */
gboolean _gtk_mount_operation_kill_process (GPid      pid,
                                            GError  **error);

#endif /* __GTK_MOUNT_OPERATION_PRIVATE_H__ */
