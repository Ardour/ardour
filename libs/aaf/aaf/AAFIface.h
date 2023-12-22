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

#ifndef __AAFIface_h__
#define __AAFIface_h__

/**
 * @file LibAAF/AAFIface/AAFIface.h
 * @brief AAF processing
 * @author Adrien Gesta-Fline
 * @version 0.1
 * @date 04 october 2017
 *
 * @ingroup AAFIface
 * @addtogroup AAFIface
 * @{
 * @brief Abstraction layer to interpret the Objects/Class and retrieve data.
 */

#include "aaf/AAFCore.h"
#include "aaf/AAFTypes.h"

enum aafiEssenceType {
  AAFI_ESSENCE_TYPE_PCM = 0x01,
  AAFI_ESSENCE_TYPE_WAVE = 0x02,
  AAFI_ESSENCE_TYPE_AIFC = 0x03,
  AAFI_ESSENCE_TYPE_BWAV = 0x04,
};

/**
 * Flags for aafiAudioGain.flags.
 */

typedef enum aafiAudioGain_e {
  AAFI_AUDIO_GAIN_CONSTANT = 1 << 0, // 0x0001
  AAFI_AUDIO_GAIN_VARIABLE = 1 << 1, // 0x0002

} aafiAudioGain_e;

#define AAFI_AUDIO_GAIN_MASK                                                   \
  (AAFI_AUDIO_GAIN_CONSTANT | AAFI_AUDIO_GAIN_VARIABLE)

/**
 * Flags for aafiTransition.flags.
 */

typedef enum aafiTransition_e {
  AAFI_TRANS_SINGLE_CURVE = 1 << 4, // 0x0010
  AAFI_TRANS_TWO_CURVE = 1 << 5,    // 0x0020

  AAFI_TRANS_FADE_IN = 1 << 6,  // 0x0040
  AAFI_TRANS_FADE_OUT = 1 << 7, // 0x0080
  AAFI_TRANS_XFADE = 1 << 8,    // 0x0100

} aafiTransition_e;

#define AAFI_TRANS_CURVE_COUNT_MASK                                            \
  (AAFI_TRANS_SINGLE_CURVE | AAFI_TRANS_TWO_CURVE)

#define AAFI_TRANS_FADE_MASK                                                   \
  (AAFI_TRANS_FADE_IN | AAFI_TRANS_FADE_OUT | AAFI_TRANS_XFADE)

/**
 * Flags for aafiTransition.flags and aafiAudioGain.flags
 */

typedef enum aafiInterpolation_e {
  AAFI_INTERPOL_NONE = 1 << 10,     // 0x0400
  AAFI_INTERPOL_LINEAR = 1 << 11,   // 0x0800
  AAFI_INTERPOL_LOG = 1 << 12,      // 0x1000
  AAFI_INTERPOL_CONSTANT = 1 << 13, // 0x2000
  AAFI_INTERPOL_POWER = 1 << 14,    // 0x4000
  AAFI_INTERPOL_BSPLINE = 1 << 15,  // 0x8000

} aafiInterpolation_e;

#define AAFI_INTERPOL_MASK                                                     \
  (AAFI_INTERPOL_NONE | AAFI_INTERPOL_LINEAR | AAFI_INTERPOL_LOG |             \
   AAFI_INTERPOL_CONSTANT | AAFI_INTERPOL_POWER | AAFI_INTERPOL_BSPLINE)

/**
 * Specifies a Transition that can be a fade in, a fade out or a Cross fade, and
 * that can have one or two curves.
 *
 * With a single curve (AAFI_TRANS_SINGLE_CURVE), the same curve is mirrored and
 * applied as fade in and fade out to obtain a cross fade.
 *
 * Having two curves (AAFI_TRANS_TWO_CURVE) allows a cross fade to have one
 * curve per fade.
 *
 * A transition should have at least two points, one at time zero and one at
 * time 1.
 * TODO To finish
 */

