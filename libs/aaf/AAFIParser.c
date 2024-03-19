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
 * @file LibAAF/AAFIface/AAFIParser.c
 * @brief AAF processing
 * @author Adrien Gesta-Fline
 * @version 0.1
 * @date 27 june 2018
 *
 * @ingroup AAFIface
 * @addtogroup AAFIface
 *
 *
 *
 *
 * @{
 */

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aaf/AAFDefs/AAFClassDefUIDs.h"
#include "aaf/AAFDefs/AAFDataDefs.h"
#include "aaf/AAFDefs/AAFExtEnum.h"
#include "aaf/AAFDefs/AAFInterpolatorDefs.h"
#include "aaf/AAFDefs/AAFOPDefs.h"
#include "aaf/AAFDefs/AAFOperationDefs.h"
#include "aaf/AAFDefs/AAFParameterDefs.h"
#include "aaf/AAFDefs/AAFPropertyIDs.h"
#include "aaf/AAFDefs/AAFTypeDefUIDs.h"

#include "aaf/AAFDump.h"
#include "aaf/AAFIEssenceFile.h"
#include "aaf/AAFIParser.h"
#include "aaf/AAFIface.h"
#include "aaf/AAFToText.h"

#include "aaf/MediaComposer.h"
#include "aaf/ProTools.h"
#include "aaf/Resolve.h"

#include "aaf/log.h"
#include "aaf/utils.h"

#define debug(...) \
	AAF_LOG (aafi->log, aafi, LOG_SRC_ID_AAF_IFACE, VERB_DEBUG, __VA_ARGS__)

#define warning(...) \
	AAF_LOG (aafi->log, aafi, LOG_SRC_ID_AAF_IFACE, VERB_WARNING, __VA_ARGS__)

#define error(...) \
	AAF_LOG (aafi->log, aafi, LOG_SRC_ID_AAF_IFACE, VERB_ERROR, __VA_ARGS__)

static aafRational_t AAFI_DEFAULT_TC_EDIT_RATE = { 25, 1 };

#define RESET_CTX__AudioGain(ctx)                      \
	ctx.current_clip_is_muted              = 0;    \
	ctx.current_clip_gain                  = NULL; \
	ctx.current_clip_gain_is_used          = 0;    \
	ctx.current_clip_variable_gain         = NULL; \
	ctx.current_clip_variable_gain_is_used = 0;

#define RESET_CTX__AudioChannelCombiner(ctx)         \
	ctx.current_clip_is_combined            = 0; \
	ctx.current_combined_clip_total_channel = 0; \
	ctx.current_combined_clip_channel_num   = 0; \
	ctx.current_combined_clip_forced_length = 0;

#define RESET_CONTEXT(ctx)                \
	ctx.current_track         = NULL; \
	ctx.current_audio_essence = NULL; \
	ctx.current_clip          = NULL; \
	RESET_CTX__AudioGain (ctx);       \
	RESET_CTX__AudioChannelCombiner (ctx);

static int
parse_Mob (AAF_Iface* aafi, aafObject* Mob, td* __ptd);
static int
parse_CompositionMob (AAF_Iface* aafi, aafObject* CompoMob, td* __ptd);
static int
parse_SourceMob (AAF_Iface* aafi, aafObject* SourceMob, td* __ptd);

static int
parse_MobSlot (AAF_Iface* aafi, aafObject* MobSlot, td* __ptd);
static int
parse_TimelineMobSlot (AAF_Iface* aafi, aafObject* TimelineMobSlot, td* __ptd);
static int
parse_EventMobSlot (AAF_Iface* aafi, aafObject* EventMobSlot, td* __ptd);

static int
parse_Component (AAF_Iface* aafi, aafObject* Component, td* __ptd);

static int
parse_Transition (AAF_Iface* aafi, aafObject* Transition, td* __ptd);
static int
parse_Segment (AAF_Iface* aafi, aafObject* Segment, td* __ptd);

static int
parse_Filler (AAF_Iface* aafi, aafObject* Filler, td* __ptd);
static int
parse_SourceClip (AAF_Iface* aafi, aafObject* SourceClip, td* __ptd);
static int
parse_Timecode (AAF_Iface* aafi, aafObject* Timecode, td* __ptd);
static int
parse_DescriptiveMarker (AAF_Iface* aafi, aafObject* DescriptiveMarker, td* __ptd);
static int
parse_Sequence (AAF_Iface* aafi, aafObject* Sequence, td* __ptd);
static int
parse_Selector (AAF_Iface* aafi, aafObject* Selector, td* __ptd);
static int
parse_NestedScope (AAF_Iface* aafi, aafObject* NestedScope, td* __ptd);
static int
parse_OperationGroup (AAF_Iface* aafi, aafObject* OpGroup, td* __ptd);

static int
parse_Parameter (AAF_Iface* aafi, aafObject* Parameter, td* __ptd);
static int
parse_ConstantValue (AAF_Iface* aafi, aafObject* ConstantValue, td* __ptd);
static int
parse_VaryingValue (AAF_Iface* aafi, aafObject* VaryingValue, td* __ptd);

static int
parse_EssenceDescriptor (AAF_Iface* aafi, aafObject* EssenceDesc, td* __ptd);
static int
parse_PCMDescriptor (AAF_Iface* aafi, aafObject* PCMDescriptor, td* __ptd);
static int
parse_WAVEDescriptor (AAF_Iface* aafi, aafObject* WAVEDescriptor, td* __ptd);
static int
parse_AIFCDescriptor (AAF_Iface* aafi, aafObject* AIFCDescriptor, td* __ptd);

static int
parse_DigitalImageDescriptor (AAF_Iface* aafi, aafObject* DIDescriptor, td* __ptd);
static int
parse_CDCIDescriptor (AAF_Iface* aafi, aafObject* CDCIDescriptor, td* __ptd);

static int
parse_Locator (AAF_Iface* aafi, aafObject* Locator, td* __ptd);
static int
parse_NetworkLocator (AAF_Iface* aafi, aafObject* NetworkLocator, td* __ptd);

static int
parse_EssenceData (AAF_Iface* aafi, aafObject* EssenceData, td* __ptd);

static int
retrieve_UserComments (AAF_Iface* aafi, aafObject* UserComments, aafiMetaData** metadataList);
static int
retrieve_ControlPoints (AAF_Iface* aafi, aafObject* Points, aafRational_t* times[], aafRational_t* values[]);

/* ****************************************************************************
 *                                 M o b
 * ****************************************************************************

 *                            Mob (abs)
 *                             |
 *                             |--> CompositionMob
 *                             |--> MasterMob
 *                             `--> SourceMob
 */

static int
parse_Mob (AAF_Iface* aafi, aafObject* Mob, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 1);

	int rc = 0;

	aafObject* MobSlots = aaf_get_propertyValue (Mob, PID_Mob_Slots, &AAFTypeID_MobSlotStrongReferenceVector);

	if (!MobSlots) {
		TRACE_OBJ_ERROR (aafi, Mob, &__td, "Missing Mob::Slots");
		goto err;
	}

	if (aafUIDCmp (Mob->Class->ID, &AAFClassID_CompositionMob)) {
		parse_CompositionMob (aafi, Mob, &__td);
	} else if (aafUIDCmp (Mob->Class->ID, &AAFClassID_SourceMob)) {
		parse_SourceMob (aafi, Mob, &__td);
	} else {
		/* NOTE: MasterMob is accessed directly from parse_SourceClip() */
		TRACE_OBJ_NO_SUPPORT (aafi, Mob, &__td);
	}

	aafObject* MobSlot = NULL;

	uint32_t i = 0;
	AAFI_foreach_ObjectInSet (&MobSlot, MobSlots, i, __td)
	{
		parse_MobSlot (aafi, MobSlot, &__td);
	}

	goto end;

err:
	rc = -1;

end:
	return rc;
}

static int
parse_CompositionMob (AAF_Iface* aafi, aafObject* CompoMob, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 0);

	aafUID_t* UsageCode = aaf_get_propertyValue (CompoMob, PID_Mob_UsageCode, &AAFTypeID_UsageType);

	if ((aafUIDCmp (aafi->aafd->Header.OperationalPattern, &AAFOPDef_EditProtocol) && aafUIDCmp (UsageCode, &AAFUsage_TopLevel)) ||
	    (!aafUIDCmp (aafi->aafd->Header.OperationalPattern, &AAFOPDef_EditProtocol) && (aafUIDCmp (UsageCode, &AAFUsage_TopLevel) || !UsageCode))) {
		aafi->ctx.TopLevelCompositionMob = CompoMob;
		aafi->compositionName            = aaf_get_propertyValue (CompoMob, PID_Mob_Name, &AAFTypeID_String);

		aafObject* UserComments = aaf_get_propertyValue (CompoMob, PID_Mob_UserComments, &AAFTypeID_TaggedValueStrongReferenceVector);

		if (retrieve_UserComments (aafi, UserComments, &aafi->metadata) < 0) {
			TRACE_OBJ_WARNING (aafi, CompoMob, &__td, "Error parsing Mob::UserComments");
		} else {
			TRACE_OBJ (aafi, CompoMob, &__td);
		}
	} else {
		TRACE_OBJ_NO_SUPPORT (aafi, CompoMob, &__td);
	}

	return 0;
}

static int
parse_SourceMob (AAF_Iface* aafi, aafObject* SourceMob, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 0);

	aafMobID_t* MobID = aaf_get_propertyValue (SourceMob, PID_Mob_MobID, &AAFTypeID_MobIDType);

	if (!MobID) {
		TRACE_OBJ_ERROR (aafi, SourceMob, &__td, "Missing Mob::MobID");
		return -1;
	}

	aafTimeStamp_t* CreationTime = aaf_get_propertyValue (SourceMob, PID_Mob_CreationTime, &AAFTypeID_TimeStamp);

	if (!CreationTime) {
		TRACE_OBJ_ERROR (aafi, SourceMob, &__td, "Missing Mob::CreationTime");
		return -1;
	}

	aafObject* EssenceDesc = aaf_get_propertyValue (SourceMob, PID_SourceMob_EssenceDescription, &AAFTypeID_EssenceDescriptorStrongReference);

	if (!EssenceDesc) {
		TRACE_OBJ_ERROR (aafi, SourceMob, &__td, "Missing SourceMob::EssenceDescription");
		return -1;
	}

	TRACE_OBJ (aafi, SourceMob, &__td);

	/*
	 * SourceMob can be parsed for Audio and Video.
	 * aafi->ctx.current_audio_essence is set, it means we are parsing Audio, so
	 * we need to retrieve more data.
	 */

	if (aafi->ctx.current_audio_essence) {
		aafiAudioEssenceFile* audioEssenceFile = (aafiAudioEssenceFile*)aafi->ctx.current_audio_essence;

		memcpy (audioEssenceFile->umid, MobID, sizeof (aafMobID_t));

		int rc = snprintf (audioEssenceFile->originationDate, sizeof (((aafiAudioEssenceFile*)0)->originationDate), "%04u:%02u:%02u",
		                   (CreationTime->date.year <= 9999) ? CreationTime->date.year : 0,
		                   (CreationTime->date.month <= 99) ? CreationTime->date.month : 0,
		                   (CreationTime->date.day <= 99) ? CreationTime->date.day : 0);

		assert (rc > 0 && (size_t)rc < sizeof (((aafiAudioEssenceFile*)0)->originationDate));

		rc = snprintf (audioEssenceFile->originationTime, sizeof (((aafiAudioEssenceFile*)0)->originationTime), "%02u:%02u:%02u",
		               (CreationTime->time.hour <= 99) ? CreationTime->time.hour : 0,
		               (CreationTime->time.minute <= 99) ? CreationTime->time.minute : 0,
		               (CreationTime->time.second <= 99) ? CreationTime->time.second : 0);

		assert (rc > 0 && (size_t)rc < sizeof (((aafiAudioEssenceFile*)0)->originationTime));
	}

	__td.ll[__td.lv] = 2;
	parse_EssenceDescriptor (aafi, EssenceDesc, &__td);
	__td.ll[__td.lv] = 1;

	return 0;
}

/* ****************************************************************************
 *                             M o b S l o t
 * ****************************************************************************
 *
 *                          MobSlot (abs)
 *                             |
 *                             |--> TimelineMobSlot
 *                             |--> EventMobSlot
 *                             `--> StaticMobSlot
 */

static int
parse_MobSlot (AAF_Iface* aafi, aafObject* MobSlot, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 1);

	aafObject* Segment = aaf_get_propertyValue (MobSlot, PID_MobSlot_Segment, &AAFTypeID_SegmentStrongReference);

	if (!Segment) {
		TRACE_OBJ_ERROR (aafi, MobSlot, &__td, "Missing MobSlot::Segment");
		return -1;
	}

	if (aafUIDCmp (MobSlot->Class->ID, &AAFClassID_TimelineMobSlot)) {
		if (parse_TimelineMobSlot (aafi, MobSlot, &__td) < 0) {
			return -1;
		}
	} else if (aafUIDCmp (MobSlot->Class->ID, &AAFClassID_EventMobSlot)) {
		if (parse_EventMobSlot (aafi, MobSlot, &__td) < 0) {
			return -1;
		}
	} else {
		TRACE_OBJ_NO_SUPPORT (aafi, MobSlot, &__td);
		return -1;
	}

	parse_Segment (aafi, Segment, &__td);

	return 0;
}

static int
parse_TimelineMobSlot (AAF_Iface* aafi, aafObject* TimelineMobSlot, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 0);

	aafObject* ParentMob = aaf_get_ObjectAncestor (TimelineMobSlot, &AAFClassID_Mob);

	if (!ParentMob) {
		TRACE_OBJ_ERROR (aafi, TimelineMobSlot, &__td, "Could not retrieve parent Mob");
		return -1;
	}

	uint32_t* track_num = aaf_get_propertyValue (TimelineMobSlot, PID_MobSlot_PhysicalTrackNumber, &AAFTypeID_UInt32);

	if (!track_num) {
		debug ("Missing MobSlot::PhysicalTrackNumber");
	}

	aafObject* Segment = aaf_get_propertyValue (TimelineMobSlot, PID_MobSlot_Segment, &AAFTypeID_SegmentStrongReference);

	if (!Segment) {
		TRACE_OBJ_ERROR (aafi, TimelineMobSlot, &__td, "Missing MobSlot::Segment");
		return -1;
	}

	aafWeakRef_t* dataDefWeakRef = aaf_get_propertyValue (Segment, PID_Component_DataDefinition, &AAFTypeID_DataDefinitionWeakReference);

	if (!dataDefWeakRef) {
		TRACE_OBJ_ERROR (aafi, Segment, &__td, "Could not retrieve Component::DataDefinition from Segment child");
		return -1;
	}

	aafUID_t* DataDefinition = aaf_get_DataIdentificationByWeakRef (aafi->aafd, dataDefWeakRef);

	if (!DataDefinition) {
		TRACE_OBJ_ERROR (aafi, TimelineMobSlot, &__td, "Could not retrieve DataDefinition");
		return -1;
	}

	aafRational_t* edit_rate = aaf_get_propertyValue (TimelineMobSlot, PID_TimelineMobSlot_EditRate, &AAFTypeID_Rational);

	if (!edit_rate) {
		TRACE_OBJ_ERROR (aafi, TimelineMobSlot, &__td, "Missing TimelineMobSlot::EditRate");
		return -1;
	}

	if (aafUIDCmp (ParentMob->Class->ID, &AAFClassID_CompositionMob)) {
		/*
		 * Each TimelineMobSlot represents a track, either audio or video.
		 *
		 * The Timeline MobSlot::Segment should hold a Sequence of Components.
		 * This Sequence represents the timeline track. Therefore, each SourceClip
		 * contained in the Sequence::Components represents a clip on the timeline.
		 *
		 * CompositionMob can have TimelineMobSlots, StaticMobSlots, EventMobSlots
		 */

		/*
		 * TODO: implement multiple TopLevel compositions support
		 */

		if (aafUIDCmp (DataDefinition, &AAFDataDef_Sound) ||
		    aafUIDCmp (DataDefinition, &AAFDataDef_LegacySound)) {
			/*
			 * « In a CompositionMob or MasterMob, PhysicalTrackNumber is the output
			 * channel number that the MobSlot should be routed to when played. »
			 */

			if (aafi->ctx.TopLevelCompositionMob == ParentMob /*!parentMobUsageCode || aafUIDCmp( parentMobUsageCode, &AAFUsage_TopLevel )*/ /*!aafi->ctx.is_inside_derivation_chain*/) {
				TRACE_OBJ (aafi, TimelineMobSlot, &__td);

				uint32_t tracknumber = (track_num) ? *track_num : (aafi->Audio->track_count + 1);

				aafiAudioTrack* track = aafi_newAudioTrack (aafi);

				track->number    = tracknumber;
				track->name      = aaf_get_propertyValue (TimelineMobSlot, PID_MobSlot_SlotName, &AAFTypeID_String);
				track->edit_rate = edit_rate;

				aafi->Audio->track_count += 1;

				aafi->ctx.current_track = track;

				/*
				 * Avid Media Composer
				 */

				void* TimelineMobAttributeList = aaf_get_propertyValue (TimelineMobSlot, aaf_get_PropertyIDByName (aafi->aafd, "TimelineMobAttributeList"), &AAFTypeID_TaggedValueStrongReferenceVector);

				if (TimelineMobAttributeList) {
					int32_t* solo = aaf_get_TaggedValueByName (aafi->aafd, TimelineMobAttributeList, "AudioMixerCompSolo", &AAFTypeID_Int32);
					int32_t* mute = aaf_get_TaggedValueByName (aafi->aafd, TimelineMobAttributeList, "AudioMixerCompMute", &AAFTypeID_Int32);

					if (solo && *solo) {
						track->solo = 1;
					}

					if (mute && *mute) {
						track->mute = 1;
					}
				}
			} else {
				TRACE_OBJ (aafi, TimelineMobSlot, &__td);
			}
		} else if (aafUIDCmp (DataDefinition, &AAFDataDef_Picture) ||
		           aafUIDCmp (DataDefinition, &AAFDataDef_LegacyPicture)) {
			if (aafi->Video->Tracks) {
				TRACE_OBJ_ERROR (aafi, TimelineMobSlot, &__td, "Current implementation supports only one video track");
				return -1;
			}

			TRACE_OBJ (aafi, TimelineMobSlot, &__td);

			uint32_t tracknumber = (track_num) ? *track_num : 1 /* current implementation supports only one video track */;

			aafiVideoTrack* track = aafi_newVideoTrack (aafi);

			track->number    = tracknumber;
			track->name      = aaf_get_propertyValue (TimelineMobSlot, PID_MobSlot_SlotName, &AAFTypeID_String);
			track->edit_rate = edit_rate;
		} else if (aafUIDCmp (DataDefinition, &AAFDataDef_Timecode) ||
		           aafUIDCmp (DataDefinition, &AAFDataDef_LegacyTimecode)) {
			TRACE_OBJ (aafi, TimelineMobSlot, &__td);
		} else if (aafUIDCmp (DataDefinition, &AAFDataDef_DescriptiveMetadata)) {
			/*
			 * Avid Media Composer 23.12, markers with duration (no-duration markers are held by AAFClassID_EventMobSlot)
			 *
		│ 04179│     └──◻ AAFClassID_TimelineMobSlot [slot:3003 track:12] (DataDef: AAFDataDef_DescriptiveMetadata) : Segmentation
		│ 01946│          └──◻ AAFClassID_Sequence (Length: 4)
		│ 03031│               └──◻ AAFClassID_DescriptiveMarker (Length: 4)   (MetaProps: CommentMarkerAttributeList[0xffd4] CommentMarkerColor[0xffdb] CommentMarkerUSer[0xffda] CommentMarkerDate[0xffd9] CommentMarkerTime[0xffd8])
			 */

			TRACE_OBJ (aafi, TimelineMobSlot, &__td);
		} else {
			TRACE_OBJ_NO_SUPPORT (aafi, TimelineMobSlot, &__td);
			return -1;
		}
	} else if (aafUIDCmp (ParentMob->Class->ID, &AAFClassID_MasterMob)) {
		TRACE_OBJ (aafi, TimelineMobSlot, &__td);
	} else if (aafUIDCmp (ParentMob->Class->ID, &AAFClassID_SourceMob)) {
		/*
		 * SourceMob can be parsed for Audio and Video.
		 * aafi->ctx.current_audio_essence is set, it means we are parsing Audio, so
		 * we need to retrieve more data.
		 */

		if (aafi->ctx.current_audio_essence) {
			aafPosition_t* Origin = aaf_get_propertyValue (TimelineMobSlot, PID_TimelineMobSlot_Origin, &AAFTypeID_PositionType);

			if (!Origin) {
				TRACE_OBJ_ERROR (aafi, TimelineMobSlot, &__td, "Missing TimelineMobSlot::Origin");
				return -1;
			}

			TRACE_OBJ (aafi, TimelineMobSlot, &__td);

			aafiAudioEssenceFile* audioEssenceFile = (aafiAudioEssenceFile*)aafi->ctx.current_audio_essence;

			audioEssenceFile->sourceMobSlotOrigin   = *Origin;
			audioEssenceFile->sourceMobSlotEditRate = edit_rate;
		} else {
			TRACE_OBJ (aafi, TimelineMobSlot, &__td);
		}
	} else {
		/*
		 * AAFClassID_MasterMob and AAFClassID_SourceMob are accessed directly from TimelineMobSlot > SourceClip
		 */
		TRACE_OBJ_NO_SUPPORT (aafi, TimelineMobSlot, &__td);
		return -1;
	}

	return 0;
}

