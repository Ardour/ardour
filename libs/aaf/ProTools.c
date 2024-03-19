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

#include <stdio.h>

#include "aaf/AAFIface.h"
#include "aaf/ProTools.h"

#define debug(...) \
	AAF_LOG (aafi->log, aafi, LOG_SRC_ID_AAF_IFACE, VERB_DEBUG, __VA_ARGS__)

#define warning(...) \
	AAF_LOG (aafi->log, aafi, LOG_SRC_ID_AAF_IFACE, VERB_WARNING, __VA_ARGS__)

#define error(...) \
	AAF_LOG (aafi->log, aafi, LOG_SRC_ID_AAF_IFACE, VERB_ERROR, __VA_ARGS__)

/* English : "Fade "	(Same as JA and DE)	 */
static const char PROTOOLS_CLIP_NAME_FADE_EN[] = "\x46\x61\x64\x65\x20";
/* French : "Fondu "	*/
static const char PROTOOLS_CLIP_NAME_FADE_FR[] = "\x46\x6f\x6e\x64\x75\x20";
/* Spanish : "Fundido"	*/
static const char PROTOOLS_CLIP_NAME_FADE_ES[] = "\x46\x75\x6e\x64\x69\x64\x6f\x20";
/* Korean : "페이드"	*/
static const char PROTOOLS_CLIP_NAME_FADE_KO[] = "\xed\x8e\x98\xec\x9d\xb4\xeb\x93\x9c";
/* Chinese (S) : "淡变 " */
static const char PROTOOLS_CLIP_NAME_FADE_ZH_CN[] = "\xe6\xb7\xa1\xe5\x8f\x98\x20";
/* Chinese (T) : "淡變 " */
static const char PROTOOLS_CLIP_NAME_FADE_ZH_TW[] = "\xe6\xb7\xa1\xe8\xae\x8a\x20";

/* English : "Sample accurate edit"	*/
static const char PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_EN[] = "\x53\x61\x6d\x70\x6c\x65\x20\x61\x63\x63\x75\x72\x61\x74\x65\x20\x65\x64\x69\x74";
/* German : "Samplegenaue Bearbeitung"	*/
static const char PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_DE[] = "\x53\x61\x6d\x70\x6c\x65\x67\x65\x6e\x61\x75\x65\x20\x42\x65\x61\x72\x62\x65\x69\x74\x75\x6e\x67";
/* Spanish : "Edición con precisión de muestra"	*/
static const char PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_ES[] = "\x45\x64\x69\x63\x69\xc3\xb3\x6e\x20\x63\x6f\x6e\x20\x70\x72\x65\x63\x69\x73\x69\xc3\xb3\x6e\x20\x64\x65\x20\x6d\x75\x65\x73\x74\x72\x61";
/* French : "Modification à l'échantillon près"	*/
static const char PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_FR[] = "\x4d\x6f\x64\x69\x66\x69\x63\x61\x74\x69\x6f\x6e\x20\xc3\xa0\x20\x6c\x27\xc3\xa9\x63\x68\x61\x6e\x74\x69\x6c\x6c\x6f\x6e\x20\x70\x72\xc3\xa8\x73";
/* Japanese : "サンプル精度編集"	*/
static const char PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_JA[] = "\xe3\x82\xb5\xe3\x83\xb3\xe3\x83\x97\xe3\x83\xab\xe7\xb2\xbe\xe5\xba\xa6\xe7\xb7\xa8\xe9\x9b\x86";
/* Korean : "샘플 단위 정밀 편집"	*/
static const char PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_KO[] = "\xec\x83\x98\xed\x94\x8c\x20\xeb\x8b\xa8\xec\x9c\x84\x20\xec\xa0\x95\xeb\xb0\x80\x20\xed\x8e\xb8\xec\xa7\x91";
/* Chinese (S) : "精确采样编辑"	*/
static const char PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_ZH_CN[] = "\xe7\xb2\xbe\xe7\xa1\xae\xe9\x87\x87\xe6\xa0\xb7\xe7\xbc\x96\xe8\xbe\x91";
/* Chinese (T) : "精確取樣編輯"	*/
static const char PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_ZH_TW[] = "\xe7\xb2\xbe\xe7\xa2\xba\xe5\x8f\x96\xe6\xa8\xa3\xe7\xb7\xa8\xe8\xbc\xaf";

static int
is_rendered_fade (const char* clipName);
static int
is_sample_accurate_edit (const char* clipName);
static int
remove_sampleAccurateEditClip (AAF_Iface* aafi, aafiAudioTrack* audioTrack, aafiTimelineItem* saeItem);
static int
replace_clipFade (AAF_Iface* aafi, aafiAudioTrack* audioTrack, aafiTimelineItem* fadeItem);