typedef struct aafiTransition {
  /**
   * Should hold the transition type (either single param or two param),
   * the transition fade type (in, out, x) and the interpolation used.
   */

  int flags;

  /**
   * Length of the transition, in edit units.
   */

  aafPosition_t len;

  /**
   * The cut point. In the case the transition is removed or cannot be played,
   * the cut point specifies where in the transition, the preceding segment
   * should end and where the following segment should start.
   */

  aafPosition_t cut_pt;

  /**
   * Points count for the single curve, or the first one of the two. This
   * specifies both the number of points (time/value) in the transition curve,
   * and consequently the size of time_a[] and value_a[] arrays.
   */

  int pts_cnt_a;

  /**
   * Array of time points, where the corresponding level value should apply
   * either to the single curve, or to the first one of the two.
   */

  aafRational_t *time_a;

  /**
   * Multiplier level values, each one applying at the corresponding indexed
   * time for either the single curve, or the first one of the two. The interval
   * between two points shall be calculated using the specified interpolation.
   */

  aafRational_t *value_a;

  /**
   * Points count for the second curve, only when Transition has the
   * AAFI_TRANS_TWO_CURVE flag. This specifies both the number of points
   * (time/value) in the transition curve, and consequently the size of time_b[]
   * and value_b[] arrays.
   */

  int pts_cnt_b;

  /**
   * Array of time points, where the corresponding level value should apply to
   * the second curve. Used only if Transition has the AAFI_TRANS_TWO_CURVE
   * flag.
   */

  aafRational_t **time_b;

  /**
   * Multiplier level values, each one applying at the corresponding indexed
   * time. The interval between two points shall be calculated using the
   * specified interpolation. Used only if Transitions has the
   * AAFI_TRANS_TWO_CURVE flag.
   */

  aafRational_t **value_b;

} aafiTransition;

/**
 * Specifies a Gain to apply either to a Clip (aafiAudioClip.gain) or to an
 * entire Track (aafiAudioTrack.gain), that is to all the Clips contained by
 * that Track.
 *
 * A Gain can be of to types :
 *
 * 	* Constant (AAFI_AUDIO_GAIN_CONSTANT) : A Constant gain specifies a
 * single value as a multiplier to be applied to the Clip or Track.
 *
 * 	* Variable (AAFI_AUDIO_GAIN_VARIABLE) : A Variable gain specifies
 * multiple points ( time / value ) that form all together the automation curve.
 * The values between two points are calculated by interpolating between the two
 * values.
 *
 * Both the Gain type and the interpolation mode are specified in the
 * aafiAudioGain.flags with the values from aafiAudioGain_e and
 * aafiInterpolation_e.
 *
 * In the case of a Constant Gain, the single multiplier value should be
 * retrieved from aafiAudioGain.value[0].
 */

typedef struct aafiAudioGain {
  /**
   * Should hold the gain type (either Constant or Variable), and if it is
   * Variable, the interpolation used to calculate the values between two time
   * points.
   */

  uint16_t flags; // Type : Constant (single multiplier for entire clip) or
                  //		  Variable (automation)
                  // Interpolation : Linear, Log, Constant, Power, BSpline

  /**
   * Points count. This specifies both the number of points (time/value) in the
   * gain automation, and is consequently the size of time[] and value[] arrays.
   */

  int64_t pts_cnt;

  /**
   * Array of time points, where the corresponding level value should apply.
   */

  aafRational_t *time;

  /**
   * Multiplier level values, each one applying at the corresponding indexed
   * time. The interval between two points shall be calculated using the
   * specified interpolation.
   */

  aafRational_t *value;

} aafiAudioGain;

typedef struct aafiAudioGain aafiAudioPan;

typedef struct aafiAudioEssence {

  wchar_t *original_file_path; // NetworkLocator::URLString the original URI
                               // hold in AAF
  wchar_t *usable_file_path; // Holds a real usable file path, once an embedded
                             // essence has been extracted, or once en external
                             // essence has been found.
  wchar_t *file_name; // MasterMob::Name the original file name. Might be NULL
                      // if MasterMob has no name. One should always use
                      // unique_file_name which is guaranted to be set.
  wchar_t *unique_file_name; // unique name generated from file_name. Sometimes,
                             // multiple files share the same names so this
                             // unique name should be used on export.

  uint16_t clip_count; // number of clips with this essence

  /* total samples for 1 channel (no matter channel count). (duration /
   * sampleRate) = duration in seconds */
  uint64_t length; // Length of Essence Data

  cfbNode *node; // The node holding the audio stream if embedded

  aafMobID_t
      *sourceMobID; // Holds the SourceMob Mob::ID references this EssenceData
  uint32_t sourceMobSlotID; // SlotID of the MobSlot inside MasterMob
                            // (CompoMob's Sequence SourceClip::SourceMobSlotID)
  aafMobID_t *masterMobID;  // Holds the MasterMob Mob::ID (used by CompoMob's
                            // Sequence SourceClip::SourceID)
  uint32_t masterMobSlotID; // SlotID of the MobSlot inside MasterMob
                            // (CompoMob's Sequence SourceClip::SourceMobSlotID)

  aafObject *SourceMob;

  enum aafiEssenceType
      type; // depends on PCMDescriptor WAVEDescriptor AIFCDescriptor

  uint8_t is_embedded;

  aafProperty *summary; // WAVEDescriptor AIFCDescriptor

  // uint32_t       format;
  uint32_t samplerate;
  int16_t samplesize;
  int16_t channels;

  aafRational_t *mobSlotEditRate;

  // BWF BEXT chunk data
  char description[256];
  char originator[32]; // could be set with header::ProductName
  char originatorReference[32];
  uint64_t timeReference;       // SourceMob TimelineMobSlot::Origin
  unsigned char umid[64];       // SourceMob::MobID (32 Bytes, basic form)
  char originationDate[10 + 1]; // SourceMob::CreationDate
  char originationTime[8 + 1];  // SourceMob::CreationTime

  void *user;
  // TODO peakEnveloppe
  struct aafiAudioEssence *next;

} aafiAudioEssence;

