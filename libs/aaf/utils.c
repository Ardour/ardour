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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "aaf/utils.h"

#define BUILD_PATH_DEFAULT_BUF_SIZE 1024

int laaf_util_wstr_contains_nonlatin(const wchar_t *str) {
  for (size_t i = 0; str[i] != 0x0000; i++) {
    /* if char is out of the Basic Latin range */
    if (str[i] > 0xff) {
      return 1;
    }
  }

  return 0;
}

char *laaf_util_clean_filename(char *fname) {
  /*
   * sanitize file/dir name
   * https://stackoverflow.com/a/31976060
   */
  size_t len = strlen(fname);

  for (size_t i = 0; i < len; i++) {

    unsigned char c = fname[i];

    if (c == '/' || c == '<' || c == '>' || c == ':' || c == '"' || c == '|' ||
        c == '?' || c == '*' || c == '\\' || (c > 0 && c < 0x20)) {
      fname[i] = '_';
    }
  }

  /* windows filenames can't end with ' ' or '.' */
  for (int i = len - 1; i > 0; i--) {
    char c = fname[i];
    if (c != ' ' && c != '.') {
      break;
    }
    fname[i] = '_';
  }

  return fname;
}

const char *laaf_util_fop_get_file(const char *filepath) {
  if (filepath == NULL) {
    return NULL;
  }

  const char *end = filepath + strlen(filepath);

  while (end > filepath && !IS_DIR_SEP(*end)) {
    --end;
  }

  return (IS_DIR_SEP(*end)) ? end + 1 : end;
}

int laaf_util_fop_is_wstr_fileext(const wchar_t *filepath, const wchar_t *ext) {
  if (filepath == NULL) {
    return 0;
  }

  const wchar_t *end = filepath + wcslen(filepath);
  size_t extlen = 0;

  while (end > filepath && (*end) != '.') {
    --end;
    extlen++;
  }

  if ((*end) == '.') {
    end++;
    extlen--;
  }

  if (extlen != wcslen(ext)) {
    return 0;
  }

  // printf(" end: %ls    ext: %ls\n", end, ext );

  for (size_t i = 0; i < extlen; i++) {
    // printf("end: %c  !=  %c\n", *(end+i), *(ext+i));
    if (tolower(*(end + i)) != tolower(*(ext + i))) {
      return 0;
    }
  }

  return 1;
}

char *laaf_util_build_path(const char *sep, const char *first, ...) {
  char *str = malloc(BUILD_PATH_DEFAULT_BUF_SIZE);

  if (str == NULL) {
    return NULL;
  }

  size_t len = BUILD_PATH_DEFAULT_BUF_SIZE;
  size_t offset = 0;

  va_list args;

  if (!sep) {
    sep = DIR_SEP_STR;
  }

  int element_count = 0;

  va_start(args, first);

  const char *arg = first;

  do {

    int arglen = strlen(arg);
    int argstart = 0;
    int has_leading_sep = 0;

    /* trim leading DIR_SEP */
    for (int i = 0; arg[i] != 0x00; i++) {
      if (IS_DIR_SEP(arg[i])) {
        has_leading_sep = 1;
        argstart++;
      } else {
        break;
      }
    }

    /* trim trailing DIR_SEP */
    for (int i = arglen - 1; i >= argstart; i--) {
      if (IS_DIR_SEP(arg[i])) {
        arglen--;
      } else {
        break;
      }
    }

    /* TODO ? */
    if (element_count == 0 && has_leading_sep) {
    } else {
    }

    size_t reqlen =
        snprintf(NULL, 0, "%.*s", arglen - argstart, arg + argstart) + 1;

    if (offset + reqlen >= len) {

      reqlen = ((offset + reqlen) > (len + BUILD_PATH_DEFAULT_BUF_SIZE))
                   ? (offset + reqlen)
                   : (len + BUILD_PATH_DEFAULT_BUF_SIZE);

      char *tmp = realloc(str, (offset + reqlen));

      if (tmp) {
        str = tmp;
        len = (offset + reqlen);
      } else {
        free(str);
        return NULL;
      }
    }

    offset += snprintf(
        str + offset, len - offset, "%s%.*s",
        ((element_count == 0 && has_leading_sep) || (element_count > 0)) ? sep
                                                                         : "",
        arglen - argstart, arg + argstart);

    element_count++;

  } while ((arg = va_arg(args, char *)) != NULL);

  va_end(args);

  return str;
}