int
protools_AAF (struct AAF_Iface* aafi)
{
	int probe = 0;

	/* NOTE: CompanyName is "Digidesign, Inc." at least since ProTools 10.3.10.613, and still today (2024) */

	if (aafi->aafd->Identification.CompanyName && strcmp (aafi->aafd->Identification.CompanyName, "Digidesign, Inc.") == 0) {
		probe++;
	}

	if (aafi->aafd->Identification.ProductName && strcmp (aafi->aafd->Identification.ProductName, "ProTools") == 0) {
		probe++;
	}

	if (probe == 2) {
		return 1;
	}

	return 0;
}

static int
is_rendered_fade (const char* clipName)
{
	return (strcmp (clipName, PROTOOLS_CLIP_NAME_FADE_EN) == 0) ||
	       (strcmp (clipName, PROTOOLS_CLIP_NAME_FADE_ES) == 0) ||
	       (strcmp (clipName, PROTOOLS_CLIP_NAME_FADE_FR) == 0) ||
	       (strcmp (clipName, PROTOOLS_CLIP_NAME_FADE_ZH_CN) == 0) ||
	       (strcmp (clipName, PROTOOLS_CLIP_NAME_FADE_ZH_TW) == 0) ||
	       (strcmp (clipName, PROTOOLS_CLIP_NAME_FADE_KO) == 0);
}

static int
is_sample_accurate_edit (const char* clipName)
{
	return (strcmp (clipName, PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_EN) == 0) ||
	       (strcmp (clipName, PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_DE) == 0) ||
	       (strcmp (clipName, PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_ES) == 0) ||
	       (strcmp (clipName, PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_FR) == 0) ||
	       (strcmp (clipName, PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_JA) == 0) ||
	       (strcmp (clipName, PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_ZH_CN) == 0) ||
	       (strcmp (clipName, PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_ZH_TW) == 0) ||
	       (strcmp (clipName, PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_KO) == 0);
}

static int
remove_sampleAccurateEditClip (AAF_Iface* aafi, aafiAudioTrack* audioTrack, aafiTimelineItem* saeItem)
{
	/*
	 * Note: In this function, we assume we need to expand a clip to remove an
	 * attached sample accurate edit. TODO: Ensure it's always possible with ProTools
	 */

	aafiAudioClip* saeClip = saeItem->data;

	if (saeItem->prev) {
		if (saeItem->prev->type == AAFI_AUDIO_CLIP) {
			aafiAudioClip* leftClip = saeItem->prev->data;

			if (saeClip->pos == (leftClip->pos + leftClip->len)) {
				aafPosition_t essenceLength = aafi_convertUnit (leftClip->essencePointerList->essenceFile->length, leftClip->essencePointerList->essenceFile->samplerateRational, leftClip->track->edit_rate);

				if ((essenceLength - leftClip->essence_offset - leftClip->len) >= saeClip->len) {
					debug ("Removing SAE \"%s\" : left clip \"%s\" goes from length %" PRIi64 " to %" PRIi64,
					       saeClip->essencePointerList->essenceFile->unique_name,
					       leftClip->essencePointerList->essenceFile->unique_name,
					       leftClip->len,
					       leftClip->len + saeClip->len);

					leftClip->len += saeClip->len;

					aafi_removeTimelineItem (aafi, saeItem);

					audioTrack->clipCount--;
					return 1;
				}
				// else {
				// 	debug( L"SAE \"%s\" : left clip \"%s\" has not enough right handle : %lu but %lu is required",
				// 		saeClip->essencePointerList->essenceFile->unique_name,
				// 		leftClip->essencePointerList->essenceFile->unique_name,
				// 		(essenceLength - leftClip->essence_offset - leftClip->len),
				// 		saeClip->len );
				// }
			}
		}
	}

	if (saeItem->next) {
		if (saeItem->next->type == AAFI_AUDIO_CLIP) {
			aafiAudioClip* rightClip = saeItem->next->data;

			if ((saeClip->pos + saeClip->len) == rightClip->pos) {
				if (rightClip->essence_offset >= saeClip->len) {
					debug ("Removing SAE \"%s\" : right clip \"%s\" goes from length: %" PRIi64 " to %" PRIi64 ", pos: %" PRIi64 " to %" PRIi64 ", source offset: %" PRIi64 " to %" PRIi64,
					       saeClip->essencePointerList->essenceFile->unique_name,
					       rightClip->essencePointerList->essenceFile->unique_name,
					       rightClip->len,
					       rightClip->len + saeClip->len,
					       rightClip->pos,
					       rightClip->pos - saeClip->len,
					       rightClip->essence_offset,
					       rightClip->essence_offset - saeClip->len);

					rightClip->pos -= saeClip->len;
					rightClip->len += saeClip->len;
					rightClip->essence_offset -= saeClip->len;

					aafi_removeTimelineItem (aafi, saeItem);

					audioTrack->clipCount--;
					return 1;
				}
				// else {
				// 	debug( L"SAE \"%s\" : right clip \"%s\" has not enough left handle : %lu but %lu is required",
				// 		saeClip->essencePointerList->essenceFile->unique_name,
				// 		rightClip->essencePointerList->essenceFile->unique_name,
				// 		rightClip->essence_offset,
				// 		saeClip->len );
				// }
			}
		}
	}

	return 0;
}