static int
parse_EventMobSlot (AAF_Iface* aafi, aafObject* EventMobSlot, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 0);

	aafRational_t* edit_rate = aaf_get_propertyValue (EventMobSlot, PID_EventMobSlot_EditRate, &AAFTypeID_Rational);

	if (!edit_rate) {
		TRACE_OBJ_ERROR (aafi, EventMobSlot, &__td, "Missing EventMobSlot::EditRate");
		return -1;
	}

	TRACE_OBJ (aafi, EventMobSlot, &__td);

	aafi->ctx.current_markers_edit_rate = edit_rate;

	return 0;
}

/* ****************************************************************************
 *                           C o m p o n e n t
 * ****************************************************************************
 *
 *                     Component (abs)
 *                          |
 *                    ,-----------.
 *                    |           |
 *               Transition    Segment (abs)
 *                                |
 *                                |--> Sequence
 *                                |--> Filler
 *                                |--> TimeCode
 *                                |--> OperationGroup
 *                                `--> SourceReference (abs)
 *                                            |
 *                                            `--> SourceClip
 */

static int
parse_Component (AAF_Iface* aafi, aafObject* Component, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 0);

	if (aafUIDCmp (Component->Class->ID, &AAFClassID_Transition)) {
		parse_Transition (aafi, Component, &__td);
	} else {
		parse_Segment (aafi, Component, &__td);
	}

	return 0;
}

static int
parse_Transition (AAF_Iface* aafi, aafObject* Transition, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 1);

	/*
	 * A Transition between a Filler and a SourceClip sets a Fade In.
	 * A Transition between a SourceClip and a Filler sets a Fade Out.
	 * A Transition between two SourceClips sets a Cross-Fade.
	 *
	 * Since the Transition applies to the elements that suround it in
	 * the Sequence, the OperationGroup::InputSegments is left unused.
	 */

	aafWeakRef_t* dataDefWeakRef = aaf_get_propertyValue (Transition, PID_Component_DataDefinition, &AAFTypeID_DataDefinitionWeakReference);

	if (!dataDefWeakRef) {
		TRACE_OBJ_ERROR (aafi, Transition, &__td, "Missing Component::DataDefinition.");
		return -1;
	}

	aafUID_t* DataDefinition = aaf_get_DataIdentificationByWeakRef (aafi->aafd, dataDefWeakRef);

	if (!DataDefinition) {
		TRACE_OBJ_ERROR (aafi, Transition, &__td, "Could not retrieve DataDefinition");
		return -1;
	}

	if (!aafUIDCmp (DataDefinition, &AAFDataDef_Sound) &&
	    !aafUIDCmp (DataDefinition, &AAFDataDef_LegacySound)) {
		TRACE_OBJ_ERROR (aafi, Transition, &__td, "Current implementation does not support video Transitions");
		return -1;
	}

	int64_t* length = aaf_get_propertyValue (Transition, PID_Component_Length, &AAFTypeID_LengthType);

	if (!length) {
		TRACE_OBJ_ERROR (aafi, Transition, &__td, "Missing Component::Length");
		return -1;
	}

	aafObject* OpGroup = aaf_get_propertyValue (Transition, PID_Transition_OperationGroup, &AAFTypeID_OperationGroupStrongReference);

	if (!OpGroup) {
		TRACE_OBJ_ERROR (aafi, Transition, &__td, "Missing Transition::OperationGroup");
		return -1;
	}

	aafPosition_t* cutPoint = aaf_get_propertyValue (Transition, PID_Transition_CutPoint, &AAFTypeID_PositionType);

	if (!cutPoint) {
		/* not encountered though */
		debug ("Missing Transition::CutPoint : setting cut point to Transition::Length/2");
	}

	uint32_t fadeType = 0;

	if (Transition->prev != NULL && aafUIDCmp (Transition->prev->Class->ID, &AAFClassID_Filler)) {
		fadeType |= AAFI_TRANS_FADE_IN;
	} else if (Transition->next != NULL && aafUIDCmp (Transition->next->Class->ID, &AAFClassID_Filler)) {
		fadeType |= AAFI_TRANS_FADE_OUT;
	} else if (Transition->next != NULL && aafUIDCmp (Transition->next->Class->ID, &AAFClassID_Filler) == 0 &&
	           Transition->prev != NULL && aafUIDCmp (Transition->prev->Class->ID, &AAFClassID_Filler) == 0) {
		fadeType |= AAFI_TRANS_XFADE;
	} else {
		TRACE_OBJ_ERROR (aafi, Transition, &__td, "Could not guess if type is FadeIn, FadeOut or xFade");
		return -1;
	}

	TRACE_OBJ (aafi, Transition, &__td);

	aafiTransition* Trans = aafi_newTransition (aafi, aafi->ctx.current_track);

	Trans->len    = *length;
	Trans->flags  = fadeType;
	Trans->cut_pt = (cutPoint) ? *cutPoint : (Trans->len / 2);

	/*
	 * OperationGroup *might* contain a Parameter (ParameterDef_Level) specifying
	 * the fade curve. However, this parameter is optional regarding AAF_EditProtocol
	 * and there is most likely no implementation that exports custom fade curves.
	 * Thus, we only retrieve ParameterDef_Level to possibly set interpolation, and we
	 * always set the fade as defined in AAF_EditProtocol, with only two points :
	 *
	 * « ParameterDef_Level (optional; default is a VaryingValue object
	 * with two control points: Value 0 at time 0, and value 1 at time 1) »
	 */

	if (fadeType & AAFI_TRANS_FADE_IN ||
	    fadeType & AAFI_TRANS_XFADE) {
		Trans->value_a[0].numerator   = 0;
		Trans->value_a[0].denominator = 0;
		Trans->value_a[1].numerator   = 1;
		Trans->value_a[1].denominator = 1;
	} else if (fadeType & AAFI_TRANS_FADE_OUT) {
		Trans->value_a[0].numerator   = 1;
		Trans->value_a[0].denominator = 1;
		Trans->value_a[1].numerator   = 0;
		Trans->value_a[1].denominator = 0;
	}

	aafi->ctx.current_transition = Trans;

	parse_OperationGroup (aafi, OpGroup, &__td);

	aafi->ctx.current_transition = NULL;
	aafi->ctx.current_track->current_pos -= *length;

	return 0;
}

int
parse_Segment (AAF_Iface* aafi, aafObject* Segment, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 0);

	if (aafUIDCmp (Segment->Class->ID, &AAFClassID_Sequence)) {
		return parse_Sequence (aafi, Segment, &__td);
	} else if (aafUIDCmp (Segment->Class->ID, &AAFClassID_SourceClip)) {
		return parse_SourceClip (aafi, Segment, &__td);
	} else if (aafUIDCmp (Segment->Class->ID, &AAFClassID_OperationGroup)) {
		return parse_OperationGroup (aafi, Segment, &__td);
	} else if (aafUIDCmp (Segment->Class->ID, &AAFClassID_Filler)) {
		return parse_Filler (aafi, Segment, &__td);
	} else if (aafUIDCmp (Segment->Class->ID, &AAFClassID_Selector)) {
		return parse_Selector (aafi, Segment, &__td);
	} else if (aafUIDCmp (Segment->Class->ID, &AAFClassID_NestedScope)) {
		return parse_NestedScope (aafi, Segment, &__td);
	} else if (aafUIDCmp (Segment->Class->ID, &AAFClassID_Timecode)) {
		return parse_Timecode (aafi, Segment, &__td);
	} else if (aafUIDCmp (Segment->Class->ID, &AAFClassID_DescriptiveMarker)) {
		return parse_DescriptiveMarker (aafi, Segment, &__td);
	} else if (aafUIDCmp (Segment->Class->ID, &AAFClassID_EssenceGroup)) {
		/*
		 * Should provide support for multiple essences representing the same
		 * source material with different resolution, compression, codec, etc.
		 *
		 * TODO To be tested with Avid and rendered effects ?
		 */

		__td.lv++;
		TRACE_OBJ_NO_SUPPORT (aafi, Segment, &__td);
		__td.lv--;

		return -1;
	} else {
		__td.lv++;
		TRACE_OBJ_NO_SUPPORT (aafi, Segment, &__td);
		__td.lv--;

		return -1;
	}

	return 0;
}

static int
parse_Filler (AAF_Iface* aafi, aafObject* Filler, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 1);
	__td.eob = 1;

	aafWeakRef_t* dataDefWeakRef = aaf_get_propertyValue (Filler, PID_Component_DataDefinition, &AAFTypeID_DataDefinitionWeakReference);

	if (!dataDefWeakRef) {
		TRACE_OBJ_ERROR (aafi, Filler, &__td, "Missing Component::DataDefinition.");
		return -1;
	}

	aafUID_t* DataDefinition = aaf_get_DataIdentificationByWeakRef (aafi->aafd, dataDefWeakRef);

	if (!DataDefinition) {
		TRACE_OBJ_ERROR (aafi, Filler, &__td, "Could not retrieve DataDefinition");
		return -1;
	}

	/*
	 * This represents an empty space on the timeline, between two clips,
	 * which is Component::Length long.
	 * TODO: is realy parent mandatory a Sequence or Selector ?
	 */

	int64_t* length = aaf_get_propertyValue (Filler, PID_Component_Length, &AAFTypeID_LengthType);

	if (!length) {
		TRACE_OBJ_ERROR (aafi, Filler, &__td, "Missing Component::Length");
		return -1;
	}

	if (aafUIDCmp (DataDefinition, &AAFDataDef_Sound) ||
	    aafUIDCmp (DataDefinition, &AAFDataDef_LegacySound)) {
		aafi->ctx.current_track->current_pos += *length;
	} else if (aafUIDCmp (DataDefinition, &AAFDataDef_Picture) ||
	           aafUIDCmp (DataDefinition, &AAFDataDef_LegacyPicture)) {
		aafi->Video->Tracks->current_pos += *length;
	}

	TRACE_OBJ (aafi, Filler, &__td);

	return 0;
}

