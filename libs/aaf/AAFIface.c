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

/**
 * @file LibAAF/AAFIface/AAFIface.c
 * @brief AAF processing
 * @author Adrien Gesta-Fline
 * @version 0.1
 * @date 04 october 2017
 *
 * @ingroup AAFIface
 * @addtogroup AAFIface
 *
 * The AAFIface provides the actual processing of the AAF Objects in order to
 * show essences and clips in a simplified manner. Indeed, AAF has many
 * different ways to store data and metadata. Thus, the AAFIface is an
 * abstraction layer that provides a constant and unique representation method
 * of essences and clips.
 *
 *
 *
 * @{
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aaf/AAFIParser.h"
#include "aaf/AAFIface.h"
#include "aaf/debug.h"
#include "aaf/utils.h"

#ifdef _WIN32
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

#define debug(...)                                                             \
  _dbg(aafi->dbg, aafi, DEBUG_SRC_ID_AAF_IFACE, VERB_DEBUG, __VA_ARGS__)

#define warning(...)                                                           \
  _dbg(aafi->dbg, aafi, DEBUG_SRC_ID_AAF_IFACE, VERB_WARNING, __VA_ARGS__)

#define error(...)                                                             \
  _dbg(aafi->dbg, aafi, DEBUG_SRC_ID_AAF_IFACE, VERB_ERROR, __VA_ARGS__)

AAF_Iface *aafi_alloc(AAF_Data *aafd) {
  AAF_Iface *aafi = calloc(sizeof(AAF_Iface), sizeof(unsigned char));

  if (aafi == NULL) {
    return NULL;
  }

  aafi->dbg = laaf_new_debug();

  if (aafi->dbg == NULL) {
    return NULL;
  }

  aafi->Audio = calloc(sizeof(aafiAudio), sizeof(unsigned char));

  if (aafi->Audio == NULL) {
    return NULL;
  }

  aafi->Audio->Essences = NULL;
  aafi->Audio->samplerate = 0;
  aafi->Audio->samplesize = 0;
  aafi->Audio->Tracks = NULL;
  aafi->Audio->track_count = 0;
  aafi->Audio->length = 0;

  aafi->Video = calloc(sizeof(aafiVideo), sizeof(unsigned char));

  if (aafi->Video == NULL) {
    return NULL;
  }

  aafi->Video->Essences = NULL;
  aafi->Video->Tracks = NULL;
  aafi->Video->length = 0;

  if (aafd != NULL) {
    aafi->aafd = aafd;
  } else {
    aafi->aafd = aaf_alloc(aafi->dbg);
  }

  aafi->Markers = NULL;

  aafi->compositionName = NULL;

  aafi->ctx.is_inside_derivation_chain = 0;
  aafi->ctx.options.forbid_nonlatin_filenames = 0;
  aafi->ctx.options.trace = 0;

  return aafi;
}

void aafi_set_debug(AAF_Iface *aafi, verbosityLevel_e v, int ansicolor,
                    FILE *fp,
                    void (*callback)(struct dbg *dbg, void *ctxdata, int lib,
                                     int type, const char *srcfile,
                                     const char *srcfunc, int lineno,
                                     const char *msg, void *user),
                    void *user) {
  aafi->dbg->verb = v;
  aafi->dbg->ansicolor = ansicolor;
  aafi->dbg->fp = fp;

  if (callback) {
    aafi->dbg->debug_callback = callback;
  }

  if (user) {
    aafi->dbg->user = user;
  }
}

int aafi_set_option_int(AAF_Iface *aafi, const char *optname, int val) {

  if (strcmp(optname, "trace") == 0) {
    aafi->ctx.options.trace = val;
    return 0;
  } else if (strcmp(optname, "trace_meta") == 0) {
    aafi->ctx.options.trace_meta = val;
    return 0;
  } else if (strcmp(optname, "forbid_nonlatin_filenames") == 0) {
    aafi->ctx.options.forbid_nonlatin_filenames = val;
    return 0;
  } else if (strcmp(optname, "protools") == 0) {
    aafi->ctx.options.protools = val;
    return 0;
  } else if (strcmp(optname, "resolve") == 0) {
    aafi->ctx.options.resolve = val;
    return 0;
  }

  return 1;
}

int aafi_set_option_str(AAF_Iface *aafi, const char *optname, const char *val) {

  if (strcmp(optname, "media_location") == 0) {

    if (aafi->ctx.options.media_location) {
      free(aafi->ctx.options.media_location);
    }

    aafi->ctx.options.media_location = (val) ? laaf_util_c99strdup(val) : NULL;

    return 0;
  } else if (strcmp(optname, "dump_class_aaf_properties") == 0) {

    if (aafi->ctx.options.dump_class_aaf_properties) {
      free(aafi->ctx.options.dump_class_aaf_properties);
      aafi->ctx.options.dump_class_aaf_properties = NULL;
    }

    if (val == NULL)
      return 0;

    aafi->ctx.options.dump_class_aaf_properties =
        malloc((strlen(val) + 1) * sizeof(wchar_t));

    if (aafi->ctx.options.dump_class_aaf_properties == NULL) {
      return -1;
    }

    swprintf(aafi->ctx.options.dump_class_aaf_properties, strlen(val) + 1,
             L"%" WPRIs, val);

    return 0;
  } else if (strcmp(optname, "dump_class_raw_properties") == 0) {

    if (aafi->ctx.options.dump_class_raw_properties) {
      free(aafi->ctx.options.dump_class_raw_properties);
      aafi->ctx.options.dump_class_raw_properties = NULL;
    }

    if (val == NULL)
      return 0;

    aafi->ctx.options.dump_class_raw_properties =
        malloc((strlen(val) + 1) * sizeof(wchar_t));

    if (aafi->ctx.options.dump_class_raw_properties == NULL) {
      return -1;
    }

    swprintf(aafi->ctx.options.dump_class_raw_properties, strlen(val) + 1,
             L"%" WPRIs, val);

    return 0;
  }

  return 1;
}

void aafi_release(AAF_Iface **aafi) {
  if (*aafi == NULL)
    return;

  aaf_release(&(*aafi)->aafd);

  if ((*aafi)->compositionName != NULL) {
    free((*aafi)->compositionName);
  }

  if ((*aafi)->Comments) {
    aafi_freeUserComments(&((*aafi)->Comments));
  }

  if ((*aafi)->Audio != NULL) {

    if ((*aafi)->Audio->Tracks != NULL) {
      aafi_freeAudioTracks(&(*aafi)->Audio->Tracks);
    }

    if ((*aafi)->Audio->Essences != NULL) {
      aafi_freeAudioEssences(&(*aafi)->Audio->Essences);
    }

    free((*aafi)->Audio);
  }

  if ((*aafi)->Video != NULL) {

    if ((*aafi)->Video->Tracks != NULL) {
      aafi_freeVideoTracks(&(*aafi)->Video->Tracks);
    }

    if ((*aafi)->Video->Essences != NULL) {
      aafi_freeVideoEssences(&(*aafi)->Video->Essences);
    }

    free((*aafi)->Video);
  }

  if ((*aafi)->Markers) {
    aafi_freeMarkers(&(*aafi)->Markers);
  }

  if ((*aafi)->ctx.options.dump_class_aaf_properties) {
    free((*aafi)->ctx.options.dump_class_aaf_properties);
  }

  if ((*aafi)->ctx.options.dump_class_raw_properties) {
    free((*aafi)->ctx.options.dump_class_raw_properties);
  }

  if ((*aafi)->ctx.options.media_location) {
    free((*aafi)->ctx.options.media_location);
  }

  if ((*aafi)->Timecode != NULL) {
    free((*aafi)->Timecode);
  }

  if ((*aafi)->dbg) {
    laaf_free_debug((*aafi)->dbg);
  }

  free(*aafi);

  *aafi = NULL;
}

int aafi_load_file(AAF_Iface *aafi, const char *file) {
  if (aaf_load_file(aafi->aafd, file)) {
    error("Could not load file : %s\n", file);
    return 1;
  }

  aafi_retrieveData(aafi);

  return 0;
}

aafiTransition *aafi_get_fadein(aafiTimelineItem *audioItem) {

  if (audioItem->prev != NULL && audioItem->prev->type == AAFI_TRANS) {
    aafiTransition *Trans = audioItem->prev->data;

    if (Trans->flags & AAFI_TRANS_FADE_IN)
      return Trans;
  }

  return NULL;
}

aafiTransition *aafi_get_fadeout(aafiTimelineItem *audioItem) {

  if (audioItem->next != NULL && audioItem->next->type == AAFI_TRANS) {
    aafiTransition *Trans = audioItem->next->data;

    if (Trans->flags & AAFI_TRANS_FADE_OUT)
      return Trans;
  }

  return NULL;
}

aafiTransition *aafi_get_xfade(aafiTimelineItem *audioItem) {
  if (audioItem->prev != NULL && audioItem->prev->type == AAFI_TRANS) {
    aafiTransition *Trans = audioItem->prev->data;

    if (Trans->flags & AAFI_TRANS_XFADE)
      return Trans;
  }

  return NULL;
}

aafiMarker *aafi_newMarker(AAF_Iface *aafi, aafRational_t *editRate,
                           aafPosition_t start, aafPosition_t length,
                           wchar_t *name, wchar_t *comment,
                           uint16_t *(RGBColor[3])) {
  aafiMarker *marker = malloc(sizeof(aafiMarker));

  marker->edit_rate = editRate;
  marker->start = start;
  marker->length = length;

  marker->name = name;
  marker->comment = comment;

  marker->prev = NULL;
  marker->next = NULL;

  if (RGBColor) {
    marker->RGBColor[0] = (*RGBColor)[0];
    marker->RGBColor[1] = (*RGBColor)[1];
    marker->RGBColor[2] = (*RGBColor)[2];
  }

  if (aafi->Markers != NULL) {

    aafiMarker *tmp = aafi->Markers;

    for (; tmp != NULL; tmp = tmp->next)
      if (tmp->next == NULL)
        break;

    tmp->next = marker;
    marker->prev = marker;
  } else {
    aafi->Markers = marker;
    marker->prev = NULL;
  }

  return marker;
}

void aafi_freeMarkers(aafiMarker **Markers) {
  aafiMarker *marker = NULL;
  aafiMarker *nextMarker = NULL;

  for (marker = (*Markers); marker != NULL; marker = nextMarker) {

    nextMarker = marker->next;

    if (marker->name)
      free(marker->name);

    if (marker->comment)
      free(marker->comment);

    free(marker);
  }

  *Markers = NULL;
}

aafiTimelineItem *aafi_newTimelineItem(AAF_Iface *aafi, void *track,
                                       int itemType) {

  aafiTimelineItem *item = NULL;

  if (itemType == AAFI_AUDIO_CLIP) {

    item = calloc(sizeof(aafiTimelineItem), sizeof(char));

    if (item == NULL) {
      error("%s.", strerror(errno));
      return NULL;
    }

    item->type = AAFI_AUDIO_CLIP;

    item->data = calloc(sizeof(aafiAudioClip), sizeof(char));

    aafiAudioClip *audioClip = item->data;

    audioClip->track = (aafiAudioTrack *)track;
    audioClip->Item = item;
  } else if (itemType == AAFI_VIDEO_CLIP) {

    item = calloc(sizeof(aafiTimelineItem), sizeof(char));

    if (item == NULL) {
      error("%s.", strerror(errno));
      return NULL;
    }

    item->type = AAFI_VIDEO_CLIP;

    item->data = calloc(sizeof(aafiVideoClip), sizeof(char));

    aafiVideoClip *videoClip = item->data;

    videoClip->track = (aafiVideoTrack *)track;
  } else if (itemType == AAFI_TRANS) {

    item = calloc(sizeof(aafiTimelineItem), sizeof(char));

    if (item == NULL) {
      error("%s.", strerror(errno));
      return NULL;
    }

    item->type = AAFI_TRANS;

    item->data = calloc(sizeof(aafiTransition), sizeof(char));
  }

  if (itemType == AAFI_AUDIO_CLIP || itemType == AAFI_TRANS) {

    if (track != NULL) {

      /* Add to track's item list */

      if (((aafiAudioTrack *)track)->Items != NULL) {

        aafiTimelineItem *tmp = ((aafiAudioTrack *)track)->Items;

        for (; tmp != NULL; tmp = tmp->next)
          if (tmp->next == NULL)
            break;

        tmp->next = item;
        item->prev = tmp;
      } else {
        ((aafiAudioTrack *)track)->Items = item;
        item->prev = NULL;
      }
    }
  } else if (itemType == AAFI_VIDEO_CLIP) {

    if (track != NULL) {

      /* Add to track's item list */

      if (((aafiVideoTrack *)track)->Items != NULL) {

        aafiTimelineItem *tmp = ((aafiVideoTrack *)track)->Items;

        for (; tmp != NULL; tmp = tmp->next)
          if (tmp->next == NULL)
            break;

        tmp->next = item;
        item->prev = tmp;
      } else {
        ((aafiVideoTrack *)track)->Items = item;
        item->prev = NULL;
      }
    }
  }

  return item;
}

