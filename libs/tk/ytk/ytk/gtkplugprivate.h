/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_PLUG_PRIVATE_H__
#define __GTK_PLUG_PRIVATE_H__

/* In gtkplug.c: */
void _gtk_plug_send_delete_event      (GtkWidget        *widget);
void _gtk_plug_add_all_grabbed_keys   (GtkPlug          *plug);
void _gtk_plug_focus_first_last       (GtkPlug          *plug,
				       GtkDirectionType  direction);
void _gtk_plug_handle_modality_on     (GtkPlug          *plug);
void _gtk_plug_handle_modality_off    (GtkPlug          *plug);

/* In backend-specific file: */

/*
 * _gtk_plug_windowing_get_id:
 *
 * @plug: a #GtkPlug
 *
 * Returns the native window system identifier for the plug's window.
 */
GdkNativeWindow _gtk_plug_windowing_get_id (GtkPlug *plug);

/*
 * _gtk_plug_windowing_realize_toplevel:
 *
 * @plug_window: a #GtkPlug's #GdkWindow
 *
 * Called from GtkPlug's realize method. Should tell the corresponding
 * socket that the plug has been realized.
 */
void _gtk_plug_windowing_realize_toplevel (GtkPlug *plug);

/*
 * _gtk_plug_windowing_map_toplevel:
 *
 * @plug: a #GtkPlug
 *
 * Called from GtkPlug's map method. Should tell the corresponding
 * #GtkSocket that the plug has been mapped.
 */
void _gtk_plug_windowing_map_toplevel (GtkPlug *plug);

/*
 * _gtk_plug_windowing_map_toplevel:
 *
 * @plug: a #GtkPlug
 *
 * Called from GtkPlug's unmap method. Should tell the corresponding
 * #GtkSocket that the plug has been unmapped.
 */
void _gtk_plug_windowing_unmap_toplevel (GtkPlug *plug);

/*
 * _gtk_plug_windowing_set_focus:
 *
 * @plug: a #GtkPlug
 *
 * Called from GtkPlug's set_focus method. Should tell the corresponding
 * #GtkSocket to request focus.
 */
void _gtk_plug_windowing_set_focus (GtkPlug *plug);

/*
 * _gtk_plug_windowing_add_grabbed_key:
 *
 * @plug: a #GtkPlug
 * @accelerator_key: a key
 * @accelerator_mods: modifiers for it
 *
 * Called from GtkPlug's keys_changed method. Should tell the
 * corresponding #GtkSocket to grab the key.
 */
void _gtk_plug_windowing_add_grabbed_key (GtkPlug         *plug,
					  guint            accelerator_key,
					  GdkModifierType  accelerator_mods);

/*
 * _gtk_plug_windowing_remove_grabbed_key:
 *
 * @plug: a #GtkPlug
 * @accelerator_key: a key
 * @accelerator_mods: modifiers for it
 *
 * Called from GtkPlug's keys_changed method. Should tell the
 * corresponding #GtkSocket to remove the key grab.
 */
void _gtk_plug_windowing_remove_grabbed_key (GtkPlug         *plug,
					     guint            accelerator_key,
					     GdkModifierType  accelerator_mods);

/*
 * _gtk_plug_windowing_focus_to_parent:
 *
 * @plug: a #GtkPlug
 * @direction: a direction
 *
 * Called from GtkPlug's focus method. Should tell the corresponding
 * #GtkSocket to move the focus.
 */
void _gtk_plug_windowing_focus_to_parent (GtkPlug         *plug,
					  GtkDirectionType direction);

/*
 * _gtk_plug_windowing_filter_func:
 *
 * @gdk_xevent: a windowing system native event
 * @event: a pre-allocated empty GdkEvent
 * @data: the #GtkPlug
 *
 * Event filter function installed on plug windows.
 */
GdkFilterReturn _gtk_plug_windowing_filter_func (GdkXEvent *gdk_xevent,
						 GdkEvent  *event,
						 gpointer   data);

#endif /* __GTK_PLUG_PRIVATE_H__ */
