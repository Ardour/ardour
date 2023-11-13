/*
 * Copyright (C) 2023 Adrien Gesta-Fline
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

#include <stdint.h>
#include <stdio.h>

#include "aaf/AAFDefs/AAFPropertyIDs.h"
#include "aaf/AAFDefs/AAFTypeDefUIDs.h"
#include "aaf/AAFIParser.h"
#include "aaf/AAFToText.h"

#include "aaf/debug.h"
#include "aaf/libaaf.h"

#define debug(...) \
	_dbg (aafi->dbg, aafi, DEBUG_SRC_ID_AAF_IFACE, VERB_DEBUG, __VA_ARGS__)

#define warning(...) \
	_dbg (aafi->dbg, aafi, DEBUG_SRC_ID_AAF_IFACE, VERB_WARNING, __VA_ARGS__)

#define error(...) \
	_dbg (aafi->dbg, aafi, DEBUG_SRC_ID_AAF_IFACE, VERB_ERROR, __VA_ARGS__)

int
resolve_AAF (struct AAF_Iface* aafi)
{
	int probe = 0;

	if (aafi->aafd->Identification.CompanyName && wcsncmp (aafi->aafd->Identification.CompanyName, L"Blackmagic Design", wcslen (L"Blackmagic Design")) == 0) {
		probe++;
	}

	if (aafi->aafd->Identification.ProductName && wcsncmp (aafi->aafd->Identification.ProductName, L"DaVinci Resolve", wcslen (L"DaVinci Resolve")) == 0) {
		probe++;
	}

	if (probe == 2) {
		return 1;
	}

	return 0;
}

int
resolve_parse_aafObject_Selector (struct AAF_Iface* aafi, aafObject* Selector, td* __ptd)
{
	/*
	 *  Resolve 18.5
	 *
	 *  The Selector Object was only seen used to describe a disabled clip :
	 *    - Selected property Object is an empty Filler
	 *    - Alternate keeps track of the original clip.
	 *
03411││              ├──◻ AAFClassID_Selector
01926││ Selected --> │    └──◻ AAFClassID_Filler
02359││ Alternate -> │    └──◻ AAFClassID_OperationGroup (OpIdent: AAFOperationDef_MonoAudioGain)
03822││              │         ├──◻ AAFClassID_VaryingValue
02877││              │         └──◻ AAFClassID_SourceClip
02882││              │              └──◻ AAFClassID_MasterMob (UsageCode: n/a) : speech-sample.mp3 - disabled
04460││              │                   └──◻ AAFClassID_TimelineMobSlot
03089││              │                        └──◻ AAFClassID_SourceClip
04167││              │                             └──◻ AAFClassID_SourceMob (UsageCode: n/a) : speech-sample.mp3 - disabled
01249││              │                                  └──◻ AAFClassID_PCMDescriptor
01455││              │                                       └──◻ AAFClassID_NetworkLocator : file:///C:/Users/user/Desktop/libAAF/test/res/speech-sample.mp3
	 */

	struct trace_dump __td;
	__td_set (__td, __ptd, 0);

	aafObject* Selected = aaf_get_propertyValue (Selector, PID_Selector_Selected, &AAFTypeID_SegmentStrongReference);

	if (Selected == NULL) { /* req */
		DUMP_OBJ_ERROR (aafi, Selector, &__td, "Missing PID_Selector_Selected");
		return -1;
	}

	aafObject* Alternates = aaf_get_propertyValue (Selector, PID_Selector_Alternates, &AAFTypeID_SegmentStrongReferenceVector);

	if (Alternates == NULL) { /* opt */
		DUMP_OBJ_WARNING (aafi, Selector, &__td, "Missing PID_Selector_Alternates");
		return -1;
	}

	void* ComponentAttributeList = aaf_get_propertyValue (Selector, aaf_get_PropertyIDByName (aafi->aafd, L"ComponentAttributeList"), &AAFUID_NULL);

	if (ComponentAttributeList == NULL) {
		DUMP_OBJ_ERROR (aafi, Selector, &__td, "Missing AAFClassID_Selector::ComponentAttributeList");
		return -1;
	}

	DUMP_OBJ (aafi, Selector, &__td);

	// aaf_dump_ObjectProperties( aafi->aafd, Selector );
	// aaf_dump_ObjectProperties( aafi->aafd, ComponentAttributeList );

	int        ismuted            = 0;
	aafObject* ComponentAttribute = NULL;

	aaf_foreach_ObjectInSet (&ComponentAttribute, ComponentAttributeList, NULL)
	{
		/* TODO implement retrieve_TaggedValue() */

		wchar_t* name = aaf_get_propertyValue (ComponentAttribute, PID_TaggedValue_Name, &AAFTypeID_String);

		if (name == NULL) { /* req */
			DUMP_OBJ_ERROR (aafi, ComponentAttribute, &__td, "Missing PID_TaggedValue_Name");
			continue;
		}

		aafIndirect_t* Indirect = aaf_get_propertyValue (ComponentAttribute, PID_TaggedValue_Value, &AAFTypeID_Indirect);

		if (Indirect == NULL) {
			DUMP_OBJ_ERROR (aafi, ComponentAttribute, &__td, "Missing PID_TaggedValue_Value");
			free (name);
			continue;
		}

		int32_t* value = aaf_get_indirectValue (aafi->aafd, Indirect, &AAFTypeID_Int32);

		if (value == NULL) {
			DUMP_OBJ_ERROR (aafi, ComponentAttribute, &__td, "Could not retrieve Indirect value for PID_TaggedValue_Value");
			free (name);
			continue;
		}

		// debug( "Tagged | Name: %ls    Value : %u", name, *value );

		if (aafi->ctx.options.resolve & RESOLVE_INCLUDE_DISABLED_CLIPS) {
			if (wcsncmp (name, L"_DISABLE_CLIP_FLAG", wcslen (L"_DISABLE_CLIP_FLAG")) == 0 && *value == 1) {
				ismuted                         = 1;
				aafi->ctx.current_clip_is_muted = 1;

				aafObject* Alternate = NULL;

				int i = 0;
				aaf_foreach_ObjectInSet (&Alternate, Alternates, NULL)
				{
					if (i == 0) { /* there should be only one Segment in set, but still. Let's be carefull */
						aafi_parse_Segment (aafi, Alternate, &__td);
					} else {
						DUMP_OBJ_ERROR (aafi, Alternate, &__td, "Multiple Alternates in Davinci Resolve selector");
					}
					i++;
				}
			}
		}

		free (name);
	}

	/* aafi->ctx.current_clip_is_muted was already reset at this point */
	if (ismuted == 0) {
		return aafi_parse_Segment (aafi, Selected, &__td);
	}

	return 0;
}

