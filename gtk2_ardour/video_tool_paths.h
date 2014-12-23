/*
    Copyright (C) 2010-2013 Paul Davis
    Author: Robin Gareus <robin@gareus.org>

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

#ifndef __gtk_ardour_video_tool_paths_h__
#define __gtk_ardour_video_tool_paths_h__

namespace ArdourVideoToolPaths {

	bool harvid_exe (std::string &harvid_exe);
	bool xjadeo_exe (std::string &xjadeo_exe);
	bool transcoder_exe (std::string &ffmpeg_exe, std::string &ffprobe_exe);

};

#endif /* __gtk_ardour_video_tool_paths_h__ */