static int
parse_SourceClip (AAF_Iface* aafi, aafObject* SourceClip, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 1);

	aafWeakRef_t* dataDefWeakRef = aaf_get_propertyValue (SourceClip, PID_Component_DataDefinition, &AAFTypeID_DataDefinitionWeakReference);

	if (!dataDefWeakRef) {
		TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Missing Component::DataDefinition.");
		return -1;
	}

	aafUID_t* DataDefinition = aaf_get_DataIdentificationByWeakRef (aafi->aafd, dataDefWeakRef);

	if (!DataDefinition) {
		TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Could not retrieve DataDefinition");
		return -1;
	}

	aafObject* ParentMob = aaf_get_ObjectAncestor (SourceClip, &AAFClassID_Mob);

	if (!ParentMob) {
		TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Could not retrieve parent Mob");
		return -1;
	}

	aafMobID_t* parentMobID = aaf_get_propertyValue (ParentMob, PID_Mob_MobID, &AAFTypeID_MobIDType);

	if (!parentMobID) {
		TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Missing parent Mob::MobID");
		return -1;
	}

	aafUID_t* parentMobUsageCode = aaf_get_propertyValue (ParentMob, PID_Mob_UsageCode, &AAFTypeID_UsageType);

	if (!parentMobUsageCode) {
		debug ("Missing parent Mob Mob::UsageCode");
	}

	aafMobID_t* sourceID = aaf_get_propertyValue (SourceClip, PID_SourceReference_SourceID, &AAFTypeID_MobIDType);

	uint32_t* SourceMobSlotID = aaf_get_propertyValue (SourceClip, PID_SourceReference_SourceMobSlotID, &AAFTypeID_UInt32);

	if (!SourceMobSlotID) {
		TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Missing SourceReference::SourceMobSlotID");
		return -1;
	}

	/*
	 * TODO: handle SourceReference::ChannelIDs and SourceReference::MonoSourceSlotIDs
	 * (Multi-channels)
	 */

	aafObject* targetMob     = NULL;
	aafObject* targetMobSlot = NULL;

	if (sourceID == NULL || aafMobIDCmp (sourceID, &AAFMOBID_NULL)) {
		/*
		 * p.49 : To create a SourceReference that refers to a MobSlot within
		 * the same Mob as the SourceReference, omit the SourceID property.
		 *
		 * [SourceID] Identifies the Mob being referenced. If the property has a
		 * value 0, it means that the Mob owning the SourceReference describes
		 * the original source.
		 *
		 * TODO: in that case, is MobSlots NULL ?
		 */

		// sourceID = parentMobID;
		// targetMob = ParentMob;

		debug ("SourceReference::SourceID is missing or NULL");
	} else {
		targetMob = aaf_get_MobByID (aafi->aafd->Mobs, sourceID);

		if (!targetMob) {
			TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Could not retrieve target Mob by ID : %s", aaft_MobIDToText (sourceID));
			return -1;
		}

		aafObject* targetMobSlots = aaf_get_propertyValue (targetMob, PID_Mob_Slots, &AAFTypeID_MobSlotStrongReferenceVector);

		if (!targetMobSlots) {
			TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Missing target Mob::Slots");
			return -1;
		}

		targetMobSlot = aaf_get_MobSlotBySlotID (targetMobSlots, *SourceMobSlotID);

		if (!targetMobSlot) {
			/* TODO check if there is a workaround :
			 * valgrind --track-origins=yes --leak-check=full ./bin/AAFInfo --trace --aaf-essences  --aaf-clips /home/agfline/Programming/libaaf/LibAAF/test/private/aaf/ADP/ADP_STTRACK_CLIPGAIN_TRACKGAIN_XFADE_NOOPTONEXPORT.aaf --verb 3
			 */
			TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Could not retrieve target MobSlot ID : %u", *SourceMobSlotID);
			return -1;
		}
	}

	/* *** Clip *** */

	if (aafUIDCmp (ParentMob->Class->ID, &AAFClassID_CompositionMob)) {
		int64_t* length = aaf_get_propertyValue (SourceClip, PID_Component_Length, &AAFTypeID_LengthType);

		if (!length) {
			TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Missing Component::Length");
			return -1;
		}

		int64_t* startTime = aaf_get_propertyValue (SourceClip, PID_SourceClip_StartTime, &AAFTypeID_PositionType);

		if (!startTime) {
			TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Missing SourceClip::StartTime");
			return -1;
		}

		if (aafUIDCmp (DataDefinition, &AAFDataDef_Sound) ||
		    aafUIDCmp (DataDefinition, &AAFDataDef_LegacySound)) {
			if (aafi->ctx.TopLevelCompositionMob == ParentMob /*!parentMobUsageCode || aafUIDCmp( parentMobUsageCode, &AAFUsage_TopLevel )*/) {
				if (aafi->ctx.current_clip_is_combined &&
				    aafi->ctx.current_combined_clip_channel_num > 0) {
					/*
					 * Parsing multichannel audio clip (AAFOperationDef_AudioChannelCombiner)
					 * We already parsed the first SourceClip in AAFOperationDef_AudioChannelCombiner.
					 * We just have to check everything match for all clips left (each clip represents a channel)
					 *
				│ 02277│               ├──◻ AAFClassID_OperationGroup (OpIdent: AAFOperationDef_AudioChannelCombiner; Length: 517207)
				│ 02816│               │    ├──◻ AAFClassID_SourceClip (Length: 516000)
				│ 02821│               │    │    └──◻ AAFClassID_MasterMob (UsageCode: n/a) : 7
				│ 04622│               │    │         └──◻ AAFClassID_TimelineMobSlot [slot:1 track:1] (DataDef : AAFDataDef_Sound)
				│ 03185│               │    │              └──◻ AAFClassID_SourceClip (Length: 523723)
				│ 04346│               │    │                   └──◻ AAFClassID_SourceMob (UsageCode: n/a) : 7
				│ 01393│               │    │                        └──◻ AAFClassID_WAVEDescriptor
				│ 01532│               │    │                             └──◻ AAFClassID_NetworkLocator : file://localhost/C:/Users/user/Desktop/res/7.1-SMPTE-channel-identif.wav
				│      │               │    │
				│ 02666│               │    ├──◻ AAFClassID_SourceClip (Length: 516000)
				│ 02666│               │    ├──◻ AAFClassID_SourceClip (Length: 516000)
				│ 02666│               │    ├──◻ AAFClassID_SourceClip (Length: 516000)
				│ 02666│               │    ├──◻ AAFClassID_SourceClip (Length: 516000)
				│ 02666│               │    ├──◻ AAFClassID_SourceClip (Length: 516000)
				│ 02666│               │    ├──◻ AAFClassID_SourceClip (Length: 516000)
				│ 02666│               │    └──◻ AAFClassID_SourceClip (Length: 516000)
					 */

					if (aafi->ctx.current_combined_clip_forced_length == 0 &&
					    aafi->ctx.current_clip->len != *length) {
						TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "SourceClip length does not match first one in AAFOperationDef_AudioChannelCombiner");
						return -1;
					}

					if (targetMob && !aafUIDCmp (targetMob->Class->ID, &AAFClassID_MasterMob)) {
						TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Multichannel-combined SourceClip does not target a MasterMob: %s", aaft_ClassIDToText (aafi->aafd, targetMob->Class->ID));
						return -1;
					}

					if (aafMobIDCmp (aafi->ctx.current_clip->essencePointerList->essenceFile->masterMobID, sourceID) &&
					    aafi->ctx.current_clip->essencePointerList->essenceFile->masterMobSlotID == *SourceMobSlotID) {
						/*
						 * Clip channel rely on the same audio file source (single multichannel file)
						 *
						 * Assume that all clip channels will point to the same multichannel essence file, in the right order.
						 * (Davinci Resolve multichannel clips)
						 */

						aafi->ctx.current_clip->essencePointerList->essenceChannel = 0;

						TRACE_OBJ_INFO (aafi, SourceClip, &__td, "Ignore parsing of clip channel %i pointing to the same audio source file", aafi->ctx.current_combined_clip_channel_num + 1);
						return 0;
					}
				}

				if (!aafi->ctx.current_clip_is_combined ||
				    (aafi->ctx.current_clip_is_combined && aafi->ctx.current_combined_clip_channel_num == 0)) {
					/*
					 * Create new clip, only if we are parsing a single mono clip, or if
					 * we are parsing the first SourceClip describing the first channel of
					 * a multichannel clip inside an AAFOperationDef_AudioChannelCombiner
					 */

					aafiAudioClip* audioClip = aafi_newAudioClip (aafi, aafi->ctx.current_track);

					aafiTimelineItem* timelineItem = audioClip->timelineItem;

					timelineItem->pos = aafi->ctx.current_track->current_pos;
					timelineItem->len = (aafi->ctx.current_combined_clip_forced_length) ? aafi->ctx.current_combined_clip_forced_length : *length;

					audioClip->gain       = aafi->ctx.current_clip_gain;
					audioClip->automation = aafi->ctx.current_clip_variable_gain;
					audioClip->mute       = aafi->ctx.current_clip_is_muted;
					audioClip->pos        = aafi->ctx.current_track->current_pos;

					if (aafi->ctx.avid_warp_clip_edit_rate) {
						audioClip->essence_offset = aafi_convertUnit (*startTime, aafi->ctx.avid_warp_clip_edit_rate, aafi->ctx.current_track->edit_rate);
						audioClip->len            = aafi_convertUnit ((aafi->ctx.current_combined_clip_forced_length) ? aafi->ctx.current_combined_clip_forced_length : *length, aafi->ctx.avid_warp_clip_edit_rate, aafi->ctx.current_track->edit_rate);
					} else {
						audioClip->essence_offset = *startTime;
						audioClip->len            = (aafi->ctx.current_combined_clip_forced_length) ? aafi->ctx.current_combined_clip_forced_length : *length;
					}

					aafi->ctx.current_track->current_pos += audioClip->len;
					aafi->ctx.current_track->clipCount++;

					aafi->ctx.current_clip_gain_is_used++;
					aafi->ctx.current_clip_variable_gain_is_used++;

					/*
					 * ComponentAttributeList is non-standard, but used by Avid Media Composer
					 * and Davinci Resolve to attach Clip Notes.
					 */

					void* ComponentAttributeList = aaf_get_propertyValue (SourceClip, aaf_get_PropertyIDByName (aafi->aafd, "ComponentAttributeList"), &AAFTypeID_TaggedValueStrongReferenceVector);

					if (ComponentAttributeList) {
						char* comment = aaf_get_TaggedValueByName (aafi->aafd, ComponentAttributeList, "_COMMENT", &AAFTypeID_String);

						if (comment) {
							aafiMetaData* meta = aafi_newMetadata (aafi, &audioClip->metadata);

							if (!meta) {
								warning ("Could not create new Metadata.");
							} else {
								meta->text = comment;
								meta->name = laaf_util_c99strdup ("_COMMENT");

								if (!meta->name) {
									error ("Could not duplicate meta name : %s", "_COMMENT");
									aafiMetaData* tmp = meta->next;
									aafi_freeMetadata (&meta);
									audioClip->metadata = tmp;
								}
							}
						}
					}

					aafi->ctx.current_clip = audioClip;
				}

				if (aafi->ctx.current_clip_is_combined == 0) {
					if (aafi->ctx.current_track->format != AAFI_TRACK_FORMAT_NOT_SET &&
					    aafi->ctx.current_track->format != AAFI_TRACK_FORMAT_MONO) {
						TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Track format (%u) does not match current clip (%u)", aafi->ctx.current_track->format, AAFI_TRACK_FORMAT_MONO);
					} else {
						aafi->ctx.current_track->format = AAFI_TRACK_FORMAT_MONO;
					}
				}
			} else if (aafUIDCmp (parentMobUsageCode, &AAFUsage_SubClip)) {
				/* Exemple of AAFUsage_SubClip in ../test/private/aaf/AAF/E2R_52min/224E2R190008_ENQUETES_DE_REGION_N32_LA_SECHERESSE_UNE_FATALITE.aaf
				 * TODO: Looks like a sub-clip is just used to reference an existing clip, but to show it in UI with a different name.
				 * It seems that sub-clip length always matches MasterMob > SourceLip length
				 * Therefore, we should only parse its name.
				 *
			│ 02709││         ├──◻ AAFClassID_SourceClip (Length: 183)
			│ 02723││         │    └──◻ AAFClassID_CompositionMob (UsageCode: AAFUsage_SubClip) : plato 8 1ere.Copy.01.new.06  (MetaProps: SubclipBegin[0xfff6] SubclipFullLength[0xfff7] MobAttributeList[0xfff9])
			│ 04478││         │         └──◻ AAFClassID_TimelineMobSlot [slot:2 track:2] (DataDef : AAFDataDef_Sound)
			│ 02978││         │              └──◻ AAFClassID_SourceClip (Length: 681)
			│ 02983││         │                   └──◻ AAFClassID_MasterMob (UsageCode: n/a) : FTVHD-X400-0946.new.06  (MetaProps: MobAttributeList[0xfff9] ConvertFrameRate[0xfff8])
			│ 04561││         │                        └──◻ AAFClassID_TimelineMobSlot [slot:2 track:2] (DataDef : AAFDataDef_Sound) : FTVHD-X400-0946
			│ 03188││         │                             └──◻ AAFClassID_SourceClip (Length: 681)
			│ 04305││         │                                  └──◻ AAFClassID_SourceMob (UsageCode: n/a) : FTVHD-X400-0946.PHYS  (MetaProps: MobAttributeList[0xfff9])
			│ 01342││         │                                       └──◻ AAFClassID_PCMDescriptor   (MetaProps: DataOffset[0xffde])
				 */

				aafi->ctx.current_clip->subClipName = aaf_get_propertyValue (ParentMob, PID_Mob_Name, &AAFTypeID_String);

				if (!aafi->ctx.current_clip->subClipName) {
					debug ("Missing parent Mob::Name (sub-clip name)");
				}

				aafObject* UserComments = aaf_get_propertyValue (ParentMob, PID_Mob_UserComments, &AAFTypeID_TaggedValueStrongReferenceVector);

				if (retrieve_UserComments (aafi, UserComments, &aafi->ctx.current_clip->metadata) < 0) {
					warning ("Error parsing parent Mob::UserComments");
				}
			} else if (aafUIDCmp (parentMobUsageCode, &AAFUsage_AdjustedClip)) {
				// if ( aafi->ctx.current_adjusted_clip_gain ) {
				// 	aafi_applyGainOffset( aafi, &aafi->ctx.current_clip->gain, aafi->ctx.current_adjusted_clip_gain );
				// 	aafi_freeAudioGain( aafi->ctx.current_adjusted_clip_gain );
				// 	aafi->ctx.current_adjusted_clip_gain = NULL;
				// }
			} else if (!parentMobUsageCode) {
				debug ("CompositionMob UsageCode is NULL. Keep on parsing...");
			} else {
				debug ("Unsupported CompositionMob UsageCode: %s", aaft_UsageCodeToText (parentMobUsageCode));
				TRACE_OBJ_NO_SUPPORT (aafi, SourceClip, &__td);
				return -1;
			}

			if (targetMob && aafUIDCmp (targetMob->Class->ID, &AAFClassID_MasterMob)) {
				if (!targetMobSlot) {
					TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Missing target MobSlot");
					return -1;
				}

				TRACE_OBJ (aafi, SourceClip, &__td);

				__td.lv++;
				TRACE_OBJ (aafi, targetMob, &__td);

				parse_MobSlot (aafi, targetMobSlot, &__td);
			} else if (targetMob && aafUIDCmp (targetMob->Class->ID, &AAFClassID_CompositionMob)) {
				/*
				 * If SourceClip points to a CompositionMob instead of a MasterMob, we
				 * are at the begining (or inside) a derivation chain.
				 */

				TRACE_OBJ (aafi, SourceClip, &__td);

				__td.lv++;
				TRACE_OBJ (aafi, targetMob, &__td);

				parse_MobSlot (aafi, targetMobSlot, &__td);
			} else {
				TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Targeted Mob no supported: %s", aaft_ClassIDToText (aafi->aafd, targetMob->Class->ID));
				return -1;
			}

		} else if (aafUIDCmp (DataDefinition, &AAFDataDef_Picture) ||
		           aafUIDCmp (DataDefinition, &AAFDataDef_LegacyPicture)) {
			if (aafi->ctx.TopLevelCompositionMob != ParentMob) {
				TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Current implementation does not support parsing video SourceClip out of TopLevel CompositionMob");
				return -1;
			}

			if (aafi->Video->Tracks->timelineItems) {
				TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Current implementation supports only one video clip");
				return -1;
			}

			/* Add the new clip */

			aafiVideoClip* videoClip = aafi_newVideoClip (aafi, aafi->Video->Tracks);

			aafiTimelineItem* timelineItem = videoClip->timelineItem;

			timelineItem->pos = aafi->Video->Tracks->current_pos;
			timelineItem->len = *length;

			videoClip->pos            = aafi->Video->Tracks->current_pos;
			videoClip->len            = *length;
			videoClip->essence_offset = *startTime;

			aafi->Video->Tracks->current_pos += videoClip->len;

			aafi->ctx.current_video_clip = videoClip;

			if (targetMob && aafUIDCmp (targetMob->Class->ID, &AAFClassID_MasterMob)) {
				if (!targetMobSlot) {
					TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Missing target MobSlot");
					return -1;
				}

				TRACE_OBJ (aafi, SourceClip, &__td);

				__td.lv++;
				TRACE_OBJ (aafi, targetMob, &__td);

				parse_MobSlot (aafi, targetMobSlot, &__td);
			} else {
				TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Current implementation does not support video SourceClip not targetting a MasterMob: %s", (targetMob) ? aaft_ClassIDToText (aafi->aafd, targetMob->Class->ID) : "[MISSING TARGET MOB]");
				return -1;
			}
		}
	}

	/* *** Essence *** */

	else if (aafUIDCmp (ParentMob->Class->ID, &AAFClassID_MasterMob)) {
		aafMobID_t* masterMobID = aaf_get_propertyValue (ParentMob, PID_Mob_MobID, &AAFTypeID_MobIDType);

		if (!masterMobID) {
			TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Missing parent Mob::MobID");
			return -1;
		}

		aafObject* ParentMobSlot = aaf_get_ObjectAncestor (SourceClip, &AAFClassID_MobSlot);

		if (!ParentMobSlot) {
			TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Could not retrieve parent MobSlot");
			return -1;
		}

		uint32_t* masterMobSlotID = aaf_get_propertyValue (ParentMobSlot, PID_MobSlot_SlotID, &AAFTypeID_UInt32);

		uint32_t* essenceChannelNum = aaf_get_propertyValue (ParentMobSlot, PID_MobSlot_PhysicalTrackNumber, &AAFTypeID_UInt32);

		if (aafUIDCmp (DataDefinition, &AAFDataDef_Sound) ||
		    aafUIDCmp (DataDefinition, &AAFDataDef_LegacySound)) {
			if (!aafi->ctx.current_clip) {
				TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "aafi->ctx.current_clip not set");
				return -1;
			}

			/*
			 * Check if this Essence has already been retrieved
			 */

			aafiAudioEssenceFile* audioEssenceFile = NULL;

			AAFI_foreachAudioEssenceFile (aafi, audioEssenceFile)
			{
				if (aafMobIDCmp (audioEssenceFile->sourceMobID, sourceID) && audioEssenceFile->sourceMobSlotID == (unsigned)*SourceMobSlotID) {
					__td.eob = 1;
					TRACE_OBJ_INFO (aafi, SourceClip, &__td, "Essence already parsed: Linking with %s", audioEssenceFile->name);
					aafi->ctx.current_clip->essencePointerList = aafi_newAudioEssencePointer (aafi, &aafi->ctx.current_clip->essencePointerList, audioEssenceFile, essenceChannelNum);
					return 0;
				}
			}

			/* new Essence, carry on. */

			audioEssenceFile = aafi_newAudioEssence (aafi);

			audioEssenceFile->masterMobSlotID = *masterMobSlotID;
			audioEssenceFile->masterMobID     = masterMobID;
			audioEssenceFile->name            = aaf_get_propertyValue (ParentMob, PID_Mob_Name, &AAFTypeID_String);

			if (audioEssenceFile->name == NULL) {
				debug ("Missing parent Mob::Name (essence file name)");
			}

			audioEssenceFile->sourceMobSlotID = *SourceMobSlotID;
			audioEssenceFile->sourceMobID     = sourceID;

			aafObject* SourceMob = aaf_get_MobByID (aafi->aafd->Mobs, audioEssenceFile->sourceMobID);

			if (!SourceMob) {
				TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Could not retrieve SourceMob by ID : %s", aaft_MobIDToText (audioEssenceFile->sourceMobID));
				return -1;
			}

			aafi->ctx.current_audio_essence = audioEssenceFile;

			void* MobUserComments = aaf_get_propertyValue (ParentMob, PID_Mob_UserComments, &AAFTypeID_TaggedValueStrongReferenceVector);

			if (retrieve_UserComments (aafi, MobUserComments, &audioEssenceFile->metadata) < 0) {
				TRACE_OBJ_WARNING (aafi, SourceClip, &__td, "Error parsing parent Mob::UserComments");
			} else {
				TRACE_OBJ (aafi, SourceClip, &__td);
			}

			audioEssenceFile->SourceMob = SourceMob;

			aafObject* EssenceData = aaf_get_EssenceDataByMobID (aafi->aafd, audioEssenceFile->sourceMobID);

			if (EssenceData)
				__td.ll[__td.lv] = 2;

			parse_Mob (aafi, SourceMob, &__td);

			if (EssenceData)
				__td.ll[__td.lv] = 1;

			if (EssenceData) {
				/* If EssenceData was found, it means essence is embedded */
				parse_EssenceData (aafi, EssenceData, &__td);
				__td.ll[__td.lv] = 0;
			}

			aafi_build_unique_audio_essence_name (aafi, audioEssenceFile);

			aafi->ctx.current_clip->essencePointerList = aafi_newAudioEssencePointer (aafi, &aafi->ctx.current_clip->essencePointerList, audioEssenceFile, essenceChannelNum);
			aafi->ctx.current_audio_essence            = NULL;
		} else if (aafUIDCmp (DataDefinition, &AAFDataDef_Picture) ||
		           aafUIDCmp (DataDefinition, &AAFDataDef_LegacyPicture)) {
			if (!aafi->ctx.current_video_clip) {
				TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "aafi->ctx.current_video_clip not set");
				return -1;
			}

			/*
			 * Check if this Essence has already been retrieved
			 */

			aafiVideoEssence* videoEssenceFile = NULL;

			AAFI_foreachVideoEssence (aafi, videoEssenceFile)
			{
				if (aafMobIDCmp (videoEssenceFile->sourceMobID, sourceID) && videoEssenceFile->sourceMobSlotID == (unsigned)*SourceMobSlotID) {
					__td.eob = 1;
					TRACE_OBJ_INFO (aafi, SourceClip, &__td, "Essence already parsed: Linking with %s", videoEssenceFile->name);
					aafi->ctx.current_video_clip->Essence = videoEssenceFile;
					return 0;
				}
			}

			/* new Essence, carry on. */

			videoEssenceFile = aafi_newVideoEssence (aafi);

			aafi->ctx.current_video_clip->Essence = videoEssenceFile;

			videoEssenceFile->masterMobSlotID = *masterMobSlotID;
			videoEssenceFile->masterMobID     = masterMobID;
			videoEssenceFile->name            = aaf_get_propertyValue (ParentMob, PID_Mob_Name, &AAFTypeID_String);

			if (videoEssenceFile->name == NULL) {
				debug ("Missing parent Mob::Name (essence file name)");
			}

			videoEssenceFile->sourceMobSlotID = *SourceMobSlotID;
			videoEssenceFile->sourceMobID     = sourceID;

			TRACE_OBJ (aafi, SourceClip, &__td);

			aafObject* SourceMob = aaf_get_MobByID (aafi->aafd->Mobs, videoEssenceFile->sourceMobID);

			if (!SourceMob) {
				TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Could not retrieve SourceMob by ID : %s", aaft_MobIDToText (videoEssenceFile->sourceMobID));
				return -1;
			}

			videoEssenceFile->SourceMob = SourceMob;

			aafi->ctx.current_video_essence = videoEssenceFile;

			aafObject* EssenceData = aaf_get_EssenceDataByMobID (aafi->aafd, videoEssenceFile->sourceMobID);

			parse_Mob (aafi, SourceMob, &__td);

			if (EssenceData) {
				/*
				 * It means essence is embedded, otherwise it's not.
				 */
				parse_EssenceData (aafi, EssenceData, &__td);
			}

			/*
			 * No need to check for uniqueness, current version supports only one video clip.
			 */
			videoEssenceFile->unique_name = laaf_util_c99strdup (videoEssenceFile->name);

			if (!videoEssenceFile->unique_name) {
				TRACE_OBJ_ERROR (aafi, SourceClip, &__td, "Could not duplicate video essence unique name : %s", videoEssenceFile->name);
				return -1;
			}

			aafi->ctx.current_video_essence = NULL;
		}
	} else if (aafUIDCmp (ParentMob->Class->ID, &AAFClassID_SourceMob)) {
		/* Nothing to parse here at first glance : SourceMob > TimelineMobSlot > SourceClip */
		TRACE_OBJ (aafi, SourceClip, &__td);
	} else {
		TRACE_OBJ_NO_SUPPORT (aafi, SourceClip, &__td);
		return -1;
	}

	return 0;
}

static int
parse_Timecode (AAF_Iface* aafi, aafObject* Timecode, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 1);
	__td.eob = 1;

	aafPosition_t* tc_start = aaf_get_propertyValue (Timecode, PID_Timecode_Start, &AAFTypeID_PositionType);

	if (tc_start == NULL) {
		TRACE_OBJ_ERROR (aafi, Timecode, &__td, "Missing Timecode::Start");
		return -1;
	}

	uint16_t* tc_fps = aaf_get_propertyValue (Timecode, PID_Timecode_FPS, &AAFTypeID_UInt16);

	if (tc_fps == NULL) {
		TRACE_OBJ_ERROR (aafi, Timecode, &__td, "Missing Timecode::FPS");
		return -1;
	}

	uint8_t* tc_drop = aaf_get_propertyValue (Timecode, PID_Timecode_Drop, &AAFTypeID_UInt8);

	if (tc_drop == NULL) {
		TRACE_OBJ_ERROR (aafi, Timecode, &__td, "Missing Timecode::Drop");
		return -1;
	}

	/* TODO this should be retrieved directly from TimelineMobSlot */

	aafObject* ParentMobSlot = aaf_get_ObjectAncestor (Timecode, &AAFClassID_MobSlot);

	if (!ParentMobSlot) {
		TRACE_OBJ_ERROR (aafi, Timecode, &__td, "Could not retrieve parent MobSlot");
		return -1;
	}

	aafRational_t* tc_edit_rate = aaf_get_propertyValue (ParentMobSlot, PID_TimelineMobSlot_EditRate, &AAFTypeID_Rational);

	if (tc_edit_rate == NULL) {
		TRACE_OBJ_ERROR (aafi, Timecode, &__td, "Missing parent TimelineMobSlot::EditRate");
		return -1;
	}

	if (aafi->Timecode) {
		TRACE_OBJ_WARNING (aafi, Timecode, &__td, "Timecode was already set, ignoring (%lu, %u fps)", *tc_start, *tc_fps);
		return -1;
	}

	aafiTimecode* tc = calloc (1, sizeof (aafiTimecode));

	if (!tc) {
		TRACE_OBJ_ERROR (aafi, Timecode, &__td, "Out of memory");
		return -1;
	}

	tc->start     = *tc_start;
	tc->fps       = *tc_fps;
	tc->drop      = *tc_drop;
	tc->edit_rate = tc_edit_rate;

	aafi->Timecode = tc;

	TRACE_OBJ (aafi, Timecode, &__td);

	return 0;
}