int aafi_removeTimelineItem(AAF_Iface *aafi, aafiTimelineItem *item) {

  if (item->prev != NULL) {
    item->prev->next = item->next;
  }

  if (item->next != NULL) {
    item->next->prev = item->prev;
  }

  aafiAudioTrack *audioTrack = NULL;

  foreach_audioTrack(audioTrack, aafi) {
    if (audioTrack->Items == item) {
      audioTrack->Items = item->next;
    }
  }

  aafi_freeTimelineItem(&item);

  return 0;
}

void aafi_freeAudioGain(aafiAudioGain *gain) {
  if (gain == NULL) {
    return;
  }

  if (gain->time != NULL) {
    free(gain->time);
  }

  if (gain->value != NULL) {
    free(gain->value);
  }

  free(gain);
}

void aafi_freeAudioPan(aafiAudioPan *pan) {
  aafi_freeAudioGain((aafiAudioGain *)pan);
}

void aafi_freeAudioClip(aafiAudioClip *audioClip) {
  if (audioClip->gain != NULL) {
    aafi_freeAudioGain(audioClip->gain);
  }

  if (audioClip->automation != NULL) {
    aafi_freeAudioGain(audioClip->automation);
  }
}

void aafi_freeTimelineItem(aafiTimelineItem **item) {
  if ((*item)->type == AAFI_TRANS) {
    aafi_freeTransition((aafiTransition *)((*item)->data));
    free((*item)->data);
  } else if ((*item)->type == AAFI_AUDIO_CLIP) {
    aafi_freeAudioClip((aafiAudioClip *)((*item)->data));
    free((*item)->data);
  } else if ((*item)->type == AAFI_VIDEO_CLIP) {
    free((*item)->data);
  }

  free(*item);

  *item = NULL;
}