static int
replace_clipFade (AAF_Iface* aafi, aafiAudioTrack* audioTrack, aafiTimelineItem* fadeItem)
{
	aafiAudioClip* fadeClip = fadeItem->data;

	aafiTimelineItem* prevItem1 = fadeItem->prev;
	aafiTimelineItem* prevItem2 = (prevItem1 && prevItem1->prev) ? prevItem1->prev : NULL;

	aafiTimelineItem* nextItem1 = fadeItem->next;
	aafiTimelineItem* nextItem2 = (nextItem1 && nextItem1->next) ? nextItem1->next : NULL;

	aafiAudioClip* prevClip = NULL;
	aafiAudioClip* nextClip = NULL;

	if (prevItem1 && prevItem1->type == AAFI_AUDIO_CLIP) {
		prevClip = prevItem1->data;

		if (fadeClip->pos == (prevClip->pos + prevClip->len)) {
			/* a previous clip is touching this fadeClip on the left */

			if (is_sample_accurate_edit (prevClip->essencePointerList->essenceFile->name)) {
				remove_sampleAccurateEditClip (aafi, audioTrack, prevItem1);

				if (prevItem2 && prevItem2->type == AAFI_AUDIO_CLIP) {
					aafiAudioClip* prevClip2 = prevItem2->data;

					if (fadeClip->pos == (prevClip2->pos + prevClip2->len)) {
						prevClip = prevClip2;
						debug ("Got a clip \"%s\" preceding fadeClip \"%s\"",
						       prevClip->essencePointerList->essenceFile->unique_name,
						       fadeClip->essencePointerList->essenceFile->unique_name);
					} else {
						prevClip = NULL;
					}
				} else {
					prevClip = NULL;
				}
			} else {
				debug ("Got a clip \"%s\" preceding fadeClip \"%s\"",
				       prevClip->essencePointerList->essenceFile->unique_name,
				       fadeClip->essencePointerList->essenceFile->unique_name);
			}
		} else {
			prevClip = NULL;
		}
	}

	if (nextItem1 && nextItem1->type == AAFI_AUDIO_CLIP) {
		nextClip = nextItem1->data;

		if ((fadeClip->pos + fadeClip->len) == nextClip->pos) {
			/* a following clip is touching this fadeClip on the right */

			if (is_sample_accurate_edit (nextClip->essencePointerList->essenceFile->name)) {
				remove_sampleAccurateEditClip (aafi, audioTrack, nextItem1);

				if (nextItem2 && nextItem2->type == AAFI_AUDIO_CLIP) {
					aafiAudioClip* nextClip2 = nextItem2->data;

					if ((fadeClip->pos + fadeClip->len) == nextClip2->pos) {
						nextClip = nextClip2;
						debug ("Got a clip \"%s\" following fadeClip \"%s\"",
						       nextClip->essencePointerList->essenceFile->unique_name,
						       fadeClip->essencePointerList->essenceFile->unique_name);
					} else {
						nextClip = NULL;
					}
				} else {
					nextClip = NULL;
				}
			} else {
				debug ("Got a clip \"%s\" following fadeClip \"%s\"",
				       nextClip->essencePointerList->essenceFile->unique_name,
				       fadeClip->essencePointerList->essenceFile->unique_name);
			}
		} else {
			nextClip = NULL;
		}
	}

	if (!prevClip && !nextClip) {
		debug ("FadeClip \"%s\" is not surrounding by any touching clip",
		       fadeClip->essencePointerList->essenceFile->unique_name);
		return 0;
	}

	/*
	 * Ensures we have enough handle in surrounding clips to expand by fade length
	 */

	if (prevClip) {
		aafPosition_t essenceLength = aafi_convertUnit (prevClip->essencePointerList->essenceFile->length, prevClip->essencePointerList->essenceFile->samplerateRational, prevClip->track->edit_rate);

		if ((essenceLength - prevClip->essence_offset - prevClip->len) < fadeClip->len) {
			warning ("Previous clip \"%s\" has not enough handle to build a fade in place of \"%s\"",
			         prevClip->essencePointerList->essenceFile->unique_name,
			         fadeClip->essencePointerList->essenceFile->unique_name);
			return -1;
		}
	}

	if (nextClip) {
		if (nextClip->essence_offset < fadeClip->len) {
			warning ("Next clip \"%s\" has not enough handle to build a fade in place of \"%s\"",
			         nextClip->essencePointerList->essenceFile->unique_name,
			         fadeClip->essencePointerList->essenceFile->unique_name);
			return -1;
		}
	}

	debug ("Replacing fadeClip \"%s\" with a %s transition of length %" PRIi64,
	       fadeClip->essencePointerList->essenceFile->unique_name,
	       (prevClip && nextClip) ? "X-Fade" : (nextClip) ? "FadeIn"
	                                                      : "FadeOut",
	       fadeClip->len);

	/*
	 * changes existing aafiTimelineItem from aafiAudioClip into aafiTransition
	 */

	aafPosition_t fadeClipLength = fadeClip->len;

	fadeItem->type = AAFI_TRANS;

	aafi_freeAudioClip (fadeItem->data);

	fadeItem->data = calloc (1, sizeof (aafiTransition));

	if (!fadeItem->data) {
		error ("Out of memory");
		aafi_removeTimelineItem (aafi, fadeItem);
		audioTrack->clipCount--;
		return 1; /* important ! */
	}

	aafiTransition* trans = fadeItem->data;

	trans->len   = fadeClipLength;
	trans->flags = AAFI_INTERPOL_LINEAR;

	trans->time_a  = calloc (2, sizeof (aafRational_t));
	trans->value_a = calloc (2, sizeof (aafRational_t));

	if (!trans->time_a || !trans->value_a) {
		error ("Out of memory");
		aafi_removeTimelineItem (aafi, fadeItem);
		audioTrack->clipCount--;
		return 1; /* important ! */
	}

	if (prevClip && nextClip) {
		prevClip->len += fadeClipLength;

		nextClip->pos -= fadeClipLength;
		nextClip->len += fadeClipLength;
		nextClip->essence_offset -= fadeClipLength;

		trans->flags |= AAFI_TRANS_XFADE;
		trans->cut_pt = fadeClipLength / 2;

		trans->value_a[0].numerator   = 0;
		trans->value_a[0].denominator = 0;
		trans->value_a[1].numerator   = 1;
		trans->value_a[1].denominator = 1;
	} else if (prevClip) {
		prevClip->len += fadeClipLength;

		trans->flags |= AAFI_TRANS_FADE_OUT;
		trans->cut_pt = fadeClipLength;

		trans->value_a[0].numerator   = 1;
		trans->value_a[0].denominator = 1;
		trans->value_a[1].numerator   = 0;
		trans->value_a[1].denominator = 0;
	} else if (nextClip) {
		nextClip->pos -= fadeClipLength;
		nextClip->len += fadeClipLength;
		nextClip->essence_offset -= fadeClipLength;

		trans->flags |= AAFI_TRANS_FADE_IN;
		trans->cut_pt = 0;

		trans->value_a[0].numerator   = 0;
		trans->value_a[0].denominator = 0;
		trans->value_a[1].numerator   = 1;
		trans->value_a[1].denominator = 1;
	}

	audioTrack->clipCount--;

	return 1;
}

