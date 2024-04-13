/*
 * Copyright (C) 2017-2024 Adrien Gesta-Fline
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

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "aaf/AAFIEssenceFile.h"
#include "aaf/AAFIface.h"
#include "aaf/log.h"
#include "aaf/version.h"

#include "aaf/MediaComposer.h"
#include "aaf/utils.h"

#include "aaf/RIFFParser.h"
#include "aaf/URIParser.h"

#define debug(...) \
	AAF_LOG (aafi->log, aafi, LOG_SRC_ID_AAF_IFACE, VERB_DEBUG, __VA_ARGS__)

#define success(...) \
	AAF_LOG (aafi->log, aafi, LOG_SRC_ID_AAF_IFACE, VERB_SUCCESS, __VA_ARGS__)

#define warning(...) \
	AAF_LOG (aafi->log, aafi, LOG_SRC_ID_AAF_IFACE, VERB_WARNING, __VA_ARGS__)

#define error(...) \
	AAF_LOG (aafi->log, aafi, LOG_SRC_ID_AAF_IFACE, VERB_ERROR, __VA_ARGS__)

static int
set_audioEssenceWithRIFF (AAF_Iface* aafi, const char* filename, aafiAudioEssenceFile* audioEssenceFile, struct RIFFAudioFile* RIFFAudioFile, int isExternalFile);
static size_t
embeddedAudioDataReaderCallback (unsigned char* buf, size_t offset, size_t reqLen, void* user1, void* user2, void* user3);
static size_t
externalAudioDataReaderCallback (unsigned char* buf, size_t offset, size_t reqLen, void* user1, void* user2, void* user3);

int
aafi_build_unique_audio_essence_name (AAF_Iface* aafi, aafiAudioEssenceFile* audioEssenceFile)
{
	if (audioEssenceFile->unique_name) {
		debug ("Unique name was already set");
		return -1;
	}

	if (aafi->ctx.options.mobid_essence_filename) {
		aafUID_t* uuid = &(audioEssenceFile->sourceMobID->material);

		int rc = laaf_util_snprintf_realloc (&audioEssenceFile->unique_name, 0, 0, "%08x-%04x-%04x-%02x%02x%02x%02x%02x%02x%02x%02x",
		                                     uuid->Data1,
		                                     uuid->Data2,
		                                     uuid->Data3,
		                                     uuid->Data4[0],
		                                     uuid->Data4[1],
		                                     uuid->Data4[2],
		                                     uuid->Data4[3],
		                                     uuid->Data4[4],
		                                     uuid->Data4[5],
		                                     uuid->Data4[6],
		                                     uuid->Data4[7]);

		if (rc < 0) {
			error ("Failed to set unique filename with SourceMobID UID");
			free (audioEssenceFile->unique_name);
			return -1;
		}

		return 0;
	} else {
		audioEssenceFile->unique_name = laaf_util_c99strdup ((audioEssenceFile->name) ? audioEssenceFile->name : "unknown");

		if (!audioEssenceFile->unique_name) {
			error ("Could not duplicate essence name : %s", (audioEssenceFile->name) ? audioEssenceFile->name : "unknown");
			return -1;
		}
	}

	size_t unique_size = strlen (audioEssenceFile->unique_name) + 1;

	int                   index = 0;
	aafiAudioEssenceFile* ae    = NULL;

	AAFI_foreachAudioEssenceFile (aafi, ae)
	{
		if (ae != audioEssenceFile && ae->unique_name != NULL && strcmp (ae->unique_name, audioEssenceFile->unique_name) == 0) {
			if (laaf_util_snprintf_realloc (&audioEssenceFile->unique_name, &unique_size, 0, "%s_%i", (audioEssenceFile->name) ? audioEssenceFile->name : "unknown", ++index) < 0) {
				error ("Failed to increment unique filename");
				return -1;
			}

			/* recheck entire essence list */
			ae = aafi->Audio->essenceFiles;
		}
	}

	return 0;
}

