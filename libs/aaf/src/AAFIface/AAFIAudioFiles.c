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

#include <libaaf/AAFIAudioFiles.h>
#include <libaaf/AAFIface.h>
#include <libaaf/debug.h>

#include "RIFFParser.h"
#include "URIParser.h"

#include <libaaf/utils.h>

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

#define debug(...)                                                             \
  _dbg(aafi->dbg, aafi, DEBUG_SRC_ID_AAF_IFACE, VERB_DEBUG, __VA_ARGS__)

#define warning(...)                                                           \
  _dbg(aafi->dbg, aafi, DEBUG_SRC_ID_AAF_IFACE, VERB_WARNING, __VA_ARGS__)

#define error(...)                                                             \
  _dbg(aafi->dbg, aafi, DEBUG_SRC_ID_AAF_IFACE, VERB_ERROR, __VA_ARGS__)

static size_t embeddedAudioDataReaderCallback(unsigned char *buf, size_t offset,
                                              size_t reqLen, void *user1,
                                              void *user2, void *user3);
static size_t externalAudioDataReaderCallback(unsigned char *buf, size_t offset,
                                              size_t reqLen, void *user1,
                                              void *user2, void *user3);

char *aafi_locate_external_essence_file(AAF_Iface *aafi,
                                        const wchar_t *original_uri_filepath,
                                        const char *search_location) {
  /*
   * Absolute Uniform Resource Locator (URL) complying with RFC 1738 or relative
   * Uniform Resource Identifier (URI) complying with RFC 2396 for file
   * containing the essence. If it is a relative URI, the base URI is determined
   * from the URI of the AAF file itself.
   *
   * Informative note: A valid URL or URI uses a constrained character set and
   * uses the / character as the path separator.
   */

  char *uri_filepath = NULL;
  char *local_filepath = NULL;
  char *aaf_path = NULL;
  char *retpath = NULL;

  struct uri *uri = NULL;

  if (original_uri_filepath == NULL) {
    error("Cant locate a NULL filepath");
    goto err;
  }

  size_t uri_filepath_len = wcslen(original_uri_filepath) + 1;

  uri_filepath = malloc(uri_filepath_len);

  if (uri_filepath == NULL) {
    error("Could not allocate memory : %s", strerror(errno));
    goto err;
  }

  int reqlen =
      snprintf(uri_filepath, uri_filepath_len, "%ls", original_uri_filepath);

  if (reqlen < 0 || (unsigned)reqlen >= uri_filepath_len) {
    error("Failed converting wide char URI filepath to byte char%s",
          (reqlen < 0) ? " : encoding error" : "");
    goto err;
  }

  // debug( "Original URI filepath : %s", uri_filepath );

  uri = uriParse(uri_filepath, URI_OPT_DECODE_ALL, aafi->dbg);

  if (uri == NULL) {
    error("Could not parse URI");
    goto err;
  }

  if (uri->path == NULL) {
    error("Could not retrieve <path> out of URI");
    goto err;
  }

  // debug( "Decoded URI's filepath : %s", uri->path );

  /* extract relative path to essence file : "<firstparent>/<essence.file>" */

  char *relativeEssencePath = NULL;
  char *p = uri->path + strlen(uri->path);

  int sepcount = 0;

  while (p > uri->path) {
    if (*p ==
        '/') { /* parsing URI, so will always be '/' as separator character */
      sepcount++;
      if (sepcount == 2) {
        relativeEssencePath = (p + 1);
        break;
      }
    }
    p--;
  }

  const char *essenceFileName = laaf_util_fop_get_file(uri->path);

  // debug( "Essence filename : %s", essenceFileName );

  if (search_location) {

    /*
     * "<search_location>/<essence.file>"
     */

    local_filepath = laaf_util_build_path(DIR_SEP_STR, search_location,
                                          essenceFileName, NULL);

    if (local_filepath == NULL) {
      error("Could not build search filepath");
      goto err;
    }

    // debug( "Search filepath : %s", local_filepath );

    if (access(local_filepath, F_OK) != -1) {
      // debug( "FOUND: %s", local_filepath );
      retpath = local_filepath;
      goto found;
    }

    free(local_filepath);
    local_filepath = NULL;

    /*
     * "<search_location>/<firstparentInOriginalEssencePath>/<essence.file>"
     */

    local_filepath = laaf_util_build_path(DIR_SEP_STR, search_location,
                                          relativeEssencePath, NULL);

    if (local_filepath == NULL) {
      error("Could not build search filepath");
      goto err;
    }

    // debug( "Search filepath : %s", local_filepath );

    if (access(local_filepath, F_OK) != -1) {
      // debug( "FOUND: %s", local_filepath );
      retpath = local_filepath;
      goto found;
    }

    free(local_filepath);
    local_filepath = NULL;
  }

  /* Try AAF essence's URI */

  if (access(uri_filepath, F_OK) != -1) {
    // debug( "FOUND: %s", uri_filepath );
    retpath = uri_filepath;
    goto found;
  }

  /* Try <path> part of URI */

  if (access(uri->path, F_OK) != -1) {
    // debug( "FOUND: %s", uri->path );
    retpath = uri->path;
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
   *    - Essence URI  :
   * file://localhost/C:/Users/user/Desktop/AudioFiles/essence.wav =
   * /home/user/AudioFiles/essence.file
   */

  /* extract path to AAF file */

  aaf_path = laaf_util_c99strdup(aafi->aafd->cfbd->file);

  if (aaf_path == NULL) {
    error("Could not duplicate AAF filepath");
    goto err;
  }

  p = aaf_path + strlen(aaf_path);

  while (p > aaf_path) {
    if (IS_DIR_SEP(*p)) {
      *p = 0x00;
      break;
    }
    p--;
  }

  /*
   * "<localPathToAAFfile>/<essence.file>"
   */

  local_filepath =
      laaf_util_build_path(DIR_SEP_STR, aaf_path, essenceFileName, NULL);

  if (local_filepath == NULL) {
    error("Could not build filepath");
    goto err;
  }

  // debug( "AAF relative filepath : %s", local_filepath );

  if (access(local_filepath, F_OK) != -1) {
    // debug( "FOUND: %s", filepath );
    retpath = local_filepath;
    goto found;
  }

  free(local_filepath);
  local_filepath = NULL;

  /*
   * "<localPathToAAFfile>/<firstparentInOriginalEssencePath>/<essence.file>"
   */

  local_filepath =
      laaf_util_build_path(DIR_SEP_STR, aaf_path, relativeEssencePath, NULL);

  if (local_filepath == NULL) {
    error("Could not build filepath");
    goto err;
  }

  // debug( "AAF relative filepath : %s", local_filepath );

  if (access(local_filepath, F_OK) != -1) {
    // debug( "FOUND: %s", filepath );
    retpath = local_filepath;
    goto found;
  }

  free(local_filepath);
  local_filepath = NULL;

  // debug("File not found");

found:
  retpath = laaf_util_c99strdup(retpath);
  goto end;

err:
  retpath = NULL;

end:
  if (uri)
    uriFree(uri);

  if (uri_filepath)
    free(uri_filepath);

  if (local_filepath)
    free(local_filepath);

  if (aaf_path)
    free(aaf_path);

  return retpath;

  /*
          * AAFInfo --aaf-clips ../libaaf_testfiles/fonk_2.AAF
          file://localhost/Users/horlaprod/Music/Logic/fonk_2/Audio
     Files_1/fonk_2_3#04.wav

          * AAFInfo --aaf-clips
     ../libaaf_testfiles/ADP/ADP3_51-ST-MONO-NOBREAKOUT.aaf
          file:///C:/Users/Loviniou/Downloads/ChID-BLITS-EBU-Narration441-16b.wav

          * AAFInfo --aaf-clips ../libaaf_testfiles/ADP/ADP2_SEQ-FULL.aaf
          file://?/E:/Adrien/ADPAAF/Sequence A Rendu.mxf

          * AAFInfo --aaf-clips
     ../libaaf_testfiles/TEST-AVID_COMP2977052\ \ -\ \ OFF\ PODIUM\ ETAPE\ 2.aaf
          file:////C:/Users/mix_limo/Desktop/TEST2977052  -  OFF PODIUM
     ETAPE 2.aaf

          * AAFInfo --aaf-clips ../ardio/watchfolder/3572607_RUGBY_F_1_1.aaf
          file://10.87.230.71/mixage/DR2/Avid
     MediaFiles/MXF/1/3572607_RUGBY_F2_S65CFA3D0V.mxf

          * AAFInfo --aaf-clips ../libaaf_testfiles/ProTools/pt2MCC.aaf
          file:///_system/Users/horlaprod/pt2MCCzmhsFRHQgdgsTMQX.mxf
  */
}

int aafi_extract_audio_essence(AAF_Iface *aafi, aafiAudioEssence *audioEssence,
                               const char *outfilepath,
                               const wchar_t *forcedFileName) {
  int rc = 0;
  int reqlen = 0;
  FILE *fp = NULL;
  char *filename = NULL;
  char *filepath = NULL;

  unsigned char *data = NULL;
  uint64_t datasz = 0;

  if (audioEssence->is_embedded == 0) {
    warning("Audio essence is not embedded : nothing to extract");
    return -1;
  }

  /* Retrieve stream from CFB */

  cfb_getStream(aafi->aafd->cfbd, audioEssence->node, &data, &datasz);

  if (data == NULL) {
    error("Could not retrieve audio essence stream from CFB");
    goto err;
  }

  /* Build file path */

  reqlen =
      snprintf(NULL, 0, "%ls.%s",
               (forcedFileName != NULL) ? forcedFileName
                                        : audioEssence->unique_file_name,
               (audioEssence->type == AAFI_ESSENCE_TYPE_AIFC) ? AIFF_FILE_EXT
                                                              : WAV_FILE_EXT);

  if (reqlen < 0) {
    error("Failed to build filename");
    goto err;
  }

  int filenamelen = reqlen + 1;

  filename = malloc(filenamelen);

  if (filename == NULL) {
    error("Could not allocate memory : %s", strerror(errno));
    goto err;
  }

  rc = snprintf(filename, filenamelen, "%ls.%s",
                (forcedFileName != NULL) ? forcedFileName
                                         : audioEssence->unique_file_name,
                (audioEssence->type == AAFI_ESSENCE_TYPE_AIFC) ? AIFF_FILE_EXT
                                                               : WAV_FILE_EXT);

  if (rc < 0 || (unsigned)rc >= (unsigned)filenamelen) {
    error("Failed to build filename");
    goto err;
  }

  filepath = laaf_util_build_path(DIR_SEP_STR, outfilepath,
                                  laaf_util_clean_filename(filename), NULL);

  if (filepath == NULL) {
    error("Could not build filepath");
    goto err;
  }

  fp = fopen(filepath, "wb");

  if (fp == NULL) {
    error("Could not open '%s' for writing : %s", filepath, strerror(errno));
    goto err;
  }

  if (audioEssence->type == AAFI_ESSENCE_TYPE_PCM) {

    struct wavFmtChunk wavFmt;
    wavFmt.channels = audioEssence->channels;
    wavFmt.samples_per_sec = audioEssence->samplerate;
    wavFmt.bits_per_sample = audioEssence->samplesize;

    struct wavBextChunk wavBext;
    memset(&wavBext, 0x00, sizeof(wavBext));
    memcpy(wavBext.umid, audioEssence->sourceMobID, sizeof(aafMobID_t));
    if (audioEssence->mobSlotEditRate) {
      wavBext.time_reference =
          eu2sample(audioEssence->samplerate, audioEssence->mobSlotEditRate,
                    audioEssence->timeReference);
    }

    if (datasz >= (uint32_t)-1) {
      // TODO RF64 support ?
      error("Audio essence is bigger than maximum wav file size (2^32 bytes) : "
            "%" PRIu64 " bytes",
            datasz);
      goto err;
    }

    if (riff_writeWavFileHeader(fp, &wavFmt, &wavBext, (uint32_t)datasz,
                                aafi->dbg) < 0) {
      error("Could not write wav audio header : %s", filepath);
      goto err;
    }
  }

  uint64_t writtenBytes = fwrite(data, sizeof(unsigned char), datasz, fp);

  if (writtenBytes < datasz) {
    error("Could not write audio file (%" PRIu64
          " bytes written out of %" PRIu64 " bytes) : %s",
          writtenBytes, datasz, filepath);
    goto err;
  }

  audioEssence->usable_file_path =
      malloc((strlen(filepath) + 1) * sizeof(wchar_t));

  if (audioEssence->usable_file_path == NULL) {
    error("Could not allocate memory : %s", strerror(errno));
    goto err;
  }

  reqlen = swprintf(audioEssence->usable_file_path, strlen(filepath) + 1,
                    L"%" WPRIs, filepath);

  if (reqlen < 0) {
    error("Failed setting usable_file_path");
    goto err;
  }

  rc = 0;
  goto end;

err:
  rc = -1;

end:
  if (filename)
    free(filename);

  if (filepath)
    free(filepath);

  if (data)
    free(data);

  if (fp)
    fclose(fp);

  return rc;
}

int aafi_parse_audio_summary(AAF_Iface *aafi, aafiAudioEssence *audioEssence) {
  // laaf_util_dump_hex( audioEssence->summary->val, audioEssence->summary->len
  // );

  int rc = 0;
  char *externalFilePath = NULL;
  FILE *fp = NULL;

  struct RIFFAudioFile RIFFAudioFile;

  if (audioEssence->is_embedded) {

    if (audioEssence->summary == NULL) {
      warning("TODO: Audio essence has no summary. TODO: Should try essence "
              "data stream ?");
      goto err;
    }

    /*
     *	Adobe Premiere Pro, embedded mp3/mp4 files converted to PCM/AIFF on
     *export, AAFClassID_AIFCDescriptor, 'COMM' is valid.
     *	______________________________ Hex Dump ______________________________
     *
     *	46 4f 52 4d 00 00 00 32  41 49 46 43 43 4f 4d 4d  |  FORM...2 AIFCCOMM
     *	00 00 00 26 00 01 00 00  00 00 00 10 40 0e bb 80  |  ........ ........
     *	00 00 00 00 00 00 4e 4f  4e 45 0e 4e 6f 74 20 43  |  ......NO NE.Not.C
     *	6f 6d 70 72 65 73 73 65  64 00                    |  ompresse d.
     *	______________________________________________________________________
     */

    // laaf_util_dump_hex( audioEssence->summary->val,
    // audioEssence->summary->len );

    rc = riff_parseAudioFile(&RIFFAudioFile, RIFF_PARSE_ONLY_HEADER,
                             &embeddedAudioDataReaderCallback,
                             audioEssence->summary->val,
                             &audioEssence->summary->len, aafi, aafi->dbg);

    if (rc < 0) {
      warning("TODO: Could not parse embedded essence summary. Should try "
              "essence data stream ?");
      goto err;
    }

    audioEssence->channels = RIFFAudioFile.channels;
    audioEssence->samplerate = RIFFAudioFile.sampleRate;
    audioEssence->samplesize = RIFFAudioFile.sampleSize;
    audioEssence->length = RIFFAudioFile.duration;
  } else {

    /* TODO: can external essence have audioEssence->summary too ? If mp3
     * (Resolve 18.5.aaf) ? */

    externalFilePath = aafi_locate_external_essence_file(
        aafi, audioEssence->original_file_path,
        aafi->ctx.options.media_location);

    if (externalFilePath == NULL) {
      error("Could not locate external audio essence file '%ls'",
            audioEssence->original_file_path);
      return -1;
    }

    audioEssence->usable_file_path =
        malloc((strlen(externalFilePath) + 1) * sizeof(wchar_t));

    if (audioEssence->usable_file_path == NULL) {
      error("Could not allocate memory : %s", strerror(errno));
      goto err;
    }

    rc = swprintf(audioEssence->usable_file_path, strlen(externalFilePath) + 1,
                  L"%" WPRIs, externalFilePath);

    if (rc < 0) {
      error("Failed setting usable_file_path");
      goto err;
    }

    if (laaf_util_fop_is_wstr_fileext(audioEssence->original_file_path,
                                      L"wav") ||
        laaf_util_fop_is_wstr_fileext(audioEssence->original_file_path,
                                      L"wave") ||
        laaf_util_fop_is_wstr_fileext(audioEssence->original_file_path,
                                      L"aif") ||
        laaf_util_fop_is_wstr_fileext(audioEssence->original_file_path,
                                      L"aiff") ||
        laaf_util_fop_is_wstr_fileext(audioEssence->original_file_path,
                                      L"aifc")) {
      fp = fopen(externalFilePath, "rb");

      if (fp == NULL) {
        error("Could not open external audio essence file for reading : %s",
              externalFilePath);
        goto err;
      }

      rc = riff_parseAudioFile(&RIFFAudioFile, RIFF_PARSE_ONLY_HEADER,
                               &externalAudioDataReaderCallback, fp,
                               externalFilePath, aafi, aafi->dbg);

      if (rc < 0) {
        error("Failed parsing external audio essence file : %s",
              externalFilePath);
        goto err;
      }

      audioEssence->channels = RIFFAudioFile.channels;
      audioEssence->samplerate = RIFFAudioFile.sampleRate;
      audioEssence->samplesize = RIFFAudioFile.sampleSize;
      audioEssence->length = RIFFAudioFile.duration;
    }
  }

  rc = 0;
  goto end;

err:
  rc = -1;

end:
  if (fp)
    fclose(fp);

  if (externalFilePath)
    free(externalFilePath);

  return rc;
}

static size_t embeddedAudioDataReaderCallback(unsigned char *buf, size_t offset,
                                              size_t reqLen, void *user1,
                                              void *user2, void *user3) {
  unsigned char *data = user1;
  size_t datasz = *(size_t *)user2;
  AAF_Iface *aafi = (AAF_Iface *)user3;

  if (offset >= datasz) {
    error("Requested data starts beyond data length");
    return -1;
  }

  if (offset + reqLen >= datasz) {
    reqLen = datasz - (offset + reqLen);
  }

  memcpy(buf, data + offset, reqLen);

  return reqLen;
}

static size_t externalAudioDataReaderCallback(unsigned char *buf, size_t offset,
                                              size_t reqLen, void *user1,
                                              void *user2, void *user3) {
  FILE *fp = (FILE *)user1;
  const char *filename = (const char *)user2;
  AAF_Iface *aafi = (AAF_Iface *)user3;

  if (fseek(fp, offset, SEEK_SET) < 0) {
    error("Could not seek to %zu in file '%s' : %s", offset, filename,
          strerror(errno));
    return -1;
  }

  size_t read = fread(buf, sizeof(unsigned char), reqLen, fp);

  if (read < reqLen) {
    error("File read failed at %zu (expected %zu, read %zu) in file '%s' : %s",
          offset, reqLen, read, filename, strerror(errno));
    return -1;
  }

  return read;
}
