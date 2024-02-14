/*
 * Copyright (C) 2017-2023 Adrien Gesta-Fline
 *
 * This file is part of libAAF.
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

#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "aaf/AAFIAudioFiles.h"
#include "aaf/AAFIface.h"
#include "aaf/debug.h"

#include "aaf/RIFFParser.h"
#include "aaf/URIParser.h"

#include "aaf/utils.h"

#if defined(__linux__)
#include <linux/limits.h>
#include <arpa/inet.h>
#include <mntent.h>
#include <unistd.h> /* access() */
#elif defined(__APPLE__)
#include <sys/syslimits.h>
#include <unistd.h> /* access() */
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#include <limits.h>
#define R_OK 4 /* Test for read permission.  */
#define W_OK 2 /* Test for write permission.  */
#define F_OK 0 /* Test for existence.  */
#ifndef _MSC_VER
#include <unistd.h> // access()
#endif
#endif

#define WAV_FILE_EXT "wav"
#define AIFF_FILE_EXT "aif"

#define debug(...) \
	_dbg (aafi->dbg, aafi, DEBUG_SRC_ID_AAF_IFACE, VERB_DEBUG, __VA_ARGS__)

#define warning(...) \
	_dbg (aafi->dbg, aafi, DEBUG_SRC_ID_AAF_IFACE, VERB_WARNING, __VA_ARGS__)

#define error(...) \
	_dbg (aafi->dbg, aafi, DEBUG_SRC_ID_AAF_IFACE, VERB_ERROR, __VA_ARGS__)

static size_t
embeddedAudioDataReaderCallback (unsigned char* buf, size_t offset, size_t reqLen, void* user1, void* user2, void* user3);
static size_t
externalAudioDataReaderCallback (unsigned char* buf, size_t offset, size_t reqLen, void* user1, void* user2, void* user3);