char*
aafi_locate_external_essence_file (AAF_Iface* aafi, const char* original_uri_filepath, const char* search_location)
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

	char*       local_filepath = NULL;
	char*       aaf_path       = NULL;
	const char* foundpath      = NULL;
	char*       retpath        = NULL;

	struct uri* uri = NULL;

	if (original_uri_filepath == NULL) {
		error ("Cant locate a NULL filepath");
		goto err;
	}

	debug ("Original URI : %s", original_uri_filepath);

	uri = laaf_uri_parse (original_uri_filepath, URI_OPT_DECODE_ALL, aafi->log);

	if (uri == NULL) {
		error ("Could not parse URI");
		goto err;
	}

	if (uri->path == NULL) {
		error ("Could not retrieve <path> out of URI");
		goto err;
	}

	debug ("Decoded URI's path : %s", uri->path);

	/* extract relative path to essence file : "<firstparent>/<essence.file>" */

	const char* relativeEssencePath = NULL;
	const char* essenceFileName     = NULL;

	int   sepcount = 0;
	char* p        = uri->path + strlen (uri->path);

	while (p > uri->path) {
		if (*p == '/') { /* parsing URI, so will always be '/' as separator character */
			sepcount++;
			if (sepcount == 1) {
				essenceFileName = (p + 1);
			} else if (sepcount == 2) {
				relativeEssencePath = (p + 1);
				break;
			}
		}
		p--;
	}

	if (!relativeEssencePath) {
		error ("Could not retrieve relative file path out of URI : %s", uri->path);
		goto err;
	}

	if (!essenceFileName) {
		error ("Could not retrieve file name out of URI : %s", uri->path);
		goto err;
	}

	debug ("Essence filename : %s", essenceFileName);

	if (search_location) {
		/*
		 * "<search_location>/<essence.file>"
		 */

		local_filepath = laaf_util_build_path ("/", search_location, essenceFileName, NULL);

		if (!local_filepath) {
			error ("Could not build search filepath");
			goto err;
		}

		debug ("Search filepath : %s", local_filepath);

		if (laaf_util_file_exists (local_filepath) == 1) {
			foundpath = local_filepath;
			goto found;
		}

		free (local_filepath);
		local_filepath = NULL;

		/*
		 * "<search_location>/<firstparentInOriginalEssencePath>/<essence.file>"
		 */

		local_filepath = laaf_util_build_path ("/", search_location, relativeEssencePath, NULL);

		if (!local_filepath) {
			error ("Could not build search filepath");
			goto err;
		}

		debug ("Search filepath : %s", local_filepath);

		if (laaf_util_file_exists (local_filepath) == 1) {
			foundpath = local_filepath;
			goto found;
		}

		free (local_filepath);
		local_filepath = NULL;
	}

	/* Try raw essence's URI, just in case... */

	if (laaf_util_file_exists (original_uri_filepath) == 1) {
		foundpath = original_uri_filepath;
		goto found;
	}

	/* Try <path> part of URI */

	if (laaf_util_file_exists (uri->path) == 1) {
		foundpath = uri->path;
		goto found;
	}

	/*
	 * Try to locate essence file from the AAF file location.
	 *
	 * e.g.
	 *      AAF filepath : /home/user/AAFFile.aaf
	 *    + Essence URI  : file://localhost/C:/Users/user/Desktop/AudioFiles/essence.wav
	 *    = /home/user/AudioFiles/essence.file
	 */

	/* extract path to AAF file */

	aaf_path = laaf_util_c99strdup (aafi->aafd->cfbd->file);

	if (!aaf_path) {
		error ("Could not duplicate AAF filepath : %s", aafi->aafd->cfbd->file);
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

	if (!local_filepath) {
		error ("Could not build filepath");
		goto err;
	}

	debug ("AAF relative filepath : %s", local_filepath);

	if (laaf_util_file_exists (local_filepath) == 1) {
		foundpath = local_filepath;
		goto found;
	}

	free (local_filepath);
	local_filepath = NULL;

	/*
	 * "<localPathToAAFfile>/<firstparentInOriginalEssencePath>/<essence.file>"
	 */

	local_filepath = laaf_util_build_path (DIR_SEP_STR, aaf_path, relativeEssencePath, NULL);

	if (!local_filepath) {
		error ("Could not build filepath");
		goto err;
	}

	debug ("AAF relative sub filepath : %s", local_filepath);

	if (laaf_util_file_exists (local_filepath) == 1) {
		foundpath = local_filepath;
		goto found;
	}

	free (local_filepath);
	local_filepath = NULL;

	debug ("File not found");

found:
	if (foundpath) {
		/*
		 * When runing through wine, computing absolute path adds a Z:/ drive letter.
		 * This causes issue when trying to make relative essence path from the AAF
		 * file path, since it also went through laaf_util_absolute_path().
		 * So even if foundpath is already absolute, we need that drive letter at it
		 * start.
		 */
		retpath = laaf_util_absolute_path (foundpath);

		if (!retpath) {
			error ("Could not make absolute path to located file : %s", foundpath);
			goto err;
		}

		debug ("File found at : %s", foundpath);
	}

	goto end;

err:
	retpath = NULL;

end:
	laaf_uri_free (uri);

	free (local_filepath);
	free (aaf_path);

	return retpath;
}

