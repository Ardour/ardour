// -*- c++ -*-
#ifndef _GTKMM_ACCELMAP_H
#define _GTKMM_ACCELMAP_H

/* $Id$ */

/* accelmap.h
 *
 * Copyright (C) 2002 The Gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include <string>

#include <gdkmm/types.h>
#include <gtkmm/accelkey.h>

namespace Gtk
{

namespace AccelMap
{

//TODO: Why is the accel_path a std::string, instead of a Glib::ustring? murrayc.

/** Registers a new accelerator with the global accelerator map.
 * This function should only be called once per accel_path
 * with the canonical accel_key and accel_mods for this path.
 * To change the accelerator during runtime programatically, use
 * change_entry().
 * The accelerator path must consist of "<WINDOWTYPE>/Category1/Category2/.../Action",
 * where <WINDOWTYPE> should be a unique application-specific identifier, that
 * corresponds to the kind of window the accelerator is being used in, e.g. "Gimp-Image",
 * "Abiword-Document" or "Gnumeric-Settings".
 * The Category1/.../Action portion is most appropriately chosen by the action the
 * accelerator triggers, i.e. for accelerators on menu items, choose the item's menu path,
 * e.g. "File/Save As", "Image/View/Zoom" or "Edit/Select All".
 * So a full valid accelerator path may look like:
 * "<Gimp-Toolbox>/File/Dialogs/Tool Options...".
 *
 * @param accel_path valid accelerator path
 * @param accel_key the accelerator key
 * @param accel_mods the accelerator modifiers
 *
 */
void add_entry(const std::string& accel_path, 
               guint accel_key, 
               Gdk::ModifierType accel_mods);

/** Changes the accel_key and accel_mods currently associated with accel_path.
 * Due to conflicts with other accelerators, a change may not always be possible,
 * replace indicates whether other accelerators may be deleted to resolve such
 * conflicts. A change will only occur if all conflicts could be resolved (which
 * might not be the case if conflicting accelerators are locked). Successful
 * changes are indicated by a true return value.
 *
 * @param accel_path  a valid accelerator path
 * @param accel_key   the new accelerator key
 * @param accel_mods  the new accelerator modifiers
 * @param replace     true if other accelerators may be deleted upon conflicts
 * @result     true if the accelerator could be changed, false otherwise
 */               
bool change_entry(const std::string& accel_path, 
                  guint accel_key, 
                  Gdk::ModifierType accel_mods,
                  bool replace);

/** Parses a file previously saved with save() for
 * accelerator specifications, and propagates them accordingly.
 *
 * @param filename a file containing accelerator specifications
 */                  
void load(const std::string& filename);

/** Saves current accelerator specifications (accelerator path, key
 * and modifiers) to filename.
 * The file is written in a format suitable to be read back in by
 * load().
 *
 * @param filename the file to contain accelerator specifications
 */
void save(const std::string& filename);

/** Locks the given accelerator path.
 *
 * Locking an accelerator path prevents its accelerator from being changed
 * during runtime. A locked accelerator path can be unlocked by
 * unlock_path(). Refer to change_entry()
 * about runtime accelerator changes.
 *
 * Note that locking of individual accelerator paths is independent from
 * locking the #GtkAccelGroup containing them. For runtime accelerator
 * changes to be possible both the accelerator path and its AccelGroup
 * have to be unlocked.
 *
 * @param accel_path a valid accelerator path
 *
 * @newin{2,4}
 */
void lock_path(const std::string& accel_path);

/** Unlocks the given accelerator path. Refer to gtk_accel_map_lock_path()
 * about accelerator path locking.
 *
 * @param accel_path a valid accelerator path
 *
 * @newin{2,4}
 */
void unlock_path(const std::string& accel_path);

/** Looks up the accelerator entry for accel_path.
 * @param accel_path A valid accelerator path.
 * @result true if accel_path is known, false otherwise
 *
 * @newin{2,10}
 */
bool lookup_entry (const Glib::ustring& accel_path);

/** Looks up the accelerator entry for accel_path and fills in key.
 * @param accel_path A valid accelerator path.
 * @param key The accelerator key to be filled in.
 * @result true if accel_path is known, false otherwise
 *
 * @newin{2,10}
 */
bool lookup_entry (const Glib::ustring& accel_path, Gtk::AccelKey& key);

} // namespace AccelMap

} // namespace Gtk


#endif /* _GTKMM_ACCELMAP_H */