wchar_t*
aafi_locate_external_essence_file (AAF_Iface* aafi, const wchar_t* original_uri_filepath, const char* search_location)
{
	/*
	 * Absolute Uniform Resource Locator (URL) complying with RFC 1738 or relative
	 * Uniform Resource Identifier (URI) complying with RFC 2396 for file containing
	 * the essence. If it is a relative URI, the base URI is determined from the URI
	 * of the AAF file itself.
	 *
	 * Informative note: A valid URL or URI uses a constrained character set and
	 * uses the / character as the path separator.
	 */

	char*    uri_filepath   = NULL;
	char*    local_filepath = NULL;
	char*    aaf_path       = NULL;
	char*    foundpath      = NULL;
	wchar_t* retpath        = NULL;

	struct uri* uri = NULL;

	if (original_uri_filepath == NULL) {
		error ("Cant locate a NULL filepath");
		goto err;
	}

	uri_filepath = laaf_util_wstr2str (original_uri_filepath);

	if (uri_filepath == NULL) {
		error ("Could not convert original_uri_filepath from wstr to str : %ls", original_uri_filepath);
		goto err;
	}

	// debug( "Original URI : %s", uri_filepath );

	uri = uriParse (uri_filepath, URI_OPT_DECODE_ALL, aafi->dbg);

	if (uri == NULL) {
		error ("Could not parse URI");
		goto err;
	}

	if (uri->path == NULL) {
		error ("Could not retrieve <path> out of URI");
		goto err;
	}

	// debug( "Decoded URI's path : %s", uri->path );

	/* extract relative path to essence file : "<firstparent>/<essence.file>" */

	char* relativeEssencePath = NULL;
	char* p                   = uri->path + strlen (uri->path);

	int sepcount = 0;

	while (p > uri->path) {
		if (*p == '/') { /* parsing URI, so will always be '/' as separator character */
			sepcount++;
			if (sepcount == 2) {
				relativeEssencePath = (p + 1);
				break;
			}
		}
		p--;
	}

	const char* essenceFileName = laaf_util_fop_get_file (uri->path);

	// debug( "Essence filename : %s", essenceFileName );

	if (search_location) {
		/*
		 * "<search_location>/<essence.file>"
		 */

		local_filepath = laaf_util_build_path (DIR_SEP_STR, search_location, essenceFileName, NULL);

		if (local_filepath == NULL) {
			error ("Could not build search filepath");
			goto err;
		}

		// debug( "Search filepath : %s", local_filepath );

		if (access (local_filepath, F_OK) != -1) {
			// debug( "FOUND: %s", local_filepath );
			foundpath = local_filepath;
			goto found;
		}

		free (local_filepath);
		local_filepath = NULL;

		/*
		 * "<search_location>/<firstparentInOriginalEssencePath>/<essence.file>"
		 */

		local_filepath = laaf_util_build_path (DIR_SEP_STR, search_location, relativeEssencePath, NULL);

		if (local_filepath == NULL) {
			error ("Could not build search filepath");
			goto err;
		}

		// debug( "Search filepath : %s", local_filepath );

		if (access (local_filepath, F_OK) != -1) {
			// debug( "FOUND: %s", local_filepath );
			foundpath = local_filepath;
			goto found;
		}

		free (local_filepath);
		local_filepath = NULL;
	}

	/* Try AAF essence's URI */

	if (access (uri_filepath, F_OK) != -1) {
		// debug( "FOUND: %s", uri_filepath );
		foundpath = uri_filepath;
		goto found;
	}

	/* Try <path> part of URI */

	if (access (uri->path, F_OK) != -1) {
		// debug( "FOUND: %s", uri->path );
		foundpath = uri->path;
		goto found;
	}

	if (uri->flags & URI_T_LOCALHOST) {
		// debug( "URI targets localhost : %s", uri_filepath );
	} else {
		if (uri->flags & URI_T_HOST_IPV4) {
			// debug( "URI targets IPV4 : %s", uri_filepath );
		} else if (uri->flags & URI_T_HOST_IPV6) {
			// debug( "URI targets IPV6 : %s", uri_filepath );
		} else if (uri->flags & URI_T_HOST_REGNAME) {
			// debug( "URI targets hostname : %s", uri_filepath );
		}
	}

	/*
	 * Try to locate essence file from the AAF file location.
	 *
	 * e.g.
	 *    - AAF filepath : /home/user/AAFFile.aaf
	 *    - Essence URI  : file://localhost/C:/Users/user/Desktop/AudioFiles/essence.wav
	 *    = /home/user/AudioFiles/essence.file
	 */

	/* extract path to AAF file */

	aaf_path = laaf_util_c99strdup (aafi->aafd->cfbd->file);

	if (aaf_path == NULL) {
		error ("Could not duplicate AAF filepath");
		goto err;
	}

	p = aaf_path + strlen (aaf_path);

	while (p > aaf_path) {
		if (IS_DIR_SEP (*p)) {
			*p = 0x00;
			break;
		}
		p--;
	}

	/*
	 * "<localPathToAAFfile>/<essence.file>"
	 */

	local_filepath = laaf_util_build_path (DIR_SEP_STR, aaf_path, essenceFileName, NULL);

	if (local_filepath == NULL) {
		error ("Could not build filepath");
		goto err;
	}

	// debug( "AAF relative filepath : %s", local_filepath );

	if (access (local_filepath, F_OK) != -1) {
		// debug( "FOUND: %s", filepath );
		foundpath = local_filepath;
		goto found;
	}

	free (local_filepath);
	local_filepath = NULL;

	/*
	 * "<localPathToAAFfile>/<firstparentInOriginalEssencePath>/<essence.file>"
	 */

	local_filepath = laaf_util_build_path (DIR_SEP_STR, aaf_path, relativeEssencePath, NULL);

	if (local_filepath == NULL) {
		error ("Could not build filepath");
		goto err;
	}

	// debug( "AAF relative filepath : %s", local_filepath );

	if (access (local_filepath, F_OK) != -1) {
		// debug( "FOUND: %s", filepath );
		foundpath = local_filepath;
		goto found;
	}

	free (local_filepath);
	local_filepath = NULL;

	// debug("File not found");

found:
	if (foundpath) {
		retpath = laaf_util_str2wstr (foundpath);

		if (retpath == NULL) {
			error ("Could not convert foundpath from str to wstr : %s", foundpath);
			goto err;
		}
	}

	goto end;

err:
	retpath = NULL;

end:
	if (uri)
		uriFree (uri);

	if (uri_filepath)
		free (uri_filepath);

	if (local_filepath)
		free (local_filepath);

	if (aaf_path)
		free (aaf_path);

	return retpath;

	/*
		* AAFInfo --aaf-clips ../libaaf_testfiles/fonk_2.AAF
		file://localhost/Users/horlaprod/Music/Logic/fonk_2/Audio Files_1/fonk_2_3#04.wav

		* AAFInfo --aaf-clips ../libaaf_testfiles/ADP/ADP3_51-ST-MONO-NOBREAKOUT.aaf
		file:///C:/Users/Loviniou/Downloads/ChID-BLITS-EBU-Narration441-16b.wav

		* AAFInfo --aaf-clips ../libaaf_testfiles/ADP/ADP2_SEQ-FULL.aaf
		file://?/E:/Adrien/ADPAAF/Sequence A Rendu.mxf

		* AAFInfo --aaf-clips ../libaaf_testfiles/TEST-AVID_COMP2977052\ \ -\ \ OFF\ PODIUM\ ETAPE\ 2.aaf
		file:////C:/Users/mix_limo/Desktop/TEST2977052  -  OFF PODIUM ETAPE 2.aaf

		* AAFInfo --aaf-clips ../ardio/watchfolder/3572607_RUGBY_F_1_1.aaf
		file://10.87.230.71/mixage/DR2/Avid MediaFiles/MXF/1/3572607_RUGBY_F2_S65CFA3D0V.mxf

		* AAFInfo --aaf-clips ../libaaf_testfiles/ProTools/pt2MCC.aaf
		file:///_system/Users/horlaprod/pt2MCCzmhsFRHQgdgsTMQX.mxf
	*/
}

