/*
    Copyright (C) 2014 Waves Audio Ltd.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef Tracks_OpenFileDialog_h
#define Tracks_OpenFileDialog_h

#include <string>

#include "i18n.h"

namespace ARDOUR
{
        std::string save_file_dialog (std::string initial_path = "", std::string title = _("Save"));
        std::string open_file_dialog (std::string initial_path = "", std::string title = _("Open"));
        std::vector<std::string> open_file_dialog (std::vector<std::string> extensions, std::string initial_path = "", std::string title = _("Open"));
        std::string choose_folder_dialog (std::string initial_path = "", std::string title = _("Choose Folder"));
}

#endif /* Tracks_OpenFileDialog_h */