int
aafi_extractAudioEssenceFile (AAF_Iface* aafi, aafiAudioEssenceFile* audioEssenceFile, enum aafiExtractFormat extractFormat, const char* outpath, uint64_t sampleOffset, uint64_t sampleLength, const char* forcedFileName, char** usable_file_path)
{
	int   rc       = 0;
	int   tmp      = 0;
	FILE* fp       = NULL;
	char* filename = NULL;
	char* filepath = NULL;

	int write_header    = 0;
	int extracting_clip = 0;

	unsigned char* data   = NULL;
	uint64_t       datasz = 0;

	if (audioEssenceFile->is_embedded == 0) {
		error ("Audio essence is not embedded : nothing to extract");
		goto err;
	}

	if (!outpath) {
		error ("Missing output path");
		goto err;
	}

	if (audioEssenceFile->usable_file_path) {
		debug ("usable_file_path was already set");
		free (audioEssenceFile->usable_file_path);
		audioEssenceFile->usable_file_path = NULL;
	}

	uint64_t pcmByteOffset = sampleOffset * audioEssenceFile->channels * (audioEssenceFile->samplesize / 8);
	uint64_t pcmByteLength = sampleLength * audioEssenceFile->channels * (audioEssenceFile->samplesize / 8);

	/* Retrieve stream from CFB */

	cfb_getStream (aafi->aafd->cfbd, audioEssenceFile->node, &data, &datasz);

	if (data == NULL) {
		error ("Could not retrieve audio essence stream from CFB");
		goto err;
	}

	/* Calculate offset and length */

	debug ("Requesting extract of essence \"%s\"", audioEssenceFile->name);
	debug (" -    ReqSampleOffset: %" PRIu64 " samples (%" PRIu64 " bytes)", sampleOffset, pcmByteOffset);
	debug (" -    ReqSampleLength: %" PRIu64 " samples (%" PRIu64 " bytes)", sampleLength, pcmByteLength);
	debug (" -   FileHeaderOffset: %" PRIu64 " bytes (0x%04" PRIx64 ")",
	       audioEssenceFile->pcm_audio_start_offset,
	       audioEssenceFile->pcm_audio_start_offset);
	debug (" - EssenceTotalLength: %" PRIu64 " bytes", datasz);

	uint64_t sourceFileOffset = 0;

	if (pcmByteOffset ||
	    pcmByteLength ||
	    extractFormat != AAFI_EXTRACT_DEFAULT) {
		if (audioEssenceFile->type != AAFI_ESSENCE_TYPE_PCM) {
			sourceFileOffset += audioEssenceFile->pcm_audio_start_offset;
		}
		write_header = 1;
	}

	if (pcmByteOffset || pcmByteLength) {
		extracting_clip = 1;
	}

	sourceFileOffset += pcmByteOffset;

	if ((datasz - audioEssenceFile->pcm_audio_start_offset) < (pcmByteLength + sourceFileOffset)) {
		error ("Requested audio range (%" PRIi64 " bytes) is bigger than source audio size (%" PRIu64 " bytes)",
		       (pcmByteLength + sourceFileOffset),
		       datasz - audioEssenceFile->pcm_audio_start_offset);
		goto err;
	}

	datasz = (pcmByteLength) ? pcmByteLength : (datasz - sourceFileOffset);

	if (datasz >= (uint32_t)-1) {
		error ("Audio essence is bigger than maximum wav file size (2^32 bytes) : %" PRIu64 " bytes", datasz);
		goto err;
	}

	debug (" -  Calculated Offset: %" PRIu64 " bytes", sourceFileOffset);
	debug (" -  Calculated Length: %" PRIu64 " bytes", datasz);

	if (audioEssenceFile->type != AAFI_ESSENCE_TYPE_PCM) {
		if (!write_header) {
			debug ("Writting exact copy of embedded file.");
		} else {
			debug ("Rewriting file header.");
		}
	}

	/* Build file path */
	const char* name = NULL;

	if (forcedFileName) {
		name = forcedFileName;
	} else {
		name = audioEssenceFile->unique_name;
	}

	const char* fileext = NULL;

	if (write_header ||
	    audioEssenceFile->type == AAFI_ESSENCE_TYPE_WAVE ||
	    audioEssenceFile->type == AAFI_ESSENCE_TYPE_PCM) {
		if (!laaf_util_is_fileext (name, "wav") &&
		    !laaf_util_is_fileext (name, "wave")) {
			fileext = "wav";
		}
	} else if (!write_header &&
	           audioEssenceFile->type == AAFI_ESSENCE_TYPE_AIFC) {
		if (!laaf_util_is_fileext (name, "aif") &&
		    !laaf_util_is_fileext (name, "aiff") &&
		    !laaf_util_is_fileext (name, "aifc")) {
			fileext = "aif";
		}
	}

	if (fileext) {
		if (laaf_util_snprintf_realloc (&filename, NULL, 0, "%s.%s", name, fileext) < 0) {
			error ("Could not concat filename + fileext");
			goto err;
		}
	} else {
		filename = laaf_util_c99strdup (name);

		if (!filename) {
			error ("Could not duplicate filename : %s", name);
			goto err;
		}
	}

	filepath = laaf_util_build_path (DIR_SEP_STR, outpath, laaf_util_clean_filename (filename), NULL);

	if (!filepath) {
		error ("Could not build filepath.");
		goto err;
	}

	fp = fopen (filepath, "wb");

	if (!fp) {
		error ("Could not open '%s' for writing : %s", filepath, strerror (errno));
		goto err;
	}

	if (write_header ||
	    audioEssenceFile->type == AAFI_ESSENCE_TYPE_PCM) {
		struct wavFmtChunk wavFmt;
		wavFmt.channels        = audioEssenceFile->channels;
		wavFmt.samples_per_sec = audioEssenceFile->samplerate;
		wavFmt.bits_per_sample = audioEssenceFile->samplesize;

		struct wavBextChunk wavBext;
		memset (&wavBext, 0x00, sizeof (wavBext));

		memcpy (wavBext.umid, audioEssenceFile->sourceMobID, sizeof (aafMobID_t));

		tmp = snprintf (wavBext.originator, sizeof (((struct wavBextChunk*)0)->originator), "%s %s", aafi->aafd->Identification.ProductName, (mediaComposer_AAF (aafi)) ? "" : aafi->aafd->Identification.ProductVersionString);

		assert (tmp > 0 && (size_t)tmp < sizeof (((struct wavBextChunk*)0)->originator));

		tmp = snprintf (wavBext.originator_reference, sizeof (((struct wavBextChunk*)0)->originator_reference), "libAAF %s", LIBAAF_VERSION);

		assert (tmp > 0 && (size_t)tmp < sizeof (((struct wavBextChunk*)0)->originator_reference));

		tmp = snprintf (wavBext.description, sizeof (((struct wavBextChunk*)0)->description), "%s\n%s.aaf", audioEssenceFile->name, aafi->compositionName);

		assert (tmp > 0 && (size_t)tmp < sizeof (((struct wavBextChunk*)0)->description));

		memcpy (wavBext.origination_date, audioEssenceFile->originationDate, sizeof (((struct wavBextChunk*)0)->origination_date));
		memcpy (wavBext.origination_time, audioEssenceFile->originationTime, sizeof (((struct wavBextChunk*)0)->origination_time));

		wavBext.time_reference = aafi_convertUnitUint64 (audioEssenceFile->sourceMobSlotOrigin, audioEssenceFile->sourceMobSlotEditRate, audioEssenceFile->samplerateRational);

		if (laaf_riff_writeWavFileHeader (fp, &wavFmt, (extractFormat != AAFI_EXTRACT_WAV) ? &wavBext : NULL, (uint32_t)datasz, aafi->log) < 0) {
			error ("Could not write wav audio header : %s", filepath);
			goto err;
		}
	}

	uint64_t writtenBytes = 0;

	if (write_header && audioEssenceFile->type == AAFI_ESSENCE_TYPE_AIFC && audioEssenceFile->samplesize > 8) {
		unsigned char sample[4];
		uint16_t      samplesize = (audioEssenceFile->samplesize >> 3);

		for (uint64_t i = 0; i < datasz; i += samplesize) {
			if (samplesize == 2) {
				sample[0] = *(unsigned char*)(data + pcmByteOffset + i + 1);
				sample[1] = *(unsigned char*)(data + pcmByteOffset + i);
			} else if (samplesize == 3) {
				sample[0] = *(unsigned char*)(data + pcmByteOffset + i + 2);
				sample[1] = *(unsigned char*)(data + pcmByteOffset + i + 1);
				sample[2] = *(unsigned char*)(data + pcmByteOffset + i);
			} else if (samplesize == 4) {
				sample[0] = *(unsigned char*)(data + pcmByteOffset + i + 3);
				sample[1] = *(unsigned char*)(data + pcmByteOffset + i + 2);
				sample[2] = *(unsigned char*)(data + pcmByteOffset + i + 1);
				sample[3] = *(unsigned char*)(data + pcmByteOffset + i);
			}

			writtenBytes += fwrite (sample, samplesize, 1, fp);
		}

		writtenBytes *= samplesize;
	} else {
		writtenBytes = fwrite (data + sourceFileOffset, sizeof (unsigned char), datasz, fp);
	}

	if (writtenBytes < datasz) {
		error ("Could not write audio file (%" PRIu64 " bytes written out of %" PRIu64 " bytes) : %s", writtenBytes, datasz, filepath);
		goto err;
	}

	if (!extracting_clip) {
		/*
		 * Set audioEssenceFile->usable_file_path only if we axtract essence, not if we
		 * extract clip (subset of an essence), as a single essence can have multiple
		 * clips using it. Otherwise, we would reset audioEssenceFile->usable_file_path
		 * as many times as there are clips using the same essence.
		 */
		audioEssenceFile->usable_file_path = laaf_util_c99strdup (filepath);

		if (!audioEssenceFile->usable_file_path) {
			error ("Could not duplicate usable filepath : %s", filepath);
			goto err;
		}
	}

	if (usable_file_path) {
		*usable_file_path = laaf_util_c99strdup (filepath);

		if (!(*usable_file_path)) {
			error ("Could not duplicate usable filepath : %s", filepath);
			goto err;
		}
	}

	rc = 0;
	goto end;

err:
	rc = -1;

end:
	free (filename);
	free (filepath);
	free (data);

	if (fp)
		fclose (fp);

	return rc;
}