int
aafi_extract_audio_essence (AAF_Iface* aafi, aafiAudioEssence* audioEssence, const char* outfilepath, const wchar_t* forcedFileName)
{
	int   rc       = 0;
	int   reqlen   = 0;
	FILE* fp       = NULL;
	char* filename = NULL;
	char* filepath = NULL;

	unsigned char* data   = NULL;
	uint64_t       datasz = 0;

	if (audioEssence->is_embedded == 0) {
		warning ("Audio essence is not embedded : nothing to extract");
		return -1;
	}

	/* Retrieve stream from CFB */

	cfb_getStream (aafi->aafd->cfbd, audioEssence->node, &data, &datasz);

	if (data == NULL) {
		error ("Could not retrieve audio essence stream from CFB");
		goto err;
	}

	/* Build file path */

	reqlen = snprintf (NULL, 0, "%ls.%s", (forcedFileName != NULL) ? forcedFileName : audioEssence->unique_file_name, (audioEssence->type == AAFI_ESSENCE_TYPE_AIFC) ? AIFF_FILE_EXT : WAV_FILE_EXT);

	if (reqlen < 0) {
		error ("Failed to build filename");
		goto err;
	}

	int filenamelen = reqlen + 1;

	filename = malloc (filenamelen);

	if (filename == NULL) {
		error ("Could not allocate memory : %s", strerror (errno));
		goto err;
	}

	rc = snprintf (filename, filenamelen, "%ls.%s", (forcedFileName != NULL) ? forcedFileName : audioEssence->unique_file_name, (audioEssence->type == AAFI_ESSENCE_TYPE_AIFC) ? AIFF_FILE_EXT : WAV_FILE_EXT);

	if (rc < 0 || (unsigned)rc >= (unsigned)filenamelen) {
		error ("Failed to build filename");
		goto err;
	}

	filepath = laaf_util_build_path (DIR_SEP_STR, outfilepath, laaf_util_clean_filename (filename), NULL);

	if (filepath == NULL) {
		error ("Could not build filepath");
		goto err;
	}

	fp = fopen (filepath, "wb");

	if (fp == NULL) {
		error ("Could not open '%s' for writing : %s", filepath, strerror (errno));
		goto err;
	}

	if (audioEssence->type == AAFI_ESSENCE_TYPE_PCM) {
		struct wavFmtChunk wavFmt;
		wavFmt.channels        = audioEssence->channels;
		wavFmt.samples_per_sec = audioEssence->samplerate;
		wavFmt.bits_per_sample = audioEssence->samplesize;

		struct wavBextChunk wavBext;
		memset (&wavBext, 0x00, sizeof (wavBext));
		memcpy (wavBext.umid, audioEssence->sourceMobID, sizeof (aafMobID_t));
		if (audioEssence->mobSlotEditRate) {
			wavBext.time_reference = laaf_util_converUnit (audioEssence->timeReference, audioEssence->mobSlotEditRate, audioEssence->samplerateRational);
		}

		if (datasz >= (uint32_t)-1) {
			// TODO RF64 support ?
			error ("Audio essence is bigger than maximum wav file size (2^32 bytes) : %" PRIu64 " bytes", datasz);
			goto err;
		}

		if (riff_writeWavFileHeader (fp, &wavFmt, &wavBext, (uint32_t)datasz, aafi->dbg) < 0) {
			error ("Could not write wav audio header : %s", filepath);
			goto err;
		}
	}

	uint64_t writtenBytes = fwrite (data, sizeof (unsigned char), datasz, fp);

	if (writtenBytes < datasz) {
		error ("Could not write audio file (%" PRIu64 " bytes written out of %" PRIu64 " bytes) : %s", writtenBytes, datasz, filepath);
		goto err;
	}

	audioEssence->usable_file_path = malloc ((strlen (filepath) + 1) * sizeof (wchar_t));

	if (audioEssence->usable_file_path == NULL) {
		error ("Could not allocate memory : %s", strerror (errno));
		goto err;
	}

	audioEssence->usable_file_path = laaf_util_str2wstr (filepath);

	if (audioEssence->usable_file_path == NULL) {
		error ("Could not convert usable_file_path from str to wstr : %s", filepath);
		goto err;
	}

	rc = 0;
	goto end;

err:
	rc = -1;

end:
	if (filename)
		free (filename);

	if (filepath)
		free (filepath);

	if (data)
		free (data);

	if (fp)
		fclose (fp);

	return rc;
}