void aafi_freeTimelineItems(aafiTimelineItem **items) {
  aafiTimelineItem *item = NULL;
  aafiTimelineItem *nextItem = NULL;

  for (item = (*items); item != NULL; item = nextItem) {
    nextItem = item->next;
    aafi_freeTimelineItem(&item);
  }

  *items = NULL;
}

aafiUserComment *aafi_newUserComment(AAF_Iface *aafi,
                                     aafiUserComment **CommentList) {

  aafiUserComment *UserComment = calloc(sizeof(aafiUserComment), 1);

  if (UserComment == NULL) {
    error("%s.", strerror(errno));
    return NULL;
  }

  if (CommentList != NULL) {
    UserComment->next = *CommentList;
    *CommentList = UserComment;
  } else {
    *CommentList = UserComment;
  }

  return UserComment;
}

void aafi_freeUserComments(aafiUserComment **CommentList) {
  aafiUserComment *UserComment = *CommentList;
  aafiUserComment *tmp = NULL;

  while (UserComment != NULL) {

    tmp = UserComment;
    UserComment = UserComment->next;

    if (tmp->name != NULL) {
      free(tmp->name);
    }

    if (tmp->text != NULL) {
      free(tmp->text);
    }

    free(tmp);
  }

  *CommentList = NULL;
}