static int
parse_DescriptiveMarker (AAF_Iface* aafi, aafObject* DescriptiveMarker, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 1);

	aafPosition_t* start = aaf_get_propertyValue (DescriptiveMarker, PID_Event_Position, &AAFTypeID_PositionType);

	if (!start) {
		/*
		 * « If an Event is in a TimelineMobSlot or a StaticMobSlot, it shall not have a Position
		 * property. If an Event is in a EventMobSlot, it shall have a Position property. »
		 */
		TRACE_OBJ_ERROR (aafi, DescriptiveMarker, &__td, "Missing Event::Position");
		return -1;
	}

	TRACE_OBJ (aafi, DescriptiveMarker, &__td);

	aafPosition_t* length  = aaf_get_propertyValue (DescriptiveMarker, PID_Component_Length, &AAFTypeID_PositionType);
	char*          comment = aaf_get_propertyValue (DescriptiveMarker, PID_Event_Comment, &AAFTypeID_String);
	char*          name    = aaf_get_propertyValue (DescriptiveMarker, aaf_get_PropertyIDByName (aafi->aafd, "CommentMarkerUser"), &AAFTypeID_String);

	if (!name) {
		/* Avid Media Composer 23.12 */
		name = aaf_get_propertyValue (DescriptiveMarker, aaf_get_PropertyIDByName (aafi->aafd, "CommentMarkerUSer"), &AAFTypeID_String);
	}

	uint16_t*    RGBColor     = NULL;
	aafProperty* RGBColorProp = aaf_get_property (DescriptiveMarker, aaf_get_PropertyIDByName (aafi->aafd, "CommentMarkerColor"));

	if (RGBColorProp) {
		if (RGBColorProp->len != sizeof (uint16_t) * 3) {
			error ("CommentMarkerColor has wrong size: %u", RGBColorProp->len);
		} else {
			RGBColor = RGBColorProp->val;

			/* big endian to little endian */
			RGBColor[0] = (RGBColor[0] >> 8) | (RGBColor[0] << 8);
			RGBColor[1] = (RGBColor[1] >> 8) | (RGBColor[1] << 8);
			RGBColor[2] = (RGBColor[2] >> 8) | (RGBColor[2] << 8);
		}
	}

	aafi_newMarker (aafi, aafi->ctx.current_markers_edit_rate, *start, ((length) ? *length : 0), name, comment, &RGBColor);

	return 0;
}

static int
parse_Sequence (AAF_Iface* aafi, aafObject* Sequence, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 1);

	aafObject* Component  = NULL;
	aafObject* Components = aaf_get_propertyValue (Sequence, PID_Sequence_Components, &AAFTypeID_ComponentStrongReferenceVector);

	if (!Components) {
		TRACE_OBJ_ERROR (aafi, Sequence, &__td, "Missing Sequence::Components");
		return -1;
	}

	TRACE_OBJ (aafi, Sequence, &__td);

	/*
	 * "Audio Warp" OperationGroup appears in Avid Media Composer AAF files, when
	 * a clip with a different frame rate from the project was *linked* into Avid
	 * rather than beeing properly imported.
	 *
	 * "Audio Warp" OperationGroup is pointing to a Sequence with two ComponentAttributes:
	 *    _MIXMATCH_RATE_NUM
	 *    _MIXMATCH_RATE_DENOM
	 *
	 * Those parameters set the valid edit rate for SourceClip::StartTime (essence
	 * offset) and Component::Length, in total violation of the standard saying that
	 * TimelineMobSlot::EditRate shall always be used.
	 *
│ 02009││    │         ├──◻ AAFClassID_OperationGroup (OpIdent: Audio Warp; Length: 168)   (MetaProps: ComponentAttributeList[0xffc9])
│ 02283││    │         │    ├──◻ AAFClassID_ConstantValue (ParamDef: AvidMotionInputFormat; Type: AAFTypeID_Int32) : 2
│ 02283││    │         │    ├──◻ AAFClassID_ConstantValue (ParamDef: AAFParameterDef_SpeedRatio; Type: AAFTypeID_Rational) : 1/2
│ 02283││    │         │    ├──◻ AAFClassID_ConstantValue (ParamDef: AvidPhase; Type: AAFTypeID_Int32) : 0
│ 02283││    │         │    ├──◻ AAFClassID_ConstantValue (ParamDef: AvidMotionOutputFormat; Type: AAFTypeID_Int32) : 0
│ 02283││    │         │    └──◻ AAFClassID_ConstantValue (ParamDef: AvidMotionPulldown; Type: AAFTypeID_Int32) : 0
│ 01628││    │         │    └──◻ AAFClassID_Sequence (Length: 336)  (MetaProps: ComponentAttributeList[0xffc9])
│ 01195││    │         │         └──◻ AAFClassID_SourceClip (Length: 336)
│ 01198││    │         │              └──◻ AAFClassID_MasterMob (UsageCode: n/a) : UTS_LIMO_LIMO_3_2024_01_30_11_15_45.new.04  (MetaProps: MobAttributeList[0xfff9] ConvertFrameRate[0xfff8])
│ 00513││    │         │                   └──◻ AAFClassID_TimelineMobSlot [slot:1 track:1] (DataDef: AAFDataDef_Sound)
│ 01628││    │         │                        └──◻ AAFClassID_Sequence (Length: 576)
│ 01359││    │         │                             └──◻ AAFClassID_SourceClip (Length: 576)
│ 00272││    │         │                                  ├──◻ AAFClassID_SourceMob (UsageCode: n/a)   (MetaProps: MobAttributeList[0xfff9])
│ 02806││    │         │                                  │    ├──◻ AAFClassID_WAVEDescriptor (ContainerIdent : AAFContainerDef_AAF)
│ 03010││    │         │                                  │    │    └──◻ AAFClassID_NetworkLocator (URLString: file://10.87.230.71/mixage/AAF_Vers_Mixage/MONTAGE_06/4550985%20SUIVI%20AGRICULTEURS%20VERS%20PARIS.aaf)
│      ││    │         │                                  │    │
│ 00532││    │         │                                  │    └──◻ AAFClassID_TimelineMobSlot [slot:1 track:1] (DataDef: AAFDataDef_Sound)
│ 01470││    │         │                                  │         └──◻ AAFClassID_SourceClip (Length: 576)
│ 03071││    │         │                                  └──◻ AAFClassID_EssenceData (Data: Data-2702)

	 */

	void* ComponentAttributeList = aaf_get_propertyValue (Sequence, aaf_get_PropertyIDByName (aafi->aafd, "ComponentAttributeList"), &AAFTypeID_TaggedValueStrongReferenceVector);

	if (ComponentAttributeList) {
		int32_t* rateNum   = aaf_get_TaggedValueByName (aafi->aafd, ComponentAttributeList, "_MIXMATCH_RATE_NUM", &AAFTypeID_Int32);
		int32_t* rateDenom = aaf_get_TaggedValueByName (aafi->aafd, ComponentAttributeList, "_MIXMATCH_RATE_DENOM", &AAFTypeID_Int32);

		if (rateNum && rateDenom) {
			aafi->ctx.avid_warp_clip_edit_rate = malloc (sizeof (aafRational_t));

			if (!aafi->ctx.avid_warp_clip_edit_rate) {
				error ("Out of memory");
				return -1;
			}

			aafi->ctx.avid_warp_clip_edit_rate->numerator   = *rateNum;
			aafi->ctx.avid_warp_clip_edit_rate->denominator = *rateDenom;
			debug ("Got Avid audio warp edit rate : %i/%i",
			       aafi->ctx.avid_warp_clip_edit_rate->numerator,
			       aafi->ctx.avid_warp_clip_edit_rate->denominator);
		}
	}

	uint32_t i = 0;
	AAFI_foreach_ObjectInSet (&Component, Components, i, __td)
	{
		parse_Component (aafi, Component, &__td);
	}

	free (aafi->ctx.avid_warp_clip_edit_rate);
	aafi->ctx.avid_warp_clip_edit_rate = NULL;

	return 0;
}

static int
parse_Selector (AAF_Iface* aafi, aafObject* Selector, td* __ptd)
{
	/*
	 * The Selector class is a sub-class of the Segment class.
	 *
	 * The Selector class provides the value of a single Segment (PID_Selector_Selected)
	 * while preserving references to unused alternatives (PID_Selector_Alternates)
	 */

	struct trace_dump __td;
	__td_set (__td, __ptd, 1);

	aafObject* Selected = aaf_get_propertyValue (Selector, PID_Selector_Selected, &AAFTypeID_SegmentStrongReference);

	if (!Selected) {
		TRACE_OBJ_ERROR (aafi, Selector, &__td, "Missing Selector::Selected");
		return -1;
	}

	TRACE_OBJ (aafi, Selector, &__td);

	aafObject* Alternates = aaf_get_propertyValue (Selector, PID_Selector_Alternates, &AAFTypeID_SegmentStrongReferenceVector);

	if (Alternates) {
		__td.lv++;
		TRACE_OBJ_INFO (aafi, Alternates, &__td, "Selector Alternates (dropping)");
		__td.lv--;
	}

	/*
	 * ComponentAttributeList is non-standard, used by Avid Media Composer and
	 * Davinci Resolve to describe a disabled (muted) clip. This way, any unaware
	 * implementation will parse Selected Object containing a Filler and ignore
	 * the disabled clip inside Alternates.
	 */

	void* ComponentAttributeList = aaf_get_propertyValue (Selector, aaf_get_PropertyIDByName (aafi->aafd, "ComponentAttributeList"), &AAFTypeID_TaggedValueStrongReferenceVector);

	if (ComponentAttributeList) {
		int32_t* disabledClip = aaf_get_TaggedValueByName (aafi->aafd, ComponentAttributeList, "_DISABLE_CLIP_FLAG", &AAFTypeID_Int32);

		if (disabledClip) {
			if (*disabledClip) {
				aafi->ctx.current_clip_is_muted = 1;
			}

			/*
			 * When Selector has _DISABLE_CLIP_FLAG, Alternates should point
			 * to a single Alternates Object containing the disabled clip.
			 */
			if (Alternates) {
				return parse_Segment (aafi, Alternates, &__td);
			} else {
				return parse_Segment (aafi, Selected, &__td);
			}
		}
	} else {
		/*
		 * without specific software implementation, we stick to Selected Object
		 * and forget about any Alternates Objects.
		 */
		return parse_Segment (aafi, Selected, &__td);
	}

	return -1;
}

static int
parse_NestedScope (AAF_Iface* aafi, aafObject* NestedScope, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 1);

	/*
	 * NestedScope seems to be only used for video clips in Avid Media Composer.
	 * Not sure how to handle it...
	 */

	aafObject* Slot  = NULL;
	aafObject* Slots = aaf_get_propertyValue (NestedScope, PID_NestedScope_Slots, &AAFTypeID_SegmentStrongReferenceVector);

	if (!Slots) {
		TRACE_OBJ_ERROR (aafi, NestedScope, &__td, "Missing NestedScope::Slots");
		return -1;
	}

	TRACE_OBJ (aafi, NestedScope, &__td);

	uint32_t i = 0;
	AAFI_foreach_ObjectInSet (&Slot, Slots, i, __td)
	{
		parse_Segment (aafi, Slot, &__td);
	}

	return 0;
}

static int
parse_OperationGroup (AAF_Iface* aafi, aafObject* OpGroup, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 1);

	if (!aaf_get_property (OpGroup, PID_OperationGroup_InputSegments) &&
	    !aaf_get_property (OpGroup, PID_OperationGroup_Parameters)) {
		__td.eob = 1;
	}

	aafObject* ParentMob = aaf_get_ObjectAncestor (OpGroup, &AAFClassID_Mob);

	if (!ParentMob) {
		TRACE_OBJ_ERROR (aafi, OpGroup, &__td, "Could not retrieve parent Mob");
		return -1;
	}

	if (!aafUIDCmp (ParentMob->Class->ID, &AAFClassID_CompositionMob)) {
		TRACE_OBJ_ERROR (aafi, OpGroup, &__td, "OperationGroup is currently supported only in CompositionMob, not in %s", aaft_ClassIDToText (aafi->aafd, ParentMob->Class->ID));
		return -1;
	}

	int rc = 0;

	aafWeakRef_t* OperationDefWeakRef = aaf_get_propertyValue (OpGroup, PID_OperationGroup_Operation, &AAFTypeID_OperationDefinitionWeakReference);

	if (!OperationDefWeakRef) {
		TRACE_OBJ_ERROR (aafi, OpGroup, &__td, "Missing OperationGroup::Operation");
		return -1;
	}

	aafUID_t* OperationIdentification = aaf_get_OperationIdentificationByWeakRef (aafi->aafd, OperationDefWeakRef);

	if (!OperationIdentification) {
		TRACE_OBJ_ERROR (aafi, OpGroup, &__td, "Could not retrieve OperationIdentification");
		return -1;
	}

	/*
	 * We need to check if OperationGroup is a direct child of TopLevelCompositionMob > TimelineMobSlot.
	 *  - If it is, it means that the OperationGroup affect the current Track.
	 *  - If it's not (eg. it's a child of a Sequence), then OperationGroup applies
	 *    to all descendent clips.
	 *
	 * NOTE: OperationGroup can be a child of another OperationGroup. So we can't
	 * just check direct Parent, we need to loop.
	 */

	aafObject* OperationGroupParent = OpGroup->Parent;
	while (OperationGroupParent && aafUIDCmp (OperationGroupParent->Class->ID, &AAFClassID_OperationGroup)) {
		OperationGroupParent = OperationGroupParent->Parent;
	}

	if (!OperationGroupParent) {
		error ("OperationGroup has no parent !");
		return -1;
	}

	if (aafUIDCmp (OperationGroupParent->Class->ID, &AAFClassID_TimelineMobSlot) &&
	    ParentMob == aafi->ctx.TopLevelCompositionMob) {
		aafi->ctx.current_opgroup_affect_track = 1;
	} else {
		aafi->ctx.current_opgroup_affect_track = 0;
	}

	if (aafUIDCmp (OperationIdentification, &AAFOperationDef_MonoAudioDissolve)) {
		if (!aafUIDCmp (OpGroup->Parent->Class->ID, &AAFClassID_Transition)) {
			TRACE_OBJ_ERROR (aafi, OpGroup, &__td, "Parent should be AAFClassID_Transition");
			return -1;
		}

		TRACE_OBJ (aafi, OpGroup, &__td);

		aafiTransition* Trans = aafi->ctx.current_transition;

		/*
		 * Mono Audio Dissolve (Fade, Cross Fade)
		 *
		 * The same parameter (curve/level) is applied to the outgoing fade on first
		 * clip (if any) and to the incoming fade on second clip (if any).
		 */

		Trans->flags |= AAFI_TRANS_SINGLE_CURVE;

		aafObject* Param      = NULL;
		aafObject* Parameters = aaf_get_propertyValue (OpGroup, PID_OperationGroup_Parameters, &AAFTypeID_ParameterStrongReferenceVector);

		uint32_t i = 0;
		AAFI_foreach_ObjectInSet (&Param, Parameters, i, __td)
		{
			parse_Parameter (aafi, Param, &__td);
		}

		/*
		 * Avid Media Composer doesn't use the standard method to set interpolation. Instead,
		 * it always sets InterpolationIdentification to Linear, and it sets the actual
		 * interpolation in both :
		 * 	- OperationGroup > ComponentAttributeList > _ATN_AUDIO_DISSOLVE_CURVETYPE
		 * 	- OperationGroup > Parameters > AAFClassID_ConstantValue (ParamDef: Curve Type; Type: AAFTypeID_Int32) : 1
		 *
		 * Note: _ATN_AUDIO_DISSOLVE_CURVETYPE was observed since v8.4 (2015), however "Curve Type" was observed since v18.12.7.
		 * Using _ATN_AUDIO_DISSOLVE_CURVETYPE provides a better support for older Avid MC versions.
		 */

		void* ComponentAttributeList = aaf_get_propertyValue (OpGroup, aaf_get_PropertyIDByName (aafi->aafd, "ComponentAttributeList"), &AAFTypeID_TaggedValueStrongReferenceVector);

		if (ComponentAttributeList) {
			int32_t* curveType = aaf_get_TaggedValueByName (aafi->aafd, ComponentAttributeList, "_ATN_AUDIO_DISSOLVE_CURVETYPE", &AAFTypeID_Int32);

			if (curveType) {
				switch (*curveType) {
					case AVID_MEDIA_COMPOSER_CURVE_TYPE_LINEAR:
						Trans->flags &= ~(AAFI_INTERPOL_MASK);
						Trans->flags |= AAFI_INTERPOL_LINEAR;
						break;
					case AVID_MEDIA_COMPOSER_CURVE_TYPE_EQUAL_POWER:
						Trans->flags &= ~(AAFI_INTERPOL_MASK);
						Trans->flags |= AAFI_INTERPOL_POWER;
						break;
					default:
						debug ("Unknown Avid Media Composer fade curve: %i", *curveType);
						break;
				}
			}
		}

		if (!(Trans->flags & AAFI_INTERPOL_MASK)) {
			debug ("Setting fade interpolation to default Linear");
			Trans->flags |= AAFI_INTERPOL_LINEAR;
		}

		aafi_dump_obj (aafi, NULL, &__td, 0, NULL, -1, "");
	} else if (aafUIDCmp (OperationIdentification, &AAFOperationDef_AudioChannelCombiner)) {
		TRACE_OBJ (aafi, OpGroup, &__td);

		aafObject* InputSegment  = NULL;
		aafObject* InputSegments = aaf_get_propertyValue (OpGroup, PID_OperationGroup_InputSegments, &AAFTypeID_SegmentStrongReferenceVector);

		aafi->ctx.current_clip_is_combined            = 1;
		aafi->ctx.current_combined_clip_total_channel = InputSegments->Header->_entryCount;
		aafi->ctx.current_combined_clip_channel_num   = 0;
		aafi->ctx.current_combined_clip_forced_length = 0;

		if (resolve_AAF (aafi)) {
			/*
			 * This is clearly a violation of the standard (p 57). When Davinci Resolve
			 * exports multichannel clips, it does not set SourceClip::Length correctly.
			 * Instead, it's more like some sort of frame-rounded value which doesn't match
			 * the timeline. However, the correct value is set to OperationGroup::length...
			 */
			int64_t* length                               = aaf_get_propertyValue (OpGroup, PID_Component_Length, &AAFTypeID_LengthType);
			aafi->ctx.current_combined_clip_forced_length = (length) ? *length : 0;
		}

		uint32_t i = 0;
		AAFI_foreach_ObjectInSet (&InputSegment, InputSegments, i, __td)
		{
			parse_Segment (aafi, InputSegment, &__td);
			aafi->ctx.current_combined_clip_channel_num++;
		}

		aafi_dump_obj (aafi, NULL, &__td, 0, NULL, -1, "");

		aafiAudioTrack* current_track = aafi->ctx.current_track;

		aafiTrackFormat_e track_format = AAFI_TRACK_FORMAT_UNKNOWN;

		if (aafi->ctx.current_combined_clip_total_channel == 2) {
			track_format = AAFI_TRACK_FORMAT_STEREO;
		} else if (aafi->ctx.current_combined_clip_total_channel == 6) {
			track_format = AAFI_TRACK_FORMAT_5_1;
		} else if (aafi->ctx.current_combined_clip_total_channel == 8) {
			track_format = AAFI_TRACK_FORMAT_7_1;
		} else {
			TRACE_OBJ_ERROR (aafi, OpGroup, &__td, "Unknown track format (%u)", aafi->ctx.current_combined_clip_total_channel);
			RESET_CTX__AudioChannelCombiner (aafi->ctx);
			return -1;
		}

		if (current_track->format != AAFI_TRACK_FORMAT_NOT_SET &&
		    current_track->format != track_format) {
			TRACE_OBJ_ERROR (aafi, OpGroup, &__td, "Track format (%u) does not match current clip (%u)", current_track->format, track_format);
			RESET_CTX__AudioChannelCombiner (aafi->ctx);
			return -1;
		}

		current_track->format = track_format;
		RESET_CTX__AudioChannelCombiner (aafi->ctx);
	} else if (aafUIDCmp (OperationIdentification, &AAFOperationDef_MonoAudioGain)) {
		aafObject* Param      = NULL;
		aafObject* Parameters = aaf_get_propertyValue (OpGroup, PID_OperationGroup_Parameters, &AAFTypeID_ParameterStrongReferenceVector);

		if (!Parameters) {
			TRACE_OBJ_ERROR (aafi, OpGroup, &__td, "Missing OperationGroup::Parameters");
			rc = -1;
			goto end; /* we still have to parse InputSegments, so we're not losing any clip */
		}

		TRACE_OBJ (aafi, OpGroup, &__td);

		uint32_t i = 0;
		AAFI_foreach_ObjectInSet (&Param, Parameters, i, __td)
		{
			parse_Parameter (aafi, Param, &__td);
		}
	} else if (aafUIDCmp (OperationIdentification, &AAFOperationDef_MonoAudioPan)) {
		/* TODO Should Only be Track-based (first Segment of TimelineMobSlot.) */

		aafObject* Param      = NULL;
		aafObject* Parameters = aaf_get_propertyValue (OpGroup, PID_OperationGroup_Parameters, &AAFTypeID_ParameterStrongReferenceVector);

		if (!Parameters) {
			TRACE_OBJ_ERROR (aafi, OpGroup, &__td, "Missing OperationGroup::Parameters");
			rc = -1;
			goto end; /* we still have to parse InputSegments, so we're not losing any clip */
		}

		TRACE_OBJ (aafi, OpGroup, &__td);

		uint32_t i = 0;
		AAFI_foreach_ObjectInSet (&Param, Parameters, i, __td)
		{
			parse_Parameter (aafi, Param, &__td);
		}
	} else if (aafUIDCmp (OperationIdentification, aaf_get_OperationDefIDByName (aafi->aafd, "Audio Warp"))) {
		aafObject* Param      = NULL;
		aafObject* Parameters = aaf_get_propertyValue (OpGroup, PID_OperationGroup_Parameters, &AAFTypeID_ParameterStrongReferenceVector);

		if (!Parameters) {
			TRACE_OBJ_ERROR (aafi, OpGroup, &__td, "Missing OperationGroup::Parameters");
			rc = -1;
			goto end; /* we still have to parse InputSegments, so we're not losing any clip */
		}

		TRACE_OBJ (aafi, OpGroup, &__td);

		uint32_t i = 0;
		AAFI_foreach_ObjectInSet (&Param, Parameters, i, __td)
		{
			parse_Parameter (aafi, Param, &__td);
		}
	} else {
		/*
		 * Unknown usage and implementation, not encountered yet:
		 *
		 * 	AAFOperationDef_MonoAudioMixdown
		 * 	AAFOperationDef_StereoAudioGain
		 * 	AAFOperationDef_TwoParameterMonoAudioDissolve
		 * 	AAFOperationDef_StereoAudioDissolve
		 */
		debug ("Unsupported OperationIdentification: %s", aaft_OperationDefToText (aafi->aafd, OperationIdentification));
		TRACE_OBJ_NO_SUPPORT (aafi, OpGroup, &__td);

		aafObject* Param      = NULL;
		aafObject* Parameters = aaf_get_propertyValue (OpGroup, PID_OperationGroup_Parameters, &AAFTypeID_ParameterStrongReferenceVector);

		if (!Parameters) {
			// TRACE_OBJ_ERROR( aafi, OpGroup, &__td, "Missing OperationGroup::Parameters" );
			rc = -1;
			goto end; /* we still have to parse InputSegments, so we're not losing any clip */
		}

		uint32_t i = 0;
		AAFI_foreach_ObjectInSet (&Param, Parameters, i, __td)
		{
			parse_Parameter (aafi, Param, &__td);
		}
	}