int
aafi_parse_audio_essence (AAF_Iface* aafi, aafiAudioEssence* audioEssence)
{
	// aafi->dbg->_dbg_msg_pos += laaf_util_dump_hex( audioEssence->summary->val, audioEssence->summary->len, &aafi->dbg->_dbg_msg, &aafi->dbg->_dbg_msg_size, aafi->dbg->_dbg_msg_pos );

	int                  rc               = 0;
	char*                externalFilePath = NULL;
	FILE*                fp               = NULL;
	struct RIFFAudioFile RIFFAudioFile;

	/* try audioEssence->summary first, for both embedded and external */

	if (audioEssence->summary) {
		rc = riff_parseAudioFile (&RIFFAudioFile, RIFF_PARSE_AAF_SUMMARY, &embeddedAudioDataReaderCallback, audioEssence->summary->val, &audioEssence->summary->len, aafi, aafi->dbg);

		if (rc < 0) {
			warning ("Could not parse essence summary of %ls", audioEssence->file_name);

			if (audioEssence->is_embedded) {
				return -1;
			}
		} else {
			audioEssence->channels   = RIFFAudioFile.channels;
			audioEssence->samplerate = RIFFAudioFile.sampleRate;
			audioEssence->samplesize = RIFFAudioFile.sampleSize;
			audioEssence->length     = RIFFAudioFile.sampleCount;

			audioEssence->samplerateRational->numerator   = audioEssence->samplerate;
			audioEssence->samplerateRational->denominator = 1;

			return 0;
		}
	} else if (audioEssence->is_embedded) {
		if (audioEssence->type != AAFI_ESSENCE_TYPE_PCM) {
			warning ("TODO: Embedded audio essence has no summary. Should we try essence data stream ?");
		}

		return -1;
	} else if (!audioEssence->usable_file_path) {
		// warning( "Can't parse a missing external essence file" );
		return -1;
	}

	if (laaf_util_fop_is_wstr_fileext (audioEssence->usable_file_path, L"wav") ||
	    laaf_util_fop_is_wstr_fileext (audioEssence->usable_file_path, L"wave") ||
	    laaf_util_fop_is_wstr_fileext (audioEssence->usable_file_path, L"aif") ||
	    laaf_util_fop_is_wstr_fileext (audioEssence->usable_file_path, L"aiff") ||
	    laaf_util_fop_is_wstr_fileext (audioEssence->usable_file_path, L"aifc")) {
		externalFilePath = laaf_util_wstr2str (audioEssence->usable_file_path);

		if (externalFilePath == NULL) {
			error ("Could not convert usable_file_path from wstr to str : %ls", audioEssence->usable_file_path);
			goto err;
		}

		fp = fopen (externalFilePath, "rb");

		if (fp == NULL) {
			error ("Could not open external audio essence file for reading : %s", externalFilePath);
			goto err;
		}

		rc = riff_parseAudioFile (&RIFFAudioFile, 0, &externalAudioDataReaderCallback, fp, externalFilePath, aafi, aafi->dbg);

		if (rc < 0) {
			error ("Failed parsing external audio essence file : %s", externalFilePath);
			goto err;
		}

		if (audioEssence->channels > 0 && audioEssence->channels != RIFFAudioFile.channels) {
			warning ("%ls : summary channel count (%i) mismatch located file (%i)", audioEssence->usable_file_path, audioEssence->channels, RIFFAudioFile.channels);
		}

		if (audioEssence->samplerate > 0 && audioEssence->samplerate != RIFFAudioFile.sampleRate) {
			warning ("%ls : summary samplerate (%i) mismatch located file (%i)", audioEssence->usable_file_path, audioEssence->samplerate, RIFFAudioFile.sampleRate);
		}

		if (audioEssence->samplesize > 0 && audioEssence->samplesize != RIFFAudioFile.sampleSize) {
			warning ("%ls : summary samplesize (%i) mismatch located file (%i)", audioEssence->usable_file_path, audioEssence->samplesize, RIFFAudioFile.sampleSize);
		}

		if (audioEssence->length > 0 && audioEssence->length != RIFFAudioFile.sampleCount) {
			warning ("%ls : summary samplecount (%" PRIi64 ") mismatch located file (%" PRIi64 ")", audioEssence->usable_file_path, audioEssence->length, RIFFAudioFile.sampleCount);
		}

		audioEssence->channels   = RIFFAudioFile.channels;
		audioEssence->samplerate = RIFFAudioFile.sampleRate;
		audioEssence->samplesize = RIFFAudioFile.sampleSize;
		audioEssence->length     = RIFFAudioFile.sampleCount;

		audioEssence->samplerateRational->numerator   = audioEssence->samplerate;
		audioEssence->samplerateRational->denominator = 1;
	} else {
		/*
		 * should be considered as a non-pcm audio format
		 *
│ 04317│├──◻ AAFClassID_TimelineMobSlot [slot:6 track:4] (DataDef : AAFDataDef_Sound) : Audio 4 - Layered Audio Editing
│ 01943││    └──◻ AAFClassID_Sequence
│ 02894││         └──◻ AAFClassID_SourceClip
│ 02899││              └──◻ AAFClassID_MasterMob (UsageCode: n/a) : speech-sample
│ 04405││                   └──◻ AAFClassID_TimelineMobSlot [slot:1 track:1] (DataDef : AAFDataDef_Sound)
│ 03104││                        └──◻ AAFClassID_SourceClip
│ 04140││                             └──◻ AAFClassID_SourceMob (UsageCode: n/a) : speech-sample
│ 01287││                                  └──◻ AAFClassID_PCMDescriptor
│ 01477││                                       └──◻ AAFClassID_NetworkLocator : file:///C:/Users/user/Desktop/libAAF/test/res/speech-sample.mp3
		 *
		 */

		audioEssence->type = AAFI_ESSENCE_TYPE_UNK;

		// /* clears any wrong data previously retrieved out of AAFClassID_PCMDescriptor */
		// audioEssence->samplerate = 0;
		// audioEssence->samplesize = 0;
	}

	rc = 0;
	goto end;

err:
	rc = -1;

end:
	if (fp)
		fclose (fp);

	if (externalFilePath)
		free (externalFilePath);

	return rc;
}