int laaf_util_snprintf_realloc(char **str, int *size, size_t offset,
                               const char *format, ...) {
  int tmpsize = 0;

  if (!size) {
    size = &tmpsize;
  }

  int retval, needed;
  va_list ap;

  va_start(ap, format);

  while (0 <= (retval =
                   vsnprintf((*str) + offset, (*size) - offset, format, ap)) &&
         (int64_t)((*size) - offset) < (needed = retval + 1)) {
    va_end(ap);

    *size *= 2;

    if ((int64_t)((*size) - offset) < needed)
      *size = needed + offset;

    char *p = realloc(*str, *size);

    if (p) {
      *str = p;
    } else {
      free(*str);
      *str = NULL;
      *size = 0;
      return -1;
    }

    va_start(ap, format);
  }

  va_end(ap);

  return retval;
}

int laaf_util_vsnprintf_realloc(char **str, int *size, int offset,
                                const char *fmt, va_list *args) {
  va_list args2, args3;

  va_copy(args2, *args);
  va_copy(args3, *args);

  int needed = vsnprintf(NULL, 0, fmt, args2) + 1;

  if (needed >= (*size) - offset) {

    char *p = realloc(*str, offset + needed);

    if (p) {
      *str = p;
      *size = offset + needed;
    } else {
      /* If realloc() fails, the original block is left untouched; it is not
       * freed or moved. */
      va_end(args2);
      va_end(args3);

      return -1;
    }
  }
  va_end(args2);

  int written = vsnprintf((*str) + offset, (*size) - offset, fmt, args3);

  va_end(args3);

  return written;
}

char *laaf_util_c99strdup(const char *src) {
  if (!src) {
    return NULL;
  }

  int len = 0;

  while (src[len]) {
    len++;
  }

  char *str = malloc(len + 1);

  if (!str)
    return NULL;

  char *p = str;

  while (*src) {
    *(p++) = *(src++);
  }

  *p = '\0';

  return str;
}

int laaf_util_dump_hex(const unsigned char *stream, size_t stream_sz,
                       char **buf, int *bufsz, int offset) {
  if (stream == NULL) {
    return -1;
  }

  int initialOffset = offset;
  uint32_t i = 0;

  char hex[49];
  char ascii[19];

  uint32_t count = 0;

  offset +=
      laaf_util_snprintf_realloc(buf, bufsz, offset,
                                 " ______________________________ Hex Dump "
                                 "______________________________\n\n");

  while (count < stream_sz) {

    uint32_t lineLen = (stream_sz - count) / 16;

    if (lineLen <= 0)
      lineLen = (stream_sz) % 16;
    else
      lineLen = 16;

    memset(&hex, 0x20, sizeof(hex));
    memset(&ascii, 0x00, sizeof(ascii));

    uint32_t linepos = 0;

    for (i = 0; i < lineLen; i++) {

      linepos += snprintf(&hex[linepos], sizeof(hex) - (linepos), "%02x%s",
                          *(const unsigned char *)(stream + count + i),
                          (i == 7) ? "  " : " ");

      if (i < 8) {
        if (isalnum(*(stream + count + i)))
          ascii[i] = *(const unsigned char *)(stream + count + i);
        else
          ascii[i] = '.';
      } else if (i > 8) {
        if (isalnum(*(stream + count + i)))
          ascii[i + 1] = *(const unsigned char *)(stream + count + i);
        else
          ascii[i + 1] = '.';
      } else {
        if (isalnum(*(stream + count + i))) {
          ascii[i] = ' ';
          ascii[i + 1] = *(const unsigned char *)(stream + count + i);
        } else {
          ascii[i] = ' ';
          ascii[i + 1] = '.';
        }
      }
    }

    /* Fill with blank the rest of the line */
    if (lineLen < 16) {
      for (i = linepos; i < 48; i++) {
        hex[linepos++] = 0x20;
      }
    }

    /* terminate  the line */
    hex[48] = 0x00;

    count += lineLen;

    offset += laaf_util_snprintf_realloc(buf, bufsz, offset, " %s  |  %s\n",
                                         hex, ascii);
  }

  offset += laaf_util_snprintf_realloc(buf, bufsz, offset,
                                       " ______________________________________"
                                       "________________________________\n\n");

  return offset - initialOffset; /* bytes written */
}