end:

	/*
	 * Parses Segments in the OperationGroup::InputSegments, only if
	 * OperationGroup is not a Transition as a Transition has no InputSegments,
	 * and not an AudioChannelCombiner as they were already parsed.
	 */

	if (!aafUIDCmp (OpGroup->Parent->Class->ID, &AAFClassID_Transition) &&
	    !aafUIDCmp (OperationIdentification, &AAFOperationDef_AudioChannelCombiner)) {
		aafObject* InputSegment  = NULL;
		aafObject* InputSegments = aaf_get_propertyValue (OpGroup, PID_OperationGroup_InputSegments, &AAFTypeID_SegmentStrongReferenceVector);

		uint32_t i = 0;
		AAFI_foreach_ObjectInSet (&InputSegment, InputSegments, i, __td)
		{
			parse_Segment (aafi, InputSegment, &__td);
		}
	}

	/* End of current OperationGroup context. */

	aafObject* Obj = OpGroup;
	for (; Obj != NULL && aafUIDCmp (Obj->Class->ID, &AAFClassID_ContentStorage) == 0; Obj = Obj->Parent)
		if (!aafUIDCmp (Obj->Class->ID, &AAFClassID_OperationGroup))
			break;

	if (aafUIDCmp (OperationIdentification, &AAFOperationDef_MonoAudioGain)) {
		if (!aafUIDCmp (Obj->Class->ID, &AAFClassID_TimelineMobSlot)) {
			if (aafi->ctx.current_clip_gain_is_used == 0) {
				aafi_freeAudioGain (aafi->ctx.current_clip_gain);
			}

			if (aafi->ctx.current_clip_variable_gain_is_used == 0) {
				aafi_freeAudioGain (aafi->ctx.current_clip_variable_gain);
			}

			RESET_CTX__AudioGain (aafi->ctx);
		}
	}

	return rc;
}

/*
 *           Parameter (abs)
 *               |
 *       ,--------------.
 *       |              |
 * ConstantValue   VaryingValue
 *
 *
 * A Parameter object shall be owned by an OperationGroup object.
 */

static int
parse_Parameter (AAF_Iface* aafi, aafObject* Parameter, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 0);

	if (aafUIDCmp (Parameter->Class->ID, &AAFClassID_ConstantValue)) {
		return parse_ConstantValue (aafi, Parameter, &__td);
	} else if (aafUIDCmp (Parameter->Class->ID, &AAFClassID_VaryingValue)) {
		return parse_VaryingValue (aafi, Parameter, &__td);
	} else {
		__td_set (__td, __ptd, 1);
		TRACE_OBJ_ERROR (aafi, Parameter, &__td, "Parameter is neither of class Constant nor Varying : %s", aaft_ClassIDToText (aafi->aafd, Parameter->Class->ID));
	}

	return -1;
}

static int
parse_ConstantValue (AAF_Iface* aafi, aafObject* ConstantValue, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 1);

	if (!aaf_get_propertyValue (ConstantValue->Parent, PID_OperationGroup_InputSegments, &AAFTypeID_SegmentStrongReferenceVector)) {
		__td.eob = 1;
	}

	aafUID_t* ParamDef = aaf_get_propertyValue (ConstantValue, PID_Parameter_Definition, &AAFTypeID_AUID);

	if (!ParamDef) {
		TRACE_OBJ_ERROR (aafi, ConstantValue, &__td, "Missing Parameter::Definition");
		return -1;
	}

	aafWeakRef_t* OperationDefWeakRef = aaf_get_propertyValue (ConstantValue->Parent, PID_OperationGroup_Operation, &AAFTypeID_OperationDefinitionWeakReference);

	if (!OperationDefWeakRef) {
		TRACE_OBJ_ERROR (aafi, ConstantValue, &__td, "Missing OperationGroup::Operation");
		return -1;
	}

	aafUID_t* OperationIdentification = aaf_get_OperationIdentificationByWeakRef (aafi->aafd, OperationDefWeakRef);

	if (!OperationIdentification) {
		TRACE_OBJ_ERROR (aafi, ConstantValue, &__td, "Could not retrieve OperationIdentification from parent");
		return -1;
	}

	aafIndirect_t* Indirect = aaf_get_propertyValue (ConstantValue, PID_ConstantValue_Value, &AAFTypeID_Indirect);

	if (!Indirect) {
		TRACE_OBJ_ERROR (aafi, ConstantValue, &__td, "Missing ConstantValue::Value");
		return -1;
	}

	aafObject* ParentMob = aaf_get_ObjectAncestor (ConstantValue, &AAFClassID_Mob);

	aafUID_t* mobUsageCode = aaf_get_propertyValue (ParentMob, PID_Mob_UsageCode, &AAFTypeID_UsageType);

	if (aafUIDCmp (OperationIdentification, &AAFOperationDef_MonoAudioGain) &&
	    aafUIDCmp (ParamDef, &AAFParameterDef_Amplitude)) {
		aafRational_t* value = aaf_get_indirectValue (aafi->aafd, Indirect, &AAFTypeID_Rational);

		if (!value) {
			TRACE_OBJ_ERROR (aafi, ConstantValue, &__td, "Could not retrieve Indirect value for ConstantValue::Value");
			return -1;
		}

		aafiAudioGain* Gain = aafi_newAudioGain (aafi, AAFI_AUDIO_GAIN_CONSTANT, 0, value);

		if (!Gain) {
			TRACE_OBJ_ERROR (aafi, ConstantValue, &__td, "Could not create new gain");
			return -1;
		}

		if (aafUIDCmp (mobUsageCode, &AAFUsage_AdjustedClip)) {
			/*
			 * « Some applications support the notion of an adjusted-clip in which an
			 * effect is applied directly to a clip and applies to all uses of that
			 * clip, e.g. an audio gain effect. »
			 *
			 * Only Avid Media Composer seems to make use of AdjustedClip, in a way that
			 * it doesn't affect the timeline composition. It looks like any gain applied
			 * to a source clip (inside a bin), is just audible when playing that clip
			 * in preview.
			 *
			 * Thus, we just ignore it.
			 */
			debug ("Ignoring AdjustedClip audio level: %i/%i (%+05.1lf dB) ", Gain->value[0].numerator, Gain->value[0].denominator, 20 * log10 (aafRationalToDouble (Gain->value[0])));
			TRACE_OBJ_WARNING (aafi, ConstantValue, &__td, "Ignoring AdjustedClip audio level");
			aafi_freeAudioGain (Gain);
		} else if (aafi->ctx.current_opgroup_affect_track) {
			/*
			 * Track-based Volume
			 */
			if (!aafi->ctx.current_track) {
				TRACE_OBJ_ERROR (aafi, ConstantValue, &__td, "Current track not set, dropping this volume: %i/%i (%+05.1lf dB)", Gain->value[0].numerator, Gain->value[0].denominator, 20 * log10 (aafRationalToDouble (Gain->value[0])));
				aafi_freeAudioGain (Gain);
				return -1;
			} else if (aafi->ctx.current_track->gain) {
				TRACE_OBJ_ERROR (aafi, ConstantValue, &__td, "Track volume was already set, dropping this one: %i/%i (%+05.1lf dB)", Gain->value[0].numerator, Gain->value[0].denominator, 20 * log10 (aafRationalToDouble (Gain->value[0])));
				aafi_freeAudioGain (Gain);
				return -1;
			} else {
				aafi->ctx.current_track->gain = Gain;
				TRACE_OBJ (aafi, ConstantValue, &__td);
			}
		} else {
			/*
			 * Clip-based Gain
			 * Gain is saved in context and it will be set to all OperationGroup descendent clips.
			 */
			if (aafi->ctx.current_clip_gain) {
				TRACE_OBJ_ERROR (aafi, ConstantValue, &__td, "Clip gain was already set, dropping this one: %i/%i (%+05.1lf dB)", Gain->value[0].numerator, Gain->value[0].denominator, 20 * log10 (aafRationalToDouble (Gain->value[0])));
				aafi_freeAudioGain (Gain);
				return -1;
			} else {
				aafi->ctx.current_clip_gain = Gain;
				TRACE_OBJ (aafi, ConstantValue, &__td);
			}
		}
	} else if (aafUIDCmp (OperationIdentification, &AAFOperationDef_MonoAudioPan) &&
	           aafUIDCmp (ParamDef, &AAFParameterDef_Pan)) {
		aafRational_t* value = aaf_get_indirectValue (aafi->aafd, Indirect, &AAFTypeID_Rational);

		if (!value) {
			TRACE_OBJ_ERROR (aafi, ConstantValue, &__td, "Could not retrieve Indirect value for ConstantValue::Value");
			return -1;
		}

		if (!aafi->ctx.current_opgroup_affect_track) {
			/*
			 * « Pan automation shall be track-based. If an application has a different
			 * native representation (e.g., clip-based pan), it shall convert to and
			 * from its native representation when exporting and importing the composition. »
			 *
			 * NOTE: Never encountered clip-based pan AAF.
			 */
			TRACE_OBJ_ERROR (aafi, ConstantValue, &__td, "Pan shall be track based");
			return -1;
		}

		if (!aafi->ctx.current_track) {
			TRACE_OBJ_ERROR (aafi, ConstantValue, &__td, "Current track not set");
			return -1;
		}

		aafiAudioPan* Pan = aafi_newAudioPan (aafi, AAFI_AUDIO_GAIN_CONSTANT, 0, value);

		if (!Pan) {
			TRACE_OBJ_ERROR (aafi, ConstantValue, &__td, "Could not create new pan");
			return -1;
		}

		aafi->ctx.current_track->pan = Pan;
		TRACE_OBJ (aafi, ConstantValue, &__td);
	} else {
		TRACE_OBJ_NO_SUPPORT (aafi, ConstantValue, &__td);
	}

	return 0;
}

