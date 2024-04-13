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
 * The AAFIface provides the actual processing of the AAF Objects in order to show
 * essences and clips in a simplified manner. Indeed, AAF has many different ways to
 * store data and metadata. Thus, the AAFIface is an abstraction layer that provides
 * a constant and unique representation method of essences and clips.
 *
 *
 *
 * @{
 */

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aaf/AAFIParser.h"
#include "aaf/AAFIface.h"
#include "aaf/log.h"
#include "aaf/utils.h"

#define debug(...) \
	AAF_LOG (aafi->log, aafi, LOG_SRC_ID_AAF_IFACE, VERB_DEBUG, __VA_ARGS__)

#define warning(...) \
	AAF_LOG (aafi->log, aafi, LOG_SRC_ID_AAF_IFACE, VERB_WARNING, __VA_ARGS__)

#define error(...) \
	AAF_LOG (aafi->log, aafi, LOG_SRC_ID_AAF_IFACE, VERB_ERROR, __VA_ARGS__)

AAF_Iface*
aafi_alloc (AAF_Data* aafd)
{
	AAF_Iface* aafi = calloc (1, sizeof (AAF_Iface));

	if (!aafi) {
		return NULL;
	}

	aafi->log = laaf_new_log ();

	if (!aafi->log) {
		goto err;
	}

	aafi->Audio = calloc (1, sizeof (aafiAudio));

	if (!aafi->Audio) {
		goto err;
	}

	aafi->Video = calloc (1, sizeof (aafiVideo));

	if (!aafi->Video) {
		goto err;
	}

	if (aafd) {
		aafi->aafd = aafd;
	} else {
		aafi->aafd = aaf_alloc (aafi->log);

		if (!aafi->aafd) {
			goto err;
		}
	}

	return aafi;

err:
	aafi_release (&aafi);

	return NULL;
}

void
aafi_set_debug (AAF_Iface* aafi, enum verbosityLevel_e verb, int ansicolor, FILE* fp, void (*callback) (struct aafLog* log, void* ctxdata, int lib, int type, const char* srcfile, const char* srcfunc, int lineno, const char* msg, void* user), void* user)
{
	if (!aafi) {
		return;
	}

	aafi->log->verb      = verb;
	aafi->log->ansicolor = ansicolor;
	aafi->log->fp        = fp;

	if (callback) {
		aafi->log->log_callback = callback;
	}

	if (user) {
		aafi->log->user = user;
	}

#ifdef _WIN32
	/* we dont want junk bytes to be printed to a windows file */
	if (fp != stdout && fp != stderr) {
		aafi->log->ansicolor = 0;
	}
#endif
}

int
aafi_set_option_int (AAF_Iface* aafi, const char* optname, int val)
{
	if (strcmp (optname, "trace") == 0) {
		aafi->ctx.options.trace = val;
		return 0;
	} else if (strcmp (optname, "dump_meta") == 0) {
		aafi->ctx.options.dump_meta = val;
		return 0;
	} else if (strcmp (optname, "dump_tagged_value") == 0) {
		aafi->ctx.options.dump_tagged_value = val;
		return 0;
	} else if (strcmp (optname, "protools") == 0) {
		aafi->ctx.options.protools = val;
		return 0;
	} else if (strcmp (optname, "mobid_essence_filename") == 0) {
		aafi->ctx.options.mobid_essence_filename = val;
		return 0;
	}

	return 1;
}

int
aafi_set_option_str (AAF_Iface* aafi, const char* optname, const char* val)
{
	if (strcmp (optname, "media_location") == 0) {
		free (aafi->ctx.options.media_location);
		aafi->ctx.options.media_location = laaf_util_c99strdup (val);

		if (val && !aafi->ctx.options.media_location) {
			return -1;
		}

		return 0;
	} else if (strcmp (optname, "dump_class_aaf_properties") == 0) {
		free (aafi->ctx.options.dump_class_aaf_properties);
		aafi->ctx.options.dump_class_aaf_properties = laaf_util_c99strdup (val);

		if (val && !aafi->ctx.options.dump_class_aaf_properties) {
			return -1;
		}

		return 0;
	} else if (strcmp (optname, "dump_class_raw_properties") == 0) {
		free (aafi->ctx.options.dump_class_raw_properties);
		aafi->ctx.options.dump_class_raw_properties = laaf_util_c99strdup (val);

		if (val && !aafi->ctx.options.dump_class_raw_properties) {
			return -1;
		}

		return 0;
	}

	return 1;
}

int
aafi_load_file (AAF_Iface* aafi, const char* file)
{
	if (!aafi || !file || aaf_load_file (aafi->aafd, file)) {
		return 1;
	}

	aafi_retrieveData (aafi);

	return 0;
}

void
aafi_release (AAF_Iface** aafi)
{
	if (!aafi || !(*aafi)) {
		return;
	}

	aaf_release (&(*aafi)->aafd);

	if ((*aafi)->Audio != NULL) {
		aafi_freeAudioTracks (&(*aafi)->Audio->Tracks);
		aafi_freeAudioEssences (&(*aafi)->Audio->essenceFiles);

		free ((*aafi)->Audio);
	}

	if ((*aafi)->Video != NULL) {
		aafi_freeVideoTracks (&(*aafi)->Video->Tracks);
		aafi_freeVideoEssences (&(*aafi)->Video->essenceFiles);

		free ((*aafi)->Video);
	}

	aafi_freeMarkers (&(*aafi)->Markers);
	aafi_freeMetadata (&((*aafi)->metadata));

	free ((*aafi)->compositionName);

	free ((*aafi)->ctx.options.dump_class_aaf_properties);
	free ((*aafi)->ctx.options.dump_class_raw_properties);
	free ((*aafi)->ctx.options.media_location);
	free ((*aafi)->Timecode);

	laaf_free_log ((*aafi)->log);

	free (*aafi);

	*aafi = NULL;
}

aafiAudioClip*
aafi_timelineItemToAudioClip (aafiTimelineItem* audioItem)
{
	if (!audioItem || audioItem->type != AAFI_AUDIO_CLIP) {
		return NULL;
	}

	return audioItem->data;
}

aafiTransition*
aafi_timelineItemToCrossFade (aafiTimelineItem* audioItem)
{
	if (!audioItem || audioItem->type != AAFI_TRANS) {
		return NULL;
	}

	aafiTransition* Trans = audioItem->data;

	if (!Trans || !(Trans->flags & AAFI_TRANS_XFADE))
		return NULL;

	return Trans;
}

aafiTransition*
aafi_getFadeIn (aafiAudioClip* audioClip)
{
	if (!audioClip) {
		return NULL;
	}

	aafiTimelineItem* audioItem = audioClip->timelineItem;

	if (!audioItem) {
		return NULL;
	}

	if (audioItem->prev != NULL &&
	    audioItem->prev->type == AAFI_TRANS) {
		aafiTransition* Trans = audioItem->prev->data;

		if (Trans->flags & AAFI_TRANS_FADE_IN)
			return Trans;
	}

	return NULL;
}

aafiTransition*
aafi_getFadeOut (aafiAudioClip* audioClip)
{
	if (!audioClip) {
		return NULL;
	}

	aafiTimelineItem* audioItem = audioClip->timelineItem;

	if (!audioItem) {
		return NULL;
	}

	if (audioItem->next != NULL &&
	    audioItem->next->type == AAFI_TRANS) {
		aafiTransition* Trans = audioItem->next->data;

		if (Trans->flags & AAFI_TRANS_FADE_OUT)
			return Trans;
	}

	return NULL;
}

int
aafi_get_clipIndex (aafiAudioClip* audioClip)
{
	if (!audioClip) {
		return 0;
	}

	int                      index        = 0;
	struct aafiTimelineItem* timelineItem = NULL;
	struct aafiAudioTrack*   track        = audioClip->track;

	AAFI_foreachTrackItem (track, timelineItem)
	{
		if (timelineItem->type == AAFI_AUDIO_CLIP) {
			index++;
		}
		if (timelineItem->data == audioClip) {
			return index;
		}
	}

	return 0;
}

aafPosition_t
aafi_convertUnit (aafPosition_t value, aafRational_t* valueEditRate, aafRational_t* destEditRate)
{
	if (!valueEditRate || !destEditRate) {
		return value;
	}

	if (valueEditRate->numerator == destEditRate->numerator &&
	    valueEditRate->denominator == destEditRate->denominator) {
		/* same rate, no conversion */
		return value;
	}

	double valueEditRateFloat = ((valueEditRate->denominator == 0) ? 0.0 : ((float)valueEditRate->numerator / (float)valueEditRate->denominator));
	double destEditRateFloat  = ((destEditRate->denominator == 0) ? 0.0 : ((float)destEditRate->numerator / (float)destEditRate->denominator));

	if (valueEditRateFloat == 0) {
		return 0;
	}

	return (aafPosition_t) ((double)value * (destEditRateFloat / valueEditRateFloat));
}

uint64_t
aafi_convertUnitUint64 (aafPosition_t value, aafRational_t* valueEditRate, aafRational_t* destEditRate)
{
	if (!valueEditRate || !destEditRate) {
		if (value < 0) {
			/* TODO is ULONG_MAX ok for max uint64_t ? */
			return ULONG_MAX;
		}

		return (uint64_t)value;
	}

	if (valueEditRate->numerator == destEditRate->numerator &&
	    valueEditRate->denominator == destEditRate->denominator) {
		/* same rate, no conversion */
		if (value < 0) {
			/* TODO is ULONG_MAX ok for max uint64_t ? */
			return ULONG_MAX;
		}

		return (uint64_t)value;
	}

	double valueEditRateFloat = ((valueEditRate->denominator == 0) ? 0.0 : ((float)valueEditRate->numerator / (float)valueEditRate->denominator));
	double destEditRateFloat  = ((destEditRate->denominator == 0) ? 0.0 : ((float)destEditRate->numerator / (float)destEditRate->denominator));

	if (valueEditRateFloat == 0) {
		return 0;
	}

	return (uint64_t) ((double)value * (destEditRateFloat / valueEditRateFloat));
}

int
aafi_removeTimelineItem (AAF_Iface* aafi, aafiTimelineItem* timelineItem)
{
	if (!timelineItem) {
		return 0;
	}

	if (timelineItem->prev != NULL) {
		timelineItem->prev->next = timelineItem->next;
	}

	if (timelineItem->next != NULL) {
		timelineItem->next->prev = timelineItem->prev;
	}

	aafiAudioTrack* audioTrack = NULL;

	AAFI_foreachAudioTrack (aafi, audioTrack)
	{
		if (audioTrack->timelineItems == timelineItem) {
			audioTrack->timelineItems = timelineItem->next;
		}
	}

	aafi_freeTimelineItem (timelineItem);

	return 0;
}

int
aafi_getAudioEssencePointerChannelCount (aafiAudioEssencePointer* essencePointerList)
{
	/*
	 * If essencePointerList holds a single multichannel essence file and if
	 * essencePointer->essenceChannel is set, then clip is mono and audio comes
	 * from essencePointer->essenceChannel of essencePointer.essenceFile.
	 *
	 * If essencePointerList holds a single multichannel essence file and if
	 * essencePointer->essenceChannel is null, then clip is multichannel and
	 * clip channel count equals essence->channels.
	 *
	 * If essencePointerList holds multiple pointers to multiple essence files,
	 * then each file should be mono and describe a clip channel. Thus, clip
	 * channel count equals pointers count.
	 */

	// if ( !essencePointerList ) {
	// 	return 0;
	// }

	int                      essencePointerCount = 0;
	aafiAudioEssencePointer* essencePointer      = NULL;

	AAFI_foreachEssencePointer (essencePointerList, essencePointer)
	{
		essencePointerCount++;
	}

	return (essencePointerCount > 1) ? essencePointerCount : (essencePointerList->essenceChannel) ? 1
	                                                                                              : essencePointerList->essenceFile->channels;
}

int
aafi_applyGainOffset (AAF_Iface* aafi, aafiAudioGain** gain, aafiAudioGain* offset)
{
	if ((offset->flags & AAFI_AUDIO_GAIN_MASK) & AAFI_AUDIO_GAIN_VARIABLE) {
		debug ("Variable gain offset is not supported");
		return -1;
	}

	if (*gain == NULL) {
		/*
		 * apply offset as new gain
		 */

		debug ("Applying gain to clip as a new gain");

		(*gain) = aafi_newAudioGain (aafi, offset->flags & AAFI_AUDIO_GAIN_MASK, offset->flags & AAFI_INTERPOL_MASK, NULL);

		(*gain)->time  = calloc ((uint64_t)offset->pts_cnt, sizeof (aafRational_t));
		(*gain)->value = calloc ((uint64_t)offset->pts_cnt, sizeof (aafRational_t));

		if (!(*gain)->time || !(*gain)->value) {
			error ("Out of memory");
			aafi_freeAudioGain (*gain);
			return -1;
		}

		for (unsigned int i = 0; i < (*gain)->pts_cnt; i++) {
			(*gain)->value[i].numerator   = offset->value[0].numerator;
			(*gain)->value[i].denominator = offset->value[0].denominator;
			// debug( "Setting (*gain)->value[%i] = %i/%i",
			// 	i,
			// 	(*gain)->value[i].numerator,
			// 	(*gain)->value[i].denominator );
		}
	} else {
		/*
		 * update existing constant or variable gain
		 */

		debug ("Applying gain to clip: %i/%i (%+05.1lf dB) ",
		       (*gain)->value[0].numerator,
		       (*gain)->value[0].denominator,
		       20 * log10 (aafRationalToDouble ((*gain)->value[0])));

		for (unsigned int i = 0; i < (*gain)->pts_cnt; i++) {
			/*
			 * most of the time, gain values are made of very high numbers and denominator
			 * is the same accross all gains in file. Thus, we devide both gain numbers
			 * by offset denominator, so we fit inside uint32_t.
			 */
			(*gain)->value[i].numerator   = (int32_t) (((int64_t) (*gain)->value[i].numerator * (int64_t)offset->value[0].numerator) / (int64_t)offset->value[0].denominator);
			(*gain)->value[i].denominator = (int32_t) (((int64_t) (*gain)->value[i].denominator * (int64_t)offset->value[0].denominator) / (int64_t)offset->value[0].denominator);
			// debug( "Setting (*gain)->value[%i] = %i/%i * %i/%i",
			// 	i,
			// 	(*gain)->value[i].numerator,
			// 	(*gain)->value[i].denominator,
			// 	offset->value[0].numerator,
			// 	offset->value[0].denominator );
		}
	}

	return 0;
}

aafiAudioTrack*
aafi_newAudioTrack (AAF_Iface* aafi)
{
	aafiAudioTrack* track = calloc (1, sizeof (aafiAudioTrack));

	if (!track) {
		error ("Out of memory");
		return NULL;
	}

	track->Audio  = aafi->Audio;
	track->format = AAFI_TRACK_FORMAT_NOT_SET;
	track->next   = NULL;

	/* Add to track list */

	if (aafi->Audio->Tracks != NULL) {
		aafiAudioTrack* tmp = aafi->Audio->Tracks;

		for (; tmp != NULL; tmp = tmp->next)
			if (tmp->next == NULL)
				break;

		tmp->next = track;
	} else {
		aafi->Audio->Tracks = track;
	}

	return track;
}

aafiVideoTrack*
aafi_newVideoTrack (AAF_Iface* aafi)
{
	aafiVideoTrack* track = calloc (1, sizeof (aafiVideoTrack));

	if (!track) {
		error ("Out of memory");
		return NULL;
	}

	track->Video = aafi->Video;
	track->next  = NULL;

	/* Add to track list */

	if (aafi->Video->Tracks != NULL) {
		aafiVideoTrack* tmp = aafi->Video->Tracks;

		for (; tmp != NULL; tmp = tmp->next)
			if (tmp->next == NULL)
				break;

		tmp->next = track;
	} else {
		aafi->Video->Tracks = track;
	}

	return track;
}

aafiTimelineItem*
aafi_newTimelineItem (AAF_Iface* aafi, void* track, int itemType, void* data)
{
	aafiTimelineItem* timelineItem = calloc (1, sizeof (aafiTimelineItem));

	if (!timelineItem) {
		error ("Out of memory");
		return NULL;
	}

	timelineItem->type = itemType;
	timelineItem->data = data;

	if (itemType == AAFI_AUDIO_CLIP || itemType == AAFI_TRANS) {
		if (track != NULL) {
			/* Add to track's timelineItem list */

			if (((aafiAudioTrack*)track)->timelineItems != NULL) {
				aafiTimelineItem* tmp = ((aafiAudioTrack*)track)->timelineItems;

				for (; tmp != NULL; tmp = tmp->next)
					if (tmp->next == NULL)
						break;

				tmp->next          = timelineItem;
				timelineItem->prev = tmp;
			} else {
				((aafiAudioTrack*)track)->timelineItems = timelineItem;
				timelineItem->prev                      = NULL;
			}
		}
	} else if (itemType == AAFI_VIDEO_CLIP) {
		if (track != NULL) {
			/* Add to track's timelineItem list */

			if (((aafiVideoTrack*)track)->timelineItems != NULL) {
				aafiTimelineItem* tmp = ((aafiVideoTrack*)track)->timelineItems;

				for (; tmp != NULL; tmp = tmp->next)
					if (tmp->next == NULL)
						break;

				tmp->next          = timelineItem;
				timelineItem->prev = tmp;
			} else {
				((aafiVideoTrack*)track)->timelineItems = timelineItem;
				timelineItem->prev                      = NULL;
			}
		}
	}

	return timelineItem;
}

aafiAudioClip*
aafi_newAudioClip (AAF_Iface* aafi, aafiAudioTrack* track)
{
	aafiAudioClip* audioClip = calloc (1, sizeof (aafiAudioClip));

	if (!audioClip) {
		error ("Out of memory");
		return NULL;
	}

	audioClip->track        = track;
	audioClip->timelineItem = aafi_newTimelineItem (aafi, track, AAFI_AUDIO_CLIP, audioClip);

	if (!audioClip->timelineItem) {
		error ("Could not create new timelineItem");
		free (audioClip);
		return NULL;
	}

	return audioClip;
}

aafiVideoClip*
aafi_newVideoClip (AAF_Iface* aafi, aafiVideoTrack* track)
{
	aafiVideoClip* videoClip = calloc (1, sizeof (aafiVideoClip));

	if (!videoClip) {
		error ("Out of memory");
		return NULL;
	}

	videoClip->track        = track;
	videoClip->timelineItem = aafi_newTimelineItem (aafi, track, AAFI_VIDEO_CLIP, videoClip);

	if (!videoClip->timelineItem) {
		error ("Could not create new timelineItem");
		free (videoClip);
		return NULL;
	}

	return videoClip;
}

aafiTransition*
aafi_newTransition (AAF_Iface* aafi, aafiAudioTrack* track)
{
	aafiTransition* trans = calloc (1, sizeof (aafiTransition));

	if (!trans) {
		error ("Out of memory");
		return NULL;
	}

	trans->timelineItem = aafi_newTimelineItem (aafi, track, AAFI_TRANS, trans);

	if (!trans->timelineItem) {
		error ("Could not create new timelineItem");
		free (trans);
		return NULL;
	}

	trans->time_a  = calloc (2, sizeof (aafRational_t));
	trans->value_a = calloc (2, sizeof (aafRational_t));

	if (!trans->time_a || !trans->value_a) {
		error ("Out of memory");
		aafi_freeTimelineItem (trans->timelineItem);
		return NULL;
	}

	return trans;
}

aafiMarker*
aafi_newMarker (AAF_Iface* aafi, aafRational_t* editRate, aafPosition_t start, aafPosition_t length, char* name, char* comment, uint16_t*(RGBColor[]))
{
	aafiMarker* marker = calloc (sizeof (aafiMarker), 1);

	if (!marker) {
		error ("Out of memory");
		return NULL;
	}

	marker->edit_rate = editRate;
	marker->start     = start;
	marker->length    = length;

	marker->name    = name;
	marker->comment = comment;

	marker->prev = NULL;
	marker->next = NULL;

	if (RGBColor && *RGBColor) {
		marker->RGBColor[0] = (*RGBColor)[0];
		marker->RGBColor[1] = (*RGBColor)[1];
		marker->RGBColor[2] = (*RGBColor)[2];
	}

	if (aafi->Markers != NULL) {
		aafiMarker* tmp = aafi->Markers;

		for (; tmp != NULL; tmp = tmp->next)
			if (tmp->next == NULL)
				break;

		tmp->next    = marker;
		marker->prev = marker;
	} else {
		aafi->Markers = marker;
		marker->prev  = NULL;
	}

	return marker;
}

aafiMetaData*
aafi_newMetadata (AAF_Iface* aafi, aafiMetaData** CommentList)
{
	if (!CommentList) {
		return NULL;
	}

	aafiMetaData* UserComment = calloc (1, sizeof (aafiMetaData));

	if (!UserComment) {
		error ("Out of memory");
		return NULL;
	}

	if (*CommentList != NULL) {
		UserComment->next = *CommentList;
		*CommentList      = UserComment;
	} else {
		*CommentList = UserComment;
	}

	return UserComment;
}

aafiAudioEssencePointer*
aafi_newAudioEssencePointer (AAF_Iface* aafi, aafiAudioEssencePointer** list, aafiAudioEssenceFile* audioEssenceFile, uint32_t* essenceChannelNum)
{
	aafiAudioEssencePointer* essencePointer = calloc (1, sizeof (aafiAudioEssencePointer));

	if (!essencePointer) {
		error ("Out of memory");
		return NULL;
	}

	essencePointer->aafi           = aafi;
	essencePointer->essenceFile    = audioEssenceFile;
	essencePointer->essenceChannel = (essenceChannelNum) ? *essenceChannelNum : 0;

	if (*list) {
		aafiAudioEssencePointer* last = *list;
		while (last->next != NULL) {
			last = last->next;
		}
		last->next = essencePointer;
	} else {
		*list = essencePointer;

		essencePointer->aafiNext        = aafi->Audio->essencePointerList;
		aafi->Audio->essencePointerList = essencePointer;
	}

	return *list;
}

aafiAudioEssenceFile*
aafi_newAudioEssence (AAF_Iface* aafi)
{
	aafiAudioEssenceFile* audioEssenceFile = calloc (1, sizeof (aafiAudioEssenceFile));

	if (!audioEssenceFile) {
		error ("Out of memory");
		goto err;
	}

	audioEssenceFile->samplerateRational = malloc (sizeof (aafRational_t));

	if (!audioEssenceFile->samplerateRational) {
		error ("Out of memory");
		goto err;
	}

	audioEssenceFile->samplerateRational->numerator   = 1;
	audioEssenceFile->samplerateRational->denominator = 1;

	audioEssenceFile->next = aafi->Audio->essenceFiles;

	aafi->Audio->essenceFiles = audioEssenceFile;
	aafi->Audio->essenceCount++;

	return audioEssenceFile;

err:
	if (audioEssenceFile) {
		free (audioEssenceFile->samplerateRational);
		free (audioEssenceFile);
	}

	return NULL;
}

aafiVideoEssence*
aafi_newVideoEssence (AAF_Iface* aafi)
{
	aafiVideoEssence* videoEssenceFile = calloc (1, sizeof (aafiVideoEssence));

	if (!videoEssenceFile) {
		error ("Out of memory");
		return NULL;
	}

	videoEssenceFile->next = aafi->Video->essenceFiles;

	aafi->Video->essenceFiles = videoEssenceFile;

	return videoEssenceFile;
}

aafiAudioGain*
aafi_newAudioGain (AAF_Iface* aafi, enum aafiAudioGain_e type, enum aafiInterpolation_e interpol, aafRational_t* singleValue)
{
	aafiAudioGain* Gain = calloc (1, sizeof (aafiAudioGain));

	if (!Gain) {
		error ("Out of memory");
		return NULL;
	}

	Gain->flags |= type;
	Gain->flags |= interpol;

	if (singleValue) {
		Gain->pts_cnt = 1;
		Gain->value   = calloc (1, sizeof (aafRational_t));

		if (!Gain->value) {
			error ("Out of memory");
			free (Gain);
			return NULL;
		}

		memcpy (&Gain->value[0], singleValue, sizeof (aafRational_t));
	}

	return Gain;
}

aafiAudioGain*
aafi_newAudioPan (AAF_Iface* aafi, enum aafiAudioGain_e type, enum aafiInterpolation_e interpol, aafRational_t* singleValue)
{
	return aafi_newAudioGain (aafi, type, interpol, singleValue);
}

void
aafi_freeAudioTracks (aafiAudioTrack** tracks)
{
	if (!tracks || !(*tracks)) {
		return;
	}

	aafiAudioTrack* track     = NULL;
	aafiAudioTrack* nextTrack = NULL;

	for (track = (*tracks); track != NULL; track = nextTrack) {
		nextTrack = track->next;

		free (track->name);
		aafi_freeAudioGain (track->gain);
		aafi_freeAudioPan (track->pan);
		aafi_freeTimelineItems (&track->timelineItems);

		free (track);
	}

	*tracks = NULL;
}

void
aafi_freeVideoTracks (aafiVideoTrack** tracks)
{
	if (*(tracks) == NULL) {
		return;
	}

	aafiVideoTrack* track     = NULL;
	aafiVideoTrack* nextTrack = NULL;

	for (track = (*tracks); track != NULL; track = nextTrack) {
		nextTrack = track->next;

		free (track->name);
		aafi_freeTimelineItems (&track->timelineItems);

		free (track);
	}

	*tracks = NULL;
}

void
aafi_freeTimelineItems (aafiTimelineItem** timelineItems)
{
	aafiTimelineItem* timelineItem = NULL;
	aafiTimelineItem* nextItem     = NULL;

	for (timelineItem = (*timelineItems); timelineItem != NULL; timelineItem = nextItem) {
		nextItem = timelineItem->next;
		aafi_freeTimelineItem (timelineItem);
	}

	*timelineItems = NULL;
}

void
aafi_freeTimelineItem (aafiTimelineItem* timelineItem)
{
	if (!timelineItem) {
		return;
	}

	if (timelineItem->type == AAFI_TRANS) {
		aafi_freeTransition (timelineItem->data);
	} else if (timelineItem->type == AAFI_AUDIO_CLIP) {
		aafi_freeAudioClip (timelineItem->data);
	} else if (timelineItem->type == AAFI_VIDEO_CLIP) {
		free (timelineItem->data);
	}

	free (timelineItem);
}

void
aafi_freeAudioClip (aafiAudioClip* audioClip)
{
	if (!audioClip) {
		return;
	}

	free (audioClip->subClipName);

	aafi_freeAudioGain (audioClip->gain);
	aafi_freeAudioGain (audioClip->automation);
	aafi_freeMetadata (&(audioClip->metadata));

	aafi_freeAudioEssencePointer (audioClip->essencePointerList);

	free (audioClip);
}

void
aafi_freeTransition (aafiTransition* Transition)
{
	if (!Transition) {
		return;
	}

	free (Transition->value_a);
	free (Transition->value_b);
	free (Transition->time_a);
	free (Transition->time_b);

	free (Transition);
}

void
aafi_freeMarkers (aafiMarker** Markers)
{
	aafiMarker* marker     = NULL;
	aafiMarker* nextMarker = NULL;

	for (marker = (*Markers); marker != NULL; marker = nextMarker) {
		nextMarker = marker->next;

		free (marker->name);
		free (marker->comment);

		free (marker);
	}

	*Markers = NULL;
}

void
aafi_freeMetadata (aafiMetaData** CommentList)
{
	aafiMetaData* UserComment = *CommentList;
	aafiMetaData* tmp         = NULL;

	while (UserComment != NULL) {
		tmp         = UserComment;
		UserComment = UserComment->next;

		free (tmp->name);
		free (tmp->text);

		free (tmp);
	}

	*CommentList = NULL;
}

void
aafi_freeAudioEssencePointer (aafiAudioEssencePointer* essencePointer)
{
	aafiAudioEssencePointer* next = NULL;

	while (essencePointer) {
		next = essencePointer->next;
		free (essencePointer);
		essencePointer = next;
	}
}

void
aafi_freeAudioEssences (aafiAudioEssenceFile** audioEssenceFile)
{
	if (*(audioEssenceFile) == NULL) {
		return;
	}

	aafiAudioEssenceFile* nextAudioEssence = NULL;

	for (; (*audioEssenceFile) != NULL; *audioEssenceFile = nextAudioEssence) {
		nextAudioEssence = (*audioEssenceFile)->next;

		free ((*audioEssenceFile)->original_file_path);
		free ((*audioEssenceFile)->usable_file_path);
		free ((*audioEssenceFile)->name);
		free ((*audioEssenceFile)->unique_name);
		free ((*audioEssenceFile)->samplerateRational);

		aafi_freeMetadata (&((*audioEssenceFile)->metadata));

		free (*audioEssenceFile);
	}

	*audioEssenceFile = NULL;
}

void
aafi_freeVideoEssences (aafiVideoEssence** videoEssenceFile)
{
	if (*(videoEssenceFile) == NULL) {
		return;
	}

	aafiVideoEssence* nextVideoEssence = NULL;

	for (; (*videoEssenceFile) != NULL; *videoEssenceFile = nextVideoEssence) {
		nextVideoEssence = (*videoEssenceFile)->next;

		free ((*videoEssenceFile)->original_file_path);
		free ((*videoEssenceFile)->usable_file_path);
		free ((*videoEssenceFile)->name);
		free ((*videoEssenceFile)->unique_name);

		free (*videoEssenceFile);
	}

	*videoEssenceFile = NULL;
}

void
aafi_freeAudioGain (aafiAudioGain* gain)
{
	if (gain == NULL) {
		return;
	}

	free (gain->time);
	free (gain->value);

	free (gain);
}

void
aafi_freeAudioPan (aafiAudioPan* pan)
{
	aafi_freeAudioGain ((aafiAudioGain*)pan);
}

/**
 * @}
 */