void aafi_freeTransition(aafiTransition *Transition) {
  if (Transition->value_a != NULL) {
    free(Transition->value_a);
  }

  if (Transition->value_b != NULL) {
    free(Transition->value_b);
  }

  if (Transition->time_a != NULL) {
    free(Transition->time_a);
  }

  if (Transition->time_b != NULL) {
    free(Transition->time_b);
  }
}

aafiAudioTrack *aafi_newAudioTrack(AAF_Iface *aafi) {
  aafiAudioTrack *track = calloc(sizeof(aafiAudioTrack), sizeof(unsigned char));

  if (track == NULL) {
    error("%s.", strerror(errno));
    return NULL;
  }

  track->Audio = aafi->Audio;
  track->format = AAFI_TRACK_FORMAT_NOT_SET;
  track->pan = NULL;
  track->gain = NULL;
  track->current_pos = 0;
  track->next = NULL;

  /* Add to track list */

  if (aafi->Audio->Tracks != NULL) {

    aafiAudioTrack *tmp = aafi->Audio->Tracks;

    for (; tmp != NULL; tmp = tmp->next)
      if (tmp->next == NULL)
        break;

    tmp->next = track;
  } else {
    aafi->Audio->Tracks = track;
  }

  return track;
}

void aafi_freeAudioTracks(aafiAudioTrack **tracks) {
  if (*(tracks) == NULL) {
    return;
  }

  aafiAudioTrack *track = NULL;
  aafiAudioTrack *nextTrack = NULL;

  for (track = (*tracks); track != NULL; track = nextTrack) {

    nextTrack = track->next;

    if (track->name != NULL) {
      free(track->name);
    }

    if (track->gain != NULL) {
      aafi_freeAudioGain(track->gain);
    }

    if (track->pan != NULL) {
      aafi_freeAudioPan(track->pan);
    }

    if (track->Items != NULL) {
      aafi_freeTimelineItems(&(track->Items));
    }

    free(track);
  }

  *tracks = NULL;
}