typedef struct aafiVideoEssence {

  wchar_t *original_file_path; // NetworkLocator::URLString should point to
                               // original essence file if external (and in some
                               // cases, points to the AAF itself if internal..)
  wchar_t *usable_file_path; // TODO, not that used.. to be tweaked.  ---- Holds
                             // the file path, once the essence has been
                             // exported, copied or linked.
  wchar_t *file_name;        // MasterMob::Name -> file name
  wchar_t *unique_file_name; // unique name generated from file_name. Sometimes,
                             // multiple files share the same names so this
                             // unique name should be used on export.

  uint64_t length; // Length of Essence Data

  cfbNode *node; // The node holding the audio stream if embedded

  aafRational_t *framerate;

  aafMobID_t
      *sourceMobID; // Holds the SourceMob Mob::ID references this EssenceData
  uint32_t sourceMobSlotID; // SlotID of the MobSlot inside MasterMob
                            // (CompoMob's Sequence SourceClip::SourceMobSlotID)
  aafMobID_t *masterMobID;  // Holds the MasterMob Mob::ID (used by CompoMob's
                            // Sequence SourceClip::SourceID)
  uint32_t masterMobSlotID; // SlotID of the MobSlot inside MasterMob
                            // (CompoMob's Sequence SourceClip::SourceMobSlotID)

  aafObject *SourceMob;

  // uint16_t       type;	// depends on PCMDescriptor WAVEDescriptor
  // AIFCDescriptor

  uint8_t is_embedded;

  aafProperty *summary;

  // TODO peakEnveloppe
  struct aafiVideoEssence *next;

} aafiVideoEssence;

/* forward declaration */
struct aafiAudioTrack;
struct aafiVideoTrack;

typedef struct aafiAudioClip {

  struct aafiAudioTrack *track;

  aafiAudioEssence *Essence;

  /*
   * Some editors (like Resolve) support automation attached to a clip AND a
   * fixed value clip gain
   */
  aafiAudioGain *gain;
  aafiAudioGain *automation;

  int mute;

  int channel_count;

  aafPosition_t
      pos; /* in edit unit, edit rate definition is aafiAudioTrack->edit_rate */

  aafPosition_t
      len; /* in edit unit, edit rate definition is aafiAudioTrack->edit_rate */

  /*
   * Start position in source file, set from SourceClip::StartTime
   *
   * « Specifies the offset from the origin of the referenced Mob MobSlot in
   edit units
   * determined by the SourceClip object’s context.
   *
   * A SourceClip’s StartTime and Length values are in edit units determined by
   the slot
   * owning the SourceClip.

   * Informative note: If the SourceClip references a MobSlot that specifies a
   different
   * edit rate than the MobSlot owning the SourceClip, the StartTime and Length
   are in
   * edit units of the slot owning the SourceClip, and not edit units of the
   referenced slot.»
   */

  aafPosition_t essence_offset; /* in edit unit, edit rate definition is
                                   aafiAudioTrack->edit_rate */

  struct aafiTimelineItem *Item; // Corresponding timeline item, currently used
                                 // in ardour to retrieve fades/x-fades

  aafMobID_t *masterMobID; // MobID of the associated MasterMob
                           // (PID_SourceReference_SourceID)

} aafiAudioClip;