int
resolve_parse_aafObject_DescriptiveMarker (struct AAF_Iface* aafi, aafObject* DescriptiveMarker, td* __ptd)
{
	/*
	 *  Resolve 18.5
	 */

	struct trace_dump __td;
	__td_set (__td, __ptd, 1);

	aafPosition_t* start = aaf_get_propertyValue (DescriptiveMarker, PID_Event_Position, &AAFTypeID_PositionType);

	if (start == NULL) { /* req (TODO: conditional) */
		DUMP_OBJ_ERROR (aafi, DescriptiveMarker, &__td, "Missing PID_Event_Position");
		return -1;
	}

	aafPosition_t* length = aaf_get_propertyValue (DescriptiveMarker, PID_Component_Length, &AAFTypeID_PositionType);

	wchar_t* comment = aaf_get_propertyValue (DescriptiveMarker, PID_Event_Comment, &AAFTypeID_String);

	wchar_t* name = aaf_get_propertyValue (DescriptiveMarker, aaf_get_PropertyIDByName (aafi->aafd, L"CommentMarkerUser"), &AAFTypeID_String);

	uint16_t* RGBColor = NULL;

	aafProperty* RGBColorProp = aaf_get_property (DescriptiveMarker, aaf_get_PropertyIDByName (aafi->aafd, L"CommentMarkerColor"));

	if (RGBColorProp) {
		if (RGBColorProp->len != sizeof (uint16_t) * 3) {
			error ("CommentMarkerColor has wrong size of %u", RGBColorProp->len);
		} else {
			RGBColor = RGBColorProp->val;

			/* big endian to little endian */

			RGBColor[0] = (RGBColor[0] >> 8) | (RGBColor[0] << 8);
			RGBColor[1] = (RGBColor[1] >> 8) | (RGBColor[1] << 8);
			RGBColor[2] = (RGBColor[2] >> 8) | (RGBColor[2] << 8);
		}
	}

	aafi_newMarker (aafi, aafi->ctx.current_markers_edit_rate, *start, ((length) ? *length : 0), name, comment, &RGBColor);

	DUMP_OBJ (aafi, DescriptiveMarker, &__td);

	return 0;
}