int
aafi_extractAudioClip (AAF_Iface* aafi, aafiAudioClip* audioClip, enum aafiExtractFormat extractFormat, const char* outpath)
{
	int rc = 0;

	for (aafiAudioEssencePointer* audioEssencePtr = audioClip->essencePointerList; audioEssencePtr; audioEssencePtr = audioEssencePtr->next) {
		aafiAudioEssenceFile* audioEssenceFile = audioClip->essencePointerList->essenceFile;

		uint64_t sampleOffset = aafi_convertUnitUint64 (audioClip->essence_offset, audioClip->track->edit_rate, audioEssenceFile->samplerateRational);
		uint64_t sampleLength = aafi_convertUnitUint64 (audioClip->len, audioClip->track->edit_rate, audioEssenceFile->samplerateRational);

		char* name             = NULL;
		char* usable_file_path = NULL;

		laaf_util_snprintf_realloc (&name, NULL, 0, "%i_%i_%s", audioClip->track->number, aafi_get_clipIndex (audioClip), audioClip->essencePointerList->essenceFile->unique_name);

		if ((rc += aafi_extractAudioEssenceFile (aafi, audioEssenceFile, extractFormat, outpath, sampleOffset, sampleLength, name, &usable_file_path)) == 0) {
			success ("Audio clip file extracted to %s\"%s\"%s",
			         ANSI_COLOR_DARKGREY (aafi->log),
			         usable_file_path,
			         ANSI_COLOR_RESET (aafi->log));
		} else {
			error ("Audio clip file extraction failed : %s\"%s\"%s", ANSI_COLOR_DARKGREY (aafi->log), name, ANSI_COLOR_RESET (aafi->log));
		}

		free (usable_file_path);
		free (name);

		usable_file_path = NULL;
		name             = NULL;
	}

	return rc;
}