typedef struct aafiVideoClip {
  struct aafiVideoTrack *track;

  aafiVideoEssence *Essence;

  aafPosition_t pos;

  aafPosition_t len;

  aafPosition_t essence_offset; // start position in the source file

  aafMobID_t *masterMobID; // MobID of the associated MasterMob
                           // (PID_SourceReference_SourceID)

} aafiVideoClip;

typedef enum aafiTimelineItem_type_e {
  AAFI_AUDIO_CLIP = 0x0001,
  AAFI_VIDEO_CLIP = 0x0002,
  AAFI_TRANS = 0x0003,

} aafiTimelineItem_type_e;

/**
 * This structure can old either an aafiAudioClip, aafiVideoClip or an
 * aafiTransition struct.
 */

typedef struct aafiTimelineItem {
  int type;

  struct aafiTimelineItem *next;
  struct aafiTimelineItem *prev;

  void *data; /* aafiTransition or aafiAudioClip or aafiVideoClip */

} aafiTimelineItem;

/**
 *
 */

typedef struct aafiTimecode {
  /**
   * Timecode start in EditUnit. (session start)
   */

  aafPosition_t start;

  /**
   * Timecode end in EditUnit. (session end)
   */

  aafPosition_t end;

  /**
   * Frame per second.
   */

  uint16_t fps;

  /**
   * Indicates whether the timecode is drop (True value) or nondrop (False
   * value)
   */

  uint8_t drop;

  /**
   * Keeps track of the TimelineMobSlot EditRate.
   * TODO do we care ?
   */

  aafRational_t *edit_rate;

} aafiTimecode;

/**
 * Values for aafiAudioTrack.format.
 */

typedef enum aafiTrackFormat_e {
  AAFI_TRACK_FORMAT_NOT_SET = 0,
  AAFI_TRACK_FORMAT_MONO = 1,
  AAFI_TRACK_FORMAT_STEREO = 2,
  AAFI_TRACK_FORMAT_5_1 = 6,
  AAFI_TRACK_FORMAT_7_1 = 8,
  AAFI_TRACK_FORMAT_UNKNOWN = 99

} aafiTrackFormat_e;

/* forward declaration */
struct aafiAudio;
struct aafiVideo;

typedef struct aafiAudioTrack {
  /**
   * Track number
   * TODO Should it start at one ?
   * TODO Optional, should have a guess (i++) option.
   */

  uint32_t number;

  uint16_t format; // aafiTrackFormat_e, value = channel count

  /**
   * Track name
   */

  wchar_t *name;

  /**
   * Holds the Gain to apply on that track, that is the track volume Fader.
   */

  aafiAudioGain *gain;

  aafiAudioPan *pan;

  /**
   * Holds the timeline items of that track, that is aafiAudioClip and
   * aafiTransition structures.
   */

  struct aafiTimelineItem *Items;

  /**
   * The edit rate of all the contained Clips, Transitions, also lengths and
   * track->current_pos;
   */

  aafRational_t *edit_rate;

  /**
   * Pointer to the aafiAudio for convenient access.
   */

  struct aafiAudio *Audio;

  /**
   * Pointer to the next aafiAudioTrack structure in the aafiAudio.Tracks list.
   */

  aafPosition_t current_pos;

  struct aafiAudioTrack *next;

} aafiAudioTrack;

typedef struct aafiVideoTrack {
  /**
   * Track number
   * TODO Should it start at one ?
   * TODO Optional, should have a guess (i++) option.
   */

  uint32_t number;

  /**
   * Track name
   */

  wchar_t *name;

  /**
   * Holds the timeline items of that track, that is aafiVideoClip and
   * aafiTransition structures.
   */

  struct aafiTimelineItem *Items;

  /**
   * The edit rate of all the contained Clips and Transitions.
   */

  aafRational_t *edit_rate;

  /**
   * Pointer to the aafiVideo for convenient access.
   */

  struct aafiVideo *Video;

  /**
   * Pointer to the next aafiVideoTrack structure in the aafiVideo.Tracks list.
   */

  aafPosition_t current_pos;

  struct aafiVideoTrack *next;

} aafiVideoTrack;

typedef struct aafiUserComment {
  wchar_t *name;

  wchar_t *text;

  struct aafiUserComment *next;

} aafiUserComment;

typedef struct aafiAudio {
  /**
   * Holds the sequence start timecode.
   */

  aafPosition_t start;
  aafPosition_t length;
  aafRational_t length_editRate;

  int64_t samplerate;
  int16_t samplesize;

  /**
   * Holds the Essence list.
   */

  aafiAudioEssence *Essences;

  /**
   * Holds the Track list.
   */

  aafiAudioTrack *Tracks;
  uint32_t track_count;

} aafiAudio;

