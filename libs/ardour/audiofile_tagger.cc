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

#include "ardour/audiofile_tagger.h"

#include "ardour/session_metadata.h"

#include "pbd/convert.h"

#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/oggfile.h>
#include <taglib/tag.h>
#include <taglib/taglib.h>
#include <taglib/xiphcomment.h>

/* Convert string to TagLib::String */
#define TL_STR(string) TagLib::String ((string).c_str(), TagLib::String::UTF8)

using namespace PBD;

namespace ARDOUR
{

bool
AudiofileTagger::tag_file (std::string const & filename, SessionMetadata const & metadata)
{
	TagLib::FileRef file (filename.c_str());
	TagLib::Tag & tag (*file.tag());

	tag_generic (tag, metadata);

	/* FLAC */

	TagLib::FLAC::File * flac_file;
	if ((flac_file = dynamic_cast<TagLib::FLAC::File *> (file.file()))) {
		TagLib::Ogg::XiphComment * vorbis_tag;
		if ((vorbis_tag = dynamic_cast<TagLib::Ogg::XiphComment *> (flac_file->xiphComment (true)))) {
			tag_vorbis_comment (*vorbis_tag, metadata);
		} else {
			std::cerr << "Could not get Xiph comment for FLAC file!" << std::endl;
		}
	}

	/* Ogg */

	TagLib::Ogg::File * ogg_file;
	if ((ogg_file = dynamic_cast<TagLib::Ogg::File *> (file.file()))) {
		TagLib::Ogg::XiphComment * vorbis_tag;
		if ((vorbis_tag = dynamic_cast<TagLib::Ogg::XiphComment *> (ogg_file->tag()))) {
			tag_vorbis_comment (*vorbis_tag, metadata);
		} else {
			std::cerr << "Could not get Xiph comment for Ogg file!" << std::endl;
		}
	}

	file.save();
	return true;
}

bool
AudiofileTagger::tag_generic (TagLib::Tag & tag, SessionMetadata const & metadata)
{
	tag.setTitle (TL_STR(metadata.title()));
	tag.setArtist (TL_STR(metadata.artist()));
	tag.setAlbum (TL_STR(metadata.album()));
	tag.setComment (TL_STR(metadata.comment()));
	tag.setGenre (TL_STR(metadata.genre()));
	tag.setYear (metadata.year());
	tag.setTrack (metadata.track_number());

	return true;
}

bool
AudiofileTagger::tag_vorbis_comment (TagLib::Ogg::XiphComment & tag, SessionMetadata const & metadata)
{
	tag.addField ("COPYRIGHT", TL_STR(metadata.copyright()));
	tag.addField ("ISRC", TL_STR(metadata.isrc()));
	tag.addField ("GROUPING ", TL_STR(metadata.grouping()));
	tag.addField ("SUBTITLE", TL_STR(metadata.subtitle()));
	tag.addField ("ALBUMARTIST", TL_STR(metadata.album_artist()));
	tag.addField ("LYRICIST", TL_STR(metadata.lyricist()));
	tag.addField ("COMPOSER", TL_STR(metadata.composer()));
	tag.addField ("CONDUCTOR", TL_STR(metadata.conductor()));
	tag.addField ("REMIXER", TL_STR(metadata.remixer()));
	tag.addField ("ARRANGER", TL_STR(metadata.arranger()));
	tag.addField ("ENGINEER", TL_STR(metadata.engineer()));
	tag.addField ("PRODUCER", TL_STR(metadata.producer()));
	tag.addField ("DJMIXER", TL_STR(metadata.dj_mixer()));
	tag.addField ("MIXER", TL_STR(metadata.mixer()));
	tag.addField ("COMPILATION", TL_STR(metadata.compilation()));
	tag.addField ("DISCSUBTITLE", TL_STR(metadata.disc_subtitle()));
	tag.addField ("DISCNUMBER", to_string (metadata.disc_number(), std::dec));

	// No field for total discs or tracks

	return true;
}


} // namespace ARDOUR

