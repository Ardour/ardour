/*
 * Copyright (C) 2026 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/session.h"
#include "ardour/vca_folder.h"

using namespace ARDOUR;

void
Session::add_folders (FolderList& new_folders)
{
	{
		PBD::RWLock::WriterLock wl (_folder_lock);
		_folders.insert (_folders.end(), new_folders.begin(), new_folders.end());
	}

	FolderAdded (new_folders); /* EMIT SIGNAL */
}

void
Session::folderize (std::shared_ptr<VCA> vca)
{
	assert (vca);
	std::shared_ptr<VCAFolder> f (new VCAFolder (vca, *this, vca->name(), PresentationInfo::Flag (0)));
	FolderList fl ({f});
	add_folders (fl);
}

void
Session::unfolderize (std::shared_ptr<Folder> folder)
{
}