typedef struct aafiVideo {
  /**
   * Holds the sequence start timecode.
   */

  aafPosition_t start;
  aafPosition_t length;
  aafRational_t length_editRate;

  /**
   * Holds the Essence list.
   */

  aafiVideoEssence *Essences;

  /**
   * Holds the Track list.
   */

  aafiVideoTrack *Tracks;

} aafiVideo;

typedef struct aafiMarker {

  /*
   * TODO: link marker to specific track ? (optional in AAF standard, not yet
   * seen in AAF files)
   */

  aafPosition_t start;
  aafPosition_t length;
  aafRational_t *edit_rate;

  wchar_t *name;
  wchar_t *comment;
  uint16_t RGBColor[3];

  struct aafiMarker *prev;
  struct aafiMarker *next;

} aafiMarker;

// typedef enum aafiCurrentTreeType_e
// {
// 	AAFI_TREE_TYPE_AUDIO = 0,
// 	AAFI_TREE_TYPE_VIDEO = 1
//
// } aafiCurrentTreeType_e;

typedef struct aafiContext {

  /* Set in parse_MobSlot(), specifies if we're inside an audio or video context
   */
  // aafiCurrentTreeType_e current_tree_type;

  /*
   * Current MobSlot Segment's DataDefinition
   * Mob::Slots > MobSlot::Segment > Component::DataDefinition
   */

  // aafUID_t  *DataDef;

  /* Clip */

  aafiAudioTrack *current_track;

  /* Must be casted to aafiAudioTrack or aafiVideoTrack, according to
   * aafiContext::current_tree_type */
  // void * current_track;
  // int    current_track_number; // used only when missing
  // MobSlot::PhysicalTrackNumber

  // aafPosition_t     current_pos;
  aafiAudioClip *current_clip;
  aafiVideoClip *current_video_clip;
  int current_clip_is_muted;

  int current_clip_is_combined; // Inside
                                // OperationGroup::AAFOperationDef_AudioChannelCombiner
  int current_combined_clip_total_channel;
  int current_combined_clip_channel_num; // current SourceClip represents
                                         // channel num

  /* Transition */

  aafiTransition *current_transition;

  /* Gain */

  aafiAudioGain *current_clip_gain;
  aafiAudioGain *current_clip_automation;
  int clips_using_gain; // if none then free( current_clip_gain );
  int clips_using_automation;

  /* Essence */

  // aafiAudioEssence *current_audioEssence;
  // void *current_essence;
  aafiAudioEssence *current_essence;
  aafiVideoEssence *current_video_essence;

  aafRational_t *current_markers_edit_rate;

  int is_inside_derivation_chain;

  struct options {

    int trace;
    int trace_meta;
    wchar_t *dump_class_aaf_properties;
    wchar_t *dump_class_raw_properties;
    char *media_location;
    char forbid_nonlatin_filenames;
    /* vendor specific */
    uint32_t resolve;
    uint32_t protools;
  } options;

} aafiContext;

typedef struct AAF_Iface {
  aafiContext ctx;

  /**
   * Keeps track of the AAF_Data structure.
   */

  AAF_Data *aafd;

  aafiAudio *Audio;

  aafiVideo *Video;

  aafiTimecode *Timecode;

  aafiMarker *Markers;

  wchar_t *compositionName;

  aafPosition_t compositionStart; // set from aafi->Timecode->start
  aafRational_t compositionStart_editRate;

  aafPosition_t compositionLength;
  aafRational_t compositionLength_editRate;

  aafiUserComment *Comments;

  struct dbg *dbg;

} AAF_Iface;

#define foreach_audioTrack(audioTrack, aafi)                                   \
  for (audioTrack = aafi->Audio->Tracks; audioTrack != NULL;                   \
       audioTrack = audioTrack->next)

#define foreach_videoTrack(videoTrack, aafi)                                   \
  for (videoTrack = aafi->Video->Tracks; videoTrack != NULL;                   \
       videoTrack = videoTrack->next)

#define foreach_Item(item, track)                                              \
  for (item = track->Items; item != NULL; item = item->next)

#define foreachEssence(essence, essenceList)                                   \
  for (essence = essenceList; essence != NULL; essence = essence->next)

#define foreachMarker(marker, aafi)                                            \
  for (marker = aafi->Markers; marker != NULL; marker = marker->next)