static int
parse_VaryingValue (AAF_Iface* aafi, aafObject* VaryingValue, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 1);

	if (!aaf_get_propertyValue (VaryingValue->Parent, PID_OperationGroup_InputSegments, &AAFTypeID_SegmentStrongReferenceVector)) {
		__td.eob = 1;
	}

	aafUID_t* ParamDef = aaf_get_propertyValue (VaryingValue, PID_Parameter_Definition, &AAFTypeID_AUID);

	if (!ParamDef) {
		TRACE_OBJ_ERROR (aafi, VaryingValue, &__td, "Missing Parameter::Definition");
		return -1;
	}

	aafWeakRef_t* OperationDefWeakRef = aaf_get_propertyValue (VaryingValue->Parent, PID_OperationGroup_Operation, &AAFTypeID_OperationDefinitionWeakReference);

	if (!OperationDefWeakRef) {
		TRACE_OBJ_ERROR (aafi, VaryingValue, &__td, "Missing OperationGroup::Operation");
		return -1;
	}

	aafUID_t* OperationIdentification = aaf_get_OperationIdentificationByWeakRef (aafi->aafd, OperationDefWeakRef);

	if (!OperationIdentification) {
		TRACE_OBJ_ERROR (aafi, VaryingValue, &__td, "Could not retrieve OperationIdentification from parent");
		return -1;
	}

	aafWeakRef_t* InterpolationDefWeakRef = aaf_get_propertyValue (VaryingValue, PID_VaryingValue_Interpolation, &AAFTypeID_InterpolationDefinitionWeakReference);

	if (!InterpolationDefWeakRef) {
		TRACE_OBJ_ERROR (aafi, VaryingValue, &__td, "Missing VaryingValue::Interpolation.");
		return -1;
	}

	aafiInterpolation_e interpolation               = 0;
	aafUID_t*           InterpolationIdentification = aaf_get_InterpolationIdentificationByWeakRef (aafi->aafd, InterpolationDefWeakRef);

	if (!InterpolationIdentification) {
		TRACE_OBJ_WARNING (aafi, VaryingValue, &__td, "Could not retrieve InterpolationIdentification: Falling back to Linear");
		interpolation = AAFI_INTERPOL_LINEAR;
	} else if (aafUIDCmp (InterpolationIdentification, &AAFInterpolationDef_None)) {
		interpolation = AAFI_INTERPOL_NONE;
	} else if (aafUIDCmp (InterpolationIdentification, &AAFInterpolationDef_Linear)) {
		interpolation = AAFI_INTERPOL_LINEAR;
	} else if (aafUIDCmp (InterpolationIdentification, &AAFInterpolationDef_Power)) {
		interpolation = AAFI_INTERPOL_POWER;
	} else if (aafUIDCmp (InterpolationIdentification, &AAFInterpolationDef_Constant)) {
		interpolation = AAFI_INTERPOL_CONSTANT;
	} else if (aafUIDCmp (InterpolationIdentification, &AAFInterpolationDef_BSpline)) {
		interpolation = AAFI_INTERPOL_BSPLINE;
	} else if (aafUIDCmp (InterpolationIdentification, &AAFInterpolationDef_Log)) {
		interpolation = AAFI_INTERPOL_LOG;
	} else {
		TRACE_OBJ_WARNING (aafi, VaryingValue, &__td, "Unknown InterpolationIdentification value: Falling back to Linear");
		interpolation = AAFI_INTERPOL_LINEAR;
	}

	aafObject* Points = aaf_get_propertyValue (VaryingValue, PID_VaryingValue_PointList, &AAFTypeID_ControlPointStrongReferenceVector);

	if (!Points) {
		/*
		 * Some AAF files from ProTools and LogicPro break standard by having no
		 * PointList entry for AAFOperationDef_MonoAudioGain.
		 */
		TRACE_OBJ_WARNING (aafi, VaryingValue, &__td, "Missing VaryingValue::PointList");
		return -1;
	}

	if (aafUIDCmp (OperationIdentification, &AAFOperationDef_MonoAudioDissolve) &&
	    aafUIDCmp (ParamDef, &AAFParameterDef_Level)) {
		aafiTransition* Trans = aafi->ctx.current_transition;

		if (!Trans) {
			TRACE_OBJ_ERROR (aafi, VaryingValue, &__td, "Context current_transition not set");
			return -1;
		}

		Trans->flags |= interpolation;

		/*
		 * OperationGroup *might* contain a Parameter (ParameterDef_Level) specifying
		 * the fade curve. However, this parameter is optional regarding AAF_EditProtocol
		 * and there is most likely no implementation that exports custom fade curves.
		 * Thus, we only retrieve ParameterDef_Level to set interpolation, and we
		 * always set the fade as defined in AAF_EditProtocol, with only two points :
		 *
		 * « ParameterDef_Level (optional; default is a VaryingValue object
		 * with two control points: Value 0 at time 0, and value 1 at time 1) »
		 */

		TRACE_OBJ (aafi, VaryingValue, &__td);
	} else if (aafUIDCmp (OperationIdentification, &AAFOperationDef_MonoAudioGain) &&
	           (aafUIDCmp (ParamDef, &AAFParameterDef_Amplitude) || aafUIDCmp (ParamDef, aaf_get_ParamDefIDByName (aafi->aafd, "AvidControlClipRatio")))) {
		aafiAudioGain* Gain = aafi_newAudioGain (aafi, 0, interpolation, NULL);

		if (!Gain) {
			TRACE_OBJ_ERROR (aafi, VaryingValue, &__td, "Could not create new gain");
			return -1;
		}

		int pts_cnt = retrieve_ControlPoints (aafi, Points, &Gain->time, &Gain->value);

		if (pts_cnt < 0) {
			TRACE_OBJ_ERROR (aafi, VaryingValue, &__td, "Could not retrieve ControlPoints");
			free (Gain);
			return -1;
		}

		Gain->pts_cnt = (unsigned int)pts_cnt;

		// for ( int i = 0; i < Gain->pts_cnt; i++ ) {
		// 	debug( "time_%i : %i/%i   value_%i : %i/%i", i, Gain->time[i].numerator, Gain->time[i].denominator, i, Gain->value[i].numerator, Gain->value[i].denominator );
		// }

		/*
		 * If gain has 2 ControlPoints with both the same value, it means
		 * we have a flat gain curve. So we can assume constant gain here.
		 */

		if (Gain->pts_cnt == 2 &&
		    (Gain->value[0].numerator == Gain->value[1].numerator) &&
		    (Gain->value[0].denominator == Gain->value[1].denominator)) {
			if (aafRationalToDouble (Gain->value[0]) == 1.0f) {
				/*
				 * Skipping any 1:1 gain allows not to miss any other actual gain (eg. DR_Audio_Levels.aaf, Resolve 18.5.AAF)
				 *
				     │ 02412││         ├──◻ AAFClassID_OperationGroup (OpIdent: AAFOperationDef_MonoAudioGain; Length: 284630)
				 err │ 03839││         │    ├──◻ AAFClassID_VaryingValue : : Value is continuous 1:1 (0db), skipping it.
				     │      ││         │    │
				     │ 02412││         │    └──◻ AAFClassID_OperationGroup (OpIdent: AAFOperationDef_MonoAudioGain; Length: 284630)
				     │ 03660││         │         ├──◻ AAFClassID_ConstantValue (Value: 6023/536870912  -99.0 dB)
				     │ 02983││         │         └──◻ AAFClassID_SourceClip (Length: 284630)
				     │ 02988││         │              └──◻ AAFClassID_MasterMob (UsageCode: n/a) : speech-sample.mp3 - -100db
				     │ 04553││         │                   └──◻ AAFClassID_TimelineMobSlot [slot:1 track:1] (DataDef : AAFDataDef_Sound)
				     │ 03193││         │                        └──◻ AAFClassID_SourceClip (Length: 284630)
				     │ 04297││         │                             └──◻ AAFClassID_SourceMob (UsageCode: n/a) : speech-sample.mp3 - -100db
				     │ 01342││         │                                  └──◻ AAFClassID_PCMDescriptor
				     │ 01529││         │                                       └──◻ AAFClassID_NetworkLocator : file:///C:/Users/user/Desktop/libAAF/test/res/speech-sample.mp3
				 */
				TRACE_OBJ_INFO (aafi, VaryingValue, &__td, "Value is continuous 1:1 (0db), skipping it.");
				aafi_freeAudioGain (Gain);
				return -1;
			}
			Gain->flags |= AAFI_AUDIO_GAIN_CONSTANT;
		} else {
			Gain->flags |= AAFI_AUDIO_GAIN_VARIABLE;
		}

		if (aafi->ctx.current_opgroup_affect_track) {
			/*
			 * Track-based Volume
			 */
			if (!aafi->ctx.current_track) {
				TRACE_OBJ_ERROR (aafi, VaryingValue, &__td, "Current track not set, dropping this volume");
				aafi_freeAudioGain (Gain);
				return -1;
			}
			if (aafi->ctx.current_track->gain) {
				TRACE_OBJ_ERROR (aafi, VaryingValue, &__td, "Track volume was already set");
				aafi_freeAudioGain (Gain);
				return -1;
			} else {
				aafi->ctx.current_track->gain = Gain;
				TRACE_OBJ (aafi, VaryingValue, &__td);
			}
		} else {
			/*
			 * Clip-based Gain
			 * Gain is saved in context and it will be set to all OperationGroup descendent clips.
			 */
			if (Gain->flags & AAFI_AUDIO_GAIN_CONSTANT) {
				if (aafi->ctx.current_clip_gain) {
					TRACE_OBJ_ERROR (aafi, VaryingValue, &__td, "Clip gain was already set");
					aafi_freeAudioGain (Gain);
					return -1;
				} else {
					aafi->ctx.current_clip_gain = Gain;
					TRACE_OBJ (aafi, VaryingValue, &__td);
				}
			} else {
				if (aafi->ctx.current_clip_variable_gain) {
					TRACE_OBJ_ERROR (aafi, VaryingValue, &__td, "Clip automation was already set");
					aafi_freeAudioGain (Gain);
					return -1;
				} else {
					aafi->ctx.current_clip_variable_gain = Gain;
					TRACE_OBJ (aafi, VaryingValue, &__td);
				}
			}
		}
	} else if (aafUIDCmp (OperationIdentification, &AAFOperationDef_MonoAudioPan) &&
	           aafUIDCmp (ParamDef, &AAFParameterDef_Pan)) {
		if (!aafi->ctx.current_opgroup_affect_track) {
			/*
			 * « Pan automation shall be track-based. If an application has a different
			 * native representation (e.g., clip-based pan), it shall convert to and
			 * from its native representation when exporting and importing the composition. »
			 *
			 * NOTE: Never encountered clip-based pan AAF.
			 */
			TRACE_OBJ_ERROR (aafi, VaryingValue, &__td, "Pan shall be track based");
			return -1;
		}

		if (!aafi->ctx.current_track) {
			TRACE_OBJ_ERROR (aafi, VaryingValue, &__td, "Current track not set");
			return -1;
		}

		if (aafi->ctx.current_track->pan) {
			TRACE_OBJ_ERROR (aafi, VaryingValue, &__td, "Track Pan was already set");
			return -1;
		}

		aafiAudioPan* Pan = aafi_newAudioPan (aafi, 0, interpolation, NULL);

		if (!Pan) {
			TRACE_OBJ_ERROR (aafi, VaryingValue, &__td, "Could not create new pan");
			return -1;
		}

		int pts_cnt = retrieve_ControlPoints (aafi, Points, &Pan->time, &Pan->value);

		if (pts_cnt < 0) {
			TRACE_OBJ_ERROR (aafi, VaryingValue, &__td, "Could not retrieve ControlPoints");
			free (Pan);
			return -1;
		}

		Pan->pts_cnt = (unsigned int)pts_cnt;

		// for ( int i = 0; i < Gain->pts_cnt; i++ ) {
		// 	debug( "time_%i : %i/%i   value_%i : %i/%i", i, Gain->time[i].numerator, Gain->time[i].denominator, i, Gain->value[i].numerator, Gain->value[i].denominator  );
		// }

		/*
		 * If Pan has 2 ControlPoints with both the same value, it means
		 * we have a constant Pan curve. So we can assume constant Pan here.
		 */

		if (Pan->pts_cnt == 2 &&
		    (Pan->value[0].numerator == Pan->value[1].numerator) &&
		    (Pan->value[0].denominator == Pan->value[1].denominator)) {
			Pan->flags |= AAFI_AUDIO_GAIN_CONSTANT;
		} else {
			Pan->flags |= AAFI_AUDIO_GAIN_VARIABLE;
		}

		aafi->ctx.current_track->pan = Pan;
		TRACE_OBJ (aafi, VaryingValue, &__td);
	} else {
		TRACE_OBJ_NO_SUPPORT (aafi, VaryingValue, &__td);
	}

	return 0;
}

/* ****************************************************************************
 *                    E s s e n c e D e s c r i p t o r
 * ****************************************************************************
 *
 *  EssenceDescriptor (abs)
 *          |
 *          |--> FileDescriptor (abs)
 *          |          |
 *          |          |--> WAVEDescriptor
 *          |          |--> AIFCDescriptor
 *          |          |--> SoundDescriptor
 *          |          |          |
 *          |          |          `--> PCMDescriptor
 *          |          |
 *          |          `--> DigitalImageDescriptor (abs)
 *          |                     |
 *          |                     `--> CDCIDescriptor
 *          |
 *          |
 *          |--> PhysicalDescriptor
 *          `--> TapeDescriptor
 */

static int
parse_EssenceDescriptor (AAF_Iface* aafi, aafObject* EssenceDesc, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 1);

	if (!aaf_get_property (EssenceDesc, PID_EssenceDescriptor_Locator))
		__td.eob = 1;

	if (aafUIDCmp (EssenceDesc->Class->ID, &AAFClassID_PCMDescriptor)) {
		parse_PCMDescriptor (aafi, EssenceDesc, &__td);
	} else if (aafUIDCmp (EssenceDesc->Class->ID, &AAFClassID_WAVEDescriptor)) {
		parse_WAVEDescriptor (aafi, EssenceDesc, &__td);
	} else if (aafUIDCmp (EssenceDesc->Class->ID, &AAFClassID_AIFCDescriptor)) {
		parse_AIFCDescriptor (aafi, EssenceDesc, &__td);
	} else if (aafUIDCmp (EssenceDesc->Class->ID, &AAFClassID_SoundDescriptor)) {
		/* Compressed Audio (MP3, AAC ?). Not encountered yet (Davinci Resolve describes MP3 using PCMDescriptor...) */
		TRACE_OBJ_NO_SUPPORT (aafi, EssenceDesc, &__td);
	} else if (aafUIDCmp (EssenceDesc->Class->ID, &AAFClassID_AES3PCMDescriptor)) {
		/* Not described in specs, not encountered yet. */
		TRACE_OBJ_NO_SUPPORT (aafi, EssenceDesc, &__td);
	} else if (aafUIDCmp (EssenceDesc->Class->ID, &AAFClassID_MultipleDescriptor)) {
		/*
		 * A MultipleDescriptor contains a vector of FileDescriptor objects and is
		 * used when the file source consists of multiple tracks of essence (e.g MXF).
		 * Each essence track is described by a MobSlots object in the SourceMob and a
		 * FileDescriptor object. The FileDescriptor is linked to the MobSlot by
		 * setting the FileDescriptor::LinkedSlotID property equal to the
		 * MobSlot::SlotID property.
		 *
		 * -> test.aaf
		 * -> /test/private/thirdparty/2997fps-DFTC.aaf
		 */

		TRACE_OBJ_NO_SUPPORT (aafi, EssenceDesc, &__td);
	} else if (aafUIDCmp (EssenceDesc->Class->ID, &AAFClassID_CDCIDescriptor)) {
		parse_CDCIDescriptor (aafi, EssenceDesc, &__td);
	} else {
		TRACE_OBJ_NO_SUPPORT (aafi, EssenceDesc, &__td);
	}

	/*
	 * Locators are a property of EssenceDescriptor. The property holds a vector of
	 * Locators object, that should provide information to help find a file that
	 * contains the essence (WAV, MXF, etc.) or to help find the physical media.
	 *
	 * A Locator can either be a NetworkLocator or a TextLocator.
	 *
	 * A NetworkLocator holds a URLString property :
	 *
	 * p.41 : Absolute Uniform Resource Locator (URL) complying with RFC 1738 or relative
	 * Uniform Resource Identifier (URI) complying with RFC 2396 for file containing
	 * the essence. If it is a relative URI, the base URI is determined from the URI
	 * of the AAF file itself.
	 * Informative note: A valid URL or URI uses a constrained character set and uses
	 * the / character as the path separator.
	 */

	aafObject* Locator  = NULL;
	aafObject* Locators = aaf_get_propertyValue (EssenceDesc, PID_EssenceDescriptor_Locator, &AAFTypeID_LocatorStrongReferenceVector);

	uint32_t i = 0;
	AAFI_foreach_ObjectInSet (&Locator, Locators, i, __td)
	{
		/* TODO retrieve all locators, then when searching file, try all parsed locators. */
		parse_Locator (aafi, Locator, &__td);
	}

	return 0;
}

static int
parse_PCMDescriptor (AAF_Iface* aafi, aafObject* PCMDescriptor, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 0);

	aafiAudioEssenceFile* audioEssenceFile = (aafiAudioEssenceFile*)aafi->ctx.current_audio_essence;

	if (!audioEssenceFile) {
		TRACE_OBJ_ERROR (aafi, PCMDescriptor, &__td, "aafi->ctx.current_audio_essence not set");
		return -1;
	}

	audioEssenceFile->type = AAFI_ESSENCE_TYPE_PCM;

	/* Duration of the essence in sample units (not edit units !) */
	aafPosition_t* length = aaf_get_propertyValue (PCMDescriptor, PID_FileDescriptor_Length, &AAFTypeID_PositionType);

	if (!length) {
		TRACE_OBJ_ERROR (aafi, PCMDescriptor, &__td, "Missing FileDescriptor::Length");
		return -1;
	}

	audioEssenceFile->length = *length;

	uint32_t* channels = aaf_get_propertyValue (PCMDescriptor, PID_SoundDescriptor_Channels, &AAFTypeID_UInt32);

	if (!channels) {
		TRACE_OBJ_ERROR (aafi, PCMDescriptor, &__td, "Missing SoundDescriptor::Channels");
		return -1;
	}

	if (*channels >= USHRT_MAX) {
		TRACE_OBJ_ERROR (aafi, PCMDescriptor, &__td, "SoundDescriptor::Channels bigger than USHRT_MAX");
		return -1;
	}

	audioEssenceFile->channels = *(uint16_t*)channels;

	aafRational_t* samplerate = aaf_get_propertyValue (PCMDescriptor, PID_FileDescriptor_SampleRate, &AAFTypeID_Rational);

	if (!samplerate) {
		TRACE_OBJ_ERROR (aafi, PCMDescriptor, &__td, "Missing FileDescriptor::SampleRate");
		return -1;
	}

	if (samplerate->denominator != 1) {
		TRACE_OBJ_ERROR (aafi, PCMDescriptor, &__td, "FileDescriptor::SampleRate should be integer but is %i/%i", samplerate->numerator, samplerate->denominator);
		return -1;
	}

	if (samplerate->numerator < 0) {
		TRACE_OBJ_ERROR (aafi, PCMDescriptor, &__td, "FileDescriptor::SampleRate value is invalid : %i", samplerate->numerator);
		return -1;
	}

	audioEssenceFile->samplerate = (uint32_t)samplerate->numerator;

	audioEssenceFile->samplerateRational->numerator   = samplerate->numerator;
	audioEssenceFile->samplerateRational->denominator = samplerate->denominator;

	uint32_t* samplesize = aaf_get_propertyValue (PCMDescriptor, PID_SoundDescriptor_QuantizationBits, &AAFTypeID_UInt32);

	if (!samplesize) {
		TRACE_OBJ_ERROR (aafi, PCMDescriptor, &__td, "Missing SoundDescriptor::QuantizationBits");
		return -1;
	}

	if (*samplesize >= USHRT_MAX) {
		TRACE_OBJ_ERROR (aafi, PCMDescriptor, &__td, "SoundDescriptor::QuantizationBits bigger than USHRT_MAX : %u", samplesize);
		return -1;
	}

	TRACE_OBJ (aafi, PCMDescriptor, &__td);

	audioEssenceFile->samplesize = *(uint16_t*)samplesize;

	return 0;
}

static int
parse_WAVEDescriptor (AAF_Iface* aafi, aafObject* WAVEDescriptor, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 0);

	aafiAudioEssenceFile* audioEssenceFile = (aafiAudioEssenceFile*)aafi->ctx.current_audio_essence;

	if (!audioEssenceFile) {
		TRACE_OBJ_ERROR (aafi, WAVEDescriptor, &__td, "aafi->ctx.current_audio_essence not set");
		return -1;
	}

	audioEssenceFile->type = AAFI_ESSENCE_TYPE_WAVE;

	aafProperty* summary = aaf_get_property (WAVEDescriptor, PID_WAVEDescriptor_Summary);

	if (!summary) {
		TRACE_OBJ_ERROR (aafi, WAVEDescriptor, &__td, "Missing WAVEDescriptor::Summary");
		return -1;
	}

	audioEssenceFile->summary = summary;

	/*
	 * NOTE : Summary is parsed later in "post-processing" aafi_retrieveData(),
	 * to be sure clips and essences are linked, so we are able to fallback on
	 * essence stream in case summary does not contain the full header part.
	 *
	 * TODO parse it here
	 */

	TRACE_OBJ (aafi, WAVEDescriptor, &__td);

	return 0;
}

static int
parse_AIFCDescriptor (AAF_Iface* aafi, aafObject* AIFCDescriptor, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 0);

	aafiAudioEssenceFile* audioEssenceFile = (aafiAudioEssenceFile*)aafi->ctx.current_audio_essence;

	if (!audioEssenceFile) {
		TRACE_OBJ_ERROR (aafi, AIFCDescriptor, &__td, "aafi->ctx.current_audio_essence not set");
		return -1;
	}

	audioEssenceFile->type = AAFI_ESSENCE_TYPE_AIFC;

	aafProperty* summary = aaf_get_property (AIFCDescriptor, PID_AIFCDescriptor_Summary);

	if (!summary) {
		TRACE_OBJ_ERROR (aafi, AIFCDescriptor, &__td, "Missing AIFCDescriptor::Summary");
		return -1;
	}

	audioEssenceFile->summary = summary;

	/*
	 * NOTE : Summary is parsed later in "post-processing" aafi_retrieveData(),
	 * to be sure clips and essences are linked, so we are able to fallback on
	 * essence stream in case summary does not contain the full header part.
	 */

	TRACE_OBJ (aafi, AIFCDescriptor, &__td);

	return 0;
}

static int
parse_DigitalImageDescriptor (AAF_Iface* aafi, aafObject* DIDescriptor, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 0);

	/* TODO parse and save content to videoEssenceFile */

	aafiVideoEssence* videoEssenceFile = aafi->ctx.current_video_essence;

	if (!videoEssenceFile) {
		TRACE_OBJ_ERROR (aafi, DIDescriptor, &__td, "aafi->ctx.current_video_essence not set");
		return -1;
	}

	/*
	 * « Informative note: In the case of picture essence, the Sample Rate is usually the frame rate. The value should be
	 * numerically exact, for example {25,1} or {30000, 1001}. »
	 *
	 * « Informative note: Care should be taken if a sample rate of {2997,100} is encountered, since this may have been intended
	 * as a (mistaken) approximation to the exact value. »
	 */

	aafRational_t* framerate = aaf_get_propertyValue (DIDescriptor, PID_FileDescriptor_SampleRate, &AAFTypeID_Rational);

	if (!framerate) {
		TRACE_OBJ_ERROR (aafi, DIDescriptor, &__td, "Missing FileDescriptor::SampleRate (framerate)");
		return -1;
	}

	videoEssenceFile->framerate = framerate;

	debug ("Video framerate : %i/%i", framerate->numerator, framerate->denominator);

	/*
	 * All mandatory properties below are treated as optional, because we assume that
	 * video will be an external file so we are not using those, and because some AAF
	 * implementations does not even set those mandatory properties (eg. Davinci Resolve).
	 *
	 * TODO: parse PID_FileDescriptor_Length ?
	 */

	return 0;
}

