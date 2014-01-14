/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#ifndef __ardour_audiofile_tagger_h__
#define __ardour_audiofile_tagger_h__

#include <string>

#include <taglib/tag.h>
#include <taglib/taglib.h>
#include <taglib/xiphcomment.h>

#include "ardour/libardour_visibility.h"

namespace ARDOUR
{

class SessionMetadata;

/// Class with static functions for tagging audiofiles
class LIBARDOUR_API AudiofileTagger
{
  public:

	/* Tags file with metadata, return true on success */

	static bool tag_file (std::string const & filename, SessionMetadata const & metadata);

  private:

	static bool tag_generic (TagLib::Tag & tag, SessionMetadata const & metadata);
	static bool tag_vorbis_comment (TagLib::Ogg::XiphComment & tag, SessionMetadata const & metadata);
};



} // namespace ARDOUR

#endif /* __ardour_audiofile_tagger_h__ */