#define aeDuration_h(audioEssence)                                             \
  ((audioEssence->samplerate == 0)                                             \
       ? 0                                                                     \
       : ((uint16_t)(audioEssence->length / audioEssence->samplerate /         \
                     (audioEssence->samplesize / 8)) /                         \
          3600))

#define aeDuration_m(audioEssence)                                             \
  ((audioEssence->samplerate == 0)                                             \
       ? 0                                                                     \
       : ((uint16_t)(audioEssence->length / audioEssence->samplerate /         \
                     (audioEssence->samplesize / 8)) %                         \
          3600 / 60))

#define aeDuration_s(audioEssence)                                             \
  ((audioEssence->samplerate == 0)                                             \
       ? 0                                                                     \
       : ((uint16_t)(audioEssence->length / audioEssence->samplerate /         \
                     (audioEssence->samplesize / 8)) %                         \
          3600 % 60))

#define aeDuration_ms(audioEssence)                                            \
  ((audioEssence->samplerate == 0)                                             \
       ? 0                                                                     \
       : ((uint16_t)(audioEssence->length /                                    \
                     (audioEssence->samplerate / 1000) /                       \
                     (audioEssence->samplesize / 8)) %                         \
          3600000 % 60000 % 1000))

#define convertEditUnit(val, fromRate, toRate)                                 \
  (int64_t)((val) * (aafRationalToFloat((toRate)) *                            \
                     (1 / aafRationalToFloat((fromRate)))))

#define eu2sample(samplerate, edit_rate, val)                                  \
  (int64_t)(val * (samplerate * (1 / aafRationalToFloat((*edit_rate)))))

void aafi_set_debug(AAF_Iface *aafi, verbosityLevel_e v, int ansicolor,
                    FILE *fp,
                    void (*callback)(struct dbg *dbg, void *ctxdata, int lib,
                                     int type, const char *srcfile,
                                     const char *srcfunc, int lineno,
                                     const char *msg, void *user),
                    void *user);

int aafi_set_option_int(AAF_Iface *aafi, const char *optname, int val);
int aafi_set_option_str(AAF_Iface *aafi, const char *optname, const char *val);

AAF_Iface *aafi_alloc(AAF_Data *aafd);

void aafi_release(AAF_Iface **aafi);

int aafi_load_file(AAF_Iface *aafi, const char *file);

aafiTransition *aafi_get_fadein(aafiTimelineItem *audioItem);

aafiTransition *aafi_get_fadeout(aafiTimelineItem *audioItem);

aafiTransition *aafi_get_xfade(aafiTimelineItem *audioItem);

aafiMarker *aafi_newMarker(AAF_Iface *aafi, aafRational_t *editRate,
                           aafPosition_t start, aafPosition_t length,
                           wchar_t *name, wchar_t *comment,
                           uint16_t *RGBColor[3]);

void aafi_freeMarkers(aafiMarker **aafi);

aafiAudioTrack *aafi_newAudioTrack(AAF_Iface *aafi);

void aafi_freeAudioTracks(aafiAudioTrack **tracks);

aafiVideoTrack *aafi_newVideoTrack(AAF_Iface *aafi);

void aafi_freeVideoTracks(aafiVideoTrack **tracks);

aafiTimelineItem *aafi_newTimelineItem(AAF_Iface *aafi, void *track,
                                       int itemType);

int aafi_removeTimelineItem(AAF_Iface *aafi, aafiTimelineItem *item);

void aafi_freeAudioGain(aafiAudioGain *gain);

void aafi_freeAudioPan(aafiAudioPan *pan);

void aafi_freeAudioClip(aafiAudioClip *audioClip);

void aafi_freeTimelineItem(aafiTimelineItem **item);

void aafi_freeTimelineItems(aafiTimelineItem **items);

aafiUserComment *aafi_newUserComment(AAF_Iface *aafi,
                                     aafiUserComment **CommentList);

void aafi_freeUserComments(aafiUserComment **CommentList);

void aafi_freeTransition(aafiTransition *trans);

aafiAudioEssence *aafi_newAudioEssence(AAF_Iface *aafi);

void aafi_freeAudioEssences(aafiAudioEssence **essences);

aafiVideoEssence *aafi_newVideoEssence(AAF_Iface *aafi);

void aafi_freeVideoEssences(aafiVideoEssence **videoEssence);

/**
 * @}
 */

#endif // !__AAFIface_h__