static int
parse_CDCIDescriptor (AAF_Iface* aafi, aafObject* CDCIDescriptor, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 0);

	/* TODO parse CDCI class */

	int rc = parse_DigitalImageDescriptor (aafi, CDCIDescriptor, __ptd);

	if (!rc)
		TRACE_OBJ (aafi, CDCIDescriptor, &__td);

	return rc;
}

/*
 *            Locator (abs)
 *               |
 *       ,---------------.
 *       |               |
 * NetworkLocator   TextLocator
 */

static int
parse_Locator (AAF_Iface* aafi, aafObject* Locator, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 1);
	__td.eob = 1;

	if (aafUIDCmp (Locator->Class->ID, &AAFClassID_NetworkLocator)) {
		parse_NetworkLocator (aafi, Locator, &__td);
	} else if (aafUIDCmp (Locator->Class->ID, &AAFClassID_TextLocator)) {
		/*
		 * A TextLocator object provides information to the user to help locate the file
		 * containing the essence or to locate the physical media. The TextLocator is not
		 * intended for applications to use without user intervention.
		 *
		 * Not encountered yet.
		 */

		TRACE_OBJ_NO_SUPPORT (aafi, Locator, &__td);

		char* name = aaf_get_propertyValue (Locator, PID_TextLocator_Name, &AAFTypeID_String);
		debug ("Got an AAFClassID_TextLocator : \"%s\"", name);
		free (name);
	} else {
		TRACE_OBJ_NO_SUPPORT (aafi, Locator, &__td);
	}

	return 0;
}

static int
parse_NetworkLocator (AAF_Iface* aafi, aafObject* NetworkLocator, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 0);

	/*
	 * This holds an URI pointing to the essence file, when it is not embedded.
	 * However, sometimes it holds an URI to the AAF file itself when essence is
	 * embedded so it's not a reliable way to guess if essence is embedded or not.
	 */

	char* original_file_path = aaf_get_propertyValue (NetworkLocator, PID_NetworkLocator_URLString, &AAFTypeID_String);

	if (original_file_path == NULL) {
		TRACE_OBJ_ERROR (aafi, NetworkLocator, &__td, "Missing NetworkLocator::URLString");
		goto err;
	}

	if (aafi->ctx.current_audio_essence) {
		if (aafi->ctx.current_audio_essence->original_file_path) {
			TRACE_OBJ_ERROR (aafi, NetworkLocator, &__td, "File path was already set");
			goto err;
		} else {
			aafi->ctx.current_audio_essence->original_file_path = original_file_path;
		}
	} else if (aafi->ctx.current_video_essence) {
		if (aafi->ctx.current_video_essence->original_file_path) {
			TRACE_OBJ_ERROR (aafi, NetworkLocator, &__td, "File path was already set");
			goto err;
		} else {
			aafi->ctx.current_video_essence->original_file_path = original_file_path;
		}
	} else {
		TRACE_OBJ_ERROR (aafi, NetworkLocator, &__td, "No current essence set");
		goto err;
	}

	TRACE_OBJ (aafi, NetworkLocator, &__td);

	return 0;

err:
	free (original_file_path);

	return -1;
}

static int
parse_EssenceData (AAF_Iface* aafi, aafObject* EssenceData, td* __ptd)
{
	struct trace_dump __td;
	__td_set (__td, __ptd, 1);
	__td.eob = 1;

	int rc = 0;

	char* streamName = NULL;
	char* dataPath   = NULL;

	aafiAudioEssenceFile* audioEssenceFile = (aafiAudioEssenceFile*)aafi->ctx.current_audio_essence;

	if (!audioEssenceFile) {
		TRACE_OBJ_ERROR (aafi, EssenceData, &__td, "aafi->ctx.current_audio_essence not set");
		return -1;
	}

	streamName = aaf_get_propertyValue (EssenceData, PID_EssenceData_Data, &AAFTypeID_String);

	if (!streamName) {
		TRACE_OBJ_ERROR (aafi, EssenceData, &__td, "Missing EssenceData::Data");
		goto err;
	}

	char* path = aaf_get_ObjectPath (EssenceData);

	if (!path) {
		TRACE_OBJ_ERROR (aafi, EssenceData, &__td, "Could not retrieve EssenceData node path");
		goto err;
	}

	dataPath = laaf_util_build_path (AAF_DIR_SEP_STR, path, streamName, NULL);

	if (!dataPath) {
		TRACE_OBJ_ERROR (aafi, EssenceData, &__td, "Could not build Data stream path");
		goto err;
	}

	cfbNode* DataNode = cfb_getNodeByPath (aafi->aafd->cfbd, dataPath, 0);

	if (!DataNode) {
		TRACE_OBJ_ERROR (aafi, EssenceData, &__td, "Could not retrieve Data stream node: %s", dataPath);
		goto err;
	}

	TRACE_OBJ (aafi, EssenceData, &__td);

	debug ("Embedded data stream : %s", dataPath);

	audioEssenceFile->node        = DataNode;
	audioEssenceFile->is_embedded = 1;

	rc = 0;

	goto end;

err:
	rc = -1;

end:

	free (streamName);
	free (dataPath);

	return rc;
}

static int
retrieve_UserComments (AAF_Iface* aafi, aafObject* UserComments, aafiMetaData** metadataList)
{
	aafObject* UserComment = NULL;

	int error = 0;

	AAF_foreach_ObjectInSet (&UserComment, UserComments, NULL)
	{
		char* text = NULL;
		char* name = NULL;

		if (!aafUIDCmp (UserComment->Class->ID, &AAFClassID_TaggedValue)) {
			warning ("Parsing UserComments: Expected TaggedValue but got %s", aaft_ClassIDToText (aafi->aafd, UserComment->Class->ID));
			goto UserCommentError;
		}

		name = aaf_get_propertyValue (UserComment, PID_TaggedValue_Name, &AAFTypeID_String);

		if (!name) {
			warning ("Parsing UserComments: Missing TaggedValue::Name");
			goto UserCommentError;
		}

		aafIndirect_t* Indirect = aaf_get_propertyValue (UserComment, PID_TaggedValue_Value, &AAFTypeID_Indirect);

		if (!Indirect) {
			warning ("Parsing UserComments: Missing TaggedValue::Value");
			goto UserCommentError;
		}

		text = aaf_get_indirectValue (aafi->aafd, Indirect, &AAFTypeID_String);

		if (!text) {
			warning ("Parsing UserComments: Could not retrieve Indirect value for TaggedValue::Value");
			goto UserCommentError;
		}

		aafiMetaData* Comment = aafi_newMetadata (aafi, metadataList);

		if (!Comment) {
			warning ("Parsing UserComments: Could not create new UserComment");
			goto UserCommentError;
		}

		Comment->name = name;
		Comment->text = text;

		continue;

	UserCommentError:

		error++;

		free (name);
		free (text);
	}

	if (error)
		return -1;

	return 0;
}

static int
retrieve_ControlPoints (AAF_Iface* aafi, aafObject* Points, aafRational_t* times[], aafRational_t* values[])
{
	/*
	 * We do not handle trace here, because there could possibly be hundreds of
	 * ControlPoints to print, plus retrieve_ControlPoints() is being called before
	 * VaryingValue Object is logged.
	 */

	*times  = calloc (Points->Header->_entryCount, sizeof (aafRational_t));
	*values = calloc (Points->Header->_entryCount, sizeof (aafRational_t));

	if (!*times || !*values) {
		error ("Out of memory");
		return -1;
	}

	aafObject* Point = NULL;

	unsigned int i = 0;

	AAF_foreach_ObjectInSet (&Point, Points, NULL)
	{
		if (!aafUIDCmp (Point->Class->ID, &AAFClassID_ControlPoint)) {
			error ("Object is not AAFClassID_ControlPoint : %s", aaft_ClassIDToText (aafi->aafd, Point->Class->ID));
			continue;
		}

		aafRational_t* time = aaf_get_propertyValue (Point, PID_ControlPoint_Time, &AAFTypeID_Rational);

		if (!time) {
			error ("Missing ControlPoint::Time");

			free (*times);
			*times = NULL;
			free (*values);
			*values = NULL;

			return -1;
		}

		aafIndirect_t* Indirect = aaf_get_propertyValue (Point, PID_ControlPoint_Value, &AAFTypeID_Indirect);

		if (!Indirect) {
			error ("Missing Indirect ControlPoint::Value");

			free (*times);
			*times = NULL;
			free (*values);
			*values = NULL;

			return -1;
		}

		aafRational_t* value = aaf_get_indirectValue (aafi->aafd, Indirect, &AAFTypeID_Rational);

		if (!value) {
			error ("Could not retrieve Indirect value for ControlPoint::Value");

			free (*times);
			*times = NULL;
			free (*values);
			*values = NULL;

			return -1;
		}

		memcpy ((*times + i), time, sizeof (aafRational_t));
		memcpy ((*values + i), value, sizeof (aafRational_t));

		i++;
	}

	if (Points->Header->_entryCount != i) {
		warning ("ControlPoints _entryCount (%i) does not match iteration (%i).", Points->Header->_entryCount, i);
		return (int)i;
	}

	return (int)Points->Header->_entryCount;
}

int
aafi_retrieveData (AAF_Iface* aafi)
{
	/* this __td is only here for debug/error, normal trace is printed from parse_Mob() */
	static td __td;
	__td.fn  = __LINE__;
	__td.pfn = 0;
	__td.lv  = 0;
	__td.ll  = calloc (1024, sizeof (int));
	if (!__td.ll) {
		error ("Out of memory");
		return -1;
	}
	__td.ll[0] = 0;

	int        compositionMobParsed = 0;
	aafObject* Mob                  = NULL;

	uint32_t i = 0;
	AAFI_foreach_ObjectInSet (&Mob, aafi->aafd->Mobs, i, __td)
	{
		if (aafUIDCmp (Mob->Class->ID, &AAFClassID_MasterMob) ||
		    aafUIDCmp (Mob->Class->ID, &AAFClassID_SourceMob)) {
			// TRACE_OBJ_WARNING( aafi, Mob, &__td, " PRINTS FOR DEBUG ONLY: Will be parsed later" );
			continue;
		}

		if (!aafUIDCmp (Mob->Class->ID, &AAFClassID_CompositionMob)) {
			/* there should not be anything other than MasterMob, SourceMob or CompositionMob */
			TRACE_OBJ_NO_SUPPORT (aafi, Mob, &__td);
			continue;
		}

		aafUID_t* UsageCode = aaf_get_propertyValue (Mob, PID_Mob_UsageCode, &AAFTypeID_UsageType);

		if (!aafUIDCmp (UsageCode, &AAFUsage_TopLevel) && (aafUIDCmp (aafi->aafd->Header.OperationalPattern, &AAFOPDef_EditProtocol) || UsageCode)) {
			/*
			 * If we run against AAFOPDef_EditProtocol, we process only TopLevels CompositionMobs.
			 * If there is more than one, we have multiple Compositions in a single AAF.
			 */
			// TRACE_OBJ_WARNING( aafi, Mob, &__td, " PRINTS FOR DEBUG ONLY: Will be parsed later" );
			continue;
		}

		if (compositionMobParsed) {
			TRACE_OBJ_ERROR (aafi, Mob, &__td, "Multiple top level CompositionMob not supported yet");
			continue;
		}

		RESET_CONTEXT (aafi->ctx);

		__td.fn  = __LINE__;
		__td.pfn = 0;
		__td.lv  = 0;

		parse_Mob (aafi, Mob, &__td);

		if (aafUIDCmp (UsageCode, &AAFUsage_TopLevel)) {
			compositionMobParsed = 1;
		}
	}

	free (__td.ll);

	if (aafi->Timecode == NULL) {
		/* TODO, shouldn't we leave aafi->Timecode as NULL ? */
		warning ("No timecode found in file. Setting to 00:00:00:00 @ 25fps");

		aafiTimecode* tc = calloc (1, sizeof (aafiTimecode));

		if (!tc) {
			error ("Out of memory");
			return -1;
		}

		tc->start     = 0;
		tc->fps       = 25;
		tc->drop      = 0;
		tc->edit_rate = &AAFI_DEFAULT_TC_EDIT_RATE;

		aafi->Timecode = tc;
	}

	/* Post processing */

	aafiAudioEssenceFile* audioEssenceFile = NULL;

	AAFI_foreachAudioEssenceFile (aafi, audioEssenceFile)
	{
		if (!audioEssenceFile->is_embedded) {
			audioEssenceFile->usable_file_path = aafi_locate_external_essence_file (aafi, audioEssenceFile->original_file_path, aafi->ctx.options.media_location);

			if (audioEssenceFile->usable_file_path == NULL) {
				warning ("Could not locate external audio essence file '%s'", audioEssenceFile->original_file_path);
			}
		}

		if (audioEssenceFile->summary || audioEssenceFile->usable_file_path) {
			aafi_parse_audio_essence (aafi, audioEssenceFile);
		}
	}

	/*
	 * Define AAF samplerate and samplesize with the most used values accross all audio essences.
	 */

	uint32_t maxOccurence = 0;

	AAFI_foreachAudioEssenceFile (aafi, audioEssenceFile)
	{
		uint32_t              count = 1;
		aafiAudioEssenceFile* ae    = NULL;

		if (audioEssenceFile->samplerate == aafi->Audio->samplerate &&
		    audioEssenceFile->samplesize == aafi->Audio->samplesize) {
			continue;
		}

		AAFI_foreachEssence (audioEssenceFile->next, ae)
		{
			if (audioEssenceFile->samplerate == ae->samplerate && audioEssenceFile->samplesize == ae->samplesize) {
				count++;
			}
		}

		debug ("Essence count @ %u Hz / %u bits : %i", audioEssenceFile->samplerate, audioEssenceFile->samplesize, count);

		if (count > maxOccurence) {
			maxOccurence                    = count;
			aafi->Audio->samplesize         = audioEssenceFile->samplesize;
			aafi->Audio->samplerate         = audioEssenceFile->samplerate;
			aafi->Audio->samplerateRational = audioEssenceFile->samplerateRational;
		}
	}

	aafiVideoEssence* videoEssenceFile = NULL;

	AAFI_foreachVideoEssence (aafi, videoEssenceFile)
	{
		if (videoEssenceFile->original_file_path == NULL) {
			continue;
		}

		videoEssenceFile->usable_file_path = aafi_locate_external_essence_file (aafi, videoEssenceFile->original_file_path, aafi->ctx.options.media_location);

		if (videoEssenceFile->usable_file_path == NULL) {
			error ("Could not locate external video essence file '%s'", videoEssenceFile->original_file_path);
			continue;
		}
	}

	aafPosition_t   trackEnd   = 0;
	aafiAudioTrack* audioTrack = NULL;

	AAFI_foreachAudioTrack (aafi, audioTrack)
	{
		if (aafi->compositionLength_editRate) {
			trackEnd = aafi_convertUnit (audioTrack->current_pos, audioTrack->edit_rate, aafi->compositionLength_editRate);
		} else {
			trackEnd = audioTrack->current_pos;
		}

		if (trackEnd > aafi->compositionLength) {
			debug ("Setting compositionLength with audio track \"%s\" (%u) : %" PRIi64, audioTrack->name, audioTrack->number, audioTrack->current_pos);
			aafi->compositionLength          = audioTrack->current_pos;
			aafi->compositionLength_editRate = audioTrack->edit_rate;
		}

		aafiTimelineItem* audioItem = NULL;
		aafiAudioClip*    audioClip = NULL;

		AAFI_foreachTrackItem (audioTrack, audioItem)
		{
			if (audioItem->type == AAFI_TRANS) {
				continue;
			}

			audioClip           = (aafiAudioClip*)audioItem->data;
			audioClip->channels = aafi_getAudioEssencePointerChannelCount (audioClip->essencePointerList);
		}
	}

	aafiVideoTrack* videoTrack = NULL;

	AAFI_foreachVideoTrack (aafi, videoTrack)
	{
		if (aafi->compositionLength_editRate) {
			trackEnd = aafi_convertUnit (videoTrack->current_pos, videoTrack->edit_rate, aafi->compositionLength_editRate);
		} else {
			trackEnd = videoTrack->current_pos;
		}

		if (trackEnd > aafi->compositionLength) {
			debug ("Setting compositionLength with video track \"%s\" (%u) : %" PRIi64, videoTrack->name, videoTrack->number, videoTrack->current_pos);
			aafi->compositionLength          = videoTrack->current_pos;
			aafi->compositionLength_editRate = videoTrack->edit_rate;
		}
	}

	aafi->compositionStart          = aafi->Timecode->start;
	aafi->compositionStart_editRate = aafi->Timecode->edit_rate;

	if (protools_AAF (aafi)) {
		protools_post_processing (aafi);
	}

	return 0;
}