aafiVideoTrack *aafi_newVideoTrack(AAF_Iface *aafi) {
  aafiVideoTrack *track = calloc(sizeof(aafiVideoTrack), sizeof(unsigned char));

  if (track == NULL) {
    error("%s.", strerror(errno));
    return NULL;
  }

  track->Video = aafi->Video;
  track->current_pos = 0;
  track->next = NULL;

  /* Add to track list */

  if (aafi->Video->Tracks != NULL) {

    aafiVideoTrack *tmp = aafi->Video->Tracks;

    for (; tmp != NULL; tmp = tmp->next)
      if (tmp->next == NULL)
        break;

    tmp->next = track;
  } else {
    aafi->Video->Tracks = track;
  }

  return track;
}

void aafi_freeVideoTracks(aafiVideoTrack **tracks) {
  if (*(tracks) == NULL) {
    return;
  }

  aafiVideoTrack *track = NULL;
  aafiVideoTrack *nextTrack = NULL;

  for (track = (*tracks); track != NULL; track = nextTrack) {

    nextTrack = track->next;

    if (track->name != NULL) {
      free(track->name);
    }

    if (track->Items != NULL) {
      aafi_freeTimelineItems(&(track->Items));
    }

    free(track);
  }

  *tracks = NULL;
}

aafiAudioEssence *aafi_newAudioEssence(AAF_Iface *aafi) {
  aafiAudioEssence *audioEssence =
      calloc(sizeof(aafiAudioEssence), sizeof(char));

  if (audioEssence == NULL) {
    error("%s.", strerror(errno));
    return NULL;
  }

  audioEssence->next = aafi->Audio->Essences;

  audioEssence->original_file_path = NULL;
  audioEssence->usable_file_path = NULL;
  audioEssence->file_name = NULL;
  audioEssence->unique_file_name = NULL;
  audioEssence->clip_count = 0;
  audioEssence->user = NULL;

  aafi->Audio->Essences = audioEssence;

  return audioEssence;
}

void aafi_freeAudioEssences(aafiAudioEssence **audioEssence) {
  if (*(audioEssence) == NULL) {
    return;
  }

  aafiAudioEssence *nextAudioEssence = NULL;

  for (; (*audioEssence) != NULL; *audioEssence = nextAudioEssence) {

    nextAudioEssence = (*audioEssence)->next;

    if ((*audioEssence)->original_file_path != NULL) {
      free((*audioEssence)->original_file_path);
    }

    if ((*audioEssence)->usable_file_path != NULL) {
      free((*audioEssence)->usable_file_path);
    }

    if ((*audioEssence)->file_name != NULL) {
      free((*audioEssence)->file_name);
    }

    if ((*audioEssence)->unique_file_name != NULL) {
      free((*audioEssence)->unique_file_name);
    }

    free(*audioEssence);
  }

  *audioEssence = NULL;
}

aafiVideoEssence *aafi_newVideoEssence(AAF_Iface *aafi) {
  aafiVideoEssence *videoEssence =
      calloc(sizeof(aafiVideoEssence), sizeof(char));

  if (videoEssence == NULL) {
    error("%s.", strerror(errno));
    return NULL;
  }

  videoEssence->next = aafi->Video->Essences;

  videoEssence->original_file_path = NULL;
  videoEssence->usable_file_path = NULL;
  videoEssence->file_name = NULL;
  videoEssence->unique_file_name = NULL;

  aafi->Video->Essences = videoEssence;

  return videoEssence;
}

void aafi_freeVideoEssences(aafiVideoEssence **videoEssence) {
  if (*(videoEssence) == NULL) {
    return;
  }

  aafiVideoEssence *nextVideoEssence = NULL;

  for (; (*videoEssence) != NULL; *videoEssence = nextVideoEssence) {

    nextVideoEssence = (*videoEssence)->next;

    if ((*videoEssence)->original_file_path != NULL) {
      free((*videoEssence)->original_file_path);
    }

    if ((*videoEssence)->usable_file_path != NULL) {
      free((*videoEssence)->usable_file_path);
    }

    if ((*videoEssence)->file_name != NULL) {
      free((*videoEssence)->file_name);
    }

    if ((*videoEssence)->unique_file_name != NULL) {
      free((*videoEssence)->unique_file_name);
    }

    free(*videoEssence);
  }

  *videoEssence = NULL;
}

/**
 * @}
 */