static size_t
embeddedAudioDataReaderCallback (unsigned char* buf, size_t offset, size_t reqLen, void* user1, void* user2, void* user3)
{
	unsigned char* data   = user1;
	size_t         datasz = *(size_t*)user2;
	AAF_Iface*     aafi   = (AAF_Iface*)user3;

	if (offset > datasz) {
		error ("Requested data starts beyond data length");
		return -1;
	}

	if (offset + reqLen > datasz) {
		reqLen = datasz - (offset + reqLen);
	}

	memcpy (buf, data + offset, reqLen);

	return reqLen;
}

static size_t
externalAudioDataReaderCallback (unsigned char* buf, size_t offset, size_t reqLen, void* user1, void* user2, void* user3)
{
	FILE*       fp       = (FILE*)user1;
	const char* filename = (const char*)user2;
	AAF_Iface*  aafi     = (AAF_Iface*)user3;

	if (fseek (fp, offset, SEEK_SET) < 0) {
		error ("Could not seek to %zu in file '%s' : %s", offset, filename, strerror (errno));
		return -1;
	}

	size_t read = fread (buf, sizeof (unsigned char), reqLen, fp);

	if (read < reqLen) {
		error ("File read failed at %zu (expected %zu, read %zu) in file '%s' : %s", offset, reqLen, read, filename, strerror (errno));
		return -1;
	}

	return read;
}