void
aafi_dump_obj (AAF_Iface* aafi, aafObject* Obj, struct trace_dump* __td, int state, const char* func, int line, const char* fmt, ...)
{
	va_list        args;
	struct aafLog* log = aafi->log;

	if (aafi->ctx.options.trace == 0) {
		enum verbosityLevel_e verbtype = VERB_QUIET;

		switch (state) {
			case TD_ERROR:
				verbtype = VERB_ERROR;
				break;
			case TD_WARNING:
			case TD_NOT_SUPPORTED:
				verbtype = VERB_WARNING;
				break;
			// case TD_INFO:
			// case TD_OK:            verbtype = VERB_DEBUG;   break;
			default:
				return;
		}

		if (aafi->log->verb < verbtype) {
			return;
		}

		char* buf = NULL;

		va_start (args, fmt);

		int rc = laaf_util_vsnprintf_realloc (&buf, NULL, 0, fmt, args);

		va_end (args);

		if (rc < 0) {
			LOG_BUFFER_WRITE (log, "laaf_util_vsnprintf_realloc() error");
			return;
		}

		laaf_write_log (aafi->log, aafi, LOG_SRC_ID_AAF_IFACE, VERB_ERROR, __FILENAME__, func, line, buf);

		free (buf);

		return;
	}

	if (Obj) {
		switch (state) {
			case TD_ERROR:
				LOG_BUFFER_WRITE (log, "%serr %s%s %s", ANSI_COLOR_RED (log), ANSI_COLOR_DARKGREY (log), TREE_LINE, ANSI_COLOR_RED (log));
				break;
			case TD_WARNING:
				LOG_BUFFER_WRITE (log, "%swrn %s%s %s", ANSI_COLOR_YELLOW (log), ANSI_COLOR_DARKGREY (log), TREE_LINE, ANSI_COLOR_YELLOW (log));
				break;
			case TD_NOT_SUPPORTED:
				LOG_BUFFER_WRITE (log, "%suns %s%s %s", ANSI_COLOR_ORANGE (log), ANSI_COLOR_DARKGREY (log), TREE_LINE, ANSI_COLOR_ORANGE (log));
				break;
			default:
				LOG_BUFFER_WRITE (log, "    %s%s ", ANSI_COLOR_DARKGREY (log), TREE_LINE);
				break;
		}
		LOG_BUFFER_WRITE (log, "%05i", line);
	} else {
		LOG_BUFFER_WRITE (log, "    %s%s%s      ", ANSI_COLOR_DARKGREY (log), TREE_LINE, ANSI_COLOR_RESET (log));
	}

	LOG_BUFFER_WRITE (log, "%s%s%s", ANSI_COLOR_DARKGREY (log), TREE_LINE, ANSI_COLOR_RESET (log)); /* │ */

	/* Print left padding and vertical lines */

	if (__td->lv > 0) {
		for (int i = 0; i < __td->lv; i++) {
			/* current level iteration has more than one entry remaining in loop */

			if (__td->ll[i] > 1) {
				/* next level iteration is current trace */

				if (i + 1 == __td->lv) {
					if (Obj) {
						LOG_BUFFER_WRITE (log, "%s ", TREE_ENTRY); /* ├── */
					} else {
						LOG_BUFFER_WRITE (log, "%s ", TREE_PADDED_LINE); /* │ */
					}
				} else {
					LOG_BUFFER_WRITE (log, "%s ", TREE_PADDED_LINE); /* │ */
				}
			} else if (i + 1 == __td->lv && Obj) {
				LOG_BUFFER_WRITE (log, "%s ", TREE_LAST_ENTRY); /* └── */
			} else {
				LOG_BUFFER_WRITE (log, "    ");
			}
		}
	}

	if (Obj) {
		switch (state) {
			case TD_ERROR:
				LOG_BUFFER_WRITE (log, "%s", ANSI_COLOR_RED (log));
				break;
			case TD_WARNING:
				LOG_BUFFER_WRITE (log, "%s", ANSI_COLOR_YELLOW (log));
				break;
			case TD_NOT_SUPPORTED:
				LOG_BUFFER_WRITE (log, "%s", ANSI_COLOR_ORANGE (log));
				break;
			case TD_INFO:
			case TD_OK:
				LOG_BUFFER_WRITE (log, "%s", ANSI_COLOR_CYAN (log));
				break;
		}

		LOG_BUFFER_WRITE (log, "%s ", aaft_ClassIDToText (aafi->aafd, Obj->Class->ID));

		LOG_BUFFER_WRITE (log, "%s", ANSI_COLOR_RESET (log));

		if (aaf_ObjectInheritsClass (Obj, &AAFClassID_Mob)) {
			aafMobID_t* mobID     = aaf_get_propertyValue (Obj, PID_Mob_MobID, &AAFTypeID_MobIDType);
			char*       name      = aaf_get_propertyValue (Obj, PID_Mob_Name, &AAFTypeID_String);
			aafUID_t*   usageCode = aaf_get_propertyValue (Obj, PID_Mob_UsageCode, &AAFTypeID_UsageType);

			LOG_BUFFER_WRITE (log, "(UsageCode: %s%s%s) %s%s",
			                  ANSI_COLOR_DARKGREY (log),
			                  aaft_UsageCodeToText (usageCode),
			                  ANSI_COLOR_RESET (log),
			                  (name && name[0] != 0x00) ? ": " : "",
			                  (name) ? name : "");

			free (name);

			LOG_BUFFER_WRITE (log, " MobID: %s%s%s",
			                  ANSI_COLOR_DARKGREY (log),
			                  (mobID) ? aaft_MobIDToText (mobID) : "none",
			                  ANSI_COLOR_RESET (log));
		} else if (aafUIDCmp (Obj->Class->ID, &AAFClassID_TimelineMobSlot)) {
			aafObject* Segment        = aaf_get_propertyValue (Obj, PID_MobSlot_Segment, &AAFTypeID_SegmentStrongReference);
			char*      name           = aaf_get_propertyValue (Obj, PID_MobSlot_SlotName, &AAFTypeID_String);
			uint32_t*  slotID         = aaf_get_propertyValue (Obj, PID_MobSlot_SlotID, &AAFTypeID_UInt32);
			uint32_t*  trackNo        = aaf_get_propertyValue (Obj, PID_MobSlot_PhysicalTrackNumber, &AAFTypeID_UInt32);
			aafUID_t*  DataDefinition = NULL;

			aafWeakRef_t* dataDefWeakRef = aaf_get_propertyValue (Segment, PID_Component_DataDefinition, &AAFTypeID_DataDefinitionWeakReference);

			if (dataDefWeakRef) {
				DataDefinition = aaf_get_DataIdentificationByWeakRef (aafi->aafd, dataDefWeakRef);
			}

			LOG_BUFFER_WRITE (log, "[slot:%s%i%s track:%s%i%s] (DataDef: %s%s%s) %s%s ",
			                  ANSI_COLOR_BOLD (log),
			                  (slotID) ? (int)(*slotID) : -1,
			                  ANSI_COLOR_RESET (log),
			                  ANSI_COLOR_BOLD (log),
			                  (trackNo) ? (int)(*trackNo) : -1,
			                  ANSI_COLOR_RESET (log),
			                  (state == TD_NOT_SUPPORTED) ? ANSI_COLOR_ORANGE (log) : ANSI_COLOR_DARKGREY (log),
			                  aaft_DataDefToText (aafi->aafd, DataDefinition),
			                  ANSI_COLOR_RESET (log),
			                  (name && name[0] != 0x00) ? ": " : "",
			                  (name) ? name : "");

			free (name);
		} else if (aafUIDCmp (Obj->Class->ID, &AAFClassID_OperationGroup)) {
			aafUID_t* OperationIdentification = NULL;

			aafWeakRef_t* OperationDefWeakRef = aaf_get_propertyValue (Obj, PID_OperationGroup_Operation, &AAFTypeID_OperationDefinitionWeakReference);

			if (OperationDefWeakRef) {
				OperationIdentification = aaf_get_OperationIdentificationByWeakRef (aafi->aafd, OperationDefWeakRef);
			}

			int64_t* length = aaf_get_propertyValue (Obj, PID_Component_Length, &AAFTypeID_LengthType);

			LOG_BUFFER_WRITE (log, "(OpIdent: %s%s%s; Length: %s%li%s) ",
			                  (state == TD_NOT_SUPPORTED) ? ANSI_COLOR_ORANGE (log) : ANSI_COLOR_DARKGREY (log),
			                  aaft_OperationDefToText (aafi->aafd, OperationIdentification),
			                  ANSI_COLOR_RESET (log),
			                  ANSI_COLOR_DARKGREY (log),
			                  (length) ? *length : -1,
			                  ANSI_COLOR_RESET (log));
		} else if (aaf_ObjectInheritsClass (Obj, &AAFClassID_Component)) {
			int64_t* length = aaf_get_propertyValue (Obj, PID_Component_Length, &AAFTypeID_LengthType);

			LOG_BUFFER_WRITE (log, "(Length: %s%li%s",
			                  ANSI_COLOR_DARKGREY (log),
			                  (length) ? *length : -1,
			                  ANSI_COLOR_RESET (log));

			if (aafUIDCmp (Obj->Class->ID, &AAFClassID_Transition)) {
				aafPosition_t* cutPoint = aaf_get_propertyValue (Obj, PID_Transition_CutPoint, &AAFTypeID_PositionType);

				if (cutPoint) {
					LOG_BUFFER_WRITE (log, "; CutPoint: %s%li%s",
					                  ANSI_COLOR_DARKGREY (log),
					                  *cutPoint,
					                  ANSI_COLOR_RESET (log));
				}
			}

			LOG_BUFFER_WRITE (log, ")");
		} else if (aafUIDCmp (Obj->Class->ID, &AAFClassID_ConstantValue)) {
			aafIndirect_t* Indirect = aaf_get_propertyValue (Obj, PID_ConstantValue_Value, &AAFTypeID_Indirect);

			if (Indirect) {
				aafUID_t* ParamDef = aaf_get_propertyValue (Obj, PID_Parameter_Definition, &AAFTypeID_AUID);

				LOG_BUFFER_WRITE (log, "(ParamDef: %s%s%s; Type: %s%s%s) ",
				                  (state == TD_NOT_SUPPORTED) ? ANSI_COLOR_ORANGE (log) : ANSI_COLOR_DARKGREY (log),
				                  aaft_ParameterToText (aafi->aafd, ParamDef),
				                  ANSI_COLOR_RESET (log),
				                  ANSI_COLOR_DARKGREY (log),
				                  aaft_TypeIDToText (&Indirect->TypeDef),
				                  ANSI_COLOR_RESET (log));

				LOG_BUFFER_WRITE (log, ": %s%s%s",
				                  ANSI_COLOR_DARKGREY (log),
				                  aaft_IndirectValueToText (aafi->aafd, Indirect),
				                  ANSI_COLOR_RESET (log));

				if (aafUIDCmp (ParamDef, &AAFParameterDef_Amplitude) &&
				    aafUIDCmp (&Indirect->TypeDef, &AAFTypeID_Rational)) {
					aafRational_t* value = aaf_get_indirectValue (aafi->aafd, Indirect, NULL);

					LOG_BUFFER_WRITE (log, " %s(%+05.1lf dB)%s",
					                  ANSI_COLOR_DARKGREY (log),
					                  20 * log10 (aafRationalToDouble (*value)),
					                  ANSI_COLOR_RESET (log));
				}

				__td->eob = 0;
			}
		} else if (aafUIDCmp (Obj->Class->ID, &AAFClassID_VaryingValue)) {
			aafUID_t* ParamDef                    = aaf_get_propertyValue (Obj, PID_Parameter_Definition, &AAFTypeID_AUID);
			aafUID_t* InterpolationIdentification = NULL;

			aafWeakRef_t* InterpolationDefWeakRef = aaf_get_propertyValue (Obj, PID_VaryingValue_Interpolation, &AAFTypeID_InterpolationDefinitionWeakReference);

			if (InterpolationDefWeakRef) {
				InterpolationIdentification = aaf_get_InterpolationIdentificationByWeakRef (aafi->aafd, InterpolationDefWeakRef);
			}

			LOG_BUFFER_WRITE (log, " (ParamDef: %s%s%s; Interpol: %s%s%s) ",
			                  (state == TD_NOT_SUPPORTED) ? ANSI_COLOR_ORANGE (log) : ANSI_COLOR_DARKGREY (log),
			                  aaft_ParameterToText (aafi->aafd, ParamDef),
			                  ANSI_COLOR_RESET (log),
			                  ANSI_COLOR_DARKGREY (log),
			                  aaft_InterpolationToText (InterpolationIdentification),
			                  ANSI_COLOR_RESET (log));

			__td->eob = 0;
		} else if (aafUIDCmp (Obj->Class->ID, &AAFClassID_NetworkLocator)) {
			char* url = aaf_get_propertyValue (Obj, PID_NetworkLocator_URLString, &AAFTypeID_String);

			if (url) {
				LOG_BUFFER_WRITE (log, "(URLString: %s%s%s)",
				                  ANSI_COLOR_DARKGREY (log),
				                  url,
				                  ANSI_COLOR_RESET (log));

				free (url);
			}
		} else if (aafUIDCmp (Obj->Class->ID, &AAFClassID_EssenceData)) {
			char* streamName = aaf_get_propertyValue (Obj, PID_EssenceData_Data, &AAFTypeID_String);

			if (streamName) {
				LOG_BUFFER_WRITE (log, "(Data: %s%s%s)",
				                  ANSI_COLOR_DARKGREY (log),
				                  streamName,
				                  ANSI_COLOR_RESET (log));

				free (streamName);
			}
		} else if (aaf_ObjectInheritsClass (Obj, &AAFClassID_FileDescriptor)) {
			aafUID_t* ContainerFormat = NULL;

			aafWeakRef_t* ContainerDefWeakRef = aaf_get_propertyValue (Obj, PID_FileDescriptor_ContainerFormat, &AAFTypeID_ClassDefinitionWeakReference);

			if (ContainerDefWeakRef) {
				ContainerFormat = aaf_get_ContainerIdentificationByWeakRef (aafi->aafd, ContainerDefWeakRef);
			}

			LOG_BUFFER_WRITE (log, "(ContainerIdent : %s%s%s)",
			                  ANSI_COLOR_DARKGREY (log),
			                  aaft_ContainerToText (ContainerFormat),
			                  ANSI_COLOR_RESET (log));
		}

		if (state == TD_INFO) {
			LOG_BUFFER_WRITE (log, ": %s", ANSI_COLOR_CYAN (log));
		} else if (state == TD_WARNING) {
			LOG_BUFFER_WRITE (log, ": %s", ANSI_COLOR_YELLOW (log));
		} else if (state == TD_ERROR) {
			LOG_BUFFER_WRITE (log, ": %s", ANSI_COLOR_RED (log));
		}

		va_start (args, fmt);

		int rc = laaf_util_vsnprintf_realloc (&log->_msg, &log->_msg_size, log->_msg_pos, fmt, args);

		if (rc < 0) {
			LOG_BUFFER_WRITE (log, "laaf_util_vsnprintf_realloc() error");
		} else {
			log->_msg_pos += (size_t)rc;
		}

		va_end (args);

		if (state == TD_ERROR || state == TD_INFO) {
			LOG_BUFFER_WRITE (log, ".");
		}

		int hasUnknownProps = 0;

		if (!aafi->ctx.options.dump_class_aaf_properties) {
			aafProperty* Prop = NULL;

			for (Prop = Obj->Properties; Prop != NULL; Prop = Prop->next) {
				if (Prop->def->meta) {
					LOG_BUFFER_WRITE (log, "%s%s %s[0x%04x]", ANSI_COLOR_RESET (log), (!hasUnknownProps) ? "  (MetaProps:" : "", aaft_PIDToText (aafi->aafd, Prop->pid), Prop->pid);
					hasUnknownProps++;
				}
			}

			if (hasUnknownProps) {
				LOG_BUFFER_WRITE (log, ")");
			}
		}

		if (aafi->ctx.options.dump_tagged_value) {
			if (aaf_ObjectInheritsClass (Obj, &AAFClassID_Mob)) {
				aafObject* UserComments = aaf_get_propertyValue (Obj, PID_Mob_UserComments, &AAFTypeID_TaggedValueStrongReferenceVector);
				aafObject* Attributes   = aaf_get_propertyValue (Obj, PID_Mob_Attributes, &AAFTypeID_TaggedValueStrongReferenceVector);

				if (UserComments) {
					LOG_BUFFER_WRITE (log, "\n    Mob::UserComments:\n");
					aaf_dump_TaggedValueSet (aafi->aafd, UserComments, "     ");
				}
				if (Attributes) {
					LOG_BUFFER_WRITE (log, "\n    Mob::Attributes:\n");
					aaf_dump_TaggedValueSet (aafi->aafd, Attributes, "     ");
				}
			} else if (aaf_ObjectInheritsClass (Obj, &AAFClassID_Component)) {
				aafObject* UserComments = aaf_get_propertyValue (Obj, PID_Component_UserComments, &AAFTypeID_TaggedValueStrongReferenceVector);
				aafObject* Attributes   = aaf_get_propertyValue (Obj, PID_Component_Attributes, &AAFTypeID_TaggedValueStrongReferenceVector);

				if (UserComments) {
					LOG_BUFFER_WRITE (log, "\n    Component::UserComments:\n");
					aaf_dump_TaggedValueSet (aafi->aafd, UserComments, "     ");
				}
				if (Attributes) {
					LOG_BUFFER_WRITE (log, "\n    Component::Attributes:\n");
					aaf_dump_TaggedValueSet (aafi->aafd, Attributes, "     ");
				}
			}
		}

		if (aafi->ctx.options.dump_meta && hasUnknownProps) {
			LOG_BUFFER_WRITE (log, "\n\n%s", ANSI_COLOR_MAGENTA (log));
			LOG_BUFFER_WRITE (log, "    ======================================================================\n");
			LOG_BUFFER_WRITE (log, "                           AAF Meta Properties Dump\n");
			LOG_BUFFER_WRITE (log, "    ======================================================================\n");
			LOG_BUFFER_WRITE (log, "%s", ANSI_COLOR_RESET (log));

			aafProperty* Prop = NULL;

			for (Prop = Obj->Properties; Prop != NULL; Prop = Prop->next) {
				if (Prop->def->meta) {
					if (aafi->ctx.options.dump_meta) {
						if (Prop->sf == SF_STRONG_OBJECT_REFERENCE_VECTOR) {
							LOG_BUFFER_WRITE (log, "\n");
							LOG_BUFFER_WRITE (log, "    [%s0x%04x%s] %s (%s)\n",
							                  ANSI_COLOR_MAGENTA (log),
							                  Prop->pid,
							                  ANSI_COLOR_RESET (log),
							                  aaft_PIDToText (aafi->aafd, Prop->pid),
							                  aaft_StoredFormToText (Prop->sf));

							void* propValue = aaf_get_propertyValue (Obj, Prop->pid, &AAFTypeID_TaggedValueStrongReferenceVector);

							log->color_reset = ANSI_COLOR_MAGENTA (log);
							aaf_dump_TaggedValueSet (aafi->aafd, propValue, "     ");
							log->color_reset = NULL;
						} else {
							LOG_BUFFER_WRITE (log, "\n");
							aaf_dump_ObjectProperty (aafi->aafd, Prop, "    ");
						}
					}
				}
			}
		}

		if (aafi->ctx.options.dump_class_raw_properties && strcmp (aaft_ClassIDToText (aafi->aafd, Obj->Class->ID), aafi->ctx.options.dump_class_raw_properties) == 0) {
			LOG_BUFFER_WRITE (log, "\n\n");
			LOG_BUFFER_WRITE (log, "    ======================================================================\n");
			LOG_BUFFER_WRITE (log, "                        CFB Object Properties Dump\n");
			LOG_BUFFER_WRITE (log, "    ======================================================================\n");
			LOG_BUFFER_WRITE (log, "%s", ANSI_COLOR_DARKGREY (log));
			LOG_BUFFER_WRITE (log, "    %s\n", aaft_ClassIDToText (aafi->aafd, Obj->Class->ID));
			LOG_BUFFER_WRITE (log, "    %s/properties\n", aaf_get_ObjectPath (Obj));
			LOG_BUFFER_WRITE (log, "%s\n\n", ANSI_COLOR_RESET (log));

			aaf_dump_nodeStreamProperties (aafi->aafd, cfb_getChildNode (aafi->aafd->cfbd, "properties", Obj->Node), "    ");

			LOG_BUFFER_WRITE (log, "\n");
		}

		if (aafi->ctx.options.dump_class_aaf_properties && strcmp (aaft_ClassIDToText (aafi->aafd, Obj->Class->ID), aafi->ctx.options.dump_class_aaf_properties) == 0) {
			LOG_BUFFER_WRITE (log, "\n\n");
			LOG_BUFFER_WRITE (log, "    ======================================================================\n");
			LOG_BUFFER_WRITE (log, "                             AAF Properties Dump\n");
			LOG_BUFFER_WRITE (log, "    ======================================================================\n");
			LOG_BUFFER_WRITE (log, "%s", ANSI_COLOR_DARKGREY (log));
			LOG_BUFFER_WRITE (log, "    %s\n", aaft_ClassIDToText (aafi->aafd, Obj->Class->ID));
			LOG_BUFFER_WRITE (log, "    %s/properties\n", aaf_get_ObjectPath (Obj));
			LOG_BUFFER_WRITE (log, "%s\n\n", ANSI_COLOR_RESET (log));

			aaf_dump_ObjectProperties (aafi->aafd, Obj, "    ");

			LOG_BUFFER_WRITE (log, "\n");
		}

		LOG_BUFFER_WRITE (log, "%s", ANSI_COLOR_RESET (log));
	}

	log->log_callback (log, (void*)aafi, LOG_SRC_ID_TRACE, 0, "", "", 0, log->_msg, log->user);

	/* if end of branch, print one line padding */
	if (Obj && (__td->eob || state == TD_ERROR))
		aafi_dump_obj (aafi, NULL, __td, 0, NULL, -1, "");
}

/**
 * @}
 */