static int
set_audioEssenceWithRIFF (AAF_Iface* aafi, const char* filename, aafiAudioEssenceFile* audioEssenceFile, struct RIFFAudioFile* RIFFAudioFile, int isExternalFile)
{
	if (RIFFAudioFile->sampleCount >= INT64_MAX) {
		error ("%s : summary sample count is bigger than INT64_MAX (%" PRIu64 ")", audioEssenceFile->usable_file_path, RIFFAudioFile->sampleCount);
		return -1;
	}

	if (RIFFAudioFile->sampleRate >= INT_MAX) {
		error ("%s : summary sample rate is bigger than INT_MAX (%li)", audioEssenceFile->usable_file_path, RIFFAudioFile->sampleRate);
		return -1;
	}

	if (audioEssenceFile->channels > 0 && audioEssenceFile->channels != RIFFAudioFile->channels) {
		warning ("%s : summary channel count (%i) mismatch %s (%i)", filename, audioEssenceFile->channels, ((isExternalFile) ? "located file" : "previously retrieved data"), RIFFAudioFile->channels);
	}

	if (audioEssenceFile->samplerate > 0 && audioEssenceFile->samplerate != RIFFAudioFile->sampleRate) {
		warning ("%s : summary samplerate (%i) mismatch %s (%i)", filename, audioEssenceFile->samplerate, ((isExternalFile) ? "located file" : "previously retrieved data"), RIFFAudioFile->sampleRate);
	}

	if (audioEssenceFile->samplesize > 0 && audioEssenceFile->samplesize != RIFFAudioFile->sampleSize) {
		warning ("%s : summary samplesize (%i) mismatch %s (%i)", filename, audioEssenceFile->samplesize, ((isExternalFile) ? "located file" : "previously retrieved data"), RIFFAudioFile->sampleSize);
	}

	if (audioEssenceFile->length > 0 && (uint64_t)audioEssenceFile->length != RIFFAudioFile->sampleCount) {
		warning ("%s : summary samplecount (%" PRIi64 ") mismatch %s (%" PRIi64 ")", filename, audioEssenceFile->length, ((isExternalFile) ? "located file" : "previously retrieved data"), RIFFAudioFile->sampleCount);
	}

	audioEssenceFile->channels   = RIFFAudioFile->channels;
	audioEssenceFile->samplerate = RIFFAudioFile->sampleRate;
	audioEssenceFile->samplesize = RIFFAudioFile->sampleSize;

	audioEssenceFile->length                        = (aafPosition_t)RIFFAudioFile->sampleCount;
	audioEssenceFile->pcm_audio_start_offset        = (uint64_t)RIFFAudioFile->pcm_audio_start_offset;
	audioEssenceFile->samplerateRational->numerator = (int32_t)audioEssenceFile->samplerate;

	audioEssenceFile->samplerateRational->denominator = 1;

	return 0;
}

