/*
 * Copyright (C) 2008-2010 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
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

#include "ardour/audiofile_tagger.h"

#include "ardour/session_metadata.h"

#include "pbd/string_convert.h"

#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/oggfile.h>
#include <taglib/rifffile.h>
#include <taglib/wavfile.h>
#include <taglib/aifffile.h>
#include <taglib/tag.h>
#include <taglib/taglib.h>
#include <taglib/xiphcomment.h>

/* Convert string to TagLib::String */
#define TL_STR(string) TagLib::String ((string).c_str(), TagLib::String::UTF8)

using namespace PBD;

namespace ARDOUR
{
bool
AudiofileTagger::tag_file (std::string const& filename, SessionMetadata const& metadata)
{
	/* see also SessionMetadata::av_export_tag () for ffmpeg/liblame */

	TagLib::FileRef file (filename.c_str ());
	if (file.isNull ()) {
		std::cerr << "TagLib::FileRef is null for file '" << filename << "'" << std::endl;
		return true; // continue anyway?!
	}

	if (!file.tag ()) {
		std::cerr << "TagLib::Tag is null for file" << filename << std::endl;
		return true; // continue anyway?!
	}

	TagLib::Tag& tag (*file.tag ());

	tag_generic (tag, metadata);

	/* FLAC */
	TagLib::FLAC::File* flac_file;
	if ((flac_file = dynamic_cast<TagLib::FLAC::File*> (file.file ()))) {
		TagLib::Ogg::XiphComment* vorbis_tag;
		if ((vorbis_tag = dynamic_cast<TagLib::Ogg::XiphComment*> (flac_file->xiphComment (true)))) {
			tag_vorbis_comment (*vorbis_tag, metadata);
		} else {
			std::cerr << "Could not get Xiph comment for FLAC file!" << std::endl;
		}
	}

	/* Ogg */
	TagLib::Ogg::File* ogg_file;
	if ((ogg_file = dynamic_cast<TagLib::Ogg::File*> (file.file ()))) {
		TagLib::Ogg::XiphComment* vorbis_tag;
		if ((vorbis_tag = dynamic_cast<TagLib::Ogg::XiphComment*> (ogg_file->tag ()))) {
			tag_vorbis_comment (*vorbis_tag, metadata);
		} else {
			std::cerr << "Could not get Xiph comment for Ogg file!" << std::endl;
		}
	}

	TagLib::RIFF::WAV::File* wav_file;
	if ((wav_file = dynamic_cast<TagLib::RIFF::WAV::File*> (file.file ()))) {
		TagLib::RIFF::Info::Tag* info_tag = dynamic_cast<TagLib::RIFF::Info::Tag*> (wav_file->InfoTag ());
		assert (info_tag);
		tag_riff_info (*info_tag, metadata);
#if 1 // Also add id3v2 header to .wav
		TagLib::ID3v2::Tag* id3v2_tag = dynamic_cast<TagLib::ID3v2::Tag*> (wav_file->tag ());
		assert (id3v2_tag);
		tag_id3v2 (*id3v2_tag, metadata);
#endif
	}

	TagLib::RIFF::AIFF::File* aiff_file;
	if ((aiff_file = dynamic_cast<TagLib::RIFF::AIFF::File*> (file.file ()))) {
		TagLib::ID3v2::Tag* id3v2_tag = dynamic_cast<TagLib::ID3v2::Tag*> (aiff_file->tag ());
		assert (id3v2_tag);
		tag_id3v2 (*id3v2_tag, metadata);
	}

	file.save ();
	return true;
}

bool
AudiofileTagger::tag_generic (TagLib::Tag& tag, SessionMetadata const& metadata)
{
	tag.setTitle (TL_STR (metadata.title ()));
	tag.setArtist (TL_STR (metadata.artist ()));
	tag.setAlbum (TL_STR (metadata.album ()));
	tag.setComment (TL_STR (metadata.comment ()));
	tag.setGenre (TL_STR (metadata.genre ()));
	tag.setYear (metadata.year ());
	tag.setTrack (metadata.track_number ());

	return true;
}

bool
AudiofileTagger::tag_vorbis_comment (TagLib::Ogg::XiphComment& tag, SessionMetadata const& metadata)
{
	tag.addField ("COPYRIGHT", TL_STR (metadata.copyright ()));
	tag.addField ("ISRC", TL_STR (metadata.isrc ()));
	tag.addField ("GROUPING ", TL_STR (metadata.grouping ()));
	tag.addField ("SUBTITLE", TL_STR (metadata.subtitle ()));
	tag.addField ("ALBUMARTIST", TL_STR (metadata.album_artist ()));
	tag.addField ("LYRICIST", TL_STR (metadata.lyricist ()));
	tag.addField ("COMPOSER", TL_STR (metadata.composer ()));
	tag.addField ("CONDUCTOR", TL_STR (metadata.conductor ()));
	tag.addField ("REMIXER", TL_STR (metadata.remixer ()));
	tag.addField ("ARRANGER", TL_STR (metadata.arranger ()));
	tag.addField ("ENGINEER", TL_STR (metadata.engineer ()));
	tag.addField ("PRODUCER", TL_STR (metadata.producer ()));
	tag.addField ("DJMIXER", TL_STR (metadata.dj_mixer ()));
	tag.addField ("MIXER", TL_STR (metadata.mixer ()));
	tag.addField ("COMPILATION", TL_STR (metadata.compilation ()));
	tag.addField ("DISCSUBTITLE", TL_STR (metadata.disc_subtitle ()));
	tag.addField ("DISCNUMBER", to_string (metadata.disc_number ()));

	// No field for total discs or tracks

	return true;
}

bool
AudiofileTagger::tag_riff_info (TagLib::RIFF::Info::Tag& tag, SessionMetadata const& metadata)
{
	/* tag_generic() already takes care of all relevant fields:
	 * https://taglib.org/api/classTagLib_1_1RIFF_1_1Info_1_1Tag.html#pub-methods
	 */
	return tag_generic (tag, metadata);
}

bool
AudiofileTagger::tag_id3v2 (TagLib::ID3v2::Tag& tag, SessionMetadata const& metadata)
{
	/* TODO: consider adding custom frames
	 * https://taglib.org/api/classTagLib_1_1ID3v2_1_1Frame.html
	 */
	return tag_generic (tag, metadata);
}

} // namespace ARDOUR