int
protools_post_processing (AAF_Iface* aafi)
{
	aafiAudioTrack* audioTrack = NULL;

	AAFI_foreachAudioTrack (aafi, audioTrack)
	{
		aafiTimelineItem* audioItem = audioTrack->timelineItems;

		while (audioItem != NULL) {
			aafiTimelineItem* audioItemNext = audioItem->next;

			if (audioItem->type != AAFI_AUDIO_CLIP) {
				audioItem = audioItemNext;
				continue;
			}

			aafiAudioClip* audioClip = (aafiAudioClip*)audioItem->data;

			char* clipName = audioClip->essencePointerList->essenceFile->name;

			int previousClipCount = audioTrack->clipCount;

			if ((aafi->ctx.options.protools & AAFI_PROTOOLS_OPT_REPLACE_CLIP_FADES) &&
			    is_rendered_fade (clipName)) {
				replace_clipFade (aafi, audioTrack, audioItem);

				if (previousClipCount != audioTrack->clipCount) {
					audioItem = audioTrack->timelineItems;
					continue;
				}
			} else if ((aafi->ctx.options.protools & AAFI_PROTOOLS_OPT_REMOVE_SAMPLE_ACCURATE_EDIT) &&
			           is_sample_accurate_edit (clipName)) {
				remove_sampleAccurateEditClip (aafi, audioTrack, audioItem);

				if (previousClipCount != audioTrack->clipCount) {
					audioItem = audioTrack->timelineItems;
					continue;
				}
			}

			audioItem = audioItemNext;
		}
	}

	return 0;
}