int
aafi_parse_audio_essence (AAF_Iface* aafi, aafiAudioEssenceFile* audioEssenceFile)
{
	int                  rc             = 0;
	uint64_t             dataStreamSize = 0;
	unsigned char*       dataStream     = NULL;
	FILE*                fp             = NULL;
	struct RIFFAudioFile RIFFAudioFile;

	/* try audioEssenceFile->summary first, for both embedded and external */

	if (audioEssenceFile->summary) {
		rc = laaf_riff_parseAudioFile (&RIFFAudioFile, RIFF_PARSE_AAF_SUMMARY, &embeddedAudioDataReaderCallback, audioEssenceFile->summary->val, &audioEssenceFile->summary->len, aafi, aafi->log);

		if (rc < 0) {
			if (!audioEssenceFile->is_embedded && !audioEssenceFile->usable_file_path) {
				warning ("Could not parse essence summary of \"%s\".", audioEssenceFile->name);
				goto err;
			}

			warning ("Could not parse essence summary of \"%s\". %s",
			         audioEssenceFile->name,
			         (audioEssenceFile->is_embedded) ? "Trying essence data stream." : (audioEssenceFile->usable_file_path) ? "Trying external essence file."
			                                                                                                                : " WTF ???");
		} else {
			if (set_audioEssenceWithRIFF (aafi, "AAF Summary", audioEssenceFile, &RIFFAudioFile, 0) < 0) {
				goto err;
			}

			if (!RIFFAudioFile.channels ||
			    !RIFFAudioFile.sampleRate ||
			    !RIFFAudioFile.sampleSize ||
			    !RIFFAudioFile.sampleCount) {
				/*
				 * Adobe Premiere Pro AIFC/WAVE Summaries of external files are missing
				 * SSND chunk/DATA chunk size (RIFFAudioFile.sampleCount)
				 */

				if (!audioEssenceFile->is_embedded && !audioEssenceFile->usable_file_path) {
					warning ("Summary of \"%s\" is missing some data.", audioEssenceFile->name);
					goto err;
				}

				warning ("Summary of \"%s\" is missing some data. %s",
				         audioEssenceFile->name,
				         (audioEssenceFile->is_embedded) ? "Trying essence data stream." : (audioEssenceFile->usable_file_path) ? "Trying external essence file."
				                                                                                                                : " WTF ???");
			} else {
				goto end;
			}
		}
	} else if (audioEssenceFile->is_embedded) {
		warning ("Embedded audio essence \"%s\" has no summary. Trying essence data stream.", audioEssenceFile->name);
	} else if (audioEssenceFile->usable_file_path) {
		warning ("External audio essence \"%s\" has no summary. Trying external file.", audioEssenceFile->name);
	}

	if (audioEssenceFile->is_embedded) {
		cfb_getStream (aafi->aafd->cfbd, audioEssenceFile->node, &dataStream, &dataStreamSize);

		if (dataStream == NULL) {
			error ("Could not retrieve audio essence stream from CFB");
			goto err;
		}

		rc = laaf_riff_parseAudioFile (&RIFFAudioFile, RIFF_PARSE_AAF_SUMMARY, &embeddedAudioDataReaderCallback, dataStream, &dataStreamSize, aafi, aafi->log);

		if (rc < 0) {
			warning ("Could not parse embedded essence stream of \"%s\".", audioEssenceFile->name);
			goto err;
		}

		if (set_audioEssenceWithRIFF (aafi, "AAF Embedded stream", audioEssenceFile, &RIFFAudioFile, 0) < 0) {
			goto err;
		}

		goto end;
	}

	if (laaf_util_is_fileext (audioEssenceFile->usable_file_path, "wav") ||
	    laaf_util_is_fileext (audioEssenceFile->usable_file_path, "wave") ||
	    laaf_util_is_fileext (audioEssenceFile->usable_file_path, "aif") ||
	    laaf_util_is_fileext (audioEssenceFile->usable_file_path, "aiff") ||
	    laaf_util_is_fileext (audioEssenceFile->usable_file_path, "aifc")) {
		fp = fopen (audioEssenceFile->usable_file_path, "rb");

		if (fp == NULL) {
			error ("Could not open external audio essence file for reading : %s", audioEssenceFile->usable_file_path);
			goto err;
		}

		rc = laaf_riff_parseAudioFile (&RIFFAudioFile, 0, &externalAudioDataReaderCallback, fp, audioEssenceFile->usable_file_path, aafi, aafi->log);

		if (rc < 0) {
			error ("Failed parsing external audio essence file : %s", audioEssenceFile->usable_file_path);
			goto err;
		}

		if (set_audioEssenceWithRIFF (aafi, audioEssenceFile->usable_file_path, audioEssenceFile, &RIFFAudioFile, 1) < 0) {
			goto err;
		}
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

		audioEssenceFile->type = AAFI_ESSENCE_TYPE_UNK;
	}

	rc = 0;
	goto end;

err:
	rc = -1;

end:
	free (dataStream);

	if (fp)
		fclose (fp);

	return rc;
}

static size_t
embeddedAudioDataReaderCallback (unsigned char* buf, size_t offset, size_t reqlen, void* user1, void* user2, void* user3)
{
	unsigned char* data   = user1;
	size_t         datasz = *(size_t*)user2;
	AAF_Iface*     aafi   = (AAF_Iface*)user3;

	if (offset > datasz) {
		error ("Requested data starts beyond data length");
		return RIFF_READER_ERROR;
	}

	if (offset + reqlen > datasz) {
		reqlen = datasz - (offset + reqlen);
	}

	memcpy (buf, data + offset, reqlen);

	return reqlen;
}

static size_t
externalAudioDataReaderCallback (unsigned char* buf, size_t offset, size_t reqlen, void* user1, void* user2, void* user3)
{
	FILE*       fp       = (FILE*)user1;
	const char* filename = (const char*)user2;
	AAF_Iface*  aafi     = (AAF_Iface*)user3;

#ifdef _WIN32
	assert (offset < _I64_MAX);

	if (_fseeki64 (fp, (__int64)offset, SEEK_SET) < 0) {
		error ("Could not seek to %" PRIu64 " in file '%s' : %s", offset, filename, strerror (errno));
		return RIFF_READER_ERROR;
	}
#else
	assert (offset < LONG_MAX);

	if (fseek (fp, (long)offset, SEEK_SET) < 0) {
		error ("Could not seek to %" PRIu64 " in file '%s' : %s", offset, filename, strerror (errno));
		return RIFF_READER_ERROR;
	}
#endif

	size_t byteRead = fread (buf, sizeof (unsigned char), reqlen, fp);

	if (feof (fp)) {
		if (byteRead < reqlen) {
			error ("Incomplete fread() of '%s' due to EOF : %" PRIu64 " bytes read out of %" PRIu64 " requested", filename, byteRead, reqlen);
			return RIFF_READER_ERROR;
		}
		debug ("fread() : EOF reached in file '%s'", filename);
	} else if (ferror (fp)) {
		if (byteRead < reqlen) {
			error ("Incomplete fread() of '%s' due to error : %" PRIu64 " bytes read out of %" PRIu64 " requested", filename, byteRead, reqlen);
		} else {
			error ("fread() error of '%s' : %" PRIu64 " bytes read out of %" PRIu64 " requested", filename, byteRead, reqlen);
		}
		return RIFF_READER_ERROR;
	}

	return byteRead;
}
